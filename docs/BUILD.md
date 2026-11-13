# Build using Genius Ventures CMake template

This repository follows the same pattern as Genius AI Boss: the `build/` directory is the **Git submodule** `GeniusVentures/cmaketemplate`, not a conventional `cmake -S . -B build` output folder.

```bash
git clone --recurse-submodules https://github.com/Super-Genius/GQHSM-CPP.git
cd GQHSM-CPP
git checkout develop
git submodule update --init --recursive

# macOS:
mkdir -p build/OSX/Debug
cd build/OSX/Debug
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja
ctest --output-on-failure

# Linux from repository root:
mkdir -p build/Linux/x86_64/Debug
cd build/Linux/x86_64/Debug
cmake ../.. -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja
ctest --output-on-failure
```

The project-specific CMake entry is `cmake/CommonBuildParameters.cmake`. The top-level `CMakeLists.txt` intentionally rejects direct configuration. Install `nlohmann_json` with its CMake package configuration before configuring; this early executable has only one dependency. The full Genius Ventures thirdparty bundle is not needed.
