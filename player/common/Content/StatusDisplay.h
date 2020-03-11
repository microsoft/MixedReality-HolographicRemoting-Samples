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

#include "..\Common\DeviceResourcesCommon.h"
#include "ShaderStructures.h"

#include <string>

#include <winrt\Windows.Networking.Connectivity.h>

class StatusDisplay
{
public:
    // Available text formats
    enum TextFormat : uint32_t
    {
        Small = 0,
        Large,
        LargeBold,
        Medium,

        TextFormatCount
    };

    // Available text colors
    enum TextColor : uint32_t
    {
        White = 0,
        Yellow,
        Red,

        TextColorCount
    };

    // A single line in the status display with all its properties
    struct Line
    {
        std::wstring text;
        TextFormat format = Large;
        TextColor color = White;
        float lineHeightMultiplier = 1.0f;
        bool alignBottom = false;
    };

public:
    StatusDisplay(const std::shared_ptr<DXHelper::DeviceResourcesCommon>& deviceResources);

    void Update(float deltaTimeInSeconds);

    void Render();

    void CreateDeviceDependentResources();
    void ReleaseDeviceDependentResources();

    /*
        Methods to change the contents of the text display
    */

    // Clear all lines
    void ClearLines();

    // Set a new set of lines replacing the existing ones
    void SetLines(winrt::array_view<Line> lines);

    // Update the text of a single line
    void UpdateLineText(size_t index, std::wstring text);

    // Add a new line returning the index of the new line
    size_t AddLine(const Line& line);

    // Check if a line with the given index exists
    bool HasLine(size_t index);

    /*
        Methods to change the displayed image
    */

    // Set the image displayed
    void SetImage(const winrt::com_ptr<ID3D11ShaderResourceView>& imageView);

    // Enable or disable the rendering of the image
    void SetImageEnabled(bool enabled)
    {
        m_imageEnabled = enabled;
    }

    // Repositions the status display
    void PositionDisplay(float deltaTimeInSeconds, const winrt::Windows::UI::Input::Spatial::SpatialPointerPose& pointerPose);

    // Get the center position of the status display
    winrt::Windows::Foundation::Numerics::float3 GetPosition()
    {
        return m_positionImage;
    }

    void UpdateTextScale(
        winrt::Windows::Graphics::Holographic::HolographicStereoTransform holoTransform,
        float screenWidth,
        float screenHeight,
        float quadFov,
        float heightRatio = 1.0f);

    // Get default FOV for the text quad in degree.
    float GetDefaultQuadFov()
    {
        return m_defaultQuadFov;
    }
    // Get statistics FOV for the text quad in degree.
    float GetStatisticsFov()
    {
        return m_statisticsFov;
    }
    // Get the statistics height ratio.
    float GetStatisticsHeightRatio()
    {
        return m_statisticsHeightRatio;
    }

private:
    // Runtime representation of a text line.
    struct RuntimeLine
    {
        winrt::com_ptr<IDWriteTextLayout> layout = nullptr;
        DWRITE_TEXT_METRICS metrics = {};
        std::wstring text = {};
        TextFormat format = Large;
        TextColor color = White;
        float lineHeightMultiplier = 1.0f;
        bool alignBottom = false;
    };

    void CreateFonts();
    void CreateBrushes();
    void UpdateLineInternal(RuntimeLine& runtimLine, const Line& line);
    void UpdateConstantBuffer(
        float deltaTimeInSeconds,
        ModelConstantBuffer& buffer,
        winrt::Windows::Foundation::Numerics::float3 position,
        winrt::Windows::Foundation::Numerics::float3 lastPosition);

    winrt::com_ptr<ID2D1SolidColorBrush> m_brushes[TextColorCount] = {};
    winrt::com_ptr<IDWriteTextFormat> m_textFormats[TextFormatCount] = {};
    std::vector<RuntimeLine> m_lines;
    std::mutex m_lineMutex;

    // Cached pointer to device resources.
    std::shared_ptr<DXHelper::DeviceResourcesCommon> m_deviceResources;

    // Resources related to text rendering.
    winrt::com_ptr<ID3D11Texture2D> m_textTexture;
    winrt::com_ptr<ID3D11ShaderResourceView> m_textShaderResourceView;
    winrt::com_ptr<ID3D11RenderTargetView> m_textRenderTarget;
    winrt::com_ptr<ID2D1RenderTarget> m_d2dTextRenderTarget;

    // Direct3D resources for quad geometry.
    winrt::com_ptr<ID3D11InputLayout> m_inputLayout;
    winrt::com_ptr<ID3D11Buffer> m_vertexBufferImage;
    winrt::com_ptr<ID3D11Buffer> m_vertexBufferText;
    winrt::com_ptr<ID3D11Buffer> m_indexBuffer;
    winrt::com_ptr<ID3D11VertexShader> m_vertexShader;
    winrt::com_ptr<ID3D11GeometryShader> m_geometryShader;
    winrt::com_ptr<ID3D11PixelShader> m_pixelShader;
    winrt::com_ptr<ID3D11Buffer> m_modelConstantBuffer;

    // Direct3D resources for a texture.
    winrt::com_ptr<ID3D11ShaderResourceView> m_imageView;
    winrt::com_ptr<ID3D11SamplerState> m_imageSamplerState;

    winrt::com_ptr<ID3D11SamplerState> m_textSamplerState;
    winrt::com_ptr<ID3D11BlendState> m_textAlphaBlendState;
    winrt::com_ptr<ID3D11DepthStencilState> m_depthStencilState;

    // System resources for quad geometry.
    ModelConstantBuffer m_modelConstantBufferDataImage = {};
    ModelConstantBuffer m_modelConstantBufferDataText = {};
    uint32_t m_indexCount = 0;

    // Variables used with the rendering loop.
    bool m_loadingComplete = false;
    float m_degreesPerSecond = 45.f;
    winrt::Windows::Foundation::Numerics::float3 m_positionImage = {0.f, 0.f, -2.f};
    winrt::Windows::Foundation::Numerics::float3 m_lastPositionImage = {0.f, 0.f, -2.f};
    winrt::Windows::Foundation::Numerics::float3 m_velocity = {0.f, 0.f, 0.f};
    winrt::Windows::Foundation::Numerics::float3 m_positionText = {0.f, -0.1f, -0.1f};
    winrt::Windows::Foundation::Numerics::float3 m_lastPositionText = {0.f, -0.1f, -0.1f};

    // If the current D3D Device supports VPRT, we can avoid using a geometry
    // shader just to set the render target array index.
    bool m_usingVprtShaders = false;

    // This is the rate at which the hologram position is interpolated ("lerped") to the current location.
    const float c_lerpRate = 4.0f;

    bool m_imageEnabled = true;

    // The distance to the camera in fwd-direction.
    float m_statusDisplayDistance = 1.0f;
    // The view Projection matrix.
    DirectX::XMFLOAT4X4 m_projection;

    // Default size, gets adjusted based on HMD.
    int m_textTextureWidth = 128;
    int m_textTextureHeight = 128;

    // Default size, gets adjusted based on the HMD and FOV.
    float m_virtualDisplaySizeInchX = 10.0f;
    float m_virtualDisplaySizeInchY = 10.0f;

    // The current FOV for the text quad in degree.
    float m_currentQuadFov{};
    // The current height ratio of the quad.
    float m_currentHeightRatio{};
    // The default FOV for the text quad in degree.
    float m_defaultQuadFov = 25.0f;
    // The statistics FOV for the text quad in degree.
    float m_statisticsFov = 23.0f;
    // The height ratio for the statistics quad in percent.
    float m_statisticsHeightRatio = 0.4f;
};
