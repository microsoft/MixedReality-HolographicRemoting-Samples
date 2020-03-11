#include <pch.h>

#include "CameraResources.h"
#include "DeviceResourcesUWP.h"

using namespace D2D1;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::Graphics::Holographic;

using namespace DXHelper;

void DeviceResourcesUWP::SetWindow(const winrt::Windows::UI::Core::CoreWindow& window)
{
    UnregisterHolographicEventHandlers();

    m_holographicSpace = HolographicSpace::CreateForCoreWindow(window);

    InitializeUsingHolographicSpace();

    m_cameraAddedToken = m_holographicSpace.CameraAdded({this, &DeviceResourcesUWP::OnCameraAdded});
    m_cameraRemovedToken = m_holographicSpace.CameraRemoved({this, &DeviceResourcesUWP::OnCameraRemoved});
}

void DeviceResourcesUWP::InitializeUsingHolographicSpace()
{
    // The holographic space might need to determine which adapter supports
    // holograms, in which case it will specify a non-zero PrimaryAdapterId.
    LUID id = {m_holographicSpace.PrimaryAdapterId().LowPart, m_holographicSpace.PrimaryAdapterId().HighPart};

    // When a primary adapter ID is given to the app, the app should find
    // the corresponding DXGI adapter and use it to create Direct3D devices
    // and device contexts. Otherwise, there is no restriction on the DXGI
    // adapter the app can use.
    if ((id.HighPart != 0) || (id.LowPart != 0))
    {
        UINT createFlags = 0;
#ifdef DEBUG
        if (SdkLayersAvailable())
        {
            createFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif
        // Create the DXGI factory.
        winrt::com_ptr<IDXGIFactory1> dxgiFactory;
        winrt::check_hresult(CreateDXGIFactory2(createFlags, IID_PPV_ARGS(dxgiFactory.put())));
        winrt::com_ptr<IDXGIFactory4> dxgiFactory4;
        dxgiFactory.as(dxgiFactory4);

        // Retrieve the adapter specified by the holographic space.
        m_dxgiAdapter = nullptr;
        winrt::check_hresult(dxgiFactory4->EnumAdapterByLuid(id, IID_PPV_ARGS(m_dxgiAdapter.put())));
    }
    else
    {
        m_dxgiAdapter = nullptr;
    }

    CreateDeviceResources();

    m_holographicSpace.SetDirect3D11Device(m_d3dInteropDevice);
}

void DeviceResourcesUWP::OnCameraAdded(
    winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
    winrt::Windows::Graphics::Holographic::HolographicSpaceCameraAddedEventArgs const& args)
{
    UseHolographicCameraResources([this, camera = args.Camera()](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap) {
        cameraResourceMap[camera.Id()] = std::make_unique<CameraResources>(camera);
    });
}

void DeviceResourcesUWP::OnCameraRemoved(
    winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
    winrt::Windows::Graphics::Holographic::HolographicSpaceCameraRemovedEventArgs const& args)
{
    UseHolographicCameraResources([this, camera = args.Camera()](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap) {
        CameraResources* pCameraResources = cameraResourceMap[camera.Id()].get();

        if (pCameraResources != nullptr)
        {
            pCameraResources->ReleaseResourcesForBackBuffer(this);
            cameraResourceMap.erase(camera.Id());
        }
    });
}

void DeviceResourcesUWP::UnregisterHolographicEventHandlers()
{
    if (m_holographicSpace != nullptr)
    {
        // Clear previous event registrations.
        m_holographicSpace.CameraAdded(m_cameraAddedToken);
        m_cameraAddedToken = {};
        m_holographicSpace.CameraRemoved(m_cameraRemovedToken);
        m_cameraRemovedToken = {};
    }
}

// Validates the back buffer for each HolographicCamera and recreates
// resources for back buffers that have changed.
// Locks the set of holographic camera resources until the function exits.
void DeviceResourcesUWP::EnsureCameraResources(HolographicFrame frame, HolographicFramePrediction prediction)
{
    UseHolographicCameraResources([this, frame, prediction](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap) {
        for (HolographicCameraPose const& cameraPose : prediction.CameraPoses())
        {
            HolographicCameraRenderingParameters renderingParameters = frame.GetRenderingParameters(cameraPose);
            CameraResources* pCameraResources = cameraResourceMap[cameraPose.HolographicCamera().Id()].get();

            pCameraResources->CreateResourcesForBackBuffer(this, renderingParameters);
        }
    });
}

DeviceResourcesUWP::~DeviceResourcesUWP()
{
    UnregisterHolographicEventHandlers();
}

// Recreate all device resources and set them back to the current state.
// Locks the set of holographic camera resources until the function exits.
void DeviceResourcesUWP::HandleDeviceLost()
{
    NotifyDeviceLost();

    UseHolographicCameraResources([this](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap) {
        for (auto& pair : cameraResourceMap)
        {
            CameraResources* pCameraResources = pair.second.get();
            pCameraResources->ReleaseResourcesForBackBuffer(this);
        }
    });

    InitializeUsingHolographicSpace();

    NotifyDeviceRestored();
}

void DeviceResourcesUWP::CreateDeviceResources()
{
    DeviceResourcesCommon::CreateDeviceResources();

    // Acquire the DXGI interface for the Direct3D device.
    winrt::com_ptr<IDXGIDevice3> dxgiDevice;
    m_d3dDevice.as(dxgiDevice);

    // Wrap the native device using a WinRT interop object.
    winrt::com_ptr<::IInspectable> object;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), reinterpret_cast<IInspectable**>(winrt::put_abi(object))));
    m_d3dInteropDevice = object.as<IDirect3DDevice>();
}

// Present the contents of the swap chain to the screen.
// Locks the set of holographic camera resources until the function exits.
void DeviceResourcesUWP::Present(HolographicFrame frame)
{
    HolographicFramePresentResult presentResult =
        frame.PresentUsingCurrentPrediction(HolographicFramePresentWaitBehavior::DoNotWaitForFrameToFinish);

    // Note, by not waiting on PresentUsingCurrentPrediction and instead using WaitForNextFrameReadyWithHeadStart we avoid going into
    // pipelined mode.
    try
    {
        // WaitForNextFrameReadyWithHeadStart has been added in 10.0.17763.0.
        if (!m_useLegacyWaitBehavior)
        {
            m_holographicSpace.WaitForNextFrameReadyWithHeadStart(winrt::Windows::Foundation::TimeSpan(0));
        }
        else
        {
            frame.WaitForFrameToFinish();
        }
    }
    catch (winrt::hresult_error& err)
    {
        switch (err.code())
        {
            case DXGI_ERROR_DEVICE_HUNG:
            case DXGI_ERROR_DEVICE_REMOVED:
            case DXGI_ERROR_DEVICE_RESET:
                presentResult = HolographicFramePresentResult::DeviceRemoved;
                break;

            default:
                try
                {
                    m_useLegacyWaitBehavior = true;
                    frame.WaitForFrameToFinish();
                }
                catch (winrt::hresult_error& err2)
                {
                    switch (err2.code())
                    {
                        case DXGI_ERROR_DEVICE_HUNG:
                        case DXGI_ERROR_DEVICE_REMOVED:
                        case DXGI_ERROR_DEVICE_RESET:
                            presentResult = HolographicFramePresentResult::DeviceRemoved;
                            break;

                        default:
                            throw err2;
                    }
                }
                break;
        }

        // The PresentUsingCurrentPrediction API will detect when the graphics device
        // changes or becomes invalid. When this happens, it is considered a Direct3D
        // device lost scenario.
        if (presentResult == HolographicFramePresentResult::DeviceRemoved)
        {
            // The Direct3D device, context, and resources should be recreated.
            HandleDeviceLost();
        }
    }
}
