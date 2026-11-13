# GNUS-NEO-SWARM JSON compatibility

`Model::ParseCompatible(json)` accepts the existing GNUS-NEO-SWARM shape.
`Model::Parse(json)` still accepts the earlier flat prototype. Callers select
which contract to load; the library does not guess or mix field names.
This adapter does not establish the GQHSM editor's final export format.

```cpp
auto model = gqhsm::Model::ParseCompatible(json);
gqhsm::Machine machine(model);
// Register callbacks named in the model before starting.
machine.start();
machine.dispatch("begin_classification");
```

| Input field | C++ behavior |
| --- | --- |
| `stateMachineInfo.name` | Machine identity used by snapshots |
| `stateMachineInfo.version` | Integer 1; defaults to 1 |
| `states[].name` and recursive `children` | Globally unique state names and parent hierarchy |
| `isStartState` | At most one per sibling list; otherwise the first sibling is initial |
| `entryAction`, `exitAction` | Named host callbacks |
| Top-level `transitions[]` | Attached to their named source states |
| `eventSignal`, `guardCondition`, `action` | Event, named guard callback, named action callback |
| `evaluationOrderPriority` | Signed 32-bit integer, lower first within a state; defaults to 0 |
| `transitionType: Normal` (default) | External transition; self/ancestor-source transitions run exit and entry actions |
| `transitionType: Internal` | Action-only; target must equal source; active child stays active |
| `eventSource` | Empty/absent only; named ports raise an error |

The current leaf gets the first chance to handle an event, then its ancestors.
Equal priorities for the same source/event raise an error, rather than depend
on JSON array order. Guards are callback names, never evaluated expressions.
Missing callbacks raise errors. A false guard allows the next candidate.

Unknown fields raise errors, including timers, history, and deferred-event
settings. Known descriptive fields (`metadata`, transition `name`, machine
`description` and `namespace`) do not change execution. Internal transitions
to a different state are unsupported and raise errors.

The adapter normalizes into the existing executor model. Its `external` flag
selects exit/reentry behavior for Normal transitions. The flat prototype's
existing default behavior and higher-first `priority` ordering stay intact.
As before, callback side effects are not rolled back if a callback throws.

## Evidence and fixture

The source contract and router fixture come from
[GNUS-NEO-SWARM state_machine.py](https://github.com/GeniusVentures/GNUS-NEO-SWARM/blob/main/gnus-poc/router/state_machine.py),
blob `bbebe125171afd2d112ab506c171e80aa2d04ac0`, inspected November 13, 2026.
`tests/fixtures/neo-router.json` is the `_ROUTER_CLASSIFIER_DEFINITION` data,
without Python runtime code. It exercises hit, rejected-guard, and complete
miss paths in C++. The synthetic compatibility cases cover nested hierarchy,
entry/exit order, internal and external self transitions, snapshot recovery,
integer priority limits, missing callbacks, and invalid/unsupported models.

The Python schema describes priority ordering and hierarchy, but its runtime
neither sorts those priorities nor proves nested execution. These tests
establish the C++ behavior; they do not claim Python/C++ runtime parity.
