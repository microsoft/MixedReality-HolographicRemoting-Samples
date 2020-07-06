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

#include <Content/PerceptionDeviceHandler.h>
#include <Content/QRCodeRenderer.h>
#include <Content/QRCodeTracker.h>

#include <Common/DirectXHelper.h>
#include <Common/PerceptionTypes.h>

#include <Content/FrustumCulling.h>

#include <winrt/Windows.Perception.Spatial.h>

namespace
{
    using namespace DirectX;

    void AppendColoredTriangle(
        winrt::Windows::Foundation::Numerics::float3 p0,
        winrt::Windows::Foundation::Numerics::float3 p1,
        winrt::Windows::Foundation::Numerics::float3 p2,
        winrt::Windows::Foundation::Numerics::float3 color,
        std::vector<VertexPositionNormalColor>& vertices)
    {
        VertexPositionNormalColor vertex;
        vertex.color = XMFLOAT3(&color.x);
        vertex.normal = XMFLOAT3(0.0f, 0.0f, 0.0f);

        vertex.pos = XMFLOAT3(&p0.x);
        vertices.push_back(vertex);
        vertex.pos = XMFLOAT3(&p1.x);
        vertices.push_back(vertex);
        vertex.pos = XMFLOAT3(&p2.x);
        vertices.push_back(vertex);
    }
} // namespace

QRCodeRenderer::QRCodeRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources)
    : RenderableObject(deviceResources)
{
}

void QRCodeRenderer::Update(
    PerceptionDeviceHandler& perceptionDeviceHandler,
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem)
{
    auto processQRCode = [this, renderingCoordinateSystem](QRCode& code) {
        auto codeCS = code.GetCoordinateSystem();
        float size = code.GetPhysicalSize();
        auto codeToRendering = codeCS.TryGetTransformTo(renderingCoordinateSystem);
        if (!codeToRendering)
        {
            return;
        }
        else
        {
            m_renderableQRCodes.push_back({size, codeToRendering.Value()});
        }
    };

    m_renderableQRCodes.clear();
    auto processQRCodeTracker = [this, processQRCode](QRCodeTracker& tracker) { tracker.ForEachQRCode(processQRCode); };

    m_vertices.clear();
    perceptionDeviceHandler.ForEachRootObjectOfType<QRCodeTracker>(processQRCodeTracker);

    auto modelTransform = winrt::Windows::Foundation::Numerics::float4x4::identity();
    UpdateModelConstantBuffer(modelTransform);
}

void QRCodeRenderer::Draw(unsigned int numInstances, winrt::Windows::Foundation::IReference<SpatialBoundingFrustum> cullingFrustum)
{
    for (RenderableQRCode code : m_renderableQRCodes)
    {
        // Frustum culling
        winrt::Windows::Foundation::Numerics::float3 center =
            winrt::Windows::Foundation::Numerics::transform({0, 0, 0}, code.codeToRendering);
        float radius = sqrtf(2 * code.size * code.size);

        if (FrustumCulling::SphereInFrustum(center, radius, cullingFrustum))
        {
            winrt::Windows::Foundation::Numerics::float3 positions[4] = {
                {0.0f, 0.0f, 0.0f}, {0.0f, code.size, 0.0f}, {code.size, code.size, 0.0f}, {code.size, 0.0f, 0.0f}};
            for (int i = 0; i < 4; ++i)
            {
                positions[i] = winrt::Windows::Foundation::Numerics::transform(positions[i], code.codeToRendering);
            }

            winrt::Windows::Foundation::Numerics::float3 col{1.0f, 1.0f, 0.0f};
            AppendColoredTriangle(positions[0], positions[2], positions[1], col, m_vertices);
            AppendColoredTriangle(positions[0], positions[3], positions[2], col, m_vertices);
        }
    }

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
