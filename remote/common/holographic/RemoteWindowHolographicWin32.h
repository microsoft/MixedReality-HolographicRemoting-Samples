#pragma once

#include <holographic/RemoteWindowHolographic.h>

class RemoteWindowHolographicWin32 : public RemoteWindowHolographic
{
public:
    RemoteWindowHolographicWin32(const std::shared_ptr<IRemoteAppHolographic>& app);

    // IRemoteWindowHolographic methods

    virtual winrt::com_ptr<IDXGISwapChain1> CreateSwapChain(
        const winrt::com_ptr<ID3D11Device1>& device, const DXGI_SWAP_CHAIN_DESC1* desc) override;

    virtual winrt::Windows::Graphics::Holographic::HolographicSpace CreateHolographicSpace() override;

    virtual winrt::Windows::UI::Input::Spatial::SpatialInteractionManager CreateInteractionManager() override;

    virtual void SetWindowTitle(std::wstring title) override;

    // RemoteWindowHolographicWin32 methods

    void InitializeHwnd(HWND hWnd);

    void DeinitializeHwnd();

    void OnKeyPress(char key);

    void OnResize(int width, int height);

private:
    HWND m_hWnd = 0;
};
