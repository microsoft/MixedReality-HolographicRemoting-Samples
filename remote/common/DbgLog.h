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

#include <strsafe.h>

static void DebugLog(_In_z_ LPCWSTR format, ...)
{
    wchar_t buffer[1024];
    LPWSTR bufEnd = nullptr;

    va_list args;
    va_start(args, format);
    HRESULT hr =
        StringCchVPrintfExW(buffer, _countof(buffer), &bufEnd, nullptr, STRSAFE_FILL_BEHIND_NULL | STRSAFE_FILL_ON_FAILURE, format, args);

    if (SUCCEEDED(hr))
    {
        if (*bufEnd != L'\n')
        {
            StringCchCatW(buffer, _countof(buffer), L"\r\n");
        }

        OutputDebugStringW(buffer);
    }

    va_end(args);
}
