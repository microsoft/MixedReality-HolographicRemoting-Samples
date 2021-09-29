#pragma once

#include <CameraResourcesD3D11Holographic.h>
#include <DeviceResourcesD3D11.h>

#include <winrt/Windows.Graphics.Holographic.h>

namespace DXHelper
{
    class DeviceResourcesD3D11Holographic : public DeviceResourcesD3D11
    {
    public:
        DeviceResourcesD3D11Holographic();
        ~DeviceResourcesD3D11Holographic();

        void HandleDeviceLost();
        void Present(winrt::Windows::Graphics::Holographic::HolographicFrame frame);

        enum class WaitResult
        {
            Success,
            Failure,
            DeviceLost
        };
        WaitResult WaitForNextFrameReady();

        void SetHolographicSpace(winrt::Windows::Graphics::Holographic::HolographicSpace holographicSpace);

        void EnsureCameraResources(
            winrt::Windows::Graphics::Holographic::HolographicFrame frame,
            winrt::Windows::Graphics::Holographic::HolographicFramePrediction prediction,
            winrt::Windows::Perception::Spatial::SpatialCoordinateSystem focusPointCoordinateSystem,
            winrt::Windows::Foundation::Numerics::float3 focusPointPosition);

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

        void OnIsAvailableChanged(
            const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& args);

        void UnregisterHolographicEventHandlers();

        // Direct3D interop objects.
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_d3dInteropDevice;

        // The holographic space provides a preferred DXGI adapter ID.
        winrt::Windows::Graphics::Holographic::HolographicSpace m_holographicSpace = nullptr;

        bool m_useLegacyWaitBehavior = false;
        bool m_nextPresentMustWait = false;
        bool m_firstFramePresented = false;

        // Back buffer resources, etc. for attached holographic cameras.
        std::map<UINT32, std::unique_ptr<CameraResourcesD3D11Holographic>> m_cameraResources;
        std::mutex m_cameraResourcesLock;

        winrt::event_token m_cameraAddedToken;
        winrt::event_token m_cameraRemovedToken;
        winrt::Windows::Graphics::Holographic::HolographicSpace::IsAvailableChanged_revoker m_isAvailableChangedRevoker;
    };
} // namespace DXHelper

// Device-based resources for holographic cameras are stored in a std::map. Access this list by providing a
// callback to this function, and the std::map will be guarded from add and remove
// events until the callback returns. The callback is processed immediately and must
// not contain any nested calls to UseHolographicCameraResources.
// The callback takes a parameter of type std::map<UINT32, std::unique_ptr<CameraResources>>&
// through which the list of cameras will be accessed.
template <typename LCallback>
void DXHelper::DeviceResourcesD3D11Holographic::UseHolographicCameraResources(LCallback const& callback)
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
                throw;
        }
    }
}
