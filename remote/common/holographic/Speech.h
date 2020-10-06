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

#include <memory>

#include <winrt/Microsoft.Holographic.AppRemoting.h>
#include <winrt/Windows.Foundation.h>

namespace Speech
{
    struct IRemoteSpeechReceiver
    {
        virtual void OnRecognizedSpeech(const winrt::hstring& recognizedText) = 0;
    };

    winrt::fire_and_forget InitializeSpeechAsync(
        winrt::Microsoft::Holographic::AppRemoting::IRemoteSpeech remoteSpeech,
        winrt::Microsoft::Holographic::AppRemoting::IRemoteSpeech::OnRecognizedSpeech_revoker& onRecognizedSpeechRevoker,
        std::weak_ptr<IRemoteSpeechReceiver> sampleRemoteAppWeak);
} // namespace Speech
