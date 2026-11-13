# GQHSM-CPP test strategy

Following GeniusVentures/evmrelay: independent GoogleTest binaries, CTest,
xUnit XML, happy and unhappy paths, and complete functional event sequences.

| Target | Scope |
| --- | --- |
| gqhsm_model_tests | Model parsing, structural validation, invalid JSON |
| gqhsm_machine_tests | Guards, actions, hierarchy, snapshots, exceptional paths |
| gqhsm_workflow_tests | Payment-like lifecycle, failure, refund, restart, isolation |
| gqhsm_property_tests | Seeded event sequences and determinism stress |
| gqhsm_compatible_model_tests | Existing router JSON, nested states, priorities, transition kinds, rejected models |

Run from a configured build/OSX/Debug or build/Linux/x86_64/Debug:

~~~bash
cmake --build . --parallel
ctest --output-on-failure
ctest -L unit --output-on-failure
ctest -L functional --output-on-failure
ctest -L negative --output-on-failure
~~~

The cmaketemplate addtest() helper emits xUnit reports to the build directory's
xunit folder. Tests use GoogleTest assertions, not C assert(), including Release.

CI gates: Debug and Release on Linux/macOS/Windows, ASan/UBSan, and later TSan
once shared-instance concurrency is a supported feature. JSON model fuzzing,
GQHSM exporter compatibility, timers, history and ports remain future work.

Tests are deterministic and offline. Functional tests use fake processor
callbacks and host-managed snapshot persistence, not a live payment provider.
Callbacks' external effects cannot be rolled back by the current executor.

Reference: https://github.com/GeniusVentures/evmrelay/tree/main/test

## Local verification: November 13, 2026

All 90 GoogleTest cases passed across five CTest targets on Linux with GCC
14.2, nlohmann_json 3.11.3, GoogleTest 1.15.2, and CMake 4.4.4. The library
and tests compiled with `-fsanitize=address,undefined -fno-omit-frame-pointer`.
The run used `ASAN_OPTIONS=detect_leaks=0`: LeakSanitizer cannot operate under
this environment's process tracer. Leak checking remains unverified.

A temporary CMake harness loaded the repository's actual `src/CMakeLists.txt`
and `tests/CMakeLists.txt` and the pinned `build/cmake/functions.cmake`
`addtest()` helper. Dependencies came from their upstream tagged sources.
This verifies the library, five test targets, CTest registration, and xUnit
reports. It does **not** verify the full platform wrapper's thirdparty/zkLLVM
bootstrap, other operating systems, or mobile toolchains. No replacement
build entrypoint was added to the repository.
