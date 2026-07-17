---
name: cgn-mcp
description: "Use when an agent needs to inspect or build CGN targets through the CGN Model Context Protocol server."
---

# CGN MCP Skill

Use MCP tools instead of constructing ad hoc shell build commands when the CGN
server is available.

## Required Workflow

1. Call `cgn_list_configs` before the first build in an unfamiliar workspace.
2. Use `cgn_analyse` or `cgn_query` to inspect a target and diagnose failures.
3. Call `cgn_build` with both `target_label` and a non-empty `config_name`.

```json
{
  "target_label": "@cell//dir:name",
  "config_name": "debug"
}
```

`config_name` must be a named configuration from `cgn_setup.cgn.cc`. Do not
depend on command-line `--target` to alter `DEFAULT` for MCP builds.

## Tool Selection

| Tool | Use when |
|---|---|
| `cgn_list_configs` | configuration name is unknown |
| `cgn_analyse` | inspect a target without building |
| `cgn_query` | need detailed configuration or target information |
| `cgn_build` | target and named configuration are known |

Use full labels across cells: `@cell//dir:name`. If a tool reports a script
compile failure, repair the relevant `.cgn.cc` source and analyse again before
building.
