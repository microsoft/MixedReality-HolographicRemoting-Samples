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
#pragma once

#ifdef XR_USE_GRAPHICS_API_D3D11
#define FOR_EACH_D3D11_EXTENSION_FUNCTION(_) _(xrGetD3D11GraphicsRequirementsKHR)
#else
#define FOR_EACH_D3D11_EXTENSION_FUNCTION(_)
#endif

#if XR_MSFT_spatial_anchor
#define FOR_EACH_SPATIAL_ANCHOR_FUNCTION(_) \
    _(xrCreateSpatialAnchorMSFT)            \
    _(xrCreateSpatialAnchorSpaceMSFT)       \
    _(xrDestroySpatialAnchorMSFT)
#else
#define FOR_EACH_SPATIAL_ANCHOR_FUNCTION(_)
#endif

#if XR_MSFT_holographic_remoting
#define FOR_EACH_HAR_EXPERIMENTAL_EXTENSION_FUNCTION(_) \
    _(xrRemotingSetContextPropertiesMSFT)               \
    _(xrRemotingConnectMSFT)                            \
    _(xrRemotingListenMSFT)                             \
    _(xrRemotingDisconnectMSFT)                         \
    _(xrRemotingGetConnectionStateMSFT)                 \
    _(xrRemotingSetSecureConnectionClientCallbacksMSFT) \
    _(xrRemotingSetSecureConnectionServerCallbacksMSFT) \
    _(xrCreateRemotingDataChannelMSFT)                  \
    _(xrDestroyRemotingDataChannelMSFT)                 \
    _(xrGetRemotingDataChannelStateMSFT)                \
    _(xrSendRemotingDataMSFT)                           \
    _(xrRetrieveRemotingDataMSFT)
#else
#define FOR_EACH_HAR_EXPERIMENTAL_EXTENSION_FUNCTION(_)
#endif

#if XR_MSFT_holographic_remoting_speech
#define FOR_EACH_HAR_EXPERIMENTAL_SPEECH_EXTENSION_FUNCTION(_) \
    _(xrInitializeRemotingSpeechMSFT)                          \
    _(xrRetrieveRemotingSpeechRecognizedTextMSFT)
#else
#define FOR_EACH_HAR_EXPERIMENTAL_SPEECH_EXTENSION_FUNCTION(_)
#endif

#define FOR_EACH_SAMPLE_EXTENSION_FUNCTION(_)       \
    FOR_EACH_D3D11_EXTENSION_FUNCTION(_)            \
    FOR_EACH_SPATIAL_ANCHOR_FUNCTION(_)             \
    FOR_EACH_HAR_EXPERIMENTAL_EXTENSION_FUNCTION(_) \
    FOR_EACH_HAR_EXPERIMENTAL_SPEECH_EXTENSION_FUNCTION(_)

#define GET_INSTANCE_PROC_ADDRESS(name) \
    (void)xrGetInstanceProcAddr(instance, #name, reinterpret_cast<PFN_xrVoidFunction*>(const_cast<PFN_##name*>(&name)));
#define DEFINE_PROC_MEMBER(name) const PFN_##name name{nullptr};

namespace xr {
    struct ExtensionDispatchTable {
        FOR_EACH_SAMPLE_EXTENSION_FUNCTION(DEFINE_PROC_MEMBER)

        ExtensionDispatchTable() = default;
        void PopulateDispatchTable(XrInstance instance) {
            FOR_EACH_SAMPLE_EXTENSION_FUNCTION(GET_INSTANCE_PROC_ADDRESS)
        }
    };
} // namespace xr

#undef DEFINE_PROC_MEMBER
#undef GET_INSTANCE_PROC_ADDRESS
#undef FOR_EACH_EXTENSION_FUNCTION
