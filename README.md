# OCTIBOT - AI Robot Starter Toolkit & SDK for OCTI

**Version: 1.5.0**

Copyright (C) 2026, Charles Chiou

`octibot` is a cross-platform C++17 robot starter toolkit, SDK, and fleet runner for the OCTI board game platform. It enables developers and researchers to build, tune, and deploy custom autonomous AI players on OCTI servers (`https://octi.selfso.com`).

---

## Documentation, Setup & Developer Guides

| Guide | Description |
| :--- | :--- |
| **[Linux Setup & Build Guide (`doc/SETUP_LINUX.md`)](doc/SETUP_LINUX.md)** | Step-by-step installation of prerequisite packages on **Ubuntu 24.04 LTS**, compiling `octibot`, configuring `~/.config/octibot/tokens.conf` & `octibot.conf`, and connecting to `octi.selfso.com`. |
| **[Windows Setup & Build Guide (`doc/SETUP_WINDOWS.md`)](doc/SETUP_WINDOWS.md)** | Native **Windows 10/11** build guide with Visual Studio (MSVC) & CMake, configuring `%APPDATA%\OCTIBOT\tokens.conf` & `octibot.conf`, and running `octibot.exe`. |
| **[Algorithm Authoring & Porting Guide (`doc/WRITING_BOTS.md`)](doc/WRITING_BOTS.md)** | Deep technical guide on writing custom `OctiBotAgent` subclasses, fast `captureSnapshot`/`restoreSnapshot` tree search, emitting search telemetry in table chat, and porting Python/Rust/ONNX engines. |
| **[Robot Wire Protocol Specification (`doc/ROBOT_PROTOCOL.md`)](doc/ROBOT_PROTOCOL.md)** | Complete REST + Server-Sent Events (SSE) protocol specification (`X-Octi-Robot-Lease`, `octi_bot_` tokens, move telemetry, and Human-Only Elo protection). |
| **[Bring Your 'Bot! Onboarding Portal (`https://octi.selfso.com/robots/`)](https://octi.selfso.com/robots/)** | Visual walkthrough of robot registration, token generation, win/loss statistics, and live table chat search state broadcasting. |

---

## Features

- **Standard C++17 SDK (`liboctibot`)**:
  - `OctiBotConfigStore`: Zero-dependency, line-oriented `key = value` and `[profile]` section parser supporting arbitrary algorithmic parameters, comments, quoted strings, and `tokens.conf` token pools.
  - `OctiBotPolicyEngine`: Configurable matchmaking, table discovery, ruleset filtering (2P, 4P, OFK, 1-base vs 3-base, superprongs, edgeless, time controls, `host_variants`), and social governance (`friend_list`, `ban_list`, draw/undo/pause/rematch proposals).
  - `OctiBotAgent` & `StockAlphaBetaAgent`: Clean virtual agent interface (`onConfigure`, `onConnected`, `onLobbyChat`, `onGameStarted`, `computeMove`, `onProposal`, `onGameChat`, `onGameOver`, `supportsVariant`, `resourceRequirements`, `setSearchAffinityPlan`) paired with `OctiBotAgentRegistry` and a ready-to-play alpha-beta search agent (`OctiBotEngine`, Lazy SMP, transposition table, opening book, `OctiLeafEvaluator` hooks, and 86 configurable heuristic/search parameters).
  - `OctiBotRunner` & `OctiBotFleetRunner`: Full connection lifecycle manager handling authentication, token-pool claiming, SSE lobby/room event loops, turn dispatch, automatic reconnect, and single-process multi-profile or multi-instance supervision.
- **Search Telemetry & Chat Announcements**:
  - `announce_move_search_chat` (default `true`): Automatically announces search depth, visited nodes, evaluation score, and elapsed milliseconds to in-game board chat upon move submission.
  - `publish_move_telemetry` (default `false`): Embeds structured search telemetry (`depth`, `nodes`, `score`, `elapsed_ms`) into move submissions for permanent `.octi` transcript archiving.
- **Starter Executable (`octibot`)**:
  - Single standard `octibot` target (`examples/reference_bot/Main.cxx`) backed by `StockAlphaBetaAgent` (`algorithm = alpha_beta`), ready to play out of the box or serve as the entry point for custom `OctiBotAgent` subclasses.
- **Robust Network Protocol**:
  - Superseding token leases with graceful takeover (`--supersede` / `allow_supersede`) and standby reclaim.
  - Seamless active game reconnect upon crash or restart.
  - Server-Sent Events (SSE) streaming with low-latency event notification.
  - Human-only Elo rating invariant.

---

## Quickstart

### 1. Build

Prerequisites:
- C++17 compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- CMake 3.16+
- Python 3 (`python3` or `python` on `PATH`, used at CMake configure time to generate rulebook headers)
- OpenSSL development libraries (`libssl-dev` on Linux; native WinHTTP on Windows)
- CppUTest, RapidCheck, `libsqlite3-dev`, and a sibling `../octi/server` checkout (optional, for running the Linux `test_octibot_sdk` qualification suite)

```bash
make all
# Optional (requires sibling ../octi/server checkout + CppUTest/RapidCheck):
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

Create an `octibot.conf` in the working directory or in `~/.config/octibot/octibot.conf` (Linux) / `%APPDATA%\OCTIBOT\octibot.conf` (Windows):

```ini
# Global defaults
server_host = 127.0.0.1
server_port = 8080
server_use_ssl = false
verbosity = verbose               # verbose (default, 3), info (2), error (1), quiet (0)

# Table & gameplay behavior
table_behavior = auto             # auto, join_only, host_only, matchmaking, idle
accept_modes = 2P, 4P, OFK
publish_move_telemetry = false
announce_move_search_chat = true  # Announce search stats in board chat after move

# Default profile (runs stock alpha-beta engine)
[stock]
robot_name = stock
algorithm = alpha_beta
time_budget_ms = 1500
max_search_depth = 6

[bot_fast]
robot_name = tactician
algorithm = alpha_beta
time_budget_ms = 500
max_search_depth = 4

[bot_deep]
robot_name = bot_deep
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
# Custom config/token paths: --config <path> --tokens-file <path>
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

---

## Writing a Custom C++ Bot

Custom AI engines subclass `octi::OctiBotAgent` and register with `OctiBotAgentRegistry`:

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
        (void) game;
        (void) mySeat;
        (void) remainingClockMs;
        const octi::OctiGame *g = board.getGame();
        if (!g) {
            return "";
        }
        std::vector<octi::OctiMove> legalMoves = g->generateLegalMoves();
        if (legalMoves.empty()) {
            return "";
        }
        // Populate outTelemetry to broadcast search depth/nodes/score in table chat
        if (outTelemetry) {
            outTelemetry->depthReached = 1;
            outTelemetry->nodesVisited = static_cast<std::int64_t>(legalMoves.size());
            outTelemetry->evalScore    = 0;
            outTelemetry->elapsedMs    = 1;
            outTelemetry->present      = true;
        }
        return legalMoves.front().asString();
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

## Protocol & Developer References

- See **[`doc/WRITING_BOTS.md`](doc/WRITING_BOTS.md)** for a complete walk-through of `OctiBotAgent`, fast `captureSnapshot`/`restoreSnapshot` tree search, `StockAlphaBetaAgent` customization, and external engine integration.
- See **[`doc/ROBOT_PROTOCOL.md`](doc/ROBOT_PROTOCOL.md)** for complete wire protocol details (HTTP REST + SSE, headers, superseding token lifecycle, and payload schemas).
