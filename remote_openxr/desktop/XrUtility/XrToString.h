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

#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>
#include <openxr/openxr_msft_holographic_remoting.h>
#include <openxr/openxr_msft_remoting_speech.h>

#include <string>

#define XR_LIST_ENUM_XrRemotingResult(_)                                  \
    _(XR_ERROR_REMOTING_NOT_DISCONNECTED_MSFT, -1000065000)               \
    _(XR_ERROR_REMOTING_CODEC_NOT_FOUND_MSFT, -1000065001)                \
    _(XR_ERROR_REMOTING_CALLBACK_ERROR_MSFT, -1000065002)                 \
    _(XR_ERROR_REMOTING_DEPTH_BUFFER_STREAM_DISABLED_MSFT, -1000065003)   \
    _(XR_ERROR_REMOTING_DATA_CHANNEL_INVALID_ID_MSFT, -1000065004)        \
    _(XR_ERROR_REMOTING_DATA_CHANNEL_CLOSED_MSFT, -1000065005)            \
    _(XR_ERROR_REMOTING_DATA_CHANNEL_OPEN_PENDING_MSFT, -1000065006)      \
    _(XR_ERROR_REMOTING_DATA_CHANNEL_ID_ALREADY_IN_USE_MSFT, -1000065007) \
    _(XR_ERROR_REMOTING_DATA_CHANNEL_INVALID_DATA_MSFT, -1000065008)      \
    _(XR_ERROR_REMOTING_DATA_CHANNEL_PACKET_EXPIRED_MSFT, -1000065009)    \
    _(XR_ERROR_REMOTING_MAX_ENUM, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrRemotingSpeechResult(_)              \
    _(XR_ERROR_REMOTING_SPEECH_PACKET_EXPIRED, -1000144000) \
    _(XR_ERROR_REMOTING_SPEECH_MAX_ENUM, 0x7FFFFFFF)

// Macro to generate stringify functions for OpenXR enumerations based data provided in openxr_reflection.h
// clang-format off
#define ENUM_CASE_STR(name, val) case name: return #name;

// Returns C string pointing to a string literal. Unknown values are returned as 'Unknown <type>'.
#define MAKE_TO_CSTRING_FUNC(enumType)                      \
    constexpr const char* ToCString(enumType e) noexcept {  \
        switch (e) {                                        \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR)          \
            default: return "Unknown " #enumType;           \
        }                                                   \
    }

// Returns a STL string. Unknown values are stringified as an integer.
#define MAKE_TO_STRING_FUNC(enumType)                  \
    inline std::string ToString(enumType e) {          \
        switch (e) {                                   \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR)     \
            default: return std::to_string(e);         \
        }                                              \
    }

#define MAKE_TO_STRING_FUNCS(enumType) \
    MAKE_TO_CSTRING_FUNC(enumType) \
    MAKE_TO_STRING_FUNC(enumType)
// clang-format on

namespace xr {
    MAKE_TO_STRING_FUNCS(XrReferenceSpaceType);
    MAKE_TO_STRING_FUNCS(XrViewConfigurationType);
    MAKE_TO_STRING_FUNCS(XrEnvironmentBlendMode);
    MAKE_TO_STRING_FUNCS(XrSessionState);
    MAKE_TO_STRING_FUNCS(XrResult);
    MAKE_TO_STRING_FUNCS(XrRemotingResult);
    MAKE_TO_STRING_FUNCS(XrStructureType);
    MAKE_TO_STRING_FUNCS(XrFormFactor);
    MAKE_TO_STRING_FUNCS(XrEyeVisibility);
    MAKE_TO_STRING_FUNCS(XrObjectType);
    MAKE_TO_STRING_FUNCS(XrActionType);
    MAKE_TO_STRING_FUNCS(XrHandEXT);
    MAKE_TO_STRING_FUNCS(XrHandPoseTypeMSFT);
    MAKE_TO_CSTRING_FUNC(XrHandJointEXT);
    MAKE_TO_STRING_FUNCS(XrVisibilityMaskTypeKHR);
} // namespace xr
