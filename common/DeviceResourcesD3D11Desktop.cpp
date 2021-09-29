#include <pch.h>

#include <DeviceResourcesD3D11Desktop.h>

#include <d3d11_4.h>
#include <dxgi1_4.h>

using namespace DXHelper;

DeviceResourcesD3D11Desktop::DeviceResourcesD3D11Desktop() = default;
DeviceResourcesD3D11Desktop::~DeviceResourcesD3D11Desktop() = default;

void DeviceResourcesD3D11Desktop::HandleDeviceLost()
{
    NotifyDeviceLost();

    CreateDeviceResources();

    NotifyDeviceRestored();
}

void DXHelper::DeviceResourcesD3D11Desktop::SetWindow(HWND hWnd)
{
    m_hWnd = hWnd;
    CreateDeviceResources();
}

void DXHelper::DeviceResourcesD3D11Desktop::Present()
{
    m_swapChain->Present(1, 0);
}

ID3D11RenderTargetView* DXHelper::DeviceResourcesD3D11Desktop::GetBackbufferRTV() const
{
    return m_backbufferRTV.get();
}

ID3D11Texture2D* DXHelper::DeviceResourcesD3D11Desktop::GetBackbuffer() const
{
    return m_backbuffer.get();
}

ID3D11DepthStencilView* DXHelper::DeviceResourcesD3D11Desktop::GetDepthView() const
{
    return m_depthView.get();
}

ID3D11Texture2D* DXHelper::DeviceResourcesD3D11Desktop::GetDepthBuffer() const
{
    return m_depthBuffer.get();
}

ID3D11RenderTargetView* DXHelper::DeviceResourcesD3D11Desktop::GetProxyBackbufferRTV() const
{
    return m_proxyBackbufferRTV.get();
}

ID3D11Texture2D* DXHelper::DeviceResourcesD3D11Desktop::GetProxyBackbuffer() const
{
    return m_proxyBackbuffer.get();
}

void DXHelper::DeviceResourcesD3D11Desktop::CreateDeviceResources()
{
    DeviceResourcesD3D11::CreateDeviceResources();

    RECT rc;
    GetClientRect(m_hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    winrt::com_ptr<IDXGIFactory2> dxgiFactory;
    HRESULT result = m_dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.put()));
    if (FAILED(result))
    {
        throw winrt::hresult_error(result, L"Failed to get dxgi factory");
    }

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = width;
    sd.Height = height;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    result = dxgiFactory->CreateSwapChainForHwnd(m_d3dDevice.get(), m_hWnd, &sd, nullptr, nullptr, m_swapChain.put());
    if (FAILED(result))
    {
        throw winrt::hresult_error(result, L"Failed to create swapchain");
    }

    // Create a render target view
    result = m_swapChain->GetBuffer(0, IID_PPV_ARGS(m_backbuffer.put()));
    if (FAILED(result))
    {
        throw winrt::hresult_error(result, L"Failed to get backbuffer");
    }

    result = m_d3dDevice->CreateRenderTargetView(m_backbuffer.get(), nullptr, m_backbufferRTV.put());
    if (FAILED(result))
    {
        throw winrt::hresult_error(result, L"Failed to get backbuffer RTV");
    }

    // Create depth stencil texture
    {
        D3D11_TEXTURE2D_DESC descDepth = {};
        descDepth.Width = width;
        descDepth.Height = height;
        descDepth.MipLevels = 1;
        descDepth.ArraySize = 1;
        descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        descDepth.SampleDesc.Count = 1;
        descDepth.SampleDesc.Quality = 0;
        descDepth.Usage = D3D11_USAGE_DEFAULT;
        descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        descDepth.CPUAccessFlags = 0;
        descDepth.MiscFlags = 0;
        result = m_d3dDevice->CreateTexture2D(&descDepth, nullptr, m_depthBuffer.put());
        if (FAILED(result))
        {
            throw winrt::hresult_error(result, L"Failed to create depth buffer");
        }

        // Create the depth stencil view
        D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
        descDSV.Format = descDepth.Format;
        descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        descDSV.Texture2D.MipSlice = 0;
        result = m_d3dDevice->CreateDepthStencilView(m_depthBuffer.get(), &descDSV, m_depthView.put());
        if (FAILED(result))
        {
            throw winrt::hresult_error(result, L"Failed to create depth stencil view");
        }
    }

    // Create the proxy backbuffer
    {
        D3D11_TEXTURE2D_DESC desc = {};
        m_backbuffer->GetDesc(&desc);
        desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

        m_d3dDevice->CreateTexture2D(&desc, nullptr, m_proxyBackbuffer.put());

        // Create the proxy backbuffer RTV
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};

        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Format = desc.Format;

        m_d3dDevice->CreateRenderTargetView(m_proxyBackbuffer.get(), &rtvDesc, m_proxyBackbufferRTV.put());
    }

    // Setup the viewport
    {
        D3D11_VIEWPORT vp;
        vp.Width = (FLOAT)width;
        vp.Height = (FLOAT)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        m_d3dContext->RSSetViewports(1, &vp);
    }
}
