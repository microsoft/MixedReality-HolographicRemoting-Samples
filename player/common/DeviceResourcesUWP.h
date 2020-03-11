#pragma once

#include "DeviceResourcesCommon.h"

#include <winrt/Windows.Graphics.Holographic.h>

namespace DXHelper
{
    class CameraResources;

    class DeviceResourcesUWP : public DeviceResourcesCommon
    {
    public:
        ~DeviceResourcesUWP();

        void HandleDeviceLost();
        void Present(winrt::Windows::Graphics::Holographic::HolographicFrame frame);

        void SetWindow(const winrt::Windows::UI::Core::CoreWindow& window);

        void EnsureCameraResources(
            winrt::Windows::Graphics::Holographic::HolographicFrame frame,
            winrt::Windows::Graphics::Holographic::HolographicFramePrediction prediction);

        // Holographic accessors.
        template <typename LCallback>
        void UseHolographicCameraResources(LCallback const& callback);

        // Holographic accessors.
        winrt::Windows::Graphics::Holographic::HolographicSpace GetHolographicSpace()
        {
            return m_holographicSpace;
        }

        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice GetD3DInteropDevice() const
        {
            return m_d3dInteropDevice;
        }

    protected:
        virtual void CreateDeviceResources() override;

    private:
        void InitializeUsingHolographicSpace();
        void OnCameraAdded(
            winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
            winrt::Windows::Graphics::Holographic::HolographicSpaceCameraAddedEventArgs const& args);

        void OnCameraRemoved(
            winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
            winrt::Windows::Graphics::Holographic::HolographicSpaceCameraRemovedEventArgs const& args);

        void UnregisterHolographicEventHandlers();

        // Direct3D interop objects.
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_d3dInteropDevice;

        // The holographic space provides a preferred DXGI adapter ID.
        winrt::Windows::Graphics::Holographic::HolographicSpace m_holographicSpace = nullptr;

        bool m_useLegacyWaitBehavior = false;

        // Back buffer resources, etc. for attached holographic cameras.
        std::map<UINT32, std::unique_ptr<CameraResources>> m_cameraResources;
        std::mutex m_cameraResourcesLock;

        winrt::event_token m_cameraAddedToken;
        winrt::event_token m_cameraRemovedToken;
    };
} // namespace DXHelper

// Device-based resources for holographic cameras are stored in a std::map. Access this list by providing a
// callback to this function, and the std::map will be guarded from add and remove
// events until the callback returns. The callback is processed immediately and must
// not contain any nested calls to UseHolographicCameraResources.
// The callback takes a parameter of type std::map<UINT32, std::unique_ptr<CameraResources>>&
// through which the list of cameras will be accessed.
template <typename LCallback>
void DXHelper::DeviceResourcesUWP::UseHolographicCameraResources(LCallback const& callback)
{
    try
    {
        std::lock_guard<std::mutex> guard(m_cameraResourcesLock);
        callback(m_cameraResources);
    }
    catch (const winrt::hresult_error& err)
    {
        switch (err.code())
        {
            case DXGI_ERROR_DEVICE_HUNG:
            case DXGI_ERROR_DEVICE_REMOVED:
            case DXGI_ERROR_DEVICE_RESET:
                HandleDeviceLost();
                break;

            default:
                throw err;
        }
    }
}
