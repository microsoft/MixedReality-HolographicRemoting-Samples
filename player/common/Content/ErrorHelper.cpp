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

#include "ErrorHelper.h"

#include "StatusDisplay.h"

using namespace winrt::Microsoft::Holographic::AppRemoting;

ErrorHelper::ErrorHelper()
{
}

void ErrorHelper::Apply(std::unique_ptr<StatusDisplay>& statusDisplay)
{
    std::scoped_lock lock(m_lineMutex);
    for (const auto& line : m_lines)
    {
        statusDisplay->AddLine(StatusDisplay::Line{line.text, StatusDisplay::Small, StatusDisplay::Red});
    }
}

void ErrorHelper::AddError(const std::wstring& message, float timeToShowInSeconds)
{
    std::scoped_lock lock(m_lineMutex);
    m_lines.push_back(ErrorLine{message, timeToShowInSeconds});
}

void ErrorHelper::ClearErrors()
{
    std::scoped_lock lock(m_lineMutex);
    m_lines.clear();
}

bool ErrorHelper::ProcessOnDisconnect(const ConnectionFailureReason& reason)
{
    switch (reason)
    {
        case ConnectionFailureReason::Unknown:
            AddError(L"Disconnect: Unknown reason");
            return true;

        case ConnectionFailureReason::HandshakeUnreachable:
            AddError(L"Disconnect: Handshake server is unreachable");
            return true;

        case ConnectionFailureReason::HandshakeConnectionFailed:
            AddError(L"Disconnect: Handshake server closed the connection prematurely; likely due to TLS/Plain mismatch "
                     L"or invalid certificate");
            return true;

        case ConnectionFailureReason::AuthenticationFailed:
            AddError(L"Disconnect: Authentication with the handshake server failed");
            return true;

        case ConnectionFailureReason::RemotingVersionMismatch:
            AddError(L"Disconnect: No common compatible remoting version could be determined during handshake");
            return true;

        case ConnectionFailureReason::IncompatibleTransportProtocols:
            AddError(L"Disconnect: No common transport protocol could be determined during handshake");
            return true;

        case ConnectionFailureReason::HandshakeFailed:
            AddError(L"Disconnect: Handshake failed for any other reason");
            return true;

        case ConnectionFailureReason::TransportUnreachable:
            AddError(L"Disconnect: Transport server is unreachable");
            return true;

        case ConnectionFailureReason::TransportConnectionFailed:
            AddError(L"Disconnect: Transport connection was closed before all communication channels had been set up");
            return true;

        case ConnectionFailureReason::ProtocolVersionMismatch:
            AddError(L"Disconnect: Transport connection was closed due to protocol version mismatch. "
                     L"Please go to the store app and check for any updates and install them to potentially resolve "
                     L"this error.");
            return true;

        case ConnectionFailureReason::ProtocolError:
            AddError(L"Disconnect: A protocol error occurred that was severe enough to invalidate the current connection "
                     L"or connection attempt");
            return true;

        case ConnectionFailureReason::VideoCodecNotAvailable:
            AddError(L"Disconnect: Transport connection was closed due to the requested video codec not being available");
            return true;

        case ConnectionFailureReason::Canceled:
            AddError(L"Disconnect: Connection attempt has been canceled");
            return true;

        case ConnectionFailureReason::ConnectionLost:
            AddError(L"Disconnect: Connection has been lost or closed by remote side");
            return false;

        case ConnectionFailureReason::DeviceLost:
            AddError(L"Disconnect: Connection has been closed due to graphics device loss");
            return true;

        case ConnectionFailureReason::HandshakeNetworkUnreachable:
            AddError(L"Disconnect: Handshake - Network unreachable");
            return true;

        case ConnectionFailureReason::HandshakeConnectionRefused:
            AddError(L"Disconnect: Handshake - Connection has been refused by remote host");
            return true;

        case ConnectionFailureReason::VideoFormatNotAvailable:
            AddError(L"Disconnect: Transport connection was closed due to the requested video format not being available");
            return true;
    }

    return false;
}

bool ErrorHelper::UpdateInternal(float deltaTimeInSeconds)
{
    std::scoped_lock lock(m_lineMutex);

    bool linesRemoved = false;

    auto itErrorLines = m_lines.begin();
    while (itErrorLines != m_lines.end())
    {
        itErrorLines->timeUntilRemovalInSeconds -= deltaTimeInSeconds;
        if (itErrorLines->timeUntilRemovalInSeconds <= 0.0f)
        {
            itErrorLines = m_lines.erase(itErrorLines);
            linesRemoved = true;

            continue;
        }

        ++itErrorLines;
    }

    if (linesRemoved)
    {
        return true;
    }

    return false;
}
