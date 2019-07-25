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

#include "SpatialInputRenderer.h"

#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Perception.People.h>
#include <winrt/Windows.Perception.Spatial.h>

#include "../Common/DirectXHelper.h"
#include <algorithm>

namespace
{
    using namespace DirectX;

    void AppendColoredTriangle(XMFLOAT3 p0, XMFLOAT3 p1, XMFLOAT3 p2, XMFLOAT3 color, std::vector<VertexPositionNormalColor>& vertices)
    {
        VertexPositionNormalColor vertex;
        vertex.color = color;
        vertex.normal = XMFLOAT3(0.0f, 0.0f, 0.0f);

        vertex.pos = p0;
        vertices.push_back(vertex);
        vertex.pos = p1;
        vertices.push_back(vertex);
        vertex.pos = p2;
        vertices.push_back(vertex);
    }
} // namespace

SpatialInputRenderer::SpatialInputRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources)
    : RenderableObject(deviceResources)
{
    m_manager = winrt::Windows::UI::Input::Spatial::SpatialInteractionManager::GetForCurrentView();
    m_referenceFrame = winrt::Windows::Perception::Spatial::SpatialLocator::GetDefault().CreateAttachedFrameOfReferenceAtCurrentHeading();
}

void SpatialInputRenderer::Update(
    winrt::Windows::Perception::PerceptionTimestamp timestamp,
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem)
{
    m_transforms.clear();
    m_joints.clear();

    auto headingAdjustment = m_referenceFrame.TryGetRelativeHeadingAtTimestamp(timestamp);
    if (headingAdjustment)
    {
        // keep coordinate systems facing user
        m_referenceFrame.AdjustHeading(-headingAdjustment.Value());
        auto coordinateSystem = m_referenceFrame.GetStationaryCoordinateSystemAtTimestamp(timestamp);

        auto spatialPointerPose = winrt::Windows::UI::Input::Spatial::SpatialPointerPose::TryGetAtTimestamp(coordinateSystem, timestamp);
        if (spatialPointerPose)
        {
            if (auto eyesPose = spatialPointerPose.Eyes())
            {
                if (auto gaze = eyesPose.Gaze())
                {
                    auto position = gaze.Value().Origin + gaze.Value().Direction;
                    quaternion orientation = quaternion::identity();
                    m_transforms.emplace_back(QTransform(position, orientation));
                }
            }
        }

        auto states = m_manager.GetDetectedSourcesAtTimestamp(timestamp);

        m_transforms.reserve(states.Size());
        for (const auto& state : states)
        {
            auto location = state.Properties().TryGetLocation(coordinateSystem);
            if (location)
            {
                if (location.Position())
                {
                    using namespace winrt::Windows::Foundation::Numerics;

                    auto position = location.Position().Value();
                    quaternion orientation = quaternion::identity();

                    if (location.Orientation())
                    {
                        orientation = location.Orientation().Value();
                    }

                    DirectX::XMVECTOR xmPosition = DirectX::XMLoadFloat3(&position);
                    DirectX::XMVECTOR xmOrientation = DirectX::XMLoadQuaternion(&orientation);
                    m_transforms.emplace_back(QTransform(xmPosition, xmOrientation));
                }

                if (auto sourcePose = location.SourcePointerPose())
                {
                    m_joints.push_back({sourcePose.Position(), sourcePose.Orientation(), 1.0f, 0.01f});
                }
            }

            auto handPose = state.TryGetHandPose();
            if (handPose)
            {
                constexpr const winrt::Windows::Perception::People::HandJointKind jointKinds[] = {
                    winrt::Windows::Perception::People::HandJointKind::Palm,
                    winrt::Windows::Perception::People::HandJointKind::Wrist,
                    winrt::Windows::Perception::People::HandJointKind::ThumbMetacarpal,
                    winrt::Windows::Perception::People::HandJointKind::ThumbProximal,
                    winrt::Windows::Perception::People::HandJointKind::ThumbDistal,
                    winrt::Windows::Perception::People::HandJointKind::ThumbTip,
                    winrt::Windows::Perception::People::HandJointKind::IndexMetacarpal,
                    winrt::Windows::Perception::People::HandJointKind::IndexProximal,
                    winrt::Windows::Perception::People::HandJointKind::IndexIntermediate,
                    winrt::Windows::Perception::People::HandJointKind::IndexDistal,
                    winrt::Windows::Perception::People::HandJointKind::IndexTip,
                    winrt::Windows::Perception::People::HandJointKind::MiddleMetacarpal,
                    winrt::Windows::Perception::People::HandJointKind::MiddleProximal,
                    winrt::Windows::Perception::People::HandJointKind::MiddleIntermediate,
                    winrt::Windows::Perception::People::HandJointKind::MiddleDistal,
                    winrt::Windows::Perception::People::HandJointKind::MiddleTip,
                    winrt::Windows::Perception::People::HandJointKind::RingMetacarpal,
                    winrt::Windows::Perception::People::HandJointKind::RingProximal,
                    winrt::Windows::Perception::People::HandJointKind::RingIntermediate,
                    winrt::Windows::Perception::People::HandJointKind::RingDistal,
                    winrt::Windows::Perception::People::HandJointKind::RingTip,
                    winrt::Windows::Perception::People::HandJointKind::LittleMetacarpal,
                    winrt::Windows::Perception::People::HandJointKind::LittleProximal,
                    winrt::Windows::Perception::People::HandJointKind::LittleIntermediate,
                    winrt::Windows::Perception::People::HandJointKind::LittleDistal,
                    winrt::Windows::Perception::People::HandJointKind::LittleTip};
                constexpr const size_t jointCount = _countof(jointKinds);
                winrt::Windows::Perception::People::JointPose jointPoses[jointCount] = {};

                if (handPose.TryGetJoints(coordinateSystem, jointKinds, jointPoses))
                {
                    for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex)
                    {
                        m_joints.push_back({jointPoses[jointIndex].Position,
                                            jointPoses[jointIndex].Orientation,
                                            jointPoses[jointIndex].Radius * 3,
                                            jointPoses[jointIndex].Radius});
                    }
                }
            }
        }

        auto modelTransform = coordinateSystem.TryGetTransformTo(renderingCoordinateSystem);
        if (modelTransform)
        {
            UpdateModelConstantBuffer(modelTransform.Value());
        }
    }
}

void SpatialInputRenderer::Draw(unsigned int numInstances)
{
    if (!m_transforms.empty())
    {
        std::vector<VertexPositionNormalColor> vertices;
        for (const auto& transform : m_transforms)
        {
            DirectX::XMFLOAT3 trianglePositions[3] = {
                DirectX::XMFLOAT3(0.0f, 0.03f, 0.0f), DirectX::XMFLOAT3(0.01f, 0.0f, 0.0f), DirectX::XMFLOAT3(-0.01f, 0.0f, 0.0f)};

            AppendColoredTriangle(
                transform.TransformPosition(trianglePositions[0]),
                transform.TransformPosition(trianglePositions[1]),
                transform.TransformPosition(trianglePositions[2]),
                DirectX::XMFLOAT3{0, 0, 1},
                vertices);
        }

        for (const auto& joint : m_joints)
        {
            auto jointVertices = CalculateJointVisualizationVertices(joint.position, joint.orientation, joint.length, joint.radius);
            vertices.insert(vertices.end(), jointVertices.begin(), jointVertices.end());
        }

        const UINT stride = sizeof(vertices[0]);
        const UINT offset = 0;
        D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
        vertexBufferData.pSysMem = vertices.data();
        const CD3D11_BUFFER_DESC vertexBufferDesc(static_cast<UINT>(vertices.size() * stride), D3D11_BIND_VERTEX_BUFFER);
        winrt::com_ptr<ID3D11Buffer> vertexBuffer;
        winrt::check_hresult(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, vertexBuffer.put()));

        m_deviceResources->UseD3DDeviceContext([&](auto context) {
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            ID3D11Buffer* pBuffer = vertexBuffer.get();
            context->IASetVertexBuffers(0, 1, &pBuffer, &stride, &offset);
            context->DrawInstanced(static_cast<UINT>(vertices.size()), numInstances, offset, 0);
        });
    }
}


std::vector<VertexPositionNormalColor> SpatialInputRenderer::CalculateJointVisualizationVertices(
    float3 jointPosition, quaternion jointOrientation, float jointLength, float jointRadius)
{
    using namespace DirectX;

    constexpr const size_t verticesCount = 2 * 4 * 3;
    std::vector<VertexPositionNormalColor> vertices;
    vertices.reserve(verticesCount);

    float centerHeight = std::min<float>(jointRadius, 0.5f * jointLength);
    float centerXandY = jointRadius / sqrtf(2.0f);

    QTransform jointTransform = QTransform(jointPosition, jointOrientation);

    XMFLOAT3 baseVertexPosition = jointTransform.TransformPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));
    XMFLOAT3 centerVertexPositions[4] = {
        jointTransform.TransformPosition(XMFLOAT3(-centerXandY, -centerXandY, -centerHeight)),
        jointTransform.TransformPosition(XMFLOAT3(-centerXandY, +centerXandY, -centerHeight)),
        jointTransform.TransformPosition(XMFLOAT3(+centerXandY, +centerXandY, -centerHeight)),
        jointTransform.TransformPosition(XMFLOAT3(+centerXandY, -centerXandY, -centerHeight)),
    };
    XMFLOAT3 topVertexPosition = jointTransform.TransformPosition(XMFLOAT3(0.0f, 0.0f, -jointLength));

    AppendColoredTriangle(baseVertexPosition, centerVertexPositions[0], centerVertexPositions[1], XMFLOAT3(0.0f, 0.0f, 0.4f), vertices);
    AppendColoredTriangle(baseVertexPosition, centerVertexPositions[1], centerVertexPositions[2], XMFLOAT3(0.0f, 0.4f, 0.0f), vertices);
    AppendColoredTriangle(baseVertexPosition, centerVertexPositions[2], centerVertexPositions[3], XMFLOAT3(0.4f, 0.0f, 0.0f), vertices);
    AppendColoredTriangle(baseVertexPosition, centerVertexPositions[3], centerVertexPositions[0], XMFLOAT3(0.4f, 0.4f, 0.0f), vertices);
    AppendColoredTriangle(topVertexPosition, centerVertexPositions[1], centerVertexPositions[0], XMFLOAT3(0.0f, 0.0f, 0.6f), vertices);
    AppendColoredTriangle(topVertexPosition, centerVertexPositions[2], centerVertexPositions[1], XMFLOAT3(0.0f, 0.6f, 0.0f), vertices);
    AppendColoredTriangle(topVertexPosition, centerVertexPositions[3], centerVertexPositions[2], XMFLOAT3(0.6f, 0.0f, 0.0f), vertices);
    AppendColoredTriangle(topVertexPosition, centerVertexPositions[0], centerVertexPositions[3], XMFLOAT3(0.6f, 0.6f, 0.0f), vertices);

    return vertices;
}
