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

#include "SampleHostWindowUWP.h"

#include "Common\DbgLog.h"
#include "Common\Speech.h"

#include <iterator>
#include <sstream>

#include <HolographicAppRemoting/Streamer.h>

#include <windows.graphics.directx.direct3d11.interop.h>

#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.ui.ViewManagement.h>

#define INITIAL_WINDOW_WIDTH 1280
#define INITIAL_WINDOW_HEIGHT 720

#define TITLE_SEPARATOR L" | "

using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Microsoft::Holographic::AppRemoting;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;


// The main function is only used to initialize our IFrameworkView class.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment();
    CoreApplication::Run(winrt::make<SampleHostWindowUWPView>());
}

SampleHostWindowUWP::SampleHostWindowUWP()
{
    ApplicationView::PreferredLaunchViewSize(winrt::Windows::Foundation::Size(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT));
}

// The first method called when the IFrameworkView is being created.
void SampleHostWindowUWP::Initialize(const CoreApplicationView& applicationView)
{
    CoreApplication::Suspending({this, &SampleHostWindowUWP::OnSuspending});
    CoreApplication::Resuming({this, &SampleHostWindowUWP::OnResuming});

    applicationView.Activated({this, &SampleHostWindowUWP::OnViewActivated});

    m_main = std::make_shared<SampleHostMain>(weak_from_this());
}

// Called when the CoreWindow object is created (or re-created).
void SampleHostWindowUWP::SetWindow(const CoreWindow& window)
{
    m_window = window;

    window.SizeChanged({this, &SampleHostWindowUWP::OnWindowSizeChanged});
    window.VisibilityChanged({this, &SampleHostWindowUWP::OnVisibilityChanged});
    window.Closed({this, &SampleHostWindowUWP::OnWindowClosed});
    window.KeyDown({this, &SampleHostWindowUWP::OnKeyDown});
}

// Initializes scene resources, or loads a previously saved app state.
void SampleHostWindowUWP::Load(const winrt::hstring& entryPoint)
{
}

// This method is called after the window becomes active.
void SampleHostWindowUWP::Run()
{
    CoreWindow window = CoreWindow::GetForCurrentThread();
    window.Activate();

    while (!m_windowClosed)
    {
        if (m_windowVisible)
        {
            CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

            if (m_main)
            {
                if (const HolographicFrame& holographicFrame = m_main->Update())
                {
                    m_main->Render(holographicFrame);
                }
            }
        }
        else
        {
            CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
        }
    }
}

// Required for IFrameworkView.
// Terminate events do not cause Uninitialize to be called. It will be called if your IFrameworkView
// class is torn down while the app is in the foreground.
void SampleHostWindowUWP::Uninitialize()
{
}


winrt::com_ptr<IDXGISwapChain1>
    SampleHostWindowUWP::CreateSwapChain(const winrt::com_ptr<ID3D11Device1>& device, const DXGI_SWAP_CHAIN_DESC1* desc)
{
    winrt::com_ptr<IDXGIDevice3> dxgiDevice;
    device.as(dxgiDevice);

    winrt::com_ptr<IDXGIAdapter> dxgiAdapter;
    winrt::check_hresult(dxgiDevice->GetAdapter(dxgiAdapter.put()));

    winrt::com_ptr<IDXGIFactory4> dxgiFactory;
    winrt::check_hresult(dxgiAdapter->GetParent(__uuidof(dxgiFactory), dxgiFactory.put_void()));

    winrt::com_ptr<IDXGISwapChain1> swapChain;
    winrt::check_hresult(dxgiFactory->CreateSwapChainForCoreWindow(
        device.get(), static_cast<::IUnknown*>(winrt::get_abi(m_window)), desc, nullptr, swapChain.put()));

    return swapChain;
}

void SampleHostWindowUWP::SetWindowTitle(std::wstring title)
{
    auto dispatcher = winrt::Windows::ApplicationModel::Core::CoreApplication::MainView().CoreWindow().Dispatcher();

    auto doSetWindowTitle = [title]() {
        try
        {
            if (auto view = winrt::Windows::UI::ViewManagement::ApplicationView::GetForCurrentView())
            {
                view.Title(winrt::to_hstring(title.c_str()));
            }
        }
        catch (const winrt::hresult_error&)
        {
        }
    };

    if (dispatcher.HasThreadAccess())
    {
        doSetWindowTitle();
    }
    else
    {
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, doSetWindowTitle);
    }
}

// Application lifecycle event handlers.

void SampleHostWindowUWP::OnSuspending(const winrt::Windows::Foundation::IInspectable& sender, const SuspendingEventArgs& args)
{
}

void SampleHostWindowUWP::OnResuming(
    const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& args)
{
    // Restore any data or state that was unloaded on suspend. By default, data
    // and state are persisted when resuming from suspend. Note that this event
    // does not occur if the app was previously terminated.

    // Insert your code here.
}

// Window event handlers.

void SampleHostWindowUWP::OnWindowSizeChanged(
    const winrt::Windows::Foundation::IInspectable& sender, const WindowSizeChangedEventArgs& args)
{
    winrt::Windows::Foundation::Size size = args.Size();
    m_main->OnResize(static_cast<int>(size.Width + 0.5f), static_cast<int>(size.Height + 0.5f));
}

void SampleHostWindowUWP::OnVisibilityChanged(
    const winrt::Windows::Foundation::IInspectable& sender, const VisibilityChangedEventArgs& args)
{
    m_windowVisible = args.Visible();
}

void SampleHostWindowUWP::OnWindowClosed(const CoreWindow& window, const CoreWindowEventArgs& args)
{
    m_windowClosed = true;
}

void SampleHostWindowUWP::OnKeyDown(const CoreWindow& window, const KeyEventArgs& args)
{
    int32_t key = static_cast<int32_t>(args.VirtualKey());
    if (key >= 0 && key <= 0xFF)
    {
        m_main->OnKeyPress(static_cast<char>(tolower(key)));
    }
}

void SampleHostWindowUWP::OnViewActivated(CoreApplicationView const& sender, IActivatedEventArgs const& activationArgs)
{
    using namespace winrt::Windows::ApplicationModel;
    using namespace Activation;
    using namespace Core;

    std::wstring host = L"127.0.0.1";
    int32_t port = 8265;

    if (activationArgs != nullptr)
    {
        ActivationKind activationKind = activationArgs.Kind();
        if (activationKind == Activation::ActivationKind::Launch)
        {
            LaunchActivatedEventArgs launchArgs = activationArgs.as<LaunchActivatedEventArgs>();

            std::vector<std::wstring> args;
            std::wistringstream stream(std::wstring(launchArgs.Arguments()));
            std::copy(
                std::istream_iterator<std::wstring, wchar_t>(stream),
                std::istream_iterator<std::wstring, wchar_t>(),
                std::back_inserter(args));

            for (const std::wstring& arg : args)
            {
                if (arg.size() == 0)
                    continue;

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
                }
            }
        }
    }

    // check for invalid port numbers
    if (port < 0 || port > 65535)
    {
        port = 0;
    }

    m_ipAddress = host;
    m_port = port;

    m_main->SetHostOptions(false, m_ipAddress, m_port);

    // Run() won't start until the CoreWindow is activated.
    sender.CoreWindow().Activate();
}

SampleHostWindowUWPView::SampleHostWindowUWPView()
{
    m_window = std::make_shared<SampleHostWindowUWP>();
}

winrt::Windows::ApplicationModel::Core::IFrameworkView SampleHostWindowUWPView::CreateView()
{
    return *this;
}

void SampleHostWindowUWPView::Initialize(const winrt::Windows::ApplicationModel::Core::CoreApplicationView& applicationView)
{
    m_window->Initialize(applicationView);
}

void SampleHostWindowUWPView::SetWindow(const winrt::Windows::UI::Core::CoreWindow& window)
{
    m_window->SetWindow(window);
}

void SampleHostWindowUWPView::Load(const winrt::hstring& entryPoint)
{
    m_window->Load(entryPoint);
}

void SampleHostWindowUWPView::Run()
{
    m_window->Run();
}

void SampleHostWindowUWPView::Uninitialize()
{
    m_window->Uninitialize();
}
