# Hybrid Rendering Engine Module

[![Build status][s1]][av] [![License: MIT][s3]][li]

[s1]: https://ci.appveyor.com/api/projects/status/8m3u1m2uhjpnumbj?svg=true
[s3]: https://img.shields.io/badge/License-MIT-orange.svg

[av]: https://ci.appveyor.com/project/korfriend/hybridrenderingengine
[li]: https://opensource.org/licenses/MIT

<br/>
This project is for a hybrid rendering engine that handles the fusion of multiple volumetric and potentially transparent surface data based on our VisMotive framework. This project includes the source code for the paper entitled "z-thickness blending for multi-fragment merging" (preparing submission to Euro Graphics). It is also used for visualizing informative point sets for our local isosurfacing technique (https://github.com/korfriend/LocalIsosurfaceModeler), which is based on the paper entitled "Confidence-controlled Local Isosurfacing" (accepted in TVCG and preparing preprint (200810)). Detailed "get started" will be prepared with a sample code project. You can download the code by using Git and cloning the repository, or downloading it as zip. This will give you the full C++ source code that you must build for yourself. 

### Platforms:
- Windows PC Desktop (x64)

### Requirements:

- Windows 10
- Visual Studio 2017
- GPU that supports at least Direct3D 11.3

### Dependencies:

- GL math (included)
- VisMotive (included)
- Direct3D 11.3 (default windows SDK) 

### Build Environments
Current build environment assumes the following structure of the developement folders. As external dependencies, our VisMotive-based projects use the core APIs and libraries (https://github.com/korfriend/VisMotive-CoreAPIs/) for most of the volumetric and polygonal processing tasks. To be clear your folder structure should be something quite similar to:

    yourdevfolder/
     |
     ├──bin (built files are available in https://github.com/korfriend/VisMotive-BuiltBinary)
     │   ├──X64_Debug
     │   └──X64_Release
     └──External Projects
         ├──HybridRenderingEngine (this module project https://github.com/korfriend/HybridRenderingEngine)
         ├──other module projects
         ├──...
         └──...

build folders (e.g., X64_Debug and X64_Release) should include the following dll files
- CommonUnits.dll
- GpuManager.dll

### What Next?!
- Sample code for "get started" will be available via VisMotive framework.
