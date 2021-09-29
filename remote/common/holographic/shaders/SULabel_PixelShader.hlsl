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


Texture2D       tex     : t0;
SamplerState    samp    : s0;

// Per-pixel data passed through the pixel shader.
struct PixelShaderInput
{
    float4      pos                 : SV_POSITION;
    min16float3 color               : COLOR0;
    float2      uv                  : TEXCOORD0;
    uint        idx                 : TEXCOORD1;
    float2      barycentricCoords   : TEXCOORD2;
};


// Pixel shader for multisampled texture shading.
float4 main(PixelShaderInput input) : SV_TARGET
{
    const float2 offsets[4] = {float2(-0.125, -0.375), float2(0.375, -0.125), float2(-0.375, 0.125), float2(0.125f, 0.375f)};
    const float2 dtdx = ddx(input.uv);
    const float2 dtdy = ddy(input.uv);
    
    float4 color = float4(0, 0, 0, 0);
    
    [unroll] 
    for (int i = 0; i < 4; ++i)
    {
        color += 0.25 * tex.Sample(samp, input.uv + offsets[i].x * dtdx + offsets[i].y * dtdy);
    }
    
    return color;
}
