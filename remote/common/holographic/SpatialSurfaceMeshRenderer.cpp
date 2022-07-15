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

#include <holographic/SpatialSurfaceMeshRenderer.h>

#include <DirectXHelper.h>

using namespace winrt::Windows;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Foundation::Numerics;
using namespace Concurrency;

// for debugging -> remove
bool g_freeze = false;
bool g_freezeOnFrame = false;

// Initializes D2D resources used for text rendering.
SpatialSurfaceMeshRenderer::SpatialSurfaceMeshRenderer(const std::shared_ptr<DXHelper::DeviceResourcesD3D11>& deviceResources)
    : m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();

    m_spatialLocator = SpatialLocator::GetDefault();
    if (m_spatialLocator)
    {
        m_spatialLocatorLocabilityChangedEventRevoker =
            m_spatialLocator.LocatabilityChanged(winrt::auto_revoke, {this, &SpatialSurfaceMeshRenderer::OnLocatibilityChanged});

        m_attachedFrameOfReference = m_spatialLocator.CreateAttachedFrameOfReferenceAtCurrentHeading();
    }
}

SpatialSurfaceMeshRenderer::~SpatialSurfaceMeshRenderer()
{
}

void SpatialSurfaceMeshRenderer::CreateDeviceDependentResources()
{
    auto asyncAccess = Surfaces::SpatialSurfaceObserver::RequestAccessAsync();
    asyncAccess.Completed([this](auto handler, auto asyncStatus) {
        m_surfaceObserver = Surfaces::SpatialSurfaceObserver();

        if (m_surfaceObserver)
        {
            m_observedSurfaceChangedToken = m_surfaceObserver.ObservedSurfacesChanged(
                [this](Surfaces::SpatialSurfaceObserver, winrt::Windows::Foundation::IInspectable const& handler) {
                    OnObservedSurfaceChanged();
                });
        }
    });

    std::vector<byte> vertexShaderFileData = DXHelper::ReadFromFile(L"SRMesh_VertexShader.cso");
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateVertexShader(
        vertexShaderFileData.data(), vertexShaderFileData.size(), nullptr, m_vertexShader.put()));

    constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 1> vertexDesc = {{
        {"POSITION", 0, DXGI_FORMAT_R16G16B16A16_SNORM, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    }};

    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateInputLayout(
        vertexDesc.data(),
        static_cast<UINT>(vertexDesc.size()),
        vertexShaderFileData.data(),
        static_cast<UINT>(vertexShaderFileData.size()),
        m_inputLayout.put()));

    std::vector<byte> geometryShaderFileData = DXHelper::ReadFromFile(L"SRMesh_GeometryShader.cso");

    // After the pass-through geometry shader file is loaded, create the shader.
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateGeometryShader(
        geometryShaderFileData.data(), geometryShaderFileData.size(), nullptr, m_geometryShader.put()));

    std::vector<byte> pixelShaderFileData = DXHelper::ReadFromFile(L"SRMesh_PixelShader.cso");
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreatePixelShader(
        pixelShaderFileData.data(), pixelShaderFileData.size(), nullptr, m_pixelShader.put()));

    const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(SRMeshConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
    winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, m_modelConstantBuffer.put()));

    m_loadingComplete = true;
}

void SpatialSurfaceMeshRenderer::ReleaseDeviceDependentResources()
{
    if (m_surfaceObserver)
    {
        m_surfaceObserver.ObservedSurfacesChanged(m_observedSurfaceChangedToken);
        m_surfaceObserver = nullptr;
    }
    m_loadingComplete = false;
    m_inputLayout = nullptr;
    m_vertexShader = nullptr;
    m_geometryShader = nullptr;
    m_pixelShader = nullptr;
    m_modelConstantBuffer = nullptr;
}

void SpatialSurfaceMeshRenderer::OnObservedSurfaceChanged()
{
    if (g_freeze)
        return;

    m_surfaceChangedCounter++; // just for debugging purposes
    m_sufaceChanged = true;
}

void SpatialSurfaceMeshRenderer::OnLocatibilityChanged(
    const SpatialLocator& spatialLocator, const winrt::Windows::Foundation::IInspectable&)
{
    SpatialLocatability locatibility = spatialLocator.Locatability();
    if (locatibility != SpatialLocatability::PositionalTrackingActive)
    {
        m_meshParts.clear();
    }
}

SpatialSurfaceMeshPart* SpatialSurfaceMeshRenderer::GetOrCreateMeshPart(winrt::guid id)
{
    GUID key = id;
    auto found = m_meshParts.find(key);
    if (found == m_meshParts.cend())
    {
        m_meshParts[id] = std::make_unique<SpatialSurfaceMeshPart>(this);
        return m_meshParts[id].get();
    }

    return found->second.get();
}

void SpatialSurfaceMeshRenderer::Update(
    winrt::Windows::Perception::PerceptionTimestamp timestamp,
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem)
{
    if (m_surfaceObserver == nullptr)
        return;

    // update bounding volume (every frame)
    {
        SpatialBoundingBox axisAlignedBoundingBox = {
            {-5.0f, -5.0f, -2.5f},
            {10.0f, 10.0f, 5.f},
        };

        using namespace std::chrono_literals;
        auto now = std::chrono::high_resolution_clock::now();
        auto delta = now - m_boundingVolumeUpdateTime;

        if (m_attachedFrameOfReference && delta > 1s)
        {
            SpatialCoordinateSystem attachedCoordinateSystem =
                m_attachedFrameOfReference.GetStationaryCoordinateSystemAtTimestamp(timestamp);
            SpatialBoundingVolume volume = SpatialBoundingVolume::FromBox(attachedCoordinateSystem, axisAlignedBoundingBox);
            m_surfaceObserver.SetBoundingVolume(volume);

            m_boundingVolumeUpdateTime = now;
        }
    }

    if (m_sufaceChanged)
    {
        // first mark all as not used
        for (auto& pair : m_meshParts)
        {
            pair.second->m_inUse = false;
        }

        auto mapContainingSurfaceCollection = m_surfaceObserver.GetObservedSurfaces();

        for (auto pair : mapContainingSurfaceCollection)
        {
            if (SpatialSurfaceMeshPart* meshPart = GetOrCreateMeshPart(pair.Key()))
            {
                meshPart->Update(pair.Value());
                g_freeze = g_freezeOnFrame;
            }
        }

        // purge the ones not used
        for (MeshPartMap::const_iterator itr = m_meshParts.cbegin(); itr != m_meshParts.cend();)
        {
            itr = itr->second->IsInUse() ? std::next(itr) : m_meshParts.erase(itr);
        }

        m_sufaceChanged = false;
    }

    // every frame, bring the model matrix to rendering space
    for (auto& pair : m_meshParts)
    {
        pair.second->UpdateModelMatrix(renderingCoordinateSystem);
    }
}

void SpatialSurfaceMeshRenderer::Render(bool isStereo)
{
    if (!m_loadingComplete || m_meshParts.empty())
        return;

    m_deviceResources->UseD3DDeviceContext([&](auto context) {
        // Each vertex is one instance of the VertexPositionColorTexture struct.
        const uint32_t stride = sizeof(SpatialSurfaceMeshPart::Vertex_t);
        const uint32_t offset = 0;
        ID3D11Buffer* pBufferToSet;
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(m_inputLayout.get());

        // Attach the vertex shader.
        context->VSSetShader(m_vertexShader.get(), nullptr, 0);
        // Apply the model constant buffer to the vertex shader.
        pBufferToSet = m_modelConstantBuffer.get();
        context->VSSetConstantBuffers(0, 1, &pBufferToSet);

        // geometry shader
        context->GSSetShader(m_geometryShader.get(), nullptr, 0);

        // pixel shader
        context->PSSetShader(m_zfillOnly ? nullptr : m_pixelShader.get(), nullptr, 0);

        // Apply the model constant buffer to the pixel shader.
        pBufferToSet = m_modelConstantBuffer.get();
        context->PSSetConstantBuffers(0, 1, &pBufferToSet);

        // render each mesh part
        for (auto& pair : m_meshParts)
        {
            SpatialSurfaceMeshPart* part = pair.second.get();
            if (part->m_indexCount == 0)
                continue;

            if (part->m_needsUpload)
            {
                part->UploadData();
            }

            // update part specific model matrix
            context->UpdateSubresource(m_modelConstantBuffer.get(), 0, nullptr, &part->m_constantBufferData, 0, 0);

            ID3D11Buffer* pBufferToSet2 = part->m_vertexBuffer.get();
            context->IASetVertexBuffers(0, 1, &pBufferToSet2, &stride, &offset);
            context->IASetIndexBuffer(part->m_indexBuffer.get(), DXGI_FORMAT_R16_UINT, 0);
            // draw the mesh
            context->DrawIndexedInstanced(part->m_indexCount, isStereo ? 2 : 1, 0, 0, 0);
        }

        // set geometry shader back
        context->GSSetShader(nullptr, nullptr, 0);
    });
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// SRMeshPart
//////////////////////////////////////////////////////////////////////////////////////////////////////////

SpatialSurfaceMeshPart::SpatialSurfaceMeshPart(SpatialSurfaceMeshRenderer* owner)
    : m_owner(owner)
{
    auto identity = DirectX::XMMatrixIdentity();
    m_constantBufferData.modelMatrix = reinterpret_cast<DirectX::XMFLOAT4X4&>(identity);
    m_vertexScale.x = m_vertexScale.y = m_vertexScale.z = 1.0f;
}

void SpatialSurfaceMeshPart::Update(Surfaces::SpatialSurfaceInfo surfaceInfo)
{
    m_inUse = true;
    m_updateInProgress = true;
    double TriangleDensity = 750.0; // from Hydrogen
    auto asyncOpertation = surfaceInfo.TryComputeLatestMeshAsync(TriangleDensity);
    asyncOpertation.Completed([this](winrt::Windows::Foundation::IAsyncOperation<Surfaces::SpatialSurfaceMesh> result, auto asyncStatus) {
        Surfaces::SpatialSurfaceMesh mesh = result.GetResults();
        UpdateMesh(mesh);
        m_updateInProgress = false;
    });
}

void SpatialSurfaceMeshPart::UpdateModelMatrix(winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem)
{
    if (m_coordinateSystem == nullptr)
        return;

    auto modelTransform = m_coordinateSystem.TryGetTransformTo(renderingCoordinateSystem);
    if (modelTransform)
    {
        float4x4 matrixWinRt = transpose(modelTransform.Value());
        DirectX::XMMATRIX transformMatrix = DirectX::XMLoadFloat4x4(&matrixWinRt);
        DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(m_vertexScale.x, m_vertexScale.y, m_vertexScale.z);
        DirectX::XMMATRIX result = DirectX::XMMatrixMultiply(transformMatrix, scaleMatrix);
        DirectX::XMStoreFloat4x4(&m_constantBufferData.modelMatrix, result);
    }
}

void SpatialSurfaceMeshPart::UpdateMesh(Surfaces::SpatialSurfaceMesh mesh)
{
    m_coordinateSystem = mesh.CoordinateSystem();

    Surfaces::SpatialSurfaceMeshBuffer vertexBuffer = mesh.VertexPositions();
    Surfaces::SpatialSurfaceMeshBuffer indexBuffer = mesh.TriangleIndices();

    DirectXPixelFormat vertexFormat = vertexBuffer.Format();
    DirectXPixelFormat indexFormat = indexBuffer.Format();

    assert(vertexFormat == DirectXPixelFormat::R16G16B16A16IntNormalized);
    assert(indexFormat == DirectXPixelFormat::R16UInt);

    uint32_t vertexCount = vertexBuffer.ElementCount();
    uint32_t indexCount = indexBuffer.ElementCount();
    assert((indexCount % 3) == 0);

    if (vertexCount == 0 || indexCount == 0)
    {
        m_indexCount = 0;
        return;
    }

    // convert vertices:
    {
        Vertex_t* dest = MapVertices(vertexCount);
        winrt::Windows::Storage::Streams::IBuffer vertexData = vertexBuffer.Data();
        uint8_t* vertexRaw = vertexData.data();
        int vertexStride = vertexData.Length() / vertexCount;
        assert(vertexStride == 8); // DirectXPixelFormat::R16G16B16A16IntNormalized
        winrt::Windows::Foundation::Numerics::float3 positionScale = mesh.VertexPositionScale();
        m_vertexScale.x = positionScale.x;
        m_vertexScale.y = positionScale.y;
        m_vertexScale.z = positionScale.z;
        memcpy(dest, vertexRaw, vertexCount * sizeof(Vertex_t));
        UnmapVertices();
    }

    // convert indices
    {
        uint16_t* dest = MapIndices(indexCount);
        winrt::Windows::Storage::Streams::IBuffer indexData = indexBuffer.Data();
        uint16_t* source = (uint16_t*)indexData.data();
        for (uint32_t i = 0; i < indexCount; i++)
        {
            assert(source[i] < vertexCount);
            dest[i] = source[i];
        }
        UnmapIndices();
    }
    m_needsUpload = true;
}

SpatialSurfaceMeshPart::Vertex_t* SpatialSurfaceMeshPart::MapVertices(uint32_t vertexCount)
{
    m_vertexCount = vertexCount;
    if (vertexCount > m_vertexData.size())
        m_vertexData.resize(vertexCount);
    return &m_vertexData[0];
}

void SpatialSurfaceMeshPart::UnmapVertices()
{
}

uint16_t* SpatialSurfaceMeshPart::MapIndices(uint32_t indexCount)
{
    m_indexCount = indexCount;
    if (indexCount > m_indexData.size())
        m_indexData.resize(indexCount);
    return &m_indexData[0];
}

void SpatialSurfaceMeshPart::UnmapIndices()
{
}

void SpatialSurfaceMeshPart::UploadData()
{
    if (m_vertexCount > m_allocatedVertexCount)
    {
        m_vertexBuffer = nullptr;

        const int alignment = 1024;
        m_allocatedVertexCount = ((m_vertexCount + alignment - 1) / alignment) * alignment; // align to reasonable size

        const int elementSize = sizeof(Vertex_t);
        const CD3D11_BUFFER_DESC vertexBufferDesc(
            m_allocatedVertexCount * elementSize, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

        winrt::check_hresult(m_owner->m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, nullptr, m_vertexBuffer.put()));
    }

    if (m_indexCount > m_allocatedIndexCount)
    {
        m_indexBuffer = nullptr;
        const int alignment = 3 * 1024;
        m_allocatedIndexCount = ((m_indexCount + alignment - 1) / alignment) * alignment; // align to reasonable size

        const int elementSize = sizeof(uint16_t);
        const CD3D11_BUFFER_DESC indexBufferDesc(
            m_allocatedIndexCount * elementSize, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

        winrt::check_hresult(m_owner->m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, nullptr, m_indexBuffer.put()));
    }

    // upload data
    D3D11_MAPPED_SUBRESOURCE resource;

    m_owner->m_deviceResources->UseD3DDeviceContext([&](auto context) {
        context->Map(m_vertexBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
        memcpy(resource.pData, &m_vertexData[0], sizeof(Vertex_t) * m_vertexCount);
        context->Unmap(m_vertexBuffer.get(), 0);

        context->Map(m_indexBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
        memcpy(resource.pData, &m_indexData[0], sizeof(uint16_t) * m_indexCount);
        context->Unmap(m_indexBuffer.get(), 0);
    });
}
