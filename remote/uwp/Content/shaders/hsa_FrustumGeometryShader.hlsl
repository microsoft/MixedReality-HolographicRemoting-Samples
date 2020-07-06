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
    float4      pos      : SV_POSITION;
    float2      uv       : TEXCOORD0;
    uint        instId   : TEXCOORD1;
    float3      worldPos : TEXCOORD2;
};

// Per-vertex data passed to the rasterizer.
struct GeometryShaderOutput
{
    float4      pos      : SV_POSITION;
    float2      uv       : TEXCOORD0;
    float3      normal   : NORMAL;
    uint        rtvId    : SV_RenderTargetArrayIndex;
};

// This geometry shader is a pass-through that leaves the geometry unmodified 
// and sets the render target array index.
[maxvertexcount(3)]
void main(triangle GeometryShaderInput input[3], inout TriangleStream<GeometryShaderOutput> outStream)
{
    GeometryShaderOutput output;

    float3 faceNormal = normalize(cross((input[1].worldPos - input[0].worldPos), (input[2].worldPos - input[0].worldPos)));

    [unroll(3)]
    for (int i = 0; i < 3; ++i)
    {
        output.pos      = input[i].pos;
        output.uv       = input[i].uv;
        output.normal   = faceNormal;
        output.rtvId    = input[i].instId;

        outStream.Append(output);
    }
}
