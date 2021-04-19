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

Supported Unity versions | Holographic Remoting version
:-----------------: | :----------------: |
Unity 2020 or higher |2.5.0 |

The [Holographic Remoting Samples](https://github.com/microsoft/MixedReality-HolographicRemoting-Samples) repository hosts sample applications for [Holographic Remoting](https://docs.microsoft.com/en-us/windows/mixed-reality/holographic-remoting-player). The two remote samples show how to write an application for streaming content to a Microsoft HoloLens 2 or a PC running Windows Mixed Reality.

| Sample | Platform | Runtime |
| ---------- | -------- | ----------- |
| remote | HoloLens 2, PC | Mixed Reality |
| remote_openxr | HoloLens 2, PC | OpenXR |

The player sample shows how to write an application running on your Microsoft HoloLens 2 or a Windows Mixed Reality PC and receive streamed content, using the Mixed Reality runtime. The player sample is very similar to the Holographic Remoting Player available in the store. <!-- Link? -->

| Sample | Platform | Runtime |
| ---------- | -------- | ----------- |
| player | HoloLens 2, PC | Mixed Reality |

## Contents

| File/folder | Description |
|-------------|-------------|
| `player` | Unity assets, scenes, prefabs, and scripts. |
| `remote` | Project manifest and packages list. |
| `remote_openxr` | Unity asset setting files. |
| `.clang-format` | Generated user settings from Unity. |
| `.editorconfig` | Define what to ignore at commit time. |
| `.gitignore` | Define what data to ignore at commit time. |
| `CODE_OF_CONDUCT.md` | Details on the Microsoft Open Source Code of Conduct. |
| `LICENSE`   | The license for the sample. |
| `README.md` | This README file. |

## Prerequisites

* Visual Studio 2017 
    * With C++ Build Packages
    * With C++ UWP Build Packages
    * With Spectre Mitigation Libraries Packages (for release builds only)
    * With ARM and ARM64 C++ Packages
* Windows SDK 10.0.18362.0 (for Windows 10, version 1903)

## Setup

Clone or download this sample repository.

### Mixed Reality

1. Open one of the ```.sln``` files either under ```player/``` or ```remote/```. 
2. On first use, ensure to restore any missing NuGet packages. <!-- Link? -->

For more information, please refer to the official [Mixed Reality documentation](https://docs.microsoft.com/en-us/windows/mixed-reality/).

### OpenXR

1. Open the ```.sln``` file under ```remote_openxr```. 
2. On first use ensure, to restore any missing NuGet packages. <!-- Link? -->

For more information, please refer to the official [OpenXR reference](https://www.khronos.org/openxr/).

## Running the samples

Build and run. When running the remote app, pass the ip address of your HoloLens device as first argument to the application.

## Key concepts 

<!-- Content? -->

## Best practices

<!-- Content? -->

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
