# The GeniusVentures/cmaketemplate platform entrypoints include this file.
# Do not duplicate compiler/toolchain setup in the repository root.
cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# This small library needs only the JSON parser; it must not pull in
# the full Genius Ventures thirdparty bundle.
find_package(nlohmann_json CONFIG REQUIRED)

include(CTest)
add_subdirectory("${PROJECT_ROOT}/src" "${CMAKE_BINARY_DIR}/gqhsm_cpp")
