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
#include <vector>

#include <winrt/Microsoft.Holographic.AppRemoting.h>

// Helper class for PlayerFrameStatistics. Accumulates frame statistics in a 1 second long fixed window and formats it into a readable
// string.
class PlayerFrameStatisticsHelper
{
public:
    // Returns the accumulated statistics of the last 1s fixed window as string.
    std::wstring GetStatisticsString() const;

    // Updates the statistics with the provided statistics data.
    void Update(const winrt::Microsoft::Holographic::AppRemoting::PlayerFrameStatistics& frameStatistics);

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    TimePoint m_currWindowStartTime = Clock::now();
    std::vector<winrt::Microsoft::Holographic::AppRemoting::PlayerFrameStatistics> m_currWindowFrameStats;
    std::vector<winrt::Microsoft::Holographic::AppRemoting::PlayerFrameStatistics> m_lastWindowFrameStats;
    uint32_t m_videoFramesDiscardedTotal = 0;
};
