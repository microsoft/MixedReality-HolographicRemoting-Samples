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
#include "RenderableObject.h"
#include <vector>

#include <winrt/Windows.UI.Input.Spatial.h>

using namespace winrt::Windows::Foundation::Numerics;

struct QTransform
{
    QTransform() = default;
    QTransform(const DirectX::XMVECTOR& position, const DirectX::XMVECTOR& orientation)
        : m_position(position)
        , m_orientation(orientation)
    {
    }
    QTransform(const float3& position, const quaternion& orientation)
        : m_position(DirectX::XMLoadFloat3(&position))
        , m_orientation(DirectX::XMLoadQuaternion(&orientation))
    {
    }


    DirectX::XMVECTOR TransformNormal(const DirectX::XMVECTOR& normal) const
    {
        return DirectX::XMVector3Rotate(normal, m_orientation);
    }

    DirectX::XMVECTOR TransformPosition(const DirectX::XMVECTOR& position) const
    {
        DirectX::XMVECTOR rotated = TransformNormal(position);
        return DirectX::XMVectorAdd(rotated, m_position);
    }

    DirectX::XMFLOAT3 TransformPosition(const DirectX::XMFLOAT3& position) const
    {
        DirectX::XMFLOAT3 result;
        XMStoreFloat3(&result, TransformPosition(DirectX::XMLoadFloat3(&position)));
        return result;
    }

    DirectX::XMVECTOR m_position;
    DirectX::XMVECTOR m_orientation;
};

class SpatialInputRenderer : public RenderableObject
{
public:
    SpatialInputRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources);

    void Update(
        winrt::Windows::Perception::PerceptionTimestamp timestamp,
        winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem);

private:
    struct Joint
    {
        float3 position;
        quaternion orientation;
        float length;
        float radius;
    };

private:
    static std::vector<VertexPositionNormalColor>
        CalculateJointVisualizationVertices(float3 jointPosition, quaternion jointOrientation, float jointLength, float jointRadius);

    void Draw(unsigned int numInstances) override;

    winrt::Windows::UI::Input::Spatial::SpatialInteractionManager m_manager{nullptr};
    winrt::Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference m_referenceFrame{nullptr};
    std::vector<QTransform> m_transforms;
    std::vector<Joint> m_joints;
};
