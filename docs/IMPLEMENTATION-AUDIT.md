# Existing HSM implementations and JSON model audit

**Reviewed:** 2026-11-13. **Scope:** Relevant accessible repositories owned by
`Super-Genius` and `GeniusVentures`, including selected development branches.
Search/index coverage is not proof that no implementation exists on an
unindexed/private branch or in local, unpublished code.

## What exists

| Source | Language and model | Finding |
| --- | --- | --- |
| [ElementalEngine2/QHStateMachine](https://github.com/Super-Genius/ElementalEngine2/tree/master/Plugins2/ElementalEngine/QHStateMachine) | C++, XML `.xms` models | Native event-driven hierarchical executor, state object, event and transition handlers, Lua callbacks, tests/editor. It predates later work and depends on Elemental Engine; the legacy code reads an XML `guard` field but does not execute that guard. Multiple transitions use weighted random selection. |
| [GQHSM/qf4net](https://github.com/Super-Genius/qf4net/tree/master/GQHSM) | C#, XML state glyph model | Richer guards, timeouts, ports, state hierarchies and model tooling. No standalone native C++ runtime found in this source. |
| [hsm](https://github.com/Super-Genius/hsm) | C++, code-defined states | MIT-licensed Antonio Maiorano fork. Includes transitions and hierarchy; the separate [unit-tests branch](https://github.com/Super-Genius/hsm/tree/unit-tests) adds Catch tests. It does not execute JSON models. |
| [elementalengine3](https://github.com/Super-Genius/elementalengine3) | C++ engine | Both `main` and `develop` trees inspected; no state-machine implementation or QHSM source identified in the tracked tree. |
| [GNUS-NEO-SWARM router/state_machine.py](https://github.com/GeniusVentures/GNUS-NEO-SWARM/blob/main/gnus-poc/router/state_machine.py) | Python, data-driven JSON; 2026 | Newest concrete GQHSM-compatible JSON execution code identified. Defines `GQHSM_JSON_SCHEMA`, `RouterStateMachine`, handler registry and a seven-state classifier. It wraps Python `transitions.Machine`; the library is imported lazily. |
| [GNUS-NEO-SWARM tests](https://github.com/GeniusVentures/GNUS-NEO-SWARM/blob/main/gnus-poc/tests/test_state_machine.py) | Python pytest | Flat-machine guard/trigger tests; tests skip if Python `transitions` is unavailable. Nested-state execution is not verified by these tests. |
| [GNUS-NEO-SWARM GQHSM coordination ADR](https://github.com/GeniusVentures/GNUS-NEO-SWARM/blob/develop/.planning/workstreams/neoswarm/architecture/gqhsm-expert-model-coordination.md) | Design | Explicitly says to reuse the existing GQHSM compatibility contract, not invent another state-machine format. |
| [GCS epistemic-arbitration document §21.14.2](https://github.com/GeniusVentures/GeniusCognitiveSystem/blob/main/docs/architecture/epistemic-arbitration-and-cognitive-os.md) | Design JSON, not C++ executor | Shows another *illustrative* data-driven machine format, with `machine`, `initialState`, object-valued `states`, `onEntry`, `do` and `globalActions`. It is not the same shape as the GNUS-NEO-SWARM Python schema. |

The `SuperGenius` main/develop source trees, `GeniusSDK`,
`GeniusNetwork`, `gnus-ai`, `GeniusCognitiveSystem`, and the
`GNUS-NEO-SWARM` main/develop trees were also inspected for native HSM
source paths or code-search matches. No newer, standalone **native C++
JSON-driven GQHSM executor** was identified in those checked trees.

## Major JSON contract conflict

The current GQHSM-CPP JSON model is an **unverified prototype**, not a
confirmed GQHSM editor output. It conflicts with the schema already defined
in the 2026 Python implementation:

| Contract item | GNUS-NEO-SWARM `GQHSM_JSON_SCHEMA` | GQHSM-CPP prototype |
| --- | --- | --- |
| Machine identity | `stateMachineInfo.name` | `name` |
| Version | `stateMachineInfo.version` | `version` |
| States | `states[].name`, nested `children`, `isStartState` | flat `states[].id`, `parent`, `initial` |
| Entry and exit | `entryAction`, `exitAction` strings | `entry`, `exit` arrays |
| Transitions | global `transitions[]` with `source`, `target` | nested `states[].on[]` |
| Events and guards | `eventSignal`, `guardCondition`, `action` | `event`, `guard`, `actions` |
| Priorities | `evaluationOrderPriority`: smaller runs first | `priority`: larger runs first |
| Ports and kind | `eventSource`, `transitionType` | not yet represented |

There is also a third **illustrative, not normative** JSON format in the GCS
epistemic-arbitration design. Do not promote that example into a second
canonical model format.

## Execution gaps to verify

1. The Python router imports `Machine, State` from `transitions` and
   constructs `State(children=...)`, rather than using a documented
   `HierarchicalMachine` with nested-state support. It is not evidence of
   working deep hierarchy; add direct nested-state compatibility tests.
2. The Python code declares `evaluationOrderPriority` but its transition
   construction does not explicitly use that field. Priority semantics need
   tests, not assumptions.
3. The Python tests use `pytest.importorskip("transitions")`. Ensure a CI
   target installs the dependency so tests cannot silently skip.
4. The C++ prototype does not support timers, histories, ports or true
   transactional effect handling yet. Its current test sources have not been
   verified by a full build.

## Recommended next implementation steps

1. **Select one canonical JSON format** with the GQHSM editor/exporter.
   Start by checking the Python `GQHSM_JSON_SCHEMA` against the actual
   GQHSM C# XML model and its expected export. Preserve compatibility with
   the existing 2026 JSON examples through fixtures or a versioned adapter.
2. Add JSON Schema and cross-runtime **golden model fixtures**: nested
   hierarchy, internal transitions, guard ordering, entry/exit ordering,
   timeouts, history, ports, and invalid models.
3. Bring the C++ executor into compliance with the chosen model. Do not
   silently reinterpret priority order or ignore unknown callbacks.
4. Use old ElementalEngine2 code for behavioral tests and documentation,
   not as a dependency. Evaluate code copying only after license/provenance
   review; the original EE License is distinct from MIT.
5. Keep the C++ library independent of RocksDB, OpenAPI, Flutter and
   application-specific business operations.

## Licensing

The new `GQHSM-CPP` original code is MIT licensed. The older
`Super-Genius/hsm` fork is MIT licensed by Antonio Maiorano and requires
preserving the applicable copyright notice if copied. ElementalEngine2
source carries the EE License. The GQHSM/qf4net and StateProto sources
need individual provenance/license checks before any reuse.
