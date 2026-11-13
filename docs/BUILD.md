# Build using Genius Ventures CMake template

This repository uses the same CMake entrypoint pattern as Genius AI Boss. `build/` is the **Git submodule** `GeniusVentures/cmaketemplate`, not a generic generated build folder.

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

The platform wrapper includes `cmake/CommonBuildParameters.cmake`, which defines the GQHSM-CPP targets. The root `CMakeLists.txt` rejects direct configuration.

**Important:** `GeniusVentures/cmaketemplate` currently includes `CommonCompilerOptions.cmake` unconditionally. This shared setup may locate or download the Genius Ventures `thirdparty` and `zkLLVM` bundles before the GQHSM-specific CMake file runs. The library itself links only to `nlohmann_json` and requires its CMake config to be available. No shortcut around the shared template bootstrap has been implemented. A future optional lightweight mode would need a change to `cmaketemplate`, not a duplicate entrypoint here.

The checked-in code has **not yet been verified by a CI build**. Multiplatform checks remain open roadmap items.
