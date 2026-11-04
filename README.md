# OCTIBOT - AI Robot Toolkit & SDK for OCTI

**Version: 1.0.1**

Copyright (C) 2026, Charles Chiou

`octibot` is a cross-platform C++17 robot toolkit, SDK, and fleet runner for the OCTI board game platform. It enables developers, researchers, and AI agents to build, tune, and deploy autonomous or Centaur AI players on OCTI servers.

---

## Features

- **Standard C++17 SDK (`liboctibot`)**:
  - `OctiBotConfigStore`: Zero-dependency, line-oriented `key = value` and `[profile]` section parser supporting arbitrary algorithmic parameters, comments, quoted strings, and token pools.
  - `OctiBotPolicyEngine`: Configurable matchmaking, table discovery, ruleset filtering (2P, 4P, OFK, 1-base vs 3-base, superprongs, edgeless, time controls), and social governance (`friend_list`, `ban_list`, proposals).
  - `OctiBotConversationEngine`: Interactive in-game and lobby chat handler supporting public commands (`@bot help`, `@bot stats`), owner management (`@bot status`, `@bot host`, `@bot set`), and thread-safe inbox buffering for Agentic AI.
  - `OctiBotMcpServer`: Built-in Model Context Protocol (MCP) server exposing 11 specialized tools, enabling Centaur AI agents to inspect state, analyze positions with alpha-beta search, make moves, and converse.
  - `OctiBotAgentCliLauncher`: Managed supervisor for external agent CLIs (`agy`, `claude`, `cursor-agent`) with guaranteed process cleanup.
- **Reference Bot Executables**:
  - `octibot-reference`: Production-grade autonomous bot supporting alpha-beta search, dynamic time allocation, and single-process multi-profile fleet deployment.
  - `octibot-minimal`: Lightweight template demonstrating move generation, evaluation, and custom config parameter access.
  - `octibot-mcp`: Dedicated Centaur MCP mode runner.
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
./build/octibot-reference --profile stock
# Detailed stdout logging is enabled by default (verbosity = verbose).
# Override via CLI: --verbosity <verbose|info|error|quiet>, --verbose / -v, or --quiet / -q
```

**Run multiple profiles concurrently in a single process:**
```bash
./build/octibot-reference --profiles stock,bot_fast
```

**Run all configured profiles:**
```bash
./build/octibot-reference --all-profiles
```

**Run multiple clone instances of a profile:**
```bash
./build/octibot-reference --profile stock --instances 3
```

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
./build/octibot-reference --profile stock --mcp-stdio
```

### Ephemeral / Managed CLI Supervisor:
Launch a managed child CLI agent that connects to an ephemeral loopback MCP port:
```bash
./build/octibot-reference --profile stock --ai-cli agy --ai-prompt "Analyze table chat and assist with tactical move recommendations."
```

Or run interactively in the terminal:
```bash
./build/octibot-reference --profile stock --ai-cli claude --ai-interactive
```

---

## Protocol Reference & Skill Guide

- See `doc/ROBOT_PROTOCOL.md` for complete wire protocol details (HTTP REST + SSE, headers, superseding token lifecycle, and payload schemas).
- See `skills/octibot/SKILL.md` for the autonomous agent skill guide (rules, notation, search heuristics, and MCP tool workflows).
