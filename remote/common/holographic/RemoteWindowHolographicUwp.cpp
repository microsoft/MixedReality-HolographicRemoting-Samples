#include <pch.h>

#include <holographic/RemoteWindowHolographicUWP.h>

#include <holographic/IRemoteAppHolographic.h>

#include <dxgi1_4.h>

#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.ViewManagement.h>

#include <sstream>

using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Input::Spatial;
using namespace winrt::Windows::UI::ViewManagement;

namespace
{
    constexpr const LONG windowInitialWidth = 1280;
    constexpr const LONG windowInitialHeight = 720;
} // namespace

RemoteWindowHolographicUwp::RemoteWindowHolographicUwp(const std::shared_ptr<IRemoteAppHolographic>& app, const CoreWindow& coreWindow)
    : RemoteWindowHolographic(app)
    , m_coreWindow(coreWindow)
{
    m_onKeyDownRevoker = m_coreWindow.KeyDown(winrt::auto_revoke, {this, &RemoteWindowHolographicUwp::OnKeyDown});
    m_onSizeChangedRevoker = m_coreWindow.SizeChanged(winrt::auto_revoke, {this, &RemoteWindowHolographicUwp::OnSizeChanged});
}

winrt::com_ptr<IDXGISwapChain1>
    RemoteWindowHolographicUwp::CreateSwapChain(const winrt::com_ptr<ID3D11Device1>& device, const DXGI_SWAP_CHAIN_DESC1* desc)
{
    winrt::com_ptr<IDXGIDevice3> dxgiDevice;
    device.as(dxgiDevice);

    winrt::com_ptr<IDXGIAdapter> dxgiAdapter;
    winrt::check_hresult(dxgiDevice->GetAdapter(dxgiAdapter.put()));

    winrt::com_ptr<IDXGIFactory4> dxgiFactory;
    winrt::check_hresult(dxgiAdapter->GetParent(__uuidof(dxgiFactory), dxgiFactory.put_void()));

    winrt::com_ptr<IDXGISwapChain1> swapChain;
    winrt::check_hresult(dxgiFactory->CreateSwapChainForCoreWindow(
        device.get(), static_cast<::IUnknown*>(winrt::get_abi(m_coreWindow)), desc, nullptr, swapChain.put()));

    return swapChain;
}

HolographicSpace RemoteWindowHolographicUwp::CreateHolographicSpace()
{
    return HolographicSpace::CreateForCoreWindow(m_coreWindow);
}

SpatialInteractionManager RemoteWindowHolographicUwp::CreateInteractionManager()
{
    return SpatialInteractionManager::GetForCurrentView();
}

void RemoteWindowHolographicUwp::SetWindowTitle(std::wstring title)
{
    auto dispatcher = m_coreWindow.Dispatcher();

    auto doSetWindowTitle = [title]() {
        try
        {
            if (auto view = ApplicationView::GetForCurrentView())
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

void RemoteWindowHolographicUwp::OnKeyDown(const CoreWindow& window, const KeyEventArgs& args)
{
    int32_t key = static_cast<int32_t>(args.VirtualKey());
    if (key >= 0 && key <= 0xFF)
    {
        if (m_app)
        {
            m_app->OnKeyPress(static_cast<char>(tolower(key)));
        }
    }
}

void RemoteWindowHolographicUwp::OnSizeChanged(
    const winrt::Windows::Foundation::IInspectable& sender, const WindowSizeChangedEventArgs& args)
{
    winrt::Windows::Foundation::Size size = args.Size();

    if (m_app)
    {
        m_app->OnResize(static_cast<int>(size.Width + 0.5f), static_cast<int>(size.Height + 0.5f));
    }
}

RemoteWindowHolographicUwpView::RemoteWindowHolographicUwpView()
{
    ApplicationView::PreferredLaunchViewSize(winrt::Windows::Foundation::Size(windowInitialWidth, windowInitialHeight));
}

IFrameworkView RemoteWindowHolographicUwpView::CreateView()
{
    return *this;
}

void RemoteWindowHolographicUwpView::Initialize(const CoreApplicationView& applicationView)
{
    applicationView.Activated({this, &RemoteWindowHolographicUwpView::OnViewActivated});

    m_app = CreateRemoteAppHolographic();
}

void RemoteWindowHolographicUwpView::SetWindow(const CoreWindow& coreWindow)
{
    m_coreWindow = coreWindow;
    if (m_app)
    {
        if (m_coreWindow)
        {
            m_window = std::make_unique<RemoteWindowHolographicUwp>(m_app, m_coreWindow);
            m_app->SetWindow(m_window.get());

            m_onWindowClosedRevoker = m_coreWindow.Closed(winrt::auto_revoke, {this, &RemoteWindowHolographicUwpView::OnWindowClosed});
            m_onWindowVisibilityChangedRevoker =
                m_coreWindow.VisibilityChanged(winrt::auto_revoke, {this, &RemoteWindowHolographicUwpView::OnWindowVisibilityChanged});
        }
        else
        {
            m_onWindowVisibilityChangedRevoker.revoke();
            m_onWindowClosedRevoker.revoke();

            m_app->SetWindow(nullptr);
            m_window = nullptr;
        }
    }
}

void RemoteWindowHolographicUwpView::Load(const winrt::hstring& entryPoint)
{
}

void RemoteWindowHolographicUwpView::Run()
{
    while (!m_windowClosed)
    {
        if (m_windowVisible)
        {
            CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

            if (m_app)
            {
                m_app->Tick();
            }
        }
        else
        {
            CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
        }
    }
}

void RemoteWindowHolographicUwpView::Uninitialize()
{
}

void RemoteWindowHolographicUwpView::OnViewActivated(CoreApplicationView const& sender, IActivatedEventArgs const& activationArgs)
{
    if (activationArgs != nullptr)
    {
        ActivationKind activationKind = activationArgs.Kind();
        if (activationKind == ActivationKind::Launch)
        {
            LaunchActivatedEventArgs launchArgs = activationArgs.as<LaunchActivatedEventArgs>();

            if (m_app)
            {
                m_app->ParseLaunchArguments(launchArgs.Arguments());
            }
        }
    }

    // Run() won't start until the CoreWindow is activated.
    sender.CoreWindow().Activate();
}

void RemoteWindowHolographicUwpView::OnWindowClosed(const CoreWindow& window, const CoreWindowEventArgs& args)
{
    m_windowClosed = true;
}

void RemoteWindowHolographicUwpView::OnWindowVisibilityChanged(
    const winrt::Windows::Foundation::IInspectable& sender, const VisibilityChangedEventArgs& args)
{
    m_windowVisible = args.Visible();
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment();
    CoreApplication::Run(winrt::make<RemoteWindowHolographicUwpView>());
}
