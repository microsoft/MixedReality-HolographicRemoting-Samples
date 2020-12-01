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

#include <CommandLineUtility.h>

#include <regex>
#include <shellapi.h>

namespace {
    std::string SplitHostnameAndPortString(const std::string& address, uint16_t& port) {
        static std::basic_regex<char> addressMatcher("(?:(\\[.*\\])|([^:]*))(?:[:](\\d+))?");
        std::match_results<typename std::string::const_iterator> results;
        if (std::regex_match(address, results, addressMatcher)) {
            if (results[3].matched) {
                std::string portStr = results[3].str();
                port = static_cast<uint16_t>(std::strtol(portStr.c_str(), nullptr, 10));
            }

            return (results[1].matched) ? results[1].str() : results[2].str();
        } else {
            return address;
        }
    }
} // namespace

namespace sample {

    void ParseCommandLine(sample::AppOptions& options) {
        int numArgs = __argc;
        char** argList = __argv;

        for (int i = 1; i < numArgs; ++i) {
            if (strlen(argList[i]) == 0) {
                continue;
            }

            std::string arg = argList[i];
            if (arg[0] == '-') {
                std::string param = arg.substr(1);
                std::transform(param.begin(), param.end(), param.begin(), ::tolower);

                if (param == "listen") {
                    options.listen = true;
                    continue;
                }

                if (param == "standalone") {
                    options.isStandalone = true;
                    continue;
                }

                if (param == "nouserwait") {
                    options.noUserWait = true;
                    continue;
                }

                if (param == "ephemeralport") {
                    options.useEphemeralPort = true;
                    continue;
                }

                if (param == "transportport") {
                    if (numArgs > i + 1) {
                        std::string transportPortStr = argList[i + 1];
                        try {
                            options.transportPort = std::stoi(transportPortStr);
                        } catch (const std::invalid_argument&) {
                            // Ignore invalid transport port strings.
                        }
                        i++;
                    }
                    continue;
                }

                if (param == "secureconnection") {
                    options.secureConnection = true;
                    continue;
                }

                if (param == "authenticationtoken") {
                    if (numArgs > i + 1) {
                        options.authenticationToken = argList[i + 1];
                        i++;
                    }
                    continue;
                }

                if (param == "allowcertificatenamemismatch") {
                    options.allowCertificateNameMismatch = true;
                    continue;
                }

                if (param == "allowunverifiedcertificatechain") {
                    options.allowUnverifiedCertificateChain = true;
                    continue;
                }

                if (param == "certificatestore") {
                    if (numArgs > i + 1) {
                        options.certificateStore = argList[i + 1];
                        i++;
                    }
                    continue;
                }

                if (param == "keypassphrase") {
                    if (numArgs > i + 1) {
                        options.keyPassphrase = argList[i + 1];
                        i++;
                    }
                    continue;
                }

                if (param == "subjectname") {
                    if (numArgs > i + 1) {
                        options.subjectName = argList[i + 1];
                        i++;
                    }
                    continue;
                }

                if (param == "authenticationrealm") {
                    if (numArgs > i + 1) {
                        options.authenticationRealm = argList[i + 1];
                        i++;
                    }
                    continue;
                }
            }

            options.host = SplitHostnameAndPortString(arg, options.port);
        }
    }

} // namespace sample
