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

#include <pch.h>

#include <holographic/SpatialInputHandler.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Input.Spatial.h>

// Creates and initializes a GestureRecognizer that listens to a Person.
SpatialInputHandler::SpatialInputHandler(winrt::Windows::UI::Input::Spatial::SpatialInteractionManager interactionManager)
    : m_interactionManager(interactionManager)
{
    m_gestureRecognizer = winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer(
        winrt::Windows::UI::Input::Spatial::SpatialGestureSettings::Tap |
        winrt::Windows::UI::Input::Spatial::SpatialGestureSettings::ManipulationTranslate);

    m_interactionDetectedEventToken =
        m_interactionManager.InteractionDetected(winrt::Windows::Foundation::TypedEventHandler<
                                                 winrt::Windows::UI::Input::Spatial::SpatialInteractionManager,
                                                 winrt::Windows::UI::Input::Spatial::SpatialInteractionDetectedEventArgs>(
            [this](
                winrt::Windows::UI::Input::Spatial::SpatialInteractionManager,
                winrt::Windows::UI::Input::Spatial::SpatialInteractionDetectedEventArgs args) {
                m_gestureRecognizer.CaptureInteraction(args.Interaction());
            }));

    m_tappedEventToken = m_gestureRecognizer.Tapped(winrt::Windows::Foundation::TypedEventHandler<
                                                    winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                                                    winrt::Windows::UI::Input::Spatial::SpatialTappedEventArgs>(
        [this](
            winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer, winrt::Windows::UI::Input::Spatial::SpatialTappedEventArgs args) {
            std::lock_guard _lg(m_manipulationStateLock);
            m_tapped = args;
        }));

    m_manipulationStartedEventToken =
        m_gestureRecognizer.ManipulationStarted(winrt::Windows::Foundation::TypedEventHandler<
                                                winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                                                winrt::Windows::UI::Input::Spatial::SpatialManipulationStartedEventArgs>(
            [this](
                winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                winrt::Windows::UI::Input::Spatial::SpatialManipulationStartedEventArgs args) {
                std::lock_guard _lg(m_manipulationStateLock);
                m_manipulationStarted = args;
            }));

    m_manipulationUpdatedEventToken =
        m_gestureRecognizer.ManipulationUpdated(winrt::Windows::Foundation::TypedEventHandler<
                                                winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                                                winrt::Windows::UI::Input::Spatial::SpatialManipulationUpdatedEventArgs>(
            [this](
                winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                winrt::Windows::UI::Input::Spatial::SpatialManipulationUpdatedEventArgs args) {
                std::lock_guard _lg(m_manipulationStateLock);
                m_manipulationUpdated = args;
            }));

    m_manipulationCompletedEventToken =
        m_gestureRecognizer.ManipulationCompleted(winrt::Windows::Foundation::TypedEventHandler<
                                                  winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                                                  winrt::Windows::UI::Input::Spatial::SpatialManipulationCompletedEventArgs>(
            [this](
                winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                winrt::Windows::UI::Input::Spatial::SpatialManipulationCompletedEventArgs) {
                std::lock_guard _lg(m_manipulationStateLock);
                m_manipulationResult = ManipulationResult::Completed;
            }));

    m_manipulationCanceledEventToken =
        m_gestureRecognizer.ManipulationCanceled(winrt::Windows::Foundation::TypedEventHandler<
                                                 winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                                                 winrt::Windows::UI::Input::Spatial::SpatialManipulationCanceledEventArgs>(
            [this](
                winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                winrt::Windows::UI::Input::Spatial::SpatialManipulationCanceledEventArgs) {
                std::lock_guard _lg(m_manipulationStateLock);
                m_manipulationResult = ManipulationResult::Canceled;
            }));

    m_navigationStartedEventToken =
        m_gestureRecognizer.NavigationStarted(winrt::Windows::Foundation::TypedEventHandler<
                                              winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                                              winrt::Windows::UI::Input::Spatial::SpatialNavigationStartedEventArgs>(
            [this](
                winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                winrt::Windows::UI::Input::Spatial::SpatialNavigationStartedEventArgs args) {
                char buf[128];
                sprintf_s(
                    buf,
                    "NS: %d %d %d\n",
                    static_cast<int>(args.IsNavigatingX()),
                    static_cast<int>(args.IsNavigatingY()),
                    static_cast<int>(args.IsNavigatingZ()));
                OutputDebugStringA(buf);
            }));

    m_navigationUpdatedEventToken =
        m_gestureRecognizer.NavigationUpdated(winrt::Windows::Foundation::TypedEventHandler<
                                              winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                                              winrt::Windows::UI::Input::Spatial::SpatialNavigationUpdatedEventArgs>(
            [this](
                winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                winrt::Windows::UI::Input::Spatial::SpatialNavigationUpdatedEventArgs args) {
                winrt::Windows::Foundation::Numerics::float3 offset = args.NormalizedOffset();
                char buf[128];
                sprintf_s(buf, "NU: %f %f %f\n", offset.x, offset.y, offset.z);
                OutputDebugStringA(buf);
            }));

    m_navigationCompletedEventToken =
        m_gestureRecognizer.NavigationCompleted(winrt::Windows::Foundation::TypedEventHandler<
                                                winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                                                winrt::Windows::UI::Input::Spatial::SpatialNavigationCompletedEventArgs>(
            [this](
                winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                winrt::Windows::UI::Input::Spatial::SpatialNavigationCompletedEventArgs args) {
                winrt::Windows::Foundation::Numerics::float3 offset = args.NormalizedOffset();
                char buf[128];
                sprintf_s(buf, "NC: %f %f %f\n", offset.x, offset.y, offset.z);
                OutputDebugStringA(buf);
            }));

    m_navigationCanceledEventToken =
        m_gestureRecognizer.NavigationCanceled(winrt::Windows::Foundation::TypedEventHandler<
                                               winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                                               winrt::Windows::UI::Input::Spatial::SpatialNavigationCanceledEventArgs>(
            [this](
                winrt::Windows::UI::Input::Spatial::SpatialGestureRecognizer,
                winrt::Windows::UI::Input::Spatial::SpatialNavigationCanceledEventArgs args) {
                char buf[128];
                sprintf_s(buf, "N: canceled\n");
                OutputDebugStringA(buf);
            }));
}

SpatialInputHandler::~SpatialInputHandler()
{
    // Unregister our handler for the OnSourcePressed event.
    m_interactionManager.InteractionDetected(m_interactionDetectedEventToken);
    m_gestureRecognizer.Tapped(m_tappedEventToken);
    m_gestureRecognizer.ManipulationStarted(m_manipulationStartedEventToken);
    m_gestureRecognizer.ManipulationUpdated(m_manipulationUpdatedEventToken);
    m_gestureRecognizer.ManipulationCompleted(m_manipulationCompletedEventToken);
    m_gestureRecognizer.ManipulationCanceled(m_manipulationCanceledEventToken);
}

// Checks if the user performed an input gesture since the last call to this method.
// Allows the main update loop to check for asynchronous changes to the user
// input state.
winrt::Windows::UI::Input::Spatial::SpatialTappedEventArgs SpatialInputHandler::CheckForTapped()
{
    std::lock_guard _lg(m_manipulationStateLock);
    auto tapped = m_tapped;
    m_tapped = nullptr;
    return tapped;
}

winrt::Windows::UI::Input::Spatial::SpatialManipulationStartedEventArgs SpatialInputHandler::CheckForManipulationStarted()
{
    std::lock_guard _lg(m_manipulationStateLock);
    auto manipulationStarted = m_manipulationStarted;
    m_manipulationStarted = nullptr;
    return manipulationStarted;
}

winrt::Windows::UI::Input::Spatial::SpatialManipulationUpdatedEventArgs SpatialInputHandler::CheckForManipulationUpdated()
{
    std::lock_guard _lg(m_manipulationStateLock);
    auto manipulationUpdated = m_manipulationUpdated;
    m_manipulationUpdated = nullptr;
    return manipulationUpdated;
}

SpatialInputHandler::ManipulationResult SpatialInputHandler::CheckForManipulationResult()
{
    std::lock_guard _lg(m_manipulationStateLock);
    auto manipulationResult = m_manipulationResult;
    m_manipulationResult = ManipulationResult::Unknown;
    return manipulationResult;
}
