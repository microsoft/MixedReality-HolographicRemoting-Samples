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

#include <content/SceneUnderstandingRenderer.h>

#include <DbgLog.h>
#include <DirectXColors.h>
#include <d3d11/DirectXHelper.h>

#include <winrt/Windows.Perception.Spatial.Preview.h>

using namespace DirectX;

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::Perception::Spatial;

using namespace winrt::Microsoft::MixedReality::SceneUnderstanding;

namespace
{
    // The size of the label quads in rendering space.
    constexpr float LabelQuadWidth = 0.6f;
    constexpr float LabelQuadHeight = 0.3f;

    // The texture size in pixels.
    constexpr int TextTextureWidth = 256;
    constexpr int TextTextureHeight = 128;

    // Logical size of the font in DIP.
    constexpr float LabelFontSize = 40.0f;

    // Struct to hold one entity label type entry
    struct SceneObjectLabel
    {
        std::wstring name;
        std::array<uint8_t, 3> color;
    };

    // Dictionaries to quickly access labels by the SceneObjectKind.
    const std::map<SceneObjectKind, SceneObjectLabel> m_sceneQuadsLabels{
        {SceneObjectKind::Background, SceneObjectLabel{L"Background", {255, 32, 48}}}, // Red'ish
        {SceneObjectKind::Wall, SceneObjectLabel{L"Wall", {250, 151, 133}}},           // Orange'ish
        {SceneObjectKind::Floor, SceneObjectLabel{L"Floor", {184, 237, 110}}},         // Green'ish
        {SceneObjectKind::Ceiling, SceneObjectLabel{L"Ceiling", {138, 43, 211}}},      // Purple'ish
        {SceneObjectKind::Platform, SceneObjectLabel{L"Platform", {37, 188, 183}}}     // Blue'ish
    };

    const std::map<SceneObjectKind, SceneObjectLabel> m_sceneMeshLabels{
        {SceneObjectKind::World, SceneObjectLabel{L"World", {100, 255, 255}}} // Cyan'ish
    };

} // namespace

SceneUnderstandingRenderer::SceneUnderstandingRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources)
    : m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();
}

SceneUnderstandingRenderer::~SceneUnderstandingRenderer()
{
    ReleaseDeviceDependentResources();
}

std::future<void> SceneUnderstandingRenderer::CreateDeviceDependentResources()
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    std::wstring fileNamePrefix = L"";
#else
    std::wstring fileNamePrefix = L"ms-appx:///";
#endif

    // Create the resources for label texture rendering before any thread switch occurs.
    {
        // Create texture description.
        CD3D11_TEXTURE2D_DESC textureDesc(
            DXGI_FORMAT_B8G8R8A8_UNORM, TextTextureWidth, TextTextureHeight, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

        // Create font.
        winrt::check_hresult(m_deviceResources->GetDWriteFactory()->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_MEDIUM,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            LabelFontSize,
            L"en-US",
            m_textFormat.put()));
        winrt::check_hresult(m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
        winrt::check_hresult(m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));

        // Create text sampler state.
        CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateSamplerState(&samplerDesc, m_textSamplerState.put()));

        // Create a single texture for every label.
        for (auto const& [kind, label] : m_sceneQuadsLabels)
        {
            // Create the texture.
            winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateTexture2D(&textureDesc, nullptr, m_textTextures[kind].put()));

            // Create the shader resource view.
            winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateShaderResourceView(
                m_textTextures[kind].get(), nullptr, m_textShaderResourceViews[kind].put()));

            // Create the render target view.
            winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateRenderTargetView(
                m_textTextures[kind].get(), nullptr, m_textRenderTargets[kind].put()));

            // Create the DXGI render targets.
            D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
            winrt::com_ptr<IDXGISurface> dxgiSurface;
            m_textTextures[kind].as(dxgiSurface);
            winrt::check_hresult(m_deviceResources->GetD2DFactory()->CreateDxgiSurfaceRenderTarget(
                dxgiSurface.get(), &props, m_d2dTextRenderTargets[kind].put()));

            // Create the brush.
            winrt::check_hresult(
                m_d2dTextRenderTargets[kind]->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), m_brushes[kind].put()));

            // Create the layout.
            winrt::check_hresult(m_deviceResources->GetDWriteFactory()->CreateTextLayout(
                label.name.c_str(),
                static_cast<UINT32>(label.name.size()),
                m_textFormat.get(),
                static_cast<float>(TextTextureWidth),  // Max width of the input text.
                static_cast<float>(TextTextureHeight), // Max height of the input text.
                m_layouts[kind].put()));

            m_deviceResources->UseD3DDeviceContext([&](auto context) {
                context->ClearRenderTargetView(m_textRenderTargets[kind].get(), DirectX::Colors::Transparent);

                // Draw the label name to the texture.
                m_d2dTextRenderTargets[kind]->BeginDraw();
                m_d2dTextRenderTargets[kind]->DrawTextLayout(
                    D2D1::Point2F(0, (m_layouts[kind]->GetMaxHeight() / 2.0f) - LabelFontSize),
                    m_layouts[kind].get(),
                    m_brushes[kind].get());
                m_d2dTextRenderTargets[kind]->EndDraw();
            });
        }
    }

    // Vertex shader.
    {
        std::vector<byte> vertexShaderFileData = co_await DXHelper::ReadDataAsync(fileNamePrefix + L"SU_VertexShader.cso");
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateVertexShader(
            vertexShaderFileData.data(), vertexShaderFileData.size(), nullptr, m_vertexShader.put()));

        constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 3> vertexDesc = {{
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
        }};

        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateInputLayout(
            vertexDesc.data(),
            static_cast<UINT>(vertexDesc.size()),
            vertexShaderFileData.data(),
            static_cast<UINT>(vertexShaderFileData.size()),
            m_inputLayout.put()));
    }

    // Pixel shader for scene quads.
    {
        std::vector<byte> pixelShaderFileData = co_await DXHelper::ReadDataAsync(fileNamePrefix + L"SUQuads_PixelShader.cso");
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreatePixelShader(
            pixelShaderFileData.data(), pixelShaderFileData.size(), nullptr, m_quadsPixelShader.put()));
    }

    // Pixel shader for scene quad labels.
    {
        std::vector<byte> pixelShaderFileData = co_await DXHelper::ReadDataAsync(fileNamePrefix + L"SULabel_PixelShader.cso");
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreatePixelShader(
            pixelShaderFileData.data(), pixelShaderFileData.size(), nullptr, m_labelPixelShader.put()));
    }

    // Pixel shader for scene mesh.
    {
        std::vector<byte> pixelShaderFileData = co_await DXHelper::ReadDataAsync(fileNamePrefix + L"SUMesh_PixelShader.cso");
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreatePixelShader(
            pixelShaderFileData.data(), pixelShaderFileData.size(), nullptr, m_meshPixelShader.put()));
    }

    // Geometry shader.
    {
        std::vector<byte> geometryShaderFileData = co_await DXHelper::ReadDataAsync(fileNamePrefix + L"SU_GeometryShader.cso");

        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateGeometryShader(
            geometryShaderFileData.data(), geometryShaderFileData.size(), nullptr, m_geometryShader.put()));
    }

    // Rasterizer description.
    D3D11_RASTERIZER_DESC rasterizerDesc = {D3D11_FILL_SOLID, D3D11_CULL_BACK};
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateRasterizerState(&rasterizerDesc, m_rasterizerState.put()));

    const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(DirectX::XMFLOAT4X4), D3D11_BIND_CONSTANT_BUFFER);
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, m_modelConstantBuffer.put()));

    // Create the blend state.
    {
        CD3D11_BLEND_DESC blendStateDesc(D3D11_DEFAULT);
        blendStateDesc.AlphaToCoverageEnable = FALSE;
        blendStateDesc.IndependentBlendEnable = FALSE;

        const D3D11_RENDER_TARGET_BLEND_DESC rtBlendDesc = {
            TRUE,
            D3D11_BLEND_SRC_ALPHA,
            D3D11_BLEND_INV_SRC_ALPHA,
            D3D11_BLEND_OP_ADD,
            D3D11_BLEND_INV_DEST_ALPHA,
            D3D11_BLEND_ONE,
            D3D11_BLEND_OP_ADD,
            D3D11_COLOR_WRITE_ENABLE_ALL,
        };

        for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            blendStateDesc.RenderTarget[i] = rtBlendDesc;
        }

        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBlendState(&blendStateDesc, m_blendState.put()));
    }

    m_loadingComplete = true;
}

void SceneUnderstandingRenderer::ReleaseDeviceDependentResources()
{
    m_loadingComplete = false;

    m_inputLayout = nullptr;
    m_vertexShader = nullptr;
    m_geometryShader = nullptr;
    m_quadsPixelShader = nullptr;
    m_meshPixelShader = nullptr;
    m_rasterizerState = nullptr;
    m_modelConstantBuffer = nullptr;

    for (auto const& [kind, label] : m_sceneQuadsLabels)
    {
        m_textTextures[kind] = nullptr;
        m_textShaderResourceViews[kind] = nullptr;
        m_textRenderTargets[kind] = nullptr;
        m_d2dTextRenderTargets[kind] = nullptr;
        m_brushes[kind] = nullptr;
        m_layouts[kind] = nullptr;
    }

    m_textFormat = nullptr;
    m_textSamplerState = nullptr;
    m_labelPixelShader = nullptr;
    m_blendState = nullptr;
}

void SceneUnderstandingRenderer::SetScene(Scene scene, SpatialStationaryFrameOfReference lastUpdateLocation)
{
    // While the vertices are updated asynchronously the scene cannot be set.
    std::lock_guard lock(m_mutex);

    m_scene = scene;
    m_sceneLastUpdateLocation = lastUpdateLocation;

    m_verticesOutdated = true;

    m_coordinateSystem = nullptr;
}

void SceneUnderstandingRenderer::Update(SpatialCoordinateSystem renderingCoordinateSystem)
{
    // Loading is asynchronous. Resources must be created before they can be updated.
    if (!m_loadingComplete)
    {
        return;
    }

    // Only create the vertices once if the scene was updated.
    std::lock_guard lock(m_mutex);
    if (m_verticesOutdated && !m_verticesUpdating)
    {
        m_verticesUpdating = true;
        CreateVerticesAsync(renderingCoordinateSystem, m_sceneLastUpdateLocation);
    }

    m_validSceneToRenderingTransform = false;

    if (m_scene)
    {
        if (m_coordinateSystem == nullptr)
        {
            try
            {
                m_coordinateSystem = Preview::SpatialGraphInteropPreview::CreateCoordinateSystemForNode(m_scene.OriginSpatialGraphNodeId());
            }
            catch (winrt::hresult_error const&)
            {
                m_coordinateSystem = nullptr;
                return;
            }
        }

        if (m_coordinateSystem)
        {
            // Determine the transform to go from scene space to rendering space.
            IReference<float4x4> sceneToRenderingRef = m_coordinateSystem.TryGetTransformTo(renderingCoordinateSystem);

            if (sceneToRenderingRef)
            {
                float4x4 sceneToRenderingTransform = sceneToRenderingRef.Value();

                DirectX::XMFLOAT4X4 model;
                float4x4 sceneToRenderingTransformT = transpose(sceneToRenderingTransform);
                XMStoreFloat4x4(&model, DirectX::XMLoadFloat4x4(&sceneToRenderingTransformT));

                m_deviceResources->UseD3DDeviceContext([&](auto context) {
                    // Update the model transform buffer for the holograms.
                    context->UpdateSubresource(m_modelConstantBuffer.get(), 0, nullptr, &model, 0, 0);
                });

                m_validSceneToRenderingTransform = true;
            }
        }
    }
}

winrt::fire_and_forget SceneUnderstandingRenderer::CreateVerticesAsync(
    SpatialCoordinateSystem renderingCoordinateSystem, SpatialStationaryFrameOfReference lastUpdateLocation)
{
    auto weakThis = weak_from_this();
    co_await winrt::resume_background();

    if (auto strongThis = weakThis.lock())
    {
        std::lock_guard lock(m_mutex);

        // Clear the vertices.
        m_quadVertices.clear();
        m_quadLabelsVertices.clear();
        m_meshVertices.clear();

        // Collect all scene objects, then iterate to find quad entities
        for (const SceneObject& object : m_scene.SceneObjects())
        {
            // Check if the object is in the quads labels.
            auto quadLabelPos = m_sceneQuadsLabels.find(object.Kind());
            if (quadLabelPos != m_sceneQuadsLabels.end())
            {
                const SceneObjectLabel& label = quadLabelPos->second;
                auto [r, g, b] = label.color;
                float3 color = {r / 255.0f, g / 255.0f, b / 255.0f};

                // Adds the quads to the vertex buffer for rendering, using the color indicated by the label dictionary for the quad's owner
                // entity's type.
                AddSceneQuadsVertices(object, color);

                // Adds the label quads to the vertex buffer for rendering.
                AddSceneQuadLabelVertices(object, color);
            }

            // Check if the object is in the mesh labels.
            auto meshLabelPos = m_sceneMeshLabels.find(object.Kind());
            if (meshLabelPos != m_sceneMeshLabels.end())
            {
                const SceneObjectLabel& label = meshLabelPos->second;
                auto [r, g, b] = label.color;
                float3 color = {r / 255.0f, g / 255.0f, b / 255.0f};

                // Adds the sceneMeshes to the vertex buffer for rendering, using the color indicated by the label dictionary for the quad's
                // owner entity's type.
                AddSceneMeshVertices(object, color);
            }
        }

        // Create the d3d11 vertex buffers.
        const UINT stride = sizeof(VertexPositionUVColor);
        const UINT offset = 0;

        // Quads.
        if (!m_quadVertices.empty())
        {
            m_quadVerticesBuffer = nullptr;
            D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
            vertexBufferData.pSysMem = m_quadVertices.data();
            const CD3D11_BUFFER_DESC vertexBufferDesc(static_cast<UINT>(m_quadVertices.size() * stride), D3D11_BIND_VERTEX_BUFFER);
            winrt::check_hresult(
                m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, m_quadVerticesBuffer.put()));
        }
        // Labels.
        for (auto const& [kind, vertices] : m_quadLabelsVertices)
        {
            if (!vertices.empty())
            {
                m_quadLabelsVerticesBuffer[kind] = nullptr;
                D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
                vertexBufferData.pSysMem = vertices.data();
                const CD3D11_BUFFER_DESC vertexBufferDesc(static_cast<UINT>(vertices.size() * stride), D3D11_BIND_VERTEX_BUFFER);
                winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(
                    &vertexBufferDesc, &vertexBufferData, m_quadLabelsVerticesBuffer[kind].put()));
            }
        }
        // Mesh.
        if (!m_meshVertices.empty())
        {
            m_meshVerticesBuffer = nullptr;
            D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
            vertexBufferData.pSysMem = m_meshVertices.data();
            const CD3D11_BUFFER_DESC vertexBufferDesc(static_cast<UINT>(m_meshVertices.size() * stride), D3D11_BIND_VERTEX_BUFFER);
            winrt::check_hresult(
                m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, m_meshVerticesBuffer.put()));
        }

        // Done with updating.
        m_verticesUpdating = false;
        // All the vertices are now up to date and can be used for rendering.
        m_verticesOutdated = false;
    }
}

void SceneUnderstandingRenderer::AddSceneQuadsVertices(const SceneObject& object, const float3& color)
{
    float4x4 objectToSceneTransform = object.GetLocationAsMatrix();
    for (const SceneQuad& quad : object.Quads())
    {
        // Create the quad's corner points in object space.
        const float width = quad.Extents().x;
        const float height = quad.Extents().y;
        float3 positions[4] = {
            {-width / 2, -height / 2, 0.0f}, {width / 2, -height / 2, 0.0f}, {-width / 2, height / 2, 0.0f}, {width / 2, height / 2, 0.0f}};

        // Transform the vertices to scene space.
        for (int i = 0; i < 4; ++i)
        {
            positions[i] = transform(positions[i], objectToSceneTransform);
        }

        // Create uv coordinates so that the checkerboard pattern becomes uniformly.
        float2 uvs[4] = {{0, 0}, {0, width}, {height, 0}, {height, width}};

        // Create the vertices with uv coordinates for the quad.
        AppendQuad(positions, uvs, height, width, color, m_quadVertices);
    }
}

void SceneUnderstandingRenderer::AddSceneQuadLabelVertices(const SceneObject& object, const float3& color)
{
    float4x4 objectToSceneTransform = object.GetLocationAsMatrix();
    for (const SceneQuad& quad : object.Quads())
    {
        // Create the quad's corner points in object space with a slight offset in the z-direction.
        float3 positions[4] = {
            {-LabelQuadWidth / 2, -LabelQuadHeight / 2, 0.01f},
            {LabelQuadWidth / 2, -LabelQuadHeight / 2, 0.01f},
            {-LabelQuadWidth / 2, LabelQuadHeight / 2, 0.01f},
            {LabelQuadWidth / 2, LabelQuadHeight / 2, 0.01f}};

        // Transform the vertices to scene space.
        for (int i = 0; i < 4; ++i)
        {
            positions[i] = transform(positions[i], objectToSceneTransform);
        }
        // Create uv coordinates.
        float2 uvs[4] = {{0, 1}, {1, 1}, {0, 0}, {1, 0}};

        // Create the vertices with uv coordinates for the quad labels.
        AppendQuad(positions, uvs, LabelQuadHeight, LabelQuadWidth, color, m_quadLabelsVertices[object.Kind()]);
    }
}

void SceneUnderstandingRenderer::AddSceneMeshVertices(const SceneObject& object, const float3& color)
{
    float4x4 objectToSceneTransform = object.GetLocationAsMatrix();
    for (const SceneMesh& mesh : object.Meshes())
    {
        auto indicesCount = mesh.TriangleIndexCount();
        std::vector<uint32_t> indices(indicesCount);
        mesh.GetTriangleIndices(indices);

        // Get the mesh's vertices in object space.
        std::vector<float3> vertices(mesh.VertexCount());
        mesh.GetVertexPositions(vertices);

        // Transform the vertices to scene space and create the triangles.
        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            VertexPositionUVColor vertex;
            vertex.color = DXHelper::Float3ToXMFloat3(color);
            vertex.uv = {0, 0};

            vertex.pos = DXHelper::Float3ToXMFloat3(transform(vertices[indices[i]], objectToSceneTransform));
            m_meshVertices.push_back(vertex);

            vertex.pos = DXHelper::Float3ToXMFloat3(transform(vertices[indices[i + 1]], objectToSceneTransform));
            m_meshVertices.push_back(vertex);

            vertex.pos = DXHelper::Float3ToXMFloat3(transform(vertices[indices[i + 2]], objectToSceneTransform));
            m_meshVertices.push_back(vertex);
        }
    }
}

void SceneUnderstandingRenderer::ToggleRenderingType()
{
    m_renderingType = static_cast<RenderingType>((m_renderingType + 1) % RenderingType::Max);
}

void SceneUnderstandingRenderer::Render(bool isStereo)
{
    // Loading is asynchronous. Resources must be created before drawing can occur.
    if (!m_loadingComplete)
    {
        return;
    }

    // Only render if the scene is not being updated and there is a valid scene to rendering transformation.
    if (!m_verticesUpdating && m_validSceneToRenderingTransform)
    {
        // For RenderingType::Mesh only render the scene mesh. In case of RenderingType::Quads only render the scene quads with labels. For
        // RenderingType::All render the scene mesh and the scene quads with labels.
        if (m_renderingType == RenderingType::Quads || m_renderingType == RenderingType::All)
        {
            RenderSceneQuads(isStereo);
            RenderSceneQuadsLabel(isStereo);
        }
        if (m_renderingType == RenderingType::Mesh || m_renderingType == RenderingType::All)
        {
            RenderSceneMesh(isStereo);
        }

        // Disable the geometry shader.
        m_deviceResources->UseD3DDeviceContext([&](auto context) { context->GSSetShader(nullptr, nullptr, 0); });
    }
}

void SceneUnderstandingRenderer::RenderSceneQuads(bool isStereo)
{
    // Only render if vertices are available.
    if (m_quadVertices.empty())
    {
        return;
    }

    // Use the D3D device context to update Direct3D device-based resources.
    m_deviceResources->UseD3DDeviceContext([&](auto context) {
        context->IASetInputLayout(m_inputLayout.get());

        // Attach the vertex shader.
        context->VSSetShader(m_vertexShader.get(), nullptr, 0);
        // Apply the model constant buffer to the vertex shader.
        ID3D11Buffer* modelBuffer = m_modelConstantBuffer.get();
        context->VSSetConstantBuffers(0, 1, &modelBuffer);

        context->GSSetShader(m_geometryShader.get(), nullptr, 0);

        context->PSSetShader(m_quadsPixelShader.get(), nullptr, 0);

        context->RSSetState(m_rasterizerState.get());

        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const UINT stride = sizeof(VertexPositionUVColor);
        const UINT offset = 0;
        ID3D11Buffer* pBuffer = m_quadVerticesBuffer.get();
        context->IASetVertexBuffers(0, 1, &pBuffer, &stride, &offset);

        context->DrawInstanced(static_cast<UINT>(m_quadVertices.size()), isStereo ? 2 : 1, offset, 0);
    });
}

void SceneUnderstandingRenderer::RenderSceneQuadsLabel(bool isStereo)
{
    // Use the D3D device context to update Direct3D device-based resources.
    m_deviceResources->UseD3DDeviceContext([&](auto context) {
        context->OMSetBlendState(m_blendState.get(), nullptr, 0xffffffff);
        context->IASetInputLayout(m_inputLayout.get());

        // Attach the vertex shader.
        context->VSSetShader(m_vertexShader.get(), nullptr, 0);
        ID3D11Buffer* modelBuffer = m_modelConstantBuffer.get();
        context->VSSetConstantBuffers(0, 1, &modelBuffer);

        context->GSSetShader(m_geometryShader.get(), nullptr, 0);

        context->PSSetShader(m_labelPixelShader.get(), nullptr, 0);

        ID3D11SamplerState* pSamplerToSet = m_textSamplerState.get();
        context->PSSetSamplers(0, 1, &pSamplerToSet);

        context->RSSetState(m_rasterizerState.get());

        // Render all quad labels with the same SceneObjectKind with a single draw call.
        for (auto const& [kind, vertices] : m_quadLabelsVertices)
        {
            // Only render if vertices are available.
            if (vertices.empty())
            {
                continue;
            }

            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            const UINT stride = sizeof(VertexPositionUVColor);
            const UINT offset = 0;
            ID3D11Buffer* pBuffer = m_quadLabelsVerticesBuffer[kind].get();
            context->IASetVertexBuffers(0, 1, &pBuffer, &stride, &offset);

            // Set the text label texture which contains the label name.
            ID3D11ShaderResourceView* pShaderViewToSet = m_textShaderResourceViews[kind].get();
            context->PSSetShaderResources(0, 1, &pShaderViewToSet);

            context->DrawInstanced(static_cast<UINT>(vertices.size()), isStereo ? 2 : 1, offset, 0);
        }

        context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    });
}

void SceneUnderstandingRenderer::RenderSceneMesh(bool isStereo)
{
    // Only render if vertices are available.
    if (m_meshVertices.empty())
    {
        return;
    }

    // Use the D3D device context to update Direct3D device-based resources.
    m_deviceResources->UseD3DDeviceContext([&](auto context) {
        context->OMSetBlendState(m_blendState.get(), nullptr, 0xffffffff);

        context->IASetInputLayout(m_inputLayout.get());

        context->VSSetShader(m_vertexShader.get(), nullptr, 0);
        ID3D11Buffer* modelBuffer = m_modelConstantBuffer.get();
        context->VSSetConstantBuffers(0, 1, &modelBuffer);

        context->GSSetShader(m_geometryShader.get(), nullptr, 0);

        context->PSSetShader(m_meshPixelShader.get(), nullptr, 0);

        context->RSSetState(m_rasterizerState.get());

        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const UINT stride = sizeof(VertexPositionUVColor);
        const UINT offset = 0;
        ID3D11Buffer* pBuffer = m_meshVerticesBuffer.get();
        context->IASetVertexBuffers(0, 1, &pBuffer, &stride, &offset);

        context->DrawInstanced(static_cast<UINT>(m_meshVertices.size()), isStereo ? 2 : 1, offset, 0);

        context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    });
}

// Appends a quad to the given collection.
void SceneUnderstandingRenderer::AppendQuad(
    const float3 positions[4],
    const float2 uvs[4],
    const float height,
    const float width,
    const float3& color,
    std::vector<VertexPositionUVColor>& vertices)
{
    VertexPositionUVColor vertex;
    vertex.color = DXHelper::Float3ToXMFloat3(color);

    vertex.pos = DXHelper::Float3ToXMFloat3(positions[0]);
    vertex.uv = DXHelper::Float2ToXMFloat2(uvs[0]);
    vertices.push_back(vertex);

    vertex.pos = DXHelper::Float3ToXMFloat3(positions[2]);
    vertex.uv = DXHelper::Float2ToXMFloat2(uvs[2]);
    vertices.push_back(vertex);

    vertex.pos = DXHelper::Float3ToXMFloat3(positions[3]);
    vertex.uv = DXHelper::Float2ToXMFloat2(uvs[3]);
    vertices.push_back(vertex);

    vertex.pos = DXHelper::Float3ToXMFloat3(positions[3]);
    vertex.uv = DXHelper::Float2ToXMFloat2(uvs[3]);
    vertices.push_back(vertex);

    vertex.pos = DXHelper::Float3ToXMFloat3(positions[1]);
    vertex.uv = DXHelper::Float2ToXMFloat2(uvs[1]);
    vertices.push_back(vertex);

    vertex.pos = DXHelper::Float3ToXMFloat3(positions[0]);
    vertex.uv = DXHelper::Float2ToXMFloat2(uvs[0]);
    vertices.push_back(vertex);
}

void SceneUnderstandingRenderer::Reset()
{
    std::lock_guard lock(m_mutex);

    m_scene = nullptr;
    m_sceneLastUpdateLocation = nullptr;
    m_verticesOutdated = false;
    m_verticesUpdating = false;
}
