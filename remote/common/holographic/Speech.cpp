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

#include <pch.h>

#include <holographic/Speech.h>

#include <windows.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Storage.h>

#include <filesystem>

namespace
{
    winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile> LoadGrammarFileAsync()
    {
        const wchar_t* speechGrammarFile = L"SpeechGrammar.xml";
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        wchar_t executablePath[MAX_PATH];

        if (GetModuleFileNameW(NULL, executablePath, ARRAYSIZE(executablePath)) == 0)
        {
            winrt::throw_last_error();
        }
        std::filesystem::path executableFolder(executablePath);
        executableFolder.remove_filename();
        auto rootFolder = co_await winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(executableFolder.c_str());
        auto file = co_await rootFolder.GetFileAsync(speechGrammarFile);
        co_return file;
#else
        auto rootFolder = winrt::Windows::ApplicationModel::Package::Current().InstalledLocation();
        return rootFolder.GetFileAsync(speechGrammarFile);
#endif
    }
} // namespace

winrt::fire_and_forget Speech::InitializeSpeechAsync(
    winrt::Microsoft::Holographic::AppRemoting::IRemoteSpeech remoteSpeech,
    winrt::Microsoft::Holographic::AppRemoting::IRemoteSpeech::OnRecognizedSpeech_revoker& onRecognizedSpeechRevoker,
    std::weak_ptr<IRemoteSpeechReceiver> sampleRemoteAppWeak)
{
    onRecognizedSpeechRevoker = remoteSpeech.OnRecognizedSpeech(
        winrt::auto_revoke, [sampleRemoteAppWeak](const winrt::Microsoft::Holographic::AppRemoting::RecognizedSpeech& recognizedSpeech) {
            if (auto sampleRemoteApp = sampleRemoteAppWeak.lock())
            {
                sampleRemoteApp->OnRecognizedSpeech(recognizedSpeech.RecognizedText);
            }
        });

    auto grammarFile = co_await LoadGrammarFileAsync();

    std::vector<winrt::hstring> dictionary;
    dictionary.push_back(L"Red");
    dictionary.push_back(L"Blue");
    dictionary.push_back(L"Green");
    dictionary.push_back(L"Default");
    dictionary.push_back(L"Aquamarine");

    remoteSpeech.ApplyParameters(L"en-US", grammarFile, dictionary);
}
