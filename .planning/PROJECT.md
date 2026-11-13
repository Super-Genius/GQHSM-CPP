# GQHSM-CPP

Standalone cross-platform C++17 hierarchical state-machine executor. JSON-first: GQHSM is expected to export JSON definitions. Use the existing GQHSM/qf4net and ElementalEngine2 implementations for behavioral reference, not as runtime dependencies.

## Requirements
- Deterministic named-event dispatch, hierarchical event inheritance, guarded transitions and entry/exit actions.
- Versioned JSON models, validation, machine-per-entity instances, snapshot/restore.
- Timers, history, ports, deferred event queues and advanced semantics in later phases.
- Host-supplied actions and guards. No domain logic, OpenAPI, RocksDB or Lua dependencies.
- Use **GeniusVentures/cmaketemplate** as an actual Git submodule at `build/`. Configure only through its platform-specific wrappers, as in Genius AI Boss.
- Test old and new behavior; do not copy original code without a source/license audit.

## References
- https://github.com/Super-Genius/GQHSM
- https://github.com/Super-Genius/qf4net
- https://github.com/Super-Genius/StateProto
- https://github.com/Super-Genius/ElementalEngine2/tree/master/Plugins2/ElementalEngine/QHStateMachine
