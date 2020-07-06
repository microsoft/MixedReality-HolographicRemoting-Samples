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

#include <d3d11.h>

#include <winrt/base.h>

#include <mutex>

#define ARRAY_SIZE(a) (std::extent<decltype(a)>::value)

struct ID3D11Device4;
struct IDXGIAdapter3;
struct ID2D1Factory2;
struct IDWriteFactory2;
struct IWICImagingFactory2;
struct ID3D11DeviceContext3;

namespace DXHelper
{
    // Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
    struct IDeviceNotify
    {
        virtual void OnDeviceLost() = 0;
        virtual void OnDeviceRestored() = 0;
    };

    // Creates and manages a Direct3D device and immediate context, Direct2D device and context (for debug), and the holographic swap chain.
    class DeviceResourcesCommon
    {
    public:
        DeviceResourcesCommon();
        virtual ~DeviceResourcesCommon();

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

    template <typename F>
    void D3D11StoreAndRestoreState(ID3D11DeviceContext* immediateContext, F customRenderingCode)
    {
        // Query the d3d11 state before rendering
        static_assert(
            sizeof(winrt::com_ptr<ID3D11ShaderResourceView>) == sizeof(void*),
            "Below code reiles on winrt::com_ptr being exactly one pointer in size");

        winrt::com_ptr<ID3D11VertexShader> vertexShader;
        winrt::com_ptr<ID3D11GeometryShader> geometryShader;
        winrt::com_ptr<ID3D11PixelShader> pixelShader;
        winrt::com_ptr<ID3D11Buffer> vsConstantBuffers[2], psConstantBuffers[3];
        winrt::com_ptr<ID3D11ShaderResourceView> views[4] = {};
        winrt::com_ptr<ID3D11SamplerState> psSampler;
        winrt::com_ptr<ID3D11RasterizerState> rasterizerState;
        winrt::com_ptr<ID3D11DepthStencilState> depthStencilState;
        winrt::com_ptr<ID3D11BlendState> blendState;
        winrt::com_ptr<ID3D11InputLayout> inputLayout;
        FLOAT blendFactor[4] = {};
        UINT sampleMask = 0;
        D3D11_PRIMITIVE_TOPOLOGY primitiveTopoloy = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        UINT stencilRef = 0;

        immediateContext->VSGetShader(vertexShader.put(), nullptr, nullptr);
        immediateContext->VSGetConstantBuffers(0, ARRAY_SIZE(vsConstantBuffers), reinterpret_cast<ID3D11Buffer**>(vsConstantBuffers));
        immediateContext->GSGetShader(geometryShader.put(), nullptr, nullptr);
        immediateContext->PSGetShader(pixelShader.put(), nullptr, nullptr);
        immediateContext->PSGetShaderResources(0, ARRAY_SIZE(views), reinterpret_cast<ID3D11ShaderResourceView**>(views));
        immediateContext->PSGetConstantBuffers(0, ARRAY_SIZE(psConstantBuffers), reinterpret_cast<ID3D11Buffer**>(psConstantBuffers));
        immediateContext->PSGetSamplers(0, 1, psSampler.put());
        immediateContext->RSGetState(rasterizerState.put());
        immediateContext->OMGetDepthStencilState(depthStencilState.put(), &stencilRef);
        immediateContext->OMGetBlendState(blendState.put(), blendFactor, &sampleMask);
        immediateContext->IAGetPrimitiveTopology(&primitiveTopoloy);
        immediateContext->IAGetInputLayout(inputLayout.put());

        customRenderingCode();

        // Restore the d3d11 state.
        immediateContext->VSSetShader(vertexShader.get(), nullptr, 0);
        immediateContext->VSSetConstantBuffers(0, ARRAY_SIZE(vsConstantBuffers), reinterpret_cast<ID3D11Buffer**>(vsConstantBuffers));
        immediateContext->GSSetShader(geometryShader.get(), nullptr, 0);
        immediateContext->PSSetShader(pixelShader.get(), nullptr, 0);
        immediateContext->PSSetShaderResources(0, ARRAY_SIZE(views), reinterpret_cast<ID3D11ShaderResourceView**>(views));
        immediateContext->PSSetConstantBuffers(0, ARRAY_SIZE(psConstantBuffers), reinterpret_cast<ID3D11Buffer**>(psConstantBuffers));
        immediateContext->PSSetSamplers(0, 1, reinterpret_cast<ID3D11SamplerState**>(&psSampler));
        immediateContext->RSSetState(rasterizerState.get());
        immediateContext->OMSetDepthStencilState(depthStencilState.get(), stencilRef);
        immediateContext->OMSetBlendState(blendState.get(), blendFactor, sampleMask);
        immediateContext->IASetPrimitiveTopology(primitiveTopoloy);
        immediateContext->IASetInputLayout(inputLayout.get());
    }
} // namespace DXHelper
