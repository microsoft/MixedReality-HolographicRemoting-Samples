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

#include "SamplePlayerMain.h"

#include "../common/Content/DDSTextureLoader.h"

#include <sstream>

using namespace std::chrono_literals;

using namespace winrt::Microsoft::Holographic::AppRemoting;
using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Input::Spatial;


SamplePlayerMain::~SamplePlayerMain()
{
    Uninitialize();
}

void SamplePlayerMain::ConnectOrListen()
{
    // Disconnect from a potentially existing connection first
    m_playerContext.Disconnect();

    UpdateStatusDisplay();

    // Try to establish a connection as specified in m_playerOptions
    try
    {

        const uint16_t port = (m_playerOptions.m_port != 0) ? m_playerOptions.m_port : 8265;

        if (m_playerOptions.m_listen)
        {
            // Put the PlayerContext in network server mode. In this mode the player listens for an incoming network connection.
            // The hostname specifies the local address on which the player listens on.
            // Use the port as the handshake port (where clients always connect to first), and port + 1 for the
            // primary transport implementation (clients are redirected to this port as part of the handshake).
            m_playerContext.Listen(m_playerOptions.m_hostname, port, port + 1);
        }
        else
        {
            // Put the PlayerContext in network client mode.
            // In this mode the player tries to establish a network connection to the provided hostname at the given port.
            // The port specifies the server's handshake port. The primary transport port will be specified by the server as part of the
            // handshake.
            m_playerContext.Connect(m_playerOptions.m_hostname, port);
        }
    }
    catch (winrt::hresult_error& ex)
    {
        // If Connect/Listen fails, display the error message
        // Possible reasons for this are invalid parameters or because the PlayerContext is already in connected or connecting state.
        m_errorHelper.AddError(
            std::wstring(m_playerOptions.m_listen ? L"Failed to Listen: " : L"Failed to Connect: ") + std::wstring(ex.message().c_str()));
        ConnectOrListenAfter(1s);
    }

    UpdateStatusDisplay();
}

winrt::fire_and_forget SamplePlayerMain::ConnectOrListenAfter(std::chrono::system_clock::duration time)
{
    // Get a weak reference before switching to a background thread.
    auto weakThis = get_weak();

    // Continue after the given time in a background thread
    using namespace winrt;
    co_await time;

    // Return if the player has been destroyed in the meantime
    auto strongThis = weakThis.get();
    if (!strongThis)
    {
        return;
    }

    // Try to connect or listen
    ConnectOrListen();
}

HolographicFrame SamplePlayerMain::Update(float deltaTimeInSeconds)
{
    const bool connected = (m_playerContext.ConnectionState() == ConnectionState::Connected);

    HolographicSpace holographicSpace = m_deviceResources->GetHolographicSpace();
    HolographicFrame holographicFrame = holographicSpace.CreateNextFrame();
    HolographicFramePrediction prediction = holographicFrame.CurrentPrediction();

    // Back buffers can change from frame to frame. Validate each buffer, and recreate resource views and depth buffers as needed.
    m_deviceResources->EnsureCameraResources(holographicFrame, prediction);

    // Update the accumulated statistics with the statistics from the last frame
    m_statisticsHelper.Update(m_playerContext.LastFrameStatistics());

    // Update the position of the status and error display
    SpatialCoordinateSystem coordinateSystem = nullptr;
    if (m_attachedFrameOfReference != nullptr)
    {
        coordinateSystem = m_attachedFrameOfReference.GetStationaryCoordinateSystemAtTimestamp(prediction.Timestamp());
        SpatialPointerPose pose = SpatialPointerPose::TryGetAtTimestamp(coordinateSystem, prediction.Timestamp());
        if (pose)
        {
            m_statusDisplay->PositionDisplay(deltaTimeInSeconds, pose);

            for (const HolographicCameraPose& cameraPose : prediction.CameraPoses())
            {
                HolographicCameraRenderingParameters renderingParameters = holographicFrame.GetRenderingParameters(cameraPose);

                // Set the focus point for image stabilization to the center of the status display.
                // NOTE: By doing this before the call to PlayerContext::BlitRemoteFrame (in the Render() method),
                //       the focus point can be overriden by the remote side.
                renderingParameters.SetFocusPoint(coordinateSystem, m_statusDisplay->GetPosition());
            }
        }
    }

    // Update the content of the status and error display
    if (connected && !m_trackingLost)
    {
        if (m_playerOptions.m_showStatistics)
        {
            const std::wstring statisticsString = m_statisticsHelper.GetStatisticsString();

            if (m_statusDisplay->HasLine(0))
            {
                m_statusDisplay->UpdateLineText(0, std::move(statisticsString));
            }
            else
            {
                StatusDisplay::Line line = {std::move(statisticsString), StatusDisplay::Small, StatusDisplay::Yellow, 1.0f, true};
                m_statusDisplay->AddLine(line);
            }
        }
    }
    else
    {
        if (m_playerOptions.m_listen)
        {
            auto deviceIpNew = m_ipAddressUpdater.GetIpAddress();
            if (m_deviceIp != deviceIpNew)
            {
                m_deviceIp = deviceIpNew;

                UpdateStatusDisplay();
            }
        }
    }
    m_statusDisplay->SetImageEnabled(!connected || m_trackingLost);
    m_statusDisplay->Update(deltaTimeInSeconds);

    m_errorHelper.Update(deltaTimeInSeconds, [this]() { UpdateStatusDisplay(); });

    return holographicFrame;
}

void SamplePlayerMain::Render(const HolographicFrame& holographicFrame)
{
    bool atLeastOneCameraRendered = false;

    m_deviceResources->UseHolographicCameraResources([this, holographicFrame, &atLeastOneCameraRendered](
                                                         std::map<UINT32, std::unique_ptr<DXHelper::CameraResources>>& cameraResourceMap) {
        holographicFrame.UpdateCurrentPrediction();

        HolographicFramePrediction prediction = holographicFrame.CurrentPrediction();

        SpatialCoordinateSystem coordinateSystem = nullptr;
        if (m_attachedFrameOfReference)
        {
            coordinateSystem = m_attachedFrameOfReference.GetStationaryCoordinateSystemAtTimestamp(prediction.Timestamp());
        }

        for (const HolographicCameraPose& cameraPose : prediction.CameraPoses())
        {
            DXHelper::CameraResources* pCameraResources = cameraResourceMap[cameraPose.HolographicCamera().Id()].get();

            m_deviceResources->UseD3DDeviceContext([&](ID3D11DeviceContext3* deviceContext) {
                ID3D11DepthStencilView* depthStencilView = pCameraResources->GetDepthStencilView();

                // Set render targets to the current holographic camera.
                ID3D11RenderTargetView* const targets[1] = {pCameraResources->GetBackBufferRenderTargetView()};
                deviceContext->OMSetRenderTargets(1, targets, depthStencilView);

                if (!targets[0] || !depthStencilView)
                {
                    return;
                }

                // Clear the back buffer and depth stencil view.
                deviceContext->ClearRenderTargetView(targets[0], DirectX::Colors::Transparent);
                deviceContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

                // The view and projection matrices for each holographic camera will change
                // every frame. This function refreshes the data in the constant buffer for
                // the holographic camera indicated by cameraPose.
                if (coordinateSystem)
                {
                    pCameraResources->UpdateViewProjectionBuffer(m_deviceResources, cameraPose, coordinateSystem);
                }

                // Attach the view/projection constant buffer for this camera to the graphics pipeline.
                bool cameraActive = pCameraResources->AttachViewProjectionBuffer(m_deviceResources);

                // Only render world-locked content when positional tracking is active.
                if (cameraActive)
                {
                    try
                    {
                        if (m_playerContext.ConnectionState() == ConnectionState::Connected)
                        {
                            // Blit the remote frame into the backbuffer for the HolographicFrame.
                            // NOTE: This overwrites the focus point for the current frame, if the remote application
                            // has specified a focus point during the rendering of the remote frame.
                            m_playerContext.BlitRemoteFrame();
                        }
                    }
                    catch (winrt::hresult_error err)
                    {
                        winrt::hstring msg = err.message();
                        m_errorHelper.AddError(std::wstring(L"BlitRemoteFrame failed: ") + msg.c_str());
                        UpdateStatusDisplay();
                    }

                    // Draw connection status and/or statistics.
                    m_statusDisplay->Render();
                }

                atLeastOneCameraRendered = true;
            });
        }
    });

    if (atLeastOneCameraRendered)
    {
        m_deviceResources->Present(holographicFrame);
    }
}

#pragma region IFrameworkViewSource methods

IFrameworkView SamplePlayerMain::CreateView()
{
    return *this;
}

#pragma endregion IFrameworkViewSource methods

#pragma region IFrameworkView methods

void SamplePlayerMain::Initialize(const CoreApplicationView& applicationView)
{
    applicationView.Activated({this, &SamplePlayerMain::OnViewActivated});

    // Register event handlers for app lifecycle.
    m_suspendingEventRevoker = CoreApplication::Suspending(winrt::auto_revoke, {this, &SamplePlayerMain::OnSuspending});
    m_resumingEventRevoker = CoreApplication::Resuming(winrt::auto_revoke, {this, &SamplePlayerMain::OnResuming});

    m_deviceResources = std::make_shared<DXHelper::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);

    m_spatialLocator = SpatialLocator::GetDefault();
    if (m_spatialLocator != nullptr)
    {
        m_locatabilityChangedRevoker =
            m_spatialLocator.LocatabilityChanged(winrt::auto_revoke, {this, &SamplePlayerMain::OnLocatabilityChanged});
        m_attachedFrameOfReference = m_spatialLocator.CreateAttachedFrameOfReferenceAtCurrentHeading();
    }

    // Create the player context
    // IMPORTANT: This must be done before creating the HolographicSpace (or any other call to the Holographic API).
    m_playerContext = PlayerContext::Create();

    // Register to the PlayerContext connection events
    m_playerContext.OnConnected({this, &SamplePlayerMain::OnConnected});
    m_playerContext.OnDisconnected({this, &SamplePlayerMain::OnDisconnected});

    // Set the BlitRemoteFrame timeout to 0.5s
    m_playerContext.BlitRemoteFrameTimeout(500ms);
}

void SamplePlayerMain::SetWindow(const CoreWindow& window)
{
    m_windowClosedEventRevoker = window.Closed(winrt::auto_revoke, {this, &SamplePlayerMain::OnWindowClosed});
    m_visibilityChangedEventRevoker = window.VisibilityChanged(winrt::auto_revoke, {this, &SamplePlayerMain::OnVisibilityChanged});

    // Forward the window to the device resources, so that it can create a holographic space for the window.
    m_deviceResources->SetWindow(window);

    // Initialize the status display.
    m_statusDisplay = std::make_unique<StatusDisplay>(m_deviceResources);

    LoadLogoImage();

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
    try
    {
        m_playerContext.OnDataChannelCreated([this](const IDataChannel& dataChannel, uint8_t channelId) {
            std::lock_guard lock(m_customDataChannelLock);
            m_customDataChannel = dataChannel;

            m_customChannelDataReceivedEventRevoker = m_customDataChannel.OnDataReceived(
                winrt::auto_revoke, [this](winrt::array_view<const uint8_t> dataView) { OnCustomDataChannelDataReceived(); });

            m_customChannelClosedEventRevoker = m_customDataChannel.OnClosed(winrt::auto_revoke, [this]() { OnCustomDataChannelClosed(); });
        });
    }
    catch (winrt::hresult_error err)
    {
        winrt::hstring msg = err.message();
        m_errorHelper.AddError(std::wstring(L"OnDataChannelCreated failed: ") + msg.c_str());
        UpdateStatusDisplay();
    }
#endif
}

void SamplePlayerMain::Load(const winrt::hstring& entryPoint)
{
}

void SamplePlayerMain::Run()
{
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    Clock clock;
    TimePoint timeLastUpdate = clock.now();

    while (!m_windowClosed)
    {
        TimePoint timeCurrUpdate = clock.now();
        Duration timeSinceLastUpdate = timeCurrUpdate - timeLastUpdate;
        float deltaTimeInSeconds = std::chrono::duration<float>(timeSinceLastUpdate).count();

        if (m_windowVisible && (m_deviceResources->GetHolographicSpace() != nullptr))
        {
            CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

            HolographicFrame holographicFrame = Update(deltaTimeInSeconds);

            Render(holographicFrame);
        }
        else
        {
            CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
        }

        timeLastUpdate = timeCurrUpdate;
    }
}

void SamplePlayerMain::Uninitialize()
{
#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
    OnCustomDataChannelClosed();
#endif

    if (m_deviceResources)
    {
        m_deviceResources->RegisterDeviceNotify(nullptr);
        m_deviceResources.reset();
    }

    m_locatabilityChangedRevoker.revoke();
    m_suspendingEventRevoker.revoke();
    m_resumingEventRevoker.revoke();
    m_windowClosedEventRevoker.revoke();
    m_visibilityChangedEventRevoker.revoke();
}

#pragma endregion IFrameworkView methods

#pragma region IDeviceNotify methods

void SamplePlayerMain::OnDeviceLost()
{
    m_logoImage = nullptr;

    m_statusDisplay->ReleaseDeviceDependentResources();

    // Request application restart and provide current player options to the new application instance
    std::wstringstream argsStream;
    argsStream << m_playerOptions.m_hostname.c_str() << L":" << m_playerOptions.m_port;
    if (m_playerOptions.m_listen)
    {
        argsStream << L" -listen";
    }
    if (m_playerOptions.m_showStatistics)
    {
        argsStream << L" -stats";
    }

    winrt::hstring args = argsStream.str().c_str();
    winrt::Windows::ApplicationModel::Core::CoreApplication::RequestRestartAsync(args);
}

void SamplePlayerMain::OnDeviceRestored()
{
    m_statusDisplay->CreateDeviceDependentResources();

    LoadLogoImage();
}

#pragma endregion IDeviceNotify methods

void SamplePlayerMain::LoadLogoImage()
{
    m_logoImage = nullptr;

    winrt::com_ptr<ID3D11ShaderResourceView> logoView;
    winrt::check_hresult(
        DirectX::CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"RemotingLogo.dds", m_logoImage.put(), logoView.put()));

    m_statusDisplay->SetImage(logoView);
}

SamplePlayerMain::PlayerOptions SamplePlayerMain::ParseActivationArgs(const IActivatedEventArgs& activationArgs)
{
    std::wstring host = L"";
    int32_t port = 0;
    bool listen = false;
    bool showStatistics = false;

    if (activationArgs != nullptr)
    {
        ActivationKind activationKind = activationArgs.Kind();
        switch (activationKind)
        {
            case Activation::ActivationKind::Launch:
            {
                LaunchActivatedEventArgs launchArgs = activationArgs.as<LaunchActivatedEventArgs>();

                std::vector<std::wstring> args;
                std::wistringstream stream(std::wstring(launchArgs.Arguments()));
                std::copy(
                    std::istream_iterator<std::wstring, wchar_t>(stream),
                    std::istream_iterator<std::wstring, wchar_t>(),
                    std::back_inserter(args));

                for (const std::wstring& arg : args)
                {
                    if (arg.size() == 0)
                        continue;

                    if (arg[0] == '-')
                    {
                        std::wstring param = arg.substr(1);
                        std::transform(param.begin(), param.end(), param.begin(), ::tolower);

                        if (param == L"stats")
                        {
                            showStatistics = true;
                        }

                        if (param == L"listen")
                        {
                            listen = true;
                        }

                        continue;
                    }

                    size_t colonPos = arg.find(L':');
                    if (colonPos != std::wstring::npos)
                    {
                        std::wstring portStr = arg.substr(colonPos + 1);

                        host = arg.substr(0, colonPos);
                        port = std::wcstol(portStr.c_str(), nullptr, 10);
                    }
                    else
                    {
                        host = arg.c_str();
                        port = 0;
                    }
                }
                break;
            }

            case Activation::ActivationKind::Protocol:
            {
                ProtocolActivatedEventArgs protocolArgs = activationArgs.as<ProtocolActivatedEventArgs>();
                if (auto uri = protocolArgs.Uri())
                {
                    host = uri.Host();
                    port = uri.Port();

                    if (auto query = uri.QueryParsed())
                    {
                        try
                        {
                            winrt::hstring statsValue = query.GetFirstValueByName(L"stats");
                            showStatistics = true;
                        }
                        catch (...)
                        {
                        }

                        try
                        {
                            winrt::hstring statsValue = query.GetFirstValueByName(L"listen");
                            listen = true;
                        }
                        catch (...)
                        {
                        }
                    }
                }
                break;
            }
        }
    }

    // check for invalid port numbers
    if (port < 0 || port > 65535)
    {
        port = 0;
    }

    winrt::hstring hostname = host.c_str();
    if (hostname.empty())
    {
        if (listen == m_playerOptions.m_listen)
        {
            // continue to use old hostname
            hostname = m_playerOptions.m_hostname;
        }

        else
        {
            // default to listen (as we can't connect to an unspecified host)
            hostname = L"0.0.0.0";
            listen = true;
        }
    }

    PlayerOptions playerOptions;
    playerOptions.m_hostname = hostname;
    playerOptions.m_port = port;
    playerOptions.m_listen = listen;
    playerOptions.m_showStatistics = showStatistics;

    return playerOptions;
}

void SamplePlayerMain::UpdateStatusDisplay()
{
    m_statusDisplay->ClearLines();

    if (m_trackingLost)
    {
        StatusDisplay::Line lines[] = {StatusDisplay::Line{L"Device Tracking Lost", StatusDisplay::LargeBold, StatusDisplay::White, 1.2f},
                                       StatusDisplay::Line{L"Ensure your environment is properly lit\r\n"
                                                           L"and the device's sensors are not covered.",
                                                           StatusDisplay::Small,
                                                           StatusDisplay::White,
                                                           6.0f}};
        m_statusDisplay->SetLines(lines);
    }
    else
    {
        if (m_playerContext.ConnectionState() != ConnectionState::Connected)
        {
            StatusDisplay::Line lines[] = {
                StatusDisplay::Line{L"Sample Holographic Remoting Player", StatusDisplay::LargeBold, StatusDisplay::White, 1.2f},
                StatusDisplay::Line{L"This app is a sample companion for Holographic Remoting apps.\r\n"
                                    L"Connect from a compatible app to begin.",
                                    StatusDisplay::Small,
                                    StatusDisplay::White,
                                    6.0f},
                StatusDisplay::Line{m_playerOptions.m_listen ? L"Waiting for connection on" : L"Connecting to",
                                    StatusDisplay::Large,
                                    StatusDisplay::White}};
            m_statusDisplay->SetLines(lines);

            std::wostringstream addressLine;
            addressLine << (m_playerOptions.m_listen ? m_deviceIp.c_str() : m_playerOptions.m_hostname.c_str());
            if (m_playerOptions.m_port)
            {
                addressLine << L":" << m_playerOptions.m_port;
            }
            m_statusDisplay->AddLine(StatusDisplay::Line{addressLine.str(), StatusDisplay::LargeBold, StatusDisplay::White});
        }
    }

    if (m_playerOptions.m_showStatistics)
    {
        m_statusDisplay->AddLine(StatusDisplay::Line{L"Diagnostics Enabled", StatusDisplay::Small, StatusDisplay::Yellow});
    }

    m_errorHelper.Apply(m_statusDisplay);
}

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
void SamplePlayerMain::OnCustomDataChannelDataReceived()
{
    // TODO: React on data received via the custom data channel here.

    // For example: Send back artificial response
    std::lock_guard customDataChannelLockGuard(m_customDataChannelLock);
    if (m_customDataChannel)
    {
        uint8_t data[] = {1};
        m_customDataChannel.SendData(data, true);
    }
}

void SamplePlayerMain::OnCustomDataChannelClosed()
{
    std::lock_guard customDataChannelLockGuard(m_customDataChannelLock);
    if (m_customDataChannel)
    {
        m_customChannelDataReceivedEventRevoker.revoke();
        m_customChannelClosedEventRevoker.revoke();
        m_customDataChannel = nullptr;
    }
}
#endif

void SamplePlayerMain::OnConnected()
{
    m_errorHelper.ClearErrors();
    UpdateStatusDisplay();
}

void SamplePlayerMain::OnDisconnected(ConnectionFailureReason reason)
{
    m_errorHelper.ClearErrors();
    bool error = m_errorHelper.ProcessOnDisconnect(reason);

    UpdateStatusDisplay();

    if (error)
    {
        ConnectOrListenAfter(1s);
        return;
    }

    // Reconnect immediately if not an error unless disconnect was requested.
    if (reason != ConnectionFailureReason::DisconnectRequest)
    {
        // Try to reconnect
        ConnectOrListen();
    }
}

#pragma region Spatial locator event handlers

void SamplePlayerMain::OnLocatabilityChanged(const SpatialLocator& sender, const winrt::Windows::Foundation::IInspectable& args)
{
    bool wasTrackingLost = m_trackingLost;

    switch (sender.Locatability())
    {
        case SpatialLocatability::PositionalTrackingActive:
            m_trackingLost = false;
            break;

        default:
            m_trackingLost = true;
            break;
    }

    if (m_statusDisplay && m_trackingLost != wasTrackingLost)
    {
        UpdateStatusDisplay();
    }
}

#pragma endregion Spatial locator event handlers

#pragma region Application lifecycle event handlers

void SamplePlayerMain::OnViewActivated(const CoreApplicationView& sender, const IActivatedEventArgs& activationArgs)
{
    PlayerOptions playerOptionsNew = ParseActivationArgs(activationArgs);

    bool connectionOptionsChanged =
        (playerOptionsNew.m_listen != m_playerOptions.m_listen || playerOptionsNew.m_hostname != m_playerOptions.m_hostname ||
         playerOptionsNew.m_port != m_playerOptions.m_port);

    m_playerOptions = playerOptionsNew;

    bool disconnected = m_playerContext.ConnectionState() == ConnectionState::Disconnected;
    if (disconnected || connectionOptionsChanged)
    {
        // Try to connect to or listen on the provided hostname/port
        ConnectOrListen();
    }
    else
    {
        UpdateStatusDisplay();
    }

    sender.CoreWindow().Activate();
}

void SamplePlayerMain::OnSuspending(const winrt::Windows::Foundation::IInspectable& sender, const SuspendingEventArgs& args)
{
    m_deviceResources->Trim();

    // Disconnect when app is about to suspend.
    if (m_playerContext.ConnectionState() != ConnectionState::Disconnected)
    {
        m_playerContext.Disconnect();
    }
}

void SamplePlayerMain::OnResuming(
    const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& args)
{
    // (Re-)connect when the app resumes.
    ConnectOrListen();
}

#pragma endregion Application lifecycle event handlers

#pragma region Window event handlers

void SamplePlayerMain::OnVisibilityChanged(const CoreWindow& sender, const VisibilityChangedEventArgs& args)
{
    m_windowVisible = args.Visible();
}

void SamplePlayerMain::OnWindowClosed(const CoreWindow& sender, const CoreWindowEventArgs& args)
{
    m_windowClosed = true;
}

#pragma endregion Window event handlers

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment();
    CoreApplication::Run(SamplePlayerMain());
    return 0;
}
