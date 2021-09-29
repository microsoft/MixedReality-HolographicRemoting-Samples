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

#include "StatusDisplay.h"

#include <shaders\GeometryShader.h>
#include <shaders\PixelShader.h>
#include <shaders\VPRTVertexShader.h>
#include <shaders\VertexShader.h>

#include <DirectXHelper.h>

constexpr const wchar_t Font[] = L"Segoe UI";
// Font size in percent.
constexpr float FontSizeLarge = 0.045f;
constexpr float FontSizeMedium = 0.035f;
constexpr float FontSizeSmall = 0.03f;
constexpr const wchar_t FontLanguage[] = L"en-US";
constexpr const float Degree2Rad = 3.14159265359f / 180.0f;
constexpr const float Meter2Inch = 39.37f;

using namespace DirectX;
using namespace Concurrency;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI::Input::Spatial;

namespace
{
    float3 GetPlanesIntersectionPoint(const plane& p0, const plane& p1, const plane& p2)
    {
        const float3 n1(p0.normal);
        const float3 n2(p1.normal);
        const float3 n3(p2.normal);
        const float det = dot(n1, cross(n2, n3));
        return (-p0.d * cross(n2, n3) + -p1.d * cross(n3, n1) + -p2.d * cross(n1, n2)) / det;
    }

    std::tuple<float3, float3> GetOriginAndDirectionFromFrustum(const winrt::Windows::Perception::Spatial::SpatialBoundingFrustum& frustum)
    {
        float3 points[8];

        points[0] = GetPlanesIntersectionPoint(frustum.Near, frustum.Top, frustum.Left);
        points[1] = GetPlanesIntersectionPoint(frustum.Near, frustum.Top, frustum.Right);
        points[2] = GetPlanesIntersectionPoint(frustum.Near, frustum.Bottom, frustum.Left);
        points[3] = GetPlanesIntersectionPoint(frustum.Near, frustum.Bottom, frustum.Right);
        float3 origin = (points[0] + points[1] + points[2] + points[3]) * 0.25f;

        points[4] = GetPlanesIntersectionPoint(frustum.Far, frustum.Top, frustum.Left);
        points[5] = GetPlanesIntersectionPoint(frustum.Far, frustum.Top, frustum.Right);
        points[6] = GetPlanesIntersectionPoint(frustum.Far, frustum.Bottom, frustum.Left);
        points[7] = GetPlanesIntersectionPoint(frustum.Far, frustum.Bottom, frustum.Right);
        float3 direction = normalize((points[4] + points[5] + points[6] + points[7]) * 0.25f - origin);

        return {origin, direction};
    }

} // namespace

// Initializes D2D resources used for text rendering.
StatusDisplay::StatusDisplay(const std::shared_ptr<DXHelper::DeviceResourcesD3D11>& deviceResources)
    : m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();
}

// Called once per frame. Rotates the quad, and calculates and sets the model matrix
// relative to the position transform indicated by hologramPositionTransform.
void StatusDisplay::Update(float deltaTimeInSeconds)
{
    UpdateConstantBuffer(
        deltaTimeInSeconds, m_modelConstantBufferDataImage, m_isOpaque ? m_positionContent : m_positionOffset, m_normalContent);
    UpdateConstantBuffer(deltaTimeInSeconds, m_modelConstantBufferDataText, m_positionContent, m_normalContent);
}

// Renders a frame to the screen.
void StatusDisplay::Render()
{
    // Loading is asynchronous. Resources must be created before drawing can occur.
    if (!m_loadingComplete)
    {
        return;
    }

    // First render all text using direct2D.
    {
        std::scoped_lock lock(m_lineMutex);
        if (m_lines.size() > 0 && m_lines != m_previousLines)
        {
            m_previousLines.resize(m_lines.size());
            m_runtimeLines.resize(m_lines.size());

            for (int i = 0; i < m_lines.size(); ++i)
            {
                if (m_lines[i] != m_previousLines[i])
                {
                    UpdateLineInternal(m_runtimeLines[i], m_lines[i]);
                    m_previousLines[i] = m_lines[i];
                }
            }

            m_deviceResources->UseD3DDeviceContext(
                [&](auto context) { context->ClearRenderTargetView(m_textRenderTarget.get(), DirectX::Colors::Transparent); });

            m_d2dTextRenderTarget->BeginDraw();

            const float virtualDisplayDPIy = m_textTextureHeight / m_virtualDisplaySizeInchY;
            const float dpiScaleY = virtualDisplayDPIy / 96.0f;

            float top = 0.0f;
            for (auto& line : m_runtimeLines)
            {
                if (line.alignBottom)
                {
                    top = m_textTextureHeight - (line.metrics.height * line.lineHeightMultiplier * dpiScaleY);
                }

                m_d2dTextRenderTarget->DrawTextLayout(D2D1::Point2F(0, top), line.layout.get(), m_brushes[line.color].get());
                top += line.metrics.height * line.lineHeightMultiplier;
            }

            // Ignore D2DERR_RECREATE_TARGET here. This error indicates that the device
            // is lost. It will be handled during the next call to Present.
            const HRESULT hr = m_d2dTextRenderTarget->EndDraw();
            if (hr != D2DERR_RECREATE_TARGET)
            {
                winrt::check_hresult(hr);
            }
        }
    }

    // Now render the quads into 3d space
    if (m_imageEnabled && m_imageView || !m_lines.empty())
    {
        m_deviceResources->UseD3DDeviceContext([&](auto context) {
            DXHelper::D3D11StoreAndRestoreState(context, [&]() {
                // Each vertex is one instance of the VertexPositionUV struct.
                const UINT stride = sizeof(VertexPositionUV);
                const UINT offset = 0;
                ID3D11Buffer* pBufferToSet = m_vertexBufferImage.get();
                context->IASetVertexBuffers(0, 1, &pBufferToSet, &stride, &offset);
                context->IASetIndexBuffer(m_indexBuffer.get(), DXGI_FORMAT_R16_UINT, 0);

                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                context->IASetInputLayout(m_inputLayout.get());
                context->OMSetBlendState(m_textAlphaBlendState.get(), nullptr, 0xffffffff);
                context->OMSetDepthStencilState(m_depthStencilState.get(), 0);

                context->UpdateSubresource(m_modelConstantBuffer.get(), 0, nullptr, &m_modelConstantBufferDataImage, 0, 0);

                // Apply the model constant buffer to the vertex shader.
                pBufferToSet = m_modelConstantBuffer.get();
                context->VSSetConstantBuffers(0, 1, &pBufferToSet);

                // Attach the vertex shader.
                context->VSSetShader(m_vertexShader.get(), nullptr, 0);

                // On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
                // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
                // a pass-through geometry shader sets the render target ID.
                context->GSSetShader(!m_usingVprtShaders ? m_geometryShader.get() : nullptr, nullptr, 0);

                // Attach the pixel shader.
                context->PSSetShader(m_pixelShader.get(), nullptr, 0);

                // Draw the image.
                if (m_imageEnabled && m_imageView)
                {
                    ID3D11ShaderResourceView* pShaderViewToSet = m_imageView.get();
                    context->PSSetShaderResources(0, 1, &pShaderViewToSet);

                    ID3D11SamplerState* pSamplerToSet = m_imageSamplerState.get();
                    context->PSSetSamplers(0, 1, &pSamplerToSet);

                    context->DrawIndexedInstanced(
                        m_indexCount, // Index count per instance.
                        2,            // Instance count.
                        0,            // Start index location.
                        0,            // Base vertex location.
                        0             // Start instance location.
                    );
                }

                // Draw the text.
                if (!m_lines.empty())
                {
                    // Set up for rendering the texture that contains the text
                    pBufferToSet = m_vertexBufferText.get();
                    context->IASetVertexBuffers(0, 1, &pBufferToSet, &stride, &offset);

                    ID3D11ShaderResourceView* pShaderViewToSet = m_textShaderResourceView.get();
                    context->PSSetShaderResources(0, 1, &pShaderViewToSet);

                    ID3D11SamplerState* pSamplerToSet = m_textSamplerState.get();
                    context->PSSetSamplers(0, 1, &pSamplerToSet);

                    context->UpdateSubresource(m_modelConstantBuffer.get(), 0, nullptr, &m_modelConstantBufferDataText, 0, 0);

                    context->DrawIndexedInstanced(
                        m_indexCount, // Index count per instance.
                        2,            // Instance count.
                        0,            // Start index location.
                        0,            // Base vertex location.
                        0             // Start instance location.
                    );
                }
            });
        });
    }
}

void StatusDisplay::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    CD3D11_TEXTURE2D_DESC textureDesc(
        DXGI_FORMAT_B8G8R8A8_UNORM, m_textTextureWidth, m_textTextureHeight, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

    m_textTexture = nullptr;
    device->CreateTexture2D(&textureDesc, nullptr, m_textTexture.put());

    m_textShaderResourceView = nullptr;
    device->CreateShaderResourceView(m_textTexture.get(), nullptr, m_textShaderResourceView.put());

    m_textRenderTarget = nullptr;
    device->CreateRenderTargetView(m_textTexture.get(), nullptr, m_textRenderTarget.put());

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);

    winrt::com_ptr<IDXGISurface> dxgiSurface;
    m_textTexture.as(dxgiSurface);

    m_d2dTextRenderTarget = nullptr;
    winrt::check_hresult(
        m_deviceResources->GetD2DFactory()->CreateDxgiSurfaceRenderTarget(dxgiSurface.get(), &props, m_d2dTextRenderTarget.put()));

    CreateFonts();
    CreateBrushes();

    m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

    // If the optional VPRT feature is supported by the graphics device, we
    // can avoid using geometry shaders to set the render target array index.
    const auto vertexShaderData = m_usingVprtShaders ? VPRTVertexShader : VertexShader;
    const auto vertexShaderDataSize = m_usingVprtShaders ? sizeof(VPRTVertexShader) : sizeof(VertexShader);

    // create the vertex shader and input layout.
    task<void> createVSTask = task<void>([this, device, vertexShaderData, vertexShaderDataSize]() {
        winrt::check_hresult(device->CreateVertexShader(vertexShaderData, vertexShaderDataSize, nullptr, m_vertexShader.put()));

        static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        winrt::check_hresult(
            device->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), vertexShaderData, vertexShaderDataSize, m_inputLayout.put()));
    });

    // create the pixel shader and constant buffer.
    task<void> createPSTask([this, device]() {
        winrt::check_hresult(device->CreatePixelShader(PixelShader, sizeof(PixelShader), nullptr, m_pixelShader.put()));

        const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
        winrt::check_hresult(device->CreateBuffer(&constantBufferDesc, nullptr, m_modelConstantBuffer.put()));
    });

    task<void> createGSTask;
    if (!m_usingVprtShaders)
    {
        // create the geometry shader.
        createGSTask = task<void>([this, device]() {
            winrt::check_hresult(device->CreateGeometryShader(GeometryShader, sizeof(GeometryShader), nullptr, m_geometryShader.put()));
        });
    }

    // Once all shaders are loaded, create the mesh.
    task<void> shaderTaskGroup = m_usingVprtShaders ? (createPSTask && createVSTask) : (createPSTask && createVSTask && createGSTask);
    task<void> createQuadTask = shaderTaskGroup.then([this, device]() {
        // Load mesh indices. Each trio of indices represents
        // a triangle to be rendered on the screen.
        // For example: 2,1,0 means that the vertices with indexes
        // 2, 1, and 0 from the vertex buffer compose the
        // first triangle of this mesh.
        // Note that the winding order is clockwise by default.
        static const unsigned short quadIndices[] = {
            0,
            2,
            3, // -z
            0,
            1,
            2,
        };

        m_indexCount = ARRAYSIZE(quadIndices);

        D3D11_SUBRESOURCE_DATA indexBufferData = {0};
        indexBufferData.pSysMem = quadIndices;
        indexBufferData.SysMemPitch = 0;
        indexBufferData.SysMemSlicePitch = 0;
        const CD3D11_BUFFER_DESC indexBufferDesc(sizeof(quadIndices), D3D11_BIND_INDEX_BUFFER);
        winrt::check_hresult(device->CreateBuffer(&indexBufferDesc, &indexBufferData, m_indexBuffer.put()));
    });

    // Create image sampler state
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = 3;
        samplerDesc.MipLODBias = 0.f;
        samplerDesc.BorderColor[0] = 0.f;
        samplerDesc.BorderColor[1] = 0.f;
        samplerDesc.BorderColor[2] = 0.f;
        samplerDesc.BorderColor[3] = 0.f;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        winrt::check_hresult(device->CreateSamplerState(&samplerDesc, m_imageSamplerState.put()));
    }

    // Create text sampler state
    {
        CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
        winrt::check_hresult(device->CreateSamplerState(&samplerDesc, m_textSamplerState.put()));
    }

    // Create the blend state.  This sets up a blend state for pre-multiplied alpha produced by TextRenderer.cpp's Direct2D text
    // renderer.
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

    winrt::check_hresult(device->CreateBlendState(&blendStateDesc, m_textAlphaBlendState.put()));

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    device->CreateDepthStencilState(&depthStencilDesc, m_depthStencilState.put());

    // Once the quad is loaded, the object is ready to be rendered.
    auto loadCompleteCallback = createQuadTask.then([this]() { m_loadingComplete = true; });
}

void StatusDisplay::ReleaseDeviceDependentResources()
{
    m_loadingComplete = false;
    m_usingVprtShaders = false;

    m_vertexShader = nullptr;
    m_inputLayout = nullptr;
    m_pixelShader = nullptr;
    m_geometryShader = nullptr;

    m_modelConstantBuffer = nullptr;

    m_vertexBufferImage = nullptr;
    m_vertexBufferText = nullptr;
    m_indexBuffer = nullptr;

    m_imageView = nullptr;
    m_imageSamplerState = nullptr;

    m_textSamplerState = nullptr;
    m_textAlphaBlendState = nullptr;

    for (size_t i = 0; i < ARRAYSIZE(m_brushes); i++)
    {
        m_brushes[i] = nullptr;
    }
    for (size_t i = 0; i < ARRAYSIZE(m_textFormats); i++)
    {
        m_textFormats[i] = nullptr;
    }
}

void StatusDisplay::ClearLines()
{
    std::scoped_lock lock(m_lineMutex);
    m_lines.resize(0);
}

void StatusDisplay::SetLines(winrt::array_view<Line> lines)
{
    std::scoped_lock lock(m_lineMutex);
    auto numLines = lines.size();
    m_lines.resize(numLines);

    for (uint32_t i = 0; i < numLines; i++)
    {
        assert((!lines[i].alignBottom || i == numLines - 1) && "Only the last line can use alignBottom = true");
        m_lines[i] = lines[i];
    }
}

void StatusDisplay::UpdateLineText(size_t index, std::wstring text)
{
    std::scoped_lock lock(m_lineMutex);
    if (index >= m_lines.size())
    {
        return;
    }

    m_lines[index].text = text;
}

size_t StatusDisplay::AddLine(const Line& line)
{
    std::scoped_lock lock(m_lineMutex);
    size_t newIndex = m_lines.size();
    m_lines.resize(newIndex + 1);
    m_lines[newIndex] = line;
    return newIndex;
}

bool StatusDisplay::HasLine(size_t index)
{
    std::scoped_lock lock(m_lineMutex);
    return index < m_lines.size();
}

void StatusDisplay::CreateFonts()
{
    // DIP font size, based on the horizontal size of the virtual display.
    float fontSizeLargeDIP = (m_virtualDisplaySizeInchX * FontSizeLarge) * 96;
    float fontSizeMediumDIP = (m_virtualDisplaySizeInchX * FontSizeMedium) * 96;
    float fontSizeSmallDIP = (m_virtualDisplaySizeInchX * FontSizeSmall) * 96;

    // Create Large font
    m_textFormats[Large] = nullptr;
    winrt::check_hresult(m_deviceResources->GetDWriteFactory()->CreateTextFormat(
        Font,
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSizeLargeDIP,
        FontLanguage,
        m_textFormats[Large].put()));
    winrt::check_hresult(m_textFormats[Large]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    winrt::check_hresult(m_textFormats[Large]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));

    // Create large bold font
    m_textFormats[LargeBold] = nullptr;
    winrt::check_hresult(m_deviceResources->GetDWriteFactory()->CreateTextFormat(
        Font,
        nullptr,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSizeLargeDIP,
        FontLanguage,
        m_textFormats[LargeBold].put()));
    winrt::check_hresult(m_textFormats[LargeBold]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    winrt::check_hresult(m_textFormats[LargeBold]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));

    // Create small font
    m_textFormats[Small] = nullptr;
    winrt::check_hresult(m_deviceResources->GetDWriteFactory()->CreateTextFormat(
        Font,
        nullptr,
        DWRITE_FONT_WEIGHT_MEDIUM,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSizeSmallDIP,
        FontLanguage,
        m_textFormats[Small].put()));
    winrt::check_hresult(m_textFormats[Small]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    winrt::check_hresult(m_textFormats[Small]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));

    // Create medium font
    m_textFormats[Medium] = nullptr;
    winrt::check_hresult(m_deviceResources->GetDWriteFactory()->CreateTextFormat(
        Font,
        nullptr,
        DWRITE_FONT_WEIGHT_MEDIUM,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSizeMediumDIP,
        FontLanguage,
        m_textFormats[Medium].put()));
    winrt::check_hresult(m_textFormats[Medium]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    winrt::check_hresult(m_textFormats[Medium]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));

    static_assert(TextFormatCount == 4, "Expected 4 text formats");
}

void StatusDisplay::CreateBrushes()
{
    m_brushes[White] = nullptr;
    winrt::check_hresult(m_d2dTextRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::FloralWhite), m_brushes[White].put()));

    m_brushes[Yellow] = nullptr;
    winrt::check_hresult(m_d2dTextRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), m_brushes[Yellow].put()));

    m_brushes[Red] = nullptr;
    winrt::check_hresult(m_d2dTextRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), m_brushes[Red].put()));
}

void StatusDisplay::UpdateLineInternal(RuntimeLine& runtimeLine, const Line& line)
{
    assert(line.format >= 0 && line.format < TextFormatCount && "Line text format out of bounds");
    assert(line.color >= 0 && line.color < TextColorCount && "Line text color out of bounds");

    if (line.format != runtimeLine.format || line.text != runtimeLine.text)
    {
        runtimeLine.format = line.format;
        runtimeLine.text = line.text;

        const float virtualDisplayDPIx = m_textTextureWidth / m_virtualDisplaySizeInchX;
        const float virtualDisplayDPIy = m_textTextureHeight / m_virtualDisplaySizeInchY;

        const float dpiScaleX = virtualDisplayDPIx / 96.0f;
        const float dpiScaleY = virtualDisplayDPIy / 96.0f;

        runtimeLine.layout = nullptr;
        winrt::check_hresult(m_deviceResources->GetDWriteFactory()->CreateTextLayout(
            line.text.c_str(),
            static_cast<UINT32>(line.text.length()),
            m_textFormats[line.format].get(),
            static_cast<float>(m_textTextureWidth / dpiScaleX),  // Max width of the input text.
            static_cast<float>(m_textTextureHeight / dpiScaleY), // Max height of the input text.
            runtimeLine.layout.put()));

        winrt::check_hresult(runtimeLine.layout->GetMetrics(&runtimeLine.metrics));
    }

    runtimeLine.color = line.color;
    runtimeLine.lineHeightMultiplier = line.lineHeightMultiplier;
    runtimeLine.alignBottom = line.alignBottom;
}

void StatusDisplay::SetImage(const winrt::com_ptr<ID3D11ShaderResourceView>& imageView)
{
    m_imageView = imageView;
}

// This function uses a SpatialPointerPose to position the world-locked hologram
// two meters in front of the user's heading.
void StatusDisplay::PositionDisplay(
    float deltaTimeInSeconds,
    const winrt::Windows::Perception::Spatial::SpatialBoundingFrustum& frustum,
    float imageOffsetX,
    float imageOffsetY)
{
    const auto [origin, direction] = GetOriginAndDirectionFromFrustum(frustum);

    const float3 contentPosition = origin + (direction * m_statusDisplayDistance);

    const float3 headRight = normalize(cross(direction, float3(0, 1, 0)));
    const float3 headUp = normalize(cross(headRight, direction));

    m_positionContent = lerp(m_positionContent, contentPosition, deltaTimeInSeconds * c_lerpRate);
    m_positionOffset =
        m_positionContent + (headRight * m_virtualDisplaySizeInchX * imageOffsetX) + (headUp * m_virtualDisplaySizeInchY * imageOffsetY);
    m_normalContent = direction;
}

void StatusDisplay::UpdateConstantBuffer(
    float deltaTimeInSeconds,
    ModelConstantBuffer& buffer,
    winrt::Windows::Foundation::Numerics::float3 position,
    winrt::Windows::Foundation::Numerics::float3 normal)
{
    // Create a direction normal from the hologram's position to the origin of person space.
    // This is the z-axis rotation.
    XMVECTOR facingNormal = XMVector3Normalize(-XMLoadFloat3(&normal));

    // Rotate the x-axis around the y-axis.
    // This is a 90-degree angle from the normal, in the xz-plane.
    // This is the x-axis rotation.
    XMVECTOR xAxisRotation = XMVector3Normalize(XMVectorSet(XMVectorGetZ(facingNormal), 0.f, -XMVectorGetX(facingNormal), 0.f));

    // Create a third normal to satisfy the conditions of a rotation matrix.
    // The cross product  of the other two normals is at a 90-degree angle to
    // both normals. (Normalize the cross product to avoid floating-point math
    // errors.)
    // Note how the cross product will never be a zero-matrix because the two normals
    // are always at a 90-degree angle from one another.
    XMVECTOR yAxisRotation = XMVector3Normalize(XMVector3Cross(facingNormal, xAxisRotation));

    // Construct the 4x4 rotation matrix.

    // Rotate the quad to face the user.
    XMMATRIX rotationMatrix = XMMATRIX(xAxisRotation, yAxisRotation, facingNormal, XMVectorSet(0.f, 0.f, 0.f, 1.f));

    // Position the quad.
    const XMMATRIX modelTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&position));

    // The view and projection matrices are provided by the system; they are associated
    // with holographic cameras, and updated on a per-camera basis.
    // Here, we provide the model transform for the sample hologram. The model transform
    // matrix is transposed to prepare it for the shader.
    XMStoreFloat4x4(&buffer.model, XMMatrixTranspose(rotationMatrix * modelTranslation));
}

void StatusDisplay::UpdateTextScale(
    winrt::Windows::Graphics::Holographic::HolographicStereoTransform holoTransform,
    float screenWidth,
    float screenHeight,
    bool isLandscape,
    bool isOpaque)
{
    DirectX::XMMATRIX projMat = XMLoadFloat4x4(&holoTransform.Left);
    DirectX::XMFLOAT4X4 proj;
    DirectX::XMStoreFloat4x4(&proj, projMat);
    // Check if the projection matrix has changed.
    bool projHasChanged = false;
    for (int x = 0; x < 4; ++x)
    {
        for (int y = 0; y < 4; ++y)
        {
            if (proj.m[x][y] != m_projection.m[x][y])
            {
                projHasChanged = true;
                break;
            }
        }
        if (projHasChanged)
        {
            break;
        }
    }

    m_isOpaque = isOpaque;

    float quadFov = m_defaultQuadFov;
    float heightRatio = 1.0f;
    if (isLandscape)
    {
        quadFov = m_landscapeQuadFov;
        heightRatio = m_landscapeHeightRatio;
    }

    if (m_isOpaque)
    {
        quadFov *= 1.5f;
    }

    const float fovDiff = m_currentQuadFov - quadFov;
    const float fovEpsilon = 0.1f;
    const bool quadFovHasChanged = std::abs(fovDiff) > fovEpsilon;
    m_currentQuadFov = quadFov;

    const float heightRatioDiff = m_currentHeightRatio - heightRatio;
    const float heightRatioEpsilon = 0.1f;
    const bool quadRatioHasChanged = std::abs(heightRatioDiff) > heightRatioEpsilon;
    m_currentHeightRatio = heightRatio;

    // Only update the StatusDisplay resolution and size if something has changed.
    if (projHasChanged || quadFovHasChanged || quadRatioHasChanged)
    {
        // Quad extent based on FOV.
        const float quadExtentX = tan((m_currentQuadFov / 2.0f) * Degree2Rad) * m_statusDisplayDistance;
        const float quadExtentY = m_currentHeightRatio * quadExtentX;

        // Calculate the virtual display size in inch.
        m_virtualDisplaySizeInchX = (quadExtentX * 2.0f) * Meter2Inch;
        m_virtualDisplaySizeInchY = (quadExtentY * 2.0f) * Meter2Inch;

        // Pixel perfect resolution.
        const float resX = screenWidth * quadExtentX / m_statusDisplayDistance * proj._11;
        const float resY = screenHeight * quadExtentY / m_statusDisplayDistance * proj._22;

        // sample with double resolution for multi sampling.
        m_textTextureWidth = static_cast<int>(resX * 2.0f);
        m_textTextureHeight = static_cast<int>(resY * 2.0f);

        m_projection = proj;

        // Create the new texture.
        auto device = m_deviceResources->GetD3DDevice();

        // Load mesh vertices. Each vertex has a position and a color.
        // Note that the quad size has changed from the default DirectX app
        // template. The quad size is based on the target FOV.
        const VertexPositionUV quadVerticesText[] = {
            {XMFLOAT3(-quadExtentX, quadExtentY, 0.f), XMFLOAT2(0.f, 0.f)},
            {XMFLOAT3(quadExtentX, quadExtentY, 0.f), XMFLOAT2(1.f, 0.f)},
            {XMFLOAT3(quadExtentX, -quadExtentY, 0.f), XMFLOAT2(1.f, 1.f)},
            {XMFLOAT3(-quadExtentX, -quadExtentY, 0.f), XMFLOAT2(0.f, 1.f)},
        };

        D3D11_SUBRESOURCE_DATA vertexBufferDataText = {0};
        vertexBufferDataText.pSysMem = quadVerticesText;
        vertexBufferDataText.SysMemPitch = 0;
        vertexBufferDataText.SysMemSlicePitch = 0;
        const CD3D11_BUFFER_DESC vertexBufferDescText(sizeof(quadVerticesText), D3D11_BIND_VERTEX_BUFFER);

        m_vertexBufferText = nullptr;
        winrt::check_hresult(device->CreateBuffer(&vertexBufferDescText, &vertexBufferDataText, m_vertexBufferText.put()));

        // Create image buffer
        // The image contains 50% of the textFOV.
        const float imageFOVDegree = (m_isOpaque ? 0.75f : 0.2f) * (m_currentQuadFov * 0.5f);
        const float imageQuadExtent = m_statusDisplayDistance / tan((90.0f - imageFOVDegree) * Degree2Rad);

        const VertexPositionUV quadVertices[] = {
            {XMFLOAT3(-imageQuadExtent, imageQuadExtent, 0.f), XMFLOAT2(0.f, 0.f)},
            {XMFLOAT3(imageQuadExtent, imageQuadExtent, 0.f), XMFLOAT2(1.f, 0.f)},
            {XMFLOAT3(imageQuadExtent, -imageQuadExtent, 0.f), XMFLOAT2(1.f, 1.f)},
            {XMFLOAT3(-imageQuadExtent, -imageQuadExtent, 0.f), XMFLOAT2(0.f, 1.f)},
        };

        D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
        vertexBufferData.pSysMem = quadVertices;
        vertexBufferData.SysMemPitch = 0;
        vertexBufferData.SysMemSlicePitch = 0;
        const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(quadVertices), D3D11_BIND_VERTEX_BUFFER);
        m_vertexBufferImage = nullptr;
        winrt::check_hresult(device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, m_vertexBufferImage.put()));

        CD3D11_TEXTURE2D_DESC textureDesc(
            DXGI_FORMAT_B8G8R8A8_UNORM,
            m_textTextureWidth,
            m_textTextureHeight,
            1,
            1,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

        m_textTexture = nullptr;
        device->CreateTexture2D(&textureDesc, nullptr, m_textTexture.put());

        m_textShaderResourceView = nullptr;
        device->CreateShaderResourceView(m_textTexture.get(), nullptr, m_textShaderResourceView.put());

        m_textRenderTarget = nullptr;
        device->CreateRenderTargetView(m_textTexture.get(), nullptr, m_textRenderTarget.put());

        const float virtualDisplayDPIx = m_textTextureWidth / m_virtualDisplaySizeInchX;
        const float virtualDisplayDPIy = m_textTextureHeight / m_virtualDisplaySizeInchY;

        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
            virtualDisplayDPIx,
            virtualDisplayDPIy);

        winrt::com_ptr<IDXGISurface> dxgiSurface;
        m_textTexture.as(dxgiSurface);

        m_d2dTextRenderTarget = nullptr;
        winrt::check_hresult(
            m_deviceResources->GetD2DFactory()->CreateDxgiSurfaceRenderTarget(dxgiSurface.get(), &props, m_d2dTextRenderTarget.put()));

        // Update the fonts.
        CreateFonts();

        // Trigger full recreation in the next frame
        m_previousLines.clear();
        m_runtimeLines.clear();
    }
}

bool StatusDisplay::Line::operator==(const Line& line) const
{
    return std::tie(text, format, color, lineHeightMultiplier, alignBottom) ==
           std::tie(line.text, line.format, line.color, line.lineHeightMultiplier, line.alignBottom);
}

bool StatusDisplay::Line::operator!=(const Line& line) const
{
    return !operator==(line);
}
