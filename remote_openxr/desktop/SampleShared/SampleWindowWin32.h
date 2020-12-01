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
#include <functional>
#include <mutex>

struct IDXGISwapChain1;

namespace sample {
    class SampleWindowWin32 {
    public:
        SampleWindowWin32(std::wstring title, ID3D11Device* device);
        SampleWindowWin32(std::wstring title, ID3D11Device* device, long width, long height);

        ~SampleWindowWin32();

        using KeyPressHandler = std::function<void(wchar_t)>;
        void SetKeyPressedHandler(KeyPressHandler keyPressedHandler);
        void OnKeyPress(wchar_t key);

        void SetWindowTitle(std::wstring title);

        ID3D11Texture2D* GetNextSwapchainTexture();
        void PresentSwapchain();

        void OnClosed();
        bool IsClosed() const;

    private:
        std::thread m_windowThread;
        mutable std::mutex m_windowMutex;

        HWND m_hWnd = nullptr;
        winrt::com_ptr<IDXGISwapChain1> m_swapchain;

        KeyPressHandler m_keyPressedHandler;
        bool m_isClosed;
    };
} // namespace sample
