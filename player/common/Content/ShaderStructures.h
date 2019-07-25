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

#pragma once

// Constant buffer used to send hologram position transform to the shader pipeline.
struct ModelConstantBuffer
{
    DirectX::XMFLOAT4X4 model;
};

// Assert that the constant buffer remains multiple of 16-bytes (best practice).
static_assert(
    (sizeof(ModelConstantBuffer) % (sizeof(float) * 4)) == 0,
    "Model constant buffer size must be multiple of 16-bytes (16 bytes is the length of four floats).");


// Used to send per-vertex data to the vertex shader.
struct VertexBufferElement
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 uv;
};
