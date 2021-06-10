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

#include <future>
#include <string>

#include <holographic/DeviceResources.h>

#include <winrt/Microsoft.MixedReality.SceneUnderstanding.h>

class SceneUnderstandingRenderer : public std::enable_shared_from_this<SceneUnderstandingRenderer>
{
public:
    SceneUnderstandingRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources);
    ~SceneUnderstandingRenderer();

    void SetScene(
        winrt::Microsoft::MixedReality::SceneUnderstanding::Scene scene,
        winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference lastUpdateLocation);

    std::future<void> CreateDeviceDependentResources();

    void ReleaseDeviceDependentResources();

    void Update(winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem);

    void Render(bool isStereo);

    void ToggleRenderingType();

    void Reset();

private:
    struct VertexPositionUVColor
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT2 uv;
        DirectX::XMFLOAT3 color;
    };

    enum RenderingType
    {
        None = 0,
        Quads = 1,
        Mesh = 2,
        All = 3,
        Max
    };

    winrt::fire_and_forget CreateVerticesAsync(
        winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem,
        winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference lastUpdateLocation);

    void AddSceneQuadsVertices(
        const winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObject& object,
        const winrt::Windows::Foundation::Numerics::float3& color);

    void AddSceneQuadLabelVertices(
        const winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObject& object,
        const winrt::Windows::Foundation::Numerics::float3& color);

    void AddSceneMeshVertices(
        const winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObject& object,
        const winrt::Windows::Foundation::Numerics::float3& color);

    void RenderSceneMesh(bool isStereo);
    void RenderSceneQuads(bool isStereo);
    void RenderSceneQuadsLabel(bool isStereo);

    static void AppendQuad(
        const winrt::Windows::Foundation::Numerics::float3 positions[4],
        const winrt::Windows::Foundation::Numerics::float2 uvs[4],
        const float height,
        const float width,
        const winrt::Windows::Foundation::Numerics::float3& color,
        std::vector<VertexPositionUVColor>& vertices);

    // The current renderingType.
    RenderingType m_renderingType = RenderingType::None;

    // The vertices for all scene quads.
    std::vector<VertexPositionUVColor> m_quadVertices;

    // The vertices for the same SceneObjectKind are stored in the same collection.
    std::map<winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObjectKind, std::vector<VertexPositionUVColor>> m_quadLabelsVertices;

    // The vertices for the scene mesh.
    std::vector<VertexPositionUVColor> m_meshVertices;

    // Cached pointer to device resources.
    std::shared_ptr<DXHelper::DeviceResources> m_deviceResources;

    // Direct3D resources.
    winrt::com_ptr<ID3D11Buffer> m_quadVerticesBuffer;
    std::map<winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObjectKind, winrt::com_ptr<ID3D11Buffer>> m_quadLabelsVerticesBuffer;
    winrt::com_ptr<ID3D11Buffer> m_meshVerticesBuffer;
    winrt::com_ptr<ID3D11InputLayout> m_inputLayout = nullptr;
    winrt::com_ptr<ID3D11VertexShader> m_vertexShader = nullptr;
    winrt::com_ptr<ID3D11GeometryShader> m_geometryShader = nullptr;
    winrt::com_ptr<ID3D11PixelShader> m_quadsPixelShader = nullptr;
    winrt::com_ptr<ID3D11PixelShader> m_meshPixelShader = nullptr;
    winrt::com_ptr<ID3D11RasterizerState> m_rasterizerState = nullptr;
    winrt::com_ptr<ID3D11Buffer> m_modelConstantBuffer = nullptr;

    // True if the model constant buffer up to date.
    bool m_validSceneToRenderingTransform = false;

    // Variables used with the rendering loop.
    std::atomic<bool> m_loadingComplete = false;

    // The scene understanding scene.
    winrt::Microsoft::MixedReality::SceneUnderstanding::Scene m_scene = nullptr;
    winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference m_sceneLastUpdateLocation = nullptr;
    // True if the scene was updated but the vertices for the rendering are not created yet.
    bool m_verticesOutdated = false;
    // True if the scene was updated and the vertices are currently asynchronously updated.
    bool m_verticesUpdating = false;
    // Mutex to prevent the scene from updating while the vertices are still updating.
    std::mutex m_mutex;

    // DirectX resources for text rendering.
    std::map<winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObjectKind, winrt::com_ptr<ID3D11Texture2D>> m_textTextures;
    std::map<winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObjectKind, winrt::com_ptr<ID3D11ShaderResourceView>>
        m_textShaderResourceViews;
    std::map<winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObjectKind, winrt::com_ptr<ID3D11RenderTargetView>>
        m_textRenderTargets;
    std::map<winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObjectKind, winrt::com_ptr<ID2D1RenderTarget>> m_d2dTextRenderTargets;
    std::map<winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObjectKind, winrt::com_ptr<ID2D1SolidColorBrush>> m_brushes;
    std::map<winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObjectKind, winrt::com_ptr<IDWriteTextLayout>> m_layouts;

    winrt::com_ptr<IDWriteTextFormat> m_textFormat = nullptr;
    winrt::com_ptr<ID3D11SamplerState> m_textSamplerState = nullptr;
    winrt::com_ptr<ID3D11PixelShader> m_labelPixelShader = nullptr;
    winrt::com_ptr<ID3D11BlendState> m_blendState = nullptr;

    // The spatial coordinate system.
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_coordinateSystem = nullptr;
};
