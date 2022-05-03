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

#include <fstream>

class SecureConnectionCallbacks {
public:
    SecureConnectionCallbacks(const std::string& authenticationToken,
                              bool allowCertificateNameMismatch,
                              bool allowUnverifiedCertificateChain,
                              const std::string& keyPassphrase,
                              const std::string& subjectName,
                              const std::string& certificateStore,
                              bool listen)
        : m_authenticationToken(authenticationToken)
        , m_allowCertificateNameMismatch(allowCertificateNameMismatch)
        , m_allowUnverifiedCertificateChain(allowUnverifiedCertificateChain)
        , m_keyPassphrase(keyPassphrase)
        , m_subjectName(subjectName)
        , m_certificateStoreName(certificateStore)
        , m_listen(listen) {
    }

    void InitializeSecureConnection() {
        if (m_authenticationToken.empty()) {
            throw std::logic_error("Authentication token must be specified for secure connections.");
        }

        if (m_listen) {
            if (m_certificateStoreName.empty() || m_subjectName.empty()) {
                throw std::logic_error("Certificate store and subject name must be specified for secure listening.");
            }

            constexpr size_t maxCertStoreSize = 1 << 20;
            std::ifstream certStoreStream(m_certificateStoreName, std::ios::binary);
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

    // Static callbacks
    static XrResult XRAPI_CALL
    RequestAuthenticationTokenStaticCallback(XrRemotingAuthenticationTokenRequestMSFT* authenticationTokenRequest) {
        if (!authenticationTokenRequest->context) {
            return XR_ERROR_RUNTIME_FAILURE;
        }
        return reinterpret_cast<SecureConnectionCallbacks*>(authenticationTokenRequest->context)
            ->RequestAuthenticationToken(authenticationTokenRequest);
    }

    static XrResult XRAPI_CALL
    ValidateServerCertificateStaticCallback(XrRemotingServerCertificateValidationMSFT* serverCertificateValidation) {
        if (!serverCertificateValidation->context) {
            return XR_ERROR_RUNTIME_FAILURE;
        }
        return reinterpret_cast<SecureConnectionCallbacks*>(serverCertificateValidation->context)
            ->ValidateServerCertificate(serverCertificateValidation);
    }

    static XrResult XRAPI_CALL
    ValidateAuthenticationTokenStaticCallback(XrRemotingAuthenticationTokenValidationMSFT* authenticationTokenValidation) {
        if (!authenticationTokenValidation->context) {
            return XR_ERROR_RUNTIME_FAILURE;
        }
        return reinterpret_cast<SecureConnectionCallbacks*>(authenticationTokenValidation->context)
            ->ValidateAuthenticationToken(authenticationTokenValidation);
    }

    static XrResult XRAPI_CALL RequestServerCertificateStaticCallback(XrRemotingServerCertificateRequestMSFT* serverCertificateRequest) {
        if (!serverCertificateRequest->context) {
            return XR_ERROR_RUNTIME_FAILURE;
        }
        return reinterpret_cast<SecureConnectionCallbacks*>(serverCertificateRequest->context)
            ->RequestServerCertificate(serverCertificateRequest);
    }

private:
    XrResult RequestAuthenticationToken(XrRemotingAuthenticationTokenRequestMSFT* authenticationTokenRequest) {
        const std::string tokenUtf8 = m_authenticationToken;
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

    XrResult ValidateServerCertificate(XrRemotingServerCertificateValidationMSFT* serverCertificateValidation) {
        if (!serverCertificateValidation->systemValidationResult) {
            return XR_ERROR_RUNTIME_FAILURE; // We requested system validation to be performed
        }

        serverCertificateValidation->validationResultOut = *serverCertificateValidation->systemValidationResult;
        if (m_allowCertificateNameMismatch && serverCertificateValidation->validationResultOut.nameValidationResult ==
                                                  XR_REMOTING_CERTIFICATE_NAME_VALIDATION_RESULT_MISMATCH_MSFT) {
            serverCertificateValidation->validationResultOut.nameValidationResult =
                XR_REMOTING_CERTIFICATE_NAME_VALIDATION_RESULT_MATCH_MSFT;
        }
        if (m_allowUnverifiedCertificateChain) {
            serverCertificateValidation->validationResultOut.trustedRoot = true;
        }

        return XR_SUCCESS;
    }

    XrResult ValidateAuthenticationToken(XrRemotingAuthenticationTokenValidationMSFT* authenticationTokenValidation) {
        const std::string tokenUtf8 = m_authenticationToken;
        authenticationTokenValidation->tokenValidOut =
            (authenticationTokenValidation->token != nullptr && tokenUtf8 == authenticationTokenValidation->token);
        return XR_SUCCESS;
    }

    XrResult RequestServerCertificate(XrRemotingServerCertificateRequestMSFT* serverCertificateRequest) {
        const std::string subjectNameUtf8 = m_subjectName;
        const std::string passPhraseUtf8 = m_keyPassphrase;

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

private:
    const std::string m_authenticationToken;
    const bool m_allowCertificateNameMismatch;
    const bool m_allowUnverifiedCertificateChain;
    const std::string m_keyPassphrase;
    const std::string m_subjectName;
    const std::string m_certificateStoreName;
    std::vector<uint8_t> m_certificateStore;
    const bool m_listen;
};
