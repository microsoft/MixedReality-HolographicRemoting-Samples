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

#include <SampleRemoteMain.h>

class SampleRemoteWindowWin32 : public std::enable_shared_from_this<SampleRemoteWindowWin32>, public SampleRemoteMain::IWindow
{
public:
    void Initialize();

    void InitializeHwnd(HWND hWnd);

    void ConfigureRemoting(const SampleRemoteMain::Options& options);
    void Connect();

    void InitializeStandalone();

    void Tick();

    void OnKeyPress(char key);
    void OnResize(int width, int height);

    virtual winrt::com_ptr<IDXGISwapChain1>
        CreateSwapChain(const winrt::com_ptr<ID3D11Device1>& device, const DXGI_SWAP_CHAIN_DESC1* desc) override;

    virtual winrt::Windows::Graphics::Holographic::HolographicSpace CreateHolographicSpace() override;

    virtual winrt::Windows::UI::Input::Spatial::SpatialInteractionManager CreateInteractionManager() override;

    virtual void SetWindowTitle(std::wstring title) override;

private:
    HWND m_hWnd = 0;

    std::shared_ptr<SampleRemoteMain> m_main;
};
