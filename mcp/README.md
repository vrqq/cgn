# CGN MCP Server

The CGN MCP server exposes the in-process CGN API over MCP stdio. It keeps the
analysis graph alive between requests, so repeated target queries can reuse the
loaded scripts and target cache.

## Build And Configure

From the monorepo root:

```sh
cd @cgn.d
ninja -f build_linux.ninja
cd ..
./@cgn.d/build_linuxd/cgn --cgn_out cgn-out mcp
```

Configure your MCP client with the command in [MCP configuration](MCP_CONFIG.md).
The server process must start in the monorepo root so it can load
`cgn_setup.cgn.cc`.

## Tool Contract

| Tool | Required arguments | Result |
|---|---|---|
| `cgn_list_configs` | none | Every named configuration and its settings. |
| `cgn_analyse` | `target_label` | Resolved configuration and analysis result; no Ninja build. |
| `cgn_query` | `target_label` | Full target information and resolved configuration. |
| `cgn_build` | `target_label`, `config_name` | Builds a target with the named configuration. |

All target tools accept an optional `config_name` except `cgn_build`, where it
is required. Tool labels must use CGN label syntax, for example
`@mycell//app:server`.

## Agent Workflow

1. Call `cgn_list_configs` to discover configuration names.
2. Call `cgn_analyse` or `cgn_query` to inspect a target.
3. Call `cgn_build` with both `target_label` and the chosen `config_name`.

Example build arguments:

```json
{
  "target_label": "@mycell//app:server",
  "config_name": "debug"
}
```

Do not use `--target` in the MCP server command to select a build
configuration. `--target` only alters `DEFAULT` when the server starts and
makes calls dependent on process startup arguments. Named configurations in
`cgn_setup.cgn.cc` are explicit and reproducible.

## Failure Handling

- Unknown configuration: inspect `cgn_list_configs`, then define the required name in `cgn_setup.cgn.cc`.
- Invalid label: use only CGN label characters and a valid `@cell//dir:name` target.
- Script compile or `dlopen` failure: use `cgn_analyse` first and repair the reported `.cgn.cc` error.
