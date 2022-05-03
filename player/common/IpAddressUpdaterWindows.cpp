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

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Networking.Connectivity.h>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

using namespace winrt::Windows::Networking;
using namespace winrt::Windows::Networking::Connectivity;

class IpAddressUpdaterWindows : public IIpAddressUpdater, public std::enable_shared_from_this<IpAddressUpdaterWindows>
{
public:
    void Init();

    virtual std::wstring GetIpAddress(bool IPv6) override;

private:
    void UpdateIpAddress(winrt::Windows::Foundation::IInspectable sender);

    std::mutex m_lock;

    winrt::hstring m_ipAddressIpv6;
    winrt::hstring m_ipAddressIpv4;

    NetworkInformation::NetworkStatusChanged_revoker m_networkStatusChangedRevoker;
};

void IpAddressUpdaterWindows::Init()
{
    m_networkStatusChangedRevoker = NetworkInformation::NetworkStatusChanged(
        winrt::auto_revoke, [weak_this = weak_from_this()](winrt::Windows::Foundation::IInspectable sender) {
            if (auto strong_this = weak_this.lock())
            {
                strong_this->UpdateIpAddress(sender);
            }
        });

    UpdateIpAddress(nullptr);
}

std::wstring IpAddressUpdaterWindows::GetIpAddress(bool IPv6)
{
    std::lock_guard lockGuard(m_lock);
    return IPv6 ? m_ipAddressIpv6.c_str() : m_ipAddressIpv4.c_str();
}

void IpAddressUpdaterWindows::UpdateIpAddress(winrt::Windows::Foundation::IInspectable sender)
{
    winrt::hstring ipAddressIpv4 = L"";
    winrt::hstring ipAddressIpv6 = L"";

    try
    {
        Collections::IVectorView<HostName> hostnames = NetworkInformation::GetHostNames();

        for (HostName hostname : hostnames)
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

std::shared_ptr<IIpAddressUpdater> CreateIpAddressUpdater()
{
    auto instance = std::make_shared<IpAddressUpdaterWindows>();
    instance->Init();

    return instance;
}
