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
#include <mutex>
#include <vector>
#include <winrt/Windows.UI.Input.Spatial.h>

// Sample gesture handler.
// Hooks up events to recognize a tap gesture, and keeps track of input using a boolean value.
class SpatialInputHandler
{
public:
    SpatialInputHandler(winrt::Windows::UI::Input::Spatial::SpatialInteractionManager interactionManager);
    ~SpatialInputHandler();

    enum class ManipulationResult
    {
        Unknown = 0,
        Completed,
        Canceled,
    };

    winrt::Windows::UI::Input::Spatial::SpatialTappedEventArgs CheckForTapped();
    winrt::Windows::UI::Input::Spatial::SpatialManipulationStartedEventArgs CheckForManipulationStarted();
    winrt::Windows::UI::Input::Spatial::SpatialManipulationUpdatedEventArgs CheckForManipulationUpdated();
    ManipulationResult CheckForManipulationResult();

private:
    // API objects used to process gesture input, and generate gesture events.
    winrt::Windows::UI::Input::Spatial::SpatialInteractionManager m_interactionManager = nullptr;
    winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer m_gestureRecognizer = nullptr;

    // Event registration token.
    winrt::event_token m_interactionDetectedEventToken;
    winrt::event_token m_tappedEventToken;
    winrt::event_token m_manipulationStartedEventToken;
    winrt::event_token m_manipulationUpdatedEventToken;
    winrt::event_token m_manipulationCompletedEventToken;
    winrt::event_token m_manipulationCanceledEventToken;

    winrt::event_token m_navigationStartedEventToken;
    winrt::event_token m_navigationUpdatedEventToken;
    winrt::event_token m_navigationCompletedEventToken;
    winrt::event_token m_navigationCanceledEventToken;

    // Used to indicate that a Pressed input event was received this frame.
    std::recursive_mutex m_manipulationStateLock;

    winrt::Windows::UI::Input::Spatial::SpatialTappedEventArgs m_tapped = nullptr;
    winrt::Windows::UI::Input::Spatial::SpatialManipulationStartedEventArgs m_manipulationStarted = nullptr;
    winrt::Windows::UI::Input::Spatial::SpatialManipulationUpdatedEventArgs m_manipulationUpdated = nullptr;
    ManipulationResult m_manipulationResult = ManipulationResult::Unknown;
};
