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

#include <SampleWindowWin32.h>

#include <future>

#include <dxgi1_2.h>

namespace {
    std::once_flag g_createWindowClass;

    LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        LRESULT result = 0;
        switch (msg) {
        case WM_CLOSE: {
            sample::SampleWindowWin32* window = reinterpret_cast<sample::SampleWindowWin32*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            window->OnClosed();

            DestroyWindow(hWnd);
            result = 0;
        } break;

        case WM_DESTROY: {
            PostQuitMessage(0);
            result = 0;
        } break;

        case WM_CHAR: {
            sample::SampleWindowWin32* window = reinterpret_cast<sample::SampleWindowWin32*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            window->OnKeyPress(static_cast<wchar_t>(wParam));
        }

        default: {
            result = DefWindowProc(hWnd, msg, wParam, lParam);
        } break;
        }
        return result;
    }

    HWND CreateWindowWin32(sample::SampleWindowWin32* window, std::wstring title, long width, long height) {
        const wchar_t* windowClassName = L"SampleWindowWin32Class";

        std::call_once(g_createWindowClass, [&] {
            WNDCLASSEX wcex = {};
            wcex.cbSize = sizeof(wcex);
            wcex.style = CS_HREDRAW | CS_VREDRAW;
            wcex.lpfnWndProc = ::WndProc;
            wcex.hInstance = 0;
            wcex.hIcon = LoadIcon(0, IDI_APPLICATION);
            wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wcex.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
            wcex.lpszClassName = windowClassName;
            RegisterClassEx(&wcex);
        });

        RECT rc = {0, 0, width, height};
        AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);

        HWND hWnd = CreateWindow(windowClassName,
                                 title.c_str(),
                                 WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 rc.right - rc.left,
                                 rc.bottom - rc.top,
                                 nullptr,
                                 nullptr,
                                 0,
                                 nullptr);

        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));

        return hWnd;
    }

    winrt::com_ptr<IDXGISwapChain1> CreateSwapchain(HWND hWnd, ID3D11Device* device) {
        winrt::com_ptr<IDXGISwapChain1> swapchain;

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.Stereo = false;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 3;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.Flags = 0;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        desc.Scaling = DXGI_SCALING_STRETCH;

        winrt::com_ptr<IDXGIDevice1> dxgiDevice;
        device->QueryInterface(dxgiDevice.put());

        winrt::com_ptr<IDXGIAdapter> dxgiAdapter;
        winrt::check_hresult(dxgiDevice->GetAdapter(dxgiAdapter.put()));

        winrt::com_ptr<IDXGIFactory2> dxgiFactory;
        winrt::check_hresult(dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.put())));

        winrt::check_hresult(dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
        winrt::check_hresult(dxgiFactory->CreateSwapChainForHwnd(device, hWnd, &desc, nullptr, nullptr, swapchain.put()));

        return swapchain;
    }
} // namespace

namespace sample {
    SampleWindowWin32::SampleWindowWin32(std::wstring title, ID3D11Device* device)
        : SampleWindowWin32(title, device, 512, 512) {
    }

    SampleWindowWin32::SampleWindowWin32(std::wstring title, ID3D11Device* device, long width, long height) {
        std::promise<HWND> hWndPromise;
        std::future<HWND> hWndFuture = hWndPromise.get_future();

        m_windowThread = std::thread([&]() mutable {
            HWND hWnd = nullptr;
            try {
                hWnd = CreateWindowWin32(this, title, width, height);
                hWndPromise.set_value(hWnd);
            } catch (...) {
                hWndPromise.set_exception(std::current_exception());
                return;
            }

            MSG msg;
            while (GetMessage(&msg, nullptr, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        });

        m_hWnd = hWndFuture.get();
        m_swapchain = CreateSwapchain(m_hWnd, device);

        ShowWindow(m_hWnd, SW_SHOWNORMAL);
    }

    SampleWindowWin32::~SampleWindowWin32() {
        SendMessage(m_hWnd, WM_CLOSE, 0, 0);
        m_windowThread.join();
    }

    winrt::com_ptr<ID3D11Texture2D> SampleWindowWin32::GetNextSwapchainTexture() {
        winrt::com_ptr<ID3D11Texture2D> texture;
        winrt::check_hresult(m_swapchain->GetBuffer(0, IID_PPV_ARGS(texture.put())));

        return texture;
    }

    void SampleWindowWin32::PresentSwapchain() {
        winrt::check_hresult(m_swapchain->Present(0, 0));
    }

    void SampleWindowWin32::OnClosed() {
        std::lock_guard lock(m_windowMutex);
        m_isClosed = true;
    }

    bool SampleWindowWin32::IsClosed() const {
        std::lock_guard lock(m_windowMutex);
        return m_isClosed;
    }

    void SampleWindowWin32::SetKeyPressedHandler(KeyPressHandler keyPressedHandler) {
        std::lock_guard lock(m_windowMutex);
        m_keyPressedHandler = keyPressedHandler;
    }

    void SampleWindowWin32::OnKeyPress(wchar_t key) {
        std::lock_guard lock(m_windowMutex);
        if (m_keyPressedHandler) {
            m_keyPressedHandler(key);
        }
    }

    void SampleWindowWin32::SetWindowTitle(std::wstring title) {
        SetWindowTextW(m_hWnd, title.c_str());
    }
} // namespace sample
