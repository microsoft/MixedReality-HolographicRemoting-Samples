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

#include "../Common/DirectXHelper.h"
#include "SpinningCubeRenderer.h"

#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Perception.People.h>
#include <winrt/Windows.Perception.h>

using namespace DirectX;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
SpinningCubeRenderer::SpinningCubeRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources)
    : m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();
}

// This function uses a SpatialPointerPose to position the world-locked hologram
// two meters in front of the user's heading.
void SpinningCubeRenderer::PositionHologram(const winrt::Windows::UI::Input::Spatial::SpatialPointerPose& pointerPose)
{
    if (pointerPose != nullptr)
    {
        // Try to get the gaze from eyes.
        winrt::Windows::Perception::People::EyesPose eyesPose = pointerPose.Eyes();
        if (eyesPose != nullptr)
        {
            winrt::Windows::Foundation::IReference<winrt::Windows::Perception::Spatial::SpatialRay> gaze = eyesPose.Gaze();
            if (gaze != nullptr)
            {
                PositionHologram(gaze.Value().Origin, gaze.Value().Direction);
                return;
            }
        }

        // Get the gaze direction from head.
        const auto headPosition = pointerPose.Head().Position();
        const auto headDirection = pointerPose.Head().ForwardDirection();

        PositionHologram(headPosition, headDirection);
    }
}

void SpinningCubeRenderer::SetColorFilter(DirectX::XMFLOAT4 color)
{
    m_filterColorData = color;
}

// This function uses a point and a vector to position the world-locked hologram
// two meters in front of the user's heading.
void SpinningCubeRenderer::PositionHologram(
    winrt::Windows::Foundation::Numerics::float3 headPosition, winrt::Windows::Foundation::Numerics::float3 headDirection)
{
    // The hologram is positioned two meters along the user's gaze direction.
    static const float distanceFromUser = 2.0f; // meters
    const auto gazeAtTwoMeters = headPosition + (distanceFromUser * headDirection);

    // This will be used as the translation component of the hologram's
    // model transform.
    SetPosition(gazeAtTwoMeters);
}

// Called once per frame. Rotates the cube, and calculates and sets the model matrix
// relative to the position transform indicated by hologramPositionTransform.
void SpinningCubeRenderer::Update(float totalSeconds)
{
    // Rotate the cube.
    // Convert degrees to radians, then convert seconds to rotation angle.
    const float radiansPerSecond = XMConvertToRadians(m_degreesPerSecond);
    const double relativeRotation = totalSeconds * radiansPerSecond;
    double totalRotation = m_rotationOffset;
    switch (m_pauseState)
    {
        case PauseState::Unpaused:
            totalRotation += relativeRotation;
            break;

        case PauseState::Pausing:
            m_rotationOffset += relativeRotation;
            m_pauseState = PauseState::Paused;
        case PauseState::Paused:
            totalRotation = m_rotationOffset;
            break;

        case PauseState::Unpausing:
            m_rotationOffset -= relativeRotation;
            m_pauseState = PauseState::Unpaused;
            break;
    }
    const float radians = static_cast<float>(fmod(totalRotation, XM_2PI));
    const XMMATRIX modelRotation = XMMatrixRotationY(-radians);

    // Position the cube.
    const XMMATRIX modelTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_position));

    // Multiply to get the transform matrix.
    // Note that this transform does not enforce a particular coordinate system. The calling
    // class is responsible for rendering this content in a consistent manner.
    const XMMATRIX modelTransform = XMMatrixMultiply(modelRotation, modelTranslation);

    // Store the normal transform.
    XMStoreFloat4x4(&m_modelConstantBufferData.normal, XMMatrixTranspose(modelRotation));

    // The view and projection matrices are provided by the system; they are associated
    // with holographic cameras, and updated on a per-camera basis.
    // Here, we provide the model transform for the sample hologram. The model transform
    // matrix is transposed to prepare it for the shader.
    XMStoreFloat4x4(&m_modelConstantBufferData.model, XMMatrixTranspose(modelTransform));

    // Loading is asynchronous. Resources must be created before they can be updated.
    if (!m_loadingComplete)
    {
        return;
    }

    // Use the D3D device context to update Direct3D device-based resources.
    m_deviceResources->UseD3DDeviceContext([&](auto context) {
        // Update the model transform buffer for the hologram.
        context->UpdateSubresource(m_modelConstantBuffer.get(), 0, nullptr, &m_modelConstantBufferData, 0, 0);
    });
}

// Renders one frame using the vertex and pixel shaders.
// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
// a pass-through geometry shader is also used to set the render
// target array index.
void SpinningCubeRenderer::Render(bool isStereo)
{
    // Loading is asynchronous. Resources must be created before drawing can occur.
    if (!m_loadingComplete)
    {
        return;
    }

    m_deviceResources->UseD3DDeviceContext([&](auto context) {
        ID3D11Buffer* pBufferToSet = nullptr;

        // Each vertex is one instance of the VertexPositionColor struct.
        const UINT stride = sizeof(VertexPositionNormalColor);
        const UINT offset = 0;

        pBufferToSet = m_vertexBuffer.get();
        context->IASetVertexBuffers(0, 1, &pBufferToSet, &stride, &offset);
        context->IASetIndexBuffer(
            m_indexBuffer.get(),
            DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
            0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(m_inputLayout.get());

        // Attach the vertex shader.
        context->VSSetShader(m_vertexShader.get(), nullptr, 0);
        // Apply the model constant buffer to the vertex shader.
        pBufferToSet = m_modelConstantBuffer.get();
        context->VSSetConstantBuffers(0, 1, &pBufferToSet);

        if (!m_usingVprtShaders)
        {
            // On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
            // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
            // a pass-through geometry shader is used to set the render target
            // array index.
            context->GSSetShader(m_geometryShader.get(), nullptr, 0);
        }

        context->UpdateSubresource(m_filterColorBuffer.get(), 0, nullptr, &m_filterColorData, 0, 0);

        pBufferToSet = m_filterColorBuffer.get();
        context->PSSetConstantBuffers(2, 1, &pBufferToSet);

        // Attach the pixel shader.
        context->PSSetShader(m_pixelShader.get(), nullptr, 0);

        // Draw the objects.
        context->DrawIndexedInstanced(
            m_indexCount,     // Index count per instance.
            isStereo ? 2 : 1, // Instance count.
            0,                // Start index location.
            0,                // Base vertex location.
            0                 // Start instance location.
        );
    });
}

std::future<void> SpinningCubeRenderer::CreateDeviceDependentResources()
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

    const CD3D11_BUFFER_DESC filterColorBufferDesc(sizeof(XMFLOAT4), D3D11_BIND_CONSTANT_BUFFER);
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&filterColorBufferDesc, nullptr, m_filterColorBuffer.put()));

    if (!m_usingVprtShaders)
    {
        // Load the pass-through geometry shader.
        std::vector<byte> geometryShaderFileData = co_await DXHelper::ReadDataAsync(fileNamePrefix + L"hsa_GeometryShader.cso");

        // After the pass-through geometry shader file is loaded, create the shader.
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateGeometryShader(
            geometryShaderFileData.data(), geometryShaderFileData.size(), nullptr, m_geometryShader.put()));
    }

    // Load mesh vertices. Each vertex has a position and a color.
    // Note that the cube size has changed from the default DirectX app
    // template. Windows Holographic is scaled in meters, so to draw the
    // cube at a comfortable size we made the cube width 0.2 m (20 cm).
    static const VertexPositionNormalColor cubeVertices[] = {
        {XMFLOAT3(-0.1f, -0.1f, -0.1f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f)}, // vertex 0 non-debug
        {XMFLOAT3(-0.1f, -0.1f, 0.1f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
        {XMFLOAT3(-0.1f, 0.1f, -0.1f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
        {XMFLOAT3(-0.1f, 0.1f, 0.1f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 1.0f)},
        {XMFLOAT3(0.1f, -0.1f, -0.1f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
        {XMFLOAT3(0.1f, -0.1f, 0.1f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 1.0f)},
        {XMFLOAT3(0.1f, 0.1f, -0.1f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 0.0f)},
        {XMFLOAT3(0.1f, 0.1f, 0.1f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f)},
    };

    D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
    vertexBufferData.pSysMem = cubeVertices;
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(cubeVertices), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, m_vertexBuffer.put()));

    // Load mesh indices. Each trio of indices represents
    // a triangle to be rendered on the screen.
    // For example: 2,1,0 means that the vertices with indexes
    // 2, 1, and 0 from the vertex buffer compose the
    // first triangle of this mesh.
    // Note that the winding order is clockwise by default.
    static const unsigned short cubeIndices[] = {
        2, 1, 0, // -x
        2, 3, 1,

        6, 4, 5, // +x
        6, 5, 7,

        0, 1, 5, // -y
        0, 5, 4,

        2, 6, 7, // +y
        2, 7, 3,

        0, 4, 6, // -z
        0, 6, 2,

        1, 3, 7, // +z
        1, 7, 5,
    };

    m_indexCount = ARRAYSIZE(cubeIndices);

    D3D11_SUBRESOURCE_DATA indexBufferData = {0};
    indexBufferData.pSysMem = cubeIndices;
    indexBufferData.SysMemPitch = 0;
    indexBufferData.SysMemSlicePitch = 0;
    const CD3D11_BUFFER_DESC indexBufferDesc(sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER);
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, m_indexBuffer.put()));

    // Once the cube is loaded, the object is ready to be rendered.
    m_loadingComplete = true;
}

void SpinningCubeRenderer::ReleaseDeviceDependentResources()
{
    m_loadingComplete = false;
    m_usingVprtShaders = false;
    m_vertexShader = nullptr;
    m_inputLayout = nullptr;
    m_pixelShader = nullptr;
    m_geometryShader = nullptr;
    m_modelConstantBuffer = nullptr;
    m_vertexBuffer = nullptr;
    m_indexBuffer = nullptr;
    m_filterColorBuffer = nullptr;
}

void SpinningCubeRenderer::CreateWindowSizeDependentResources()
{
}

void SpinningCubeRenderer::TogglePauseState()
{
    if (m_pauseState == PauseState::Paused)
    {
        m_pauseState = PauseState::Unpausing;
    }
    else
    {
        m_pauseState = PauseState::Pausing;
    }
}
