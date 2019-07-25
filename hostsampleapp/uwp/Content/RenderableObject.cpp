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

#include "RenderableObject.h"

#include "../Common/DirectXHelper.h"


RenderableObject::RenderableObject(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources)
    : m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();
}

void RenderableObject::UpdateModelConstantBuffer(const winrt::Windows::Foundation::Numerics::float4x4& modelTransform)
{
    winrt::Windows::Foundation::Numerics::float4x4 normalTransform = modelTransform;
    normalTransform.m41 = normalTransform.m42 = normalTransform.m43 = 0;
    UpdateModelConstantBuffer(modelTransform, normalTransform);
}

void RenderableObject::UpdateModelConstantBuffer(
    const winrt::Windows::Foundation::Numerics::float4x4& modelTransform,
    const winrt::Windows::Foundation::Numerics::float4x4& normalTransform)
{
    if (m_loadingComplete)
    {
        m_modelConstantBufferData.model = reinterpret_cast<DirectX::XMFLOAT4X4&>(transpose(modelTransform));
        m_modelConstantBufferData.normal = reinterpret_cast<DirectX::XMFLOAT4X4&>(transpose(normalTransform));

        // Update the model transform buffer for the hologram.
        m_deviceResources->UseD3DDeviceContext(
            [&](auto context) { context->UpdateSubresource(m_modelConstantBuffer.get(), 0, nullptr, &m_modelConstantBufferData, 0, 0); });
    }
}

void RenderableObject::Render(bool isStereo)
{
    if (!m_loadingComplete)
    {
        return;
    }

    // Use the D3D device context to update Direct3D device-based resources.
    m_deviceResources->UseD3DDeviceContext([&](auto context) {
        context->IASetInputLayout(m_inputLayout.get());
        context->PSSetShader(m_pixelShader.get(), nullptr, 0);

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

        Draw(isStereo ? 2 : 1);
    });
}

std::future<void> RenderableObject::CreateDeviceDependentResources()
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
    std::wstring vertexShaderFileName = m_usingVprtShaders ? L"hsa_VprtVertexShader.cso" : L"hsa_VertexShader.cso";

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

    std::vector<byte> pixelShaderFileData = co_await DXHelper::ReadDataAsync(fileNamePrefix + L"hsa_PixelShader.cso");
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreatePixelShader(
        pixelShaderFileData.data(), pixelShaderFileData.size(), nullptr, m_pixelShader.put()));

    const ModelConstantBuffer constantBuffer{
        reinterpret_cast<DirectX::XMFLOAT4X4&>(winrt::Windows::Foundation::Numerics::float4x4::identity()),
        reinterpret_cast<DirectX::XMFLOAT4X4&>(winrt::Windows::Foundation::Numerics::float4x4::identity()),
    };

    const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, m_modelConstantBuffer.put()));

    if (!m_usingVprtShaders)
    {
        // Load the pass-through geometry shader.
        std::vector<byte> geometryShaderFileData = co_await DXHelper::ReadDataAsync(fileNamePrefix + L"hsa_GeometryShader.cso");

        // After the pass-through geometry shader file is loaded, create the shader.
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateGeometryShader(
            geometryShaderFileData.data(), geometryShaderFileData.size(), nullptr, m_geometryShader.put()));
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
}
