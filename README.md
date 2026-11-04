# OCTIBOT - AI Robot Toolkit & SDK for OCTI

**Version: 1.0.3**

Copyright (C) 2026, Charles Chiou

`octibot` is a cross-platform C++17 robot toolkit, SDK, and fleet runner for the OCTI board game platform. It enables developers, researchers, and AI agents to build, tune, and deploy autonomous or Centaur AI players on OCTI servers.

---

## Features

- **Standard C++17 SDK (`liboctibot`)**:
  - `OctiBotConfigStore`: Zero-dependency, line-oriented `key = value` and `[profile]` section parser supporting arbitrary algorithmic parameters, comments, quoted strings, and token pools.
  - `OctiBotPolicyEngine`: Configurable matchmaking, table discovery, ruleset filtering (2P, 4P, OFK, 1-base vs 3-base, superprongs, edgeless, time controls), and social governance (`friend_list`, `ban_list`, proposals).
  - `OctiBotConversationEngine`: Interactive in-game and lobby chat handler supporting public commands (`@bot help`, `@bot stats`), owner management (`@bot status`, `@bot host`, `@bot set`), and thread-safe inbox buffering for Agentic AI.
  - `OctiBotMcpServer`: Built-in Model Context Protocol (MCP) server exposing 11 specialized tools, enabling Centaur AI agents to inspect state, analyze positions with alpha-beta search, make moves, and converse.
  - `OctiBotAgentCliLauncher`: Managed supervisor for external agent CLIs (`agy`, `claude`, `cursor-agent`) with guaranteed process cleanup and hardened security sandboxing.
- **Embedded Web Dashboard & `aimon` Fleet Integration**:
  - Embedded HTTP server automatically bound on `web_port = 3888` (configurable, with automatic fallback to ephemeral port if busy).
  - Live satellite tab registration with `aimon` gateway (`port 3885`), rendering an integrated dashboard inside the `aimon` web UI.
  - Real-time OCTI server health and bot telemetry (`GET /api/v1/status`).
  - Thread-safe 5,000-line scrollable `stdout` console viewer (`GET /api/v1/logs`) with monotonic sequence tailing and oldest-line eviction.
  - Interactive categorized configuration editor (`GET /api/v1/config`, `POST /api/v1/config`) with locked read-only network keys and atomic disk persistence.
- **Search Telemetry & Chat Announcements**:
  - `announce_move_search_chat` (default `true`): Automatically announces search depth, visited nodes, evaluation score, and elapsed milliseconds to in-game board chat upon move submission.
- **Hardened Multi-Backend AI CLI Sandboxing**:
  - Isolated execution environment (`/tmp/octibot-ai-sandbox` at `0700` permissions) for `agy`, `claude`, and `cursor-agent`.
  - Zero-trust network field sanitization: strict stripping of control characters and angle brackets to eliminate prompt injection risks.
  - Strict tool restriction flags (`agy --sandbox --settings`, `claude --restricted --strict-mcp-config --tools ""`, `cursor-agent --mode ask --sandbox enabled`) hard-denying external command execution, file modifications, or access outside the game MCP context.
- **Bot Executables**:
  - `octibot`: Production-grade autonomous bot supporting alpha-beta search, dynamic time allocation, single-process multi-profile fleet deployment, Centaur MCP mode, embedded web dashboard, and `aimon` integration.
  - `octibot-minimal`: Standalone, lightweight starter bot binary built with `-DOCTIBOT_MINIMAL=1`, excluding MCP server, `aimon`, and AI CLI subprocess code for minimal environments.
- **Robust Network Protocol**:
  - Superseding token leases with graceful takeover.
  - Seamless active game reconnect upon crash or restart.
  - Server-Sent Events (SSE) streaming with low-latency event notification.
  - Human-only Elo rating invariant.

---

## Quickstart

### 1. Build

Prerequisites:
- C++17 compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- CMake 3.16+
- OpenSSL development libraries (`libssl-dev`)
- CppUTest and RapidCheck (for test execution)

```bash
make all
make test
```

### 2. Register Robots & Export Tokens

1. Navigate to the OCTI web portal (`https://octi.selfso.com` or local server) and log in.
2. Open **Account Settings** $\rightarrow$ **My Registered Robots**.
3. Register up to 32 robot handles (e.g. `stock`, `tactician`, `deep_bot`).
4. Click **Export tokens.conf** (or invoke `GET /api/v1/robots/export-conf`).
5. Save the file to `tokens.conf` or install it into your user config directory:
   - Linux: `~/.config/octibot/tokens.conf` (permissions `0600`)
   - Windows: `%APPDATA%\OCTIBOT\tokens.conf`

Example `tokens.conf`:
```ini
bot_token_stock = octi_bot_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
bot_token_tactician = octi_bot_fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210
```

### 3. Configure `octibot.conf`

Create an `octibot.conf` in the working directory or in `~/.config/octibot/octibot.conf`:

```ini
# Global defaults
server_host = 127.0.0.1
server_port = 8080
server_use_ssl = false
verbosity = verbose         # verbose (default, 3), info (2), error (1), quiet (0)

# Table & gameplay behavior
table_behavior = auto       # auto, join_only, host_only, matchmaking, idle
accept_modes = 2P, 4P, OFK
publish_move_telemetry = false
announce_move_search_chat = true  # Announce search stats in board chat after move

# Embedded Web Dashboard & aimon integration
web_port = 3888                   # Dashboard HTTP port (default 3888)
aimon_enabled = true              # Register with aimon gateway (port 3885)

# Default profile (runs stock alpha-beta engine)
[stock]
algorithm = alpha_beta
time_budget_ms = 1500
max_search_depth = 6

[bot_fast]
robot_name = tactician
algorithm = alpha_beta
time_budget_ms = 500
max_search_depth = 4

[bot_deep]
algorithm = alpha_beta
time_budget_ms = 4000
max_search_depth = 8
```

### 4. Running Bots

**Run a single profile:**
```bash
./build/octibot --profile stock
# Detailed stdout logging is enabled by default (verbosity = verbose).
# Override via CLI: --verbosity <verbose|info|error|quiet>, --verbose / -v, or --quiet / -q
```

**Run multiple profiles concurrently in a single process:**
```bash
./build/octibot --profiles stock,bot_fast
```

**Run all configured profiles:**
```bash
./build/octibot --all-profiles
```

**Run multiple clone instances of a profile:**
```bash
./build/octibot --profile stock --instances 3
```

**Run the standalone lightweight starter bot (`octibot-minimal`):**
```bash
./build/octibot-minimal --profile stock
```

### 5. Embedded Web Dashboard & Fleet Supervision

When `aimon_enabled = true` or `web_port > 0`, `octibot` automatically serves a real-time web dashboard on port `3888` (or an ephemeral fallback port):
- Direct browser access: `http://localhost:3888/`
- Integrated `aimon` supervision: Appears automatically as an **octibot** satellite tab in the `aimon` management console (`http://builder.selfso.com:3883`).
- REST API endpoints:
  - `GET /api/v1/status`: Server reachability, active room status, bot record, and live search engine telemetry.
  - `GET /api/v1/logs?after_seq=<seq>`: Monotonic 5,000-line stdout console stream.
  - `GET /api/v1/config` & `POST /api/v1/config`: Live categorized configuration inspector and hot-reloader.

---

## Writing a Custom C++ Bot

Custom AI engines can be registered with `OctiBotAgentRegistry`:

```cpp
#include "OctiBotAgent.hxx"
#include "OctiBotRunner.hxx"

class MyCustomAgent : public octi::OctiBotAgent {
public:
    void onConfigure(const octi::OctiBotConfigStore &config) override {
        _aggression = config.getDouble("my_custom_aggression", 1.0);
    }

    std::string computeMove(const octi::OctiBoard &board,
                            const octi::GameViewData &game,
                            int mySeat,
                            std::int64_t remainingClockMs,
                            octi::BotMoveTelemetry *outTelemetry) override {
        std::vector<octi::OctiMove> legalMoves = board.getLegalMoves(mySeat);
        if (legalMoves.empty()) {
            return "";
        }
        // Choose best move according to custom heuristics...
        return legalMoves.front().toMoveNotation();
    }

private:
    double _aggression = 1.0;
};

int main(int argc, char **argv) {
    octi::OctiBotAgentRegistry::instance().registerAgent("my_algo",
        [](const octi::OctiBotConfigStore &config) {
            auto agent = std::make_unique<MyCustomAgent>();
            agent->onConfigure(config);
            return agent;
        });

    return octi::runFleetCli(argc, argv);
}
```

---

## Centaur Agentic AI & MCP Integration

`octibot` provides native Model Context Protocol (MCP) server support, allowing Large Language Model agents (Claude Code, Google Antigravity, Cursor) to supervise or play OCTI games interactively:

### Standard I/O MCP Mode:
```bash
./build/octibot --profile stock --mcp-stdio
```

### Ephemeral / Managed CLI Supervisor:
Launch a managed child CLI agent that connects to an ephemeral loopback MCP port:
```bash
./build/octibot --profile stock --ai-cli agy --ai-prompt "Analyze table chat and assist with tactical move recommendations."
```

Or run interactively in the terminal:
```bash
./build/octibot --profile stock --ai-cli claude --ai-interactive
```

---

## Protocol Reference & Skill Guide

- See `doc/ROBOT_PROTOCOL.md` for complete wire protocol details (HTTP REST + SSE, headers, superseding token lifecycle, and payload schemas).
- See `skills/octibot/SKILL.md` for the autonomous agent skill guide (rules, notation, search heuristics, and MCP tool workflows).
