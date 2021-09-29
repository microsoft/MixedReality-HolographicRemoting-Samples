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

#include <holographic/RenderableObject.h>

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
    QTransform(const float4x4& mat)
    {
        DirectX::XMMATRIX xmmat = DirectX::XMLoadFloat4x4(&mat);
        DirectX::XMVECTOR dummyScale;
        XMMatrixDecompose(&dummyScale, &m_orientation, &m_position, xmmat);
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

    float3 TransformPosition(const float3& position) const
    {
        DirectX::XMFLOAT3 temp;
        XMStoreFloat3(&temp, TransformPosition(DirectX::XMLoadFloat3(&position)));
        return float3(temp.x, temp.y, temp.z);
    }

    DirectX::XMVECTOR m_position;
    DirectX::XMVECTOR m_orientation;
};

class SpatialInputRenderer : public RenderableObject
{
public:
    SpatialInputRenderer(
        const std::shared_ptr<DXHelper::DeviceResourcesD3D11>& deviceResources,
        winrt::Windows::UI::Input::Spatial::SpatialInteractionManager interactionManager);

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

    struct ColoredTransform
    {
        ColoredTransform(const QTransform& transform, const DirectX::XMFLOAT3& color)
            : m_transform(transform)
            , m_color(color)
        {
        }
        ColoredTransform(const float3& position, const quaternion& orientation, const DirectX::XMFLOAT3& colorIn)
            : m_transform(position, orientation)
            , m_color(colorIn)
        {
        }
        QTransform m_transform;
        DirectX::XMFLOAT3 m_color;
    };

private:
    static std::vector<VertexPositionNormalColor> CalculateJointVisualizationVertices(
        float3 jointPosition, quaternion jointOrientation, float jointLength, float jointRadius);

    void Draw(
        unsigned int numInstances,
        winrt::Windows::Foundation::IReference<winrt::Windows::Perception::Spatial::SpatialBoundingFrustum> cullingFrustum) override;

    winrt::Windows::UI::Input::Spatial::SpatialInteractionManager m_interactionManager{nullptr};
    winrt::Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference m_referenceFrame{nullptr};
    std::vector<QTransform> m_transforms;
    std::vector<Joint> m_joints;
    std::vector<ColoredTransform> m_coloredTransforms;

    winrt::Windows::Foundation::Numerics::float4x4 m_modelTransform;
};
