#include <pch.h>

#include "CameraResources.h"
#include "DeviceResourcesUWP.h"

#include <winrt/Windows.Foundation.Metadata.h>

using namespace D2D1;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::Foundation::Numerics;

using namespace DXHelper;

void DeviceResourcesUWP::SetWindow(const winrt::Windows::UI::Core::CoreWindow& window)
{
    UnregisterHolographicEventHandlers();

    m_holographicSpace = HolographicSpace::CreateForCoreWindow(window);

    InitializeUsingHolographicSpace();

    m_cameraAddedToken = m_holographicSpace.CameraAdded({this, &DeviceResourcesUWP::OnCameraAdded});
    m_cameraRemovedToken = m_holographicSpace.CameraRemoved({this, &DeviceResourcesUWP::OnCameraRemoved});

    m_isAvailableChangedRevoker =
        m_holographicSpace.IsAvailableChanged(winrt::auto_revoke, {this, &DeviceResourcesUWP::OnIsAvailableChanged});
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
void DeviceResourcesUWP::EnsureCameraResources(
    HolographicFrame frame,
    HolographicFramePrediction prediction,
    SpatialCoordinateSystem focusPointCoordinateSystem,
    float3 focusPointPosition)
{
    UseHolographicCameraResources([this, frame, prediction, focusPointCoordinateSystem, focusPointPosition](
                                      std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap) {
        for (HolographicCameraPose const& cameraPose : prediction.CameraPoses())
        {
            HolographicCameraRenderingParameters renderingParameters = frame.GetRenderingParameters(cameraPose);
            if (focusPointCoordinateSystem)
            {
                renderingParameters.SetFocusPoint(focusPointCoordinateSystem, focusPointPosition);
            }

            CameraResources* pCameraResources = cameraResourceMap[cameraPose.HolographicCamera().Id()].get();

            pCameraResources->CreateResourcesForBackBuffer(this, renderingParameters);
        }
    });
}

DXHelper::DeviceResourcesUWP::DeviceResourcesUWP()
{
    try
    {
        // WaitForNextFrameReadyWithHeadStart has been added in 10.0.17763.0.
        m_useLegacyWaitBehavior = !winrt::Windows::Foundation::Metadata::ApiInformation::IsMethodPresent(
            L"Windows.Graphics.Holographic.HolographicSpace", L"WaitForNextFrameReadyWithHeadStart");
    }
    catch (const winrt::hresult_error&)
    {
        m_useLegacyWaitBehavior = true;
    }
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
    if (m_nextPresentMustWait)
    {
        switch (WaitForNextFrameReady())
        {
            case WaitResult::Success:
                break;
            case WaitResult::Failure:
                return; // We failed to wait for the next frame ready. Do not present.
            case WaitResult::DeviceLost:
                HandleDeviceLost();
                return;
        }
    }

    // Note, starting with Windows SDK 10.0.17763.0 we can use WaitForNextFrameReadyWithHeadStart which allows us to avoid pipelined mode.
    // Pipelined mode is basically one frame queue which allows an app to do more on the CPU and GPU. For Holographic Remoting pipelined
    // mode means one additional frame of latency.
    HolographicFramePresentWaitBehavior waitBehavior = m_useLegacyWaitBehavior
                                                           ? HolographicFramePresentWaitBehavior::WaitForFrameToFinish
                                                           : HolographicFramePresentWaitBehavior::DoNotWaitForFrameToFinish;

    HolographicFramePresentResult presentResult = frame.PresentUsingCurrentPrediction(waitBehavior);
    m_firstFramePresented = true;

    if (presentResult != HolographicFramePresentResult::Success)
    {
        m_nextPresentMustWait = true;
        HandleDeviceLost();
    }
}

void DXHelper::DeviceResourcesUWP::OnIsAvailableChanged(
    const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& args)
{
    if (m_holographicSpace.IsAvailable() == false)
    {
        // Next present must wait.
        m_nextPresentMustWait = true;
    }
}

DXHelper::DeviceResourcesUWP::WaitResult DXHelper::DeviceResourcesUWP::WaitForNextFrameReady()
{
    if (m_useLegacyWaitBehavior || !m_firstFramePresented)
    {
        return WaitResult::Failure;
    }

    try
    {
        // WaitForNextFrameReadyWithHeadStart has been added in 10.0.17763.0.
        m_holographicSpace.WaitForNextFrameReadyWithHeadStart(winrt::Windows::Foundation::TimeSpan(0));
        return WaitResult::Success;
    }
    catch (winrt::hresult_error& err)
    {
        switch (err.code())
        {
            case DXGI_ERROR_DEVICE_HUNG:
            case DXGI_ERROR_DEVICE_REMOVED:
            case DXGI_ERROR_DEVICE_RESET:
                return WaitResult::DeviceLost;

            default:
                break;
        }
    }
    return WaitResult::Failure;
}
