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

#include "../Common/PerceptionTypes.h"
#include "../Common/Utils.h"
#include "PerceptionDeviceHandler.h"

// Represents a single tracked QR code with position, size and last seen time.
class QRCode
{
public:
    QRCode(
        const GUID& id,
        PSPATIAL_GRAPH_QR_CODE_STREAM_INFO streamInfo,
        const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& coordinateSystem);
    ~QRCode();

    const GUID& GetId() const;
    float GetPhysicalSize() const;
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem GetCoordinateSystem() const;

private:
    friend class QRCodeTracker;

private:
    GUID m_id;
    PSPATIAL_GRAPH_QR_CODE_STREAM_INFO m_streamInfo;

    __int64 m_lastSeenTime{0};
    float m_physicalSizeInMeters{0};
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_coordinateSystem{nullptr};
    winrt::com_ptr<IPerceptionDevicePropertyListener> m_propertyChangedListener;
};

// Manages all active QR codes.
// Listens for events from the perception device to add, remove or update QR codes.
class QRCodeTracker : public PerceptionRootObject
{
public:
    QRCodeTracker(IPerceptionDevice* device, const GUID& typeId, const GUID& objectId);
    ~QRCodeTracker();

    // Helper function to iterate all QR codes in a thread safe manner.
    template <typename Func>
    void ForEachQRCode(Func& func)
    {
        std::lock_guard stateLock(m_stateProtect);
        for (auto& qrCodeEntry : m_qrCodesByPointer)
        {
            func(*qrCodeEntry.second.get());
        }
    }

public:
    static const GUID& GetStaticPropertyId();

private:
    // Implementation of the IPerceptionDevicePropertyChangedHandler interface.
    // Events from the perception device are propagated through an instance of this class.
    struct PropertyChangeHandler : winrt::implements<PropertyChangeHandler, IPerceptionDevicePropertyChangedHandler>
    {
        PropertyChangeHandler(QRCodeTracker& owner);

        void Dispose();

        STDMETHOD(Invoke)
        (_In_ IPerceptionDevicePropertyListener* sender, _In_ IPerceptionDevicePropertyChangedEventArgs* eventArgs) override;

    private:
        QRCodeTracker* m_owner;
    };
    friend PropertyChangeHandler;

    using QRCodesByPointerMap = std::map<const QRCode*, std::unique_ptr<QRCode>>;
    using QRCodesByGUIDMap = std::map<GUID, QRCode*, GUIDComparer>;
    using QRCodesByListenerMap = std::map<IPerceptionDevicePropertyListener*, QRCode*>;

private:
    void Start();
    void Stop();

    HRESULT HandlePropertyChange(IPerceptionDevicePropertyListener* sender, IPerceptionDevicePropertyChangedEventArgs* args);
    HRESULT HandleQRCodeListChange(const GUID* guids, UINT numGuids);
    HRESULT UpdateQRCode(QRCode& qrCode);

private:
    std::recursive_mutex m_stateProtect;

    bool m_running{false};

    winrt::com_ptr<IPerceptionDeviceObjectSubscription> m_qrTrackerSubscription;
    winrt::com_ptr<IPerceptionDevicePropertyListener> m_qrListChangeListener;
    winrt::com_ptr<PropertyChangeHandler> m_propertyChangeHandler;

    QRCodesByPointerMap m_qrCodesByPointer;
    QRCodesByGUIDMap m_qrCodesByGUID;
    QRCodesByListenerMap m_qrCodesByListener;
};
