add_library(gqhsm_cpp STATIC
    Machine.cpp
)
add_library(GQHSM::CPP ALIAS gqhsm_cpp)
target_compile_features(gqhsm_cpp PUBLIC cxx_std_17)
target_include_directories(gqhsm_cpp PUBLIC
    $<BUILD_INTERFACE:${PROJECT_ROOT}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(gqhsm_cpp PUBLIC nlohmann_json::nlohmann_json)

if(BUILD_TESTING)
    add_executable(gqhsm_cpp_tests ${PROJECT_ROOT}/tests/MachineTests.cpp)
    target_link_libraries(gqhsm_cpp_tests PRIVATE GQHSM::CPP)
    add_test(NAME gqhsm_cpp_tests COMMAND gqhsm_cpp_tests)
endif()
