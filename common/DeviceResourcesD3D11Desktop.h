#pragma once

#include <DeviceResourcesD3D11.h>

struct IDXGISwapChain1;

namespace DXHelper
{
    class DeviceResourcesD3D11Desktop : public DeviceResourcesD3D11
    {
    public:
        DeviceResourcesD3D11Desktop();
        virtual ~DeviceResourcesD3D11Desktop() override;

        void HandleDeviceLost();

        void SetWindow(HWND hWnd);

        void Present();

        ID3D11RenderTargetView* GetBackbufferRTV() const;
        ID3D11Texture2D* GetBackbuffer() const;

        ID3D11DepthStencilView* GetDepthView() const;
        ID3D11Texture2D* GetDepthBuffer() const;

        ID3D11RenderTargetView* GetProxyBackbufferRTV() const;
        ID3D11Texture2D* GetProxyBackbuffer() const;

    private:
        virtual void CreateDeviceResources() override;

        HWND m_hWnd = NULL;
        winrt::com_ptr<IDXGISwapChain1> m_swapChain;
        winrt::com_ptr<ID3D11Texture2D> m_backbuffer;
        winrt::com_ptr<ID3D11RenderTargetView> m_backbufferRTV;

        winrt::com_ptr<ID3D11Texture2D> m_depthBuffer;
        winrt::com_ptr<ID3D11DepthStencilView> m_depthView;

        winrt::com_ptr<ID3D11Texture2D> m_proxyBackbuffer;
        winrt::com_ptr<ID3D11RenderTargetView> m_proxyBackbufferRTV;
    };
} // namespace DXHelper
