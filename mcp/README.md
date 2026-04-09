# CGN MCP Server

The `cgn_mcp` binary is a [Model Context Protocol](https://modelcontextprotocol.io/) (MCP) server that exposes CGN build capabilities to AI agents (GitHub Copilot, Claude Desktop, Cursor, etc.).

## Build

```sh
# Linux debug
./debug.sh @cgn.d//mcp:cgn_mcp

# Linux release
./release.sh @cgn.d//mcp:cgn_mcp

# macOS
./debug.sh @cgn.d//mcp:cgn_mcp   # uses Xcode toolchain from cgn_setup
```

The binary is placed in `cgn-out/obj/cgn.d_/mcp_/cgn_mcp_HASHID/cgn_mcp`.

## Configuration

MCP arguments (`--target`, `--cgn-out`, etc.) are written **once** in your MCP client's config file — not typed at the CLI each time. Copy the relevant block from `cgn_mcp_config.example.json`:

```json
{
  "mcpServers": {
    "cgn": {
      "command": "./@cgn.d/build_linuxd/cgn_mcp",
      "args": ["--target", "llvm,debug,asan", "--cgn-out", "cgn-out"]
    }
  }
}
```

### VS Code (GitHub Copilot / Copilot Chat)

Add the block above to `.vscode/mcp.json` in your workspace root, or to your user settings under `"mcp"`.

### Claude Desktop

Add to `~/.config/claude/claude_desktop_config.json` (Linux) or `~/Library/Application Support/Claude/claude_desktop_config.json` (macOS).

### Cursor

Add to `.cursor/mcp.json` in the workspace root.

## Available Tools

| Tool | Description |
|------|-------------|
| `cgn_analyse` | Analyse a target (resolve deps, generate ninja) without building |
| `cgn_build` | Build a target and return primary output path(s) |
| `cgn_query` | Show the resolved Configuration and full analysis result |
| `cgn_list_configs` | List all named configs from `cgn_setup.cgn.cc` |

## Argument Reference

| Argument | Description | Example |
|----------|-------------|---------|
| `--target <tokens>` | Comma-separated config tokens (same as `./debug.sh`) | `llvm,debug,asan` |
| `--cgn-out <dir>` / `-C <dir>` | Output directory | `cgn-out` |
| `--halt_on_error` | Exit on first analysis error | — |
| `--verbose` / `-V` | Verbose output | — |
| `--scriptcc <path>` | Custom C++ compiler for `.cgn.cc` files | `/usr/bin/clang++` |
| `--winenv` | Load MSVC environment before compiling scripts (Windows) | — |

## How It Works

`cgn_mcp` starts up once, calls `api.init()` with the args from the MCP config, then enters a loop reading newline-delimited JSON-RPC 2.0 messages from stdin and writing responses to stdout — the standard MCP transport.

Each tool call directly invokes `api.create_target()` or `api.build()` (the same functions used by `cgn_analyse` / `cgn_build` in the CLI). The CGN analysis graph is retained in memory between calls, so repeated queries on the same target are fast (cache hits).
