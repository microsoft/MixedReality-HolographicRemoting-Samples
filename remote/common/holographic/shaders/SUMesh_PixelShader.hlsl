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

// Per-pixel data passed through the pixel shader.
struct PixelShaderInput
{
    float4      pos                 : SV_POSITION;
    min16float3 color               : COLOR0;
    float2      uv                  : TEXCOORD0;
    uint        idx                 : TEXCOORD1;
    float2      barycentricCoords   : TEXCOORD2;
};

#define LINE_WIDTH 1.0

float GetWireframeFactor(PixelShaderInput input)
{
    float3 barycentrics =
        float3(input.barycentricCoords.x, input.barycentricCoords.y, 1 - input.barycentricCoords.x - input.barycentricCoords.y);
    float3 deltas = fwidth(barycentrics);
    barycentrics = step(deltas * LINE_WIDTH, barycentrics);
    return 1.0 - min(barycentrics.x, min(barycentrics.y, barycentrics.z));
}

// Pixel shader for wireframe shading.
min16float4 main(PixelShaderInput input)
    : SV_TARGET
{
    return min16float4(input.color * GetWireframeFactor(input), 1.0f);
}
