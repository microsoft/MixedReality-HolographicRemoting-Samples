#pragma once

#include <holographic/RemoteWindowHolographic.h>

#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.Spatial.h>

#include <memory>

class RemoteWindowHolographicUwp : public RemoteWindowHolographic
{
public:
    RemoteWindowHolographicUwp(const std::shared_ptr<IRemoteAppHolographic>& app, const winrt::Windows::UI::Core::CoreWindow& m_coreWindow);

    // IRemoteWindowHolographic methods
    // --------------------------------

    virtual winrt::com_ptr<IDXGISwapChain1> CreateSwapChain(
        const winrt::com_ptr<ID3D11Device1>& device, const DXGI_SWAP_CHAIN_DESC1* desc) override;

    virtual winrt::Windows::Graphics::Holographic::HolographicSpace CreateHolographicSpace() override;

    virtual winrt::Windows::UI::Input::Spatial::SpatialInteractionManager CreateInteractionManager() override;

    virtual void SetWindowTitle(std::wstring title) override;

private:
    void OnKeyDown(const winrt::Windows::UI::Core::CoreWindow& window, const winrt::Windows::UI::Core::KeyEventArgs& args);
    void OnSizeChanged(
        const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Core::WindowSizeChangedEventArgs& args);

    winrt::Windows::UI::Core::CoreWindow m_coreWindow = nullptr;
    winrt::Windows::UI::Core::CoreWindow::KeyDown_revoker m_onKeyDownRevoker;
    winrt::Windows::UI::Core::CoreWindow::SizeChanged_revoker m_onSizeChangedRevoker;
};

class RemoteWindowHolographicUwpView : public winrt::implements<
                                           RemoteWindowHolographicUwpView,
                                           winrt::Windows::ApplicationModel::Core::IFrameworkViewSource,
                                           winrt::Windows::ApplicationModel::Core::IFrameworkView>
{
public:
    RemoteWindowHolographicUwpView();

    // IFrameworkViewSource methods.
    // -----------------------------

    winrt::Windows::ApplicationModel::Core::IFrameworkView CreateView();

    // IFrameworkView methods.
    // -----------------------

    // The first method called when the IFrameworkView is being created.
    virtual void Initialize(const winrt::Windows::ApplicationModel::Core::CoreApplicationView& applicationView);

    // Called when the CoreWindow object is created (or re-created).
    virtual void SetWindow(const winrt::Windows::UI::Core::CoreWindow& window);

    // Initializes scene resources, or loads a previously saved app state.
    virtual void Load(const winrt::hstring& entryPoint);

    // This method is called after the window becomes active.
    virtual void Run();

    // Required for IFrameworkView.
    // Terminate events do not cause Uninitialize to be called. It will be called if your IFrameworkView
    // class is torn down while the app is in the foreground.
    virtual void Uninitialize();

private:
    void OnViewActivated(
        winrt::Windows::ApplicationModel::Core::CoreApplicationView const& sender,
        winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs const& args);
    void OnWindowClosed(const winrt::Windows::UI::Core::CoreWindow& window, const winrt::Windows::UI::Core::CoreWindowEventArgs& args);
    void OnWindowVisibilityChanged(
        const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Core::VisibilityChangedEventArgs& args);

    std::shared_ptr<IRemoteAppHolographic> m_app;
    std::unique_ptr<RemoteWindowHolographicUwp> m_window;

    winrt::Windows::UI::Core::CoreWindow m_coreWindow = nullptr;
    winrt::Windows::UI::Core::CoreWindow::Closed_revoker m_onWindowClosedRevoker;
    winrt::Windows::UI::Core::CoreWindow::VisibilityChanged_revoker m_onWindowVisibilityChangedRevoker;

    bool m_windowClosed = false;
    bool m_windowVisible = false;
};
