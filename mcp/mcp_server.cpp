// CGN MCP Server (mcp_server.cpp)
// Implements the Model Context Protocol (MCP) 2024-11-05 over stdin/stdout JSON-RPC 2.0.
//
// Startup arguments (written once in the MCP client config file, NOT typed each time):
//   cgn_mcp --target llvm,debug,asan --cgn-out cgn-out [--halt_on_error] [--verbose]
//
// These map 1:1 to cgn CLI arguments so the same cgn_setup.cgn.cc logic applies.
//
// MCP tools exposed:
//   cgn_analyse   -- Analyse a target (no build)
//   cgn_build     -- Analyse and build a target
//   cgn_query     -- Query target info and resolved configuration
//   cgn_list_configs -- List all named configurations from cgn_setup.cgn.cc

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include "cgn_api.h"
#include "logger.h"

using json = nlohmann::json;

// ─── Helpers ────────────────────────────────────────────────────────────────

static std::string cfg_to_string(const cgnv1::Configuration &cfg) {
    std::ostringstream oss;
    bool first = true;
    for (auto &kv : cfg) {
        if (!first) oss << ", ";
        oss << kv.first << "=" << kv.second;
        first = false;
    }
    return oss.str();
}

// Validate that label characters are safe before passing to api.
// Labels must match: @?[A-Za-z0-9_.@/-]+ with ':' separator only once after '//'
static bool is_safe_label(const std::string &label) {
    if (label.empty() || label.size() > 512) return false;
    for (char c : label) {
        if (!std::isalnum((unsigned char)c) &&
            c != '_' && c != '-' && c != '.' &&
            c != '/' && c != ':' && c != '@')
            return false;
    }
    return true;
}

// ─── MCP Protocol ────────────────────────────────────────────────────────────

static const std::string MCP_PROTOCOL_VERSION = "2024-11-05";
static const std::string SERVER_NAME    = "cgn-mcp";
static const std::string SERVER_VERSION = "1.0.0";

// Write a JSON-RPC response to stdout (newline-delimited)
static void send_response(const json &resp) {
    std::cout << resp.dump() << "\n";
    std::cout.flush();
}

static void send_result(const json &id, const json &result) {
    send_response({
        {"jsonrpc", "2.0"},
        {"id",      id},
        {"result",  result}
    });
}

static void send_error(const json &id, int code, const std::string &message) {
    send_response({
        {"jsonrpc", "2.0"},
        {"id",      id},
        {"error",   {{"code", code}, {"message", message}}}
    });
}

// ─── Tool definitions ────────────────────────────────────────────────────────

static json make_tools_list() {
    return json::array({
        {
            {"name", "cgn_analyse"},
            {"description",
             "Analyse a CGN target (loads BUILD.cgn.cc, resolves deps, generates build.ninja) "
             "without executing ninja. Use this to inspect the dependency graph and output "
             "info (CxxInfo, LinkAndRunInfo, etc.) of any target."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"target_label", {
                        {"type", "string"},
                        {"description", "Factory label, e.g. '@cell//dir:name' or ':local_target'"}
                    }},
                    {"config_name", {
                        {"type", "string"},
                        {"description", "Named config from cgn_setup.cgn.cc (default: 'DEFAULT')"}
                    }}
                }},
                {"required", json::array({"target_label"})}
            }}
        },
        {
            {"name", "cgn_build"},
            {"description",
             "Analyse and build a CGN target. Equivalent to running './debug.sh @cell//target'. "
             "Returns the path(s) of the primary output files on success."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"target_label", {
                        {"type", "string"},
                        {"description", "Factory label, e.g. '@cell//dir:name'"}
                    }},
                    {"config_name", {
                        {"type", "string"},
                        {"description", "Named config from cgn_setup.cgn.cc (default: 'DEFAULT')"}
                    }}
                }},
                {"required", json::array({"target_label"})}
            }}
        },
        {
            {"name", "cgn_query"},
            {"description",
             "Query a CGN target: shows the resolved Configuration (all key=value pairs "
             "after trimming) and the full analysis result (output files, info table contents). "
             "Useful for debugging why a target gets unexpected compiler flags or paths."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"target_label", {
                        {"type", "string"},
                        {"description", "Factory label to query"}
                    }},
                    {"config_name", {
                        {"type", "string"},
                        {"description", "Named config from cgn_setup.cgn.cc (default: 'DEFAULT')"}
                    }}
                }},
                {"required", json::array({"target_label"})}
            }}
        },
        {
            {"name", "cgn_list_configs"},
            {"description",
             "List all named configurations defined in cgn_setup.cgn.cc, "
             "showing each key-value pair. Use this to discover available config names "
             "(e.g. 'DEFAULT', 'host_release') and their exact settings."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {}},
                {"required", json::array()}
            }}
        }
    });
}

// ─── Configuration loading ───────────────────────────────────────────────────

static cgnv1::Configuration load_config(const std::string &name) {
    auto [cfg, anode] = api.query_config(name.empty() ? "DEFAULT" : name);
    if (!anode)
        throw std::runtime_error{"Config '" + (name.empty() ? "DEFAULT" : name) + "' not found."};
    return cfg;
}

// ─── Tool dispatch ───────────────────────────────────────────────────────────

static json tool_analyse(const json &params) {
    std::string label      = params.value("target_label", "");
    std::string cfg_name   = params.value("config_name", "");

    if (!is_safe_label(label))
        throw std::runtime_error{"Invalid target_label: contains unsafe characters."};

    auto cfg = load_config(cfg_name);
    auto rv  = api.create_target(label, cfg);

    std::ostringstream out;
    out << "Target: " << label << "\n";
    out << "Config: " << cfg_to_string(cfg) << "\n\n";
    out << "Analysis result:\n" << rv.to_string('h');
    if (!rv.errmsg.empty())
        out << "\n\nError:\n" << rv.errmsg;

    return {{"content", json::array({{{"type","text"}, {"text", out.str()}}})}};
}

static json tool_build(const json &params) {
    std::string label    = params.value("target_label", "");
    std::string cfg_name = params.value("config_name", "");

    if (!is_safe_label(label))
        throw std::runtime_error{"Invalid target_label: contains unsafe characters."};

    auto cfg   = load_config(cfg_name);
    auto exe   = api.build(label, cfg);

    std::ostringstream out;
    out << "Build succeeded: " << label << "\n";
    out << "Config: " << cfg_to_string(cfg) << "\n";
    if (!exe.empty())
        out << "Primary output: " << exe << "\n";

    return {{"content", json::array({{{"type","text"}, {"text", out.str()}}})}};
}

static json tool_query(const json &params) {
    std::string label    = params.value("target_label", "");
    std::string cfg_name = params.value("config_name", "");

    if (!is_safe_label(label))
        throw std::runtime_error{"Invalid target_label: contains unsafe characters."};

    auto cfg = load_config(cfg_name);
    auto rv  = api.create_target(label, cfg);

    std::ostringstream out;
    out << "=== Target ===\n" << label << " (input config: " << cfg.get_id() << ")\n";
    out << "\n=== Input Configuration ===\n" << cfg_to_string(cfg) << "\n";
    out << "\n=== Analysis Result ===\n" << rv.to_string('H');
    if (!rv.errmsg.empty())
        out << "\n\n=== Error ===\n" << rv.errmsg;

    return {{"content", json::array({{{"type","text"}, {"text", out.str()}}})}};
}

static json tool_list_configs(const json &/*params*/) {
    // Query a few known config names; report what's available.
    std::vector<std::string> candidates{"DEFAULT", "host_release"};
    std::ostringstream out;
    out << "Named configurations in cgn_setup.cgn.cc:\n\n";

    for (auto &name : candidates) {
        auto [cfg, anode] = api.query_config(name);
        if (anode) {
            out << "[" << name << "]\n";
            for (auto &kv : cfg)
                out << "  " << kv.first << " = " << kv.second << "\n";
            out << "\n";
        }
    }

    return {{"content", json::array({{{"type","text"}, {"text", out.str()}}})}};
}

// ─── Request handling ─────────────────────────────────────────────────────────

static void handle_request(const json &req) {
    json id = req.contains("id") ? req["id"] : json(nullptr);
    std::string method = req.value("method", "");
    json params = req.value("params", json::object());

    // Notification (no id) → no response required
    bool is_notification = !req.contains("id");

    try {
        if (method == "initialize") {
            if (is_notification) return;
            send_result(id, {
                {"protocolVersion", MCP_PROTOCOL_VERSION},
                {"capabilities",    {{"tools", {{"listChanged", false}}}}},
                {"serverInfo",      {{"name", SERVER_NAME}, {"version", SERVER_VERSION}}}
            });
        }
        else if (method == "notifications/initialized") {
            // Acknowledgement from client; no response needed.
        }
        else if (method == "tools/list") {
            if (is_notification) return;
            send_result(id, {{"tools", make_tools_list()}});
        }
        else if (method == "tools/call") {
            if (is_notification) return;
            std::string name    = params.value("name", "");
            json        args    = params.value("arguments", json::object());
            json        result;

            if      (name == "cgn_analyse")      result = tool_analyse(args);
            else if (name == "cgn_build")         result = tool_build(args);
            else if (name == "cgn_query")         result = tool_query(args);
            else if (name == "cgn_list_configs")  result = tool_list_configs(args);
            else throw std::runtime_error{"Unknown tool: " + name};

            send_result(id, result);
        }
        else if (method == "ping") {
            if (!is_notification) send_result(id, json::object());
        }
        else {
            if (!is_notification)
                send_error(id, -32601, "Method not found: " + method);
        }
    }
    catch (const std::exception &e) {
        if (!is_notification)
            send_error(id, -32000, std::string(e.what()));
    }
}

// ─── CLI argument parsing ─────────────────────────────────────────────────────

static int show_help(const char *arg0) {
    std::cerr << arg0 << " [options]\n"
              << "  CGN MCP server — write these args in your MCP client config file.\n\n"
              << "  Compile this file by using 'cgn --some-args build @cgn.d//mcp'\n"
              << "Options:\n"
              << "  --target <tokens>    Comma-separated config tokens (e.g. llvm,debug,asan)\n"
              << "  --cgn-out <dir>      Output directory (default: cgn-out)\n"
              << "  -C <dir>             Alias for --cgn-out\n"
              << "  --halt_on_error      Abort on any analysis error\n"
              << "  --verbose / -V       Verbose output\n"
              << "  --scriptcc <path>    C++ compiler for .cgn.cc files\n"
              << "  --winenv             Load MSVC environment (Windows)\n\n"
              << "Example MCP config:\n"
              << "  {\n"
              << "    \"mcpServers\": {\n"
              << "      \"cgn\": {\n"
              << "        \"command\": \"./cgn-out/obj/@cgn.d_/mcp_/cgn_mcp_FFFF9222/cgn_mcp\",\n"
              << "        \"args\": [\"--target\", \"llvm,debug,asan\", \"--cgn-out\", \"cgn-out\"]\n"
              << "      }\n"
              << "    }\n"
              << "  }\n";
    return 1;
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
    // Parse arguments — same style as cli.cpp
    std::unordered_set<std::string> single_opts{"halt_on_error", "verbose", "winenv", "scriptcc_debug"};
    std::unordered_map<std::string, std::string> kvargs;

    for (int i = 1; i < argc;) {
        std::string k{argv[i]};
        if (k.size() >= 2 && k[0] == '-') {
            k = (k[1] == '-') ? k.substr(2) : k.substr(1);
            if (k == "C") k = "cgn-out";
            if (k == "V") k = "verbose";

            if (single_opts.count(k)) { kvargs[k] = ""; i++; }
            else if (i + 1 < argc)   { kvargs[k] = argv[i+1]; i += 2; }
            else                     { return show_help(argv[0]); }
        }
        else { i++; } // ignore positional args
    }

    if (!kvargs.count("cgn-out"))
        kvargs["cgn-out"] = "cgn-out";
    
    // enable mcp_mode
    kvargs["mcp_mode"] = "";

    // Initialize CGN
    auto api_guard = std::shared_ptr<int>(new int,
        [](int *p){ api.release(); delete p; });

    try {
        api.init(kvargs);
    }
    catch (const std::exception &e) {
        std::cerr << "cgn_mcp: init failed: " << e.what() << "\n";
        return 1;
    }

    api.logger->println("CGN mcp ready", "");

    // MCP main loop — read newline-delimited JSON from stdin
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        try {
            json req = json::parse(line);
            handle_request(req);
        }
        catch (const json::parse_error &e) {
            // Send parse error response (id is unknown so use null)
            send_error(nullptr, -32700, std::string("Parse error: ") + e.what());
        }
    }

    return 0;
}
