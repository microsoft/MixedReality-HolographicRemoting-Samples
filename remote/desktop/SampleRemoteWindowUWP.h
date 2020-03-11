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

#include "SampleRemoteMain.h"

#include "Common\DeviceResources.h"

#include <winrt/Microsoft.Holographic.AppRemoting.h>
#include <winrt/Windows.ApplicationModel.Core.h>

// Main entry point for our app. Connects the app with the Windows shell and handles application lifecycle events.
class SampleRemoteWindowUWP : public std::enable_shared_from_this<SampleRemoteWindowUWP>, public SampleRemoteMain::IWindow
{
public:
    SampleRemoteWindowUWP();

    // IFrameworkView methods
    virtual void Initialize(const winrt::Windows::ApplicationModel::Core::CoreApplicationView& applicationView);
    virtual void SetWindow(const winrt::Windows::UI::Core::CoreWindow& window);
    virtual void Load(const winrt::hstring& entryPoint);
    virtual void Run();
    virtual void Uninitialize();

    // SampleRemoteMain::IWindow methods.
    virtual winrt::com_ptr<IDXGISwapChain1>
        CreateSwapChain(const winrt::com_ptr<ID3D11Device1>& device, const DXGI_SWAP_CHAIN_DESC1* desc) override;

    virtual winrt::Windows::Graphics::Holographic::HolographicSpace CreateHolographicSpace() override;

    virtual winrt::Windows::UI::Input::Spatial::SpatialInteractionManager CreateInteractionManager() override;

    virtual void SetWindowTitle(std::wstring title) override;

protected:
    // Application lifecycle event handlers.
    void OnSuspending(
        const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::ApplicationModel::SuspendingEventArgs& args);
    void OnResuming(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& args);

    // Activation handling
    void OnViewActivated(
        winrt::Windows::ApplicationModel::Core::CoreApplicationView const& sender,
        winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs const& args);

    // Window event handlers.
    void OnWindowSizeChanged(
        const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Core::WindowSizeChangedEventArgs& args);
    void OnVisibilityChanged(
        const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Core::VisibilityChangedEventArgs& args);
    void OnWindowClosed(const winrt::Windows::UI::Core::CoreWindow& window, const winrt::Windows::UI::Core::CoreWindowEventArgs& args);
    void OnKeyDown(const winrt::Windows::UI::Core::CoreWindow& window, const winrt::Windows::UI::Core::KeyEventArgs& args);

private:
    winrt::Windows::UI::Core::CoreWindow m_window = nullptr;
    std::shared_ptr<SampleRemoteMain> m_main;
    std::wstring m_ipAddress;
    int32_t m_port = 8265;
    bool m_windowClosed = false;
    bool m_windowVisible = true;
};

class SampleRemoteWindowUWPView : public winrt::implements<
                                      SampleRemoteWindowUWPView,
                                      winrt::Windows::ApplicationModel::Core::IFrameworkViewSource,
                                      winrt::Windows::ApplicationModel::Core::IFrameworkView>
{
public:
    SampleRemoteWindowUWPView();

    // IFrameworkViewSource methods.
    winrt::Windows::ApplicationModel::Core::IFrameworkView CreateView();

    // IFrameworkView methods.
    virtual void Initialize(const winrt::Windows::ApplicationModel::Core::CoreApplicationView& applicationView);
    virtual void SetWindow(const winrt::Windows::UI::Core::CoreWindow& window);
    virtual void Load(const winrt::hstring& entryPoint);
    virtual void Run();
    virtual void Uninitialize();

private:
    std::shared_ptr<SampleRemoteWindowUWP> m_window;
};
