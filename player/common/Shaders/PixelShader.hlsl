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
    min16float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0; // Note: we use full precission floats for texture coordinates to avoid artifacts with large textures
};

Texture2D tex : t0;
SamplerState samp : s0;

float4 main(PixelShaderInput input) : SV_TARGET
{
    float2 offsets[4] = {float2(-0.125, -0.375), float2(0.375, -0.125), float2(-0.375, 0.125), float2(0.125f, 0.375f)};
    float4 color = float4(0, 0, 0, 0);
    float2 dtdx = ddx(input.uv);
    float2 dtdy = ddy(input.uv);

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        color += 0.25 * tex.Sample(samp, input.uv + offsets[i].x * dtdx + offsets[i].y * dtdy);
    }
    return color;
}
