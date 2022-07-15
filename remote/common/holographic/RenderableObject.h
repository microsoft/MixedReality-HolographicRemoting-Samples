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

#include <DeviceResourcesD3D11.h>
#include <SimpleColor_ShaderStructures.h>

#include <future>

#include <winrt/Windows.Perception.Spatial.h>

class RenderableObject
{
public:
    RenderableObject(const std::shared_ptr<DXHelper::DeviceResourcesD3D11>& deviceResources);
    virtual ~RenderableObject();

    virtual void CreateDeviceDependentResources();
    virtual void ReleaseDeviceDependentResources();

    void Render(
        bool isStereo, winrt::Windows::Foundation::IReference<winrt::Windows::Perception::Spatial::SpatialBoundingFrustum> cullingFrustum);

protected:
    void UpdateModelConstantBuffer(const winrt::Windows::Foundation::Numerics::float4x4& modelTransform);

    virtual void Draw(
        unsigned int numInstances,
        winrt::Windows::Foundation::IReference<winrt::Windows::Perception::Spatial::SpatialBoundingFrustum> cullingFrustum) = 0;

    static void AppendColoredTriangle(
        DirectX::XMFLOAT3 p0,
        DirectX::XMFLOAT3 p1,
        DirectX::XMFLOAT3 p2,
        DirectX::XMFLOAT3 color,
        std::vector<VertexPositionNormalColor>& vertices);

    static void AppendColoredTriangle(
        winrt::Windows::Foundation::Numerics::float3 p0,
        winrt::Windows::Foundation::Numerics::float3 p1,
        winrt::Windows::Foundation::Numerics::float3 p2,
        winrt::Windows::Foundation::Numerics::float3 color,
        std::vector<VertexPositionNormalColor>& vertices);

    // Cached pointer to device resources.
    std::shared_ptr<DXHelper::DeviceResourcesD3D11> m_deviceResources;

private:
    // Direct3D resources for geometry.
    winrt::com_ptr<ID3D11InputLayout> m_inputLayout;
    winrt::com_ptr<ID3D11VertexShader> m_vertexShader;
    winrt::com_ptr<ID3D11GeometryShader> m_geometryShader;
    winrt::com_ptr<ID3D11PixelShader> m_pixelShader;
    winrt::com_ptr<ID3D11Buffer> m_modelConstantBuffer;
    winrt::com_ptr<ID3D11Buffer> m_filterColorBuffer;
    winrt::com_ptr<ID3D11RasterizerState> m_rasterizerState;
    DirectX::XMFLOAT4 m_filterColorData = {1, 1, 1, 1};

    // System resources for geometry.
    ModelConstantBuffer m_modelConstantBufferData;
    uint32_t m_indexCount = 0;

    // Variables used with the rendering loop.
    std::atomic<bool> m_loadingComplete = false;

    // If the current D3D Device supports VPRT, we can avoid using a geometry
    // shader just to set the render target array index.
    bool m_usingVprtShaders = false;
};
