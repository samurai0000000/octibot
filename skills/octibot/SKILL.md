---
name: octibot
description: Autonomous and Centaur AI agent skill guide for OCTI rules, notation, tactical search, tuning, and Model Context Protocol (MCP) tool integration.
---

# OCTIBOT Agentic AI Skill Guide

Copyright (C) 2026, Charles Chiou

This skill equips an AI agent with comprehensive knowledge of OCTI board game variants, algebraic move syntax, positional evaluation principles, bot engine tuning, and Centaur MCP workflows.

---

## Section: rules

### 1. Board Geometries & Modes
- **2P (Standard 2-Player)**: Played on a 9x9 grid. Seats 1 (South/Bottom, rows 1..3) and 2 (North/Top, rows 7..9).
- **4P (4-Player Partnerships)**: Played on a 9x9 grid. 4 players in 2 teams: Team A (Seats 1 & 3) vs Team B (Seats 2 & 4). Seats are positioned at South (1), West (2), North (3), East (4).
- **OFK (OCTI for Kids)**: Fast-paced variant played on a 7x7 grid. 2 players (Seats 1 & 2).

### 2. Bases & Victory Conditions
- **Bases**: Marked squares on the board representing player home territories.
  - In standard 2P, each player starts with 3 bases (e.g. South has bases at c2, e2, g2; North has bases at c8, e8, g8).
  - In 1-base variants, each player has a single central base.
- **Victory Conditions**:
  - Occupying an opponent base: If a pod lands on an opponent base, the player wins (in 1-base mode) or captures the base (in 3-base mode). In 3-base mode, occupying all enemy bases (or majority base capture according to match rules) secures victory.
  - Pod Elimination (Wipeout): If an opponent has no pods remaining on the board and no pods in reserve, they are eliminated.
  - Elimination of all enemies grants immediate victory.

### 3. Reserves & Game Pieces
- **Pods**: Cylindrical pieces placed on squares. Each player has a starting reserve of pods (typically 4 or 7).
- **Prongs**: Small directional spears inserted into any of the 8 compass faces of a pod. Each player starts with a reserve of prongs (e.g. 24).
- **Superprongs**: Optional variant allowing prongs that enable 2-square jumps over empty squares.
- **Edgeless Board**: Optional variant where grid edges wrap around toroidally (a pod moving off the East edge appears on the West edge).

---

## Section: notation

### 1. Coordinates & Compass Bearings
- Columns are labeled `a` through `i` (from West to East for 9x9; `a` through `g` for 7x7).
- Rows are numbered `1` through `9` (from South to North for 9x9; `1` through `7` for 7x7).
- The 8 prong slots on a pod correspond to the 8 compass directions:
  - `a`: North (Up)
  - `b`: North-East (Up-Right)
  - `c`: East (Right)
  - `d`: South-East (Down-Right)
  - `e`: South (Down)
  - `f`: South-West (Down-Left)
  - `g`: West (Left)
  - `h`: North-West (Up-Left)

### 2. Move Syntaxes
- **Add Pod**: `+<sq>`
  - Places a fresh unpronged pod from reserve onto an empty base square owned by the player (e.g. `+e2`).
- **Insert Prong**: `<sq>+<dir>`
  - Inserts a prong from reserve into the specified face of an existing pod (e.g. `e2+a`).
- **Reposition Prong**: `<sq>+<new_dir>-<old_dir>`
  - Extracts a prong from `<old_dir>` and moves it to `<new_dir>` on the same pod (e.g. `e2+c-a`).
- **Step Move**: `<from_sq>-<to_sq>`
  - A pod moves 1 square in a direction in which it has a prong (e.g. `e2-e3`).
- **Single Jump**: `<from_sq>-<to_sq>`
  - A pod leaps over an adjacent pod (allied or enemy) into an empty square immediately beyond it, provided the leaping pod has a prong pointing in that jump direction (e.g. `e2-e4` jumping over `e3`).
- **Jump Capture**: `<from_sq>-<to_sq>x`
  - In variants where jumping over enemy pods captures them (or captures prongs), an `x` suffix denotes the capture (e.g. `e2-e4x`).
- **Multi-Leap**: `<sq1>-<sq2>-<sq3>...`
  - A pod may make a sequence of consecutive jumps in a single turn. Directions may change between jumps as long as the pod possesses the necessary prong for each jump.
- **Composite Multi-Pod Moves**: `<action1>,<action2>`
  - In certain variants (or handicap rules), multiple actions in one turn are comma-separated.

---

## Section: strategy

### 1. Multi-Pod Development vs Single-Pod Monopoly
- Novice players frequently over-invest prongs into a single "super-pod". If that pod is trapped, jumped, or pinned, the entire game is lost.
- Strong play develops multiple armed pods (2 to 3 pods with 2-4 prongs each), establishing mutual protection and multi-vector threats.

### 2. Orthogonal and Diagonal Versatility
- Pods possessing only orthogonal prongs (`a`, `c`, `e`, `g`) or only diagonal prongs (`b`, `d`, `f`, `h`) suffer from grid color parity restrictions and predictability.
- Blending orthogonal and diagonal prongs maximizes jumping options and evasive flexibility.

### 3. Leapfrog Batteries & Launchpads
- Positioning allied pods in linear or diagonal chains creates "launchpad batteries" that allow a trailing pod to make rapid multi-jump rushes across the board directly into an enemy base.

### 4. Hanging-Pod Jump-Capture Dangers
- A pod is "hanging" if an enemy pod can jump over it and capture it (or its prongs) on the opponent's next turn. Always verify that advancing a pod does not land it in an enemy jump trajectory without compensation.

### 5. 2-Ply Base Infiltration Threats
- An unstoppable threat is created when a pod is positioned so that it can reach an opponent base via two distinct paths, or when two pods threaten the base simultaneously.

### 6. OCTI for Kids (OFK) Prong Seizure
- In OFK, jumping an enemy pod seizes its prongs. A single capture can instantly flip material balance from losing to overwhelmingly winning.

---

## Section: tuning

`octibot` allows live configuration of its 47 evaluation and search hyperparameters via `octibot_configure_policy` or `octibot.conf`:

- `time_budget_ms`: Search clock budget per move (default 1500).
- `max_search_depth`: Maximum iterative deepening depth limit.
- `lazy_smp_threads`: Number of worker threads for parallel tree search.
- `pod_zero_prong_val` ... `pod_three_plus_prong_val`: Positional values for pods by prong count.
- `orth_diag_versatility_bonus`: Bonus for balanced prong types.
- `single_pod_monopoly_penalty`: Penalty for concentrating all prongs on one pod.
- `leapfrog_battery_weight`: Weight for aligned leaping batteries.
- `hanging_pod_base_penalty`: Penalty for leaving pods vulnerable to jump-capture.
- `move_order_capture_bonus`: Move ordering priority for tactical captures.

---

## Section: workflow

Centaur AI agents should execute the following reactive loop using MCP tools:

1. **Bootstrap & Inspection**:
   - Call `octibot_get_skill_guide(topic="all")` to understand rules and state representation.
   - Call `octibot_get_status()` to inspect connection health, assigned handle, and room.
2. **Table Setup**:
   - If in lobby, call `octibot_find_or_host_table(auto_join=true, auto_host=true)` to find or create a compatible game.
3. **Reactive Wait Loop**:
   - Call `octibot_wait_for_event(timeout_ms=30000)`.
   - The tool unblocks immediately when an event occurs:
     - `BOT_TURN`: It is the bot's turn to play.
     - `CHAT_MENTION`: An unread chat directed at the bot.
     - `PROPOSAL`: A draw, undo, pause, or rematch request.
     - `GAME_STARTED` / `GAME_OVER`: Room state transition.
4. **Turn Execution**:
   - Call `octibot_get_game_state()` to retrieve current FEN, clocks, and legal moves.
   - Call `octibot_analyze_position(top_k=3)` to obtain candidate moves scored by the alpha-beta engine.
   - Evaluate tactical and strategic nuances, then call `octibot_game_action(action="move", move="<chosen_notation>")`.
5. **Chat Interaction**:
   - Use `octibot_send_chat(channel="game", message="...")` to converse with opponents or acknowledge commands.
