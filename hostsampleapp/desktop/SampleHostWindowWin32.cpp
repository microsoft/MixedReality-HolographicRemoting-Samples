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

#include "pch.h"

#include "SampleHostWindowWin32.h"

#include <HolographicAppRemoting\Streamer.h>

#include "Common\DbgLog.h"
#include <DirectXColors.h>

#include <windows.graphics.directx.direct3d11.interop.h>

#define WINDOWCLASSNAME L"SampleHostWindowWin32Class"

LRESULT CALLBACK wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static SampleHostWindowWin32* s_sampleHostWindow;

    LRESULT result = 0;

    switch (msg)
    {
        case WM_CREATE:
        {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            s_sampleHostWindow = reinterpret_cast<SampleHostWindowWin32*>(cs->lpCreateParams);

            RECT clientRect;
            GetClientRect(hWnd, &clientRect);

            s_sampleHostWindow->OnResize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
            result = 0;
        }
        break;
        case WM_WINDOWPOSCHANGED:
        {
            auto windowPos = reinterpret_cast<WINDOWPOS*>(lParam);
            if ((windowPos->flags & SWP_NOSIZE) == 0)
            {
                RECT clientRect;
                GetClientRect(hWnd, &clientRect);

                s_sampleHostWindow->OnResize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
            }
            result = 0;
        }
        break;
        case WM_DESTROY:
        {
            s_sampleHostWindow = nullptr;
            result = 0;
            PostQuitMessage(0);
        }
        break;
        case WM_CLOSE:
        {
            DestroyWindow(hWnd);
            result = 0;
        }
        break;
        case WM_CHAR:
        {
            const int key = tolower(static_cast<int>(wParam));
            s_sampleHostWindow->OnKeyPress(static_cast<char>(key));
        }
        break;
        default:
            result = DefWindowProc(hWnd, msg, wParam, lParam);
            break;
    }

    return result;
}

void SampleHostWindowWin32::Initialize(bool listen, const std::wstring& hostname, uint32_t port)
{
    m_main = std::make_shared<SampleHostMain>(weak_from_this());
    m_main->SetHostOptions(listen, hostname, port);
}

void SampleHostWindowWin32::InitializeHwnd(HWND hWnd)
{
    m_hWnd = hWnd;
}

void SampleHostWindowWin32::Tick()
{
    if (const HolographicFrame& holographicFrame = m_main->Update())
    {
        m_main->Render(holographicFrame);
    }
}

void SampleHostWindowWin32::OnKeyPress(char key)
{
    m_main->OnKeyPress(key);
}


void SampleHostWindowWin32::OnResize(int width, int height)
{
    m_main->OnResize(width, height);
}

winrt::com_ptr<IDXGISwapChain1>
    SampleHostWindowWin32::CreateSwapChain(const winrt::com_ptr<ID3D11Device1>& device, const DXGI_SWAP_CHAIN_DESC1* desc)
{
    winrt::com_ptr<IDXGIDevice1> dxgiDevice;
    device.as(dxgiDevice);

    winrt::com_ptr<IDXGIAdapter> dxgiAdapter;
    winrt::check_hresult(dxgiDevice->GetAdapter(dxgiAdapter.put()));

    winrt::com_ptr<IDXGIFactory2> dxgiFactory;
    winrt::check_hresult(dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), dxgiFactory.put_void()));

    winrt::check_hresult(dxgiFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER));

    winrt::com_ptr<IDXGISwapChain1> swapChain = nullptr;
    winrt::check_hresult(dxgiFactory->CreateSwapChainForHwnd(device.get(), m_hWnd, desc, nullptr, nullptr, swapChain.put()));

    return swapChain;
}

void SampleHostWindowWin32::SetWindowTitle(std::wstring title)
{
    if (m_hWnd)
    {
        if (!SetWindowTextW(m_hWnd, title.c_str()))
        {
            winrt::check_hresult(HRESULT_FROM_WIN32(GetLastError()));
        }
    }
}

int main(Platform::Array<Platform::String ^> ^ args)
{
    winrt::init_apartment();

    bool listen{false};
    std::wstring host;
    uint32_t port{0};

    for (unsigned int i = 1; i < args->Length; ++i)
    {
        if (args[i]->Length() == 0)
            continue;

        std::wstring arg = args[i]->Data();
        if (arg[0] == '-')
        {
            std::wstring param = arg.substr(1);
            std::transform(param.begin(), param.end(), param.begin(), ::tolower);

            if (param == L"listen")
            {
                listen = true;
            }

            continue;
        }

        size_t colonPos = arg.find(L':');
        if (colonPos != std::wstring::npos)
        {
            std::wstring portStr = arg.substr(colonPos + 1);

            host = arg.substr(0, colonPos);
            port = std::wcstol(portStr.c_str(), nullptr, 10);
        }
        else
        {
            host = arg.c_str();
            port = 0;
        }
    }

    std::shared_ptr<SampleHostWindowWin32> sampleHostWindow = std::make_shared<SampleHostWindowWin32>();
    sampleHostWindow->Initialize(listen, host, port);

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = wndProc;
    wcex.hInstance = 0;
    wcex.hIcon = LoadIcon(0, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wcex.lpszClassName = WINDOWCLASSNAME;
    RegisterClassExW(&wcex);

    RECT rc = {0, 0, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT};
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);

    std::wstring windowName = TITLE_TEXT;

    HWND hWnd = CreateWindowW(
        WINDOWCLASSNAME,
        windowName.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        0,
        sampleHostWindow.get());

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);

    sampleHostWindow->InitializeHwnd(hWnd);

    ShowWindow(hWnd, SW_SHOWNORMAL);
    bool quit = false;
    while (!quit)
    {
        MSG msg = {0};
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                quit = true;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            sampleHostWindow->Tick();
        }
    }

    return 0;
}
