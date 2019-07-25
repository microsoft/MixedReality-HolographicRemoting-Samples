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

#include "..\pch.h"

#include <winrt/Windows.Perception.Spatial.Preview.h>

#include "QRCodeTracker.h"

#include <set>


QRCode::QRCode(
    const GUID& id,
    PSPATIAL_GRAPH_QR_CODE_STREAM_INFO streamInfo,
    const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& coordinateSystem)
    : m_id(id)
    , m_streamInfo(streamInfo)
    , m_coordinateSystem(coordinateSystem)
{
}

QRCode::~QRCode()
{
    if (m_streamInfo)
    {
        CoTaskMemFree(m_streamInfo);
    }
}

const GUID& QRCode::GetId() const
{
    return m_id;
}

float QRCode::GetPhysicalSize() const
{
    return m_physicalSizeInMeters;
}

winrt::Windows::Perception::Spatial::SpatialCoordinateSystem QRCode::GetCoordinateSystem() const
{
    return m_coordinateSystem;
}



QRCodeTracker::QRCodeTracker(IPerceptionDevice* device, const GUID& typeId, const GUID& objectId)
    : PerceptionRootObject(device, typeId, objectId)
{
    Start();
}

QRCodeTracker::~QRCodeTracker()
{
    Stop();
}

const GUID& QRCodeTracker::GetStaticPropertyId()
{
    return SPATIALPROP_QRTrackerObjectId;
}

void QRCodeTracker::Start()
{
    std::lock_guard stateLock(m_stateProtect);

    if (m_running)
    {
        return;
    }

    HRESULT hr = m_device->CreateObjectSubscription(m_objectId, UINT(1), m_qrTrackerSubscription.put());
    if (FAILED(hr))
    {
        Stop();
        return;
    }

    hr = m_device->CreatePropertyListener(GetObjectId(), SPATIALPROP_QRTracker_QRCodesList, m_qrListChangeListener.put());
    if (FAILED(hr))
    {
        Stop();
        return;
    }

    m_propertyChangeHandler = winrt::make_self<PropertyChangeHandler>(*this);
    hr = m_qrListChangeListener->SetPropertyChangedHandler(m_propertyChangeHandler.get());
    if (FAILED(hr))
    {
        Stop();
        return;
    }

    hr = m_qrListChangeListener->Start();
    if (FAILED(hr))
    {
        Stop();
        return;
    }

    m_running = true;
}

void QRCodeTracker::Stop()
{
    std::lock_guard stateLock(m_stateProtect);

    m_running = false;

    if (m_qrListChangeListener)
    {
        m_qrListChangeListener->Stop();
    }

    for (auto& qrByPointer : m_qrCodesByPointer)
    {
        QRCode* qrCode = qrByPointer.second.get();
        if (qrCode->m_propertyChangedListener)
        {
            qrCode->m_propertyChangedListener->Stop();
            qrCode->m_propertyChangedListener = nullptr;
        }
    }

    if (m_propertyChangeHandler)
    {
        m_propertyChangeHandler->Dispose();
        m_propertyChangeHandler = nullptr;
    }
}

HRESULT QRCodeTracker::HandlePropertyChange(IPerceptionDevicePropertyListener* sender, IPerceptionDevicePropertyChangedEventArgs* args)
{
    // Change event for QR code list?
    if (sender == m_qrListChangeListener.get())
    {
        const GUID* guids = static_cast<const GUID*>(args->GetValue());
        UINT numGuids = args->GetValueSize() / sizeof(GUID);
        return HandleQRCodeListChange(guids, numGuids);
    }

    // Change event for single QR code?
    {
        std::lock_guard<std::recursive_mutex> stateLock(m_stateProtect);
        auto byListenerPos = m_qrCodesByListener.find(sender);
        if (byListenerPos != m_qrCodesByListener.end())
        {
            QRCode* qrCode = byListenerPos->second;
            return UpdateQRCode(*qrCode);
        }
    }

    return S_OK;
}

HRESULT QRCodeTracker::HandleQRCodeListChange(const GUID* guids, UINT numGuids)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_stateProtect);

    if (!m_running)
    {
        return S_FALSE;
    }

    // Duplicate the list of known QR code IDs. We'll remove all entries from it that we see in
    // the incoming list, and thus will end up with a list of the IDs of all removed QR codes.
    std::set<GUID, GUIDComparer> codesNotInList;
    for (auto& kv : m_qrCodesByGUID)
    {
        codesNotInList.insert(kv.first);
    }

    // Check each QR code on the incoming list, and update the local cache
    // with new codes.
    for (size_t qrIndex = 0; qrIndex < numGuids; ++qrIndex)
    {
        const GUID& qrCodeId = guids[qrIndex];
        auto it = m_qrCodesByGUID.find(qrCodeId);
        if (it != m_qrCodesByGUID.end())
        {
            // Code is already known.
            codesNotInList.erase(qrCodeId);
            continue;
        }

        // Code is new. Read initial state, and add to collections.
        winrt::Windows::Perception::Spatial::SpatialCoordinateSystem coordinateSystem{nullptr};
        try
        {
            coordinateSystem =
                winrt::Windows::Perception::Spatial::Preview::SpatialGraphInteropPreview::CreateCoordinateSystemForNode(qrCodeId);
        }
        catch (winrt::hresult_error const& ex)
        {
            return ex.to_abi();
        }

        if (coordinateSystem == nullptr)
        {
            return E_FAIL;
        }

        void* streamData{nullptr};
        UINT streamDataSize{0};
        HRESULT hr = m_device->ReadVariableSizeProperty(qrCodeId, SPATIALPROP_QRCode_StreamInfo, &streamDataSize, &streamData, nullptr);
        if (FAILED(hr))
        {
            return hr;
        }

        if (streamDataSize == 0)
        {
            CoTaskMemFree(streamData);
            return E_FAIL;
        }

        auto newCode =
            std::make_unique<QRCode>(qrCodeId, reinterpret_cast<PSPATIAL_GRAPH_QR_CODE_STREAM_INFO>(streamData), coordinateSystem);
        QRCode* qrCode = newCode.get();

        m_qrCodesByPointer.emplace(qrCode, std::move(newCode));
        m_qrCodesByGUID.emplace(qrCodeId, qrCode);

        hr = UpdateQRCode(*qrCode);
        if (FAILED(hr))
        {
            return hr;
        }

        hr = m_device->CreatePropertyListener(qrCodeId, SPATIALPROP_QRCode_LastSeenTime, qrCode->m_propertyChangedListener.put());
        if (FAILED(hr))
        {
            return hr;
        }

        if (!m_propertyChangeHandler)
        {
            return E_UNEXPECTED;
        }

        hr = qrCode->m_propertyChangedListener->SetPropertyChangedHandler(
            m_propertyChangeHandler.as<IPerceptionDevicePropertyChangedHandler>().get());
        if (FAILED(hr))
        {
            return hr;
        }

        hr = qrCode->m_propertyChangedListener->Start();
        if (FAILED(hr))
        {
            return hr;
        }

        m_qrCodesByListener.emplace(qrCode->m_propertyChangedListener.get(), qrCode);
    }

    // Remove all QR codes that have not been seen in this update
    for (auto& qrCodeId : codesNotInList)
    {
        auto byCodeIdPos = m_qrCodesByGUID.find(qrCodeId);
        if (byCodeIdPos == m_qrCodesByGUID.end())
        {
            // Not found (this should not ever happen)
            continue;
        }

        QRCode* qrCode = byCodeIdPos->second;
        m_qrCodesByGUID.erase(byCodeIdPos);

        if (qrCode->m_propertyChangedListener)
        {
            qrCode->m_propertyChangedListener->Stop();
            qrCode->m_propertyChangedListener = nullptr;

            m_qrCodesByListener.erase(qrCode->m_propertyChangedListener.get());
        }

        m_qrCodesByPointer.erase(qrCode);
    }

    return S_OK;
}

HRESULT QRCodeTracker::UpdateQRCode(QRCode& qrCode)
{
    float physicalSizeInMeters{0};
    HRESULT hr =
        m_device->ReadProperty(qrCode.m_id, SPATIALPROP_QRCode_PhysicalSize, sizeof(physicalSizeInMeters), &physicalSizeInMeters, nullptr);
    if (FAILED(hr))
    {
        return hr;
    }
    qrCode.m_physicalSizeInMeters = physicalSizeInMeters;

    LONGLONG lastSeenTime{0};
    hr = m_device->ReadProperty(qrCode.m_id, SPATIALPROP_QRCode_LastSeenTime, sizeof(lastSeenTime), &lastSeenTime, nullptr);
    if (FAILED(hr))
    {
        return hr;
    }
    qrCode.m_lastSeenTime = lastSeenTime;

    return S_OK;
}



QRCodeTracker::PropertyChangeHandler::PropertyChangeHandler(QRCodeTracker& owner)
    : m_owner(&owner)
{
}

void QRCodeTracker::PropertyChangeHandler::Dispose()
{
    m_owner = nullptr;
}

STDMETHODIMP QRCodeTracker::PropertyChangeHandler::Invoke(
    _In_ IPerceptionDevicePropertyListener* sender, _In_ IPerceptionDevicePropertyChangedEventArgs* eventArgs)
{
    auto owner = m_owner;
    if (owner)
    {
        return owner->HandlePropertyChange(sender, eventArgs);
    }

    return S_OK;
}
