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

cbuffer ColorFilterConstantBuffer : register(b0)
{
    float4 colorFilter;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
    float4      pos         : SV_POSITION;
    min16float3 color       : COLOR0;
    uint        idx         : TEXCOORD0;
};

// Simple pixel shader.
min16float4 main(PixelShaderInput input) : SV_TARGET
{
    return min16float4(input.color, 1.f) * min16float4(colorFilter);
}
