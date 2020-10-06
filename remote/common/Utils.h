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

#include <string>

namespace Utils
{
    // comparison function to allow for GUID as a hash map key
    struct GUIDComparer
    {
        inline static int compare(const GUID& Left, const GUID& Right)
        {
            return memcmp(&Left, &Right, sizeof(GUID));
        }

        inline static bool equals(const GUID& Left, const GUID& Right)
        {
            return memcmp(&Left, &Right, sizeof(GUID)) == 0;
        }

        bool operator()(const GUID& Left, const GUID& Right) const
        {
            return compare(Left, Right) < 0;
        }
    };

    std::wstring SplitHostnameAndPortString(const std::wstring& address, uint16_t& port);
} // namespace Utils
