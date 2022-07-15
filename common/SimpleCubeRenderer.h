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

#include <DirectXMath.h>
#include <SimpleColor_ShaderStructures.h>
#include <future>

#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.Perception.Spatial.h>

class SimpleCubeRenderer
{
public:
    SimpleCubeRenderer(
        const std::shared_ptr<DXHelper::DeviceResourcesD3D11>& deviceResources,
        winrt::Windows::Foundation::Numerics::float3 position,
        winrt::Windows::Foundation::Numerics::float3 color);

    void CreateDeviceDependentResources();

    void ReleaseDeviceDependentResources();

    void Update(
        winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem,
        winrt::Windows::Perception::Spatial::SpatialCoordinateSystem objectCoordianteSystem);

    void Render(bool isStereo);

private:
    // Cached pointer to device resources.
    std::shared_ptr<DXHelper::DeviceResourcesD3D11> m_deviceResources;

    // Direct3D resources for cube geometry.
    winrt::com_ptr<ID3D11InputLayout> m_inputLayout;
    winrt::com_ptr<ID3D11Buffer> m_vertexBuffer;
    winrt::com_ptr<ID3D11Buffer> m_indexBuffer;
    winrt::com_ptr<ID3D11VertexShader> m_vertexShader;
    winrt::com_ptr<ID3D11GeometryShader> m_geometryShader;
    winrt::com_ptr<ID3D11PixelShader> m_pixelShader;
    winrt::com_ptr<ID3D11Buffer> m_modelConstantBuffer;
    winrt::com_ptr<ID3D11Buffer> m_filterColorBuffer;

    winrt::Windows::Foundation::Numerics::float3 m_position;
    winrt::Windows::Foundation::Numerics::float3 m_color;

    // System resources for cube geometry.
    ModelConstantBuffer m_modelConstantBufferData;
    uint32_t m_indexCount = 0;
    DirectX::XMFLOAT4 m_filterColorData = {1, 1, 1, 1};

    // If the current D3D Device supports VPRT, we can avoid using a geometry
    // shader just to set the render target array index.
    bool m_usingVprtShaders = false;

    const float m_cubeExtent = 0.1f;

    std::atomic<bool> m_loadingComplete = false;
    bool m_isVisible = false;
};
