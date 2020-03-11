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

#include <wrl.h>

#include <future>

#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>

#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#include <windows.graphics.directx.direct3d11.interop.h>

#include <winrt/Windows.Foundation.Collections.h>

#include <d3d11.h>
#include <dxgi1_2.h>

#include <filesystem>

namespace DXHelper
{
    // Function that reads from a binary file asynchronously.
    inline std::future<std::vector<byte>> ReadDataAsync(const std::wstring_view& filename)
    {
        using namespace winrt::Windows::Storage;
        using namespace winrt::Windows::Storage::Streams;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

        wchar_t moduleFullyQualifiedFilename[MAX_PATH] = {};
        uint32_t moduleFileNameLength = GetModuleFileNameW(NULL, moduleFullyQualifiedFilename, _countof(moduleFullyQualifiedFilename) - 1);
        moduleFullyQualifiedFilename[moduleFileNameLength] = L'\0';

        std::filesystem::path modulePath = moduleFullyQualifiedFilename;
        // winrt::hstring moduleFilename = modulePath.filename().c_str();
        modulePath.replace_filename(filename);
        winrt::hstring absoluteFilename = modulePath.c_str();

        IBuffer fileBuffer = co_await PathIO::ReadBufferAsync(absoluteFilename);
#else
        IBuffer fileBuffer = co_await PathIO::ReadBufferAsync(filename);
#endif

        std::vector<byte> returnBuffer;
        returnBuffer.resize(fileBuffer.Length());
        DataReader::FromBuffer(fileBuffer).ReadBytes(winrt::array_view<uint8_t>(returnBuffer));
        return returnBuffer;
    }

    // Converts a length in device-independent pixels (DIPs) to a length in physical pixels.
    inline float ConvertDipsToPixels(float dips, float dpi)
    {
        static const float dipsPerInch = 96.0f;
        return floorf(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
    }

#if defined(_DEBUG)
    // Check for SDK Layer support.
    inline bool SdkLayersAvailable()
    {
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_NULL, // There is no need to create a real hardware device.
            0,
            D3D11_CREATE_DEVICE_DEBUG, // Check for the SDK layers.
            nullptr,                   // Any feature level will do.
            0,
            D3D11_SDK_VERSION, // Always set this to D3D11_SDK_VERSION for Windows Runtime apps.
            nullptr,           // No need to keep the D3D device reference.
            nullptr,           // No need to know the feature level.
            nullptr            // No need to keep the D3D device context reference.
        );

        return SUCCEEDED(hr);
    }
#endif
} // namespace DXHelper
