//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"

#include "DeviceResources.h"

using namespace D2D1;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::Graphics::Holographic;

namespace DXHelper
{
#if defined(_DEBUG)
    // Check for SDK Layer support.
    inline bool SdkLayersAvailable()
    {
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_NULL, // There is no need to create a real hardware device.
            0,
            D3D11_CREATE_DEVICE_DEBUG, // Check for the SDK layers.
            nullptr,                   // Any feature level will do.
            0,
            D3D11_SDK_VERSION, // Always set this to D3D11_SDK_VERSION for Windows Runtime apps.
            nullptr,           // No need to keep the D3D device reference.
            nullptr,           // No need to know the feature level.
            nullptr            // No need to keep the D3D device context reference.
        );

        return SUCCEEDED(hr);
    }
#endif

    // Constructor for DeviceResources.
    DeviceResources::DeviceResources()
    {
        CreateDeviceIndependentResources();
    }

    DeviceResources::~DeviceResources()
    {
        UnregisterHolographicEventHandlers();
    }

    // Configures resources that don't depend on the Direct3D device.
    void DeviceResources::CreateDeviceIndependentResources()
    {
        // Initialize Direct2D resources.
        D2D1_FACTORY_OPTIONS options{};

#if defined(_DEBUG)
        // If the project is in a debug build, enable Direct2D debugging via SDK Layers.
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

        // Initialize the Direct2D Factory.
        m_d2dFactory = nullptr;
        winrt::check_hresult(
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory2), &options, m_d2dFactory.put_void()));

        // Initialize the DirectWrite Factory.
        m_dwriteFactory = nullptr;
        winrt::check_hresult(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2), reinterpret_cast<IUnknown**>(m_dwriteFactory.put_void())));

        // Initialize the Windows Imaging Component (WIC) Factory.
        m_wicFactory = nullptr;
        winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(m_wicFactory.put())));
    }

    void DeviceResources::SetWindow(const winrt::Windows::UI::Core::CoreWindow& window)
    {
        UnregisterHolographicEventHandlers();

        m_holographicSpace = HolographicSpace::CreateForCoreWindow(window);

        InitializeUsingHolographicSpace();

        m_cameraAddedToken = m_holographicSpace.CameraAdded({this, &DeviceResources::OnCameraAdded});
        m_cameraRemovedToken = m_holographicSpace.CameraRemoved({this, &DeviceResources::OnCameraRemoved});
    }

    void DeviceResources::InitializeUsingHolographicSpace()
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

    // Configures the Direct3D device, and stores handles to it and the device context.
    void DeviceResources::CreateDeviceResources()
    {
        // This flag adds support for surfaces with a different color channel ordering
        // than the API default. It is required for compatibility with Direct2D.
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
        if (SdkLayersAvailable())
        {
            // If the project is in a debug build, enable debugging via SDK Layers with this flag.
            creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
        }
#endif

        // This array defines the set of DirectX hardware feature levels this app will support.
        // Note the ordering should be preserved.
        // Note that HoloLens supports feature level 11.1. The HoloLens emulator is also capable
        // of running on graphics cards starting with feature level 10.0.
        D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_12_1,
                                             D3D_FEATURE_LEVEL_12_0,
                                             D3D_FEATURE_LEVEL_11_1,
                                             D3D_FEATURE_LEVEL_11_0,
                                             D3D_FEATURE_LEVEL_10_1,
                                             D3D_FEATURE_LEVEL_10_0};

        // Create the Direct3D 11 API device object and a corresponding context.
        winrt::com_ptr<ID3D11Device> device;
        winrt::com_ptr<ID3D11DeviceContext> context;

        const D3D_DRIVER_TYPE driverType = m_dxgiAdapter == nullptr ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN;
        const HRESULT hr = D3D11CreateDevice(
            m_dxgiAdapter.get(),      // Either nullptr, or the primary adapter determined by Windows Holographic.
            driverType,               // Create a device using the hardware graphics driver.
            0,                        // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
            creationFlags,            // Set debug and Direct2D compatibility flags.
            featureLevels,            // List of feature levels this app can support.
            ARRAYSIZE(featureLevels), // Size of the list above.
            D3D11_SDK_VERSION,        // Always set this to D3D11_SDK_VERSION for Windows Runtime apps.
            device.put(),             // Returns the Direct3D device created.
            &m_d3dFeatureLevel,       // Returns feature level of device created.
            context.put()             // Returns the device immediate context.
        );

        if (FAILED(hr))
        {
            // If the initialization fails, fall back to the WARP device.
            // For more information on WARP, see:
            // http://go.microsoft.com/fwlink/?LinkId=286690
            winrt::check_hresult(D3D11CreateDevice(
                nullptr,              // Use the default DXGI adapter for WARP.
                D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
                0,
                creationFlags,
                featureLevels,
                ARRAYSIZE(featureLevels),
                D3D11_SDK_VERSION,
                device.put(),
                &m_d3dFeatureLevel,
                context.put()));
        }

        // Store pointers to the Direct3D device and immediate context.
        device.as(m_d3dDevice);
        context.as(m_d3dContext);


        // Enable multithread protection for video decoding.
        winrt::com_ptr<ID3D10Multithread> multithread;
        device.as(multithread);
        multithread->SetMultithreadProtected(true);

        // Acquire the DXGI interface for the Direct3D device.
        winrt::com_ptr<IDXGIDevice3> dxgiDevice;
        m_d3dDevice.as(dxgiDevice);

        // Wrap the native device using a WinRT interop object.
        winrt::com_ptr<::IInspectable> object;
        winrt::check_hresult(
            CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), reinterpret_cast<IInspectable**>(winrt::put_abi(object))));
        m_d3dInteropDevice = object.as<IDirect3DDevice>();

        // Cache the DXGI adapter.
        // This is for the case of no preferred DXGI adapter, or fallback to WARP.
        winrt::com_ptr<IDXGIAdapter> dxgiAdapter;
        winrt::check_hresult(dxgiDevice->GetAdapter(dxgiAdapter.put()));
        dxgiAdapter.as(m_dxgiAdapter);

        // Check for device support for the optional feature that allows setting the render target array index from the vertex shader stage.
        D3D11_FEATURE_DATA_D3D11_OPTIONS3 options;
        m_d3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &options, sizeof(options));
        if (options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer)
        {
            m_supportsVprt = true;
        }
    }

    void DeviceResources::OnCameraAdded(
        winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
        winrt::Windows::Graphics::Holographic::HolographicSpaceCameraAddedEventArgs const& args)
    {
        UseHolographicCameraResources(
            [this, camera = args.Camera()](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap) {
                cameraResourceMap[camera.Id()] = std::make_unique<CameraResources>(camera);
            });
    }

    void DeviceResources::OnCameraRemoved(
        winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
        winrt::Windows::Graphics::Holographic::HolographicSpaceCameraRemovedEventArgs const& args)
    {
        UseHolographicCameraResources(
            [this, camera = args.Camera()](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap) {
                CameraResources* pCameraResources = cameraResourceMap[camera.Id()].get();

                if (pCameraResources != nullptr)
                {
                    pCameraResources->ReleaseResourcesForBackBuffer(this);
                    cameraResourceMap.erase(camera.Id());
                }
            });
    }

    void DeviceResources::UnregisterHolographicEventHandlers()
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
    void DeviceResources::EnsureCameraResources(HolographicFrame frame, HolographicFramePrediction prediction)
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

    // Recreate all device resources and set them back to the current state.
    // Locks the set of holographic camera resources until the function exits.
    void DeviceResources::HandleDeviceLost()
    {
        if (m_deviceNotify != nullptr)
        {
            m_deviceNotify->OnDeviceLost();
        }

        UseHolographicCameraResources([this](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap) {
            for (auto& pair : cameraResourceMap)
            {
                CameraResources* pCameraResources = pair.second.get();
                pCameraResources->ReleaseResourcesForBackBuffer(this);
            }
        });

        InitializeUsingHolographicSpace();

        if (m_deviceNotify != nullptr)
        {
            m_deviceNotify->OnDeviceRestored();
        }
    }

    // Register our DeviceNotify to be informed on device lost and creation.
    void DeviceResources::RegisterDeviceNotify(IDeviceNotify* deviceNotify)
    {
        m_deviceNotify = deviceNotify;
    }

    // Call this method when the app suspends. It provides a hint to the driver that the app
    // is entering an idle state and that temporary buffers can be reclaimed for use by other apps.
    void DeviceResources::Trim()
    {
        if (m_d3dContext)
        {
            m_d3dContext->ClearState();
        }

        if (m_d3dDevice)
        {
            winrt::com_ptr<IDXGIDevice3> dxgiDevice;
            m_d3dDevice.as(dxgiDevice);
            dxgiDevice->Trim();
        }
    }

    // Present the contents of the swap chain to the screen.
    // Locks the set of holographic camera resources until the function exits.
    void DeviceResources::Present(HolographicFrame frame)
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
} // namespace DXHelper
