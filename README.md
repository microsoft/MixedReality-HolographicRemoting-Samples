
# Holographic Remoting Samples

This repository hosts the two sample applications for Holographic Remoting. (https://docs.microsoft.com/en-us/windows/mixed-reality/holographic-remoting-player)
* The remote sample shows how to write an application streaming content to a Microsoft HoloLens 2 or a PC running Windows Mixed Reality.
* The player sample shows how to write an application running on your Microsoft HoloLens 2 or a Windows Mixed Reality PC and recieving streamed content. The player sample is very similar to the Holographic Remoting Player available in the store.

## Prerequisites

* Visual Studio 2017
    * With C++ Build Packages
    * With C++ UWP Build Packages
    * With Spectre Mitigation Libraries Packages (for release builds only)
    * With ARM and ARM64 C++ Packages
* Windows SDK 10.0.18362.0 (for Windows 10, version 1903)

## Getting Started

1. Open one of the ```.sln``` files either under ```player/``` or ```remote/```. 
2. On first use ensure to restore any missing nuget packages. 
3. Then build and run.
    * When running the remote app, pass the ip address of your HoloLens device as first argument to the application.

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
