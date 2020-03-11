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

class PerceptionDeviceHandler;

class QRCodeRenderer : public RenderableObject
{
public:
    QRCodeRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources);

    void Update(
        PerceptionDeviceHandler& perceptionDeviceHandler,
        winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem);

private:
    void Draw(unsigned int numInstances) override;

private:
    std::vector<VertexPositionNormalColor> m_vertices;
};
