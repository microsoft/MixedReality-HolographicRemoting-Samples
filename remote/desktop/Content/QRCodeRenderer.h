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

class PerceptionDeviceHandler;

struct RenderableQRCode
{
    float size;
    winrt::Windows::Foundation::Numerics::float4x4 codeToRendering;
};

class QRCodeRenderer : public RenderableObject
{
public:
    QRCodeRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources);

    void Update(
        PerceptionDeviceHandler& perceptionDeviceHandler,
        winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem);

private:
    void Draw(unsigned int numInstances, winrt::Windows::Foundation::IReference<SpatialBoundingFrustum> cullingFrustum) override;

private:
    std::vector<VertexPositionNormalColor> m_vertices;
    std::vector<RenderableQRCode> m_renderableQRCodes{};
};
