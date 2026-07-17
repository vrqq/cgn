// CGN MCP Server (mcp_server.cpp)
// Implements the Model Context Protocol (MCP) 2024-11-05 over stdin/stdout JSON-RPC 2.0.
//
// These map 1:1 to cgn CLI arguments so the same cgn_setup.cgn.cc logic applies.
//
// MCP tools exposed:
//   build     -- Analyse and build a target
//   query     -- Query target info and resolved configuration
//   list_configs -- List all named configurations from cgn_setup.cgn.cc
//   

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <functional>
#include <stdexcept>

#include "../json/single_include/nlohmann/json.hpp"
#include "../v1/cgn_api.h"

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

// Write a standard MCP stdio frame to stdout.
static void send_response(const json &resp) {
    const std::string payload = resp.dump();
    std::cout << "Content-Length: " << payload.size() << "\r\n\r\n"
              << payload;
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

static std::string strip_cr(std::string line) {
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    return line;
}

static bool read_message(json &req) {
    std::string line;

    while (std::getline(std::cin, line)) {
        line = strip_cr(std::move(line));
        if (line.empty())
            continue;

        if (line.rfind("Content-Length:", 0) == 0) {
            std::size_t content_length = 0;
            try {
                content_length = std::stoul(line.substr(std::string("Content-Length:").size()));
            } catch (...) {
                throw std::runtime_error{"Invalid Content-Length header."};
            }

            // Consume headers until the blank line that separates them from the body.
            while (std::getline(std::cin, line)) {
                line = strip_cr(std::move(line));
                if (line.empty())
                    break;
            }

            std::string body(content_length, '\0');
            std::cin.read(body.data(), static_cast<std::streamsize>(content_length));
            if (static_cast<std::size_t>(std::cin.gcount()) != content_length)
                throw std::runtime_error{"Unexpected end of input while reading framed MCP message."};

            req = json::parse(body);
            return true;
        }

        req = json::parse(line);
        return true;
    }

    return false;
}

// ─── Tool definitions ────────────────────────────────────────────────────────

static json make_tools_list() {
    return json::array({
        {
            {"name", "cgn_analyse"},
            {"description",
             "Analyse a CGN target without building it. Returns the resolved configuration "
             "and target analysis result."},
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
            {"name", "cgn_build"},
            {"description",
             "Analyse and build a CGN target with a named configuration from cgn_setup.cgn.cc. "
             "Returns the primary output path on success."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"target_label", {
                        {"type", "string"},
                        {"description", "Factory label, e.g. '@cell//dir:name'"}
                    }},
                    {"config_name", {
                        {"type", "string"},
                        {"description", "Required named configuration from cgn_setup.cgn.cc, e.g. 'debug' or 'release'"}
                    }}
                }},
                {"required", json::array({"target_label", "config_name"})}
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
             "List every named configuration defined by cgn_setup.cgn.cc and its key-value settings."},
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
    if (cfg_name.empty())
        throw std::runtime_error{"cgn_build requires a non-empty config_name."};

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
    auto names = api.list_configs();
    std::ostringstream out;
    out << "Named configurations in cgn_setup.cgn.cc:\n\n";

    for (const auto &name : names) {
        auto [cfg, anode] = api.query_config(name);
        if (anode) {
            out << "[" << name << "]\n";
            for (auto &kv : cfg)
                out << "  " << kv.first << " = " << kv.second << "\n";
            out << "\n";
        }
    }

    if (names.empty())
        out << "None. Define at least one configuration in cgn_setup.cgn.cc.\n";

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
              << "  Compile this file by using 'ninja -f build_<os>.ninja'\n"
              << "Options:\n"
              << "  --cgn_out <dir>      Output directory (default: cgn-out)\n"
              << "  -C <dir>             Alias for --cgn_out\n"
              << "  --halt_on_error      Abort on any analysis error\n"
              << "  --verbose / -V       Verbose output\n"
              << "  --scriptcc <path>    C++ compiler for .cgn.cc files\n"
              << "  --winenv             Load MSVC environment (Windows)\n\n"
              << "There are other custom cli options work for cgn_setup.cgn.cc which not listed here, \n"
              << "like --target <target_name> to judge configs['DEFAULT'].\n\n" 
              << "Example MCP config:\n"
              << "  {\n"
              << "    \"mcpServers\": {\n"
              << "      \"cgn\": {\n"
              << "        \"command\": \"./@cgn.d/build_linuxd/cgn\",\n"
              << "        \"args\": [\"--cgn_out\", \"cgn-out\", \"--halt_on_error\", \"mcp\"]\n"
              << "      }\n"
              << "    }\n"
              << "  }\n";
    return 1;
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int mcp_server()
{
    // MCP main loop — accept standard framed stdio messages, but keep the
    // raw JSON line format working for direct shell probes.
    while (true) {
        json req;
        try {
            if (!read_message(req))
                break;
            handle_request(req);
        }
        catch (const json::parse_error &e) {
            // Send parse error response (id is unknown so use null)
            send_error(nullptr, -32700, std::string("Parse error: ") + e.what());
        }
    }

    return 0;
}
