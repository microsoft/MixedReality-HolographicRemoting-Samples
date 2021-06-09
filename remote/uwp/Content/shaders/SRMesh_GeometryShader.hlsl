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
    float4  pos      : SV_POSITION;
    uint    instId   : TEXCOORD1;
};

// Per-vertex data passed to the rasterizer.
struct GeometryShaderOutput
{
    float4  pos                 : SV_POSITION;
    float3  barycentricCoords   : TEXCOORD0;
    uint    rtvId               : SV_RenderTargetArrayIndex;
};

// This geometry shader is a pass-through that leaves the geometry unmodified 
// and sets the render target array index.
[maxvertexcount(3)]
void main(triangle GeometryShaderInput input[3], inout TriangleStream<GeometryShaderOutput> outStream)
{
    static const float3 barycentricVectors[3] = {
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f)
    };

    GeometryShaderOutput output;
    [unroll(3)]
    for (int i = 0; i < 3; ++i)
    {
        output.pos      = input[i].pos;
        output.barycentricCoords = barycentricVectors[i];
        output.rtvId    = input[i].instId;

        outStream.Append(output);
    }
}
