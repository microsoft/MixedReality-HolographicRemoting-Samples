#include <pch.h>

#include <holographic/RemoteWindowHolographic.h>

RemoteWindowHolographic::RemoteWindowHolographic(const std::shared_ptr<IRemoteAppHolographic>& app)
    : m_app(app)
{
}
