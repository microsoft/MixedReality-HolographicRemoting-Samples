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

#include "IpAddressUpdater.h"


IpAddressUpdater::IpAddressUpdater()
{
    m_networkStatusChangedRevoker = winrt::Windows::Networking::Connectivity::NetworkInformation::NetworkStatusChanged(
        winrt::auto_revoke, [this](winrt::Windows::Foundation::IInspectable sender) { UpdateIpAddress(sender); });

    UpdateIpAddress(nullptr);
}

IpAddressUpdater::~IpAddressUpdater() = default;

winrt::hstring IpAddressUpdater::GetIpAddress()
{
    std::lock_guard lockGuard(m_lock);
    return m_ipAddress;
}

void IpAddressUpdater::UpdateIpAddress(winrt::Windows::Foundation::IInspectable sender)
{
    winrt::hstring ipAddress = L"(No Network Connection)";
    winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Networking::HostName> hostnames =
        winrt::Windows::Networking::Connectivity::NetworkInformation::GetHostNames();

    for (winrt::Windows::Networking::HostName hostname : hostnames)
    {
        if (hostname.IPInformation() && hostname.IPInformation().NetworkAdapter() && hostname.CanonicalName().size() <= 15) // IPV4 only
        {
            ipAddress = hostname.CanonicalName();
            break;
        }
    }

    std::lock_guard lockGuard(m_lock);
    m_ipAddress = ipAddress;
};
