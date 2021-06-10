//*********************************************************
//    Copyright (c) Microsoft. All rights reserved.
//
//    Apache 2.0 License
//
//    You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
//    implied. See the License for the specific language governing
//    permissions and limitations under the License.
//
//*********************************************************

#include "pch.h"
#include "OpenXrProgram.h"
#include "DxUtility.h"

#include <filesystem>
#include <fstream>
#include <queue>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#include <SampleShared/SampleWindowWin32.h>
#endif

// #define ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE

namespace {
    constexpr DirectX::XMVECTORF32 clearColor = {0.392156899f, 0.584313750f, 0.929411829f, 1.000000000f};

    struct ImplementOpenXrProgram : sample::IOpenXrProgram {
        ImplementOpenXrProgram(std::string applicationName,
                               std::unique_ptr<sample::IGraphicsPluginD3D11> graphicsPlugin,
                               const sample::AppOptions& options)
            : m_applicationName(std::move(applicationName))
            , m_graphicsPlugin(std::move(graphicsPlugin))
            , m_options(options) {
        }

        void Run() override {
            if (!m_options.isStandalone) {
                m_usingRemotingRuntime = EnableRemotingXR();

                if (m_usingRemotingRuntime) {
                    PrepareRemotingEnvironment();
                } else {
                    DEBUG_PRINT("RemotingXR runtime not available. Running with default OpenXR runtime.");
                }
            }

            CreateInstance();
            CreateActions();

            InitializeSystem();
            InitializeDevice();

            CreateWindowWin32();

            bool requestRestart = false;
            do {
                while (true) {
                    bool exitRenderLoop = false;
                    ProcessEvents(&exitRenderLoop, &requestRestart);
                    ProcessWindowEventsWin32(&exitRenderLoop, &requestRestart);
                    if (exitRenderLoop) {
                        break;
                    }

                    if (m_sessionRunning) {
#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
                        auto timeDelta = std::chrono::high_resolution_clock::now() - m_customDataChannelSendTime;
                        if (timeDelta > std::chrono::seconds(5)) {
                            m_customDataChannelSendTime = std::chrono::high_resolution_clock::now();

                            if (!m_userDataChannelDestroyed && m_usingRemotingRuntime) {
                                SendDataViaUserDataChannel(m_userDataChannel);
                            }
                        }
#endif

                        try {
                            PollActions();
                            RenderFrame();
                        } catch (const std::logic_error& ex) {
                            DEBUG_PRINT("Render Loop Exception: %s\n", ex.what());
                        }
                    } else {
                        // Throttle loop since xrWaitFrame won't be called.
                        using namespace std::chrono_literals;
                        std::this_thread::sleep_for(250ms);
                    }
                }

                if (requestRestart) {
                    PrepareSessionRestart();
                }
            } while (requestRestart);
        }

    private:
#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
        void CreateUserDataChannel() {
            CHECK(m_instance.Get() != XR_NULL_HANDLE);
            CHECK(m_systemId != XR_NULL_SYSTEM_ID);

            XrRemotingDataChannelCreateInfoMSFT channelInfo{static_cast<XrStructureType>(XR_TYPE_REMOTING_DATA_CHANNEL_CREATE_INFO_MSFT)};
            channelInfo.channelId = 0;
            channelInfo.channelPriority = XR_REMOTING_DATA_CHANNEL_PRIORITY_LOW_MSFT;
            CHECK_XRCMD(m_extensions.xrCreateRemotingDataChannelMSFT(m_instance.Get(), m_systemId, &channelInfo, &m_userDataChannel));
        }

        void DestroyUserDataChannel(XrRemotingDataChannelMSFT channelHandle) {
            CHECK_XRCMD(m_extensions.xrDestroyRemotingDataChannelMSFT(channelHandle));
        }

        void SendDataViaUserDataChannel(XrRemotingDataChannelMSFT channelHandle) {
            XrRemotingDataChannelStateMSFT channelState{static_cast<XrStructureType>(XR_TYPE_REMOTING_DATA_CHANNEL_STATE_MSFT)};
            CHECK_XRCMD(m_extensions.xrGetRemotingDataChannelStateMSFT(channelHandle, &channelState));

            if (channelState.connectionStatus == XR_REMOTING_DATA_CHANNEL_STATUS_OPENED_MSFT) {
                // Only send the packet if the send queue is smaller than 1MiB
                if (channelState.sendQueueSize >= 1 * 1024 * 1024) {
                    return;
                }

                DEBUG_PRINT("Holographic Remoting: SendDataViaUserDataChannel.");
                uint8_t data[] = {17};

                XrRemotingDataChannelSendDataInfoMSFT sendInfo{
                    static_cast<XrStructureType>(XR_TYPE_REMOTING_DATA_CHANNEL_SEND_DATA_INFO_MSFT)};
                sendInfo.data = data;
                sendInfo.size = sizeof(data);
                sendInfo.guaranteedDelivery = true;
                CHECK_XRCMD(m_extensions.xrSendRemotingDataMSFT(channelHandle, &sendInfo));
            }
        }
#endif
        bool EnableRemotingXR() {
            wchar_t executablePath[MAX_PATH];
            if (GetModuleFileNameW(NULL, executablePath, ARRAYSIZE(executablePath)) == 0) {
                return false;
            }

            std::filesystem::path filename(executablePath);
            filename = filename.replace_filename("RemotingXR.json");

            if (std::filesystem::exists(filename)) {
                SetEnvironmentVariableW(L"XR_RUNTIME_JSON", filename.c_str());
                return true;
            }

            return false;
        }

        void PrepareRemotingEnvironment() {
            if (!m_options.secureConnection) {
                return;
            }

            if (m_options.authenticationToken.empty()) {
                throw std::logic_error("Authentication token must be specified for secure connections.");
            }

            if (m_options.listen) {
                if (m_options.certificateStore.empty() || m_options.subjectName.empty()) {
                    throw std::logic_error("Certificate store and subject name must be specified for secure listening.");
                }

                constexpr size_t maxCertStoreSize = 1 << 20;
                std::ifstream certStoreStream(m_options.certificateStore, std::ios::binary);
                certStoreStream.seekg(0, std::ios_base::end);
                const size_t certStoreSize = certStoreStream.tellg();
                if (!certStoreStream.good() || certStoreSize == 0 || certStoreSize > maxCertStoreSize) {
                    throw std::logic_error("Error reading certificate store.");
                }
                certStoreStream.seekg(0, std::ios_base::beg);
                m_certificateStore.resize(certStoreSize);
                certStoreStream.read(reinterpret_cast<char*>(m_certificateStore.data()), certStoreSize);
                if (certStoreStream.fail()) {
                    throw std::logic_error("Error reading certificate store.");
                }
            }
        }

        bool LoadGrammarFile(std::vector<uint8_t>& grammarFileContent) {
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
            char executablePath[MAX_PATH];
            if (GetModuleFileNameA(NULL, executablePath, ARRAYSIZE(executablePath)) == 0) {
                return false;
            }

            std::filesystem::path filename(executablePath);
            filename = filename.replace_filename("OpenXRSpeechGrammar.xml");

            if (!std::filesystem::exists(filename)) {
                return false;
            }

            std::string grammarFilePath{filename.generic_u8string()};
            std::ifstream grammarFileStream(grammarFilePath, std::ios::binary);
            const size_t grammarFileSize = std::filesystem::file_size(filename);
            if (!grammarFileStream.good() || grammarFileSize == 0) {
                return false;
            }

            grammarFileContent.resize(grammarFileSize);
            grammarFileStream.read(reinterpret_cast<char*>(grammarFileContent.data()), grammarFileSize);
            if (grammarFileStream.fail()) {
                return false;
            }

            return true;
#else
            return false;
#endif
        }

        void InitializeSpeechRecognition(XrRemotingSpeechInitInfoMSFT& speechInitInfo) {
            // Specify the speech recognition language.
            strcpy_s(speechInitInfo.language, "en-US");

            // Initialize the dictionary.
            m_dictionaryEntries = {"Red", "Blue", "Green", "Aquamarine", "Default"};
            speechInitInfo.dictionaryEntries = m_dictionaryEntries.data();
            speechInitInfo.dictionaryEntriesCount = static_cast<uint32_t>(m_dictionaryEntries.size());

            // Initialize the grammar file if it exists.
            if (LoadGrammarFile(m_grammarFileContent)) {
                speechInitInfo.grammarFileSize = static_cast<uint32_t>(m_grammarFileContent.size());
                speechInitInfo.grammarFileContent = m_grammarFileContent.data();
            }
        }

        void CreateInstance() {
            CHECK(m_instance.Get() == XR_NULL_HANDLE);

            // Build out the extensions to enable. Some extensions are required and some are optional.
            const std::vector<const char*> enabledExtensions = SelectExtensions();

            // Create the instance with enabled extensions.
            XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
            createInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
            createInfo.enabledExtensionNames = enabledExtensions.data();

            createInfo.applicationInfo = {"SampleRemoteOpenXr", 1, "", 1, XR_CURRENT_API_VERSION};
            strcpy_s(createInfo.applicationInfo.applicationName, m_applicationName.c_str());

            CHECK_XRCMD(xrCreateInstance(&createInfo, m_instance.Put()));

            m_extensions.PopulateDispatchTable(m_instance.Get());
        }

        std::vector<const char*> SelectExtensions() {
            // Fetch the list of extensions supported by the runtime.
            uint32_t extensionCount;
            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr));
            std::vector<XrExtensionProperties> extensionProperties(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data()));

            std::vector<const char*> enabledExtensions;

            // Add a specific extension to the list of extensions to be enabled, if it is supported.
            auto EnableExtensionIfSupported = [&](const char* extensionName) {
                for (uint32_t i = 0; i < extensionCount; i++) {
                    if (strcmp(extensionProperties[i].extensionName, extensionName) == 0) {
                        enabledExtensions.push_back(extensionName);
                        return true;
                    }
                }
                return false;
            };

            // D3D11 extension is required for this sample, so check if it's supported.
            CHECK(EnableExtensionIfSupported(XR_KHR_D3D11_ENABLE_EXTENSION_NAME));

#if UWP
            // Require XR_EXT_win32_appcontainer_compatible extension when building in UWP context.
            CHECK(EnableExtensionIfSupported(XR_EXT_WIN32_APPCONTAINER_COMPATIBLE_EXTENSION_NAME));
#endif

            // If using the remoting runtime, the remoting extension must be present as well
            if (m_usingRemotingRuntime) {
                CHECK(EnableExtensionIfSupported(XR_MSFT_HOLOGRAPHIC_REMOTING_EXTENSION_NAME));
                CHECK(EnableExtensionIfSupported(XR_MSFT_HOLOGRAPHIC_REMOTING_FRAME_MIRRORING_EXTENSION_NAME));
                CHECK(EnableExtensionIfSupported(XR_MSFT_HOLOGRAPHIC_REMOTING_SPEECH_EXTENSION_NAME));
            }

            // Additional optional extensions for enhanced functionality. Track whether enabled in m_optionalExtensions.
            m_optionalExtensions.DepthExtensionSupported = EnableExtensionIfSupported(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
            m_optionalExtensions.UnboundedRefSpaceSupported = EnableExtensionIfSupported(XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME);
            m_optionalExtensions.SpatialAnchorSupported = EnableExtensionIfSupported(XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);

            return enabledExtensions;
        }

        void CreateActions() {
            CHECK(m_instance.Get() != XR_NULL_HANDLE);

            // Create an action set.
            {
                XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
                strcpy_s(actionSetInfo.actionSetName, "place_hologram_action_set");
                strcpy_s(actionSetInfo.localizedActionSetName, "Placement");
                CHECK_XRCMD(xrCreateActionSet(m_instance.Get(), &actionSetInfo, m_actionSet.Put()));
            }

            // Create actions.
            {
                // Enable subaction path filtering for left or right hand.
                m_subactionPaths[LeftSide] = GetXrPath("/user/hand/left");
                m_subactionPaths[RightSide] = GetXrPath("/user/hand/right");

                // Create an input action to place a hologram.
                {
                    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                    strcpy_s(actionInfo.actionName, "place_hologram");
                    strcpy_s(actionInfo.localizedActionName, "Place Hologram");
                    actionInfo.countSubactionPaths = (uint32_t)m_subactionPaths.size();
                    actionInfo.subactionPaths = m_subactionPaths.data();
                    CHECK_XRCMD(xrCreateAction(m_actionSet.Get(), &actionInfo, m_placeAction.Put()));
                }

                // Create an input action getting the left and right hand poses.
                {
                    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                    strcpy_s(actionInfo.actionName, "hand_pose");
                    strcpy_s(actionInfo.localizedActionName, "Hand Pose");
                    actionInfo.countSubactionPaths = (uint32_t)m_subactionPaths.size();
                    actionInfo.subactionPaths = m_subactionPaths.data();
                    CHECK_XRCMD(xrCreateAction(m_actionSet.Get(), &actionInfo, m_poseAction.Put()));
                }

                // Create an output action for vibrating the left and right controller.
                {
                    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                    actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
                    strcpy_s(actionInfo.actionName, "vibrate");
                    strcpy_s(actionInfo.localizedActionName, "Vibrate");
                    actionInfo.countSubactionPaths = (uint32_t)m_subactionPaths.size();
                    actionInfo.subactionPaths = m_subactionPaths.data();
                    CHECK_XRCMD(xrCreateAction(m_actionSet.Get(), &actionInfo, m_vibrateAction.Put()));
                }

                // Create an input action to exit the session.
                {
                    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                    strcpy_s(actionInfo.actionName, "exit_session");
                    strcpy_s(actionInfo.localizedActionName, "Exit session");
                    actionInfo.countSubactionPaths = (uint32_t)m_subactionPaths.size();
                    actionInfo.subactionPaths = m_subactionPaths.data();
                    CHECK_XRCMD(xrCreateAction(m_actionSet.Get(), &actionInfo, m_exitAction.Put()));
                }
            }

            // Set up suggested bindings for the simple_controller profile.
            {
                std::vector<XrActionSuggestedBinding> bindings;
                bindings.push_back({m_placeAction.Get(), GetXrPath("/user/hand/right/input/select/click")});
                bindings.push_back({m_placeAction.Get(), GetXrPath("/user/hand/left/input/select/click")});
                bindings.push_back({m_poseAction.Get(), GetXrPath("/user/hand/right/input/grip/pose")});
                bindings.push_back({m_poseAction.Get(), GetXrPath("/user/hand/left/input/grip/pose")});
                bindings.push_back({m_vibrateAction.Get(), GetXrPath("/user/hand/right/output/haptic")});
                bindings.push_back({m_vibrateAction.Get(), GetXrPath("/user/hand/left/output/haptic")});
                bindings.push_back({m_exitAction.Get(), GetXrPath("/user/hand/right/input/menu/click")});
                bindings.push_back({m_exitAction.Get(), GetXrPath("/user/hand/left/input/menu/click")});

                XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
                suggestedBindings.interactionProfile = GetXrPath("/interaction_profiles/khr/simple_controller");
                suggestedBindings.suggestedBindings = bindings.data();
                suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
                CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance.Get(), &suggestedBindings));
            }
        }

        XrResult AuthenticationRequestCallback(XrRemotingAuthenticationTokenRequestMSFT* authenticationTokenRequest) {
            const std::string tokenUtf8 = m_options.authenticationToken;
            const uint32_t tokenSize = static_cast<uint32_t>(tokenUtf8.size() + 1); // for null-termination
            if (authenticationTokenRequest->tokenCapacityIn >= tokenSize) {
                memcpy(authenticationTokenRequest->tokenBuffer, tokenUtf8.c_str(), tokenSize);
                authenticationTokenRequest->tokenSizeOut = tokenSize;
                return XR_SUCCESS;
            } else {
                authenticationTokenRequest->tokenSizeOut = tokenSize;
                return XR_ERROR_SIZE_INSUFFICIENT;
            }
        }

        static XrResult XRAPI_CALL
        AuthenticationRequestCallbackStatic(XrRemotingAuthenticationTokenRequestMSFT* authenticationTokenRequest) {
            if (!authenticationTokenRequest->context) {
                return XR_ERROR_RUNTIME_FAILURE;
            }

            return reinterpret_cast<ImplementOpenXrProgram*>(authenticationTokenRequest->context)
                ->AuthenticationRequestCallback(authenticationTokenRequest);
        }

        XrResult AuthenticationValidationCallback(XrRemotingAuthenticationTokenValidationMSFT* authenticationTokenValidation) {
            const std::string tokenUtf8 = m_options.authenticationToken;
            authenticationTokenValidation->tokenValidOut =
                (authenticationTokenValidation->token != nullptr && tokenUtf8 == authenticationTokenValidation->token);
            return XR_SUCCESS;
        }

        static XrResult XRAPI_CALL
        AuthenticationValidationCallbackStatic(XrRemotingAuthenticationTokenValidationMSFT* authenticationTokenValidation) {
            if (!authenticationTokenValidation->context) {
                return XR_ERROR_RUNTIME_FAILURE;
            }

            return reinterpret_cast<ImplementOpenXrProgram*>(authenticationTokenValidation->context)
                ->AuthenticationValidationCallback(authenticationTokenValidation);
        }

        XrResult CertificateRequestCallback(XrRemotingServerCertificateRequestMSFT* serverCertificateRequest) {
            const std::string subjectNameUtf8 = m_options.subjectName;
            const std::string passPhraseUtf8 = m_options.keyPassphrase;

            const uint32_t certStoreSize = static_cast<uint32_t>(m_certificateStore.size());
            const uint32_t subjectNameSize = static_cast<uint32_t>(subjectNameUtf8.size() + 1); // for null-termination
            const uint32_t passPhraseSize = static_cast<uint32_t>(passPhraseUtf8.size() + 1);   // for null-termination

            serverCertificateRequest->certStoreSizeOut = certStoreSize;
            serverCertificateRequest->subjectNameSizeOut = subjectNameSize;
            serverCertificateRequest->keyPassphraseSizeOut = passPhraseSize;
            if (serverCertificateRequest->certStoreCapacityIn < certStoreSize ||
                serverCertificateRequest->subjectNameCapacityIn < subjectNameSize ||
                serverCertificateRequest->keyPassphraseCapacityIn < passPhraseSize) {
                return XR_ERROR_SIZE_INSUFFICIENT;
            }

            // All buffers have sufficient size, so fill in the data
            memcpy(serverCertificateRequest->certStoreBuffer, m_certificateStore.data(), certStoreSize);
            memcpy(serverCertificateRequest->subjectNameBuffer, subjectNameUtf8.c_str(), subjectNameSize);
            memcpy(serverCertificateRequest->keyPassphraseBuffer, passPhraseUtf8.c_str(), passPhraseSize);

            return XR_SUCCESS;
        }

        static XrResult XRAPI_CALL CertificateValidationCallbackStatic(XrRemotingServerCertificateRequestMSFT* serverCertificateRequest) {
            if (!serverCertificateRequest->context) {
                return XR_ERROR_RUNTIME_FAILURE;
            }

            return reinterpret_cast<ImplementOpenXrProgram*>(serverCertificateRequest->context)
                ->CertificateRequestCallback(serverCertificateRequest);
        }

        XrResult CertificateValidationCallback(XrRemotingServerCertificateValidationMSFT* serverCertificateValidation) {
            if (!serverCertificateValidation->systemValidationResult) {
                return XR_ERROR_RUNTIME_FAILURE; // We requested system validation to be performed
            }

            serverCertificateValidation->validationResultOut = *serverCertificateValidation->systemValidationResult;
            if (m_options.allowCertificateNameMismatch && serverCertificateValidation->validationResultOut.nameValidationResult ==
                                                              XR_REMOTING_CERTIFICATE_NAME_VALIDATION_RESULT_MISMATCH_MSFT) {
                serverCertificateValidation->validationResultOut.nameValidationResult =
                    XR_REMOTING_CERTIFICATE_NAME_VALIDATION_RESULT_MATCH_MSFT;
            }
            if (m_options.allowUnverifiedCertificateChain) {
                serverCertificateValidation->validationResultOut.trustedRoot = true;
            }

            return XR_SUCCESS;
        }

        static XrResult XRAPI_CALL
        CertificateValidationCallbackStatic(XrRemotingServerCertificateValidationMSFT* serverCertificateValidation) {
            if (!serverCertificateValidation->context) {
                return XR_ERROR_RUNTIME_FAILURE;
            }

            return reinterpret_cast<ImplementOpenXrProgram*>(serverCertificateValidation->context)
                ->CertificateValidationCallback(serverCertificateValidation);
        }

        void Disconnect() {
            XrRemotingDisconnectInfoMSFT disconnectInfo{static_cast<XrStructureType>(XR_TYPE_REMOTING_DISCONNECT_INFO_MSFT)};
            CHECK_XRCMD(m_extensions.xrRemotingDisconnectMSFT(m_instance.Get(), m_systemId, &disconnectInfo));
        }

        void ConnectOrListen() {
            if (!m_usingRemotingRuntime) {
                return;
            }

            XrRemotingConnectionStateMSFT connectionState;
            CHECK_XRCMD(m_extensions.xrRemotingGetConnectionStateMSFT(m_instance.Get(), m_systemId, &connectionState, nullptr));
            if (connectionState != XR_REMOTING_CONNECTION_STATE_DISCONNECTED_MSFT) {
                return;
            }

            // Apply remote context properties while disconnected.
            {
                XrRemotingRemoteContextPropertiesMSFT contextProperties;
                contextProperties =
                    XrRemotingRemoteContextPropertiesMSFT{static_cast<XrStructureType>(XR_TYPE_REMOTING_REMOTE_CONTEXT_PROPERTIES_MSFT)};
                contextProperties.enableAudio = false;
                contextProperties.maxBitrateKbps = 20000;
                contextProperties.videoCodec = XR_REMOTING_VIDEO_CODEC_H265_MSFT;
                contextProperties.depthBufferStreamResolution = XR_REMOTING_DEPTH_BUFFER_STREAM_RESOLUTION_HALF_MSFT;
                CHECK_XRCMD(m_extensions.xrRemotingSetContextPropertiesMSFT(m_instance.Get(), m_systemId, &contextProperties));
            }

            if (m_options.listen) {
                if (m_options.secureConnection) {
                    XrRemotingSecureConnectionServerCallbacksMSFT serverCallbacks;
                    serverCallbacks.context = this;
                    serverCallbacks.requestServerCertificateCallback = CertificateValidationCallbackStatic;
                    serverCallbacks.validateAuthenticationTokenCallback = AuthenticationValidationCallbackStatic;
                    serverCallbacks.authenticationRealm = m_options.authenticationRealm.c_str();
                    CHECK_XRCMD(
                        m_extensions.xrRemotingSetSecureConnectionServerCallbacksMSFT(m_instance.Get(), m_systemId, &serverCallbacks));
                }

                XrRemotingListenInfoMSFT listenInfo{static_cast<XrStructureType>(XR_TYPE_REMOTING_LISTEN_INFO_MSFT)};
                listenInfo.listenInterface = m_options.host.empty() ? "0.0.0.0" : m_options.host.c_str();
                listenInfo.handshakeListenPort = m_options.port != 0 ? m_options.port : 8265;
                listenInfo.transportListenPort = m_options.transportPort != 0 ? m_options.transportPort : 8266;
                listenInfo.secureConnection = m_options.secureConnection;
                CHECK_XRCMD(m_extensions.xrRemotingListenMSFT(m_instance.Get(), m_systemId, &listenInfo));
            } else {
                if (m_options.secureConnection) {
                    XrRemotingSecureConnectionClientCallbacksMSFT clientCallbacks;
                    clientCallbacks.context = this;
                    clientCallbacks.requestAuthenticationTokenCallback = AuthenticationRequestCallbackStatic;
                    clientCallbacks.validateServerCertificateCallback = CertificateValidationCallbackStatic;
                    clientCallbacks.performSystemValidation = true;

                    CHECK_XRCMD(
                        m_extensions.xrRemotingSetSecureConnectionClientCallbacksMSFT(m_instance.Get(), m_systemId, &clientCallbacks));
                }

                XrRemotingConnectInfoMSFT connectInfo{static_cast<XrStructureType>(XR_TYPE_REMOTING_CONNECT_INFO_MSFT)};
                connectInfo.remoteHostName = m_options.host.empty() ? "127.0.0.1" : m_options.host.c_str();
                connectInfo.remotePort = m_options.port != 0 ? m_options.port : 8265;
                connectInfo.secureConnection = m_options.secureConnection;
                CHECK_XRCMD(m_extensions.xrRemotingConnectMSFT(m_instance.Get(), m_systemId, &connectInfo));
            }
        }

        void InitializeSystem() {
            CHECK(m_instance.Get() != XR_NULL_HANDLE);
            CHECK(m_systemId == XR_NULL_SYSTEM_ID);

            XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
            systemInfo.formFactor = m_formFactor;
            while (true) {
                XrResult result = xrGetSystem(m_instance.Get(), &systemInfo, &m_systemId);
                if (SUCCEEDED(result)) {
                    break;
                } else if (result == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
                    DEBUG_PRINT("No headset detected.  Trying again in one second...");
                    using namespace std::chrono_literals;
                    std::this_thread::sleep_for(1s);
                } else {
                    CHECK_XRRESULT(result, "xrGetSystem");
                }
            };

            // Choosing a reasonable depth range can help improve hologram visual quality.
            // Use reversed-Z (near > far) for more uniform Z resolution.
            m_nearFar = {20.f, 0.1f};
        }

        void InitializeDevice() {
            CHECK(m_instance.Get() != XR_NULL_HANDLE);
            CHECK(m_systemId != XR_NULL_SYSTEM_ID);

            // Create the D3D11 device for the adapter associated with the system.
            XrGraphicsRequirementsD3D11KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
            CHECK_XRCMD(m_extensions.xrGetD3D11GraphicsRequirementsKHR(m_instance.Get(), m_systemId, &graphicsRequirements));

            // Create a list of feature levels which are both supported by the OpenXR runtime and this application.
            std::vector<D3D_FEATURE_LEVEL> featureLevels = {D3D_FEATURE_LEVEL_12_1,
                                                            D3D_FEATURE_LEVEL_12_0,
                                                            D3D_FEATURE_LEVEL_11_1,
                                                            D3D_FEATURE_LEVEL_11_0,
                                                            D3D_FEATURE_LEVEL_10_1,
                                                            D3D_FEATURE_LEVEL_10_0};
            featureLevels.erase(std::remove_if(featureLevels.begin(),
                                               featureLevels.end(),
                                               [&](D3D_FEATURE_LEVEL fl) { return fl < graphicsRequirements.minFeatureLevel; }),
                                featureLevels.end());
            CHECK_MSG(featureLevels.size() != 0, "Unsupported minimum feature level!");

            m_device = m_graphicsPlugin->InitializeDevice(graphicsRequirements.adapterLuid, featureLevels);
        }

        void InitializeSession() {
            CHECK(m_instance.Get() != XR_NULL_HANDLE);
            CHECK(m_systemId != XR_NULL_SYSTEM_ID);
            CHECK(m_session.Get() == XR_NULL_HANDLE);

            XrGraphicsBindingD3D11KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
            graphicsBinding.device = m_device;

            XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
            createInfo.next = &graphicsBinding;
            createInfo.systemId = m_systemId;

            CHECK_XRCMD(xrCreateSession(m_instance.Get(), &createInfo, m_session.Put()));

            // If remoting speech extension is enabled
            if (m_usingRemotingRuntime) {
                XrRemotingSpeechInitInfoMSFT speechInitInfo{static_cast<XrStructureType>(XR_TYPE_REMOTING_SPEECH_INIT_INFO_MSFT)};
                InitializeSpeechRecognition(speechInitInfo);

                CHECK_XRCMD(m_extensions.xrInitializeRemotingSpeechMSFT(m_session.Get(), &speechInitInfo));
            }

            XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            std::vector<XrActionSet> actionSets = {m_actionSet.Get()};
            attachInfo.countActionSets = (uint32_t)actionSets.size();
            attachInfo.actionSets = actionSets.data();
            CHECK_XRCMD(xrAttachSessionActionSets(m_session.Get(), &attachInfo));

            // Get the xrEnumerateViewConfig
            {
                uint32_t viewConfigTypeCount;
                CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance.Get(), m_systemId, 0, &viewConfigTypeCount, nullptr));
                std::vector<XrViewConfigurationType> viewConfigTypes(viewConfigTypeCount);

                CHECK_XRCMD(xrEnumerateViewConfigurations(
                    m_instance.Get(), m_systemId, viewConfigTypeCount, &viewConfigTypeCount, viewConfigTypes.data()));
                CHECK((uint32_t)viewConfigTypes.size() == viewConfigTypeCount);
                CHECK((uint32_t)viewConfigTypes.size() > 0);

                m_primaryViewConfigType = viewConfigTypes[0];
            }

            // Choose an environment blend mode.
            {
                // Query the list of supported environment blend modes for the current system.
                uint32_t count;
                CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance.Get(), m_systemId, m_primaryViewConfigType, 0, &count, nullptr));
                CHECK(count > 0); // A system must support at least one environment blend mode.

                std::vector<XrEnvironmentBlendMode> environmentBlendModes(count);
                CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(
                    m_instance.Get(), m_systemId, m_primaryViewConfigType, count, &count, environmentBlendModes.data()));

                // This sample supports all modes, pick the system's preferred one.
                m_environmentBlendMode = environmentBlendModes[0];
            }

            CreateSpaces();
            CreateSwapchains();
        }

        void CreateSpaces() {
            CHECK(m_session.Get() != XR_NULL_HANDLE);

            // Create a app space to bridge interactions and all holograms.
            {
                if (m_optionalExtensions.UnboundedRefSpaceSupported) {
                    // Unbounded reference space provides the best app space for world-scale experiences.
                    m_appSpaceType = XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT;
                } else {
                    // If running on a platform that does not support world-scale experiences, fall back to local space.
                    m_appSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
                }

                XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                spaceCreateInfo.referenceSpaceType = m_appSpaceType;
                spaceCreateInfo.poseInReferenceSpace = xr::math::Pose::Identity();
                CHECK_XRCMD(xrCreateReferenceSpace(m_session.Get(), &spaceCreateInfo, m_appSpace.Put()));
            }

            // Create a space for each hand pointer pose.
            for (uint32_t side : {LeftSide, RightSide}) {
                XrActionSpaceCreateInfo createInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
                createInfo.action = m_poseAction.Get();
                createInfo.poseInActionSpace = xr::math::Pose::Identity();
                createInfo.subactionPath = m_subactionPaths[side];
                CHECK_XRCMD(xrCreateActionSpace(m_session.Get(), &createInfo, m_cubesInHand[side].Space.Put()));
            }
        }

        std::tuple<DXGI_FORMAT, DXGI_FORMAT> SelectSwapchainPixelFormats() {
            CHECK(m_session.Get() != XR_NULL_HANDLE);

            // Query the runtime's preferred swapchain formats.
            uint32_t swapchainFormatCount;
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session.Get(), 0, &swapchainFormatCount, nullptr));

            std::vector<int64_t> swapchainFormats(swapchainFormatCount);
            CHECK_XRCMD(xrEnumerateSwapchainFormats(
                m_session.Get(), (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));

            // Choose the first runtime-preferred format that this app supports.
            auto SelectPixelFormat = [](const std::vector<int64_t>& runtimePreferredFormats,
                                        const std::vector<DXGI_FORMAT>& applicationSupportedFormats) {
                auto found = std::find_first_of(std::begin(runtimePreferredFormats),
                                                std::end(runtimePreferredFormats),
                                                std::begin(applicationSupportedFormats),
                                                std::end(applicationSupportedFormats));
                if (found == std::end(runtimePreferredFormats)) {
                    THROW("No runtime swapchain format is supported.");
                }
                return (DXGI_FORMAT)*found;
            };

            DXGI_FORMAT colorSwapchainFormat = SelectPixelFormat(swapchainFormats, m_graphicsPlugin->SupportedColorFormats());
            DXGI_FORMAT depthSwapchainFormat = SelectPixelFormat(swapchainFormats, m_graphicsPlugin->SupportedDepthFormats());

            return {colorSwapchainFormat, depthSwapchainFormat};
        }

        void CreateSwapchains() {
            CHECK(m_session.Get() != XR_NULL_HANDLE);
            CHECK(m_renderResources == nullptr);

            m_renderResources = std::make_unique<RenderResources>();

            // Read graphics properties for preferred swapchain length and logging.
            XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
            CHECK_XRCMD(xrGetSystemProperties(m_instance.Get(), m_systemId, &systemProperties));

            // Select color and depth swapchain pixel formats.
            const auto [colorSwapchainFormat, depthSwapchainFormat] = SelectSwapchainPixelFormats();

            // Query and cache view configuration views.
            uint32_t viewCount;
            CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance.Get(), m_systemId, m_primaryViewConfigType, 0, &viewCount, nullptr));

            m_renderResources->ConfigViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
            CHECK_XRCMD(xrEnumerateViewConfigurationViews(
                m_instance.Get(), m_systemId, m_primaryViewConfigType, viewCount, &viewCount, m_renderResources->ConfigViews.data()));

            const XrViewConfigurationView& view = m_renderResources->ConfigViews[0];

            if (m_primaryViewConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
                // Using texture array for better performance, so requiring left/right views have identical sizes.
                CHECK(m_renderResources->ConfigViews[0].recommendedImageRectWidth ==
                      m_renderResources->ConfigViews[1].recommendedImageRectWidth);
                CHECK(m_renderResources->ConfigViews[0].recommendedImageRectHeight ==
                      m_renderResources->ConfigViews[1].recommendedImageRectHeight);
                CHECK(m_renderResources->ConfigViews[0].recommendedSwapchainSampleCount ==
                      m_renderResources->ConfigViews[1].recommendedSwapchainSampleCount);
            }

            // Use the system's recommended rendering parameters.
            const uint32_t imageRectWidth = view.recommendedImageRectWidth;
            const uint32_t imageRectHeight = view.recommendedImageRectHeight;
            const uint32_t swapchainSampleCount = view.recommendedSwapchainSampleCount;

            // Create swapchains with texture array for color and depth images.
            // The texture array has the size of viewCount, and they are rendered in a single pass using VPRT.
            const uint32_t textureArraySize = viewCount;
            m_renderResources->ColorSwapchain =
                CreateSwapchainD3D11(m_session.Get(),
                                     colorSwapchainFormat,
                                     imageRectWidth,
                                     imageRectHeight,
                                     textureArraySize,
                                     swapchainSampleCount,
                                     0 /*createFlags*/,
                                     XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT);

            m_renderResources->DepthSwapchain =
                CreateSwapchainD3D11(m_session.Get(),
                                     depthSwapchainFormat,
                                     imageRectWidth,
                                     imageRectHeight,
                                     textureArraySize,
                                     swapchainSampleCount,
                                     0 /*createFlags*/,
                                     XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

            // Preallocate view buffers for xrLocateViews later inside frame loop.
            m_renderResources->Views.resize(viewCount, {XR_TYPE_VIEW});
        }

        struct SwapchainD3D11;
        SwapchainD3D11 CreateSwapchainD3D11(XrSession session,
                                            DXGI_FORMAT format,
                                            uint32_t width,
                                            uint32_t height,
                                            uint32_t arraySize,
                                            uint32_t sampleCount,
                                            XrSwapchainCreateFlags createFlags,
                                            XrSwapchainUsageFlags usageFlags) {
            SwapchainD3D11 swapchain;
            swapchain.Format = format;
            swapchain.Width = width;
            swapchain.Height = height;
            swapchain.ArraySize = arraySize;

            XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            swapchainCreateInfo.arraySize = arraySize;
            swapchainCreateInfo.format = format;
            swapchainCreateInfo.width = width;
            swapchainCreateInfo.height = height;
            swapchainCreateInfo.mipCount = 1;
            swapchainCreateInfo.faceCount = 1;
            swapchainCreateInfo.sampleCount = sampleCount;
            swapchainCreateInfo.createFlags = createFlags;
            swapchainCreateInfo.usageFlags = usageFlags;

            CHECK_XRCMD(xrCreateSwapchain(session, &swapchainCreateInfo, swapchain.Handle.Put()));

            uint32_t chainLength;
            CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.Handle.Get(), 0, &chainLength, nullptr));

            swapchain.Images.resize(chainLength, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
            CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.Handle.Get(),
                                                   (uint32_t)swapchain.Images.size(),
                                                   &chainLength,
                                                   reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchain.Images.data())));

            return swapchain;
        }

        void HandleRecognizedSpeechText(const std::string& text) {
            if (text == "Red") {
                m_cubeColorFilter = {1.0f, 0.0f, 0.0f};
            } else if (text == "Green") {
                m_cubeColorFilter = {0.0f, 1.0f, 0.0f};
            } else if (text == "Blue") {
                m_cubeColorFilter = {0.0f, 0.0f, 1.0f};
            } else if (text == "Aquamarine") {
                m_cubeColorFilter = {0.0f, 1.0f, 1.0f};
            } else if (text == "Default") {
                m_cubeColorFilter = {1.0f, 1.0f, 1.0f};
            } else if (text == "Exit Program") {
                CHECK_XRCMD(xrRequestExitSession(m_session.Get()));
            } else if (text == "Reverse Direction") {
                // Reverse the rotation direction of the spinning cube
                // from anticlockwise to clockwise or vice versa.
                m_rotationDirection *= -1;
            }
        }

        void ProcessEvents(bool* exitRenderLoop, bool* requestRestart) {
            *exitRenderLoop = *requestRestart = false;

            auto pollEvent = [&](XrEventDataBuffer& eventData) -> bool {
                eventData.type = XR_TYPE_EVENT_DATA_BUFFER;
                eventData.next = nullptr;
                return CHECK_XRCMD(xrPollEvent(m_instance.Get(), &eventData)) == XR_SUCCESS;
            };

            XrEventDataBuffer eventData{};
            while (pollEvent(eventData)) {
                switch (eventData.type) {
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                    *exitRenderLoop = true;
                    *requestRestart = false;
                    return;
                }
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    const auto stateEvent = *reinterpret_cast<const XrEventDataSessionStateChanged*>(&eventData);
                    CHECK(m_session.Get() != XR_NULL_HANDLE && m_session.Get() == stateEvent.session);
                    m_sessionState = stateEvent.state;
                    switch (m_sessionState) {
                    case XR_SESSION_STATE_READY: {
                        CHECK(m_session.Get() != XR_NULL_HANDLE);
                        XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                        sessionBeginInfo.primaryViewConfigurationType = m_primaryViewConfigType;
                        CHECK_XRCMD(xrBeginSession(m_session.Get(), &sessionBeginInfo));
                        m_sessionRunning = true;
                        UpdateWindowTitleWin32();
                        break;
                    }
                    case XR_SESSION_STATE_STOPPING: {
                        m_sessionRunning = false;
                        CHECK_XRCMD(xrEndSession(m_session.Get()));
                        break;
                    }
                    case XR_SESSION_STATE_EXITING: {
                        // Do not attempt to restart, because user closed this session.
                        *exitRenderLoop = true;
                        *requestRestart = false;
                        break;
                    }
                    case XR_SESSION_STATE_LOSS_PENDING: {
                        // Session was lost, so start over and poll for new systemId.
                        *exitRenderLoop = true;
                        *requestRestart = true;
                        break;
                    }
                    }
                    break;
                }
                case XR_TYPE_REMOTING_EVENT_DATA_LISTENING_MSFT: {
                    DEBUG_PRINT("Holographic Remoting: Listening on port %d",
                                reinterpret_cast<const XrRemotingEventDataListeningMSFT*>(&eventData)->listeningPort);
                    break;
                }
                case XR_TYPE_REMOTING_EVENT_DATA_CONNECTED_MSFT: {
                    DEBUG_PRINT("Holographic Remoting: Connected.");

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
                    CreateUserDataChannel();
                    m_userDataChannelDestroyed = false;
#endif
                    break;
                }
                case XR_TYPE_REMOTING_EVENT_DATA_DISCONNECTED_MSFT: {
                    DEBUG_PRINT("Holographic Remoting: Disconnected - Reason: %d",
                                reinterpret_cast<const XrRemotingEventDataDisconnectedMSFT*>(&eventData)->disconnectReason);
                    break;
                }
#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
                case XR_TYPE_EVENT_DATA_REMOTING_DATA_CHANNEL_CREATED_MSFT: {
                    auto channelCreatedEventData = reinterpret_cast<const XrEventDataRemotingDataChannelCreatedMSFT*>(&eventData);
                    DEBUG_PRINT("Holographic Remoting: Custom data channel created.");
                    m_userDataChannel = channelCreatedEventData->channel;
                    break;
                }
                case XR_TYPE_EVENT_DATA_REMOTING_DATA_CHANNEL_OPENED_MSFT: {
                    DEBUG_PRINT("Holographic Remoting: Custom data channel opened.");
                    break;
                }
                case XR_TYPE_EVENT_DATA_REMOTING_DATA_CHANNEL_CLOSED_MSFT: {
                    auto closedEventData = reinterpret_cast<const XrEventDataRemotingDataChannelClosedMSFT*>(&eventData);
                    DEBUG_PRINT("Holographic Remoting: Custom data channel closed reason: %d", closedEventData->closedReason);
                    break;
                }
                case XR_TYPE_EVENT_DATA_REMOTING_DATA_CHANNEL_DATA_RECEIVED_MSFT: {
                    auto dataReceivedEventData = reinterpret_cast<const XrEventDataRemotingDataChannelDataReceivedMSFT*>(&eventData);
                    std::vector<uint8_t> packet(dataReceivedEventData->size);
                    uint32_t dataBytesCount;
                    CHECK_XRCMD(m_extensions.xrRetrieveRemotingDataMSFT(dataReceivedEventData->channel,
                                                                        dataReceivedEventData->packetId,
                                                                        static_cast<uint32_t>(packet.size()),
                                                                        &dataBytesCount,
                                                                        packet.data()));
                    DEBUG_PRINT("Holographic Remoting: Custom data channel data received: %d", static_cast<uint32_t>(packet[0]));
                    break;
                }

#endif
                case XR_TYPE_EVENT_DATA_REMOTING_SPEECH_RECOGNIZED_MSFT: {
                    auto speechEventData = reinterpret_cast<const XrEventDataRemotingSpeechRecognizedMSFT*>(&eventData);
                    std::string text;
                    uint32_t dataBytesCount = 0;
                    CHECK_XRCMD(m_extensions.xrRetrieveRemotingSpeechRecognizedTextMSFT(
                        m_session.Get(), speechEventData->packetId, 0, &dataBytesCount, nullptr));
                    text.resize(dataBytesCount);
                    CHECK_XRCMD(m_extensions.xrRetrieveRemotingSpeechRecognizedTextMSFT(
                        m_session.Get(), speechEventData->packetId, static_cast<uint32_t>(text.size()), &dataBytesCount, text.data()));
                    HandleRecognizedSpeechText(text);
                    break;
                }

                case XR_TYPE_EVENT_DATA_REMOTING_SPEECH_RECOGNIZER_STATE_CHANGED_MSFT: {
                    auto recognizerStateEventData =
                        reinterpret_cast<const XrEventDataRemotingSpeechRecognizerStateChangedMSFT*>(&eventData);
                    auto state = recognizerStateEventData->speechRecognizerState;
                    if (strlen(recognizerStateEventData->stateMessage) > 0) {
                        DEBUG_PRINT("Speech recognizer initialization error: %s.", recognizerStateEventData->stateMessage);
                    }

                    if (state == XR_REMOTING_SPEECH_RECOGNIZER_STATE_INITIALIZATION_FAILED_MSFT) {
                        DEBUG_PRINT("Remoting speech recognizer initialization failed.");
                    }

                    break;
                }

                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                default: {
                    DEBUG_PRINT("Ignoring event type %d", eventData.type);
                    break;
                }
                }
            }
        }

        struct Hologram;
        Hologram CreateHologram(const XrPosef& poseInAppSpace, XrTime placementTime) const {
            Hologram hologram{};
            if (m_optionalExtensions.SpatialAnchorSupported) {
                // Anchors provide the best stability when moving beyond 5 meters, so if the extension is enabled,
                // create an anchor at given location and place the hologram at the resulting anchor space.
                XrSpatialAnchorCreateInfoMSFT createInfo{XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_MSFT};
                createInfo.space = m_appSpace.Get();
                createInfo.pose = poseInAppSpace;
                createInfo.time = placementTime;

                XrResult result = m_extensions.xrCreateSpatialAnchorMSFT(
                    m_session.Get(), &createInfo, hologram.Anchor.Put(m_extensions.xrDestroySpatialAnchorMSFT));
                if (XR_SUCCEEDED(result)) {
                    XrSpatialAnchorSpaceCreateInfoMSFT createSpaceInfo{XR_TYPE_SPATIAL_ANCHOR_SPACE_CREATE_INFO_MSFT};
                    createSpaceInfo.anchor = hologram.Anchor.Get();
                    createSpaceInfo.poseInAnchorSpace = xr::math::Pose::Identity();
                    CHECK_XRCMD(m_extensions.xrCreateSpatialAnchorSpaceMSFT(m_session.Get(), &createSpaceInfo, hologram.Cube.Space.Put()));
                } else if (result == XR_ERROR_CREATE_SPATIAL_ANCHOR_FAILED_MSFT) {
                    DEBUG_PRINT("Anchor cannot be created, likely due to lost positional tracking.");
                } else {
                    CHECK_XRRESULT(result, "xrCreateSpatialAnchorMSFT");
                }
            } else {
                // If the anchor extension is not available, place hologram in the app space.
                // This works fine as long as user doesn't move far away from app space origin.
                XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                createInfo.referenceSpaceType = m_appSpaceType;
                createInfo.poseInReferenceSpace = poseInAppSpace;
                CHECK_XRCMD(xrCreateReferenceSpace(m_session.Get(), &createInfo, hologram.Cube.Space.Put()));
            }
            return hologram;
        }

        void PollActions() {
            // Get updated action states.
            std::vector<XrActiveActionSet> activeActionSets = {{m_actionSet.Get(), XR_NULL_PATH}};
            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            syncInfo.countActiveActionSets = (uint32_t)activeActionSets.size();
            syncInfo.activeActionSets = activeActionSets.data();
            CHECK_XRCMD(xrSyncActions(m_session.Get(), &syncInfo));

            // Check the state of the actions for left and right hands separately.
            for (uint32_t side : {LeftSide, RightSide}) {
                const XrPath subactionPath = m_subactionPaths[side];

                // Apply a tiny vibration to the corresponding hand to indicate that action is detected.
                auto ApplyVibration = [this, subactionPath] {
                    XrHapticActionInfo actionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
                    actionInfo.action = m_vibrateAction.Get();
                    actionInfo.subactionPath = subactionPath;

                    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
                    vibration.amplitude = 0.5f;
                    vibration.duration = XR_MIN_HAPTIC_DURATION;
                    vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
                    CHECK_XRCMD(xrApplyHapticFeedback(m_session.Get(), &actionInfo, (XrHapticBaseHeader*)&vibration));
                };

                XrActionStateBoolean placeActionValue{XR_TYPE_ACTION_STATE_BOOLEAN};
                {
                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = m_placeAction.Get();
                    getInfo.subactionPath = subactionPath;
                    CHECK_XRCMD(xrGetActionStateBoolean(m_session.Get(), &getInfo, &placeActionValue));
                }

                // When select button is pressed, place the cube at the location of the corresponding hand.
                if (placeActionValue.isActive && placeActionValue.changedSinceLastSync && placeActionValue.currentState) {
                    // Use the pose at the historical time when the action happened to do the placement.
                    const XrTime placementTime = placeActionValue.lastChangeTime;

                    // Locate the hand in the scene.
                    XrSpaceLocation handLocation{XR_TYPE_SPACE_LOCATION};
                    CHECK_XRCMD(xrLocateSpace(m_cubesInHand[side].Space.Get(), m_appSpace.Get(), placementTime, &handLocation));

                    // Ensure we have tracking before placing a cube in the scene, so that it stays reliably at a physical location.
                    if (!xr::math::Pose::IsPoseValid(handLocation)) {
                        DEBUG_PRINT("Cube cannot be placed when positional tracking is lost.");
                    } else {
                        // Place a new cube at the given location and time, and remember output placement space and anchor.
                        m_holograms.push_back(CreateHologram(handLocation.pose, placementTime));
                    }

                    ApplyVibration();
                }

                // This sample, when menu button is released, requests to quit the session, and therefore quit the application.
                {
                    XrActionStateBoolean exitActionValue{XR_TYPE_ACTION_STATE_BOOLEAN};
                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = m_exitAction.Get();
                    getInfo.subactionPath = subactionPath;
                    CHECK_XRCMD(xrGetActionStateBoolean(m_session.Get(), &getInfo, &exitActionValue));

                    if (exitActionValue.isActive && exitActionValue.changedSinceLastSync && !exitActionValue.currentState) {
                        CHECK_XRCMD(xrRequestExitSession(m_session.Get()));
                        ApplyVibration();
                    }
                }
            }
        }

        void RenderFrame() {
            CHECK(m_session.Get() != XR_NULL_HANDLE);

            XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
            XrFrameState frameState{XR_TYPE_FRAME_STATE};
            CHECK_XRCMD(xrWaitFrame(m_session.Get(), &frameWaitInfo, &frameState));

            XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
            CHECK_XRCMD(xrBeginFrame(m_session.Get(), &frameBeginInfo));

            // xrEndFrame can submit multiple layers. This sample submits one.
            std::vector<XrCompositionLayerBaseHeader*> layers;

            // The projection layer consists of projection layer views.
            XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};

            // Inform the runtime that the app's submitted alpha channel has valid data for use during composition.
            // The primary display on HoloLens has an additive environment blend mode. It will ignore the alpha channel.
            // However, mixed reality capture uses the alpha channel if this bit is set to blend content with the environment.
            layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

            // Only render when session is visible, otherwise submit zero layers.
            if (frameState.shouldRender) {
                // First update the viewState and views using latest predicted display time.
                {
                    XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
                    viewLocateInfo.viewConfigurationType = m_primaryViewConfigType;
                    viewLocateInfo.displayTime = frameState.predictedDisplayTime;
                    viewLocateInfo.space = m_appSpace.Get();

                    // The output view count of xrLocateViews is always same as xrEnumerateViewConfigurationViews.
                    // Therefore, Views can be preallocated and avoid two call idiom here.
                    uint32_t viewCapacityInput = (uint32_t)m_renderResources->Views.size();
                    uint32_t viewCountOutput;
                    CHECK_XRCMD(xrLocateViews(m_session.Get(),
                                              &viewLocateInfo,
                                              &m_renderResources->ViewState,
                                              viewCapacityInput,
                                              &viewCountOutput,
                                              m_renderResources->Views.data()));

                    CHECK(viewCountOutput == viewCapacityInput);
                    CHECK(viewCountOutput == m_renderResources->ConfigViews.size());
                    CHECK(viewCountOutput == m_renderResources->ColorSwapchain.ArraySize);
                    CHECK(viewCountOutput == m_renderResources->DepthSwapchain.ArraySize);
                }

                // Then, render projection layer into each view.
                if (RenderLayer(frameState.predictedDisplayTime, layer)) {
                    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
                }
            }

            // Submit the composition layers for the predicted display time.
            XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
            frameEndInfo.displayTime = frameState.predictedDisplayTime;
            frameEndInfo.environmentBlendMode = m_environmentBlendMode;
            frameEndInfo.layerCount = (uint32_t)layers.size();
            frameEndInfo.layers = layers.data();

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
            XrRemotingFrameMirrorImageD3D11MSFT mirrorImageD3D11{
                static_cast<XrStructureType>(XR_TYPE_REMOTING_FRAME_MIRROR_IMAGE_D3D11_MSFT)};
            mirrorImageD3D11.texture = m_window->GetNextSwapchainTexture();

            XrRemotingFrameMirrorImageInfoMSFT mirrorImageEndInfo{
                static_cast<XrStructureType>(XR_TYPE_REMOTING_FRAME_MIRROR_IMAGE_INFO_MSFT)};
            mirrorImageEndInfo.image = reinterpret_cast<const XrRemotingFrameMirrorImageBaseHeaderMSFT*>(&mirrorImageD3D11);

            frameEndInfo.next = &mirrorImageEndInfo;
#endif

            CHECK_XRCMD(xrEndFrame(m_session.Get(), &frameEndInfo));

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
            m_window->PresentSwapchain();
#endif
        }

        uint32_t AcquireAndWaitForSwapchainImage(XrSwapchain handle) {
            uint32_t swapchainImageIndex;
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            CHECK_XRCMD(xrAcquireSwapchainImage(handle, &acquireInfo, &swapchainImageIndex));

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            CHECK_XRCMD(xrWaitSwapchainImage(handle, &waitInfo));

            return swapchainImageIndex;
        }

        void InitializeSpinningCube(XrTime predictedDisplayTime) {
            auto createReferenceSpace = [session = m_session.Get()](XrReferenceSpaceType referenceSpaceType, XrPosef poseInReferenceSpace) {
                xr::SpaceHandle space;
                XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                createInfo.referenceSpaceType = referenceSpaceType;
                createInfo.poseInReferenceSpace = poseInReferenceSpace;
                CHECK_XRCMD(xrCreateReferenceSpace(session, &createInfo, space.Put()));
                return space;
            };

            m_cubeColorFilter = {1, 1, 1};
            m_rotationDirection = 1.0f;

            {
                // Initialize a big cube 1 meter in front of user.
                Hologram hologram{};
                hologram.Cube.Scale = {0.25f, 0.25f, 0.25f};
                hologram.Cube.Space = createReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, xr::math::Pose::Translation({0, 0, -1}));
                hologram.Cube.colorFilter = m_cubeColorFilter;
                m_holograms.push_back(std::move(hologram));
                m_mainCubeIndex = (uint32_t)m_holograms.size() - 1;
            }

            {
                // Initialize a small cube and remember the time when animation is started.
                Hologram hologram{};
                hologram.Cube.Scale = {0.1f, 0.1f, 0.1f};
                hologram.Cube.Space = createReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, xr::math::Pose::Translation({0, 0, -1}));
                hologram.Cube.colorFilter = m_cubeColorFilter;
                m_holograms.push_back(std::move(hologram));
                m_spinningCubeIndex = (uint32_t)m_holograms.size() - 1;

                m_spinningCubeStartTime = predictedDisplayTime;
            }
        }

        void UpdateSpinningCube(XrTime predictedDisplayTime) {
            if (!m_mainCubeIndex || !m_spinningCubeIndex) {
                // Deferred initialization of spinning cubes so they appear at right place for the first frame.
                InitializeSpinningCube(predictedDisplayTime);
            }

            // Pause spinning cube animation when app loses 3D focus
            if (IsSessionFocused()) {
                auto convertToSeconds = [](XrDuration nanoSeconds) {
                    using namespace std::chrono;
                    return duration_cast<duration<float>>(duration<XrDuration, std::nano>(nanoSeconds)).count();
                };

                const XrDuration duration = predictedDisplayTime - m_spinningCubeStartTime;
                const float seconds = convertToSeconds(duration);
                const float angle = m_rotationDirection * DirectX::XM_PIDIV2 * seconds; // Rotate 90 degrees per second
                const float radius = 0.5f;                                              // Rotation radius in meters

                // Let spinning cube rotate around the main cube's y axis.
                XrPosef pose;
                pose.position = {radius * std::sin(angle), 0, radius * std::cos(angle)};
                pose.orientation = xr::math::Quaternion::RotationAxisAngle({0, 1, 0}, angle);
                m_holograms[m_spinningCubeIndex.value()].Cube.PoseInSpace = pose;
            }
        }

        bool RenderLayer(XrTime predictedDisplayTime, XrCompositionLayerProjection& layer) {
            const uint32_t viewCount = (uint32_t)m_renderResources->ConfigViews.size();

            if (!xr::math::Pose::IsPoseValid(m_renderResources->ViewState)) {
                DEBUG_PRINT("xrLocateViews returned an invalid pose.");
                return false; // Skip rendering layers if view location is invalid
            }

            std::vector<const sample::Cube*> visibleCubes;

            auto UpdateVisibleCube = [&](sample::Cube& cube) {
                if (cube.Space.Get() != XR_NULL_HANDLE) {
                    XrSpaceLocation cubeSpaceInAppSpace{XR_TYPE_SPACE_LOCATION};
                    CHECK_XRCMD(xrLocateSpace(cube.Space.Get(), m_appSpace.Get(), predictedDisplayTime, &cubeSpaceInAppSpace));

                    // Update cube's location with latest space location
                    if (xr::math::Pose::IsPoseValid(cubeSpaceInAppSpace)) {
                        if (cube.PoseInSpace.has_value()) {
                            cube.PoseInAppSpace = xr::math::Pose::Multiply(cube.PoseInSpace.value(), cubeSpaceInAppSpace.pose);
                        } else {
                            cube.PoseInAppSpace = cubeSpaceInAppSpace.pose;
                        }
                        visibleCubes.push_back(&cube);
                    }

                    // Update cube color
                    cube.colorFilter = m_cubeColorFilter;
                }
            };

            UpdateSpinningCube(predictedDisplayTime);

            UpdateVisibleCube(m_cubesInHand[LeftSide]);
            UpdateVisibleCube(m_cubesInHand[RightSide]);

            for (auto& hologram : m_holograms) {
                UpdateVisibleCube(hologram.Cube);
            }

            m_renderResources->ProjectionLayerViews.resize(viewCount);
            if (m_optionalExtensions.DepthExtensionSupported) {
                m_renderResources->DepthInfoViews.resize(viewCount);
            }

            // Swapchain is acquired, rendered to, and released together for all views as texture array
            const SwapchainD3D11& colorSwapchain = m_renderResources->ColorSwapchain;
            const SwapchainD3D11& depthSwapchain = m_renderResources->DepthSwapchain;

            // Use the full size of the allocated swapchain image (could render smaller some frames to hit framerate)
            const XrRect2Di imageRect = {{0, 0}, {(int32_t)colorSwapchain.Width, (int32_t)colorSwapchain.Height}};
            CHECK(colorSwapchain.Width == depthSwapchain.Width);
            CHECK(colorSwapchain.Height == depthSwapchain.Height);

            const uint32_t colorSwapchainImageIndex = AcquireAndWaitForSwapchainImage(colorSwapchain.Handle.Get());
            const uint32_t depthSwapchainImageIndex = AcquireAndWaitForSwapchainImage(depthSwapchain.Handle.Get());

            // Prepare rendering parameters of each view for swapchain texture arrays
            std::vector<xr::math::ViewProjection> viewProjections(viewCount);
            for (uint32_t i = 0; i < viewCount; i++) {
                viewProjections[i] = {m_renderResources->Views[i].pose, m_renderResources->Views[i].fov, m_nearFar};

                m_renderResources->ProjectionLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
                m_renderResources->ProjectionLayerViews[i].pose = m_renderResources->Views[i].pose;
                m_renderResources->ProjectionLayerViews[i].fov = m_renderResources->Views[i].fov;
                m_renderResources->ProjectionLayerViews[i].subImage.swapchain = colorSwapchain.Handle.Get();
                m_renderResources->ProjectionLayerViews[i].subImage.imageRect = imageRect;
                m_renderResources->ProjectionLayerViews[i].subImage.imageArrayIndex = i;

                if (m_optionalExtensions.DepthExtensionSupported) {
                    m_renderResources->DepthInfoViews[i] = {XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR};
                    m_renderResources->DepthInfoViews[i].minDepth = 0;
                    m_renderResources->DepthInfoViews[i].maxDepth = 1;
                    m_renderResources->DepthInfoViews[i].nearZ = m_nearFar.Near;
                    m_renderResources->DepthInfoViews[i].farZ = m_nearFar.Far;
                    m_renderResources->DepthInfoViews[i].subImage.swapchain = depthSwapchain.Handle.Get();
                    m_renderResources->DepthInfoViews[i].subImage.imageRect = imageRect;
                    m_renderResources->DepthInfoViews[i].subImage.imageArrayIndex = i;

                    // Chain depth info struct to the corresponding projection layer view's next pointer
                    m_renderResources->ProjectionLayerViews[i].next = &m_renderResources->DepthInfoViews[i];
                }
            }

            // For HoloLens additive display, best to clear render target with transparent black color (0,0,0,0)
            constexpr DirectX::XMVECTORF32 opaqueColor = {0.184313729f, 0.309803933f, 0.309803933f, 1.000000000f};
            constexpr DirectX::XMVECTORF32 transparent = {0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f};
            const DirectX::XMVECTORF32 renderTargetClearColor =
                (m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE) ? opaqueColor : transparent;

            m_graphicsPlugin->RenderView(imageRect,
                                         renderTargetClearColor,
                                         viewProjections,
                                         colorSwapchain.Format,
                                         colorSwapchain.Images[colorSwapchainImageIndex].texture,
                                         depthSwapchain.Format,
                                         depthSwapchain.Images[depthSwapchainImageIndex].texture,
                                         visibleCubes);

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            CHECK_XRCMD(xrReleaseSwapchainImage(colorSwapchain.Handle.Get(), &releaseInfo));
            CHECK_XRCMD(xrReleaseSwapchainImage(depthSwapchain.Handle.Get(), &releaseInfo));

            layer.space = m_appSpace.Get();
            layer.viewCount = (uint32_t)m_renderResources->ProjectionLayerViews.size();
            layer.views = m_renderResources->ProjectionLayerViews.data();
            return true;
        }

        void PrepareSessionRestart() {
            m_mainCubeIndex = m_spinningCubeIndex = {};
            m_holograms.clear();
            m_renderResources.reset();
            m_appSpace.Reset();
            m_cubesInHand[LeftSide].Space.Reset();
            m_cubesInHand[RightSide].Space.Reset();
            m_session.Reset();
            m_sessionRunning = false;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
            m_graphicsPlugin->ClearView(m_window->GetNextSwapchainTexture(), clearColor);
            m_window->PresentSwapchain();

            UpdateWindowTitleWin32();
#endif
        }

        constexpr bool IsSessionFocused() const {
            return m_sessionState == XR_SESSION_STATE_FOCUSED;
        }

        XrPath GetXrPath(const char* string) const {
            return xr::StringToPath(m_instance.Get(), string);
        }

        void CreateWindowWin32() {
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
            m_window = std::make_unique<sample::SampleWindowWin32>(xr::utf8_to_wide(m_applicationName), m_device, 768, 512);
            m_window->SetKeyPressedHandler([&](wchar_t key) {
                std::scoped_lock lock{m_keyPressedMutex};
                m_keyPressedQueue.push(towlower(key));
            });

            UpdateWindowTitleWin32();
#endif
        }

        void ProcessWindowEventsWin32(bool* exitRenderLoop, bool* requestRestart) {
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
            if (m_window->IsClosed()) {
                *exitRenderLoop = true;
                *requestRestart = false;
            } else {
                while (true) {
                    wchar_t keyPress;
                    {
                        std::scoped_lock lock{m_keyPressedMutex};

                        if (m_keyPressedQueue.empty()) {
                            break;
                        }

                        keyPress = m_keyPressedQueue.front();
                        m_keyPressedQueue.pop();
                    }

                    switch (keyPress) {
                    case ' ': {
                        if (m_session.Get() == XR_NULL_HANDLE) {
                            ConnectOrListen();
                            InitializeSession();
                        }
                        break;
                    }
#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
                    case 'x': {
                        if (m_sessionRunning && m_usingRemotingRuntime && !m_userDataChannelDestroyed) {
                            DestroyUserDataChannel(m_userDataChannel);
                            m_userDataChannelDestroyed = true;
                        }
                        break;
                    }
#endif
                    case 'd': {
                        if (m_sessionRunning && m_usingRemotingRuntime) {
#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
                            if (!m_userDataChannelDestroyed) {
                                DestroyUserDataChannel(m_userDataChannel);
                                m_userDataChannelDestroyed = true;
                            }
#endif
                            Disconnect();
                        }
                        break;
                    }
                    }
                }
            }
#else
            // Fall back to auto-connect mode.
            if (!m_sessionRunning) {
                ConnectOrListen();
                InitializeSession();
            }
#endif
        }

        void UpdateWindowTitleWin32() {
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
            std::string title = m_sessionRunning ? m_applicationName + " | Press D to Disconnect"
                                                 : m_applicationName + " | " + m_options.host + " | Press Space To Connect";

            m_window->SetWindowTitle(xr::utf8_to_wide(title));
#endif
        }

    private:
        constexpr static XrFormFactor m_formFactor{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};

        const std::string m_applicationName;
        const std::unique_ptr<sample::IGraphicsPluginD3D11> m_graphicsPlugin;
        const sample::AppOptions m_options;

        bool m_usingRemotingRuntime{false};
        std::vector<uint8_t> m_certificateStore;

        xr::InstanceHandle m_instance;
        xr::SessionHandle m_session;
        uint64_t m_systemId{XR_NULL_SYSTEM_ID};
        xr::ExtensionDispatchTable m_extensions;

        struct {
            bool DepthExtensionSupported{false};
            bool UnboundedRefSpaceSupported{false};
            bool SpatialAnchorSupported{false};
        } m_optionalExtensions;

        XrViewConfigurationType m_primaryViewConfigType{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};

        xr::SpaceHandle m_appSpace;
        XrReferenceSpaceType m_appSpaceType{};

        struct Hologram {
            sample::Cube Cube;
            xr::SpatialAnchorHandle Anchor;
        };
        std::vector<Hologram> m_holograms;

        std::optional<uint32_t> m_mainCubeIndex;
        std::optional<uint32_t> m_spinningCubeIndex;
        XrTime m_spinningCubeStartTime;

        constexpr static uint32_t LeftSide = 0;
        constexpr static uint32_t RightSide = 1;
        std::array<XrPath, 2> m_subactionPaths{};
        std::array<sample::Cube, 2> m_cubesInHand{};

        xr::ActionSetHandle m_actionSet;
        xr::ActionHandle m_placeAction;
        xr::ActionHandle m_exitAction;
        xr::ActionHandle m_poseAction;
        xr::ActionHandle m_vibrateAction;

        XrEnvironmentBlendMode m_environmentBlendMode{};
        xr::math::NearFar m_nearFar{};

        struct SwapchainD3D11 {
            xr::SwapchainHandle Handle;
            DXGI_FORMAT Format{DXGI_FORMAT_UNKNOWN};
            uint32_t Width{0};
            uint32_t Height{0};
            uint32_t ArraySize{0};
            std::vector<XrSwapchainImageD3D11KHR> Images;
        };

        struct RenderResources {
            XrViewState ViewState{XR_TYPE_VIEW_STATE};
            std::vector<XrView> Views;
            std::vector<XrViewConfigurationView> ConfigViews;
            SwapchainD3D11 ColorSwapchain;
            SwapchainD3D11 DepthSwapchain;
            std::vector<XrCompositionLayerProjectionView> ProjectionLayerViews;
            std::vector<XrCompositionLayerDepthInfoKHR> DepthInfoViews;
        };

        ID3D11Device* m_device = nullptr;
        std::unique_ptr<RenderResources> m_renderResources{};

        bool m_sessionRunning{false};
        XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        std::unique_ptr<sample::SampleWindowWin32> m_window;

        std::mutex m_keyPressedMutex;
        std::queue<wchar_t> m_keyPressedQueue;
#endif
#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
        std::chrono::high_resolution_clock::time_point m_customDataChannelSendTime = std::chrono::high_resolution_clock::now();
        XrRemotingDataChannelMSFT m_userDataChannel;
        bool m_userDataChannelDestroyed = false;
#endif
        std::vector<uint8_t> m_grammarFileContent;
        std::vector<const char*> m_dictionaryEntries;
        XrVector3f m_cubeColorFilter{1, 1, 1};
        float m_rotationDirection = 1.0f;
    }; // namespace
} // namespace

namespace sample {
    std::unique_ptr<sample::IOpenXrProgram> CreateOpenXrProgram(std::string applicationName,
                                                                std::unique_ptr<sample::IGraphicsPluginD3D11> graphicsPlugin,
                                                                const sample::AppOptions& options) {
        return std::make_unique<ImplementOpenXrProgram>(std::move(applicationName), std::move(graphicsPlugin), options);
    }
} // namespace sample
