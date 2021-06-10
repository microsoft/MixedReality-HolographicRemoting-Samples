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

#include <holographic/RenderableObject.h>

#include <d3d11/DirectXHelper.h>

RenderableObject::RenderableObject(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources)
    : m_deviceResources(deviceResources)
{
    m_deviceResourcesCreated = CreateDeviceDependentResourcesInternal();
}

RenderableObject::~RenderableObject() = default;

void RenderableObject::UpdateModelConstantBuffer(const winrt::Windows::Foundation::Numerics::float4x4& modelTransform)
{
    if (m_loadingComplete)
    {
        m_modelConstantBufferData.model = reinterpret_cast<DirectX::XMFLOAT4X4&>(transpose(modelTransform));

        // Update the model transform buffer for the hologram.
        m_deviceResources->UseD3DDeviceContext(
            [&](auto context) { context->UpdateSubresource(m_modelConstantBuffer.get(), 0, nullptr, &m_modelConstantBufferData, 0, 0); });
    }
}

void RenderableObject::Render(bool isStereo, winrt::Windows::Foundation::IReference<SpatialBoundingFrustum> cullingFrustum)
{
    if (!m_loadingComplete)
    {
        return;
    }

    // Use the D3D device context to update Direct3D device-based resources.
    m_deviceResources->UseD3DDeviceContext([&](auto context) {
        context->IASetInputLayout(m_inputLayout.get());

        // Attach the vertex shader.
        context->VSSetShader(m_vertexShader.get(), nullptr, 0);

        // Apply the model constant buffer to the vertex shader.
        ID3D11Buffer* pBuffer = m_modelConstantBuffer.get();
        context->VSSetConstantBuffers(0, 1, &pBuffer);

        if (!m_usingVprtShaders)
        {
            // On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
            // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
            // a pass-through geometry shader is used to set the render target
            // array index.
            context->GSSetShader(m_geometryShader.get(), nullptr, 0);
        }

        context->UpdateSubresource(m_filterColorBuffer.get(), 0, nullptr, &m_filterColorData, 0, 0);
        ID3D11Buffer* pBufferToSet = m_filterColorBuffer.get();
        context->PSSetConstantBuffers(0, 1, &pBufferToSet);

        context->PSSetShader(m_pixelShader.get(), nullptr, 0);
        context->RSSetState(m_rasterizerState.get());

        Draw(isStereo ? 2 : 1, cullingFrustum);
    });
}

std::future<void> RenderableObject::CreateDeviceDependentResources()
{
    return CreateDeviceDependentResourcesInternal();
}

std::future<void> RenderableObject::CreateDeviceDependentResourcesInternal()
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    std::wstring fileNamePrefix = L"";
#else
    std::wstring fileNamePrefix = L"ms-appx:///";
#endif

    m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

    // On devices that do support the D3D11_FEATURE_D3D11_OPTIONS3::
    // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature
    // we can avoid using a pass-through geometry shader to set the render
    // target array index, thus avoiding any overhead that would be
    // incurred by setting the geometry shader stage.
    std::wstring vertexShaderFileName = m_usingVprtShaders ? L"SimpleColor_VertexShaderVprt.cso" : L"SimpleColor_VertexShader.cso";

    // Load shaders asynchronously.
    std::vector<byte> vertexShaderFileData = co_await DXHelper::ReadDataAsync(fileNamePrefix + vertexShaderFileName);
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateVertexShader(
        vertexShaderFileData.data(), vertexShaderFileData.size(), nullptr, m_vertexShader.put()));

    constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 3> vertexDesc = {{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
    }};

    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateInputLayout(
        vertexDesc.data(),
        static_cast<UINT>(vertexDesc.size()),
        vertexShaderFileData.data(),
        static_cast<UINT>(vertexShaderFileData.size()),
        m_inputLayout.put()));

    std::vector<byte> pixelShaderFileData = co_await DXHelper::ReadDataAsync(fileNamePrefix + L"SimpleColor_PixelShader.cso");
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreatePixelShader(
        pixelShaderFileData.data(), pixelShaderFileData.size(), nullptr, m_pixelShader.put()));

    const ModelConstantBuffer constantBuffer{
        reinterpret_cast<DirectX::XMFLOAT4X4&>(winrt::Windows::Foundation::Numerics::float4x4::identity()),
    };

    const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, m_modelConstantBuffer.put()));

    const CD3D11_BUFFER_DESC filterColorBufferDesc(sizeof(DirectX::XMFLOAT4), D3D11_BIND_CONSTANT_BUFFER);
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&filterColorBufferDesc, nullptr, m_filterColorBuffer.put()));

    if (!m_usingVprtShaders)
    {
        // Load the pass-through geometry shader.
        std::vector<byte> geometryShaderFileData = co_await DXHelper::ReadDataAsync(fileNamePrefix + L"SimpleColor_GeometryShader.cso");

        // After the pass-through geometry shader file is loaded, create the shader.
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateGeometryShader(
            geometryShaderFileData.data(), geometryShaderFileData.size(), nullptr, m_geometryShader.put()));
    }

    {
        D3D11_RASTERIZER_DESC rasterizerDesc = {D3D11_FILL_SOLID, D3D11_CULL_NONE};
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateRasterizerState(&rasterizerDesc, m_rasterizerState.put()));
    }

    m_loadingComplete = true;
}

void RenderableObject::ReleaseDeviceDependentResources()
{
    m_loadingComplete = false;
    m_usingVprtShaders = false;
    m_vertexShader = nullptr;
    m_inputLayout = nullptr;
    m_pixelShader = nullptr;
    m_geometryShader = nullptr;
    m_modelConstantBuffer = nullptr;
    m_rasterizerState = nullptr;
}

void RenderableObject::AppendColoredTriangle(
    DirectX::XMFLOAT3 p0,
    DirectX::XMFLOAT3 p1,
    DirectX::XMFLOAT3 p2,
    DirectX::XMFLOAT3 color,
    std::vector<VertexPositionNormalColor>& vertices)
{
    VertexPositionNormalColor vertex;
    vertex.color = color;
    vertex.normal = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

    vertex.pos = p0;
    vertices.push_back(vertex);
    vertex.pos = p1;
    vertices.push_back(vertex);
    vertex.pos = p2;
    vertices.push_back(vertex);
}

void RenderableObject::AppendColoredTriangle(
    winrt::Windows::Foundation::Numerics::float3 p0,
    winrt::Windows::Foundation::Numerics::float3 p1,
    winrt::Windows::Foundation::Numerics::float3 p2,
    winrt::Windows::Foundation::Numerics::float3 color,
    std::vector<VertexPositionNormalColor>& vertices)
{

    AppendColoredTriangle(
        DXHelper::Float3ToXMFloat3(p0),
        DXHelper::Float3ToXMFloat3(p1),
        DXHelper::Float3ToXMFloat3(p2),
        DXHelper::Float3ToXMFloat3(color),
        vertices);
}
