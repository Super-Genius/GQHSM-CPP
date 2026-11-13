# GQHSM-CPP

Standalone, cross-platform C++17 hierarchical state-machine executor.

This project reuses the **design** of [GQHSM](https://github.com/Super-Genius/GQHSM) / qf4net and the native C++ execution work in [ElementalEngine2](https://github.com/Super-Genius/ElementalEngine2), without requiring their runtimes.

**JSON is the native model format.** GQHSM is expected to export models as JSON. The initial JSON contract is provisional until the exporter and executor agree on the schema. The runtime does not load XML.

The library will support hierarchical states, deterministic guarded transitions, entry/exit actions, timers, history, events, and state snapshots. It must remain independent of application-specific storage, network, UI, and business rules.

See [the roadmap](.planning/ROADMAP.md), [existing implementations audit](docs/IMPLEMENTATION-AUDIT.md), and [model contract](docs/JSON-MODEL.md) on the `develop` branch for current work. The develop branch contains a minimal executor and five GoogleTest/CTest suites. All 90 cases pass in local Linux target builds under ASan/UBSan; full platform bootstrap and cross-platform CI remain unverified. See [JSON compatibility](docs/JSON-COMPATIBILITY.md) for loading the existing GNUS-NEO-SWARM model through `Model::ParseCompatible`.

## Testing

See [the test plan and commands](docs/TESTING.md). Tests cover valid and invalid models, transition rules, callback failure, full payment-like workflows and deterministic event replay. Run with CTest from the standard cmaketemplate platform build directories.

## Principles

- Native C++17 and CMake; portable to Linux, macOS, Windows, Android, and iOS.
- Machine models are data. Domains supply guards and actions.
- Deterministic execution. Never use random transitions for business events.
- No dependency on ElementalEngine, Lua, .NET, RocksDB, Flutter, or an OpenAPI runtime.
- JSON model evolution is versioned; unknown or invalid data fails clearly.
- Keep machine state per entity; persistence and business side effects belong to the host.

## Source and licensing

The earlier implementations are references, not dependencies. Do not copy source from either project until the provenance and distribution terms of the relevant files have been reviewed. GQHSM-CPP's original implementation is released under the **MIT License** (see [LICENSE](LICENSE)). Referenced projects remain subject to their own licenses; importing any existing source requires a separate license and provenance review.
