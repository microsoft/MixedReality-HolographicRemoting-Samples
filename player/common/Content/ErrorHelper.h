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

#include <winrt/Microsoft.Holographic.AppRemoting.h>

class StatusDisplay;

class ErrorHelper
{
public:
    ErrorHelper();

    template <typename F>
    void Update(float deltaTimeInSeconds, F func)
    {
        if (UpdateInternal(deltaTimeInSeconds))
        {
            func();
        }
    }

    void Apply(std::unique_ptr<StatusDisplay>& statusDisplay);

    // Adds an error message to the error display
    void AddError(const std::wstring& message, float timeToShowInSeconds = 10.0f);
    void ClearErrors();

    bool ProcessOnDisconnect(const winrt::Microsoft::Holographic::AppRemoting::ConnectionFailureReason& reason);

private:
    bool UpdateInternal(float deltaTimeInSeconds);

    struct ErrorLine
    {
        std::wstring text = {};
        float timeUntilRemovalInSeconds = 15.0f;
    };

    std::mutex m_lineMutex;
    std::vector<ErrorLine> m_lines;
};
