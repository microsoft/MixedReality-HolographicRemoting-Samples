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

#include <mutex>
#include <vector>

#include <winrt/Windows.UI.Input.Spatial.h>

#include <PerceptionDevice.h>

// Base class for perception root objects managed by the PerceptionDeviceHandler
class PerceptionRootObject
{
public:
    virtual ~PerceptionRootObject();

    const GUID& GetPropertyId() const;
    const GUID& GetObjectId() const;

protected:
    PerceptionRootObject(IPerceptionDevice* device, const GUID& typeId, const GUID& objectId);

protected:
    winrt::com_ptr<IPerceptionDevice> m_device;
    GUID m_typeId;
    GUID m_objectId;
};

// Sample perception device handler. Listens to the availability of perception devices (more accurately:
// perception root objects of known types), and retrieves data from these root objects.
class PerceptionDeviceHandler : public std::enable_shared_from_this<PerceptionDeviceHandler>
{
public:
    PerceptionDeviceHandler();
    ~PerceptionDeviceHandler();

    // Starts monitoring for perception root object changes
    void Start();

    // Stops monitoring perception root object changes
    void Stop();

    // Iterates over all perception root objects currently known
    template <typename Func>
    void ForEachRootObject(Func& func)
    {
        std::lock_guard stateLock(m_stateProtect);
        for (auto& rootObjectEntry : m_rootObjects)
        {
            func(*rootObjectEntry.second.get());
        }
    }

    // Iterates over all root objects of a certain type
    template <typename RootObjectType, typename Func>
    void ForEachRootObjectOfType(Func& func)
    {
        std::lock_guard stateLock(m_stateProtect);
        for (auto& rootObjectEntry : m_rootObjects)
        {
            PerceptionRootObject& rootObject = *rootObjectEntry.second.get();
            if (GUIDComparer::equals(rootObject.GetPropertyId(), RootObjectType::GetStaticPropertyId()))
            {
                func(static_cast<RootObjectType&>(rootObject));
            }
        }
    }

private:
    struct RootObjectChangeHandler
        : winrt::implements<RootObjectChangeHandler, IPerceptionDeviceRootObjectAddedHandler, IPerceptionDeviceRootObjectRemovedHandler>
    {
        RootObjectChangeHandler(PerceptionDeviceHandler& owner);

        IFACEMETHOD(Invoke)
        (_In_ IPerceptionDeviceRootObjectWatcher* sender, _In_ IPerceptionDeviceRootObjectAddedEventArgs* args) override;
        IFACEMETHOD(Invoke)
        (_In_ IPerceptionDeviceRootObjectWatcher* sender, _In_ IPerceptionDeviceRootObjectRemovedEventArgs* args) override;

    private:
        std::weak_ptr<PerceptionDeviceHandler> m_weakOwner;
    };
    friend RootObjectChangeHandler;

    struct RootObjectKey
    {
        GUID propertyId;
        GUID objectId;

        bool operator<(const RootObjectKey& other) const;
    };

    using RootObjectMap = std::map<RootObjectKey, std::shared_ptr<PerceptionRootObject>>;

private:
    HRESULT HandleRootObjectAdded(IPerceptionDeviceRootObjectAddedEventArgs* args);
    HRESULT HandleRootObjectRemoved(IPerceptionDeviceRootObjectRemovedEventArgs* args);

private:
    std::recursive_mutex m_stateProtect;

    bool m_running{false};

    winrt::com_ptr<IPerceptionDeviceFactory> m_perceptionDeviceFactory;
    std::vector<winrt::com_ptr<IPerceptionDeviceRootObjectWatcher>> m_rootObjectWatchers;
    winrt::com_ptr<RootObjectChangeHandler> m_rootObjectChangeHandler;

    RootObjectMap m_rootObjects;
};
