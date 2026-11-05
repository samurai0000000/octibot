# Windows Setup, Build & Execution Guide (Windows 10 / 11)

This guide walks through installing prerequisite development tools on **Windows 10 or Windows 11**, compiling **OCTIBOT** natively with **MSVC (Visual Studio 2022 or 2026)** and **CMake**, configuring your robot authentication tokens and runtime profile, and launching your robot to connect to **`octi.selfso.com`**.

---

## 1. Prerequisite Software (Windows 10 / 11)

On Windows, OCTIBOT compiles cleanly with zero external library dependencies: HTTPS/TLS 1.3 communication with `octi.selfso.com:443` uses the native Windows **WinHTTP** and **Winsock2** system libraries (`winhttp.lib`, `ws2_32.lib`) built into Windows.

### Required Tools

1. **Visual Studio 2022 or 2026** (Community, Professional, Enterprise, or **Build Tools for Visual Studio**)
   - During installation in the **Visual Studio Installer**, select the **"Desktop development with C++"** workload (includes the MSVC v143/v145 C++17 compiler and Windows 10/11 SDK).
2. **CMake** (`>= 3.20`)
   - Included with the Visual Studio C++ workload, or installable via Winget (`winget install Kitware.CMake`) or from [cmake.org](https://cmake.org/download/).
3. **Git for Windows**
   - Install via Winget (`winget install Git.Git`) or from [git-scm.com](https://git-scm.com/download/win).

### One-Line Winget Installation (PowerShell)

If you prefer installing the prerequisites from an elevated PowerShell prompt using `winget`:

```powershell
winget install --id Git.Git -e
winget install --id Kitware.CMake -e
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

---

## 2. Cloning the Repository

Open **PowerShell** (or **x64 Native Tools Command Prompt for VS**) and clone `octibot` along with its `third_party/` submodules:

```powershell
New-Item -ItemType Directory -Force -Path "$HOME\work" | Out-Null
Set-Location "$HOME\work"
git clone --recurse-submodules https://github.com/samurai0000000/octibot.git
Set-Location "$HOME\work\octibot"
```

If you already cloned the repository without `--recurse-submodules`, initialize the submodules with:

```powershell
git submodule update --init --recursive
```

---

## 3. Compiling OCTIBOT with CMake & MSVC

From the `octibot` root directory in PowerShell, configure the CMake build tree and compile the `Release` configuration:

```powershell
cmake -B build
cmake --build build --config Release --parallel
```

> **Note:** CMake automatically detects your installed Visual Studio version (`Visual Studio 17 2022` or `Visual Studio 18 2026`) and targets `x64` by default.

### Compiled Executables

Once compilation finishes, the Windows executables are located at:

| Binary Path | Description |
| :--- | :--- |
| `build\Release\octibot.exe` | Full-featured reference robot daemon (`StockAlphaBetaAgent`, 46 tunable parameters, opening book, multi-profile fleet runner, embedded Web Dashboard at `http://localhost:3888`, and Centaur MCP server) |
| `build\Release\octibot-minimal.exe` | Lightweight starter robot (`MinimalStarterAgent`) with zero MCP/web dependencies—ideal as a clean starting point for custom C++ bots |

---

## 4. Generating & Configuring Your Robot Token (`tokens.conf`)

Every robot connecting to **`octi.selfso.com`** authenticates with a dedicated `octi_bot_<64-hex>` token generated from your human player account.

### Step 4.1 — Create a Robot & Export `tokens.conf`

1. Sign in to **[https://octi.selfso.com/#account](https://octi.selfso.com/#account)** using Google, GitHub, Apple, or Facebook SSO.
2. Scroll down to **Robot Registration & Token Management** and expand the panel.
3. Enter a unique **Robot Handle** (3–24 characters, `[A-Za-z0-9_-]`, e.g., `my_win_bot`) and an optional **Description** (e.g., `Windows C++17 custom search bot`), then click **Create Robot & Token**.
   - Each player account can register up to **32 robots**.
4. Copy the generated `octi_bot_...` token immediately, or click **Export tokens.conf** to download your token mapping file.

> See the visual onboarding walkthrough at **[https://octi.selfso.com/robots/](https://octi.selfso.com/robots/)** for annotated screenshots of every step.

### Step 4.2 — Create `%APPDATA%\OCTIBOT\tokens.conf`

On Windows, `octibot.exe` automatically looks for configuration files in `%APPDATA%\OCTIBOT\` (which expands to `C:\Users\<YourUsername>\AppData\Roaming\OCTIBOT\`).

Create the directory and write `%APPDATA%\OCTIBOT\tokens.conf` in PowerShell:

```powershell
New-Item -ItemType Directory -Force -Path "$env:APPDATA\OCTIBOT" | Out-Null

@"
# Format: <robot_handle> = <octi_bot_token>
my_win_bot = octi_bot_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
"@ | Set-Content -Encoding UTF8 "$env:APPDATA\OCTIBOT\tokens.conf"
```

---

## 5. Configuring `octibot.conf`

Copy the annotated example configuration file to `%APPDATA%\OCTIBOT\octibot.conf`:

```powershell
Copy-Item "config\octibot.conf.example" "$env:APPDATA\OCTIBOT\octibot.conf"
notepad "$env:APPDATA\OCTIBOT\octibot.conf"
```

Update `%APPDATA%\OCTIBOT\octibot.conf` with your server settings and robot profile:

```ini
# ==============================================================================
# 1. Server & Authentication
# ==============================================================================
server_host = octi.selfso.com
server_port = 443
server_use_ssl = true

# Default profile to run when --profile is not specified on the CLI
default_profile = my_win_bot

# Verbosity: 0=quiet, 1=error, 2=info, 3=debug
verbosity = 2

# ==============================================================================
# 2. Table & Matchmaking Behavior
# ==============================================================================
# Options: host_only | join_waiting | matchmake | host_or_join | passive
table_behavior = host_or_join
preferred_modes = 2P
preferred_bases = 3
preferred_super_prongs = false
preferred_edgeless = false
preferred_time_control = true
preferred_initial_ms = 600000
preferred_increment_ms = 5000
allow_robots_on_hosted_table = true

# ==============================================================================
# 3. Search State Announcements & Move Telemetry
# ==============================================================================
# Post live search telemetry ("depth=D | nodes=N | eval_score=S | elapsed=Tms")
# into the game room chat after every bot move:
announce_move_search_chat = true

# Attach structured telemetry (depth, nodes, score, elapsed_ms) to move HTTP
# submissions so it is permanently recorded in the .octi game archive:
publish_move_telemetry = true

# Allow human players to query "@<bot_handle> eval" or "status" in chat:
allow_public_eval_chat = true

# ==============================================================================
# 4. Robot Profile Definition
# ==============================================================================
[profile.my_win_bot]
robot_name = my_win_bot
bot_token = auto
algorithm = alpha_beta
difficulty = 3
time_budget_ms = 10000
max_search_depth = 64
lazy_smp_threads = 2
use_opening_book = true
web_port = 3888
web_bind = 127.0.0.1
```

---

## 6. Executing Your Robot on Windows

### 6.1 Run a Single Robot Profile (`octibot.exe`)

From PowerShell in the `octibot` repository root:

```powershell
.\build\Release\octibot.exe --profile my_win_bot
```

Or specify explicit paths to `octibot.conf` and `tokens.conf`:

```powershell
.\build\Release\octibot.exe `
    --config "$env:APPDATA\OCTIBOT\octibot.conf" `
    --tokens "$env:APPDATA\OCTIBOT\tokens.conf" `
    --profile my_win_bot
```

Expected console output:

```text
OCTIBOT v1.1.0 (config=C:\Users\user\AppData\Roaming\OCTIBOT\octibot.conf, tokens=C:\Users\user\AppData\Roaming\OCTIBOT\tokens.conf)
[2026-11-05 11:05:10] [my_win_bot] Connected to octi.selfso.com:443 as @my_win_bot (owner: @your_handle, record: 0W-0L-0D)
[2026-11-05 11:05:11] [my_win_bot] Hosted new 2P table Room #1048 (3 bases, 10+5 clock, allow_robots=true)
```

### 6.2 Run the Minimal Starter Bot (`octibot-minimal.exe`)

```powershell
.\build\Release\octibot-minimal.exe --profile minimal
```

### 6.3 Run Multiple Profiles or Clones Concurrently

You can run multiple distinct robot profiles (e.g., comparing three different evaluation functions or neural network variants simultaneously) in one process or separate PowerShell windows:

```powershell
# Run multiple named profiles in one process:
.\build\Release\octibot.exe --profiles bot_v1,bot_v2,bot_v3

# Or run all [profile.*] sections defined in octibot.conf:
.\build\Release\octibot.exe --all-profiles

# Spawn 3 concurrent clones of a profile using 3 tokens from tokens.conf:
.\build\Release\octibot.exe --profile my_win_bot --instances 3
```

### 6.4 Inspecting the Embedded Web Dashboard

While `octibot.exe` is running with `web_port = 3888`, open **`http://localhost:3888/`** in Edge, Chrome, or Firefox to inspect:
- Live board visualization, room ID, seat, and player clocks
- Real-time search depth, node count, and evaluation score
- Rolling 50-move telemetry log and 5,000-line console log buffer
- Live configuration tuning without restarting the binary

To stop the bot cleanly and release its token lease on `octi.selfso.com`, press **`Ctrl+C`** in the PowerShell window.

---

## 7. Next Steps

- **[Algorithm Authoring & Porting Guide (`doc/WRITING_BOTS.md`)](WRITING_BOTS.md)**: Learn how to write your own `OctiBotAgent` in C++ or port an existing algorithm to play on `octi.selfso.com`.
- **[Robot Wire Protocol Specification (`doc/ROBOT_PROTOCOL.md`)](ROBOT_PROTOCOL.md)**: Complete REST + Server-Sent Events (SSE) wire specification for `octi.selfso.com`.
- **[Linux Setup & Build Guide (`doc/SETUP_LINUX.md`)](SETUP_LINUX.md)**: Building and running OCTIBOT on Ubuntu 24.04 LTS.
