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

class IIpAddressUpdater
{
public:
    ~IIpAddressUpdater() = default;

    virtual std::wstring GetIpAddress(bool IPv6) = 0;
};

std::shared_ptr<IIpAddressUpdater> CreateIpAddressUpdater();
