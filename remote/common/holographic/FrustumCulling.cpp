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

#include <holographic/FrustumCulling.h>

using namespace FrustumCulling;

bool FrustumCulling::PointInFrustum(
    const winrt::Windows::Foundation::Numerics::float3& point,
    const winrt::Windows::Foundation::IReference<SpatialBoundingFrustum>& cullingFrustum)
{
    if (!cullingFrustum)
    {
        return true;
    }
    SpatialBoundingFrustum frustum = cullingFrustum.Value();
    if (dot_coordinate(frustum.Bottom, point) > 0)
    {
        return false;
    }
    else if (dot_coordinate(frustum.Far, point) > 0)
    {
        return false;
    }
    else if (dot_coordinate(frustum.Left, point) > 0)
    {
        return false;
    }
    else if (dot_coordinate(frustum.Near, point) > 0)
    {
        return false;
    }
    else if (dot_coordinate(frustum.Right, point) > 0)
    {
        return false;
    }
    else if (dot_coordinate(frustum.Top, point) > 0)
    {
        return false;
    }
    return true;
}

bool FrustumCulling::SphereInFrustum(
    const winrt::Windows::Foundation::Numerics::float3& sphereCenter,
    float sphereRadius,
    const winrt::Windows::Foundation::IReference<SpatialBoundingFrustum>& cullingFrustum)
{
    if (!cullingFrustum)
    {
        return true;
    }
    SpatialBoundingFrustum frustum = cullingFrustum.Value();
    if (dot_coordinate(frustum.Bottom, sphereCenter) - sphereRadius > 0)
    {
        return false;
    }
    else if (dot_coordinate(frustum.Far, sphereCenter) - sphereRadius > 0)
    {
        return false;
    }
    else if (dot_coordinate(frustum.Left, sphereCenter) - sphereRadius > 0)
    {
        return false;
    }
    else if (dot_coordinate(frustum.Near, sphereCenter) - sphereRadius > 0)
    {
        return false;
    }
    else if (dot_coordinate(frustum.Right, sphereCenter) - sphereRadius > 0)
    {
        return false;
    }
    else if (dot_coordinate(frustum.Top, sphereCenter) - sphereRadius > 0)
    {
        return false;
    }
    return true;
}
