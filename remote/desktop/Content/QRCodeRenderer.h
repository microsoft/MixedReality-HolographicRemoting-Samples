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

#include <vector>

#include <holographic/RenderableObject.h>

#include <winrt/Microsoft.MixedReality.QR.h>
#include <winrt/Windows.Perception.Spatial.h>
#include <winrt/Windows.UI.Input.Spatial.h>

struct RenderableQRCode
{
    float size;
    winrt::Windows::Foundation::Numerics::float4x4 codeToRendering;
};

class QRCodeRenderer : public RenderableObject
{
public:
    QRCodeRenderer(const std::shared_ptr<DXHelper::DeviceResources>& deviceResources);

    void Update(winrt::Windows::Perception::Spatial::SpatialCoordinateSystem renderingCoordinateSystem);

    void OnAddedQRCode(const winrt::Microsoft::MixedReality::QR::QRCode& code);

    void OnUpdatedQRCode(const winrt::Microsoft::MixedReality::QR::QRCode& code);

    void Reset();

private:
    void Draw(unsigned int numInstances, winrt::Windows::Foundation::IReference<SpatialBoundingFrustum> cullingFrustum) override;

private:
    std::vector<VertexPositionNormalColor> m_vertices;

    std::map<winrt::Microsoft::MixedReality::QR::QRCode, winrt::Windows::Perception::Spatial::SpatialCoordinateSystem> m_qrCodes{};
    std::vector<RenderableQRCode> m_renderableQrCodes{};

    std::mutex m_mutex;
};
