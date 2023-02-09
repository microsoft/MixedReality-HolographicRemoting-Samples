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

// Helper class producing and storing statistics summary values
// Template type of input data
template <class T>
class StatisticsHelperSummary;

// Helper class accumulating frame statistics for 1 sec. fixed window,
// and then producing summary values which can be presented as readable strings.
// Template arguments allow to use different input types `T` and Summary types `Summary` for this accumulation logic.
template <class T, class Summary = StatisticsHelperSummary<T>>
class StatisticsHelper
{
public:
    typedef Summary Summary;

    // Returns the accumulated statistics of the last 1s fixed window as string.
    inline std::wstring GetStatisticsString() const
    {
        return m_summary.ToWString();
    }

    inline Summary const& GetStatisticsSummary() const
    {
        return m_summary;
    }

    // Updates the statistics with the provided statistics data.
    void Update(const T& frameStatistics);

    inline bool StatisticsHaveChanged()
    {
        return m_statsHasChanged;
    }

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    TimePoint m_currWindowStartTime = Clock::now();
    std::vector<T> m_currWindowFrameStats;
    std::vector<T> m_lastWindowFrameStats;
    bool m_statsHasChanged = true;
    Summary m_summary;
};

template <class T, class Summary>
void StatisticsHelper<T, Summary>::Update(const T& frameStatistics)
{
    using namespace std::chrono;

    m_statsHasChanged = false;

    TimePoint now = Clock::now();
    if (now > m_currWindowStartTime + 1s)
    {
        m_statsHasChanged = true;

        m_lastWindowFrameStats.swap(m_currWindowFrameStats);
        m_currWindowFrameStats.clear();

        do
        {
            m_currWindowStartTime += 1s;
        } while (now > m_currWindowStartTime + 1s);

        m_summary.BeginUpdate();
        for (auto const& frame : m_lastWindowFrameStats)
        {
            m_summary.UpdateAddFrame(frame);
        }
        m_summary.EndUpdate();
    }

    m_currWindowFrameStats.push_back(frameStatistics);
}

#include <winrt/Microsoft.Holographic.AppRemoting.h>

template <>
class StatisticsHelperSummary<winrt::Microsoft::Holographic::AppRemoting::PlayerFrameStatistics>
{
public:
    void BeginUpdate();
    void UpdateAddFrame(winrt::Microsoft::Holographic::AppRemoting::PlayerFrameStatistics const& frameStatistics);
    void EndUpdate();

    std::wstring ToWString() const;

    float timeSinceLastPresentAvg = 0.0f;
    float timeSinceLastPresentMax = 0.0f;
    uint32_t videoFramesSkipped = 0;
    uint32_t videoFramesReused = 0;
    uint32_t videoFramesReceived = 0;
    float videoFrameMinDelta = 0.0f;
    float videoFrameMaxDelta = 0.0f;
    float latencyAvg = 0.0f;
    uint32_t videoFramesDiscarded = 0;
    uint32_t videoFramesDiscardedTotal = 0;
    uint32_t frameStatsCount = 0;

protected:
    template <class T2>
    void UpdateAddFrameTimeAndVideoInfo(T2 const& frameStatistics);
};

typedef StatisticsHelper<winrt::Microsoft::Holographic::AppRemoting::PlayerFrameStatistics> PlayerFrameStatisticsHelper;

// MakeDropCmd-StripStart

#if defined(HAR_PLATFORM_WINDOWS)

#    include <HolographicAppRemoting/HybridPlayerInterface.h>

template <>
class StatisticsHelperSummary<Microsoft::Holographic::AppRemoting::HybridPlayerFrameStatistics>
    : public StatisticsHelperSummary<winrt::Microsoft::Holographic::AppRemoting::PlayerFrameStatistics>
{
public:
    void BeginUpdate();
    void UpdateAddFrame(Microsoft::Holographic::AppRemoting::HybridPlayerFrameStatistics const& frameStatistics);
    void EndUpdate();

    float latencyPoseToReceiveAvg = 0.0f;
    float latencyReceiveToPresentAvg = 0.0f;
    float latencyPresentToDisplayAvg = 0.0f;
};

#endif

// MakeDropCmd-StripEnd
