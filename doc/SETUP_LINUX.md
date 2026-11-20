# Linux Setup, Build & Execution Guide (Ubuntu 24.04 LTS)

This guide walks through installing prerequisite packages, compiling **OCTIBOT** from source on **Ubuntu 24.04 LTS** (or any modern Debian/Ubuntu distribution), configuring your robot authentication tokens and runtime profile, and launching your robot to play on **`octi.selfso.com`**.

---

## 1. Prerequisite Packages (Ubuntu 24.04 LTS)

OCTIBOT is written in C++17 and uses CMake, POSIX sockets, OpenSSL (for HTTPS/TLS 1.3 connections to `octi.selfso.com:443`), and optional test/database libraries.

Open a terminal and install the required toolchain and development headers:

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    g++ \
    cmake \
    pkg-config \
    git \
    libssl-dev \
    zlib1g-dev \
    libpng-dev \
    libsqlite3-dev \
    libcpputest-dev \
    librapidcheck-dev
```

### Package Summary

| Package | Purpose |
| :--- | :--- |
| `build-essential`, `g++`, `cmake`, `pkg-config` | C++17 compiler (`g++ >= 11`), CMake (`>= 3.16`), and build system |
| `git` | Cloning the `octibot` repository and submodules |
| `libssl-dev` | OpenSSL TLS client support (`OCTI_HAS_OPENSSL`) for `https://octi.selfso.com:443` |
| `zlib1g-dev`, `libpng-dev`, `libsqlite3-dev` | Core engine serialization and transcript utilities |
| `libcpputest-dev`, `librapidcheck-dev` | Unit testing (`CppUTest`) and property-based testing (`RapidCheck`) for `make test` |

---

## 2. Cloning the Repository

Clone the repository and initialize its `third_party/` submodules (`json`, `cpp-httplib`):

```bash
mkdir -p ~/work
cd ~/work
git clone --recurse-submodules https://github.com/samurai0000000/octibot.git
cd octibot
```

If you already cloned the repository without `--recurse-submodules`, initialize the submodules with:

```bash
git submodule update --init --recursive
```

---

## 3. Compiling OCTIBOT

The top-level `Makefile` wraps CMake and automatically parallelizes compilation across all available CPU cores.

### Build Release Binary

```bash
make all
```

This produces the standard starter and reference robot executable in `build/`:

| Binary Path | Description |
| :--- | :--- |
| `build/octibot` | Standard starter & reference robot daemon with alpha-beta engine (`StockAlphaBetaAgent`), 85 tunable heuristic/search parameters, opening book, and single/multi-profile `OctiBotFleetRunner` |

### Run the Verification Test Suite (Optional)

Verify that all rules engine, network protocol, and SDK tests pass on your machine:

```bash
make test
```

For AddressSanitizer + UndefinedBehaviorSanitizer verification during custom algorithm development:

```bash
make test SANITIZE=1
```

---

## 4. Generating & Configuring Your Robot Token (`tokens.conf`)

Every robot connecting to **`octi.selfso.com`** authenticates with a dedicated `octi_bot_<64-hex>` token generated from your human player account.

### Step 4.1 — Create a Robot & Export `tokens.conf`

1. Sign in to **[https://octi.selfso.com/#account](https://octi.selfso.com/#account)** using Google, GitHub, Apple, or Facebook SSO.
2. Scroll down to **Robot Registration & Token Management** and expand the panel.
3. Enter a unique **Robot Handle** (3–24 characters, `[A-Za-z0-9_-]`, e.g., `my_alpha_bot`) and an optional **Description** (e.g., `Custom Negamax + Quiescence bot`), then click **Create Robot & Token**.
   - Each player account can register up to **32 robots**.
4. Copy the generated `octi_bot_...` token immediately, or click **Export tokens.conf** to download your token mapping file.

> See the visual onboarding walkthrough at **[https://octi.selfso.com/robots/](https://octi.selfso.com/robots/)** for annotated screenshots of every step.

### Step 4.2 — Install `~/.config/octibot/tokens.conf`

By default on Linux, `octibot` reads tokens from `~/.config/octibot/tokens.conf` (or `$XDG_CONFIG_HOME/octibot/tokens.conf`). Create the configuration directory with restricted `0700` permissions and save your token file with `0600` permissions:

```bash
mkdir -p -m 0700 ~/.config/octibot
cat > ~/.config/octibot/tokens.conf << 'EOF'
# Format: bot_token_<robot_handle> = <octi_bot_token>
bot_token_my_alpha_bot = octi_bot_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
EOF
chmod 0600 ~/.config/octibot/tokens.conf
```

---

## 5. Configuring `octibot.conf`

Copy the annotated example configuration file into `~/.config/octibot/octibot.conf`:

```bash
cp config/octibot.conf.example ~/.config/octibot/octibot.conf
```

Open `~/.config/octibot/octibot.conf` in your editor and configure your global server settings and robot profile:

```ini
# ==============================================================================
# 1. Server & Authentication
# ==============================================================================
server_host = octi.selfso.com
server_port = 443
server_use_ssl = true

# Default profile to run when --profile is not specified on the CLI
default_profile = my_alpha_bot

# Verbosity: 0=quiet, 1=error, 2=info, 3=verbose
verbosity = info

# ==============================================================================
# 2. Table & Matchmaking Behavior
# ==============================================================================
# Options: auto | join_only | host_only | matchmaking | idle
table_behavior = auto
accept_modes = 2P, 4P, OFK
accept_bases = any
accept_super_prongs = any
accept_edgeless = any
accept_time_control = any
host_mode = 2P
host_bases = 3
host_super_prongs = false
host_edgeless = false
host_time_control_enabled = true
host_initial_ms = 600000
host_increment_ms = 5000
host_allow_robots = true

# ==============================================================================
# 3. Search State Announcements & Move Telemetry
# ==============================================================================
# Post live search telemetry ("depth=D | nodes=N | eval_score=S | elapsed=Tms")
# into the game room chat after every bot move:
announce_move_search_chat = true

# Attach structured telemetry (depth, nodes, score, elapsed_ms) to move HTTP
# submissions so it is permanently recorded in the .octi game archive:
publish_move_telemetry = true

# ==============================================================================
# 4. Robot Profile Definition
# ==============================================================================
[my_alpha_bot]
robot_name = my_alpha_bot
algorithm = alpha_beta
time_budget_ms = 10000
max_search_depth = 64
lazy_smp_threads = 2
use_opening_book = true
```

### Key Configuration Options

- **`robot_name`** & **`tokens.conf`**: `octibot` automatically resolves `bot_token_<robot_name>` from `~/.config/octibot/tokens.conf` (or you can set `bot_token = octi_bot_...` directly, or `bot_token = auto` to claim the first unused token from the pool).
- **`table_behavior`**:
  - `auto` *(recommended)*: Joins any compatible waiting table in the lobby, or hosts a new table if none is available.
  - `host_only`: Immediately creates a waiting table matching `host_mode`, `host_bases`, and `host_time_control_enabled` (or cycles through `host_variants`), and re-hosts after each game finishes.
  - `join_only`: Scans the lobby and joins compatible waiting tables that allow robots (`allow_robots = true`).
  - `matchmaking`: Enters the automated matchmaking queue with `allow_robots = true`.
  - `idle`: Connects to the lobby without autonomously joining or hosting tables.
- **`announce_move_search_chat = true`**: After each move, posts a concise summary of the search state (`depth=8 | nodes=581440 | eval_score=16 | elapsed=10001ms`) to the game room chat so opponents and spectators can follow your algorithm's thinking in real time.
- **`publish_move_telemetry = true`**: Embeds `{"ai": {"depth", "nodes", "score", "elapsed_ms"}}` into the `POST /api/v1/games/{room_id}/moves` payload, storing your algorithm's per-ply statistics in the `.octi` transcript archive.

---

## 6. Executing Your Robot

### 6.1 Run a Single Robot Profile (`octibot`)

```bash
./build/octibot \
    --config ~/.config/octibot/octibot.conf \
    --tokens-file ~/.config/octibot/tokens.conf \
    --profile my_alpha_bot
```

Since `~/.config/octibot/octibot.conf` and `~/.config/octibot/tokens.conf` are the default lookup paths, you can also simply run:

```bash
./build/octibot --profile my_alpha_bot
```

Expected console output upon connecting:

```text
OCTIBOT v1.3.0 (config=/home/user/.config/octibot/octibot.conf, tokens=/home/user/.config/octibot/tokens.conf)
[my_alpha_bot] Connecting to server octi.selfso.com:443...
[my_alpha_bot] Authenticated robot @my_alpha_bot (uuid=..., owner=@your_handle, record=0W/0L/0D)
```

### 6.2 Multi-Profile Fleet & Clone Execution

You can run multiple distinct robot profiles—or multiple concurrent clones of a profile using a pool of tokens—inside a single `octibot` process:

```bash
# Run specific named profiles concurrently:
./build/octibot --profiles stock,bot_fast,bot_deep

# Run all [profile] sections defined in octibot.conf:
./build/octibot --all-profiles

# Spawn 4 concurrent instances of a profile (claims 4 unused tokens from tokens.conf):
./build/octibot --profile my_alpha_bot --instances 4
```

### 6.3 Running Persistently in the Background (`screen` or `systemd`)

To keep your robot connected on a headless Linux server or workstation:

```bash
screen -dmS octibot ./build/octibot --profile my_alpha_bot
# Attach to view live logs:
screen -r octibot
# Gracefully stop the robot (detaches lease cleanly on octi.selfso.com):
# Press Ctrl+C inside the screen session
```

---

## 7. Next Steps

- **[Algorithm Authoring & Porting Guide (`doc/WRITING_BOTS.md`)](WRITING_BOTS.md)**: Learn how to write a custom `OctiBotAgent` in C++ or port an external engine (Python, Rust, C++, neural networks) to play on `octi.selfso.com`.
- **[Robot Wire Protocol Specification (`doc/ROBOT_PROTOCOL.md`)](ROBOT_PROTOCOL.md)**: Complete REST + Server-Sent Events (SSE) wire specification for `octi.selfso.com`.
- **[Windows Setup & Build Guide (`doc/SETUP_WINDOWS.md`)](SETUP_WINDOWS.md)**: Building and running OCTIBOT natively on Windows 10/11 with Visual Studio / MSVC.
