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


cbuffer LightConstantBuffer : register(b0)
{
    float4 lightDir;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
    float4      pos         : SV_POSITION;
    float2      uv          : TEXCOORD0;
    float3      normal      : NORMAL;
};

#define diffuse float4(0.75f, 0.75f, 0.75f, 1.0f)
#define ambientLight float4(0.05f, 0.05f, 0.05f, 1.0f)

float4 main(PixelShaderInput input)
    : SV_TARGET
{
    float2 offsets[4] = {float2(-0.125, -0.375), float2(0.375, -0.125), float2(-0.375, 0.125), float2(0.125f, 0.375f)};
    float4 color = float4(0, 0, 0, 0);
    float2 dtdx = ddx(input.uv);
    float2 dtdy = ddy(input.uv);

    [unroll] for (int i = 0; i < 4; ++i)
    {
        float2 uv = input.uv + offsets[i].x * dtdx + offsets[i].y * dtdy;
        float checkerBoard = floor(30 * uv.x) + floor(30 * uv.y);
        float c = frac(checkerBoard * 0.5f) * 2;
        color += 0.25f * float4(c + 0.15f, c + 0.15f, c + 0.15f, 1);
    }
   
  
    float4 result = saturate(color) * (ambientLight + saturate(dot(input.normal, lightDir)));
    return result;
}
