# Mirrors evmrelay: independent GoogleTest binaries registered in CTest.
# cmaketemplate addtest() also produces xUnit XML.
find_package(GTest CONFIG REQUIRED)

function(gqhsm_add_test name source label)
    addtest(${name} ${source})
    target_link_libraries(${name} GQHSM::CPP)
    target_compile_definitions(${name} PRIVATE GQHSM_TEST_FIXTURES_DIR="${GQHSM_CPP_ROOT}/tests/fixtures")
    set_tests_properties(${name} PROPERTIES LABELS "${label}" TIMEOUT 45)
endfunction()

gqhsm_add_test(gqhsm_model_tests ModelTests.cpp "unit;model;negative")
gqhsm_add_test(gqhsm_machine_tests MachineTests.cpp "unit;executor;negative")
gqhsm_add_test(gqhsm_workflow_tests FunctionalWorkflowTests.cpp "functional;integration;negative")
gqhsm_add_test(gqhsm_property_tests PropertyTests.cpp "unit;property;stress")
gqhsm_add_test(gqhsm_compatible_model_tests CompatibleModelTests.cpp "unit;model;functional;negative;compatibility")
