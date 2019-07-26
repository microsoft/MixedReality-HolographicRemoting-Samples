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

#include <winrt/Windows.Networking.Connectivity.h>


class IpAddressUpdater
{
public:
    IpAddressUpdater();
    ~IpAddressUpdater();

    winrt::hstring GetIpAddress();

private:
    void UpdateIpAddress(winrt::Windows::Foundation::IInspectable sender);

private:
    std::mutex m_lock;
    winrt::hstring m_ipAddress;

    winrt::Windows::Networking::Connectivity::NetworkInformation::NetworkStatusChanged_revoker m_networkStatusChangedRevoker;
};
