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

#include <winrt/Windows.Graphics.Holographic.h>

#include <d3d11.h>

namespace DXHelper
{
    class DeviceResourcesD3D11Holographic;

    // Constant buffer used to send the view-projection matrices to the shader pipeline.
    struct ViewProjectionConstantBuffer
    {
        DirectX::XMFLOAT4X4 viewProjection[2];
    };

    // Assert that the constant buffer remains 16-byte aligned (best practice).
    static_assert(
        (sizeof(ViewProjectionConstantBuffer) % (sizeof(float) * 4)) == 0,
        "ViewProjection constant buffer size must be 16-byte aligned (16 bytes is the length of four floats).");

    // Manages DirectX device resources that are specific to a holographic camera, such as the
    // back buffer, ViewProjection constant buffer, and viewport.
    class CameraResourcesD3D11Holographic
    {
    public:
        CameraResourcesD3D11Holographic(
            winrt::Windows::Graphics::Holographic::HolographicCamera const& holographicCamera, DXGI_FORMAT renderTargetViewFormat);

        void CreateResourcesForBackBuffer(
            DeviceResourcesD3D11Holographic* pDeviceResources,
            winrt::Windows::Graphics::Holographic::HolographicCameraRenderingParameters const& cameraParameters);
        void ReleaseResourcesForBackBuffer(DeviceResourcesD3D11Holographic* pDeviceResources);

        void UpdateViewProjectionBuffer(
            const std::shared_ptr<DeviceResourcesD3D11Holographic>& deviceResources,
            winrt::Windows::Graphics::Holographic::HolographicCameraPose const& cameraPose,
            winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& coordinateSystem);

        bool AttachViewProjectionBuffer(std::shared_ptr<DeviceResourcesD3D11Holographic>& deviceResources);

        // Direct3D device resources.
        ID3D11RenderTargetView* GetBackBufferRenderTargetView() const
        {
            return m_d3dRenderTargetView.get();
        }
        ID3D11DepthStencilView* GetDepthStencilView() const
        {
            return m_d3dDepthStencilView.get();
        }
        ID3D11Texture2D* GetBackBufferTexture2D() const
        {
            return m_d3dBackBuffer.get();
        }
        ID3D11Texture2D* GetDepthStencilTexture2D() const
        {
            return m_d3dDepthStencil.get();
        }
        D3D11_VIEWPORT GetViewport() const
        {
            return m_d3dViewport;
        }
        DXGI_FORMAT GetBackBufferDXGIFormat() const
        {
            return m_dxgiFormat;
        }

        // Render target properties.
        winrt::Windows::Foundation::Size GetRenderTargetSize() const&
        {
            return m_d3dRenderTargetSize;
        }
        bool IsRenderingStereoscopic() const
        {
            return m_isStereo;
        }

        bool IsOpaque() const
        {
            return m_isOpaque;
        }
        // The holographic camera these resources are for.
        winrt::Windows::Graphics::Holographic::HolographicCamera const& GetHolographicCamera() const
        {
            return m_holographicCamera;
        }

        winrt::Windows::Graphics::Holographic::HolographicStereoTransform GetProjectionTransform()
        {
            return m_cameraProjectionTransform;
        }

        // MakeDropCmd-StripStart
        // Freeze the camera position for debugging
        void SetFreezeCameraPosition(bool freezeCamera)
        {
            m_freezeCamera = freezeCamera;
        }
        // MakeDropCmd-StripEnd

        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface GetDepthStencilTextureInteropObject();

    private:
        // Direct3D rendering objects. Required for 3D.
        winrt::com_ptr<ID3D11RenderTargetView> m_d3dRenderTargetView;
        winrt::com_ptr<ID3D11DepthStencilView> m_d3dDepthStencilView;
        winrt::com_ptr<ID3D11Texture2D> m_d3dBackBuffer;
        winrt::com_ptr<ID3D11Texture2D> m_d3dDepthStencil;

        // Device resource to store view and projection matrices.
        winrt::com_ptr<ID3D11Buffer> m_viewProjectionConstantBuffer;

        // Direct3D rendering properties.
        DXGI_FORMAT m_dxgiFormat = DXGI_FORMAT_UNKNOWN;
        DXGI_FORMAT m_renderTargetViewFormat;
        winrt::Windows::Foundation::Size m_d3dRenderTargetSize = {};
        D3D11_VIEWPORT m_d3dViewport = {};

        // Indicates whether the camera supports stereoscopic rendering.
        bool m_isStereo = false;
        bool m_isOpaque = false;

        // MakeDropCmd-StripStart
        // If true, the camera position is frozen in place, ignoring the device position.
        bool m_freezeCamera = false;
        winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_frozenCoordinateSystem = nullptr;
        winrt::Windows::Graphics::Holographic::HolographicStereoTransform m_frozenViewTransform;
        // MakeDropCmd-StripEnd

        // Indicates whether this camera has a pending frame.
        bool m_framePending = false;

        // Pointer to the holographic camera these resources are for.
        winrt::Windows::Graphics::Holographic::HolographicCamera m_holographicCamera = nullptr;

        winrt::Windows::Graphics::Holographic::HolographicStereoTransform m_cameraProjectionTransform;
    };
} // namespace DXHelper
