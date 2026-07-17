# MCP Configuration

Use the `cgn` executable with its `mcp` subcommand. There is no separate
`cgn_mcp` executable.

```json
{
  "mcpServers": {
    "cgn": {
      "command": "${workspaceFolder}/@cgn.d/build_linuxd/cgn",
      "args": ["--cgn_out", "cgn-out", "--scriptcc-debug", "--halt-on-error", "mcp"]
    }
  }
}
```

Place this in the MCP client configuration for the monorepo. Build the debug
binary first with `cd @cgn.d && ninja -f build_linux.ninja`.

Do not put `--target` in the server arguments. It changes `DEFAULT` at startup.
For `cgn_build`, clients must provide a named `config_name` defined by
`cgn_setup.cgn.cc`. See [MCP server](README.md).
