---
page_type: sample
name: Holographic Remoting Samples
description: Samples showing how to write an application for streaming content to a Microsoft HoloLens 2 or PC running Windows Mixed Reality with the Mixed Reality or OpenXR runtime. 
languages:
- cpp
- html
products:
- windows-mixed-reality
- hololens
---

# Holographic Remoting Samples

![License](https://img.shields.io/badge/license-MIT-green.svg)

| Holographic Remoting version |
:-----------------: |
|2.6.1 |

The [Holographic Remoting Samples](https://github.com/microsoft/MixedReality-HolographicRemoting-Samples) repository hosts sample applications for [Holographic Remoting](https://docs.microsoft.com/en-us/windows/mixed-reality/holographic-remoting-player). The two remote samples show how to write an application for streaming content to a Microsoft HoloLens 2 or a PC running Windows Mixed Reality.

| Sample | Platform | Runtime |
| ---------- | -------- | ----------- |
| remote | HoloLens 2, PC | Mixed Reality |
| remote_openxr | HoloLens 2, PC | OpenXR |

The player sample shows how to write an application running on your Microsoft HoloLens 2 or a Windows Mixed Reality PC and receive streamed content, using the Mixed Reality runtime. The player sample is very similar to the [Holographic Remoting Player available in the Microsoft store](https://www.microsoft.com/p/holographic-remoting-player/9nblggh4sv40).

| Sample | Platform | Runtime |
| ---------- | -------- | ----------- |
| player | HoloLens 2, PC | Mixed Reality |

## Contents

| File/folder | Description |
|-------------|-------------|
| `player` | Holographic Remoting player sample code |
| `remote` | Holographic Remoting sample code |
| `remote_openxr` | Holographic Remoting sample code with OpenXR |
| `.clang-format` | Source code style formatting. |
| `.editorconfig` | Standard editor setup settings. |
| `.gitignore` | Define what data to ignore at commit time. |
| `CODE_OF_CONDUCT.md` | Details on the Microsoft Open Source Code of Conduct. |
| `LICENSE`   | The license for the sample. |
| `README.md` | This README file. |

## Prerequisites

Visual Studio 2019 with

- Workloads:
    - Desktop development with C++
    - Universal Windows Platform development
- Individual components
    - Windows 10 SDK (10.0.19041.0)
    - MSVC v142 - VS 2019 C++ x64/x86 build tools (newest)
    - MSVC v142 - VS 2019 C++ x64/x86 Spectre-mitigated libs (newest)
    - Net Native
    - .Net Framework 4.5 targeting pack
    - Nuget Package manager        
    - C++ 2019 Redistributable Update

For ARM builds additionally
- Individual components
    - MSVC v142 - VS 2019 C++ ARM64 build tools (newest)
    - MSVC v142 - VS 2019 C++ ARM64 Spectre-mitigated libs (newest)
    - C++ Universal Windows Platform support for v142 build tools (ARM64)


## Setup

Clone or download this sample repository.

### Mixed Reality

1. Open one of the ```.sln``` files either under ```player/``` or ```remote/```. 
2. On first use, right-click the solution and select **Restore NuGet Packages**.

For more information, please refer to the official [Mixed Reality documentation](https://docs.microsoft.com/en-us/windows/mixed-reality/).

### OpenXR

1. Open the ```.sln``` file under ```remote_openxr```. 
2. On first use, right-click the solution and select **Restore NuGet Packages**.

For more information, please refer to the official [OpenXR reference](https://www.khronos.org/openxr/).

## Running the samples

Build and run. When running the remote app, pass the ip address of your HoloLens device as first argument to the application.

## Key concepts 

The `player` sample application lets you customize the remote player experience using public APIs and the latest Holographic Remoting packages. If you don't need customization, use the [pre-packaged version on the Microsoft Store](https://www.microsoft.com/p/holographic-remoting-player/9nblggh4sv40).

Use the `remote` and `remote_openxr` samples to implement remoting functionality into a custom engine using C++. If you're using Unity or Unreal, this feature is already built-in.

## Best practices

Check out the [Holographic Remoting Player documentation](https://docs.microsoft.com/windows/mixed-reality/develop/platform-capabilities-and-apis/holographic-remoting-player) for information on quality, performance, diagnostics, and system requirements.

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
