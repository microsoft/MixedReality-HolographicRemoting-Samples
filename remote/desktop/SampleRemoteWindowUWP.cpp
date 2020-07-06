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

#include "SampleRemoteWindowUWP.h"

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
    CoreApplication::Run(winrt::make<SampleRemoteWindowUWPView>());
}

SampleRemoteWindowUWP::SampleRemoteWindowUWP()
{
    ApplicationView::PreferredLaunchViewSize(winrt::Windows::Foundation::Size(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT));
}

// The first method called when the IFrameworkView is being created.
void SampleRemoteWindowUWP::Initialize(const CoreApplicationView& applicationView)
{
    CoreApplication::Suspending({this, &SampleRemoteWindowUWP::OnSuspending});
    CoreApplication::Resuming({this, &SampleRemoteWindowUWP::OnResuming});

    applicationView.Activated({this, &SampleRemoteWindowUWP::OnViewActivated});

    m_main = std::make_shared<SampleRemoteMain>(weak_from_this());
}

// Called when the CoreWindow object is created (or re-created).
void SampleRemoteWindowUWP::SetWindow(const CoreWindow& window)
{
    m_window = window;

    window.SizeChanged({this, &SampleRemoteWindowUWP::OnWindowSizeChanged});
    window.VisibilityChanged({this, &SampleRemoteWindowUWP::OnVisibilityChanged});
    window.Closed({this, &SampleRemoteWindowUWP::OnWindowClosed});
    window.KeyDown({this, &SampleRemoteWindowUWP::OnKeyDown});
}

// Initializes scene resources, or loads a previously saved app state.
void SampleRemoteWindowUWP::Load(const winrt::hstring& entryPoint)
{
}

// This method is called after the window becomes active.
void SampleRemoteWindowUWP::Run()
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
void SampleRemoteWindowUWP::Uninitialize()
{
}

winrt::com_ptr<IDXGISwapChain1>
    SampleRemoteWindowUWP::CreateSwapChain(const winrt::com_ptr<ID3D11Device1>& device, const DXGI_SWAP_CHAIN_DESC1* desc)
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

winrt::Windows::Graphics::Holographic::HolographicSpace SampleRemoteWindowUWP::CreateHolographicSpace()
{
    return HolographicSpace::CreateForCoreWindow(m_window);
}

winrt::Windows::UI::Input::Spatial::SpatialInteractionManager SampleRemoteWindowUWP::CreateInteractionManager()
{
    return winrt::Windows::UI::Input::Spatial::SpatialInteractionManager::GetForCurrentView();
}

void SampleRemoteWindowUWP::SetWindowTitle(std::wstring title)
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

void SampleRemoteWindowUWP::OnSuspending(const winrt::Windows::Foundation::IInspectable& sender, const SuspendingEventArgs& args)
{
}

void SampleRemoteWindowUWP::OnResuming(
    const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& args)
{
    // Restore any data or state that was unloaded on suspend. By default, data
    // and state are persisted when resuming from suspend. Note that this event
    // does not occur if the app was previously terminated.

    // Insert your code here.
}

// Window event handlers.

void SampleRemoteWindowUWP::OnWindowSizeChanged(
    const winrt::Windows::Foundation::IInspectable& sender, const WindowSizeChangedEventArgs& args)
{
    winrt::Windows::Foundation::Size size = args.Size();
    m_main->OnResize(static_cast<int>(size.Width + 0.5f), static_cast<int>(size.Height + 0.5f));
}

void SampleRemoteWindowUWP::OnVisibilityChanged(
    const winrt::Windows::Foundation::IInspectable& sender, const VisibilityChangedEventArgs& args)
{
    m_windowVisible = args.Visible();
}

void SampleRemoteWindowUWP::OnWindowClosed(const CoreWindow& window, const CoreWindowEventArgs& args)
{
    m_windowClosed = true;
}

void SampleRemoteWindowUWP::OnKeyDown(const CoreWindow& window, const KeyEventArgs& args)
{
    int32_t key = static_cast<int32_t>(args.VirtualKey());
    if (key >= 0 && key <= 0xFF)
    {
        m_main->OnKeyPress(static_cast<char>(tolower(key)));
    }
}

void SampleRemoteWindowUWP::OnViewActivated(CoreApplicationView const& sender, IActivatedEventArgs const& activationArgs)
{
    using namespace winrt::Windows::ApplicationModel;
    using namespace Activation;
    using namespace Core;

    SampleRemoteMain::Options options;
    options.hostname = L"127.0.0.1";
    options.port = 8265;

#if _M_ARM || _M_ARM64
    bool isStandalone = true;
#else
    bool isStandalone = false;
#endif

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

            size_t argCount = args.size();
            for (size_t argIndex = 0; argIndex < argCount; ++argIndex)
            {
                const std::wstring& arg = args[argIndex];

                if (arg.size() == 0)
                    continue;

                if (arg == L"-standalone")
                {
                    isStandalone = true;
                    continue;
                }

                if (arg == L"-noStandalone")
                {
                    isStandalone = false;
                    continue;
                }

                if (arg == L"-listen")
                {
                    options.listen = true;
                    continue;
                }

                if (arg == L"-noautoreconnect")
                {
                    options.autoReconnect = false;
                    continue;
                }

                if (arg == L"-ephemeralport")
                {
                    options.ephemeralPort = true;
                    continue;
                }

                if (arg == L"-transportport")
                {
                    if (argIndex + 1 < argCount)
                    {
                        std::wstring transportPortStr = args[argIndex + 1];
                        try
                        {
                            options.transportPort = std::stoi(transportPortStr);
                        }
                        catch (const std::invalid_argument&)
                        {
                            // Ignore invalid transport port strings.
                        }
                        argIndex++;
                    }
                    continue;
                }

                size_t colonPos = arg.find(L':');
                if (colonPos != std::wstring::npos)
                {
                    std::wstring portStr = arg.substr(colonPos + 1);

                    options.hostname = arg.substr(0, colonPos);
                    int32_t port = std::wcstol(portStr.c_str(), nullptr, 10);

                    // check for invalid port numbers
                    if (port < 0 || port > 65535)
                    {
                        port = 0;
                    }

                    options.port = port;
                }
                else
                {
                    options.hostname = arg.c_str();
                }
            }
        }
    }

    if (!isStandalone)
    {

        m_ipAddress = options.hostname;
        m_port = options.port;

        m_main->ConfigureRemoting(options);
    }
    else
    {
        m_main->InitializeStandalone();
    }

    // Run() won't start until the CoreWindow is activated.
    sender.CoreWindow().Activate();
}

SampleRemoteWindowUWPView::SampleRemoteWindowUWPView()
{
    m_window = std::make_shared<SampleRemoteWindowUWP>();
}

winrt::Windows::ApplicationModel::Core::IFrameworkView SampleRemoteWindowUWPView::CreateView()
{
    return *this;
}

void SampleRemoteWindowUWPView::Initialize(const winrt::Windows::ApplicationModel::Core::CoreApplicationView& applicationView)
{
    m_window->Initialize(applicationView);
}

void SampleRemoteWindowUWPView::SetWindow(const winrt::Windows::UI::Core::CoreWindow& window)
{
    m_window->SetWindow(window);
}

void SampleRemoteWindowUWPView::Load(const winrt::hstring& entryPoint)
{
    m_window->Load(entryPoint);
}

void SampleRemoteWindowUWPView::Run()
{
    m_window->Run();
}

void SampleRemoteWindowUWPView::Uninitialize()
{
    m_window->Uninitialize();
}
