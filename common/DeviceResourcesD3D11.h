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

#pragma once

#include <winrt/base.h>

#include <mutex>

#define ARRAY_SIZE(a) (std::extent<decltype(a)>::value)

#include <d2d1_2.h>
#include <d3d11_4.h>
#include <dwrite_2.h>
#include <dxgi1_4.h>
#include <wincodec.h>

#include <DirectXSdkLayerSupport.h>

namespace DXHelper
{
    // Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
    struct IDeviceNotify
    {
        virtual void OnDeviceLost() = 0;
        virtual void OnDeviceRestored() = 0;
    };

    // Creates and manages a Direct3D device and immediate context, Direct2D device and context (for debug), and the holographic swap chain.
    class DeviceResourcesD3D11
    {
    public:
        DeviceResourcesD3D11();
        virtual ~DeviceResourcesD3D11();

        // Public methods related to Direct3D devices.
        void RegisterDeviceNotify(IDeviceNotify* deviceNotify);
        void Trim();

        // D3D accessors.
        ID3D11Device4* GetD3DDevice() const
        {
            return m_d3dDevice.get();
        }
        template <typename F>
        auto UseD3DDeviceContext(F func) const
        {
            std::scoped_lock lock(m_d3dContextMutex);
            return func(m_d3dContext.get());
        }
        D3D_FEATURE_LEVEL GetDeviceFeatureLevel() const
        {
            return m_d3dFeatureLevel;
        }
        bool GetDeviceSupportsVprt() const
        {
            return m_supportsVprt;
        }

        // DXGI acessors.
        IDXGIAdapter3* GetDXGIAdapter() const
        {
            return m_dxgiAdapter.get();
        }

        // D2D accessors.
        ID2D1Factory2* GetD2DFactory() const
        {
            return m_d2dFactory.get();
        }
        IDWriteFactory2* GetDWriteFactory() const
        {
            return m_dwriteFactory.get();
        }
        IWICImagingFactory2* GetWicImagingFactory() const
        {
            return m_wicFactory.get();
        }

    protected:
        // Private methods related to the Direct3D device, and resources based on that device.
        void CreateDeviceIndependentResources();
        virtual void CreateDeviceResources();

        void NotifyDeviceLost();
        void NotifyDeviceRestored();

        // Direct3D objects.
        winrt::com_ptr<ID3D11Device4> m_d3dDevice;
        mutable std::recursive_mutex m_d3dContextMutex;
        winrt::com_ptr<ID3D11DeviceContext3> m_d3dContext;
        winrt::com_ptr<IDXGIAdapter3> m_dxgiAdapter;

        // Direct2D factories.
        winrt::com_ptr<ID2D1Factory2> m_d2dFactory;
        winrt::com_ptr<IDWriteFactory2> m_dwriteFactory;
        winrt::com_ptr<IWICImagingFactory2> m_wicFactory;

        // Properties of the Direct3D device currently in use.
        D3D_FEATURE_LEVEL m_d3dFeatureLevel = D3D_FEATURE_LEVEL_10_0;

        // The IDeviceNotify can be held directly as it owns the DeviceResources.
        IDeviceNotify* m_deviceNotify = nullptr;

        // Whether or not the current Direct3D device supports the optional feature
        // for setting the render target array index from the vertex shader stage.
        bool m_supportsVprt = false;
    };
} // namespace DXHelper
