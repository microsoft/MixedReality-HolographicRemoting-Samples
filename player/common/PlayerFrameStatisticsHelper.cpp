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

void StatisticsHelperSummary<PlayerFrameStatistics>::BeginUpdate()
{
    frameStatsCount = 0;
    timeSinceLastPresentAvg = 0.0f;
    timeSinceLastPresentMax = 0.0f;
    videoFramesSkipped = 0;
    videoFramesReused = 0;
    videoFramesReceived = 0;
    videoFrameMinDelta = 0.0f;
    videoFrameMaxDelta = 0.0f;
    latencyAvg = 0.0f;
    videoFramesDiscarded = 0;
}

void StatisticsHelperSummary<PlayerFrameStatistics>::UpdateAddFrame(
    winrt::Microsoft::Holographic::AppRemoting::PlayerFrameStatistics const& frameStatistics)
{
    UpdateAddFrameTimeAndVideoInfo(frameStatistics);
    latencyAvg += frameStatistics.Latency;
}

void StatisticsHelperSummary<PlayerFrameStatistics>::EndUpdate()
{
    if (frameStatsCount > 0)
    {
        timeSinceLastPresentAvg /= static_cast<float>(frameStatsCount);
        latencyAvg /= static_cast<float>(frameStatsCount);
    }

    videoFramesDiscardedTotal += videoFramesDiscarded;
}

std::wstring StatisticsHelperSummary<PlayerFrameStatistics>::ToWString() const
{
    std::wstringstream statisticsStringStream;
    statisticsStringStream.precision(3);
    statisticsStringStream << L"Render: " << frameStatsCount << L" fps - " << timeSinceLastPresentAvg * 1000 << L" / "
                           << timeSinceLastPresentMax * 1000 << L" ms (avg/max)" << std::endl
                           << L"Video frames: " << videoFramesSkipped << L" / " << videoFramesReused << L" / " << videoFramesReceived
                           << L" skipped/reused/received" << std::endl
                           << L"Video frames delta: " << videoFrameMinDelta * 1000 << L" / " << videoFrameMaxDelta * 1000
                           << L" ms (min/max)" << std::endl
                           << L"Latency: " << latencyAvg * 1000 << L" ms (avg)" << std::endl
                           << L"Video frames discarded: " << videoFramesDiscarded << L" / " << videoFramesDiscardedTotal
                           << L" frames (last sec/total)" << std::endl;

    return statisticsStringStream.str();
}

template <class T2>
void StatisticsHelperSummary<PlayerFrameStatistics>::UpdateAddFrameTimeAndVideoInfo(T2 const& frameStatistics)
{
    frameStatsCount++;

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

    videoFramesDiscarded += frameStatistics.VideoFramesDiscarded;
}

// MakeDropCmd-StripStart

#if defined(HAR_PLATFORM_WINDOWS)

using namespace Microsoft::Holographic::AppRemoting;

void StatisticsHelperSummary<HybridPlayerFrameStatistics>::BeginUpdate()
{
    StatisticsHelperSummary<PlayerFrameStatistics>::BeginUpdate();
    latencyPoseToReceiveAvg = 0.0f;
    latencyReceiveToPresentAvg = 0.0f;
    latencyPresentToDisplayAvg = 0.0f;
}

void StatisticsHelperSummary<HybridPlayerFrameStatistics>::UpdateAddFrame(
    Microsoft::Holographic::AppRemoting::HybridPlayerFrameStatistics const& frameStatistics)
{
    StatisticsHelperSummary<PlayerFrameStatistics>::UpdateAddFrameTimeAndVideoInfo(frameStatistics);
    latencyPoseToReceiveAvg += frameStatistics.LatencyPoseToReceive;
    latencyReceiveToPresentAvg += frameStatistics.LatencyReceiveToPresent;
    latencyPresentToDisplayAvg += frameStatistics.LatencyPresentToDisplay;
}

void StatisticsHelperSummary<HybridPlayerFrameStatistics>::EndUpdate()
{
    if (frameStatsCount > 0)
    {
        latencyPoseToReceiveAvg /= static_cast<float>(frameStatsCount);
        latencyReceiveToPresentAvg /= static_cast<float>(frameStatsCount);
        latencyPresentToDisplayAvg /= static_cast<float>(frameStatsCount);
    }
    StatisticsHelperSummary<PlayerFrameStatistics>::EndUpdate();
    latencyAvg = latencyPoseToReceiveAvg + latencyReceiveToPresentAvg + latencyPresentToDisplayAvg;
}

#endif

// MakeDropCmd-StripEnd
