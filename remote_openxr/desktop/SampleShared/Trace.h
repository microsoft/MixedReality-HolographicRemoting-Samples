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

#include <chrono>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <processthreadsapi.h>
#include <string_view>
#include <thread>

namespace sample {

    inline void FormatHeader(std::string& buffer) {
        using namespace std::chrono;
        const auto now = system_clock::now();
        const auto posixTime = system_clock::to_time_t(now);
        const auto remainingTime = now - system_clock::from_time_t(posixTime);
        const uint64_t remainingMicroseconds = duration_cast<microseconds>(remainingTime).count();
        const uint32_t threadId = ::GetCurrentThreadId();

        tm localTime;
        ::localtime_s(&localTime, &posixTime);

        std::format_to(std::back_inserter(buffer),
                       "[{:02d}-{:02d}-{:02d}.{:06d}] (t:{:04x}): ",
                       localTime.tm_hour,
                       localTime.tm_min,
                       localTime.tm_sec,
                       remainingMicroseconds,
                       threadId);
    }

    template <typename... Args>
    inline void Trace(std::string_view format_str, const Args&... args) {
        std::string buffer;
        FormatHeader(buffer);
        std::vformat_to(std::back_inserter(buffer), format_str, std::make_format_args(args...));
        buffer.push_back('\n');
        buffer.push_back('\0');
        ::OutputDebugStringA(buffer.data());
    }
} // namespace sample
