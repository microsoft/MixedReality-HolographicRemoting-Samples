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

#include <chrono>

#include <content/QRCodeRenderer.h>

#include <d3d11/DirectXHelper.h>
#include <holographic/FrustumCulling.h>

#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Perception.Spatial.Preview.h>

using namespace winrt::Microsoft::MixedReality::QR;
using namespace winrt::Windows::Foundation::Numerics;

QRCodeRenderer::QRCodeRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources)
    : RenderableObject(deviceResources)
{
}

void QRCodeRenderer::OnAddedQRCode(const winrt::Microsoft::MixedReality::QR::QRCode& code)
{
    std::scoped_lock lock(m_mutex);
    m_qrCodes.insert({code, nullptr});
}

void QRCodeRenderer::OnUpdatedQRCode(const winrt::Microsoft::MixedReality::QR::QRCode& code)
{
    std::scoped_lock lock(m_mutex);

    if (m_qrCodes.find(code) != m_qrCodes.end())
    {
        // Remove the old entry;
        m_qrCodes.erase(code);
    }

    m_qrCodes.insert({code, nullptr});
}

void QRCodeRenderer::Update(winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem)
{
    std::scoped_lock lock(m_mutex);
    m_renderableQrCodes.clear();

    for (auto& [code, coordinateSystem] : m_qrCodes)
    {
        winrt::Windows::Foundation::IReference<float4x4> qrToRenderingRef = nullptr;
        if (coordinateSystem == nullptr)
        {
            try
            {
                coordinateSystem = Preview::SpatialGraphInteropPreview::CreateCoordinateSystemForNode(code.SpatialGraphNodeId());
            }
            catch (winrt::hresult_error const&)
            {
                coordinateSystem = nullptr;
                continue;
            }
        }

        if (coordinateSystem)
        {
            qrToRenderingRef = coordinateSystem.TryGetTransformTo(renderingCoordinateSystem);

            if (qrToRenderingRef)
            {
                m_renderableQrCodes.push_back({code.PhysicalSideLength(), qrToRenderingRef.Value()});
            }
        }
    }

    // The vertices are already in rendering space.
    auto modelTransform = winrt::Windows::Foundation::Numerics::float4x4::identity();
    UpdateModelConstantBuffer(modelTransform);
}

void QRCodeRenderer::Draw(unsigned int numInstances, winrt::Windows::Foundation::IReference<SpatialBoundingFrustum> cullingFrustum)
{
    std::scoped_lock lock(m_mutex);

    // Clear the vertices.
    m_vertices.clear();

    for (auto renderableCode : m_renderableQrCodes)
    {
        // Apply frustum culling.
        const float size = renderableCode.size;
        winrt::Windows::Foundation::Numerics::float3 center =
            winrt::Windows::Foundation::Numerics::transform({0, 0, 0}, renderableCode.codeToRendering);
        float radius = sqrtf(2 * size * size);

        if (FrustumCulling::SphereInFrustum(center, radius, cullingFrustum))
        {
            float3 positions[4] = {{0.0f, 0.0f, 0.0f}, {0.0f, size, 0.0f}, {size, size, 0.0f}, {size, 0.0f, 0.0f}};
            for (int i = 0; i < 4; ++i)
            {
                // Transform from entity to rendering space.
                positions[i] = transform(positions[i], renderableCode.codeToRendering);
            }

            float3 col{1.0f, 0.76f, 0.0f};
            AppendColoredTriangle(positions[0], positions[2], positions[1], col, m_vertices);
            AppendColoredTriangle(positions[0], positions[3], positions[2], col, m_vertices);
        }
    }

    // Only render if vertices are available.
    if (m_vertices.empty())
    {
        return;
    }

    const UINT stride = sizeof(m_vertices[0]);
    const UINT offset = 0;
    D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
    vertexBufferData.pSysMem = m_vertices.data();
    const CD3D11_BUFFER_DESC vertexBufferDesc(static_cast<UINT>(m_vertices.size() * stride), D3D11_BIND_VERTEX_BUFFER);
    winrt::com_ptr<ID3D11Buffer> vertexBuffer;
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, vertexBuffer.put()));

    m_deviceResources->UseD3DDeviceContext([&](auto context) {
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ID3D11Buffer* pBuffer = vertexBuffer.get();
        context->IASetVertexBuffers(0, 1, &pBuffer, &stride, &offset);
        context->DrawInstanced(static_cast<UINT>(m_vertices.size()), numInstances, offset, 0);
    });
}

void QRCodeRenderer::Reset()
{
    std::scoped_lock lock(m_mutex);

    m_qrCodes.clear();
    m_renderableQrCodes.clear();
    m_vertices.clear();
}
