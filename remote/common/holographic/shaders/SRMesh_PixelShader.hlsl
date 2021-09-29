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

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
    float4  pos                 : SV_POSITION;
    float3  barycentricCoords   : TEXCOORD0;
};

#define LINE_WIDTH 2.0

float edgeFactor(float3 coords)
{
    float3 d = fwidth(coords);
    float3 a3 = smoothstep(0.0, d*LINE_WIDTH, coords);
    return min(min(a3.x, a3.y), a3.z);
}

// The pixel shader passes through the color data. The color data from 
// is interpolated and assigned to a pixel at the rasterization step.
float4 main(PixelShaderInput input) : SV_TARGET
{
    float lineBrightness = 1.0f - edgeFactor(input.barycentricCoords);
    float4 result = float4(0.25, 0.25, 0.25, 1.0);
    result.xyz *= lineBrightness;
    return result;
}
