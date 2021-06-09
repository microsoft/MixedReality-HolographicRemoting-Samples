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

#include <Utils.h>
#include <holographic/DeviceResources.h>

#include <winrt/windows.perception.spatial.surfaces.h>

#include <future>
#include <string>

// forward
class SpatialSurfaceMeshRenderer;

struct SRMeshConstantBuffer
{
    DirectX::XMFLOAT4X4 modelMatrix;
};

// Assert that the constant buffer remains 16-byte aligned (best practice).
static_assert(
    (sizeof(SRMeshConstantBuffer) % (sizeof(float) * 4)) == 0,
    "SR mesh constant buffer size must be 16-byte aligned (16 bytes is the length of four floats).");

// represents a single piece of mesh (SpatialSurfaceMesh)
class SpatialSurfaceMeshPart
{
public:
    struct Vertex_t
    {
        // float pos[4];
        int16_t pos[4];
    };
    SpatialSurfaceMeshPart(SpatialSurfaceMeshRenderer* owner);
    void Update(winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceInfo surfaceInfo);

    void UpdateMesh(winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceMesh mesh);

    bool IsInUse() const
    {
        return m_inUse || m_updateInProgress;
    }

private:
    Vertex_t* MapVertices(uint32_t vertexCount);
    void UnmapVertices();
    uint16_t* MapIndices(uint32_t indexCount);
    void UnmapIndices();
    void UploadData();
    void UpdateModelMatrix(winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem);

    friend class SpatialSurfaceMeshRenderer;
    SpatialSurfaceMeshRenderer* m_owner;
    bool m_inUse = true;
    bool m_needsUpload = false;
    bool m_updateInProgress = false;

    GUID m_ID;
    uint32_t m_allocatedVertexCount = 0;
    uint32_t m_allocatedIndexCount = 0;
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
    winrt::com_ptr<ID3D11Buffer> m_vertexBuffer;
    winrt::com_ptr<ID3D11Buffer> m_indexBuffer;

    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_coordinateSystem = nullptr;

    // double buffered data:
    std::vector<Vertex_t> m_vertexData;
    std::vector<uint16_t> m_indexData;
    SRMeshConstantBuffer m_constantBufferData;
    DirectX::XMFLOAT3 m_vertexScale;
};

// Renders the SR mesh
class SpatialSurfaceMeshRenderer
{
public:
    SpatialSurfaceMeshRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources);
    virtual ~SpatialSurfaceMeshRenderer();

    void Update(
        winrt::Windows::Perception::PerceptionTimestamp timestamp,
        winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem);

    void Render(bool isStereo);

    std::future<void> CreateDeviceDependentResources();
    void ReleaseDeviceDependentResources();

private:
    void OnObservedSurfaceChanged();
    void OnLocatibilityChanged(
        const winrt::Windows::Perception::Spatial::SpatialLocator& spatialLocator, const winrt::Windows::Foundation::IInspectable&);
    SpatialSurfaceMeshPart* GetOrCreateMeshPart(winrt::guid id);

private:
    friend class SpatialSurfaceMeshPart;

    // Cached pointer to device resources.
    std::shared_ptr<DXHelper::DeviceResources> m_deviceResources;

    // observer:
    int m_surfaceChangedCounter = 0;
    bool m_sufaceChanged = false;
    winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceObserver m_surfaceObserver = nullptr;
    winrt::event_token m_observedSurfaceChangedToken;

    // mesh parts
    using MeshPartMap = std::map<GUID, std::unique_ptr<SpatialSurfaceMeshPart>, Utils::GUIDComparer>;
    MeshPartMap m_meshParts;

    // rendering
    bool m_zfillOnly = false;
    std::atomic<bool> m_loadingComplete = false;
    winrt::com_ptr<ID3D11InputLayout> m_inputLayout;

    winrt::com_ptr<ID3D11VertexShader> m_vertexShader;
    winrt::com_ptr<ID3D11GeometryShader> m_geometryShader;
    winrt::com_ptr<ID3D11PixelShader> m_pixelShader;

    winrt::com_ptr<ID3D11Buffer> m_modelConstantBuffer;

    winrt::Windows::Perception::Spatial::SpatialLocator m_spatialLocator = nullptr;
    winrt::Windows::Perception::Spatial::SpatialLocator::LocatabilityChanged_revoker m_spatialLocatorLocabilityChangedEventRevoker;

    // A attached frame of reference based on m_spatialLocator.
    winrt::Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference m_attachedFrameOfReference = nullptr;

    std::chrono::time_point<std::chrono::steady_clock> m_boundingVolumeUpdateTime;
};
