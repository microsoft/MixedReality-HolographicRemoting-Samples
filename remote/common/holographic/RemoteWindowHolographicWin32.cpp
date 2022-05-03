#include <pch.h>

#include <holographic/IRemoteAppHolographic.h>
#include <holographic/RemoteWindowHolographicWin32.h>

#include <HolographicSpaceInterop.h>
#include <SpatialInteractionManagerInterop.h>
#include <windows.graphics.holographic.h>
#include <windows.ui.input.spatial.h>

namespace
{
    constexpr const LONG windowInitialWidth = 1280;
    constexpr const LONG windowInitialHeight = 720;
    constexpr const wchar_t* windowInitialTitle = L"Remote";
    constexpr const wchar_t* windowClassName = L"RemoteWindowHolographicWin32Class";
} // namespace

RemoteWindowHolographicWin32::RemoteWindowHolographicWin32(const std::shared_ptr<IRemoteAppHolographic>& app)
    : RemoteWindowHolographic(app)
{
}

winrt::com_ptr<IDXGISwapChain1> RemoteWindowHolographicWin32::CreateSwapChain(
    const winrt::com_ptr<ID3D11Device1>& device, const DXGI_SWAP_CHAIN_DESC1* desc)
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

winrt::Windows::Graphics::Holographic::HolographicSpace RemoteWindowHolographicWin32::CreateHolographicSpace()
{
    using namespace winrt::Windows::Graphics::Holographic;

    // Use WinRT factory to create the holographic space.
    // See https://docs.microsoft.com/en-us/windows/win32/api/holographicspaceinterop/
    winrt::com_ptr<IHolographicSpaceInterop> holographicSpaceInterop =
        winrt::get_activation_factory<HolographicSpace, IHolographicSpaceInterop>();

    winrt::com_ptr<ABI::Windows::Graphics::Holographic::IHolographicSpace> spHolographicSpace;
    winrt::check_hresult(holographicSpaceInterop->CreateForWindow(
        m_hWnd, __uuidof(ABI::Windows::Graphics::Holographic::IHolographicSpace), winrt::put_abi(spHolographicSpace)));

    return spHolographicSpace.as<HolographicSpace>();
}

winrt::Windows::UI::Input::Spatial::SpatialInteractionManager RemoteWindowHolographicWin32::CreateInteractionManager()
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

void RemoteWindowHolographicWin32::SetWindowTitle(std::wstring title)
{
    if (m_hWnd)
    {
        SetWindowTextW(m_hWnd, title.c_str());
    }
}

void RemoteWindowHolographicWin32::InitializeHwnd(HWND hWnd)
{
    m_hWnd = hWnd;

    m_app->SetWindow(this);
}

void RemoteWindowHolographicWin32::DeinitializeHwnd()
{
    m_app->SetWindow(nullptr);

    m_hWnd = 0;
}

void RemoteWindowHolographicWin32::OnKeyPress(char key)
{
    m_app->OnKeyPress(key);
}

void RemoteWindowHolographicWin32::OnResize(int width, int height)
{
    m_app->OnResize(width, height);
}

LRESULT CALLBACK wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static RemoteWindowHolographicWin32* s_sampleHostWindow = nullptr;

    LRESULT result = 0;

    switch (msg)
    {
        case WM_CREATE:
        {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            s_sampleHostWindow = reinterpret_cast<RemoteWindowHolographicWin32*>(cs->lpCreateParams);

            RECT clientRect;
            GetClientRect(hWnd, &clientRect);

            s_sampleHostWindow->OnResize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
            result = 0;
        }
        break;

        case WM_CHAR:
        {
            const int key = static_cast<int>(wParam);
            s_sampleHostWindow->OnKeyPress(static_cast<char>(key));
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

        case WM_CLOSE:
        {
            DestroyWindow(hWnd);
            result = 0;
        }
        break;

        case WM_DESTROY:
        {
            s_sampleHostWindow->DeinitializeHwnd();
            s_sampleHostWindow = nullptr;
            result = 0;
            PostQuitMessage(0);
        }
        break;

        default:
        {
            result = DefWindowProc(hWnd, msg, wParam, lParam);
        }
        break;
    }

    return result;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    winrt::init_apartment();

    std::shared_ptr<IRemoteAppHolographic> app = CreateRemoteAppHolographic();
    if (!app)
    {
        return 1;
    }

    RemoteWindowHolographicWin32 window = RemoteWindowHolographicWin32(app);

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = wndProc;
    wcex.hInstance = 0;
    wcex.hIcon = LoadIcon(0, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wcex.lpszClassName = windowClassName;
    RegisterClassExW(&wcex);

    RECT rc = {0, 0, windowInitialWidth, windowInitialHeight};
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);

    HWND hWnd = CreateWindowW(
        windowClassName,
        windowInitialTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        0,
        &window);

    window.InitializeHwnd(hWnd);

    app->ParseLaunchArguments(lpCmdLine);

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
                app->Tick();
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
