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

#include <CameraResourcesD3D11Holographic.h>

#include <DeviceResourcesD3D11Holographic.h>
#include <DirectXHelper.h>

#include <windows.graphics.directx.direct3d11.interop.h>

#include <winrt/Windows.Perception.Spatial.h>

using namespace DirectX;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Perception::Spatial;

namespace DXHelper
{
    CameraResourcesD3D11Holographic::CameraResourcesD3D11Holographic(HolographicCamera const& camera, DXGI_FORMAT renderTargetViewFormat)
        : m_holographicCamera(camera)
        , m_isStereo(camera.IsStereo())
        , m_isOpaque(camera.Display().IsOpaque())
        , m_d3dRenderTargetSize(camera.RenderTargetSize())
        , m_dxgiFormat(DXGI_FORMAT_UNKNOWN)
        , m_renderTargetViewFormat(renderTargetViewFormat)
    {
        m_d3dViewport = CD3D11_VIEWPORT(0.f, 0.f, m_d3dRenderTargetSize.Width, m_d3dRenderTargetSize.Height);
    };

    // Updates resources associated with a holographic camera's swap chain.
    // The app does not access the swap chain directly, but it does create
    // resource views for the back buffer.
    void CameraResourcesD3D11Holographic::CreateResourcesForBackBuffer(
        DeviceResourcesD3D11Holographic* pDeviceResources, HolographicCameraRenderingParameters const& cameraParameters)
    {
        ID3D11Device* device = pDeviceResources->GetD3DDevice();

        // Get the WinRT object representing the holographic camera's back buffer.
        IDirect3DSurface surface = cameraParameters.Direct3D11BackBuffer();

        // Get the holographic camera's back buffer.
        // Holographic apps do not create a swap chain themselves; instead, buffers are
        // owned by the system. The Direct3D back buffer resources are provided to the
        // app using WinRT interop APIs.
        winrt::com_ptr<ID3D11Texture2D> cameraBackBuffer;
        winrt::check_hresult(surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>()->GetInterface(
            IID_PPV_ARGS(cameraBackBuffer.put())));

        // Determine if the back buffer has changed. If so, ensure that the render target view
        // is for the current back buffer.
        if (m_d3dBackBuffer.get() != cameraBackBuffer.get())
        {
            // This can change every frame as the system moves to the next buffer in the
            // swap chain. This mode of operation will occur when certain rendering modes
            // are activated.
            m_d3dBackBuffer = cameraBackBuffer;

            // Get the DXGI format for the back buffer.
            // This information can be accessed by the app using CameraResources::GetBackBufferDXGIFormat().
            D3D11_TEXTURE2D_DESC backBufferDesc;
            m_d3dBackBuffer->GetDesc(&backBufferDesc);
            m_dxgiFormat = backBufferDesc.Format;

            // Note, for Holographic Remoting you should explicitly specify the format as DXGI_FORMAT_B8G8R8A8_UNORM. This ensures that the
            // video data coming from the remote side is displayed as is without any automatic format conversion.
            D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
            viewDesc.ViewDimension = backBufferDesc.ArraySize > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DARRAY : D3D11_RTV_DIMENSION_TEXTURE2D;
            viewDesc.Format = m_renderTargetViewFormat;
            if (backBufferDesc.ArraySize > 1)
            {
                viewDesc.Texture2DArray.ArraySize = backBufferDesc.ArraySize;
            }

            // Create a render target view of the back buffer.
            // Creating this resource is inexpensive, and is better than keeping track of
            // the back buffers in order to pre-allocate render target views for each one.
            m_d3dRenderTargetView = nullptr;
            winrt::check_hresult(device->CreateRenderTargetView(m_d3dBackBuffer.get(), &viewDesc, m_d3dRenderTargetView.put()));

            // Check for render target size changes.
            winrt::Windows::Foundation::Size currentSize = m_holographicCamera.RenderTargetSize();
            if (m_d3dRenderTargetSize != currentSize)
            {
                // Set render target size.
                m_d3dRenderTargetSize = currentSize;

                // A new depth stencil view is also needed.
                m_d3dDepthStencil = nullptr;
                m_d3dDepthStencilView = nullptr;
            }
        }

        // Refresh depth stencil resources, if needed.
        if (m_d3dDepthStencilView == nullptr)
        {
            // Create a depth stencil view for use with 3D rendering if needed.
            CD3D11_TEXTURE2D_DESC depthStencilDesc(
                DXGI_FORMAT_R16_TYPELESS,
                static_cast<UINT>(m_d3dRenderTargetSize.Width),
                static_cast<UINT>(m_d3dRenderTargetSize.Height),
                m_isStereo ? 2 : 1, // Create two textures when rendering in stereo.
                1,                  // Use a single mipmap level.
                D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);

            // Allow sharing by default for easier interop with future D3D12 components for processing the remote or local frame.
            // This is optional, but without the flag any D3D12 components need to perform an additional copy.
            depthStencilDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

            m_d3dDepthStencil = nullptr;
            winrt::check_hresult(device->CreateTexture2D(&depthStencilDesc, nullptr, m_d3dDepthStencil.put()));

            CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(
                m_isStereo ? D3D11_DSV_DIMENSION_TEXTURE2DARRAY : D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D16_UNORM);
            winrt::check_hresult(
                device->CreateDepthStencilView(m_d3dDepthStencil.get(), &depthStencilViewDesc, m_d3dDepthStencilView.put()));
        }

        // Create the constant buffer, if needed.
        if (m_viewProjectionConstantBuffer == nullptr)
        {
            // Create a constant buffer to store view and projection matrices for the camera.
            CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
            constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
            constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            winrt::check_hresult(device->CreateBuffer(&constantBufferDesc, nullptr, m_viewProjectionConstantBuffer.put()));
        }
    }

    // Releases resources associated with a back buffer.
    void CameraResourcesD3D11Holographic::ReleaseResourcesForBackBuffer(DeviceResourcesD3D11Holographic* pDeviceResources)
    {
        // Release camera-specific resources.
        m_d3dBackBuffer = nullptr;
        m_d3dDepthStencil = nullptr;
        m_d3dRenderTargetView = nullptr;
        m_d3dDepthStencilView = nullptr;
        m_viewProjectionConstantBuffer = nullptr;

        pDeviceResources->UseD3DDeviceContext([](auto context) {
            // Ensure system references to the back buffer are released by clearing the render
            // target from the graphics pipeline state, and then flushing the Direct3D context.
            ID3D11RenderTargetView* nullViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {nullptr};
            context->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
            context->Flush();
        });
    }

    // Updates the view/projection constant buffer for a holographic camera.
    void CameraResourcesD3D11Holographic::UpdateViewProjectionBuffer(
        const std::shared_ptr<DeviceResourcesD3D11Holographic>& deviceResources,
        HolographicCameraPose const& cameraPose,
        SpatialCoordinateSystem const& coordinateSystem)
    {
        // The system changes the viewport on a per-frame basis for system optimizations.
        auto viewport = cameraPose.Viewport();
        m_d3dViewport = CD3D11_VIEWPORT(viewport.X, viewport.Y, viewport.Width, viewport.Height);

        // The projection transform for each frame is provided by the HolographicCameraPose.
        m_cameraProjectionTransform = cameraPose.ProjectionTransform();

        ViewProjectionConstantBuffer viewProjectionConstantBufferData = {};
        bool viewTransformAcquired = false;

        // MakeDropCmd-StripStart
        if (m_freezeCamera && m_frozenCoordinateSystem)
        {
            // If the camera is frozen and we have a coordinate system from the previous frame, try to translate it
            // to the current coordinate system, and override the view transform on the holographic camera.
            auto coordinateTransformContainer = coordinateSystem.TryGetTransformTo(m_frozenCoordinateSystem);

            viewTransformAcquired = coordinateTransformContainer != nullptr;
            if (viewTransformAcquired)
            {
                cameraPose.OverrideViewTransform(m_frozenCoordinateSystem, m_frozenViewTransform);

                auto coordinateTransform = coordinateTransformContainer.Value();

                XMStoreFloat4x4(
                    &viewProjectionConstantBufferData.viewProjection[0],
                    XMMatrixTranspose(
                        XMLoadFloat4x4(&coordinateTransform) * XMLoadFloat4x4(&m_frozenViewTransform.Left) *
                        XMLoadFloat4x4(&m_cameraProjectionTransform.Left)));
                XMStoreFloat4x4(
                    &viewProjectionConstantBufferData.viewProjection[1],
                    XMMatrixTranspose(
                        XMLoadFloat4x4(&coordinateTransform) * XMLoadFloat4x4(&m_frozenViewTransform.Right) *
                        XMLoadFloat4x4(&m_cameraProjectionTransform.Right)));
            }
        }
        else
        // MakeDropCmd-StripEnd
        {

            // Get a container object with the view and projection matrices for the given
            // pose in the given coordinate system.
            auto viewTransformContainer = cameraPose.TryGetViewTransform(coordinateSystem);

            // If TryGetViewTransform returns a null pointer, that means the pose and coordinate
            // system cannot be understood relative to one another; content cannot be rendered
            // in this coordinate system for the duration of the current frame.
            // This usually means that positional tracking is not active for the current frame, in
            // which case it is possible to use a SpatialLocatorAttachedFrameOfReference to render
            // content that is not world-locked instead.
            viewTransformAcquired = viewTransformContainer != nullptr;
            if (viewTransformAcquired)
            {
                // Otherwise, the set of view transforms can be retrieved.
                HolographicStereoTransform viewCoordinateSystemTransform = viewTransformContainer.Value();

                // Update the view matrices. Holographic cameras (such as Microsoft HoloLens) are
                // constantly moving relative to the world. The view matrices need to be updated
                // every frame.
                XMStoreFloat4x4(
                    &viewProjectionConstantBufferData.viewProjection[0],
                    XMMatrixTranspose(
                        XMLoadFloat4x4(&viewCoordinateSystemTransform.Left) * XMLoadFloat4x4(&m_cameraProjectionTransform.Left)));
                XMStoreFloat4x4(
                    &viewProjectionConstantBufferData.viewProjection[1],
                    XMMatrixTranspose(
                        XMLoadFloat4x4(&viewCoordinateSystemTransform.Right) * XMLoadFloat4x4(&m_cameraProjectionTransform.Right)));

                // MakeDropCmd-StripStart
                if (m_freezeCamera)
                {
                    // If camera freezing is requested, store the used coordinate system and matrices to reuse in the next frame.
                    m_frozenCoordinateSystem = coordinateSystem;
                    m_frozenViewTransform = viewCoordinateSystemTransform;
                }
                else
                {
                    m_frozenCoordinateSystem = nullptr;
                }
                // MakeDropCmd-StripEnd
            }
        }

        // Use the D3D device context to update Direct3D device-based resources.
        deviceResources->UseD3DDeviceContext([&](auto context) {
            // Loading is asynchronous. Resources must be created before they can be updated.
            if (context == nullptr || m_viewProjectionConstantBuffer == nullptr || !viewTransformAcquired)
            {
                m_framePending = false;
            }
            else
            {
                // Update the view and projection matrices.
                D3D11_MAPPED_SUBRESOURCE resource;
                context->Map(m_viewProjectionConstantBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
                memcpy(resource.pData, &viewProjectionConstantBufferData, sizeof(viewProjectionConstantBufferData));
                context->Unmap(m_viewProjectionConstantBuffer.get(), 0);

                m_framePending = true;
            }
        });
    }

    // Gets the view-projection constant buffer for the HolographicCamera and attaches it
    // to the shader pipeline.
    bool CameraResourcesD3D11Holographic::AttachViewProjectionBuffer(std::shared_ptr<DeviceResourcesD3D11Holographic>& deviceResources)
    {
        // This method uses Direct3D device-based resources.
        return deviceResources->UseD3DDeviceContext([&](auto context) {
            // Loading is asynchronous. Resources must be created before they can be updated.
            // Cameras can also be added asynchronously, in which case they must be initialized
            // before they can be used.
            if (context == nullptr || m_viewProjectionConstantBuffer == nullptr || m_framePending == false)
            {
                return false;
            }

            // Set the viewport for this camera.
            context->RSSetViewports(1, &m_d3dViewport);

            // Send the constant buffer to the vertex shader.
            ID3D11Buffer* viewProjectionConstantBuffers[1] = {m_viewProjectionConstantBuffer.get()};
            context->VSSetConstantBuffers(1, 1, viewProjectionConstantBuffers);

            // The template includes a pass-through geometry shader that is used by
            // default on systems that don't support the D3D11_FEATURE_D3D11_OPTIONS3::
            // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer extension. The shader
            // will be enabled at run-time on systems that require it.
            // If your app will also use the geometry shader for other tasks and those
            // tasks require the view/projection matrix, uncomment the following line
            // of code to send the constant buffer to the geometry shader as well.
            /*context->GSSetConstantBuffers(
                1,
                1,
                m_viewProjectionConstantBuffer.GetAddressOf()
                );*/

            m_framePending = false;

            return true;
        });
    }

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface CameraResourcesD3D11Holographic::GetDepthStencilTextureInteropObject()
    {
        // Direct3D interop APIs are used to provide the buffer to the WinRT API.
        winrt::com_ptr<IDXGIResource1> depthStencilResource;
        winrt::check_bool(m_d3dDepthStencil.try_as(depthStencilResource));
        winrt::com_ptr<IDXGISurface2> depthDxgiSurface;
        winrt::check_hresult(depthStencilResource->CreateSubresourceSurface(0, depthDxgiSurface.put()));
        winrt::com_ptr<::IInspectable> inspectableSurface;
        winrt::check_hresult(CreateDirect3D11SurfaceFromDXGISurface(
            depthDxgiSurface.get(), reinterpret_cast<IInspectable**>(winrt::put_abi(inspectableSurface))));
        return inspectableSurface.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface>();
    }
} // namespace DXHelper
