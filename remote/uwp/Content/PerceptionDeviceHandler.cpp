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

#include <pch.h>

#include <content/PerceptionDeviceHandler.h>

#include <Utils.h>
#include <content/PerceptionTypes.h>
#include <content/QRCodeTracker.h>

PerceptionRootObject::~PerceptionRootObject()
{
}

const GUID& PerceptionRootObject::GetPropertyId() const
{
    return m_typeId;
}

const GUID& PerceptionRootObject::GetObjectId() const
{
    return m_objectId;
}

PerceptionRootObject::PerceptionRootObject(IPerceptionDevice* device, const GUID& typeId, const GUID& objectId)
    : m_typeId(typeId)
    , m_objectId(objectId)
{
    m_device.copy_from(device);
}

PerceptionDeviceHandler::PerceptionDeviceHandler()
{
}

PerceptionDeviceHandler::~PerceptionDeviceHandler()
{
    Stop();
}

void PerceptionDeviceHandler::Start()
{
    std::lock_guard stateLock(m_stateProtect);

    if (m_running)
    {
        return;
    }

    HRESULT hr = PerceptionDeviceCreateFactory(IID_PPV_ARGS(m_perceptionDeviceFactory.put()));
    if (FAILED(hr))
    {
        Stop();
        return;
    }

    m_rootObjectChangeHandler = winrt::make_self<RootObjectChangeHandler>(*this);

    std::array<const GUID, 1> rootObjectIds{QRCodeTracker::GetStaticPropertyId()};
    for (size_t i = 0; i < rootObjectIds.size(); ++i)
    {
        winrt::com_ptr<IPerceptionDeviceRootObjectWatcher> watcher;
        hr = m_perceptionDeviceFactory->CreateRootObjectWatcher(1, &rootObjectIds[i], PerceptionDeviceOptions::None, watcher.put());
        if (FAILED(hr))
        {
            Stop();
            return;
        }

        hr = watcher->SetAddedHandler(m_rootObjectChangeHandler.get());
        if (FAILED(hr))
        {
            Stop();
            return;
        }

        hr = watcher->SetRemovedHandler(m_rootObjectChangeHandler.get());
        if (FAILED(hr))
        {
            Stop();
            return;
        }

        m_rootObjectWatchers.emplace_back(std::move(watcher));
    }

    m_running = true;

    for (auto& watcher : m_rootObjectWatchers)
    {
        hr = watcher->Start();
        if (FAILED(hr))
        {
            Stop();
            return;
        }
    }
}

void PerceptionDeviceHandler::Stop()
{
    std::lock_guard stateLock(m_stateProtect);

    m_running = false;

    for (auto& watcher : m_rootObjectWatchers)
    {
        watcher->Stop();
    }
    m_rootObjectWatchers.clear();
    m_rootObjectChangeHandler = nullptr;
    m_perceptionDeviceFactory = nullptr;
}

HRESULT PerceptionDeviceHandler::HandleRootObjectAdded(IPerceptionDeviceRootObjectAddedEventArgs* args)
{
    std::lock_guard stateLock(m_stateProtect);

    if (!m_running)
    {
        return S_OK;
    }

    RootObjectKey key{args->GetPropertyId(), args->GetObjectId()};
    if (m_rootObjects.find(key) != m_rootObjects.end())
    {
        return S_FALSE; // Already have that root object; don't add it twice
    }

    if (Utils::GUIDComparer::equals(key.propertyId, QRCodeTracker::GetStaticPropertyId()))
    {
        winrt::com_ptr<IPerceptionDevice> device;
        args->GetDevice(device.put());
        m_rootObjects.emplace(key, std::make_shared<QRCodeTracker>(device.get(), key.propertyId, key.objectId));
    }

    return S_OK;
}

HRESULT PerceptionDeviceHandler::HandleRootObjectRemoved(IPerceptionDeviceRootObjectRemovedEventArgs* args)
{
    std::lock_guard stateLock(m_stateProtect);

    if (!m_running)
    {
        return S_OK;
    }

    RootObjectKey key{args->GetPropertyId(), args->GetObjectId()};
    auto it = m_rootObjects.find(key);
    if (it != m_rootObjects.end())
    {
        m_rootObjects.erase(key);
    }

    return S_OK;
}

PerceptionDeviceHandler::RootObjectChangeHandler::RootObjectChangeHandler(PerceptionDeviceHandler& owner)
    : m_weakOwner(owner.weak_from_this())
{
}

STDMETHODIMP PerceptionDeviceHandler::RootObjectChangeHandler::Invoke(
    _In_ IPerceptionDeviceRootObjectWatcher* sender, _In_ IPerceptionDeviceRootObjectAddedEventArgs* args)
{
    auto owner{m_weakOwner.lock()};
    if (owner)
    {
        return owner->HandleRootObjectAdded(args);
    }

    return S_OK;
}

STDMETHODIMP PerceptionDeviceHandler::RootObjectChangeHandler::Invoke(
    _In_ IPerceptionDeviceRootObjectWatcher* sender, _In_ IPerceptionDeviceRootObjectRemovedEventArgs* args)
{
    auto owner{m_weakOwner.lock()};
    if (owner)
    {
        return owner->HandleRootObjectRemoved(args);
    }

    return S_OK;
}

bool PerceptionDeviceHandler::RootObjectKey::operator<(const RootObjectKey& other) const
{
    const auto typeIdRes = Utils::GUIDComparer::compare(propertyId, other.propertyId);
    if (typeIdRes < 0)
    {
        return true;
    }
    else if (typeIdRes > 0)
    {
        return false;
    }
    else
    {
        return Utils::GUIDComparer::compare(objectId, other.objectId) < 0;
    }
}
