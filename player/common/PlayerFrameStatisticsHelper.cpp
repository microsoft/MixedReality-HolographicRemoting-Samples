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

#include "PlayerFrameStatisticsHelper.h"

#include <sstream>


using namespace winrt::Microsoft::Holographic::AppRemoting;


std::wstring PlayerFrameStatisticsHelper::GetStatisticsString() const
{
    float timeSinceLastPresentAvg = 0.0f;
    float timeSinceLastPresentMax = 0.0f;
    uint32_t videoFramesSkipped = 0;
    uint32_t videoFramesReused = 0;
    uint32_t videoFramesReceived = 0;
    float videoFrameMinDelta = 0.0f;
    float videoFrameMaxDelta = 0.0f;
    float latencyAvg = 0.0f;
    uint32_t videoFramesDiscarded = 0;


    for (const PlayerFrameStatistics& frameStatistics : m_lastWindowFrameStats)
    {
        timeSinceLastPresentAvg += frameStatistics.TimeSinceLastPresent;
        timeSinceLastPresentMax = std::max<>(timeSinceLastPresentMax, frameStatistics.TimeSinceLastPresent);

        videoFramesSkipped += frameStatistics.VideoFramesSkipped;
        videoFramesReused += frameStatistics.VideoFrameReusedCount > 0 ? 1 : 0;
        videoFramesReceived += frameStatistics.VideoFramesReceived;

        if (frameStatistics.VideoFramesReceived > 0)
        {
            if (videoFrameMinDelta == 0.0f)
            {
                videoFrameMinDelta = frameStatistics.VideoFrameMinDelta;
                videoFrameMaxDelta = frameStatistics.VideoFrameMaxDelta;
            }
            else
            {
                videoFrameMinDelta = std::min<>(videoFrameMinDelta, frameStatistics.VideoFrameMinDelta);
                videoFrameMaxDelta = std::max<>(videoFrameMaxDelta, frameStatistics.VideoFrameMaxDelta);
            }
        }

        latencyAvg += frameStatistics.Latency;
        videoFramesDiscarded += frameStatistics.VideoFramesDiscarded;
    }

    const size_t frameStatsCount = m_lastWindowFrameStats.size();
    if (frameStatsCount > 0)
    {
        timeSinceLastPresentAvg /= static_cast<float>(frameStatsCount);
        latencyAvg /= static_cast<float>(frameStatsCount);
    }

    std::wstringstream statisticsStringStream;
    statisticsStringStream.precision(3);
    statisticsStringStream << L"Render: " << frameStatsCount << L" fps - " << timeSinceLastPresentAvg * 1000 << L" / "
                           << timeSinceLastPresentMax * 1000 << L" ms (avg / max)" << std::endl
                           << L"Video frames: " << videoFramesSkipped << L" / " << videoFramesReused << L" / " << videoFramesReceived
                           << L" skipped / reused / received" << std::endl
                           << L"Video frames delta: " << videoFrameMinDelta * 1000 << L" / " << videoFrameMaxDelta * 1000
                           << L" ms (min / max)" << std::endl
                           << L"Latency: " << latencyAvg * 1000 << L" ms (avg)" << std::endl
                           << L"Video frames discarded: " << videoFramesDiscarded << L" / " << m_videoFramesDiscardedTotal
                           << L" frames (last sec / total)" << std::endl;

    return statisticsStringStream.str();
}

void PlayerFrameStatisticsHelper::Update(const PlayerFrameStatistics& frameStatistics)
{
    using namespace std::chrono;

    TimePoint now = Clock::now();
    if (now > m_currWindowStartTime + 1s)
    {
        m_lastWindowFrameStats.swap(m_currWindowFrameStats);
        m_currWindowFrameStats.clear();

        do
        {
            m_currWindowStartTime += 1s;
        } while (now > m_currWindowStartTime + 1s);
    }

    m_currWindowFrameStats.push_back(frameStatistics);
    m_videoFramesDiscardedTotal += frameStatistics.VideoFramesDiscarded;
}
