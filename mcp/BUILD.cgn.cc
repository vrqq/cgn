// CGN MCP Server
// Exposes CGN analysis and build capabilities as a Model Context Protocol server.
// Arguments (--target, --cgn-out, etc.) are written in the MCP client config file.
//
// Build:   ./debug.sh @cgn.d//mcp:cgn_mcp
// Install: copy build output to your PATH or reference directly in mcp config

#include <cgn>

cxx_executable("cgn_mcp", x) {
    x.srcs = {"mcp_server.cpp"};
    x.include_dirs = {"../v1"};
    // x.defines = {"CGN_EXE_IMPLEMENT"};

    // Link against the CGN core library (same as cgn executable)
    x.add_dep("@cgn.d//:cgn_static", cxx::private_dep);

    // nlohmann/json for MCP JSON-RPC parsing (header-only)
    x.add_dep("@third_party//nlohmann_json", cxx::private_dep);

    if (x.cfg["os"] == "mac")
        x.ldflags = {"-Wl,-undefined,dynamic_lookup"};
}

alias("mcp", x) {
    x.actual_label = ":cgn_mcp";
    x.load_named_config("host_release");
}