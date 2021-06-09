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

float GetCheckerboardFactor(PixelShaderInput input)
{
    const float2 offsets[4] = {float2(-0.125, -0.375), float2(0.375, -0.125), float2(-0.375, 0.125), float2(0.125f, 0.375f)};
    const float2 dtdx = ddx(input.uv);
    const float2 dtdy = ddy(input.uv);

    float checkerBoardFactor = 0;
    for (int i = 0; i < 4; ++i)
    {
        float2 uv = input.uv + offsets[i].x * dtdx + offsets[i].y * dtdy;
        float checkerBoard = floor(15 * uv.x) + floor(15 * uv.y);
        float c = (frac(checkerBoard * 0.5f) * 2);
        checkerBoardFactor += 0.25f * c;
    }

    return checkerBoardFactor;
}

// Pixel shader for multisampled checkerboard shading.
min16float4 main(PixelShaderInput input)
    : SV_TARGET
{
    return min16float4(lerp(input.color, float3(0.1f, 0.1f, 0.1f), GetCheckerboardFactor(input)),1);
}
