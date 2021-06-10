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

#include "../common/CameraResources.h"
#include "../common/Content/DDSTextureLoader.h"
#include "../common/PlayerUtil.h"

#include <sstream>

#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Ui.Popups.h>

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

namespace
{
    constexpr int64_t s_loadingDotsMaxCount = 3;
}

SamplePlayerMain::SamplePlayerMain()
{
    m_canCommitDirect3D11DepthBuffer = winrt::Windows::Foundation::Metadata::ApiInformation::IsMethodPresent(
        L"Windows.Graphics.Holographic.HolographicCameraRenderingParameters", L"CommitDirect3D11DepthBuffer");
}
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
        // Fallback to default port 8265, in case no valid port number was specified
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
        co_return;
    }

    // Try to connect or listen
    ConnectOrListen();
}

HolographicFrame SamplePlayerMain::Update(float deltaTimeInSeconds, const HolographicFrame& prevHolographicFrame)
{
    SpatialCoordinateSystem focusPointCoordinateSystem = nullptr;
    float3 focusPointPosition{0.0f, 0.0f, 0.0f};

    // Update the position of the status and error display.
    // Note, this is done with the data from the previous frame before the next wait to save CPU time and get the remote frame presented as
    // fast as possible. This also means that focus point and status display position are one frame behind which is a reasonable tradeoff
    // for the time we win.
    if (prevHolographicFrame != nullptr && m_attachedFrameOfReference != nullptr)
    {
        HolographicFramePrediction prevPrediction = prevHolographicFrame.CurrentPrediction();
        SpatialCoordinateSystem coordinateSystem =
            m_attachedFrameOfReference.GetStationaryCoordinateSystemAtTimestamp(prevPrediction.Timestamp());

        auto poseIterator = prevPrediction.CameraPoses().First();
        if (poseIterator.HasCurrent())
        {
            HolographicCameraPose cameraPose = poseIterator.Current();
            if (auto visibleFrustumReference = cameraPose.TryGetVisibleFrustum(coordinateSystem))
            {
                const float imageOffsetX = m_trackingLost ? -0.0095f : -0.0125f;
                const float imageOffsetY = 0.0111f;
                m_statusDisplay->PositionDisplay(deltaTimeInSeconds, visibleFrustumReference.Value(), imageOffsetX, imageOffsetY);
            }
        }

        focusPointCoordinateSystem = coordinateSystem;
        focusPointPosition = m_statusDisplay->GetPosition();
    }

    // Update content of the status and error display.
    {
        // Update the accumulated statistics with the statistics from the last frame.
        m_statisticsHelper.Update(m_playerContext.LastFrameStatistics());

        if (m_statisticsHelper.StatisticsHaveChanged() || !m_firstRemoteFrameWasBlitted)
        {
            UpdateStatusDisplay();
        }

        const bool connected = (m_playerContext.ConnectionState() == ConnectionState::Connected);
        if (!(connected && !m_trackingLost))
        {
            if (m_playerOptions.m_listen)
            {
                auto deviceIpNew = m_ipAddressUpdater.GetIpAddress(m_playerOptions.m_ipv6);
                if (m_deviceIp != deviceIpNew)
                {
                    m_deviceIp = deviceIpNew;

                    UpdateStatusDisplay();
                }
            }
        }

        m_statusDisplay->SetImageEnabled(!connected);
        m_statusDisplay->Update(deltaTimeInSeconds);
        m_errorHelper.Update(deltaTimeInSeconds, [this]() { UpdateStatusDisplay(); });
    }

    HolographicFrame holographicFrame = m_deviceResources->GetHolographicSpace().CreateNextFrame();
    {
        // Note, we don't wait for the next frame on present which allows us to first update all view independent stuff and also create the
        // next frame before we actually wait. By doing so everything before the wait is executed while the previous frame is presented by
        // the OS and thus saves us quite some CPU time after the wait.
        m_deviceResources->WaitForNextFrameReady();
    }
    holographicFrame.UpdateCurrentPrediction();

    // Back buffers can change from frame to frame. Validate each buffer, and recreate resource views and depth buffers as needed.
    m_deviceResources->EnsureCameraResources(
        holographicFrame, holographicFrame.CurrentPrediction(), focusPointCoordinateSystem, focusPointPosition);

    return holographicFrame;
}

void SamplePlayerMain::Render(const HolographicFrame& holographicFrame)
{
    bool atLeastOneCameraRendered = false;

    m_deviceResources->UseHolographicCameraResources([this, holographicFrame, &atLeastOneCameraRendered](
                                                         std::map<UINT32, std::unique_ptr<DXHelper::CameraResources>>& cameraResourceMap) {
        HolographicFramePrediction prediction = holographicFrame.CurrentPrediction();

        SpatialCoordinateSystem coordinateSystem = nullptr;
        if (m_attachedFrameOfReference)
        {
            coordinateSystem = m_attachedFrameOfReference.GetStationaryCoordinateSystemAtTimestamp(prediction.Timestamp());
        }

        // Retrieve information about any pending render target size change requests
        bool needRenderTargetSizeChange = false;
        winrt::Windows::Foundation::Size newRenderTargetSize{};
        {
            std::lock_guard lock{m_renderTargetSizeChangeMutex};
            if (m_needRenderTargetSizeChange)
            {
                needRenderTargetSizeChange = true;
                newRenderTargetSize = m_newRenderTargetSize;
                m_needRenderTargetSizeChange = false;
            }
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

                if (coordinateSystem)
                {
                    // The view and projection matrices for each holographic camera will change
                    // every frame. This function refreshes the data in the constant buffer for
                    // the holographic camera indicated by cameraPose.
                    pCameraResources->UpdateViewProjectionBuffer(m_deviceResources, cameraPose, coordinateSystem);

                    const bool connected = (m_playerContext.ConnectionState() == ConnectionState::Connected);

                    // Reduce the fov of the statistics view.
                    bool useLandscape = m_playerOptions.m_showStatistics && connected && !m_trackingLost && m_firstRemoteFrameWasBlitted;

                    // Pass data from the camera resources to the status display.
                    m_statusDisplay->UpdateTextScale(
                        pCameraResources->GetProjectionTransform(),
                        pCameraResources->GetRenderTargetSize().Width,
                        pCameraResources->GetRenderTargetSize().Height,
                        useLandscape,
                        pCameraResources->IsOpaque());
                }

                // Attach the view/projection constant buffer for this camera to the graphics pipeline.
                bool cameraActive = pCameraResources->AttachViewProjectionBuffer(m_deviceResources);

                // Only render world-locked content when positional tracking is active.
                if (cameraActive)
                {
                    auto blitResult = BlitResult::Failed_NoRemoteFrameAvailable;

                    try
                    {
                        if (m_playerContext.ConnectionState() == ConnectionState::Connected)
                        {
                            // Blit the remote frame into the backbuffer for the HolographicFrame.
                            // NOTE: This overwrites the focus point for the current frame, if the remote application
                            // has specified a focus point during the rendering of the remote frame.
                            blitResult = m_playerContext.BlitRemoteFrame();
                        }
                    }
                    catch (winrt::hresult_error err)
                    {
                        winrt::hstring msg = err.message();
                        m_errorHelper.AddError(std::wstring(L"BlitRemoteFrame failed: ") + msg.c_str());
                        UpdateStatusDisplay();
                    }

                    // If a remote remote frame has been blitted then color and depth buffer are fully overwritten, otherwise we have to
                    // clear both buffers before we render any local content.
                    if (blitResult != BlitResult::Success_Color && blitResult != BlitResult::Success_Color_Depth)
                    {
                        // Clear the back buffer and depth stencil view.
                        deviceContext->ClearRenderTargetView(targets[0], DirectX::Colors::Transparent);
                        deviceContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
                    }
                    else
                    {
                        m_firstRemoteFrameWasBlitted = true;
                    }

                    // Render local content.
                    {
                        // NOTE: Any local custom content would be rendered here.

                        // Draw connection status and/or statistics.
                        m_statusDisplay->Render();
                    }

                    // Commit depth buffer if it has been committed by the remote app which is indicated by Success_Color_Depth.
                    // NOTE: CommitDirect3D11DepthBuffer should be the last thing before the frame is presented. By doing so the depth
                    //       buffer submitted includes remote content and local content.
                    if (m_canCommitDirect3D11DepthBuffer && blitResult == BlitResult::Success_Color_Depth)
                    {
                        auto interopSurface = pCameraResources->GetDepthStencilTextureInteropObject();
                        HolographicCameraRenderingParameters renderingParameters = holographicFrame.GetRenderingParameters(cameraPose);
                        renderingParameters.CommitDirect3D11DepthBuffer(interopSurface);
                    }
                }

                atLeastOneCameraRendered = true;
            });

            if (needRenderTargetSizeChange)
            {
                if (HolographicViewConfiguration viewConfig = cameraPose.HolographicCamera().ViewConfiguration())
                {
                    // Only request new render target size if we are dealing with an opaque (i.e., VR) display
                    if (cameraPose.HolographicCamera().Display().IsOpaque())
                    {
                        viewConfig.RequestRenderTargetSize(newRenderTargetSize);
                    }
                }
            }
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
    // Create the player context
    // IMPORTANT: This must be done before creating the HolographicSpace (or any other call to the Holographic API).
    try
    {
        m_playerContext = PlayerContext::Create();
    }
    catch (winrt::hresult_error)
    {
        // If we get here, it is likely that no Windows Holographic is installed.
        m_failedToCreatePlayerContext = true;
        // Return right away to avoid bringing down the application. This allows us to
        // later provide feedback to users about this failure.
        return;
    }

    // Register to the PlayerContext connection events
    m_playerContext.OnConnected({this, &SamplePlayerMain::OnConnected});
    m_playerContext.OnDisconnected({this, &SamplePlayerMain::OnDisconnected});
    m_playerContext.OnRequestRenderTargetSize({this, &SamplePlayerMain::OnRequestRenderTargetSize});

    // Set the BlitRemoteFrame timeout to 0.5s
    m_playerContext.BlitRemoteFrameTimeout(500ms);

    // Projection transform always reflects what has been configured on the remote side.
    m_playerContext.ProjectionTransformConfig(ProjectionTransformMode::Remote);

    // Enable 10% overRendering with 10% resolution increase. With this configuration, the viewport gets increased by 5% in each direction
    // and the DPI remains equal.
    OverRenderingConfig overRenderingConfig;
    overRenderingConfig.HorizontalViewportIncrease = 0.1f;
    overRenderingConfig.VerticalViewportIncrease = 0.1f;
    overRenderingConfig.HorizontalResolutionIncrease = 0.1f;
    overRenderingConfig.VerticalResolutionIncrease = 0.1f;
    m_playerContext.ConfigureOverRendering(overRenderingConfig);

    // Register event handlers for app lifecycle.
    m_suspendingEventRevoker = CoreApplication::Suspending(winrt::auto_revoke, {this, &SamplePlayerMain::OnSuspending});

    m_viewActivatedRevoker = applicationView.Activated(winrt::auto_revoke, {this, &SamplePlayerMain::OnViewActivated});

    m_deviceResources = std::make_shared<DXHelper::DeviceResourcesUWP>();
    m_deviceResources->RegisterDeviceNotify(this);

    m_spatialLocator = SpatialLocator::GetDefault();
    if (m_spatialLocator != nullptr)
    {
        m_locatabilityChangedRevoker =
            m_spatialLocator.LocatabilityChanged(winrt::auto_revoke, {this, &SamplePlayerMain::OnLocatabilityChanged});
        m_attachedFrameOfReference = m_spatialLocator.CreateAttachedFrameOfReferenceAtCurrentHeading();
    }
}

void SamplePlayerMain::SetWindow(const CoreWindow& window)
{
    m_windowVisible = window.Visible();

    m_windowClosedEventRevoker = window.Closed(winrt::auto_revoke, {this, &SamplePlayerMain::OnWindowClosed});
    m_visibilityChangedEventRevoker = window.VisibilityChanged(winrt::auto_revoke, {this, &SamplePlayerMain::OnVisibilityChanged});

    // We early out if we have no device resources here to avoid bringing down the application.
    // The reason for this is that we want to be able to provide feedback to users later on in
    // case the player context could not be created.
    if (!m_deviceResources)
    {
        return;
    }

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
            m_customDataChannel = dataChannel.as<IDataChannel2>();

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

    HolographicFrame prevHolographicFrame = nullptr;
    while (!m_windowClosed)
    {
        TimePoint timeCurrUpdate = clock.now();
        Duration timeSinceLastUpdate = timeCurrUpdate - timeLastUpdate;
        float deltaTimeInSeconds = std::chrono::duration<float>(timeSinceLastUpdate).count();

        // If we encountered an error while creating the player context, we are going to provide
        // users with some feedback here. We have to do this after the application has launched
        // or we are going to fail at showing the dialog box.
        if (m_failedToCreatePlayerContext && !m_shownFeedbackToUser)
        {
            CoreWindow coreWindow{CoreApplication::MainView().CoreWindow().GetForCurrentThread()};

            // Window must be active or the MessageDialog will not show.
            coreWindow.Activate();

            // Dispatch call to open MessageDialog.
            coreWindow.Dispatcher().RunAsync(
                winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                winrt::Windows::UI::Core::DispatchedHandler([]() -> winrt::fire_and_forget {
                    winrt::Windows::UI::Popups::MessageDialog failureDialog(
                        L"Failed to initialize. Please make sure that Windows Holographic is installed on your system."
                        " Windows Holographic will be installed automatically when you attach your Head-mounted Display.");

                    failureDialog.Title(L"Initialization Failure");
                    failureDialog.Commands().Append(winrt::Windows::UI::Popups::UICommand(L"Close App"));
                    failureDialog.DefaultCommandIndex(0);
                    failureDialog.CancelCommandIndex(0);

                    auto _ = co_await failureDialog.ShowAsync();

                    CoreApplication::Exit();
                }));

            m_shownFeedbackToUser = true;
        }

        if (m_windowVisible && m_deviceResources != nullptr && (m_deviceResources->GetHolographicSpace() != nullptr))
        {
            CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

            HolographicFrame holographicFrame = Update(deltaTimeInSeconds, prevHolographicFrame);
            Render(holographicFrame);
            prevHolographicFrame = holographicFrame;
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

    m_suspendingEventRevoker.revoke();
    m_viewActivatedRevoker.revoke();
    m_windowClosedEventRevoker.revoke();
    m_visibilityChangedEventRevoker.revoke();
    m_locatabilityChangedRevoker.revoke();

    if (m_deviceResources)
    {
        m_deviceResources->RegisterDeviceNotify(nullptr);
        m_deviceResources = nullptr;
    }
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
    bool argsProvided = false;
    std::wstring host = L"";
    uint16_t port = 0;
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
                std::wstring launchArgsStr = launchArgs.Arguments().c_str();

                if (launchArgsStr.length() > 0)
                {
                    argsProvided = true;

                    std::vector<std::wstring> args;
                    std::wistringstream stream(launchArgsStr);
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

                        host = PlayerUtil::SplitHostnameAndPortString(arg, port);
                    }
                }
                break;
            }

            case Activation::ActivationKind::Protocol:
            {
                argsProvided = true;

                ProtocolActivatedEventArgs protocolArgs = activationArgs.as<ProtocolActivatedEventArgs>();
                auto uri = protocolArgs.Uri();
                if (uri)
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

    PlayerOptions playerOptions;
    if (argsProvided)
    {
        // check for invalid port numbers
        if (port < 0 || port > 65535)
        {
            port = 0;
        }

        winrt::hstring hostname = host.c_str();
        if (hostname.empty())
        {
            // default to listen (as we can't connect to an unspecified host)
            hostname = L"0.0.0.0";
            listen = true;
        }

        playerOptions.m_hostname = hostname;
        playerOptions.m_port = port;
        playerOptions.m_listen = listen;
        playerOptions.m_showStatistics = showStatistics;
        playerOptions.m_ipv6 = !hostname.empty() && hostname.front() == L'[';
    }
    else
    {
        playerOptions = m_playerOptions;
    }

    return playerOptions;
}

void SamplePlayerMain::UpdateStatusDisplay()
{
    m_statusDisplay->ClearLines();

    if (m_trackingLost)
    {
        StatusDisplay::Line lines[] = {StatusDisplay::Line{L"Device Tracking Lost", StatusDisplay::Small, StatusDisplay::Yellow, 1.0f}};
        m_statusDisplay->SetLines(lines);
    }
    else
    {
        if (m_playerContext.ConnectionState() != ConnectionState::Connected)
        {
            StatusDisplay::Line lines[] = {
                StatusDisplay::Line{L"Holographic Remoting Player", StatusDisplay::LargeBold, StatusDisplay::White, 1.0f},
                StatusDisplay::Line{
                    L"This app is a companion for Holographic Remoting apps.", StatusDisplay::Small, StatusDisplay::White, 1.0f},
                StatusDisplay::Line{L"Connect from a compatible app to begin.", StatusDisplay::Small, StatusDisplay::White, 15.0f},
                StatusDisplay::Line{
                    m_playerOptions.m_listen ? L"Waiting for connection on" : L"Connecting to",
                    StatusDisplay::Small,
                    StatusDisplay::White}};
            m_statusDisplay->SetLines(lines);

            std::wostringstream addressLine;
            addressLine << (m_playerOptions.m_listen ? m_deviceIp.c_str() : m_playerOptions.m_hostname.c_str());
            if (m_playerOptions.m_port)
            {
                addressLine << L":" << m_playerOptions.m_port;
            }
            m_statusDisplay->AddLine(StatusDisplay::Line{addressLine.str(), StatusDisplay::Medium, StatusDisplay::Yellow});
            m_statusDisplay->AddLine(
                StatusDisplay::Line{L"Get help at: https://aka.ms/holographicremotinghelp", StatusDisplay::Small, StatusDisplay::White});

            if (m_playerOptions.m_showStatistics)
            {
                m_statusDisplay->AddLine(StatusDisplay::Line{L"Diagnostics Enabled", StatusDisplay::Small, StatusDisplay::Yellow});
            }
        }
        else if (m_playerContext.ConnectionState() == ConnectionState::Connected && !m_firstRemoteFrameWasBlitted)
        {
            using namespace std::chrono;

            int64_t loadingDotsCount =
                (duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count() / 250) % (s_loadingDotsMaxCount + 1);

            std::wstring dotsText;
            for (int64_t i = 0; i < loadingDotsCount; ++i)
            {
                dotsText.append(L".");
            }

            m_statusDisplay->AddLine(StatusDisplay::Line{L"", StatusDisplay::Medium, StatusDisplay::White, 7});
            m_statusDisplay->AddLine(StatusDisplay::Line{L"Receiving", StatusDisplay::Medium, StatusDisplay::White, 0.3f});
            m_statusDisplay->AddLine(StatusDisplay::Line{dotsText, StatusDisplay::Medium, StatusDisplay::White});
        }
        else
        {
            if (m_playerOptions.m_showStatistics)
            {
                const std::wstring statisticsString = m_statisticsHelper.GetStatisticsString();

                StatusDisplay::Line line = {std::move(statisticsString), StatusDisplay::Medium, StatusDisplay::Yellow, 1.0f, true};
                m_statusDisplay->AddLine(line);
            }
        }
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
        // Get send queue size. The send queue size returns the size of data, that has not been send yet, in bytes. A big number can
        // indicate that more data is being queued for sending than is actually getting sent. If possible skip sending data in this
        // case, to help the queue getting smaller again.
        uint32_t sendQueueSize = m_customDataChannel.SendQueueSize();

        // Only send the packet if the send queue is smaller than 1MiB
        if (sendQueueSize < 1 * 1024 * 1024)
        {
            uint8_t data[] = {1};

            try
            {
                m_customDataChannel.SendData(data, true);
            }
            catch (...)
            {
                // SendData might throw if channel is closed, but we did not get or process the async closed event yet.
            }
        }
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

    m_firstRemoteFrameWasBlitted = false;

    UpdateStatusDisplay();

    if (error)
    {
        ConnectOrListenAfter(1s);
        return;
    }

    // Reconnect quickly if not an error
    ConnectOrListenAfter(200ms);
}

void SamplePlayerMain::OnRequestRenderTargetSize(
    winrt::Windows::Foundation::Size requestedSize, winrt::Windows::Foundation::Size providedSize)
{
    // Store the new remote render target size
    // Note: We'll use the provided size as remote side content is going to be resampled/distorted anyway,
    // so there is no point in resolving this information into a smaller backbuffer on the player side.
    std::lock_guard lock{m_renderTargetSizeChangeMutex};
    m_needRenderTargetSizeChange = true;
    m_newRenderTargetSize = providedSize;
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

    // Prevent diagnostics to be turned off everytime the app went to background.
    if (activationArgs.PreviousExecutionState() != ApplicationExecutionState::NotRunning)
    {
        if (!playerOptionsNew.m_showStatistics)
        {
            playerOptionsNew.m_showStatistics = m_playerOptions.m_showStatistics;
        }
    }

    m_playerOptions = playerOptionsNew;

    if (m_playerContext.ConnectionState() == ConnectionState::Disconnected)
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
    winrt::com_ptr<SamplePlayerMain> main = winrt::make_self<SamplePlayerMain>();
    CoreApplication::Run(*main);
    return 0;
}
