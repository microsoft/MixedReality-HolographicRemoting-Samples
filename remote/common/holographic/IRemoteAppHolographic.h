#pragma once

#include <memory>
#include <string>

class RemoteWindowHolographic;

// Abstract base class for Holographic App Remoting remote side applications.
// Provides an abstraction layer to allow writing remote applications platform independent.
// To write a remote application, implement this interface and provide a CreateRemoteAppHolographic()
// method implementation, that creates an instance of your custom RemoteAppHolographic class.
class IRemoteAppHolographic
{
public:
    // Sets the current window for the application.
    virtual void SetWindow(RemoteWindowHolographic* window) = 0;

    // Provides the command line arguments, the application was started with.
    virtual void ParseLaunchArguments(std::wstring_view arguments) = 0;

    // Ticks the application while the window is visible.
    virtual void Tick() = 0;

    // Notifies the application about key presses.
    virtual void OnKeyPress(char key) = 0;

    // Notifies the application about the window changing its size.
    virtual void OnResize(int width, int height) = 0;
};

std::shared_ptr<IRemoteAppHolographic> CreateRemoteAppHolographic();
