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

#include <d3d11/SimpleColor_ShaderStructures.h>
#include <holographic/DeviceResources.h>

#include <winrt/Windows.UI.Input.Spatial.h>

#include <future>

// This sample renderer instantiates a basic rendering pipeline.
class SpinningCubeRenderer
{
public:
    SpinningCubeRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources);

    void CreateWindowSizeDependentResources();
    std::future<void> CreateDeviceDependentResources();
    void ReleaseDeviceDependentResources();
    void Update(
        float totalSeconds,
        winrt::Windows::Perception::PerceptionTimestamp timestamp,
        winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem);
    void SetColorFilter(DirectX::XMFLOAT4 color);
    void Render(bool isStereo, winrt::Windows::Foundation::IReference<SpatialBoundingFrustum> cullingFrustum);

    // Repositions the sample hologram.
    void PositionHologram(const winrt::Windows::UI::Input::Spatial::SpatialPointerPose& pointerPose);

    // Repositions the sample hologram, using direct measures.
    void PositionHologram(winrt::Windows::Foundation::Numerics::float3 pos, winrt::Windows::Foundation::Numerics::float3 dir);

    // Property accessors.
    void SetPosition(winrt::Windows::Foundation::Numerics::float3 pos)
    {
        m_position = pos;
    }
    const winrt::Windows::Foundation::Numerics::float3& GetPosition()
    {
        return m_position;
    }

    void Pause()
    {
        m_pauseState = PauseState::Pausing;
    }
    void Unpause()
    {
        m_pauseState = PauseState::Unpausing;
    }
    void TogglePauseState();

private:
    enum class PauseState
    {
        Unpaused = 0,
        Pausing,
        Paused,
        Unpausing,
    };

    // Cached pointer to device resources.
    std::shared_ptr<DXHelper::DeviceResources> m_deviceResources;

    // Direct3D resources for cube geometry.
    winrt::com_ptr<ID3D11InputLayout> m_inputLayout;
    winrt::com_ptr<ID3D11Buffer> m_vertexBuffer;
    winrt::com_ptr<ID3D11Buffer> m_indexBuffer;
    winrt::com_ptr<ID3D11VertexShader> m_vertexShader;
    winrt::com_ptr<ID3D11GeometryShader> m_geometryShader;
    winrt::com_ptr<ID3D11PixelShader> m_pixelShader;
    winrt::com_ptr<ID3D11Buffer> m_modelConstantBuffer;
    winrt::com_ptr<ID3D11Buffer> m_filterColorBuffer;

    // System resources for cube geometry.
    ModelConstantBuffer m_modelConstantBufferData;
    uint32_t m_indexCount = 0;
    DirectX::XMFLOAT4 m_filterColorData = {1, 1, 1, 1};

    // Variables used with the rendering loop.
    std::atomic<bool> m_loadingComplete = false;
    float m_degreesPerSecond = 180.0f;
    winrt::Windows::Foundation::Numerics::float3 m_position = {0.0f, 0.0f, -2.0f};
    PauseState m_pauseState = PauseState::Unpaused;
    double m_rotationOffset = 0;

    // If the current D3D Device supports VPRT, we can avoid using a geometry
    // shader just to set the render target array index.
    bool m_usingVprtShaders = false;

    const float m_cubeExtent = 0.1f;
    const float m_boundingSphereRadius = sqrtf(3 * m_cubeExtent * m_cubeExtent);
};
