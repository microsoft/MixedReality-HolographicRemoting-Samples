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

namespace sample {
    struct AppOptions {
        bool listen{false};
        std::string host;
        uint16_t port{0};
        uint16_t transportPort{0};
        bool isStandalone = false;
        bool noUserWait = false;
        bool useEphemeralPort = false;
        bool secureConnection{false};
        std::string authenticationToken;
        bool allowCertificateNameMismatch{false};
        bool allowUnverifiedCertificateChain{false};
        std::string certificateStore;
        std::string keyPassphrase;
        std::string subjectName;
        std::string authenticationRealm{"OpenXR Remoting"};
    };

    void ParseCommandLine(sample::AppOptions& options);

} // namespace sample
