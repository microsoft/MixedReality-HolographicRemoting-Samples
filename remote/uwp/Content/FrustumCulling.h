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

#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Perception.Spatial.h>

namespace FrustumCulling
{
    using namespace winrt::Windows::Perception::Spatial;
    // Returns true if the point is inside the frustum or if no cullingFrustum is available.
    bool PointInFrustum(
        const winrt::Windows::Foundation::Numerics::float3& point,
        const winrt::Windows::Foundation::IReference<SpatialBoundingFrustum>& cullingFrustum);

    // Returns true if the sphere(given by its center and radius) is inside the frustum, or if no cullingFrustum is available.
    bool SphereInFrustum(
        const winrt::Windows::Foundation::Numerics::float3& sphereCenter,
        float sphereRadius,
        const winrt::Windows::Foundation::IReference<SpatialBoundingFrustum>& cullingFrustum);
}; // namespace FrustumCulling
