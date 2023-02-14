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

#include <pch.h>

#include <FileUtility.h>

#include <Trace.h>
#include <XrUtility/XrString.h>
#include <format>
#include <fstream>

using namespace DirectX;

namespace sample {
    std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& path) {
        bool fileExists = false;
        try {
            std::ifstream file;
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file.open(path, std::ios::binary | std::ios::ate);
            fileExists = true;
            // If tellg fails then it will throw an exception instead of returning -1.
            std::vector<uint8_t> data(static_cast<size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(data.data()), data.size());
            return data;
        } catch (const std::ios::failure&) {
            // The exception only knows that the failbit was set so it doesn't contain anything useful.
            throw std::runtime_error(std::format("Failed to {} file: {}", fileExists ? "read" : "open", path.string()));
        }
    }

    std::filesystem::path GetAppFolder() {
        HMODULE thisModule;
#ifdef HAR_PLATFORM_WINDOWS_UWP
        thisModule = nullptr;
#else
        if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(GetAppFolder), &thisModule)) {
            throw std::runtime_error("Unable to get the module handle.");
        }
#endif

        wchar_t moduleFilename[MAX_PATH];
        ::GetModuleFileName(thisModule, moduleFilename, (DWORD)std::size(moduleFilename));
        std::filesystem::path fullPath(moduleFilename);
        return fullPath.remove_filename();
    }

    std::filesystem::path GetPathInAppFolder(const std::filesystem::path& filename) {
        return GetAppFolder() / filename;
    }

    std::filesystem::path FindFileInAppFolder(const std::filesystem::path& filename,
                                              const std::vector<std::filesystem::path>& searchFolders) {
        auto appFolder = GetAppFolder();
        for (auto folder : searchFolders) {
            auto path = appFolder / folder / filename;
            if (std::filesystem::exists(path)) {
                return path;
            }
        }

        sample::Trace(std::format("File \"{}\" is not found in app folder \"{}\" and search folders{}",
                                  xr::wide_to_utf8(filename.c_str()),
                                  xr::wide_to_utf8(appFolder.c_str()),
                                  [&searchFolders]() -> std::string {
                                      std::string buffer;
                                      for (auto& folder : searchFolders) {
                                          std::format_to(std::back_inserter(buffer), " \"{}\"", xr::wide_to_utf8(folder.c_str()));
                                      }
                                      return buffer;
                                  }())
                          .c_str());

        assert(false && "The file should be embedded in app folder in debug build.");
        return "";
    }
} // namespace sample
