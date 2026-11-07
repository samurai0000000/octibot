# Writing & Porting Custom OCTI Algorithms (`WRITING_BOTS.md`)

This guide explains how to write a new game-playing algorithm—or port an existing search engine, Monte Carlo Tree Search (MCTS) agent, or neural network model—using the **OCTIBOT** repository (`octibot`) as your foundation.

By building on `octibot`, your algorithm inherits a verified rules and move-generation engine (`octi_core`), automatic HTTPS/SSE connection management, lobby matchmaking/table-hosting policies, live search-state chat broadcasting, and `.octi` transcript telemetry out of the box.

---

## 1. Architecture Overview

The repository is divided into three clean layers:

```text
+-----------------------------------------------------------------------+
| Your Algorithm (Custom OctiBotAgent Subclass / External Engine)       |
|  - Implements computeMove(board, game, mySeat, clockMs, outTelemetry) |
|  - Optional hooks: onConfigure, onGameStarted, onProposal, onGameChat |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| octibot_sdk (sdk/include/*.hxx)                                       |
|  - OctiBotRunner: SSE event loop, turn dispatch, reconnect & lease    |
|  - OctiBotPolicyEngine: table hosting/joining, draw/undo/rematch      |
|  - OctiBotConfigStore: INI profile & tokens.conf resolution           |
|  - OctiBotMcpServer: Embedded Web Dashboard (:3888) & MCP server      |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| octi_core (core/include/*.hxx)                                        |
|  - OctiBoard, OctiGame, OctiSquare, OctiPod, OctiMove                 |
|  - 100% legal move generation (2P, 4P, OFK, 1-base/3-base, superprong)|
|  - Fast O(1)-style snapshot capture/restore for tree search           |
|  - OctiBotEngine: Reference alpha-beta + Lazy SMP + opening book      |
|  - OctiNetworkClient: HTTPS + SSE wire protocol for octi.selfso.com   |
+-----------------------------------------------------------------------+
```

---

## 2. The `OctiBotAgent` Interface

All bots implement `octi::OctiBotAgent` defined in `sdk/include/OctiBotAgent.hxx`:

```cpp
#include "OctiBotAgent.hxx"

namespace octi {

struct BotIdentity {
    std::string robotUuid;
    std::string handle;       // Registered Robot Name (e.g. "my_bot")
    std::string description;  // Registered Robot Description
    std::string ownerUuid;
    std::string ownerHandle;
    std::string activeRoomId; // Non-empty if resuming an active room on connect
    int gamesPlayed = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
};

struct BotMoveTelemetry {
    int depthReached = 0;
    std::int64_t nodesVisited = 0;
    int evalScore = 0;
    std::int64_t elapsedMs = 0;
    bool present = false;
};

class OctiBotAgent {
public:
    virtual ~OctiBotAgent() = default;

    // Called once at startup and whenever configuration is hot-reloaded
    virtual void onConfigure(const OctiBotConfigStore &config) {}

    // Called after POST /api/v1/robot/connect succeeds
    virtual void onConnected(const BotIdentity &identity) {}

    // Called when a lobby chat message arrives; set outReply to respond
    virtual void onLobbyChat(const LobbyChatEntry &msg,
                             std::string &outReply) {}

    // Called when a match transitions to IN_PROGRESS
    virtual void onGameStarted(const GameViewData &game,
                               int mySeat,
                               std::string &outGreetingChat) {}

    // REQUIRED: Compute and return a legal OCTI move notation string
    virtual std::string computeMove(const OctiBoard &board,
                                    const GameViewData &game,
                                    int mySeat,
                                    std::int64_t remainingClockMs,
                                    BotMoveTelemetry *outTelemetry = nullptr) = 0;

    // Return true to accept or false to decline a draw/undo/pause/rematch offer
    virtual bool onProposal(const GameProposalInfo &proposal,
                            const GameViewData &game) { return true; }

    // Called when a message is posted in the active game room chat
    virtual void onGameChat(const std::string &senderHandle,
                            const std::string &text,
                            std::string &outReply) {}

    // Called when a match concludes (FINISHED / GAME_OVER / ABANDONED)
    virtual void onGameOver(const GameViewData &game, int mySeat) {}
};

} // namespace octi
```

---

## 3. Inspecting the Board & Simulating Moves (`octi_core`)

When `OctiBotRunner` invokes your `computeMove(board, game, mySeat, remainingClockMs, outTelemetry)` method, `board` is already synchronized to the authoritative server FEN position.

### 3.1 Board Coordinates, Squares, and Pods

- **2-Player (`GAME_MODE_2`) and OCTI for Kids (`GAME_MODE_OFK`)**: `7x7` grid (`x` in `0..6` corresponding to files `'1'..'7'`, `y` in `0..6` corresponding to ranks `'1'..'7'`).
  - Player A (`PLAYER_A`, Seat 1) home bases are at `(1, 1)`, `(3, 1)`, `(5, 1)` (`A_HOME_SQUARE`).
  - Player B (`PLAYER_B`, Seat 2) home bases are at `(1, 5)`, `(3, 5)`, `(5, 5)` (`B_HOME_SQUARE`).
- **4-Player (`GAME_MODE_4`)**: `9x9` grid (`x` in `0..8`, `y` in `0..8`).
- **Directions (`OctiDir`)**: 8 compass directions (`DIR_N = 0`, `DIR_NE = 1`, `DIR_E = 2`, `DIR_SE = 3`, `DIR_S = 4`, `DIR_SW = 5`, `DIR_W = 6`, `DIR_NW = 7`), encoded in move strings as `'a'..'h'`.

Inspecting squares, pods, prongs, and player reserves:

```cpp
const OctiGame *g = board.getGame();
OctiPlayer us = g->getTurn(); // Active player to move (PLAYER_A..PLAYER_D)

// Reserve material for a player:
int freePods        = g->getFreePods(us);
int capturedPods    = g->getCapturedPods(us);
int freeProngs      = g->getFreeProngs(us);
int freeSuperProngs = g->getFreeSuperProngs(us);

// Iterating over the board grid:
int width  = board.getBoardWidth();
int height = board.getBoardHeight();

for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
        const OctiSquare &sq = board.squareAt(x, y);
        OctiSquareType sqType = sq.getType(); // NEUTRAL_SQUARE, A_HOME_SQUARE, B_HOME_SQUARE, ...

        for (const OctiPod &pod : sq.pods()) {
            OctiPlayer owner = podTypeToPlayer(pod.getType());
            int totalProngs = pod.getNumProngs();

            for (int d = 0; d < DIR_MAX; ++d) {
                OctiProngType pt = pod.prongAt(static_cast<OctiDir>(d));
                // pt is NO_PRONG, NORMAL_PRONG, or SUPER_PRONG
            }
        }
    }
}
```

### 3.2 Legal Move Generation & Fast Tree Search Simulation

Because `const OctiBoard &board` passed to `computeMove()` is `const`, create a mutable scratch board from `board.toPositionString()` and enable **search mode** (`setSearchMode(true)` and `suppressBoardUpdateNotification(true)`).

During tree search (minimax, alpha-beta, or MCTS rollouts), use `captureSnapshot()` and `restoreSnapshot()` on `OctiGame`—this avoids string parsing overhead and restores the entire board and reserve state in microseconds:

```cpp
// 1. Initialize a mutable scratch board matching the game rules
OctiBoard scratch(board.getGameMode(),
                  board.getGame()->useSuperProngs(),
                  board.getGame()->useEdgelessBoard(),
                  board.getGame()->getBases());
scratch.suppressBoardUpdateNotification(true);
scratch.fromPositionString(board.toPositionString());

OctiGame *sg = scratch.getGame();
sg->setSearchMode(true);

// 2. Generate all legal moves for the current side to move
std::vector<OctiMove> legalMoves;
sg->generateLegalMoves(legalMoves);

// 3. Simulate each candidate move using captureSnapshot / restoreSnapshot
OctiGameSnapshot rootSnap = sg->captureSnapshot();

for (const OctiMove &mv : legalMoves) {
    sg->move(mv);

    if (sg->isGameOver() && sg->getWinner() == us) {
        // Immediate winning move found!
        return mv.asString();
    }

    // Evaluate child state or recurse deeper...
    sg->restoreSnapshot(rootSnap);
}
```

---

## 4. Emitting Algorithm Search States in Table Chat & Telemetry

One of the best ways to showcase your robot on **`octi.selfso.com`** is to let opponents and spectators see what your algorithm is thinking in real time.

### 4.1 Automatic Post-Move Search State Chat (`announce_move_search_chat`)

Whenever your `computeMove(...)` implementation populates `outTelemetry` and sets `outTelemetry->present = true`:

```cpp
if (outTelemetry) {
    outTelemetry->depthReached = completedDepth;
    outTelemetry->nodesVisited = totalNodesSearched;
    outTelemetry->evalScore    = bestRootScore;
    outTelemetry->elapsedMs    = elapsedMilliseconds;
    outTelemetry->present      = true;
}
```

`OctiBotRunner` automatically performs three actions after submitting your move:

1. **Table Chat Broadcast (`announce_move_search_chat = true`)**:
   Posts a live search status line to the game room chat (`POST /api/v1/game/{room_id}/chat` with `"scope": "ALL"`):
   ```text
   depth=8 | nodes=581440 | eval_score=16 | elapsed=10001ms
   ```
2. **Permanent `.octi` Archive Telemetry (`publish_move_telemetry = true`)**:
   Includes your telemetry object in the move submission (`POST /api/v1/game/{room_id}/move`):
   ```json
   {
     "move": "32+a",
     "telemetry": {
       "depth": 8,
       "nodes": 581440,
       "score": 16,
       "elapsed_ms": 10001
     }
   }
   ```
   This metadata is stored permanently in the server's `.octi` v3.0 game transcript (`aiDepthReached`, `aiNodesVisited`, `aiBestScore`, `elapsedMs`) so anyone reviewing the archived match can analyze your bot's evaluation curve ply by ply.
3. **Interactive Chat Queries (`allow_public_eval_chat = true`)**:
   Human players or spectators in the room can type `@<your_bot> eval` or `@<your_bot> status` in chat, and `OctiBotRunner` will reply with your bot's latest search telemetry and W-L-D record.

### 4.2 Custom Interactive Diagnostics via `onGameChat`

If your algorithm computes rich internal metrics—such as MCTS win probabilities, top-3 principal variation (PV) lines, neural network value head predictions, or transposition table hit rates—you can expose custom chat commands by overriding `onGameChat`:

```cpp
void onGameChat(const std::string &senderHandle,
                const std::string &text,
                std::string &outReply) override {
    if (text == "!pv" || text == "!explain") {
        outReply = "PV: " + _lastPvLine +
                   " | win_prob=" + std::to_string(_lastWinProbPct) + "%" +
                   " | tt_hit=" + std::to_string(_lastTtHitPct) + "%";
    }
}
```

---

## 5. Step-by-Step Example: Creating & Registering a Custom C++ Agent

You can either modify `examples/minimal_bot/Main.cxx` directly or register a new named algorithm in `OctiBotAgentRegistry` so it can be selected via `algorithm = my_custom_algo` in `octibot.conf`.

### Example (`examples/minimal_bot/Main.cxx` or your own source file)

```cpp
#include <chrono>
#include <iostream>
#include <limits>
#include "OctiBoard.hxx"
#include "OctiBotAgent.hxx"
#include "OctiBotEngine.hxx"
#include "OctiBotRunner.hxx"
#include "OctiGame.hxx"

namespace octi {

class MyCustomNegamaxAgent : public OctiBotAgent {
public:
    explicit MyCustomNegamaxAgent(const OctiBotConfigStore &config) {
        onConfigure(config);
    }

    void onConfigure(const OctiBotConfigStore &config) override {
        _maxDepth     = config.getInt("max_search_depth", 4);
        _timeBudgetMs = config.getInt("time_budget_ms", 3000);
        applyConfigToTuningProfile(config, _profile);
    }

    void onGameStarted(const GameViewData &game,
                       int mySeat,
                       std::string &outGreetingChat) override {
        outGreetingChat = "Hello! Running MyCustomNegamaxAgent (max_depth=" +
                          std::to_string(_maxDepth) + "). Good luck!";
    }

    std::string computeMove(const OctiBoard &board,
                            const GameViewData &game,
                            int mySeat,
                            std::int64_t remainingClockMs,
                            BotMoveTelemetry *outTelemetry) override {
        (void) game;
        (void) mySeat;
        auto startTime = std::chrono::steady_clock::now();

        const OctiGame *g = board.getGame();
        if (!g) return "";

        std::vector<OctiMove> rootMoves = g->generateLegalMoves();
        if (rootMoves.empty()) return "";

        // Respect remaining clock if timed match is low on time
        std::int64_t budgetMs = _timeBudgetMs;
        if (remainingClockMs > 0 && remainingClockMs < budgetMs * 3) {
            budgetMs = std::max<std::int64_t>(200, remainingClockMs / 10);
        }

        OctiBoard scratch(board.getGameMode(),
                          g->useSuperProngs(),
                          g->useEdgelessBoard(),
                          g->getBases());
        scratch.suppressBoardUpdateNotification(true);
        scratch.fromPositionString(board.toPositionString());
        OctiGame *sg = scratch.getGame();
        sg->setSearchMode(true);

        OctiPlayer us = sg->getTurn();
        OctiGameSnapshot rootSnap = sg->captureSnapshot();

        std::string bestMove = rootMoves.front().asString();
        int bestScore = std::numeric_limits<int>::min() / 2;
        std::int64_t nodes = 0;
        int completedDepth = 1;

        // Iterative deepening loop
        for (int depth = 1; depth <= _maxDepth; ++depth) {
            int iterBestScore = std::numeric_limits<int>::min() / 2;
            std::string iterBestMove = bestMove;
            bool aborted = false;

            for (const auto &mv : rootMoves) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - startTime).count();
                if (depth > 1 && elapsed >= budgetMs) {
                    aborted = true;
                    break;
                }

                sg->move(mv);
                nodes++;

                int score;
                if (sg->isGameOver()) {
                    score = (sg->getWinner() == us) ? (100000 - depth) : (-100000 + depth);
                } else {
                    score = -negamax(scratch, depth - 1, -100000, -iterBestScore,
                                     sg->getTurn(), nodes, startTime, budgetMs, aborted);
                }
                sg->restoreSnapshot(rootSnap);

                if (aborted) break;
                if (score > iterBestScore) {
                    iterBestScore = score;
                    iterBestMove = mv.asString();
                }
            }

            if (!aborted) {
                bestMove = iterBestMove;
                bestScore = iterBestScore;
                completedDepth = depth;
            } else {
                break;
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        std::int64_t totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();

        if (outTelemetry) {
            outTelemetry->depthReached = completedDepth;
            outTelemetry->nodesVisited = nodes;
            outTelemetry->evalScore    = bestScore;
            outTelemetry->elapsedMs    = totalMs;
            outTelemetry->present      = true;
        }
        return bestMove;
    }

private:
    int negamax(OctiBoard &scratch,
                int depth,
                int alpha,
                int beta,
                OctiPlayer perspective,
                std::int64_t &nodes,
                std::chrono::steady_clock::time_point startTime,
                std::int64_t budgetMs,
                bool &aborted) {
        if ((nodes & 255) == 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed >= budgetMs) {
                aborted = true;
                return 0;
            }
        }

        OctiGame *sg = scratch.getGame();
        if (sg->isGameOver()) {
            return (sg->getWinner() == perspective) ? 90000 : -90000;
        }
        if (depth <= 0) {
            return OctiBotEngine::evaluatePosition(scratch, perspective, _profile);
        }

        std::vector<OctiMove> moves;
        sg->generateLegalMoves(moves);
        if (moves.empty()) return -80000;

        OctiGameSnapshot snap = sg->captureSnapshot();
        int best = std::numeric_limits<int>::min() / 2;

        for (const auto &mv : moves) {
            sg->move(mv);
            nodes++;
            int val = -negamax(scratch, depth - 1, -beta, -alpha,
                               sg->getTurn(), nodes, startTime, budgetMs, aborted);
            sg->restoreSnapshot(snap);
            if (aborted) return 0;
            if (val > best) best = val;
            if (best > alpha) alpha = best;
            if (alpha >= beta) break;
        }
        return best;
    }

    int _maxDepth = 4;
    int _timeBudgetMs = 3000;
    BotTuningProfile _profile;
};

} // namespace octi

int main(int argc, char **argv) {
    // Register our custom algorithm so octibot.conf can select:
    //   algorithm = my_custom_negamax
    octi::OctiBotAgentRegistry::instance().registerAgent(
        "my_custom_negamax",
        [](const octi::OctiBotConfigStore &cfg) {
            return std::make_unique<octi::MyCustomNegamaxAgent>(cfg);
        });

    return octi::runFleetCli(argc, argv);
}
```

---

## 6. Reusing Built-In `OctiBotEngine` Primitives

Even when writing a brand-new search algorithm (such as MCTS, Proof-Number Search, or a hybrid neural network evaluator), you can call static utilities on `octi::OctiBotEngine` (`core/include/OctiBotEngine.hxx`):

| Method | Purpose |
| :--- | :--- |
| `OctiBotEngine::computePositionHashStatic(board)` | Deterministic 64-bit Zobrist hash of board pods, prongs, reserves, and turn for transposition tables |
| `OctiBotEngine::computeCanonicalOpeningHash(board, &mirrored)` | Horizontal-symmetry canonical Zobrist hash for opening book lookups |
| `OctiBotEngine::mirrorMoveNotation(moveStr, width)` | Mirrors a move string across the vertical board axis (`x' = width - 1 - x`) |
| `OctiBotEngine::evaluatePosition(board, perspective, profile)` | Full 46-parameter static heuristic evaluation in centipawn-like units |
| `OctiBotEngine::probeOpeningBookTable(...)` | Probes a sorted `OpeningBookEntry[]` table with temperature-weighted candidate selection |

---

## 7. Porting External Engines & Non-C++ Algorithms

If your algorithm is written in **Python (PyTorch / JAX)**, **Rust**, **Go**, or **Java**, you have four clean integration paths:

### Option A — Subprocess / Pipe Bridge inside `OctiBotAgent`

Keep `octibot` as your connection, rules validation, and legal move generator wrapper, and invoke your external engine over `stdin`/`stdout` or a localhost socket inside `computeMove()`:
1. Serialize the current board state via `board.toPositionString()` and the legal move list (`g->generateLegalMoves()`).
2. Send `{ "fen": ..., "legal_moves": [...], "clock_ms": ... }` to your Python/Rust process.
3. Read back `{ "move": "32+a", "depth": 10, "nodes": 45000, "score": 310 }` and populate `outTelemetry`.

### Option B — Native C++/ONNX Runtime / LibTorch Integration

Link your C++ `OctiBotAgent` against **ONNX Runtime** or **LibTorch** in `CMakeLists.txt` and evaluate board feature tensors directly inside your search loop.

### Option C — Centaur / MCP External Agent Mode (`move_control = agent`)

Set `algorithm = centaur_mcp`, `move_control = agent`, and `expose_mcp_tools = true` (with `mcp_port = 3888`) in `octibot.conf`.
Your external AI agent or script can then drive the robot over HTTP JSON-RPC 2.0 (`POST http://127.0.0.1:3888/mcp`) using the 9 built-in MCP tools:
- `octibot_wait_for_event` (wakes immediately on `BOT_TURN`, `CHAT_MENTION`, `PROPOSAL`, or `GAME_OVER`)
- `octibot_get_game_state` (returns FEN, ASCII board, legal moves, and clocks)
- `octibot_analyze_position` (runs `OctiBotEngine` search to assist your agent)
- `octibot_game_action` (submits the chosen move or accepts/declines proposals)
- `octibot_send_chat` (posts messages or commentary to lobby/table chat)

### Option D — Pure Wire-Protocol Client in Any Language

If you want to write a 100% standalone client in Python, Rust, TypeScript, or Go without C++, implement the REST + Server-Sent Events (SSE) endpoints documented in **[`doc/ROBOT_PROTOCOL.md`](ROBOT_PROTOCOL.md)**.
