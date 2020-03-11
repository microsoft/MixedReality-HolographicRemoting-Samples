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

#include <windows.graphics.holographic.h>
#include <windows.ui.input.spatial.h>

#include <SampleRemoteWindowWin32.h>

#include <HolographicAppRemoting/Streamer.h>
#include <HolographicSpaceInterop.h>
#include <SpatialInteractionManagerInterop.h>

#include <Common/DbgLog.h>
#include <DirectXColors.h>

#include <codecvt>
#include <regex>

#include <windows.graphics.directx.direct3d11.interop.h>

#define WINDOWCLASSNAME L"SampleRemoteWindowWin32Class"

LRESULT CALLBACK wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static SampleRemoteWindowWin32* s_sampleHostWindow;

    LRESULT result = 0;

    switch (msg)
    {
        case WM_CREATE:
        {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            s_sampleHostWindow = reinterpret_cast<SampleRemoteWindowWin32*>(cs->lpCreateParams);

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

namespace
{
    std::wstring SplitHostnameAndPortString(const std::wstring& address, uint16_t& port)
    {
        static std::basic_regex<wchar_t> addressMatcher(L"(?:(\\[.*\\])|([^:]*))(?:[:](\\d+))?");
        std::match_results<typename std::wstring::const_iterator> results;
        if (std::regex_match(address, results, addressMatcher))
        {
            if (results[3].matched)
            {
                std::wstring portStr = results[3].str();
                port = static_cast<uint16_t>(std::wcstol(portStr.c_str(), nullptr, 10));
            }

            return (results[1].matched) ? results[1].str() : results[2].str();
        }
        else
        {
            return address;
        }
    }
} // namespace

void SampleRemoteWindowWin32::Initialize()
{
    m_main = std::make_shared<SampleRemoteMain>(weak_from_this());
}

void SampleRemoteWindowWin32::InitializeHwnd(HWND hWnd)
{
    m_hWnd = hWnd;
}

void SampleRemoteWindowWin32::ConfigureRemoting(
    bool listen, const std::wstring& hostname, uint16_t port, uint16_t transportPort, bool ephemeralPort)
{
    m_main->ConfigureRemoting(listen, hostname, port, transportPort, ephemeralPort);
}

void SampleRemoteWindowWin32::Connect()
{
    m_main->InitializeRemoteContextAndConnectOrListen();
}

void SampleRemoteWindowWin32::InitializeStandalone()
{
    m_main->InitializeStandalone();
}

void SampleRemoteWindowWin32::Tick()
{
    if (const HolographicFrame& holographicFrame = m_main->Update())
    {
        m_main->Render(holographicFrame);
    }
}

void SampleRemoteWindowWin32::OnKeyPress(char key)
{
    m_main->OnKeyPress(key);
}

void SampleRemoteWindowWin32::OnResize(int width, int height)
{
    m_main->OnResize(width, height);
}

winrt::com_ptr<IDXGISwapChain1>
    SampleRemoteWindowWin32::CreateSwapChain(const winrt::com_ptr<ID3D11Device1>& device, const DXGI_SWAP_CHAIN_DESC1* desc)
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

void SampleRemoteWindowWin32::SetWindowTitle(std::wstring title)
{
    if (m_hWnd)
    {
        if (!SetWindowTextW(m_hWnd, title.c_str()))
        {
            winrt::check_hresult(HRESULT_FROM_WIN32(GetLastError()));
        }
    }
}

winrt::Windows::Graphics::Holographic::HolographicSpace SampleRemoteWindowWin32::CreateHolographicSpace()
{
    // Use WinRT factory to create the holographic space.
    // See https://docs.microsoft.com/en-us/windows/win32/api/holographicspaceinterop/
    winrt::com_ptr<IHolographicSpaceInterop> holographicSpaceInterop =
        winrt::get_activation_factory<HolographicSpace, IHolographicSpaceInterop>();

    winrt::com_ptr<ABI::Windows::Graphics::Holographic::IHolographicSpace> spHolographicSpace;
    winrt::check_hresult(holographicSpaceInterop->CreateForWindow(
        m_hWnd, __uuidof(ABI::Windows::Graphics::Holographic::IHolographicSpace), winrt::put_abi(spHolographicSpace)));

    return spHolographicSpace.as<HolographicSpace>();
}

winrt::Windows::UI::Input::Spatial::SpatialInteractionManager SampleRemoteWindowWin32::CreateInteractionManager()
{
    using namespace winrt::Windows::UI::Input::Spatial;

    // Use WinRT factory to create the spatial interaction manager.
    // See https://docs.microsoft.com/en-us/windows/win32/api/spatialinteractionmanagerinterop/
    winrt::com_ptr<ISpatialInteractionManagerInterop> spatialInteractionManagerInterop =
        winrt::get_activation_factory<SpatialInteractionManager, ISpatialInteractionManagerInterop>();

    winrt::com_ptr<ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager> spSpatialInteractionManager;
    winrt::check_hresult(spatialInteractionManagerInterop->GetForWindow(
        m_hWnd, __uuidof(ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager), winrt::put_abi(spSpatialInteractionManager)));

    return spSpatialInteractionManager.as<SpatialInteractionManager>();
}

int main(Platform::Array<Platform::String ^> ^ args)
{
    winrt::init_apartment();

    bool listen{false};
    std::wstring host;
    uint16_t port{0};
    uint16_t transportPort{0};
    bool isStandalone = false;
    bool noUserWait = false;
    bool useEphemeralPort = false;

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
                continue;
            }

            if (param == L"standalone")
            {
                isStandalone = true;
                continue;
            }

            if (param == L"nouserwait")
            {
                noUserWait = true;
                continue;
            }

            if (param == L"ephemeralport")
            {
                useEphemeralPort = true;
                continue;
            }

            if (param == L"transportport")
            {
                if (args->Length > i + 1)
                {
                    std::wstring transportPortStr = args[i + 1]->Data();
                    transportPort = std::stoi(transportPortStr);
                    i++;
                }
                continue;
            }
        }

        host = SplitHostnameAndPortString(arg, port);
    }

    std::shared_ptr<SampleRemoteWindowWin32> sampleHostWindow = std::make_shared<SampleRemoteWindowWin32>();
    sampleHostWindow->Initialize();

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

    if (!isStandalone)
    {
        sampleHostWindow->ConfigureRemoting(listen, host, port, transportPort, useEphemeralPort);
        if (noUserWait)
        {
            sampleHostWindow->Connect();
        }
    }
    else
    {
        sampleHostWindow->InitializeStandalone();
    }

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
            try
            {
                sampleHostWindow->Tick();
            }
            catch (...)
            {
                // Unhandeled exception during tick, exit program
                return 1;
            }
        }
    }

    return 0;
}
