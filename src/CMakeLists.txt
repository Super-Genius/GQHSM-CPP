get_filename_component(GQHSM_CPP_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

add_library(gqhsm_cpp STATIC
    Machine.cpp
    CompatibleModel.cpp
)
add_library(GQHSM::CPP ALIAS gqhsm_cpp)
target_compile_features(gqhsm_cpp PUBLIC cxx_std_17)
target_include_directories(gqhsm_cpp PUBLIC
    $<BUILD_INTERFACE:${GQHSM_CPP_ROOT}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(gqhsm_cpp PUBLIC nlohmann_json::nlohmann_json)

# Tests belong only to a standalone GQHSM-CPP build. Consumers such as
# Genius AI Boss must not compile this library's tests as a submodule.
if(BUILD_TESTING AND "${PROJECT_ROOT}" STREQUAL "${GQHSM_CPP_ROOT}")
    add_subdirectory("${GQHSM_CPP_ROOT}/tests" "${CMAKE_BINARY_DIR}/gqhsm_cpp_tests")
endif()
