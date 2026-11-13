# Deliberately not a CMake configure entry.
# Use the GeniusVentures/cmaketemplate platform wrappers in build/.
cmake_minimum_required(VERSION 3.22)
message(FATAL_ERROR
  "Configure from build/<Platform>/<BuildType>, not the repository root. "
  "For example: mkdir -p build/OSX/Debug && cd build/OSX/Debug && cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug")
