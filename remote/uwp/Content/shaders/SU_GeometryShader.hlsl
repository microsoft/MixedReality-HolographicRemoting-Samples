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

// Per-vertex data from the vertex shader.
struct GeometryShaderInput
{
    float4      pos     : SV_POSITION;
    min16float3 color   : COLOR0;
    float2      uv      : TEXCOORD0;
    uint        instId  : TEXCOORD1;
};

// Per-vertex data passed to the rasterizer.
struct GeomertryShaderOutput
{
    float4      pos                 : SV_POSITION;
    min16float3 color               : COLOR0;
    float2      uv                  : TEXCOORD0;
    uint        instId              : TEXCOORD1;
    float2      barycentricCoords   : TEXCOORD2;
    uint        rtvId               : SV_RenderTargetArrayIndex;
};

[maxvertexcount(3)] 
void main(triangle GeometryShaderInput input[3], inout TriangleStream<GeomertryShaderOutput> outStream) 
{
    GeomertryShaderOutput o[3];

    o[0].barycentricCoords = float2(1, 0);
    o[1].barycentricCoords = float2(0, 1);
    o[2].barycentricCoords = float2(0, 0);

    [unroll(3)] 
    for (int i = 0; i < 3; ++i)
    {
        o[i].pos = input[i].pos;
        o[i].color = input[i].color;
        o[i].uv = input[i].uv;
        o[i].instId = input[i].instId;
        o[i].rtvId = input[i].instId;
        outStream.Append(o[i]);
    }
}