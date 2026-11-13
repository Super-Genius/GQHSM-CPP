# Flat prototype JSON model contract (provisional)

The GQHSM authoring tool is expected to export JSON, not XML. The structure here is a **temporary C++ MVP contract**. Align the fields and semantics with the editor before declaring version 1 stable. This is **not** a claim that the editor currently produces this format. **Important:** a different GQHSM-compatible JSON format already exists in `GNUS-NEO-SWARM/gnus-poc/router/state_machine.py`, with `stateMachineInfo`, `states[].children`, and top-level `transitions[]` and the *opposite* priority ordering. See [the implementation audit](IMPLEMENTATION-AUDIT.md). Do not treat this provisional format as the canonical export contract until the editor contract is agreed. The explicit `Model::ParseCompatible` adapter now accepts the existing GNUS-NEO-SWARM shape; see [compatibility behavior and limits](JSON-COMPATIBILITY.md).

```json
{
  "version": 1,
  "name": "payment",
  "initial": "pending",
  "states": [
    {
      "id": "pending",
      "on": [
        {
          "event": "AUTHORIZE",
          "guard": "can_authorize",
          "target": "authorized",
          "actions": ["on_authorized"],
          "priority": 10
        }
      ]
    },
    {
      "id": "authorized",
      "entry": ["on_entry"],
      "exit": ["on_exit"],
      "on": [{"event": "CAPTURE", "target": "captured"}]
    },
    {"id": "captured"}
  ]
}
```

- Each state may specify `parent`, `initial` child, `entry`, `exit`, and an `on` array.
- An event is resolved from the current leaf upward through its parent states.
- Guards and actions name callbacks registered by the host. A transition without `target` is an internal action-only event.
- Higher `priority` wins within a state. Equal event priorities within a state are invalid.
- A model is a shared immutable definition; each Machine holds its own current state.
- Snapshot: `{"version":1,"model":"payment","state":"captured"}`.
- Timers, history, deferrals, ports, variables, transition IDs and richer structured event payloads are intentionally out of scope for the first code baseline.
