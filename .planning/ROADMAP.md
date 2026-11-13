# Roadmap

## Phase 1: Repository and build
- [x] Initialize main and develop
- [x] Add `GeniusVentures/cmaketemplate` Git submodule in `build/`
- [x] Platform wrapper CMake entrypoints and project-specific `cmake/CommonBuildParameters.cmake`
- [x] C++17 library target and CTest smoke test (CI build verification pending)

## Phase 2: JSON model contract
- [x] Provisional JSON v1 model and structural validation
- [ ] Agree stable JSON schema with the GQHSM editor exporter
- [ ] Add JSON Schema validation and compatibility fixtures

## Phase 3: Executor
- [x] Initial hierarchical events, deterministic guard selection, callbacks and snapshots
- [ ] Confirm exact hierarchy/event semantics against original GQHSM and ElementalEngine2 tests
- [ ] Add history/deep history, timers, event deferral and ports
- [ ] Add safe transaction and action failure semantics; concurrency control
- [ ] Test runtime behavior under event races and crashes

## Phase 4: Release readiness
- [ ] Multiplatform CI for macOS/Linux/Windows; mobile toolchain validation
- [ ] Audit licenses and provenance before incorporating legacy code
- [ ] Build/install/export package targets and documentation

## Phase 5: Genius AI Boss
- [ ] Add GQHSM-CPP as a separate BOSS submodule
- [ ] Link OpenAPI and Jinja2 generators to exported JSON machine definitions
- [ ] Replace manual Commerce payment transition logic with behavior-preserving tests
