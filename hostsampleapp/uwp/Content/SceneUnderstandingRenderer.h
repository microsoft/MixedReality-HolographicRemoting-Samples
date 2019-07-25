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

#include <winrt/SceneUnderstanding.h>
#include <winrt/Windows.UI.Input.Spatial.h>

namespace RemotingHostSample
{
    class SceneUnderstandingRenderer : public RenderableObject
    {
    public:
        SceneUnderstandingRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);

        void Update(
            winrt::SceneUnderstanding::SceneProcessor& sceneProcessor,
            winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem,
            winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference lastUpdateLocation);

        void DebugLogState(
            winrt::SceneUnderstanding::SceneProcessor& sceneProcessor,
            winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem,
            winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference lastUpdateLocation);

    private:
        void Draw(unsigned int numInstances) override;

        template <typename Func>
        void ForEachQuad(winrt::SceneUnderstanding::SceneProcessor& sceneProcessor, Func f);

    private:
        std::vector<VertexPositionNormalColor> m_vertices;
    };
} // namespace RemotingHostSample
