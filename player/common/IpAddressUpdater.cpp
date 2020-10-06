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

using namespace winrt::Windows::Networking;
using namespace winrt::Windows::Networking::Connectivity;

IpAddressUpdater::IpAddressUpdater()
{
    m_networkStatusChangedRevoker = winrt::Windows::Networking::Connectivity::NetworkInformation::NetworkStatusChanged(
        winrt::auto_revoke, [this](winrt::Windows::Foundation::IInspectable sender) { UpdateIpAddress(sender); });

    UpdateIpAddress(nullptr);
}

IpAddressUpdater::~IpAddressUpdater() = default;

winrt::hstring IpAddressUpdater::GetIpAddress(bool ipv6)
{
    std::lock_guard lockGuard(m_lock);
    return ipv6 ? m_ipAddressIpv6 : m_ipAddressIpv4;
}

void IpAddressUpdater::UpdateIpAddress(winrt::Windows::Foundation::IInspectable sender)
{
    winrt::hstring ipAddressIpv4 = L"";
    winrt::hstring ipAddressIpv6 = L"";

    try
    {
        winrt::Windows::Foundation::Collections::IVectorView<HostName> hostnames = NetworkInformation::GetHostNames();

        for (winrt::Windows::Networking::HostName hostname : hostnames)
        {
            auto hostNameType = hostname.Type();
            if (hostNameType != HostNameType::Ipv4 && hostNameType != HostNameType::Ipv6)
            {
                continue;
            }

            if (hostname.IPInformation() && hostname.IPInformation().NetworkAdapter())
            {
                if (hostNameType == HostNameType::Ipv6 && ipAddressIpv6.empty())
                {
                    ipAddressIpv6 = hostname.CanonicalName();
                }
                else if (hostNameType == HostNameType::Ipv4 && ipAddressIpv4.empty())
                {
                    ipAddressIpv4 = hostname.CanonicalName();
                }
            }
        }
    }
    catch (winrt::hresult_error&)
    {
        ipAddressIpv4.clear();
        ipAddressIpv6.clear();
    }

    if (ipAddressIpv6.empty())
    {
        ipAddressIpv6 = L"(No Network Connection)";
    }

    if (ipAddressIpv4.empty())
    {
        ipAddressIpv4 = L"(No Network Connection)";
    }

    std::lock_guard lockGuard(m_lock);
    m_ipAddressIpv6 = ipAddressIpv6;
    m_ipAddressIpv4 = ipAddressIpv4;
};
