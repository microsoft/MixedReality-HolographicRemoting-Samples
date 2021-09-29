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

#include <pch.h>

#include <DeviceResourcesD3D11.h>

namespace DXHelper
{
    // Constructor for DeviceResources.
    DeviceResourcesD3D11::DeviceResourcesD3D11()
    {
        CreateDeviceIndependentResources();
    }

    DeviceResourcesD3D11::~DeviceResourcesD3D11()
    {
        if (m_d3dContext)
        {
            m_d3dContext->Flush();
            m_d3dContext->ClearState();
        }
    }

    // Configures resources that don't depend on the Direct3D device.
    void DeviceResourcesD3D11::CreateDeviceIndependentResources()
    {
        // Initialize Direct2D resources.
        D2D1_FACTORY_OPTIONS options{};

#if defined(_DEBUG)
        // If the project is in a debug build, enable Direct2D debugging via SDK Layers.
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

        // Initialize the Direct2D Factory.
        m_d2dFactory = nullptr;
        winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, m_d2dFactory.put()));

        // Initialize the DirectWrite Factory.
        m_dwriteFactory = nullptr;
        winrt::check_hresult(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2), reinterpret_cast<IUnknown**>(m_dwriteFactory.put_void())));

        // Initialize the Windows Imaging Component (WIC) Factory.
        m_wicFactory = nullptr;
        winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(m_wicFactory.put())));
    }

    // Configures the Direct3D device, and stores handles to it and the device context.
    void DeviceResourcesD3D11::CreateDeviceResources()
    {
        // This flag adds support for surfaces with a different color channel ordering
        // than the API default. It is required for compatibility with Direct2D.
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#ifdef _DEBUG
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
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_12_1,
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

    void DeviceResourcesD3D11::NotifyDeviceLost()
    {
        if (m_deviceNotify != nullptr)
        {
            m_deviceNotify->OnDeviceLost();
        }
    }

    void DeviceResourcesD3D11::NotifyDeviceRestored()
    {
        if (m_deviceNotify != nullptr)
        {
            m_deviceNotify->OnDeviceRestored();
        }
    }

    // Register our DeviceNotify to be informed on device lost and creation.
    void DeviceResourcesD3D11::RegisterDeviceNotify(IDeviceNotify* deviceNotify)
    {
        m_deviceNotify = deviceNotify;
    }

    // Call this method when the app suspends. It provides a hint to the driver that the app
    // is entering an idle state and that temporary buffers can be reclaimed for use by other apps.
    void DeviceResourcesD3D11::Trim()
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
} // namespace DXHelper
