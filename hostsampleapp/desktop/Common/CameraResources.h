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

using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Perception::Spatial;

namespace DXHelper
{
    class DeviceResources;

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
    class CameraResources
    {
    public:
        CameraResources(const HolographicCamera& holographicCamera);

        void CreateResourcesForBackBuffer(
            DXHelper::DeviceResources* pDeviceResources, const HolographicCameraRenderingParameters& cameraParameters);
        void ReleaseResourcesForBackBuffer(DXHelper::DeviceResources* pDeviceResources);

        void UpdateViewProjectionBuffer(
            std::shared_ptr<DXHelper::DeviceResources> deviceResources,
            const HolographicCameraPose& cameraPose,
            const SpatialCoordinateSystem& coordinateSystem);

        bool AttachViewProjectionBuffer(std::shared_ptr<DXHelper::DeviceResources> deviceResources);

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
        D3D11_VIEWPORT GetViewport() const
        {
            return m_d3dViewport;
        }
        DXGI_FORMAT GetBackBufferDXGIFormat() const
        {
            return m_dxgiFormat;
        }

        // Render target properties.
        winrt::Windows::Foundation::Size GetRenderTargetSize() const
        {
            return m_d3dRenderTargetSize;
        }
        bool IsRenderingStereoscopic() const
        {
            return m_isStereo;
        }

        // The holographic camera these resources are for.
        const HolographicCamera& GetHolographicCamera() const
        {
            return m_holographicCamera;
        }

    private:
        // Direct3D rendering objects. Required for 3D.
        winrt::com_ptr<ID3D11RenderTargetView> m_d3dRenderTargetView;
        winrt::com_ptr<ID3D11DepthStencilView> m_d3dDepthStencilView;
        winrt::com_ptr<ID3D11Texture2D> m_d3dBackBuffer;

        // Device resource to store view and projection matrices.
        winrt::com_ptr<ID3D11Buffer> m_viewProjectionConstantBuffer;

        // Direct3D rendering properties.
        DXGI_FORMAT m_dxgiFormat;
        winrt::Windows::Foundation::Size m_d3dRenderTargetSize;
        D3D11_VIEWPORT m_d3dViewport;

        // Indicates whether the camera supports stereoscopic rendering.
        bool m_isStereo = false;

        // Indicates whether this camera has a pending frame.
        bool m_framePending = false;

        // Pointer to the holographic camera these resources are for.
        HolographicCamera m_holographicCamera = nullptr;
    };
} // namespace DXHelper
