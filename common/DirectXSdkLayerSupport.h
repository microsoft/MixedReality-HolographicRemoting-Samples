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

namespace DXHelper
{
#ifdef _DEBUG
    // Check for SDK Layer support.
    inline bool SdkLayersAvailable()
    {
        D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_NULL;

#    if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        if (GetModuleHandleW(L"renderdoc.dll") != NULL)
        {
            // In theory there is no need to create a real hardware device for this check. Unfortunately, RenderDoc can fail without a real
            // hardware device.
            driverType = D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_HARDWARE;
        }
#    endif

        HRESULT hr = D3D11CreateDevice(
            nullptr,
            driverType,
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
