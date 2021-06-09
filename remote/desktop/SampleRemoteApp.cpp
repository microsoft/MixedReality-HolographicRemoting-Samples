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

#include <SampleRemoteApp.h>

#include <DbgLog.h>
#include <Utils.h>
#include <d3d11/DirectXHelper.h>
#include <holographic/RemoteWindowHolographic.h>
#include <holographic/Speech.h>

#include <HolographicAppRemoting/Streamer.h>

#include <iterator>
#include <sstream>

#include <DirectXColors.h>
#include <shellapi.h>
#include <winrt/Microsoft.Holographic.AppRemoting.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Perception.People.h>
#include <winrt/Windows.Security.Authorization.AppCapabilityAccess.h>

using namespace concurrency;

using namespace std::chrono_literals;

using namespace winrt::Microsoft::Holographic::AppRemoting;
using namespace winrt::Microsoft::MixedReality::SceneUnderstanding;
using namespace winrt::Microsoft::MixedReality::QR;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Perception::People;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::UI::Input;
using namespace winrt::Windows::Security::Authorization::AppCapabilityAccess;

namespace
{
    const wchar_t* StreamerConnectionStateToString(ConnectionState state, bool disconnectPending)
    {
        switch (state)
        {
            case ConnectionState::Disconnected:
                return L"Disconnected";

            case ConnectionState::Connecting:
                return L"Connecting";

            case ConnectionState::Connected:
                return disconnectPending ? L"Disconnecting" : L"Connected";
        }

        return L"Unknown";
    }
} // namespace

SampleRemoteApp::SampleRemoteApp()
{
}

SampleRemoteApp::~SampleRemoteApp()
{
    ShutdownRemoteContext();

    m_deviceResources->RegisterDeviceNotify(nullptr);
    UnregisterHolographicEventHandlers();
}

void SampleRemoteApp::SetWindow(RemoteWindowHolographic* window)
{
    m_window = window;

    m_deviceResources = std::make_shared<DXHelper::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);

    m_canCommitDirect3D11DepthBuffer = winrt::Windows::Foundation::Metadata::ApiInformation::IsMethodPresent(
        L"Windows.Graphics.Holographic.HolographicCameraRenderingParameters", L"CommitDirect3D11DepthBuffer");
}

void SampleRemoteApp::Tick()
{
    if (const HolographicFrame& holographicFrame = Update())
    {
        Render(holographicFrame);
    }
}

void SampleRemoteApp::OnKeyPress(char key)
{
    switch (key)
    {
        case ' ':
            InitializeRemoteContextAndConnectOrListen();
            break;

        case 'd':
        {
            std::lock_guard remoteContextLock(m_remoteContextAccess);
            if (m_remoteContext && m_remoteContext.ConnectionState() != ConnectionState::Disconnected)
            {
                m_disconnectPending = true;
                m_remoteContext.Disconnect();
            }
        }
        break;

        case 'p':
            m_options.showPreview = !m_options.showPreview;
            break;

        case 'l':
            LoadPosition();
            break;

        case 's':
            SavePosition();
            break;

        case 'e':
            ExportPosition();
            break;

        case 'x':
            m_commitDirect3D11DepthBuffer = !m_commitDirect3D11DepthBuffer;
            break;

        case 'c':
            m_spinningCubeRenderer->TogglePauseState();
            break;

        case 'u':
            ToggleSceneUnderstanding();
            break;
    }

    WindowUpdateTitle();
}

void SampleRemoteApp::OnResize(int width, int height)
{
    std::lock_guard _lg(m_deviceLock);

    if (width != m_width || height != m_height)
    {
        m_width = width;
        m_height = height;

        if (m_swapChain)
        {
            winrt::check_hresult(m_swapChain->ResizeBuffers(2, m_width, m_height, DXGI_FORMAT_B8G8R8A8_UNORM, 0));
        }
    }
}

void SampleRemoteApp::ParseLaunchArguments(std::wstring_view arguments)
{

    SampleRemoteApp::Options options;

#if _M_ARM || _M_ARM64
    bool isStandalone = true;
#else
    bool isStandalone = false;
#endif
    bool noUserWait = false;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    LPWSTR* args;
    int argCount;
    args = CommandLineToArgvW(arguments.data(), &argCount);
#else
    std::wstring argumentsStr = std::wstring(arguments);
    std::wistringstream argStream = std::wistringstream(argumentsStr);
    std::istream_iterator<std::wstring, wchar_t> argStreamBegin(argStream);
    std::istream_iterator<std::wstring, wchar_t> argStreamEnd = std::istream_iterator<std::wstring, wchar_t>();

    std::vector<std::wstring> args;
    std::copy(argStreamBegin, argStreamEnd, std::back_inserter(args));

    size_t argCount = args.size();
#endif
    for (int argIndex = 0; argIndex < argCount; ++argIndex)
    {
        std::wstring arg = args[argIndex];

        if (arg.length() == 0)
        {
            continue;
        }

        if (arg[0] == '-')
        {
            std::wstring param = arg.substr(1);
            std::transform(param.begin(), param.end(), param.begin(), ::tolower);

            if (param == L"listen")
            {
                options.listen = true;
                continue;
            }

            if (param == L"standalone")
            {
                isStandalone = true;
                continue;
            }

            if (arg == L"nostandalone")
            {
                isStandalone = false;
                continue;
            }

            if (param == L"nouserwait")
            {
                noUserWait = true;
                continue;
            }

            if (param == L"noautoreconnect")
            {
                options.autoReconnect = false;
                continue;
            }

            if (param == L"ephemeralport")
            {
                options.ephemeralPort = true;
                continue;
            }

            if (param == L"transportport")
            {
                if (argIndex + 1 < argCount)
                {
                    std::wstring transportPortStr = args[argIndex + 1];
                    try
                    {
                        options.transportPort = std::stoi(transportPortStr);
                    }
                    catch (const std::invalid_argument&)
                    {
                        // Ignore invalid transport port strings.
                    }
                    argIndex++;
                }
                continue;
            }
        }

        options.hostname = Utils::SplitHostnameAndPortString(arg, options.port);
    }

    if (!isStandalone)
    {
        ConfigureRemoting(options);
        if (noUserWait)
        {
            InitializeRemoteContextAndConnectOrListen();
        }
    }
    else
    {
        InitializeStandalone();
    }
}

void SampleRemoteApp::OnRecognizedSpeech(const winrt::hstring& recognizedText)
{
    bool changedColor = false;
    DirectX::XMFLOAT4 color = {1, 1, 1, 1};

    if (recognizedText == L"Red")
    {
        color = {1, 0, 0, 1};
        changedColor = true;
    }
    else if (recognizedText == L"Blue")
    {
        color = {0, 0, 1, 1};
        changedColor = true;
    }
    else if (recognizedText == L"Green")
    {
        color = {0, 1, 0, 1};
        changedColor = true;
    }
    else if (recognizedText == L"Default")
    {
        color = {1, 1, 1, 1};
        changedColor = true;
    }
    else if (recognizedText == L"Aquamarine")
    {
        color = {0, 1, 1, 1};
        changedColor = true;
    }
    else if (recognizedText == L"Load position")
    {
        LoadPosition();
    }
    else if (recognizedText == L"Save position")
    {
        SavePosition();
    }

    if (changedColor && m_spinningCubeRenderer)
    {
        m_spinningCubeRenderer->SetColorFilter(color);
    }
}

HolographicFrame SampleRemoteApp::Update()
{
    auto timeDelta = std::chrono::high_resolution_clock::now() - m_windowTitleUpdateTime;
    if (timeDelta >= 1s)
    {
        WindowUpdateTitle();

        m_windowTitleUpdateTime = std::chrono::high_resolution_clock::now();
        m_framesPerSecond = 0;
    }

    if (!m_holographicSpace)
    {
        return nullptr;
    }

    try
    {
        // Create the next holographic frame early.
        HolographicFrame holographicFrame = m_holographicSpace.CreateNextFrame();

        // NOTE: DXHelper::DeviceResources::Present does not wait for the frame to finish.
        //       Instead we wait here before we do the call to CreateNextFrame on the HolographicSpace.
        //       We do this to avoid that PeekMessage causes frame delta time spikes, say if we wait
        //       after PeekMessage WaitForNextFrameReady will compensate any time spend in PeekMessage.
        m_holographicSpace.WaitForNextFrameReady();

        // Update to latest prediction immediately after waiting.
        holographicFrame.UpdateCurrentPrediction();

        HolographicFramePrediction prediction = holographicFrame.CurrentPrediction();

        // Back buffers can change from frame to frame. Validate each buffer, and recreate resource views and depth buffers as needed.
        m_deviceResources->EnsureCameraResources(holographicFrame, prediction);

        SpatialCoordinateSystem coordinateSystem = nullptr;
        coordinateSystem = m_referenceFrame.CoordinateSystem();

        // Check for new input state since the last frame.
        Spatial::SpatialTappedEventArgs tapped = m_spatialInputHandler->CheckForTapped();
        if (tapped)
        {
            Spatial::SpatialPointerPose pointerPose = tapped.TryGetPointerPose(coordinateSystem);

            // When the Tapped spatial input event is received, the sample hologram will be repositioned two meters in front of the user.
            m_spinningCubeRenderer->PositionHologram(pointerPose);
        }
        else
        {
            static float3 initialCubePosition = float3::zero();

            auto manipulationStarted = m_spatialInputHandler->CheckForManipulationStarted();
            if (manipulationStarted)
            {
                initialCubePosition = m_spinningCubeRenderer->GetPosition();
                m_spinningCubeRenderer->Pause();
            }
            else
            {
                auto manipulationUpdated = m_spatialInputHandler->CheckForManipulationUpdated();
                if (manipulationUpdated)
                {
                    auto delta = manipulationUpdated.TryGetCumulativeDelta(coordinateSystem);
                    if (delta)
                    {
                        m_spinningCubeRenderer->SetPosition(initialCubePosition + delta.Translation());
                    }
                }
                else
                {
                    switch (m_spatialInputHandler->CheckForManipulationResult())
                    {
                        case SpatialInputHandler::ManipulationResult::Canceled:
                            m_spinningCubeRenderer->SetPosition(initialCubePosition);
                        case SpatialInputHandler::ManipulationResult::Completed:
                            m_spinningCubeRenderer->Unpause();
                            break;
                    }
                }
            }
        }

        std::chrono::duration<float> timeSinceStart = std::chrono::high_resolution_clock::now() - m_startTime;
        m_spinningCubeRenderer->Update(timeSinceStart.count(), prediction.Timestamp(), coordinateSystem);

        m_sceneUnderstandingRenderer->Update(coordinateSystem);
        m_qrCodeRenderer->Update(coordinateSystem);

        if (m_spatialSurfaceMeshRenderer)
        {
            m_spatialSurfaceMeshRenderer->Update(prediction.Timestamp(), coordinateSystem);
        }
        m_spatialInputRenderer->Update(prediction.Timestamp(), coordinateSystem);

        // We complete the frame update by using information about our content positioning to set the focus point.
        if (!m_canCommitDirect3D11DepthBuffer || !m_commitDirect3D11DepthBuffer)
        {
            for (auto cameraPose : prediction.CameraPoses())
            {
                try
                {
                    HolographicCameraRenderingParameters renderingParameters = holographicFrame.GetRenderingParameters(cameraPose);

                    // Set the focus point for image stabilization to the center of the sample hologram.
                    // NOTE: A focus point can be set for every HolographicFrame. If a focus point is set on a HolographicFrame,
                    //       it will get transmitted to the player and will get set during the PlayerContext::BlitRemoteFrame() call.
                    renderingParameters.SetFocusPoint(coordinateSystem, m_spinningCubeRenderer->GetPosition());
                }
                catch (winrt::hresult_error&)
                {
                }
            }
        }

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
        timeDelta = std::chrono::high_resolution_clock::now() - m_customDataChannelSendTime;
        if (timeDelta > 5s)
        {
            m_customDataChannelSendTime = std::chrono::high_resolution_clock::now();

            // Send ping every couple of frames if we have a custom data channel.
            std::lock_guard lock(m_customDataChannelLock);
            if (m_customDataChannel)
            {
                // Try to get send queue size. The send queue size returns the size of data, that has not been send yet, in bytes.
                // A big number might indicate that more data is queued to send than the amount of data, that is actually sent.
                // If possible skip sending data in this case, to help the queue getting smaller again.
                uint32_t sendQueueSize = m_customDataChannel.SendQueueSize();

                // Only send the packet if the send queue is smaller than 1MiB
                if (sendQueueSize < 1 * 1024 * 1024)
                {
                    uint8_t data = 1;

                    try
                    {
                        m_customDataChannel.SendData(
                            winrt::array_view<const uint8_t>(
                                reinterpret_cast<const uint8_t*>(&data), reinterpret_cast<const uint8_t*>(&data + 1)),
                            true);
                        OutputDebugString(TEXT("Request Sent.\n"));
                    }
                    catch (...)
                    {
                        // SendData might throw if channel is closed, but we did not get or process the async closed event yet.
                    }
                }
            }
        }
#endif

        return holographicFrame;
    }
    catch (const winrt::hresult_error&)
    {
        return nullptr;
    }
}

void SampleRemoteApp::Render(HolographicFrame holographicFrame)
{
    bool atLeastOneCameraRendered = false;

    m_deviceResources->UseHolographicCameraResources([this, holographicFrame, &atLeastOneCameraRendered](
                                                         std::map<UINT32, std::unique_ptr<DXHelper::CameraResources>>& cameraResourceMap) {
        holographicFrame.UpdateCurrentPrediction();
        HolographicFramePrediction prediction = holographicFrame.CurrentPrediction();

        SpatialCoordinateSystem coordinateSystem = nullptr;
        coordinateSystem = m_referenceFrame.CoordinateSystem();

        for (auto cameraPose : prediction.CameraPoses())
        {
            try
            {
                DXHelper::CameraResources* pCameraResources = cameraResourceMap[cameraPose.HolographicCamera().Id()].get();

                if (pCameraResources == nullptr || pCameraResources->GetBackBufferRenderTargetView() == nullptr)
                {
                    continue;
                }

                winrt::Windows::Foundation::IReference<SpatialBoundingFrustum> cullingFrustum =
                    cameraPose.TryGetCullingFrustum(coordinateSystem);

                m_deviceResources->UseD3DDeviceContext([&](ID3D11DeviceContext3* context) {
                    // Clear the back buffer view.
                    context->ClearRenderTargetView(pCameraResources->GetBackBufferRenderTargetView(), DirectX::Colors::Transparent);
                    context->ClearDepthStencilView(
                        pCameraResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

                    // The view and projection matrices for each holographic camera will change
                    // every frame. This function refreshes the data in the constant buffer for
                    // the holographic camera indicated by cameraPose.
                    pCameraResources->UpdateViewProjectionBuffer(m_deviceResources, cameraPose, coordinateSystem);

                    // Set up the camera buffer.
                    bool cameraActive = pCameraResources->AttachViewProjectionBuffer(m_deviceResources);

                    // Only render world-locked content when positional tracking is active.
                    if (cameraActive)
                    {
                        // Set the render target, and set the depth target drawing buffer.
                        ID3D11RenderTargetView* const targets[1] = {pCameraResources->GetBackBufferRenderTargetView()};
                        context->OMSetRenderTargets(1, targets, pCameraResources->GetDepthStencilView());

                        // Render the scene objects.
                        m_spinningCubeRenderer->Render(pCameraResources->IsRenderingStereoscopic(), cullingFrustum);

                        m_sceneUnderstandingRenderer->Render(pCameraResources->IsRenderingStereoscopic());
                        m_qrCodeRenderer->Render(pCameraResources->IsRenderingStereoscopic(), cullingFrustum);

                        if (m_spatialSurfaceMeshRenderer)
                        {
                            m_spatialSurfaceMeshRenderer->Render(pCameraResources->IsRenderingStereoscopic());
                        }
                        m_spatialInputRenderer->Render(pCameraResources->IsRenderingStereoscopic(), cullingFrustum);

                        // Commit depth buffer if available and enabled.
                        if (m_canCommitDirect3D11DepthBuffer && m_commitDirect3D11DepthBuffer)
                        {
                            auto interopSurface = pCameraResources->GetDepthStencilTextureInteropObject();
                            HolographicCameraRenderingParameters renderingParameters = holographicFrame.GetRenderingParameters(cameraPose);
                            renderingParameters.CommitDirect3D11DepthBuffer(interopSurface);
                        }
                    }
                });

                atLeastOneCameraRendered = true;
            }
            catch (const winrt::hresult_error&)
            {
            }
        }
    });

    if (atLeastOneCameraRendered)
    {
        m_deviceResources->Present(holographicFrame);
    }

    if (!m_isStandalone)
    {
        if (m_swapChain == nullptr && m_isInitialized)
        {
            // A device lost event has occurred.
            // Reconnection is necessary because the holographic streamer uses the D3D device.
            // The following resources depend on the D3D device:
            //   * Holographic streamer
            //   * Renderer
            //   * Holographic space
            // The InitializeRemoteContext() function will call the functions necessary to recreate these resources.
            ShutdownRemoteContext();
            InitializeRemoteContextAndConnectOrListen();
        }

        // Determine whether or not to copy to the preview buffer.
        bool copyPreview;
        {
            std::lock_guard remoteContextLock(m_remoteContextAccess);
            copyPreview = m_remoteContext == nullptr || m_remoteContext.ConnectionState() != ConnectionState::Connected;
        }
        if (copyPreview && m_isInitialized)
        {
            winrt::com_ptr<ID3D11Device1> spDevice;
            spDevice.copy_from(GetDeviceResources()->GetD3DDevice());

            winrt::com_ptr<ID3D11Texture2D> spBackBuffer;
            winrt::check_hresult(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), spBackBuffer.put_void()));

            // Create a render target view of the back buffer.
            // Creating this resource is inexpensive, and is better than keeping track of
            // the back buffers in order to pre-allocate render target views for each one.
            winrt::com_ptr<ID3D11RenderTargetView> spRenderTargetView;
            winrt::check_hresult(spDevice->CreateRenderTargetView(spBackBuffer.get(), nullptr, spRenderTargetView.put()));

            GetDeviceResources()->UseD3DDeviceContext(
                [&](auto context) { context->ClearRenderTargetView(spRenderTargetView.get(), DirectX::Colors::CornflowerBlue); });

            WindowPresentSwapChain();
        }
    }

    m_framesPerSecond++;
}

void SampleRemoteApp::ConfigureRemoting(const Options& options)
{
    if (!m_isInitialized)
    {
        m_options = options;
    }
}

void SampleRemoteApp::InitializeStandalone()
{
    if (!m_isInitialized)
    {
        m_isStandalone = true;
        CreateHolographicSpaceAndDeviceResources();
    }

    InitializeAccessToFeatures();
}

void SampleRemoteApp::InitializeRemoteContextAndConnectOrListen()
{
    std::lock_guard remoteContextLock(m_remoteContextAccess);

    if (!m_remoteContext && !m_isStandalone)
    {

        // Create the RemoteContext
        // IMPORTANT: This must be done before creating the HolographicSpace (or any other call to the Holographic API).
        HRESULT hr = CreateRemoteContext(m_remoteContext, 20000, true, PreferredVideoCodec::Any);

        if (hr != S_OK)
        {
            if (hr == WINCODEC_ERR_COMPONENTNOTFOUND)
            {
                DebugLog(L"Preferred video codec not found.\n");
            }
            else
            {
                DebugLog(L"Failed to create the remote context.\n");
            }
            m_remoteContext = nullptr;
            return;
        }

        // Configure for half-resolution depth.
        auto depthResolution = DepthBufferStreamResolution::Half_Resolution;
        if (m_depthDownscale == 1)
        {
            depthResolution = DepthBufferStreamResolution::Full_Resolution;
        }
        else if (m_depthDownscale == 4)
        {
            depthResolution = DepthBufferStreamResolution::Quarter_Resolution;
        }
        m_remoteContext.ConfigureDepthVideoStream(depthResolution);

        // Create the HolographicSpace
        CreateHolographicSpaceAndDeviceResources();

        if (auto remoteSpeech = m_remoteContext.GetRemoteSpeech())
        {
            Speech::InitializeSpeechAsync(remoteSpeech, m_onRecognizedSpeechRevoker, weak_from_this());
        }

        winrt::com_ptr<ID3D11Device1> device;
        device.copy_from(GetDeviceResources()->GetD3DDevice());
        WindowCreateSwapChain(device);

        DXGI_ADAPTER_DESC2 dxgiAdapterDesc;
        if (SUCCEEDED(GetDeviceResources()->GetDXGIAdapter()->GetDesc2(&dxgiAdapterDesc)) &&
            (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
        {
            DebugLog(L"Software video adapter is not supported for holographic streamer.\n");
            m_remoteContext = nullptr;
            return;
        }

        m_onConnectedEventRevoker = m_remoteContext.OnConnected(winrt::auto_revoke, [weakThis = weak_from_this()]() {
            if (auto strongThis = weakThis.lock())
            {
                strongThis->OnConnected();
            }
        });

        m_onDisconnectedEventRevoker =
            m_remoteContext.OnDisconnected(winrt::auto_revoke, [weakThis = weak_from_this()](ConnectionFailureReason failureReason) {
                if (auto strongThis = weakThis.lock())
                {
                    strongThis->OnDisconnected(failureReason);
                }
            });

        m_onSendFrameEventRevoker = m_remoteContext.OnSendFrame(
            winrt::auto_revoke,
            [weakThis = weak_from_this()](const winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface& texture) {
                if (auto strongThis = weakThis.lock())
                {
                    strongThis->OnSendFrame(texture);
                }
            });

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
        m_onDataChannelCreatedEventRevoker =
            m_remoteContext.OnDataChannelCreated(winrt::auto_revoke, [this](const IDataChannel& dataChannel, uint8_t channelId) {
                std::lock_guard lock(m_customDataChannelLock);
                m_customDataChannel = dataChannel.as<IDataChannel2>();

                m_customChannelDataReceivedEventRevoker = m_customDataChannel.OnDataReceived(
                    winrt::auto_revoke, [this](winrt::array_view<const uint8_t> dataView) { OnCustomDataChannelDataReceived(dataView); });

                m_customChannelClosedEventRevoker =
                    m_customDataChannel.OnClosed(winrt::auto_revoke, [this]() { OnCustomDataChannelClosed(); });
            });
#endif

        ConnectOrListen();
    }
}

void SampleRemoteApp::CreateHolographicSpaceAndDeviceResources()
{
    UnregisterHolographicEventHandlers();

    if (m_window)
    {
        m_holographicSpace = m_window->CreateHolographicSpace();
        m_interactionManager = m_window->CreateInteractionManager();
    }

    m_deviceResources->SetHolographicSpace(m_holographicSpace);

    m_spatialInputRenderer = std::make_shared<SpatialInputRenderer>(m_deviceResources, m_interactionManager);
    m_spatialInputHandler = std::make_shared<SpatialInputHandler>(m_interactionManager);

    m_spinningCubeRenderer = std::make_unique<SpinningCubeRenderer>(m_deviceResources);

    m_sceneUnderstandingRenderer = std::make_unique<SceneUnderstandingRenderer>(m_deviceResources);
    m_qrCodeRenderer = std::make_unique<QRCodeRenderer>(m_deviceResources);

    m_locator = SpatialLocator::GetDefault();

    // Be able to respond to changes in the positional tracking state.
    m_locatabilityChangedToken = m_locator.LocatabilityChanged({this, &SampleRemoteApp::OnLocatabilityChanged});

    m_cameraAddedToken = m_holographicSpace.CameraAdded({this, &SampleRemoteApp::OnCameraAdded});
    m_cameraRemovedToken = m_holographicSpace.CameraRemoved({this, &SampleRemoteApp::OnCameraRemoved});

    m_referenceFrame = m_locator.CreateStationaryFrameOfReferenceAtCurrentLocation(float3::zero(), quaternion(0, 0, 0, 1), 0.0);

    m_isInitialized = true;
}

void SampleRemoteApp::ConnectOrListen()
{
    std::lock_guard remoteContextLock(m_remoteContextAccess);

    if (!m_remoteContext || m_remoteContext.ConnectionState() != ConnectionState::Disconnected)
    {
        return;
    }

    // Try to establish a connection.
    try
    {
        if (m_options.ephemeralPort)
        {
            m_options.port = 0;
        }
        else if (m_options.port == 0)
        {
            m_options.port = 8265;
        }

        if (m_options.listen)
        {
            if (m_options.ephemeralPort)
            {
                m_options.transportPort = 0;
            }
            else if (m_options.transportPort == 0)
            {
                m_options.transportPort = m_options.port + 1;
            }

            if (m_options.hostname.empty())
            {
                m_options.hostname = L"0.0.0.0";
            }

            m_remoteContext.Listen(m_options.hostname, m_options.port, m_options.transportPort);
        }
        else
        {
            if (m_options.hostname.empty())
            {
                m_options.hostname = L"127.0.0.1";
            }

            m_remoteContext.Connect(m_options.hostname, m_options.port);
        }
    }
    catch (winrt::hresult_error& e)
    {
        if (m_options.listen)
        {
            DebugLog(L"Listen failed with hr = 0x%08X", e.code());
        }
        else
        {
            DebugLog(L"Connect failed with hr = 0x%08X", e.code());
        }
    }
}

void SampleRemoteApp::LoadPosition()
{
    std::lock_guard remoteContextLock(m_remoteContextAccess);
    if (!m_remoteContext)
    {
        return;
    }

    auto storeRequest = SpatialAnchorManager::RequestStoreAsync();
    storeRequest.Completed([this](winrt::Windows::Foundation::IAsyncOperation<SpatialAnchorStore> result, auto asyncStatus) {
        if (result.Status() != winrt::Windows::Foundation::AsyncStatus::Completed)
        {
            return;
        }

        const SpatialAnchorStore& store = result.GetResults();
        if (store)
        {
            auto anchors = store.GetAllSavedAnchors();
            if (anchors.HasKey(L"position"))
            {
                auto position = anchors.Lookup(L"position");
                auto positionToOrigin = position.CoordinateSystem().TryGetTransformTo(m_referenceFrame.CoordinateSystem());
                if (positionToOrigin)
                {
                    const float3 res = transform(float3::zero(), positionToOrigin.Value());
                    m_spinningCubeRenderer->SetPosition(res);
                    OutputDebugStringW(L"Loaded cube position from SpatialAnchorStore.\n");
                }
            }
        }
    });
}

void SampleRemoteApp::SavePosition()
{
    std::lock_guard remoteContextLock(m_remoteContextAccess);
    if (!m_remoteContext)
    {
        return;
    }

    auto position = SpatialAnchor::TryCreateRelativeTo(m_referenceFrame.CoordinateSystem(), m_spinningCubeRenderer->GetPosition());

    auto storeRequest = SpatialAnchorManager::RequestStoreAsync();
    storeRequest.Completed([position](winrt::Windows::Foundation::IAsyncOperation<SpatialAnchorStore> result, auto asyncStatus) {
        if (result.Status() != winrt::Windows::Foundation::AsyncStatus::Completed)
        {
            return;
        }

        const SpatialAnchorStore& store = result.GetResults();
        if (store)
        {
            store.Clear();
            if (store.TrySave(L"position", position))
            {
                OutputDebugStringW(L"Saved cube position to SpatialAnchorStore.\n");
            }
        }
    });
}

winrt::fire_and_forget SampleRemoteApp::ExportPosition()
{
    // Keep this variable although it is not used directly! This variable ensures that there is at least one reference to the remoteContext.
    winrt::Microsoft::Holographic::AppRemoting::RemoteContext remoteContext = nullptr;

    {
        std::lock_guard remoteContextLock(m_remoteContextAccess);

        if (!m_remoteContext)
        {
            return;
        }
        remoteContext = m_remoteContext;
    }

    const auto purpose = winrt::Windows::Perception::Spatial::SpatialAnchorExportPurpose::Sharing;

    auto position = SpatialAnchor::TryCreateRelativeTo(m_referenceFrame.CoordinateSystem(), m_spinningCubeRenderer->GetPosition());

    co_await winrt::resume_background();

    try
    {
        using namespace winrt::Windows::Storage::Streams;

        auto status = SpatialAnchorExporter::RequestAccessAsync().get();
        if (status != SpatialPerceptionAccessStatus::Allowed)
        {
            co_return;
        }

        auto exporter = SpatialAnchorExporter::GetDefault();
        auto sufficient = exporter.GetAnchorExportSufficiencyAsync(position, purpose).get();

        if (!sufficient.IsMinimallySufficient())
        {
            OutputDebugStringW(L"\r\nNot enough data for the anchor to export. Try again later.");
            co_return;
        }

        {
            InMemoryRandomAccessStream stream = InMemoryRandomAccessStream();
            bool result = exporter.TryExportAnchorAsync(position, purpose, stream.GetOutputStreamAt(0)).get();

            uint64_t size = stream.Size();
            if (size > UINT32_MAX)
            {
                co_return;
            }

            std::vector<uint8_t> data;
            data.resize(static_cast<size_t>(size));

            DataReader reader(stream);
            reader.LoadAsync(static_cast<uint32_t>(size));
            reader.ReadBytes(winrt::array_view(data.data(), data.data() + data.size()));

            {
                std::wostringstream debugMsg;
                debugMsg << "\r\nSuccessfully exported anchor. Size is " << size << " bytes.";
                OutputDebugStringW(debugMsg.str().c_str());
            }
        }
    }
    catch (...)
    {
    }
}

void SampleRemoteApp::InitializeAccessToFeatures()
{
    // Request access to eyes pose data.
    RequestEyesPoseAccess();

    // Request access to scene observer.
    RequestSceneObserverAccess();

    // Request updates from qr code watcher.
    RequestQRCodeWatcherUpdates();

    // If not in standalone mode the spatial surface renderer needs to get recreated on every connect, because its SpatialSurfaceObserver
    // stops working on disconnect. Uncomment the line below to render spatial surfaces. This creates the SpatialSurfaceMeshRenderer and
    // requests access from the SpatialSurfaceObserver.
    // m_spatialSurfaceMeshRenderer = std::make_unique<SpatialSurfaceMeshRenderer>(m_deviceResources);
}

void SampleRemoteApp::RequestEyesPoseAccess()
{
    try
    {
        auto asyncOpertation = winrt::Windows::Perception::People::EyesPose::RequestAccessAsync();
        asyncOpertation.Completed(
            [this](winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::UI::Input::GazeInputAccessStatus> result, auto asyncStatus) {
                winrt::Windows::UI::Input::GazeInputAccessStatus status = result.GetResults();
                switch (status)
                {
                    case winrt::Windows::UI::Input::GazeInputAccessStatus::Unspecified:
                        OutputDebugStringA("ParseGazeInputResponseData Unspecified\n");
                        break;
                    case winrt::Windows::UI::Input::GazeInputAccessStatus::Allowed:
                        OutputDebugStringA("ParseGazeInputResponseData Allowed\n");
                        break;
                    case winrt::Windows::UI::Input::GazeInputAccessStatus::DeniedByUser:
                        OutputDebugStringA("ParseGazeInputResponseData DeniedByUser\n");
                        break;
                    case winrt::Windows::UI::Input::GazeInputAccessStatus::DeniedBySystem:
                        OutputDebugStringA("ParseGazeInputResponseData DeniedBySystem\n");
                        break;
                    default:
                        break;
                }
            });
    }
    catch (winrt::hresult_error&)
    {
    }
}

winrt::fire_and_forget SampleRemoteApp::RequestSceneObserverAccess()
{
    m_hasSceneObserverAccess = false;

    try
    {
        if (!SceneObserver::IsSupported())
        {
            OutputDebugStringA("SceneObserver Unsupported\n");
            co_return;
        }

        auto keepAlive = shared_from_this();
        SceneObserverAccessStatus accessStatus = co_await SceneObserver::RequestAccessAsync();

        if (accessStatus == winrt::Microsoft::MixedReality::SceneUnderstanding::SceneObserverAccessStatus::Allowed)
        {
            OutputDebugStringA("SceneObserver Access Allowed\n");
            m_hasSceneObserverAccess = true;
        }
        else
        {
            OutputDebugStringA("SceneObserver Access Denied\n");
            m_hasSceneObserverAccess = false;
        }
    }
    catch (...)
    {
        OutputDebugStringA("SceneObserver Access Failed\n");
    }
}

void SampleRemoteApp::ToggleSceneUnderstanding()
{
    if (!m_hasSceneObserverAccess)
    {
        return;
    }

    // Create Query settings for the scene update
    SceneQuerySettings querySettings{};
    querySettings.EnableSceneObjectQuads = true;                             // Requests that the scene updates quads.
    querySettings.EnableSceneObjectMeshes = true;                            // Requests that the scene updates watertight mesh data.
    querySettings.EnableOnlyObservedSceneObjects = false;                    // Do not explicitly turn off quad inference.
    querySettings.EnableWorldMesh = true;                                    // Requests a static version of the spatial mapping mesh.
    querySettings.RequestedMeshLevelOfDetail = SceneMeshLevelOfDetail::Fine; // Requests the finest LOD of the static spatial mapping mesh.

    SceneObserver::ComputeAsync(querySettings, 10.0f)
        .Completed([&, weakThis = weak_from_this()](winrt::Windows::Foundation::IAsyncOperation<Scene> result, auto asyncStatus) {
            if (result.Status() != winrt::Windows::Foundation::AsyncStatus::Completed)
            {
                return;
            }

            if (auto strongThis = weakThis.lock())
            {
                SpatialStationaryFrameOfReference updateLocation = m_locator.CreateStationaryFrameOfReferenceAtCurrentLocation();
                Scene scene = result.GetResults();

                m_sceneUnderstandingRenderer->SetScene(scene, updateLocation);
                m_sceneUnderstandingRenderer->ToggleRenderingType();
            }
        });
}

winrt::fire_and_forget SampleRemoteApp::RequestQRCodeWatcherUpdates()
{
    try
    {
        if (!QRCodeWatcher::IsSupported())
        {
            OutputDebugStringA("QRCodeWatcher Unsupported\n");
            return;
        }

        auto keepAlive = shared_from_this();
        QRCodeWatcherAccessStatus accessStatus = co_await QRCodeWatcher::RequestAccessAsync();

        if (accessStatus == winrt::Microsoft::MixedReality::QR::QRCodeWatcherAccessStatus::Allowed)
        {
            OutputDebugStringA("QRCodeWatcher Access Allowed\n");

            m_qrWatcher = QRCodeWatcher();

            m_qrAddedRevoker = m_qrWatcher.Added(
                winrt::auto_revoke,
                [&, weakThis = weak_from_this()](
                    const winrt::Microsoft::MixedReality::QR::QRCodeWatcher&,
                    const winrt::Microsoft::MixedReality::QR::QRCodeAddedEventArgs& args) {
                    if (auto strongThis = weakThis.lock())
                    {
                        m_qrCodeRenderer->OnAddedQRCode(args.Code());
                    }
                });

            m_qrUpdatedRevoker = m_qrWatcher.Updated(
                winrt::auto_revoke,
                [&, weakThis = weak_from_this()](
                    const winrt::Microsoft::MixedReality::QR::QRCodeWatcher&,
                    const winrt::Microsoft::MixedReality::QR::QRCodeUpdatedEventArgs& args) {
                    if (auto strongThis = weakThis.lock())
                    {
                        m_qrCodeRenderer->OnUpdatedQRCode(args.Code());
                    }
                });

            m_qrWatcher.Start();
        }
        else
        {
            OutputDebugStringA("QRCodeWatcher Access Denied\n");
        }
    }
    catch (...)
    {
        OutputDebugStringA("QRCodeWatcher Access Failed\n");
    }
}

void SampleRemoteApp::UnregisterHolographicEventHandlers()
{
    if (m_holographicSpace != nullptr)
    {
        m_holographicSpace.CameraAdded(m_cameraAddedToken);
        m_holographicSpace.CameraRemoved(m_cameraRemovedToken);
    }

    if (m_locator != nullptr)
    {
        m_locator.LocatabilityChanged(m_locatabilityChangedToken);
    }
}

void SampleRemoteApp::ShutdownRemoteContext()
{
    std::lock_guard remoteContextLock(m_remoteContextAccess);

    if (m_remoteContext != nullptr)
    {
        m_onConnectedEventRevoker.revoke();
        m_onSendFrameEventRevoker.revoke();
        m_onDataChannelCreatedEventRevoker.revoke();

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
        {
            std::lock_guard lock(m_customDataChannelLock);
            m_customChannelDataReceivedEventRevoker.revoke();
            m_customChannelClosedEventRevoker.revoke();
            m_customDataChannel = nullptr;
        }
#endif

        m_remoteContext.Close();
        m_remoteContext = nullptr;
    }
}

void SampleRemoteApp::OnDeviceLost()
{
    m_spinningCubeRenderer->ReleaseDeviceDependentResources();
    m_spatialInputRenderer->ReleaseDeviceDependentResources();

    m_qrCodeRenderer->ReleaseDeviceDependentResources();
    m_sceneUnderstandingRenderer->ReleaseDeviceDependentResources();

    if (m_spatialSurfaceMeshRenderer)
    {
        m_spatialSurfaceMeshRenderer->ReleaseDeviceDependentResources();
    }
}

void SampleRemoteApp::OnDeviceRestored()
{
    m_spinningCubeRenderer->CreateDeviceDependentResources();
    m_spatialInputRenderer->CreateDeviceDependentResources();

    m_qrCodeRenderer->CreateDeviceDependentResources();
    m_sceneUnderstandingRenderer->CreateDeviceDependentResources();

    if (m_spatialSurfaceMeshRenderer)
    {
        m_spatialSurfaceMeshRenderer->CreateDeviceDependentResources();
    }
}

void SampleRemoteApp::OnCameraAdded(const HolographicSpace& sender, const HolographicSpaceCameraAddedEventArgs& args)
{
    winrt::Windows::Foundation::Deferral deferral = args.GetDeferral();
    auto holographicCamera = args.Camera();

    HolographicViewConfiguration viewConfig = holographicCamera.ViewConfiguration();
    viewConfig.PixelFormat(DirectXPixelFormat::B8G8R8A8UIntNormalized);

    create_task([this, deferral, holographicCamera]() {
        m_deviceResources->AddHolographicCamera(holographicCamera);

        deferral.Complete();
    });
}

void SampleRemoteApp::OnCameraRemoved(const HolographicSpace& sender, const HolographicSpaceCameraRemovedEventArgs& args)
{
    m_deviceResources->RemoveHolographicCamera(args.Camera());
}

void SampleRemoteApp::OnLocatabilityChanged(const SpatialLocator& sender, const winrt::Windows::Foundation::IInspectable& args)
{
    const wchar_t* locatability = L"";
    switch (sender.Locatability())
    {
        case SpatialLocatability::Unavailable:
            locatability = L"Unavailable";
            break;

        case SpatialLocatability::PositionalTrackingActivating:
            locatability = L"PositionalTrackingActivating";
            break;

        case SpatialLocatability::OrientationOnly:
            locatability = L"OrientationOnly";
            break;

        case SpatialLocatability::PositionalTrackingInhibited:
            locatability = L"PositionalTrackingInhibited";
            break;

        case SpatialLocatability::PositionalTrackingActive:
            locatability = L"PositionalTrackingActive";
            break;
    }

    winrt::hstring message = L"Positional tracking is " + winrt::to_hstring(locatability) + L".\n";
    OutputDebugStringW(message.data());
}

void SampleRemoteApp::OnConnected()
{
    WindowUpdateTitle();

    InitializeAccessToFeatures();

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
    if (m_remoteContext)
    {
        m_remoteContext.CreateDataChannel(0, DataChannelPriority::Low);
    }
#endif
}

void SampleRemoteApp::OnDisconnected(winrt::Microsoft::Holographic::AppRemoting::ConnectionFailureReason failureReason)
{
    DebugLog(L"Disconnected with reason %d", failureReason);

    {
        std::lock_guard remoteContextLock(m_remoteContextAccess);
        m_disconnectPending = false;
    }

    // Reconnect if this is a transient failure.
    if (failureReason == ConnectionFailureReason::DisconnectRequest || failureReason == ConnectionFailureReason::PeerDisconnectRequest)
    {
        ShutdownRemoteContext();
    }
    else if (
        failureReason == ConnectionFailureReason::HandshakeUnreachable || failureReason == ConnectionFailureReason::TransportUnreachable ||
        failureReason == ConnectionFailureReason::ConnectionLost)
    {
        if (m_options.autoReconnect)
        {
            DebugLog(L"Reconnecting...");
            ConnectOrListen();
        }
        else
        {
            ShutdownRemoteContext();
        }
    }
    // Failure reason None indicates a normal disconnect.
    else if (failureReason != ConnectionFailureReason::None)
    {
        DebugLog(L"Disconnected with unrecoverable error, not attempting to reconnect.");
        ShutdownRemoteContext();
    }

    WindowUpdateTitle();

    m_spatialSurfaceMeshRenderer = nullptr;

    m_hasSceneObserverAccess = false;

    m_sceneUnderstandingRenderer->Reset();
    m_qrCodeRenderer->Reset();
}

void SampleRemoteApp::OnSendFrame(const winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface& texture)
{
    if (m_options.showPreview)
    {
        winrt::com_ptr<ID3D11Device1> spDevice;
        spDevice.copy_from(GetDeviceResources()->GetD3DDevice());

        winrt::com_ptr<ID3D11Texture2D> spBackBuffer;
        winrt::check_hresult(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), spBackBuffer.put_void()));

        winrt::com_ptr<ID3D11Texture2D> texturePtr;
        {
            winrt::com_ptr<ID3D11Resource> resource;
            winrt::com_ptr<::IInspectable> inspectable = texture.as<::IInspectable>();
            winrt::com_ptr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> dxgiInterfaceAccess;
            winrt::check_hresult(inspectable->QueryInterface(__uuidof(dxgiInterfaceAccess), dxgiInterfaceAccess.put_void()));
            winrt::check_hresult(dxgiInterfaceAccess->GetInterface(__uuidof(resource), resource.put_void()));
            resource.as(texturePtr);
        }

        // Get source/dest dimensions and adjust copy rect and destination position to avoid D3D errors
        D3D11_TEXTURE2D_DESC backBufferDesc, textureDesc;
        spBackBuffer->GetDesc(&backBufferDesc);
        texturePtr->GetDesc(&textureDesc);

        UINT destX = 0, destY = 0;
        D3D11_BOX srcBox{0, 0, 0, textureDesc.Width, textureDesc.Height, 1};

        if (backBufferDesc.Width < textureDesc.Width)
        {
            // Target (BackBuffer) narrower than source (Texture)
            srcBox.left = (textureDesc.Width - backBufferDesc.Width) / 2;
            srcBox.right = srcBox.left + backBufferDesc.Width;
        }
        else if (backBufferDesc.Width > textureDesc.Width)
        {
            // Target (BackBuffer) wider than source (Texture)
            destX = (backBufferDesc.Width - textureDesc.Width) / 2;
        }

        if (backBufferDesc.Height < textureDesc.Height)
        {
            // Target (BackBuffer) shorter than source (Texture)
            srcBox.top = (textureDesc.Height - backBufferDesc.Height) / 2;
            srcBox.bottom = srcBox.top + backBufferDesc.Height;
        }
        else if (backBufferDesc.Height > textureDesc.Height)
        {
            // Target (BackBuffer) taller than source (Texture)
            destY = (backBufferDesc.Height - textureDesc.Height) / 2;
        }

        // Copy texture to back buffer
        GetDeviceResources()->UseD3DDeviceContext([&](auto context) {
            context->CopySubresourceRegion(
                spBackBuffer.get(), // dest
                0,                  // dest subresource
                destX,
                destY,
                0,                // dest x, y, z
                texturePtr.get(), // source
                0,                // source subresource
                &srcBox);         // source box, null means the entire resource
        });

        WindowPresentSwapChain();
    }
}

void SampleRemoteApp::WindowCreateSwapChain(const winrt::com_ptr<ID3D11Device1>& device)
{
    std::lock_guard _lg(m_deviceLock);

    DXGI_SWAP_CHAIN_DESC1 desc = {0};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo = false;
    desc.SampleDesc.Count = 1; // Don't use multi-sampling.
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 3; // Triple buffered
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.Flags = 0;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    desc.Scaling = DXGI_SCALING_STRETCH;

    m_swapChain = nullptr;

    if (m_window)
    {
        m_swapChain = m_window->CreateSwapChain(device, &desc);
    }
}

void SampleRemoteApp::WindowPresentSwapChain()
{
    HRESULT hr = m_swapChain->Present(0, 0);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        // The D3D device is lost.
        // This should be handled after the frame is complete.
        m_swapChain = nullptr;
    }
    else
    {
        winrt::check_hresult(hr);
    }
}

void SampleRemoteApp::WindowUpdateTitle()
{
    std::wstring title = TITLE_TEXT;
    std::wstring separator = TITLE_SEPARATOR;

    uint32_t fps = std::min<uint32_t>(120, m_framesPerSecond);
    title += separator + std::to_wstring(fps) + L" fps";

    // Title | {ip} | {State} [| Press Space to Connect] [| Preview Disabled (p toggles)]
    title += separator + m_options.hostname;
    {
        std::lock_guard remoteContextLock(m_remoteContextAccess);
        if (m_remoteContext)
        {
            auto connectionState = m_remoteContext.ConnectionState();
            title +=
                separator + (m_isInitialized ? StreamerConnectionStateToString(connectionState, m_disconnectPending) : L"Initializing");
            title += separator + ((connectionState == ConnectionState::Disconnected) ? TITLE_CONNECT_TEXT : TITLE_DISCONNECT_TEXT);
        }
        else if (!m_isStandalone)
        {
            title += separator + TITLE_CONNECT_TEXT;
        }

        if (!m_isStandalone)
        {
            title += separator + (m_options.showPreview ? TITLE_DISABLE_PREVIEW_TEXT : TITLE_ENABLE_PREVIEW_TEXT);
        }
    }

    if (m_window)
    {
        m_window->SetWindowTitle(title);
    }
}

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
void SampleRemoteApp::OnCustomDataChannelDataReceived(winrt::array_view<const uint8_t> dataView)
{
    // TODO: React on data received via the custom data channel here.
    OutputDebugString(TEXT("Response Received.\n"));
}

void SampleRemoteApp::OnCustomDataChannelClosed()
{
    std::lock_guard lock(m_customDataChannelLock);
    if (m_customDataChannel)
    {
        m_customChannelDataReceivedEventRevoker.revoke();
        m_customChannelClosedEventRevoker.revoke();
        m_customDataChannel = nullptr;
    }
}
#endif

std::shared_ptr<IRemoteAppHolographic> CreateRemoteAppHolographic()
{
    return std::make_shared<SampleRemoteApp>();
}
