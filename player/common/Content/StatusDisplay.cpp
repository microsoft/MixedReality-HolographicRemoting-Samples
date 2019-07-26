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

#define TEXTURE_WIDTH 650
#define TEXTURE_HEIGHT 650

#define Font L"Segoe UI"
#define FontSizeLarge 32.0f
#define FontSizeSmall 22.0f
#define FontLanguage L"en-US"

using namespace DirectX;
using namespace Concurrency;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI::Input::Spatial;

// Initializes D2D resources used for text rendering.
StatusDisplay::StatusDisplay(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources)
    : m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();
}

// Called once per frame. Rotates the quad, and calculates and sets the model matrix
// relative to the position transform indicated by hologramPositionTransform.
void StatusDisplay::Update(float deltaTimeInSeconds)
{
    UpdateConstantBuffer(deltaTimeInSeconds, m_modelConstantBufferDataImage, m_positionImage, m_lastPositionImage);
    UpdateConstantBuffer(deltaTimeInSeconds, m_modelConstantBufferDataText, m_positionText, m_lastPositionText);
}

// Renders a frame to the screen.
void StatusDisplay::Render()
{
    // Loading is asynchronous. Resources must be created before drawing can occur.
    if (!m_loadingComplete)
    {
        return;
    }

    // First render all text using direct2D
    m_deviceResources->UseD3DDeviceContext(
        [&](auto context) { context->ClearRenderTargetView(m_textRenderTarget.get(), DirectX::Colors::Transparent); });

    m_d2dTextRenderTarget->BeginDraw();

    {
        std::scoped_lock lock(m_lineMutex);
        if (m_lines.size() > 0)
        {
            float top = m_lines[0].metrics.height;

            for (auto& line : m_lines)
            {
                if (line.alignBottom)
                {
                    top = TEXTURE_HEIGHT - line.metrics.height;
                }
                m_d2dTextRenderTarget->DrawTextLayout(D2D1::Point2F(0, top), line.layout.get(), m_brushes[line.color].get());
                top += line.metrics.height * line.lineHeightMultiplier;
            }
        }
    }

    // Ignore D2DERR_RECREATE_TARGET here. This error indicates that the device
    // is lost. It will be handled during the next call to Present.
    const HRESULT hr = m_d2dTextRenderTarget->EndDraw();
    if (hr != D2DERR_RECREATE_TARGET)
    {
        winrt::check_hresult(hr);
    }

    // Now render the quads into 3d space
    m_deviceResources->UseD3DDeviceContext([&](auto context) {
        // Each vertex is one instance of the VertexBufferElement struct.
        const UINT stride = sizeof(VertexBufferElement);
        const UINT offset = 0;
        ID3D11Buffer* pBufferToSet = m_vertexBufferImage.get();
        context->IASetVertexBuffers(0, 1, &pBufferToSet, &stride, &offset);
        context->IASetIndexBuffer(
            m_indexBuffer.get(),
            DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
            0);

        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(m_inputLayout.get());
        context->OMSetBlendState(m_textAlphaBlendState.get(), nullptr, 0xffffffff);

        // Attach the vertex shader.
        context->VSSetShader(m_vertexShader.get(), nullptr, 0);

        context->UpdateSubresource(m_modelConstantBuffer.get(), 0, nullptr, &m_modelConstantBufferDataImage, 0, 0);

        // Apply the model constant buffer to the vertex shader.
        pBufferToSet = m_modelConstantBuffer.get();
        context->VSSetConstantBuffers(0, 1, &pBufferToSet);

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

        // Set up for rendering the texture that contains the text
        pBufferToSet = m_vertexBufferText.get();
        context->IASetVertexBuffers(0, 1, &pBufferToSet, &stride, &offset);

        ID3D11ShaderResourceView* pShaderViewToSet = m_textShaderResourceView.get();
        context->PSSetShaderResources(0, 1, &pShaderViewToSet);

        ID3D11SamplerState* pSamplerToSet = m_textSamplerState.get();
        context->PSSetSamplers(0, 1, &pSamplerToSet);

        context->UpdateSubresource(m_modelConstantBuffer.get(), 0, nullptr, &m_modelConstantBufferDataText, 0, 0);

        // Draw the text.
        context->DrawIndexedInstanced(
            m_indexCount, // Index count per instance.
            2,            // Instance count.
            0,            // Start index location.
            0,            // Base vertex location.
            0             // Start instance location.
        );

        // Reset the blend state.
        context->OMSetBlendState(nullptr, nullptr, 0xffffffff);

        // Detach our texture.
        ID3D11ShaderResourceView* emptyResource = nullptr;
        context->PSSetShaderResources(0, 1, &emptyResource);
    });
}

void StatusDisplay::CreateDeviceDependentResources()
{
    CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);

    CD3D11_TEXTURE2D_DESC textureDesc(
        DXGI_FORMAT_B8G8R8A8_UNORM, TEXTURE_WIDTH, TEXTURE_HEIGHT, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

    m_textTexture = nullptr;
    m_deviceResources->GetD3DDevice()->CreateTexture2D(&textureDesc, nullptr, m_textTexture.put());

    m_textShaderResourceView = nullptr;
    m_deviceResources->GetD3DDevice()->CreateShaderResourceView(m_textTexture.get(), nullptr, m_textShaderResourceView.put());

    m_textRenderTarget = nullptr;
    m_deviceResources->GetD3DDevice()->CreateRenderTargetView(m_textTexture.get(), nullptr, m_textRenderTarget.put());

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
    task<void> createVSTask = task<void>([this, vertexShaderData, vertexShaderDataSize]() {
        winrt::check_hresult(
            m_deviceResources->GetD3DDevice()->CreateVertexShader(vertexShaderData, vertexShaderDataSize, nullptr, m_vertexShader.put()));

        static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateInputLayout(
            vertexDesc, ARRAYSIZE(vertexDesc), vertexShaderData, vertexShaderDataSize, m_inputLayout.put()));
    });

    // create the pixel shader and constant buffer.
    task<void> createPSTask([this]() {
        winrt::check_hresult(
            m_deviceResources->GetD3DDevice()->CreatePixelShader(PixelShader, sizeof(PixelShader), nullptr, m_pixelShader.put()));

        const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, m_modelConstantBuffer.put()));
    });

    task<void> createGSTask;
    if (!m_usingVprtShaders)
    {
        // create the geometry shader.
        createGSTask = task<void>([this]() {
            winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateGeometryShader(
                GeometryShader, sizeof(GeometryShader), nullptr, m_geometryShader.put()));
        });
    }

    // Once all shaders are loaded, create the mesh.
    task<void> shaderTaskGroup = m_usingVprtShaders ? (createPSTask && createVSTask) : (createPSTask && createVSTask && createGSTask);
    task<void> createQuadTask = shaderTaskGroup.then([this]() {
        // Load mesh vertices. Each vertex has a position and a color.
        // Note that the quad size has changed from the default DirectX app
        // template. Windows Holographic is scaled in meters, so to draw the
        // quad at a comfortable size we made the quad width 0.2 m (20 cm).
        static const float imageQuadExtent = 0.23f;
        static const VertexBufferElement quadVertices[] = {
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
        winrt::check_hresult(
            m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, m_vertexBufferImage.put()));

        static const float textQuadExtent = 0.3f;
        static const VertexBufferElement quadVerticesText[] = {
            {XMFLOAT3(-textQuadExtent, textQuadExtent, 0.f), XMFLOAT2(0.f, 0.f)},
            {XMFLOAT3(textQuadExtent, textQuadExtent, 0.f), XMFLOAT2(1.f, 0.f)},
            {XMFLOAT3(textQuadExtent, -textQuadExtent, 0.f), XMFLOAT2(1.f, 1.f)},
            {XMFLOAT3(-textQuadExtent, -textQuadExtent, 0.f), XMFLOAT2(0.f, 1.f)},
        };

        D3D11_SUBRESOURCE_DATA vertexBufferDataText = {0};
        vertexBufferDataText.pSysMem = quadVerticesText;
        vertexBufferDataText.SysMemPitch = 0;
        vertexBufferDataText.SysMemSlicePitch = 0;
        const CD3D11_BUFFER_DESC vertexBufferDescText(sizeof(quadVerticesText), D3D11_BIND_VERTEX_BUFFER);
        winrt::check_hresult(
            m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDescText, &vertexBufferDataText, m_vertexBufferText.put()));


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
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, m_indexBuffer.put()));
    });

    // Create image sampler state
    {
        D3D11_SAMPLER_DESC desc = {};

        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MaxAnisotropy = 1;
        desc.MinLOD = 0;
        desc.MaxLOD = 3;
        desc.MipLODBias = 0.f;
        desc.BorderColor[0] = 0.f;
        desc.BorderColor[1] = 0.f;
        desc.BorderColor[2] = 0.f;
        desc.BorderColor[3] = 0.f;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateSamplerState(&desc, m_imageSamplerState.put()));
    }

    // Create text sampler state
    {
        CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateSamplerState(&samplerDesc, m_textSamplerState.put()));
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

    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBlendState(&blendStateDesc, m_textAlphaBlendState.put()));

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
        UpdateLineInternal(m_lines[i], lines[i]);
    }
}

void StatusDisplay::UpdateLineText(size_t index, std::wstring text)
{
    std::scoped_lock lock(m_lineMutex);
    assert(index < m_lines.size() && "Line index out of bounds");

    auto& runtimeLine = m_lines[index];

    Line line = {std::move(text), runtimeLine.format, runtimeLine.color, runtimeLine.lineHeightMultiplier};
    UpdateLineInternal(runtimeLine, line);
}

size_t StatusDisplay::AddLine(const Line& line)
{
    std::scoped_lock lock(m_lineMutex);
    auto newIndex = m_lines.size();
    m_lines.resize(newIndex + 1);
    UpdateLineInternal(m_lines[newIndex], line);
    return newIndex;
}

bool StatusDisplay::HasLine(size_t index)
{
    std::scoped_lock lock(m_lineMutex);
    return index < m_lines.size();
}

void StatusDisplay::CreateFonts()
{
    // Create Large font
    m_textFormats[Large] = nullptr;
    winrt::check_hresult(m_deviceResources->GetDWriteFactory()->CreateTextFormat(
        Font,
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        FontSizeLarge,
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
        FontSizeLarge,
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
        FontSizeSmall,
        FontLanguage,
        m_textFormats[Small].put()));
    winrt::check_hresult(m_textFormats[Small]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    winrt::check_hresult(m_textFormats[Small]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));

    static_assert(TextFormatCount == 3, "Expected 3 text formats");
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

        runtimeLine.layout = nullptr;
        winrt::check_hresult(m_deviceResources->GetDWriteFactory()->CreateTextLayout(
            line.text.c_str(),
            static_cast<UINT32>(line.text.length()),
            m_textFormats[line.format].get(),
            TEXTURE_WIDTH,  // Max width of the input text.
            TEXTURE_HEIGHT, // Max height of the input text.
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
void StatusDisplay::PositionDisplay(float deltaTimeInSeconds, const SpatialPointerPose& pointerPose)
{
    if (pointerPose != nullptr)
    {
        // Get the gaze direction relative to the given coordinate system.
        const float3 headPosition = pointerPose.Head().Position();
        const float3 headDirection = pointerPose.Head().ForwardDirection();

        const float3 offsetImage = float3(0.0f, -0.02f, 0.0f);
        const float3 gazeAtTwoMetersImage = headPosition + (2.05f * (headDirection + offsetImage));

        const float3 offsetText = float3(0.0f, -0.035f, 0.0f);
        const float3 gazeAtTwoMetersText = headPosition + (2.0f * (headDirection + offsetText));

        // Lerp the position, to keep the hologram comfortably stable.
        auto imagePosition = lerp(m_positionImage, gazeAtTwoMetersImage, deltaTimeInSeconds * c_lerpRate);
        auto textPosition = lerp(m_positionText, gazeAtTwoMetersText, deltaTimeInSeconds * c_lerpRate);

        m_lastPositionImage = m_positionImage;
        m_positionImage = imagePosition;

        m_lastPositionText = m_positionText;
        m_positionText = textPosition;
    }
}

void StatusDisplay::UpdateConstantBuffer(
    float deltaTimeInSeconds,
    ModelConstantBuffer& buffer,
    winrt::Windows::Foundation::Numerics::float3 position,
    winrt::Windows::Foundation::Numerics::float3 lastPosition)
{
    // Create a direction normal from the hologram's position to the origin of person space.
    // This is the z-axis rotation.
    XMVECTOR facingNormal = XMVector3Normalize(-XMLoadFloat3(&position));

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

    // Determine velocity.
    // Even though the motion is spherical, the velocity is still linear
    // for image stabilization.
    auto& deltaX = position - lastPosition;   // meters
    m_velocity = deltaX / deltaTimeInSeconds; // meters per second
}
