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

// Per-vertex data passed to the geometry shader.
struct VertexShaderOutput
{
    min16float4 pos : SV_POSITION;
    float2      uv  : TEXCOORD0;

    // The render target array index will be set by the geometry shader.
    uint        viewId  : TEXCOORD1;
};

#include "VertexShaderShared.hlsl"
