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

// #define ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE

#include "../common/Content/ErrorHelper.h"
#include "../common/Content/StatusDisplay.h"
#include "../common/DeviceResources.h"
#include "../common/IpAddressUpdater.h"
#include "../common/PlayerFrameStatisticsHelper.h"

#include <winrt/Microsoft.Holographic.AppRemoting.h>



class SamplePlayerMain : public winrt::implements<
                             SamplePlayerMain,
                             winrt::Windows::ApplicationModel::Core::IFrameworkViewSource,
                             winrt::Windows::ApplicationModel::Core::IFrameworkView>,
                         public DXHelper::IDeviceNotify
{
public:
    ~SamplePlayerMain();

    // Try to (re-)connect to or listen on the hostname/port, that was set during activation of the app.
    void ConnectOrListen();

    // Try to (re-)connect to or listen on the hostname/port, that was set during activation of the app after a certain amount of time
    winrt::fire_and_forget ConnectOrListenAfter(std::chrono::system_clock::duration time);

    // Starts the holographic frame and updates the content.
    winrt::Windows::Graphics::Holographic::HolographicFrame Update(float deltaTimeInSeconds);

    // Renders the current frame to each holographic camera and presents it.
    void Render(const winrt::Windows::Graphics::Holographic::HolographicFrame& holographicFrame);

public:
    // IFrameworkViewSource methods
    winrt::Windows::ApplicationModel::Core::IFrameworkView CreateView();

    // IFrameworkView methods
    void Initialize(const winrt::Windows::ApplicationModel::Core::CoreApplicationView& applicationView);
    void SetWindow(const winrt::Windows::UI::Core::CoreWindow& window);
    void Load(const winrt::hstring& entryPoint);
    void Run();
    void Uninitialize();

    // IDeviceNotify methods
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

private:
    // Options for the player which can passed in via activation arguments
    struct PlayerOptions
    {
        winrt::hstring m_hostname = L"0.0.0.0";
        uint16_t m_port = 0;
        bool m_listen = true;
        bool m_showStatistics = false;
    };

private:
    // Load the holographic remoting logo image
    void LoadLogoImage();

    // Parse activation arguments and return result as PlayerOptions struct
    PlayerOptions ParseActivationArgs(const winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs& activationArgs);

    // Setup the text display to show the connection info text
    void UpdateStatusDisplay();

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
    void OnCustomDataChannelDataReceived();
    void OnCustomDataChannelClosed();
#endif

    // PlayerContext event handlers
    void OnConnected();
    void OnDisconnected(winrt::Microsoft::Holographic::AppRemoting::ConnectionFailureReason reason);

    // SpatialLocator event handlers
    void OnLocatabilityChanged(
        const winrt::Windows::Perception::Spatial::SpatialLocator& sender, const winrt::Windows::Foundation::IInspectable& args);

    // Application lifecycle event handlers
    void OnViewActivated(
        const winrt::Windows::ApplicationModel::Core::CoreApplicationView& sender,
        const winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs& args);
    void OnSuspending(
        const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::ApplicationModel::SuspendingEventArgs& args);
    void OnResuming(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& args);

    // CoreWindow event handlers
    void OnVisibilityChanged(
        const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::VisibilityChangedEventArgs& args);
    void OnWindowClosed(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::CoreWindowEventArgs& args);

private:
    // Cached pointer to device resources.
    std::shared_ptr<DXHelper::DeviceResources> m_deviceResources;

    // SpatialLocator that is attached to the default HolographicDisplay.
    winrt::Windows::Perception::Spatial::SpatialLocator m_spatialLocator = nullptr;

    // A attached frame of reference based on m_spatialLocator.
    winrt::Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference m_attachedFrameOfReference = nullptr;

    // PlayerContext used to connect with a Holographic Remoting host and display remotly rendered frames
    winrt::Microsoft::Holographic::AppRemoting::PlayerContext m_playerContext = nullptr;

    // Player options passed in via activation args
    PlayerOptions m_playerOptions = {};

    // Renders a AppRemoting logo together with the connection state and IP address
    std::unique_ptr<StatusDisplay> m_statusDisplay;

    // Texture holding the AppRemoting logo
    winrt::com_ptr<ID3D11Resource> m_logoImage;

    // The IP address of the device the player is running on
    winrt::hstring m_deviceIp = L"127.0.0.1";

    // Monitors and provides the IP address of the device the player is running on
    IpAddressUpdater m_ipAddressUpdater;

    // Accumulates and provides remote frame statistics
    PlayerFrameStatisticsHelper m_statisticsHelper;
    ErrorHelper m_errorHelper;

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
    std::mutex m_customDataChannelLock;
    winrt::Microsoft::Holographic::AppRemoting::IDataChannel m_customDataChannel = nullptr;
    winrt::Microsoft::Holographic::AppRemoting::IDataChannel::OnDataReceived_revoker m_customChannelDataReceivedEventRevoker;
    winrt::Microsoft::Holographic::AppRemoting::IDataChannel::OnClosed_revoker m_customChannelClosedEventRevoker;
#endif

    // Indicates that tracking has been lost
    bool m_trackingLost = false;

    // CoreWindow status
    bool m_windowClosed = false;
    bool m_windowVisible = true;

    // Event registration tokens
    winrt::event_token m_locatabilityChangedToken;
    winrt::event_token m_suspendingEventToken;
    winrt::event_token m_resumingEventToken;
    winrt::event_token m_windowClosedEventToken;
    winrt::event_token m_visibilityChangedEventToken;
};
