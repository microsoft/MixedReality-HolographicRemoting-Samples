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

#include "pch.h"

#include "PlayerUtil.h"

#include <codecvt>
#include <regex>

std::wstring PlayerUtil::SplitHostnameAndPortString(const std::wstring& address, uint16_t& port)
{
    static std::basic_regex<wchar_t> addressMatcher(L"(?:(\\[.*\\])|([^:]*))(?:[:](\\d+))?");
    std::match_results<typename std::wstring::const_iterator> results;
    if (std::regex_match(address, results, addressMatcher))
    {
        if (results[3].matched)
        {
            std::wstring portStr = results[3].str();
            port = static_cast<uint16_t>(std::wcstol(portStr.c_str(), nullptr, 10));
        }

        return (results[1].matched) ? results[1].str() : results[2].str();
    }
    else
    {
        return address;
    }
}
