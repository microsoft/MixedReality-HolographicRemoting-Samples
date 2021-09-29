#pragma once

#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/windows.Ui.Input.Spatial.h>

#include <d3d11_1.h>
#include <dxgi1_2.h>

#include <memory>
#include <string>

class IRemoteAppHolographic;

// Abstract window base class for Holographic App Remoting remote side applications.
// Provides an abstraction layer for Win32 and UWP windows.
class RemoteWindowHolographic
{
public:
    // RemoteWindowHolographic constructor.
    // Needs a shared_ptr to an IRemoteAppHolographic, to notify the app about window events, e.g. key press or resize events.
    RemoteWindowHolographic(const std::shared_ptr<IRemoteAppHolographic>& app);

    // Create a SwapChain for this window
    virtual winrt::com_ptr<IDXGISwapChain1> CreateSwapChain(
        const winrt::com_ptr<ID3D11Device1>& device, const DXGI_SWAP_CHAIN_DESC1* desc) = 0;

    // Create a HolographicSpace for this window
    virtual winrt::Windows::Graphics::Holographic::HolographicSpace CreateHolographicSpace() = 0;

    // Create a SpatialInteractionManager for this window
    virtual winrt::Windows::UI::Input::Spatial::SpatialInteractionManager CreateInteractionManager() = 0;

    // Sets the window title
    virtual void SetWindowTitle(std::wstring title) = 0;

protected:
    std::shared_ptr<IRemoteAppHolographic> m_app = nullptr;
};
