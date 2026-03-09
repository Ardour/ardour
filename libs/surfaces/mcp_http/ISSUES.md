# MCP HTTP Open Issues

## 1. `plugin/get_description` does not reject an invalid route id

Status: mitigated in MCP layer (2026-03-09), verified with live MCP calls (2026-03-09), core Ardour behavior unchanged.

Observed behavior:

- Calling `plugin/get_description` with a non-route string in `id` does not return an error.
- Example input:

```json
{
  "id": "Frank",
  "pluginIndex": 0
}
```

- Example observed result:

```json
{
  "plugin": {
    "index": 0,
    "name": "ACE Reasonable Synth",
    "displayName": "ACE Reasonable Synth",
    "enabled": true,
    "active": true,
    "maker": "Ardour Community",
    "label": "ACE Reasonable Synth",
    "uniqueId": "https://community.ardour.org/node/7596"
  },
  "parameters": []
}
```

Expected behavior:

- If `id` is not a valid route id, the tool should return a JSON-RPC error such as `-32602` (`Route not found`) instead of resolving some other route implicitly.

Notes:

- The example above used `"id": "Frank"`, which appears to have been interpreted as something other than a strict route id lookup.
- This should be audited anywhere route/tool lookup accepts `id` and may fall back to name or selection unexpectedly.

Related reproduction:

- `track/get_info` with `"id": "Frank"` returned the route `Rose` (`id: "208297"`) instead of failing.
- That suggests invalid string ids may be parsed into a non-null `PBD::ID` and match an unrelated route.

Implementation note:

- `PBD::ID` string construction is not fail-closed.
- In [id.cc](/Users/frankp/Projects/ardour/libs/pbd/id.cc:54), `ID::ID (string str)` calls `string_assign (str)`.
- In [id.cc](/Users/frankp/Projects/ardour/libs/pbd/id.cc:73), `string_assign()` returns `string_to_uint64 (str, _id)`.
- The constructor and assignment operator ignore that boolean result, so malformed strings like `"Frank"` can leave `_id` in an undefined or stale state and then resolve to an unrelated object.

Implemented MCP-side fix:

- `mcp_http_server.cc` now validates user-provided object IDs as decimal-only strings before constructing `PBD::ID`.
- Route, region, and location lookups in MCP now use guarded helper functions:
  - `route_by_mcp_id`
  - `region_by_mcp_id`
  - `location_by_mcp_id`
- Invalid IDs now fail lookup deterministically in MCP (returning the existing `not found` JSON-RPC errors), instead of potentially resolving to an unrelated object.

Remaining core concern:

- `PBD::ID (string)` still does not fail-closed at the Ardour core level; this issue is broader than MCP and should be handled separately if desired.

Verification notes (live MCP, after Ardour restart):

- `track/get_info` with `id: "Frank"` now returns `-32602: Route not found`.
- `plugin/get_description` with `id: "Frank"` now returns `-32602: Route not found`.
- `track/get_info` with non-existent numeric `id: "999999999"` returns `-32602: Route not found`.
- `plugin/get_description` with non-existent numeric `id: "999999999"` returns `-32602: Route not found`.
- Valid numeric route id (example: `205535` for track `Frank`) resolves correctly in both methods.
