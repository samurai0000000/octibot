/*
 * OctiBotMcpServer.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiBotMcpServer.hxx"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <nlohmann/json.hpp>
#include <httplib.h>
#include "OctiBoard.hxx"
#include "OctiDefs.hxx"
#include "OctiGame.hxx"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace octi {

namespace {

const char *CANONICAL_SKILL_GUIDE = R"(---
name: octibot
description: Autonomous and Centaur AI agent skill guide for OCTI rules, notation, tactical search, tuning, and Model Context Protocol (MCP) tool integration.
---

# OCTIBOT Agentic AI Skill Guide

Copyright (C) 2026, Charles Chiou

This skill equips an AI agent with comprehensive knowledge of OCTI board game variants, algebraic move syntax, positional evaluation principles, bot engine tuning, and Centaur MCP workflows.

---

## Section: rules

### 1. Board Geometries & Modes
- 2P (Standard 2-Player): Played on a 9x9 grid. Seats 1 (South/Bottom, rows 1..3) and 2 (North/Top, rows 7..9).
- 4P (4-Player Partnerships): Played on a 9x9 grid. 4 players in 2 teams: Team A (Seats 1 & 3) vs Team B (Seats 2 & 4). Seats are positioned at South (1), West (2), North (3), East (4).
- OFK (OCTI for Kids): Fast-paced variant played on a 7x7 grid. 2 players (Seats 1 & 2).

### 2. Bases & Victory Conditions
- Bases: Marked squares on the board representing player home territories.
  - In standard 2P, each player starts with 3 bases.
  - In 1-base variants, each player has a single central base.
- Victory Conditions:
  - Occupying an opponent base: In 1-base mode, landing on the enemy base wins immediately. In 3-base mode, occupying all enemy bases wins.
  - Pod Elimination: If an opponent has no pods on board and no pods in reserve, they are eliminated.

### 3. Reserves & Game Pieces
- Pods: Cylindrical pieces placed on squares.
- Prongs: Directional spears inserted into any of the 8 compass faces of a pod.
- Superprongs: Optional variant allowing prongs that enable 2-square jumps over empty squares.
- Edgeless Board: Optional variant where grid edges wrap around toroidally.

---

## Section: notation

### 1. Coordinates & Compass Bearings
- Columns: 'a' through 'i' (West to East).
- Rows: '1' through '9' (South to North).
- Compass directions: 'a'=North, 'b'=North-East, 'c'=East, 'd'=South-East, 'e'=South, 'f'=South-West, 'g'=West, 'h'=North-West.

### 2. Move Syntaxes
- Add Pod: +<sq> (e.g. +e2)
- Insert Prong: <sq>+<dir> (e.g. e2+a)
- Reposition Prong: <sq>+<new_dir>-<old_dir> (e.g. e2+c-a)
- Step Move: <from>-<to> (e.g. e2-e3)
- Jump: <from>-<to> (e.g. e2-e4)
- Jump Capture: <from>-<to>x (e.g. e2-e4x)
- Multi-Leap: <sq1>-<sq2>-<sq3>
- Composite Multi-Pod Moves: <action1>,<action2>

---

## Section: strategy

### 1. Multi-Pod Development vs Single-Pod Monopoly
- Developing multiple armed pods (2-3 pods with 2-4 prongs each) prevents single-point failure and establishes cross-board threats.

### 2. Orthogonal and Diagonal Versatility
- Pods with mixed orthogonal and diagonal prongs avoid parity traps and maximize jumping pathways.

### 3. Leapfrog Batteries & Launchpads
- Aligned allied pods create leapfrog batteries enabling multi-jump rushes directly into enemy bases.

### 4. Hanging-Pod Jump-Capture Dangers
- Never leave an armed pod where an enemy pod can leap over and capture it on the following turn without adequate tactical compensation.

### 5. 2-Ply Base Threats
- Create simultaneous multi-path infiltration threats to overwhelm opponent defensive responses.

### 6. OCTI for Kids (OFK) Prong Seizure
- In OFK, jumping an enemy pod seizes its prongs, producing massive material swings.

---

## Section: tuning

Hyperparameters in octibot.conf:
- time_budget_ms: Per-move search time budget in ms (default 1500).
- max_search_depth: Maximum iterative deepening depth limit.
- lazy_smp_threads: Search concurrency threads.
- captured_enemy_base_bonus / lost_allied_base_penalty: Base value weights.
- pod_zero_prong_val ... pod_three_plus_prong_val: Piece values by prong count.
- leapfrog_battery_weight: Bonus for aligned leapfrog configurations.
- hanging_pod_base_penalty: Penalty for vulnerable pods.

---

## Section: workflow

Centaur AI Workflow:
1. octibot_get_skill_guide(topic="all")
2. octibot_get_status()
3. octibot_find_or_host_table(auto_join=true, auto_host=true)
4. octibot_wait_for_event(timeout_ms=30000)
5. octibot_get_game_state()
6. octibot_analyze_position(top_k=3)
7. octibot_game_action(action="move", move="<chosen_notation>")
8. octibot_send_chat(channel="game", message="...")
)";

} // namespace

std::string OctiBotMcpServer::getSkillGuideContent(const std::string &topic) {
    std::string fullText = CANONICAL_SKILL_GUIDE;

    std::ifstream f("skills/octibot/SKILL.md");
    if (!f.is_open()) {
        f.open("../skills/octibot/SKILL.md");
    }
    if (f.is_open()) {
        std::stringstream ss;
        ss << f.rdbuf();
        if (!ss.str().empty()) {
            fullText = ss.str();
        }
    }

    if (topic.empty() || topic == "all") {
        return fullText;
    }

    std::string marker = "## Section: " + topic;
    size_t pos = fullText.find(marker);
    if (pos == std::string::npos) {
        return "Topic '" + topic + "' not found. Available topics: all, rules, notation, strategy, tuning, workflow.";
    }

    size_t nextPos = fullText.find("## Section: ", pos + marker.size());
    if (nextPos == std::string::npos) {
        return fullText.substr(pos);
    }
    return fullText.substr(pos, nextPos - pos);
}

OctiBotMcpServer::OctiBotMcpServer(OctiBotRunner &runner)
    : _runner(runner) {
    if (_runner.config().getBool("aimon_enabled", false)) {
        startAimonBridge();
    }
}

OctiBotMcpServer::~OctiBotMcpServer() {
    stopAimonBridge();
    stopHttpServer();
}

nlohmann::json OctiBotMcpServer::buildToolsListJson() const {
    nlohmann::json tools = nlohmann::json::array();

    tools.push_back({
        {"name", "octibot_get_skill_guide"},
        {"description", "Retrieve canonical OCTI AI rules, notation, tactical strategy, tuning, and workflow guide."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"topic", {{"type", "string"}, {"description", "all, rules, notation, strategy, tuning, workflow"}}}
            }}
        }}
    });

    tools.push_back({
        {"name", "octibot_get_status"},
        {"description", "Get robot connection status, assigned handle, owner, active room, and record."},
        {"inputSchema", {{"type", "object"}}}
    });

    tools.push_back({
        {"name", "octibot_get_lobby"},
        {"description", "Inspect lobby players, open tables, recent chat, and unread chat messages."},
        {"inputSchema", {{"type", "object"}}}
    });

    tools.push_back({
        {"name", "octibot_find_or_host_table"},
        {"description", "Find compatible open tables or host a table according to policy."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"auto_join", {{"type", "boolean"}, {"description", "Automatically join best table if found"}}},
                {"auto_host", {{"type", "boolean"}, {"description", "Host a new table if no compatible table found"}}}
            }}
        }}
    });

    tools.push_back({
        {"name", "octibot_table_action"},
        {"description", "Execute table actions: create, join, leave, kick, observe, unobserve, enqueue_matchmaking, dequeue_matchmaking."},
        {"inputSchema", {
            {"type", "object"},
            {"required", {"action"}},
            {"properties", {
                {"action", {{"type", "string"}}},
                {"room_id", {{"type", "string"}}},
                {"seat", {{"type", "integer"}}},
                {"mode", {{"type", "string"}}}
            }}
        }}
    });

    tools.push_back({
        {"name", "octibot_get_game_state"},
        {"description", "Get active game room FEN, turn seat, clocks, ASCII board representation, and legal moves."},
        {"inputSchema", {{"type", "object"}}}
    });

    tools.push_back({
        {"name", "octibot_analyze_position"},
        {"description", "Run alpha-beta search on active game board (or custom FEN) and return top candidate moves scored with tactical flags."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"top_k", {{"type", "integer"}, {"description", "Number of top moves to return (default 3)"}}},
                {"fen", {{"type", "string"}, {"description", "Optional custom FEN string"}}}
            }}
        }}
    });

    tools.push_back({
        {"name", "octibot_game_action"},
        {"description", "Submit a move or game proposal action (resign, offer_draw, accept_proposal, decline_proposal, request_undo, request_pause, resume, request_rematch, claim_forfeit)."},
        {"inputSchema", {
            {"type", "object"},
            {"required", {"action"}},
            {"properties", {
                {"action", {{"type", "string"}, {"description", "move, resign, offer_draw, accept_proposal, etc."}}},
                {"move", {{"type", "string"}, {"description", "Move notation when action is 'move'"}}},
                {"arg", {{"type", "string"}, {"description", "Proposal type argument"}}}
            }}
        }}
    });

    tools.push_back({
        {"name", "octibot_send_chat"},
        {"description", "Send a chat message to lobby or game room."},
        {"inputSchema", {
            {"type", "object"},
            {"required", {"channel", "message"}},
            {"properties", {
                {"channel", {{"type", "string"}, {"description", "lobby or game"}}},
                {"message", {{"type", "string"}}},
                {"scope", {{"type", "string"}, {"description", "ALL or TEAM (for game chat)"}}},
                {"mark_inbox_read", {{"type", "boolean"}, {"description", "Mark conversation inbox read"}}}
            }}
        }}
    });

    tools.push_back({
        {"name", "octibot_configure_policy"},
        {"description", "Inspect or update bot configuration parameters and policy rules."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"key", {{"type", "string"}}},
                {"value", {{"type", "string"}}},
                {"save_to_file", {{"type", "boolean"}}}
            }}
        }}
    });

    tools.push_back({
        {"name", "octibot_wait_for_event"},
        {"description", "Block until an event occurs (bot move turn, chat mention, proposal, game start/end)."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"timeout_ms", {{"type", "integer"}, {"description", "Max wait time in ms (default 30000)"}}}
            }}
        }}
    });

    return tools;
}

std::string OctiBotMcpServer::handleJsonRpc(const std::string &jsonRequest) {
    nlohmann::json req;
    try {
        req = nlohmann::json::parse(jsonRequest);
    } catch (...) {
        nlohmann::json err;
        err["jsonrpc"] = "2.0";
        err["id"] = nullptr;
        err["error"] = {{"code", -32700}, {"message", "Parse error"}};
        return err.dump();
    }

    nlohmann::json id = req.value("id", nlohmann::json());
    std::string method = req.value("method", "");
    nlohmann::json params = req.value("params", nlohmann::json::object());

    if (!req.is_object() || (!req.contains("method") && (req.contains("result") || req.contains("error")))) {
        return "";
    }

    if (method == "notifications/initialized") {
        return "";
    }

    nlohmann::json resp;
    resp["jsonrpc"] = "2.0";
    resp["id"] = id;

    if (method == "initialize") {
        nlohmann::json result;
        result["protocolVersion"] = "2024-11-05";
        result["capabilities"] = {
            {"tools", nlohmann::json::object()},
            {"resources", nlohmann::json::object()}
        };
        result["serverInfo"] = {
            {"name", "octibot"},
            {"version", "1.0.3"}
        };
        resp["result"] = result;
        return resp.dump();
    }

    if (method == "ping") {
        resp["result"] = nlohmann::json::object();
        return resp.dump();
    }

    if (method == "resources/list") {
        nlohmann::json resList = nlohmann::json::array();
        resList.push_back({
            {"uri", "octi://skill/octibot"},
            {"name", "OCTI Robot Skill Guide"},
            {"description", "Canonical OCTI AI skill guide"},
            {"mimeType", "text/markdown"}
        });
        resp["result"] = {{"resources", resList}};
        return resp.dump();
    }

    if (method == "resources/read") {
        std::string uri = params.value("uri", "");
        if (uri == "octi://skill/octibot") {
            nlohmann::json contents = nlohmann::json::array();
            contents.push_back({
                {"uri", uri},
                {"mimeType", "text/markdown"},
                {"text", getSkillGuideContent("all")}
            });
            resp["result"] = {{"contents", contents}};
            return resp.dump();
        }
        resp["error"] = {{"code", -32602}, {"message", "Resource not found: " + uri}};
        return resp.dump();
    }

    if (method == "tools/list") {
        resp["result"] = {{"tools", buildToolsListJson()}};
        return resp.dump();
    }

    if (method == "tools/call") {
        std::string toolName = params.value("name", "");
        nlohmann::json args = params.value("arguments", nlohmann::json::object());
        std::string toolResult = callTool(toolName, args.dump());

        nlohmann::json content = nlohmann::json::array();
        content.push_back({
            {"type", "text"},
            {"text", toolResult}
        });
        resp["result"] = {
            {"content", content},
            {"isError", false}
        };
        return resp.dump();
    }

    resp["error"] = {{"code", -32601}, {"message", "Method not found: " + method}};
    return resp.dump();
}

std::string OctiBotMcpServer::callTool(const std::string &name, const std::string &argumentsJson) {
    nlohmann::json args;
    try {
        args = nlohmann::json::parse(argumentsJson);
    } catch (...) {
        args = nlohmann::json::object();
    }

    if (name == "octibot_get_skill_guide") {
        std::string topic = args.value("topic", "all");
        return getSkillGuideContent(topic);
    }

    if (name == "octibot_get_status") {
        nlohmann::json st;
        const auto &id = _runner.identity();
        st["robot_uuid"] = id.robotUuid;
        st["handle"] = id.handle;
        st["description"] = id.description;
        st["owner_uuid"] = id.ownerUuid;
        st["owner_handle"] = id.ownerHandle;
        st["active_room_id"] = _runner.currentRoomId();
        st["phase"] = _runner.currentPhase();
        st["seat"] = _runner.currentSeat();
        st["games_played"] = id.gamesPlayed;
        st["wins"] = id.wins;
        st["losses"] = id.losses;
        st["draws"] = id.draws;
        st["move_control"] = _runner.moveControl();
        st["table_behavior"] = _runner.policyEngine().tableBehavior();
        st["friends_count"] = _runner.policyEngine().friends().size();
        st["banned_count"] = _runner.policyEngine().banned().size();
        return st.dump(2);
    }

    if (name == "octibot_get_lobby") {
        auto res = _runner.networkClient().fetchLobbyState();
        nlohmann::json lobby;
        if (res.ok) {
            nlohmann::json players = nlohmann::json::array();
            for (const auto &p : res.data.players) {
                players.push_back({{"handle", p.handle}, {"status", p.status}, {"is_robot", p.isRobot}});
            }
            nlohmann::json tables = nlohmann::json::array();
            for (const auto &t : res.data.tables) {
                tables.push_back({{"room_id", t.roomId}, {"mode", t.mode}, {"host", t.hostHandle},
                                  {"seats_filled", t.seatsFilled}, {"seats_total", t.seatsTotal},
                                  {"allow_robots", t.allowRobots}});
            }
            nlohmann::json chat = nlohmann::json::array();
            for (const auto &c : res.data.recentChat) {
                chat.push_back({{"handle", c.handle}, {"text", c.text}, {"is_robot", c.isRobot}});
            }
            lobby["players"] = players;
            lobby["tables"] = tables;
            lobby["recent_chat"] = chat;
        } else {
            lobby["error"] = res.errorMessage;
        }
        lobby["unread_inbox"] = _runner.conversationEngine().getUnreadInbox().size();
        return lobby.dump(2);
    }

    if (name == "octibot_find_or_host_table") {
        bool autoJoin = args.value("auto_join", false);
        bool autoHost = args.value("auto_host", false);

        auto tablesRes = _runner.networkClient().listTables(false);
        nlohmann::json res;
        if (!tablesRes.ok) {
            res["error"] = tablesRes.errorMessage;
            return res.dump(2);
        }

        auto ranked = _runner.policyEngine().rankCompatibleTables(
            tablesRes.data, _runner.identity().ownerHandle, _runner.identity().handle);

        nlohmann::json rankedJson = nlohmann::json::array();
        for (const auto &t : ranked) {
            rankedJson.push_back({{"room_id", t.roomId}, {"host", t.hostHandle}, {"mode", t.mode},
                                  {"seats", std::to_string(t.seatsFilled) + "/" + std::to_string(t.seatsTotal)}});
        }
        res["compatible_tables"] = rankedJson;

        if (autoJoin && !ranked.empty()) {
            std::string roomId = ranked.front().roomId;
            auto joinRes = _runner.networkClient().joinTable(roomId);
            res["action_taken"] = "joined";
            res["joined_room_id"] = roomId;
            res["join_status"] = joinRes.ok ? "success" : joinRes.errorMessage;
        } else if (autoHost && ranked.empty()) {
            auto hostReq = _runner.policyEngine().buildHostTableRequest();
            auto createRes = _runner.networkClient().createTable(hostReq);
            res["action_taken"] = "hosted";
            res["hosted_room_id"] = createRes.data.roomId;
            res["host_status"] = createRes.ok ? "success" : createRes.errorMessage;
        } else {
            res["action_taken"] = "none";
        }
        return res.dump(2);
    }

    if (name == "octibot_table_action") {
        std::string action = args.value("action", "");
        nlohmann::json res;
        if (action == "create") {
            auto req = _runner.policyEngine().buildHostTableRequest();
            if (args.contains("mode")) req.mode = args["mode"].get<std::string>();
            auto r = _runner.networkClient().createTable(req);
            res["ok"] = r.ok;
            res["room_id"] = r.data.roomId;
        } else if (action == "join") {
            std::string roomId = args.value("room_id", "");
            auto r = _runner.networkClient().joinTable(roomId);
            res["ok"] = r.ok;
            res["room_id"] = r.data.roomId;
        } else if (action == "leave") {
            std::string roomId = args.value("room_id", _runner.currentRoomId());
            auto r = _runner.networkClient().leaveTable(roomId);
            res["ok"] = r.ok;
        } else if (action == "kick") {
            std::string roomId = args.value("room_id", _runner.currentRoomId());
            int seat = args.value("seat", 0);
            auto r = _runner.networkClient().kickSeat(roomId, seat);
            res["ok"] = r.ok;
        } else if (action == "observe") {
            std::string roomId = args.value("room_id", "");
            auto r = _runner.networkClient().observeTable(roomId);
            res["ok"] = r.ok;
        } else if (action == "unobserve") {
            std::string roomId = args.value("room_id", "");
            auto r = _runner.networkClient().unobserveTable(roomId);
            res["ok"] = r.ok;
        } else if (action == "enqueue_matchmaking") {
            auto req = _runner.policyEngine().buildMatchmakingRequest();
            auto r = _runner.networkClient().enqueueMatchmaking(req);
            res["ok"] = r.ok;
        } else if (action == "dequeue_matchmaking") {
            auto r = _runner.networkClient().dequeueMatchmaking();
            res["ok"] = r.ok;
        } else {
            res["error"] = "Unknown action: " + action;
        }
        return res.dump(2);
    }

    if (name == "octibot_get_game_state") {
        std::string roomId = _runner.currentRoomId();
        if (roomId.empty()) {
            return "{\"status\": \"Not in a game room\"}";
        }
        auto gv = _runner.networkClient().fetchGameView(roomId);
        nlohmann::json res;
        if (!gv.ok) {
            res["error"] = gv.errorMessage;
            return res.dump(2);
        }

        res["room_id"] = gv.data.roomId;
        res["mode"] = gv.data.mode;
        res["bases"] = gv.data.bases;
        res["phase"] = gv.data.phase;
        res["turn_seat"] = gv.data.turnSeat;
        res["turn_number"] = gv.data.plyCount;
        res["my_seat"] = _runner.currentSeat();
        res["fen"] = gv.data.fen;

        OctiGameMode m = (gv.data.mode == "4P" ? GAME_MODE_4 : (gv.data.mode == "OFK" ? GAME_MODE_OFK : GAME_MODE_2));
        OctiGameBases b = (gv.data.bases == 1 ? GAME_BASES_1 : GAME_BASES_3);
        OctiBoard board(m, gv.data.superProngs, gv.data.edgeless, b);
        if (!gv.data.fen.empty()) {
            board.fromPositionString(gv.data.fen);
        }

        std::ostringstream ss;
        ss << board;
        res["ascii_board"] = ss.str();

        nlohmann::json movesJson = nlohmann::json::array();
        if (board.getGame()) {
            auto legalMoves = board.getGame()->generateLegalMoves();
            for (const auto &mv : legalMoves) {
                movesJson.push_back(mv.asString());
            }
        }
        res["legal_moves"] = movesJson;
        return res.dump(2);
    }

    if (name == "octibot_analyze_position") {
        int topK = args.value("top_k", 3);
        std::string customFen = args.value("fen", "");

        std::string roomId = _runner.currentRoomId();
        std::string mode = "2P";
        int bases = 3;
        bool superProngs = false;
        bool edgeless = false;
        int seat = _runner.currentSeat();
        if (seat == 0) seat = 1;

        std::string fen = customFen;
        if (fen.empty() && !roomId.empty()) {
            auto gv = _runner.networkClient().fetchGameView(roomId);
            if (gv.ok) {
                fen = gv.data.fen;
                mode = gv.data.mode;
                bases = gv.data.bases;
                superProngs = gv.data.superProngs;
                edgeless = gv.data.edgeless;
                seat = gv.data.turnSeat;
            }
        }

        OctiGameMode m = (mode == "4P" ? GAME_MODE_4 : (mode == "OFK" ? GAME_MODE_OFK : GAME_MODE_2));
        OctiGameBases b = (bases == 1 ? GAME_BASES_1 : GAME_BASES_3);
        OctiBoard board(m, superProngs, edgeless, b);
        if (!fen.empty()) {
            board.fromPositionString(fen);
        }

        std::vector<OctiMove> legalMoves;
        if (board.getGame()) {
            legalMoves = board.getGame()->generateLegalMoves();
        }

        struct Candidate {
            std::string notation;
            int score = 0;
            bool isBaseWin = false;
            bool isCapture = false;
        };
        std::vector<Candidate> candidates;

        for (const auto &mv : legalMoves) {
            Candidate c;
            c.notation = mv.asString();
            c.isCapture = (c.notation.find('x') != std::string::npos);

            OctiBoard nextBoard(m, superProngs, edgeless, b);
            nextBoard.fromPositionString(board.toPositionString());
            if (nextBoard.getGame()) {
                nextBoard.getGame()->move(mv);
                c.score = OctiBotEngine::evaluatePosition(nextBoard, static_cast<OctiPlayer>(seat));
                if (nextBoard.getGame()->isGameOver() && nextBoard.getGame()->getWinner() == static_cast<OctiPlayer>(seat)) {
                    c.isBaseWin = true;
                    c.score += 50000;
                }
            }
            candidates.push_back(c);
        }

        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate &c1, const Candidate &c2) { return c1.score > c2.score; });

        nlohmann::json res = nlohmann::json::array();
        int count = std::min(topK, static_cast<int>(candidates.size()));
        for (int i = 0; i < count; ++i) {
            res.push_back({
                {"move", candidates[i].notation},
                {"score", candidates[i].score},
                {"is_base_win", candidates[i].isBaseWin},
                {"is_capture", candidates[i].isCapture}
            });
        }

        nlohmann::json out;
        out["top_candidates"] = res;
        out["total_legal_moves"] = legalMoves.size();
        return out.dump(2);
    }

    if (name == "octibot_game_action") {
        std::string action = args.value("action", "move");
        std::string err;
        nlohmann::json res;
        if (action == "move") {
            std::string moveStr = args.value("move", "");
            bool ok = _runner.submitAgentMove(moveStr, err);
            res["ok"] = ok;
            if (!ok) res["error"] = err;
        } else {
            std::string arg = args.value("arg", "");
            bool ok = _runner.executeGameAction(action, arg, err);
            res["ok"] = ok;
            if (!ok) res["error"] = err;
        }
        return res.dump(2);
    }

    if (name == "octibot_send_chat") {
        std::string channel = args.value("channel", "lobby");
        std::string msg = args.value("message", "");
        std::string scope = args.value("scope", "ALL");
        bool markRead = args.value("mark_inbox_read", true);

        nlohmann::json res;
        if (channel == "lobby") {
            auto r = _runner.networkClient().postLobbyChat(msg);
            res["ok"] = r.ok;
        } else {
            std::string roomId = _runner.currentRoomId();
            auto r = _runner.networkClient().postGameChat(roomId, scope, msg);
            res["ok"] = r.ok;
        }
        if (markRead) {
            _runner.conversationEngine().clearInbox();
        }
        return res.dump(2);
    }

    if (name == "octibot_configure_policy") {
        nlohmann::json res;
        if (args.contains("key") && args.contains("value")) {
            std::string k = args["key"].get<std::string>();
            std::string v = args["value"].get<std::string>();
            _runner.config().set(k, v);
            _runner.policyEngine().configure(_runner.config());
            res["updated"] = {{"key", k}, {"value", v}};

            if (args.value("save_to_file", false)) {
                std::string err;
                _runner.config().saveFile(OctiBotConfigStore::defaultConfigFilePath(), err);
                res["saved"] = err.empty() ? "ok" : err;
            }
        } else {
            res["table_behavior"] = _runner.policyEngine().tableBehavior();
            res["friends_only"] = _runner.policyEngine().friendsOnly();
            res["move_control"] = _runner.moveControl();
        }
        return res.dump(2);
    }

    if (name == "octibot_wait_for_event") {
        int timeoutMs = args.value("timeout_ms", 30000);
        std::string reason;
        bool triggered = _runner.waitForEvent(timeoutMs, reason);

        nlohmann::json res;
        res["triggered"] = triggered;
        res["reason"] = reason;

        auto unread = _runner.conversationEngine().getUnreadInbox();
        nlohmann::json inbox = nlohmann::json::array();
        for (const auto &item : unread) {
            inbox.push_back({
                {"id", item.id},
                {"channel", item.channel},
                {"sender", item.senderHandle},
                {"is_owner", item.isOwner},
                {"text", item.text}
            });
        }
        res["unread_inbox"] = inbox;
        return res.dump(2);
    }

    return "Unknown tool: " + name;
}

void OctiBotMcpServer::runStdio(std::istream &in, std::ostream &out) {
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::string resp = handleJsonRpc(line);
        if (!resp.empty()) {
            out << resp << "\n";
            out.flush();
        }
    }
}

void OctiBotMcpServer::registerHttpRoutes() {
    _httpServer->Get("/", [this](const httplib::Request &req, httplib::Response &res) {
        (void) req;
        res.set_content(renderDashboardHtml(), "text/html; charset=utf-8");
    });

    _httpServer->Get("/api/v1/status", [this](const httplib::Request &req, httplib::Response &res) {
        (void) req;
        res.set_content(buildStatusJson().dump(2), "application/json");
    });

    _httpServer->Get("/api/v1/logs", [this](const httplib::Request &req, httplib::Response &res) {
        std::uint64_t afterSeq = 0;
        if (req.has_param("after_seq")) {
            try {
                afterSeq = std::stoull(req.get_param_value("after_seq"));
            } catch (...) {}
        }
        std::uint64_t oldest = 0, latest = 0;
        auto entries = _runner.getLogsAfter(afterSeq, &oldest, &latest);

        nlohmann::json root;
        root["oldest_seq"] = oldest;
        root["latest_seq"] = latest;
        root["count"] = entries.size();
        nlohmann::json logsArray = nlohmann::json::array();
        for (const auto &e : entries) {
            logsArray.push_back({
                {"seq", e.seq},
                {"timestamp_ms", e.timestampMs},
                {"line", e.line}
            });
        }
        root["logs"] = logsArray;
        res.set_content(root.dump(), "application/json");
    });

    _httpServer->Get("/api/v1/config", [this](const httplib::Request &req, httplib::Response &res) {
        (void) req;
        res.set_content(buildConfigJson().dump(2), "application/json");
    });

    _httpServer->Post("/api/v1/config", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            if (!j.is_object() || !j.contains("updates") || !j["updates"].is_object()) {
                res.status = 400;
                res.set_content("{\"ok\":false,\"error\":\"Missing or invalid 'updates' object\"}", "application/json");
                return;
            }
            std::map<std::string, std::string> updates;
            for (auto it = j["updates"].begin(); it != j["updates"].end(); ++it) {
                if (it.value().is_string()) {
                    updates[it.key()] = it.value().get<std::string>();
                } else if (it.value().is_boolean()) {
                    updates[it.key()] = it.value().get<bool>() ? "true" : "false";
                } else if (it.value().is_number()) {
                    updates[it.key()] = it.value().dump();
                }
            }
            bool saveToDisk = j.value("save_to_disk", false);
            std::string err;
            if (!_runner.applyConfigUpdates(updates, saveToDisk, err)) {
                res.status = 400;
                nlohmann::json errRes = {{"ok", false}, {"error", err}};
                res.set_content(errRes.dump(), "application/json");
                return;
            }
            nlohmann::json okRes = {{"ok", true}, {"message", "Configuration updated successfully."}};
            res.set_content(okRes.dump(), "application/json");
        } catch (const std::exception &e) {
            res.status = 400;
            nlohmann::json errRes = {{"ok", false}, {"error", std::string("JSON parse error: ") + e.what()}};
            res.set_content(errRes.dump(), "application/json");
        }
    });

    _httpServer->Post("/mcp", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content(handleJsonRpc(req.body), "application/json");
    });

    _httpServer->Post("/", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content(handleJsonRpc(req.body), "application/json");
    });

    _httpServer->Post("/message", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content(handleJsonRpc(req.body), "application/json");
    });

    _httpServer->Get("/sse", [](const httplib::Request &req, httplib::Response &res) {
        (void) req;
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_content("event: endpoint\ndata: /message\n\n", "text/event-stream");
    });
}

nlohmann::json OctiBotMcpServer::buildStatusJson() {
    nlohmann::json root;

    // 1. OCTI server telemetry with 3s cached TTL
    {
        std::lock_guard<std::mutex> lock(_healthCacheMutex);
        auto now = std::chrono::steady_clock::now();
        auto diffMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastHealthCheckTime).count();
        if (_lastHealthCheckTime.time_since_epoch().count() == 0 || diffMs > 3000) {
            _lastHealthCheckTime = now;
            auto t0 = std::chrono::steady_clock::now();
            auto res = _runner.networkClient().fetchHealth();
            auto t1 = std::chrono::steady_clock::now();
            int latency = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

            nlohmann::json srv;
            std::string host = _runner.config().getString("server_host", "octi.selfso.com");
            int port = _runner.config().getInt("server_port", 443);
            bool useSsl = _runner.config().getBool("server_use_ssl", true);
            srv["endpoint"] = (useSsl ? "https://" : "http://") + host + ":" + std::to_string(port);
            srv["online"] = res.ok;
            srv["latency_ms"] = latency;
            if (res.ok) {
                srv["version"] = res.data.version;
                srv["uptime_sec"] = res.data.uptimeSec;
                srv["active_tables"] = res.data.activeTables;
                srv["online_players"] = res.data.onlinePlayers;
            } else {
                srv["error"] = res.errorMessage.empty() ? "Unreachable" : res.errorMessage;
            }
            _cachedHealthJson = srv;
        }
        root["server"] = _cachedHealthJson;
    }

    // 2. Robot Identity
    const auto &ident = _runner.identity();
    nlohmann::json robot;
    robot["handle"] = ident.handle.empty() ? _runner.config().getString("robot_name", "stock") : ident.handle;
    robot["uuid"] = ident.robotUuid;
    robot["owner_handle"] = ident.ownerHandle.empty() ? "samurai" : ident.ownerHandle;
    robot["owner_uuid"] = ident.ownerUuid;
    robot["description"] = ident.description;
    robot["profile"] = _runner.config().getString("profile", _runner.config().getString("robot_name", "stock"));
    robot["algorithm"] = _runner.config().getString("algorithm", "alpha_beta");
    robot["move_control"] = _runner.moveControl();
    robot["aimon_connected"] = (_aimonSocketFd >= 0);
    robot["bound_http_port"] = _boundPort.load();
    robot["record"] = {
        {"wins", ident.wins},
        {"losses", ident.losses},
        {"draws", ident.draws},
        {"games_played", ident.gamesPlayed}
    };
    root["robot"] = robot;

    // 3. Active Match
    nlohmann::json match;
    match["room_id"] = _runner.currentRoomId();
    match["phase"] = _runner.currentPhase();
    match["my_seat"] = _runner.currentSeat();
    match["turn_seat"] = _runner.turnSeat();
    match["ply_count"] = _runner.plyCount();
    match["fen"] = _runner.currentFen();
    match["board_ascii"] = _runner.renderAsciiBoard();
    match["ttl_remaining_ms"] = _runner.currentTtlRemainingMs();

    nlohmann::json seatsJson = nlohmann::json::array();
    for (const auto &s : _runner.currentSeats()) {
        seatsJson.push_back({
            {"seat", s.seat},
            {"handle", s.handle},
            {"is_robot", s.isRobot},
            {"clock_ms", s.clockMs}
        });
    }
    match["seats"] = seatsJson;
    root["match"] = match;

    // 4. Search Engine Telemetry & Move History
    nlohmann::json engine;
    engine["is_searching"] = _runner.isSearching();
    engine["live_depth"] = _runner.liveDepth();
    engine["live_nodes"] = _runner.liveNodes();
    engine["live_score"] = _runner.liveScore();

    const auto &lastTelem = _runner.lastTelemetry();
    if (lastTelem.present) {
        engine["last_move"] = {
            {"depth", lastTelem.depthReached},
            {"nodes", lastTelem.nodesVisited},
            {"eval_score", lastTelem.evalScore},
            {"elapsed_ms", lastTelem.elapsedMs}
        };
    } else {
        engine["last_move"] = nullptr;
    }

    nlohmann::json historyJson = nlohmann::json::array();
    for (const auto &h : _runner.getMoveHistory()) {
        historyJson.push_back({
            {"ply", h.ply},
            {"move", h.move},
            {"depth", h.depthReached},
            {"nodes", h.nodesVisited},
            {"eval_score", h.evalScore},
            {"elapsed_ms", h.elapsedMs},
            {"timestamp_ms", h.timestampMs}
        });
    }
    engine["move_history"] = historyJson;
    root["engine"] = engine;

    return root;
}

nlohmann::json OctiBotMcpServer::buildConfigJson() const {
    nlohmann::json root;
    nlohmann::json entries = nlohmann::json::object();
    for (const auto &kv : _runner.config().entries()) {
        if (kv.first == "bot_token" || kv.first.rfind("bot_token_", 0) == 0) {
            continue;
        }
        entries[kv.first] = kv.second;
    }
    root["entries"] = entries;

    nlohmann::json profiles = nlohmann::json::object();
    for (const auto &prof : _runner.config().profiles()) {
        nlohmann::json pEntries = nlohmann::json::object();
        for (const auto &kv : prof.second) {
            if (kv.first == "bot_token" || kv.first.rfind("bot_token_", 0) == 0) {
                continue;
            }
            pEntries[kv.first] = kv.second;
        }
        profiles[prof.first] = pEntries;
    }
    root["profiles"] = profiles;
    root["active_profile"] = _runner.config().getString("profile", _runner.config().getString("robot_name", "stock"));

    root["locked_keys"] = {
        "aimon_enabled", "aimon_host", "aimon_port", "aimon_gateway_port",
        "expose_mcp_tools", "web_port", "web_bind", "server_host",
        "server_port", "server_use_ssl", "default_profile", "robot_name",
        "bot_token"
    };

    return root;
}

std::string OctiBotMcpServer::renderDashboardHtml() const {
    std::string h = _runner.identity().handle;
    if (h.empty()) h = _runner.config().getString("robot_name", "stock");

    std::ostringstream oss;
    oss << R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>OCTIBOT Dashboard - )HTML" << h << R"HTML(</title>
<style>
:root {
  --bg-main: #0c0f17;
  --bg-card: #151a24;
  --bg-input: #0b0e14;
  --bg-code: #0a0d13;
  --border-subtle: #202738;
  --border-focus: #3b82f6;
  --text-main: #f8fafc;
  --text-muted: #94a3b8;
  --text-dim: #64748b;
  --accent-blue: #3b82f6;
  --accent-cyan: #06b6d4;
  --accent-green: #10b981;
  --accent-amber: #f59e0b;
  --accent-red: #ef4444;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  background: var(--bg-main);
  color: var(--text-main);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  line-height: 1.5;
  padding: 16px 24px;
}
header {
  display: flex;
  flex-wrap: wrap;
  justify-content: space-between;
  align-items: center;
  padding-bottom: 16px;
  border-bottom: 1px solid var(--border-subtle);
  margin-bottom: 20px;
}
.brand { display: flex; align-items: center; gap: 12px; }
.brand-icon { font-size: 26px; }
.brand-title { font-size: 20px; font-weight: 700; letter-spacing: -0.02em; }
.brand-subtitle { font-size: 13px; color: var(--text-muted); }
.badges { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
.badge {
  padding: 4px 10px;
  border-radius: 9999px;
  font-size: 12px;
  font-weight: 600;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  background: var(--border-subtle);
  color: var(--text-main);
  border: 1px solid rgba(255,255,255,0.05);
}
.badge-green { background: rgba(16, 185, 129, 0.15); color: #34d399; border-color: rgba(16, 185, 129, 0.3); }
.badge-amber { background: rgba(245, 158, 11, 0.15); color: #fbbf24; border-color: rgba(245, 158, 11, 0.3); }
.badge-red { background: rgba(239, 68, 68, 0.15); color: #f87171; border-color: rgba(239, 68, 68, 0.3); }
.badge-blue { background: rgba(59, 130, 246, 0.15); color: #60a5fa; border-color: rgba(59, 130, 246, 0.3); }
.dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; }
.dot-green { background: #10b981; }
.dot-amber { background: #f59e0b; }
.dot-red { background: #ef4444; }
.pulsing { animation: pulse 1.5s infinite; }
@keyframes pulse {
  0% { transform: scale(0.95); opacity: 0.8; }
  50% { transform: scale(1.15); opacity: 1; }
  100% { transform: scale(0.95); opacity: 0.8; }
}
.grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
  gap: 20px;
  margin-bottom: 24px;
}
.card {
  background: var(--bg-card);
  border: 1px solid var(--border-subtle);
  border-radius: 10px;
  padding: 18px 20px;
  display: flex;
  flex-direction: column;
  gap: 14px;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
}
.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  border-bottom: 1px solid var(--border-subtle);
  padding-bottom: 10px;
}
.card-title {
  font-size: 15px;
  font-weight: 600;
  color: var(--text-main);
  display: flex;
  align-items: center;
  gap: 8px;
}
.kv-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 13px;
}
.kv-table td {
  padding: 5px 0;
}
.kv-table td:first-child {
  color: var(--text-muted);
  width: 42%;
}
.kv-table td:last-child {
  font-weight: 500;
  color: var(--text-main);
  text-align: right;
  font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, monospace;
}
.board-ascii {
  background: var(--bg-code);
  border: 1px solid var(--border-subtle);
  border-radius: 6px;
  padding: 10px 12px;
  font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, monospace;
  font-size: 11.5px;
  line-height: 1.25;
  color: #38bdf8;
  white-space: pre;
  overflow-x: auto;
  max-height: 220px;
}
.table-scroll {
  max-height: 200px;
  overflow-y: auto;
  border: 1px solid var(--border-subtle);
  border-radius: 6px;
}
.move-history-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 12px;
  font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, monospace;
}
.move-history-table th {
  text-align: left;
  color: var(--text-muted);
  font-weight: 600;
  border-bottom: 1px solid var(--border-subtle);
  padding: 6px 6px;
  background: #11151e;
  position: sticky;
  top: 0;
}
.move-history-table td {
  padding: 5px 6px;
  border-bottom: 1px solid rgba(255,255,255,0.03);
}
.console-card { grid-column: 1 / -1; }
.console-controls {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  align-items: center;
  font-size: 13px;
}
.console-output {
  background: var(--bg-code);
  border: 1px solid var(--border-subtle);
  border-radius: 8px;
  padding: 12px 14px;
  font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, monospace;
  font-size: 12px;
  line-height: 1.45;
  color: #e2e8f0;
  height: 360px;
  overflow-y: auto;
  white-space: pre-wrap;
  word-break: break-all;
}
.config-card { grid-column: 1 / -1; }
.config-tabs {
  display: flex;
  gap: 6px;
  border-bottom: 1px solid var(--border-subtle);
  padding-bottom: 8px;
  margin-bottom: 16px;
  flex-wrap: wrap;
}
.config-tab-btn {
  background: transparent;
  color: var(--text-muted);
  border: 1px solid transparent;
  padding: 6px 14px;
  border-radius: 6px;
  cursor: pointer;
  font-size: 13px;
  font-weight: 500;
  transition: all 0.15s ease;
}
.config-tab-btn:hover { color: var(--text-main); background: rgba(255,255,255,0.05); }
.config-tab-btn.active { color: var(--text-main); background: var(--border-subtle); border-color: rgba(255,255,255,0.1); }
.config-section { display: none; }
.config-section.active {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
  gap: 16px;
}
.field-group { display: flex; flex-direction: column; gap: 4px; }
.field-group label {
  font-size: 12px;
  font-weight: 600;
  color: var(--text-muted);
  display: flex;
  justify-content: space-between;
}
.field-desc { font-size: 11px; color: var(--text-dim); }
.input-control {
  background: var(--bg-input);
  border: 1px solid var(--border-subtle);
  border-radius: 6px;
  color: var(--text-main);
  padding: 7px 10px;
  font-size: 13px;
  outline: none;
}
.input-control:focus { border-color: var(--border-focus); }
.input-control:disabled {
  background: rgba(255,255,255,0.02);
  color: var(--text-dim);
  cursor: not-allowed;
  border-color: rgba(239, 68, 68, 0.2);
}
.btn-bar { display: flex; gap: 12px; margin-top: 18px; align-items: center; }
.btn {
  padding: 8px 16px;
  border-radius: 6px;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
  border: none;
  transition: all 0.15s ease;
}
.btn-primary { background: #2563eb; color: #fff; }
.btn-primary:hover { background: #1d4ed8; }
.btn-secondary { background: #334155; color: #f1f5f9; }
.btn-secondary:hover { background: #475569; }
.btn-sm { padding: 4px 10px; font-size: 12px; }
.alert-toast {
  padding: 10px 14px;
  border-radius: 6px;
  font-size: 13px;
  display: none;
  margin-top: 12px;
}
.alert-success { background: rgba(16, 185, 129, 0.15); color: #34d399; border: 1px solid rgba(16, 185, 129, 0.3); }
.alert-error { background: rgba(239, 68, 68, 0.15); color: #f87171; border: 1px solid rgba(239, 68, 68, 0.3); }
</style>
</head>
<body>

<header>
  <div class="brand">
    <div class="brand-icon">&#x265F;</div>
    <div>
      <div class="brand-title">OCTIBOT Telemetry & Control</div>
      <div class="brand-subtitle">Autonomous OCTI Engine & MCP Server</div>
    </div>
  </div>
  <div class="badges">
    <span class="badge badge-blue"><span class="dot dot-green"></span> @<span id="badgeHandle">)HTML" << h << R"HTML(</span></span>
    <span class="badge" id="badgeServer"><span class="dot dot-amber"></span> Server: checking...</span>
    <span class="badge" id="badgeAimon"><span class="dot dot-amber"></span> Aimon: checking...</span>
    <span class="badge" id="badgeRoom">Room: #none</span>
    <span class="badge badge-amber" id="badgePhase">IDLE</span>
  </div>
</header>

<div class="grid">
  <!-- OCTI Server Telemetry -->
  <div class="card">
    <div class="card-header">
      <div class="card-title">&#x1F310; OCTI Server Telemetry</div>
      <span class="badge badge-green" id="serverHealthBadge">ONLINE</span>
    </div>
    <table class="kv-table">
      <tr><td>Server Endpoint</td><td id="srvEndpoint">-</td></tr>
      <tr><td>Connection Status</td><td id="srvStatus">-</td></tr>
      <tr><td>Round-Trip Latency</td><td id="srvLatency">-</td></tr>
      <tr><td>Server Version</td><td id="srvVersion">-</td></tr>
      <tr><td>Server Uptime</td><td id="srvUptime">-</td></tr>
      <tr><td>Online Players</td><td id="srvPlayers">-</td></tr>
      <tr><td>Active Tables</td><td id="srvTables">-</td></tr>
    </table>
  </div>

  <!-- Robot Identity & Match -->
  <div class="card">
    <div class="card-header">
      <div class="card-title">&#x1F916; Robot Identity & Active Match</div>
      <span class="badge badge-blue" id="botMoveControl">engine</span>
    </div>
    <table class="kv-table">
      <tr><td>Robot Handle / UUID</td><td id="botHandleUuid">-</td></tr>
      <tr><td>Owner Handle</td><td id="botOwner">-</td></tr>
      <tr><td>Lifetime Record</td><td id="botRecord">-</td></tr>
      <tr><td>Active Profile / Algo</td><td id="botProfileAlgo">-</td></tr>
      <tr><td>Active Room / Phase</td><td id="botRoomPhase">-</td></tr>
      <tr><td>Seat / Turn / Ply</td><td id="botSeatTurnPly">-</td></tr>
      <tr><td>Inactivity TTL</td><td id="botTtl">-</td></tr>
    </table>
    <div style="font-size:12px;font-weight:600;color:var(--text-muted);margin-top:4px;">Board Preview (ASCII):</div>
    <div class="board-ascii" id="boardAscii">Loading board...</div>
  </div>

  <!-- Live Search Engine Telemetry & Move History -->
  <div class="card" style="grid-column: 1 / -1;">
    <div class="card-header">
      <div class="card-title">&#x2699;&#xFE0F; Live Search Engine Telemetry & History</div>
      <span class="badge" id="engineStatusBadge"><span class="dot dot-amber"></span> IDLE</span>
    </div>
    <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;">
      <div style="background:var(--bg-code);padding:10px 14px;border-radius:6px;border:1px solid var(--border-subtle);">
        <div style="font-size:11px;color:var(--text-muted);font-weight:600;">LIVE SEARCH DEPTH</div>
        <div style="font-size:20px;font-weight:700;color:#38bdf8;" id="liveDepth">0</div>
      </div>
      <div style="background:var(--bg-code);padding:10px 14px;border-radius:6px;border:1px solid var(--border-subtle);">
        <div style="font-size:11px;color:var(--text-muted);font-weight:600;">LIVE NODES VISITED</div>
        <div style="font-size:20px;font-weight:700;color:#34d399;" id="liveNodes">0</div>
      </div>
      <div style="background:var(--bg-code);padding:10px 14px;border-radius:6px;border:1px solid var(--border-subtle);">
        <div style="font-size:11px;color:var(--text-muted);font-weight:600;">LIVE EVAL SCORE</div>
        <div style="font-size:20px;font-weight:700;color:#fbbf24;" id="liveScore">0</div>
      </div>
      <div style="background:var(--bg-code);padding:10px 14px;border-radius:6px;border:1px solid var(--border-subtle);">
        <div style="font-size:11px;color:var(--text-muted);font-weight:600;">LAST COMPLETED MOVE</div>
        <div style="font-size:16px;font-weight:700;color:#f8fafc;" id="lastMoveSummary">-</div>
      </div>
    </div>
    <div style="margin-top:6px;">
      <div style="font-size:12px;font-weight:600;color:var(--text-muted);margin-bottom:6px;">Move Telemetry History (Last 50 Moves):</div>
      <div class="table-scroll">
        <table class="move-history-table">
          <thead>
            <tr><th>Ply</th><th>Move</th><th>Depth</th><th>Nodes</th><th>Eval Score</th><th>Elapsed</th><th>Timestamp</th></tr>
          </thead>
          <tbody id="moveHistoryBody">
            <tr><td colspan="7" style="color:var(--text-dim);text-align:center;">No completed moves in current session.</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- Read-Only Monospace Console Viewer -->
  <div class="card console-card">
    <div class="card-header">
      <div class="card-title">&#x1F4DF; Live stdout Console (5,000 Lines Ring Buffer)</div>
      <div class="console-controls">
        <label><input type="checkbox" id="chkAutoScroll" checked> Auto-scroll</label>
        <input type="text" id="logFilter" class="input-control" placeholder="Filter logs..." style="padding:4px 8px;font-size:12px;width:160px;">
        <button class="btn btn-secondary btn-sm" id="btnClearConsole">Clear View</button>
        <span style="font-size:11px;color:var(--text-dim);" id="logStats">Lines: 0 / 5000</span>
      </div>
    </div>
    <div class="console-output" id="consoleOutput">Connecting to log stream...</div>
  </div>

  <!-- Interactive Configuration Editor -->
  <div class="card config-card">
    <div class="card-header">
      <div class="card-title">&#x2699;&#xFE0F; Interactive Configuration Editor</div>
      <span style="font-size:12px;color:var(--text-muted);">Hot-Reload & Safe Disk Persistence</span>
    </div>

    <div class="config-tabs">
      <button class="config-tab-btn active" onclick="switchConfigTab('tab-search')">Search & Engine Limits</button>
      <button class="config-tab-btn" onclick="switchConfigTab('tab-eval')">Board & Tactical Weights</button>
      <button class="config-tab-btn" onclick="switchConfigTab('tab-policy')">Table & Matchmaking Policy</button>
      <button class="config-tab-btn" onclick="switchConfigTab('tab-chat')">Chat, Proposals & AI CLI</button>
      <button class="config-tab-btn" onclick="switchConfigTab('tab-locked')">&#x1F512; Locked Connection & Aimon</button>
    </div>

    <!-- Tab 1: Search & Engine Limits -->
    <div class="config-section active" id="tab-search">
      <div class="field-group">
        <label>time_budget_ms <span class="field-desc">ms per move search</span></label>
        <input type="number" id="cfg_time_budget_ms" class="input-control" min="50" max="120000" step="50">
      </div>
      <div class="field-group">
        <label>max_search_depth <span class="field-desc">ply depth limit</span></label>
        <input type="number" id="cfg_max_search_depth" class="input-control" min="1" max="64">
      </div>
      <div class="field-group">
        <label>use_iterative_deepening</label>
        <select id="cfg_use_iterative_deepening" class="input-control">
          <option value="true">true</option><option value="false">false</option>
        </select>
      </div>
      <div class="field-group">
        <label>use_transposition_table</label>
        <select id="cfg_use_transposition_table" class="input-control">
          <option value="true">true</option><option value="false">false</option>
        </select>
      </div>
      <div class="field-group">
        <label>tt_size_mb <span class="field-desc">hash size in megabytes</span></label>
        <input type="number" id="cfg_tt_size_mb" class="input-control" min="1" max="4096">
      </div>
      <div class="field-group">
        <label>use_quiescence_search</label>
        <select id="cfg_use_quiescence_search" class="input-control">
          <option value="true">true</option><option value="false">false</option>
        </select>
      </div>
      <div class="field-group">
        <label>null_move_pruning</label>
        <select id="cfg_null_move_pruning" class="input-control">
          <option value="true">true</option><option value="false">false</option>
        </select>
      </div>
      <div class="field-group">
        <label>aspiration_windows</label>
        <select id="cfg_aspiration_windows" class="input-control">
          <option value="true">true</option><option value="false">false</option>
        </select>
      </div>
    </div>

    <!-- Tab 2: Board & Tactical Weights -->
    <div class="config-section" id="tab-eval">
      <div class="field-group">
        <label>weight_material <span class="field-desc">pod score multiplier</span></label>
        <input type="number" id="cfg_weight_material" class="input-control">
      </div>
      <div class="field-group">
        <label>weight_base_distance <span class="field-desc">goal approach bonus</span></label>
        <input type="number" id="cfg_weight_base_distance" class="input-control">
      </div>
      <div class="field-group">
        <label>weight_prong_count <span class="field-desc">prong investment score</span></label>
        <input type="number" id="cfg_weight_prong_count" class="input-control">
      </div>
      <div class="field-group">
        <label>weight_super_prong <span class="field-desc">super prong bonus</span></label>
        <input type="number" id="cfg_weight_super_prong" class="input-control">
      </div>
      <div class="field-group">
        <label>weight_central_control <span class="field-desc">board center positioning</span></label>
        <input type="number" id="cfg_weight_central_control" class="input-control">
      </div>
      <div class="field-group">
        <label>weight_mobility <span class="field-desc">candidate moves freedom</span></label>
        <input type="number" id="cfg_weight_mobility" class="input-control">
      </div>
    </div>

    <!-- Tab 3: Table & Matchmaking Policy -->
    <div class="config-section" id="tab-policy">
      <div class="field-group">
        <label>table_behavior <span class="field-desc">matchmaking action</span></label>
        <select id="cfg_table_behavior" class="input-control">
          <option value="auto">auto</option>
          <option value="host_only">host_only</option>
          <option value="join_only">join_only</option>
          <option value="idle">idle</option>
        </select>
      </div>
      <div class="field-group">
        <label>game_mode <span class="field-desc">hosted match variant</span></label>
        <select id="cfg_game_mode" class="input-control">
          <option value="2P">2P</option>
          <option value="4P">4P</option>
        </select>
      </div>
      <div class="field-group">
        <label>bases <span class="field-desc">number of bases</span></label>
        <input type="number" id="cfg_bases" class="input-control" min="1" max="4">
      </div>
      <div class="field-group">
        <label>time_control <span class="field-desc">room clock setting</span></label>
        <input type="text" id="cfg_time_control" class="input-control">
      </div>
      <div class="field-group">
        <label>auto_accept_rematch</label>
        <select id="cfg_auto_accept_rematch" class="input-control">
          <option value="true">true</option><option value="false">false</option>
        </select>
      </div>
      <div class="field-group">
        <label>max_consecutive_games</label>
        <input type="number" id="cfg_max_consecutive_games" class="input-control" min="1" max="100">
      </div>
      <div class="field-group">
        <label>auto_ready_timeout_ms</label>
        <input type="number" id="cfg_auto_ready_timeout_ms" class="input-control" min="100" max="60000">
      </div>
      <div class="field-group">
        <label>post_game_leave_delay_ms</label>
        <input type="number" id="cfg_post_game_leave_delay_ms" class="input-control" min="0" max="60000">
      </div>
    </div>

    <!-- Tab 4: Chat, Telemetry & AI CLI -->
    <div class="config-section" id="tab-chat">
      <div class="field-group">
        <label>move_control <span class="field-desc">engine vs external agent</span></label>
        <select id="cfg_move_control" class="input-control">
          <option value="engine">engine</option>
          <option value="agent">agent</option>
        </select>
      </div>
      <div class="field-group">
        <label>publish_move_telemetry <span class="field-desc">send depth/eval in move RPC</span></label>
        <select id="cfg_publish_move_telemetry" class="input-control">
          <option value="true">true</option><option value="false">false</option>
        </select>
      </div>
      <div class="field-group">
        <label>announce_move_search_chat <span class="field-desc">post search stats to room chat</span></label>
        <select id="cfg_announce_move_search_chat" class="input-control">
          <option value="true">true</option><option value="false">false</option>
        </select>
      </div>
      <div class="field-group">
        <label>agent_move_timeout_ms</label>
        <input type="number" id="cfg_agent_move_timeout_ms" class="input-control" min="100" max="60000">
      </div>
      <div class="field-group">
        <label>ai_cli_backend <span class="field-desc">agy, claude, cursor-agent, none</span></label>
        <input type="text" id="cfg_ai_cli_backend" class="input-control">
      </div>
      <div class="field-group">
        <label>ai_cli_model <span class="field-desc">LLM model string</span></label>
        <input type="text" id="cfg_ai_cli_model" class="input-control">
      </div>
      <div class="field-group">
        <label>verbosity <span class="field-desc">console log level (0..4)</span></label>
        <input type="number" id="cfg_verbosity" class="input-control" min="0" max="4">
      </div>
    </div>

    <!-- Tab 5: Locked Connection & Aimon Settings -->
    <div class="config-section" id="tab-locked">
      <div class="field-group">
        <label>aimon_enabled <span class="badge badge-red">&#x1F512; Locked</span></label>
        <input type="text" id="cfg_aimon_enabled" class="input-control" disabled>
      </div>
      <div class="field-group">
        <label>aimon_host <span class="badge badge-red">&#x1F512; Locked</span></label>
        <input type="text" id="cfg_aimon_host" class="input-control" disabled>
      </div>
      <div class="field-group">
        <label>aimon_port <span class="badge badge-red">&#x1F512; Locked</span></label>
        <input type="text" id="cfg_aimon_port" class="input-control" disabled>
      </div>
      <div class="field-group">
        <label>aimon_gateway_port <span class="badge badge-red">&#x1F512; Locked</span></label>
        <input type="text" id="cfg_aimon_gateway_port" class="input-control" disabled>
      </div>
      <div class="field-group">
        <label>web_port <span class="badge badge-red">&#x1F512; Locked</span></label>
        <input type="text" id="cfg_web_port" class="input-control" disabled>
      </div>
      <div class="field-group">
        <label>server_host <span class="badge badge-red">&#x1F512; Locked</span></label>
        <input type="text" id="cfg_server_host" class="input-control" disabled>
      </div>
      <div class="field-group">
        <label>server_port <span class="badge badge-red">&#x1F512; Locked</span></label>
        <input type="text" id="cfg_server_port" class="input-control" disabled>
      </div>
      <div class="field-group">
        <label>robot_name <span class="badge badge-red">&#x1F512; Locked</span></label>
        <input type="text" id="cfg_robot_name" class="input-control" disabled>
      </div>
    </div>

    <div class="btn-bar">
      <button class="btn btn-primary" onclick="submitConfig(true)">Apply & Save to Disk</button>
      <button class="btn btn-secondary" onclick="submitConfig(false)">Apply in Memory Only</button>
      <span style="font-size:12px;color:var(--text-dim);" id="configFeedback"></span>
    </div>
    <div class="alert-toast" id="configAlert"></div>
  </div>
</div>

<script>
let lastLogSeq = 0;
let cachedLogs = [];
let allConfigKeys = [];

function switchConfigTab(tabId) {
  document.querySelectorAll('.config-tab-btn').forEach(b => b.classList.remove('active'));
  document.querySelectorAll('.config-section').forEach(s => s.classList.remove('active'));
  event.target.classList.add('active');
  const sec = document.getElementById(tabId);
  if (sec) sec.classList.add('active');
}

async function pollStatus() {
  try {
    const res = await fetch('/api/v1/status');
    if (!res.ok) return;
    const data = await res.json();

    // Server
    if (data.server) {
      document.getElementById('srvEndpoint').textContent = data.server.endpoint || '-';
      document.getElementById('srvStatus').textContent = data.server.online ? 'Online' : 'Offline';
      document.getElementById('srvLatency').textContent = (data.server.latency_ms != null) ? data.server.latency_ms + ' ms' : '-';
      document.getElementById('srvVersion').textContent = data.server.version || '-';
      document.getElementById('srvUptime').textContent = data.server.uptime_sec ? formatUptime(data.server.uptime_sec) : '-';
      document.getElementById('srvPlayers').textContent = data.server.online_players != null ? data.server.online_players : '-';
      document.getElementById('srvTables').textContent = data.server.active_tables != null ? data.server.active_tables : '-';

      const sBadge = document.getElementById('badgeServer');
      const sHlth = document.getElementById('serverHealthBadge');
      if (data.server.online) {
        sBadge.innerHTML = '<span class="dot dot-green"></span> Server: ' + (data.server.latency_ms || 0) + 'ms';
        sBadge.className = 'badge badge-green';
        sHlth.textContent = 'ONLINE (' + (data.server.latency_ms || 0) + 'ms)';
        sHlth.className = 'badge badge-green';
      } else {
        sBadge.innerHTML = '<span class="dot dot-red"></span> Server: Offline';
        sBadge.className = 'badge badge-red';
        sHlth.textContent = 'OFFLINE';
        sHlth.className = 'badge badge-red';
      }
    }

    // Robot & Match
    if (data.robot) {
      document.getElementById('badgeHandle').textContent = data.robot.handle || 'octibot';
      document.getElementById('botHandleUuid').textContent = '@' + data.robot.handle + ' (' + (data.robot.uuid || 'no-uuid').substring(0,8) + '...)';
      document.getElementById('botOwner').textContent = '@' + (data.robot.owner_handle || 'none');
      if (data.robot.record) {
        document.getElementById('botRecord').textContent = data.robot.record.wins + 'W / ' + data.robot.record.losses + 'L / ' + data.robot.record.draws + 'D';
      }
      document.getElementById('botProfileAlgo').textContent = (data.robot.profile || 'stock') + ' / ' + (data.robot.algorithm || 'alpha_beta');
      document.getElementById('botMoveControl').textContent = data.robot.move_control || 'engine';

      const aimonBadge = document.getElementById('badgeAimon');
      if (data.robot.aimon_connected) {
        aimonBadge.innerHTML = '<span class="dot dot-green"></span> Aimon: Connected';
        aimonBadge.className = 'badge badge-green';
      } else {
        aimonBadge.innerHTML = '<span class="dot dot-dim"></span> Aimon: Standalone';
        aimonBadge.className = 'badge';
      }
    }

    if (data.match) {
      document.getElementById('badgeRoom').textContent = data.match.room_id ? ('#' + data.match.room_id) : 'Room: #none';
      document.getElementById('badgePhase').textContent = data.match.phase || 'IDLE';
      document.getElementById('botRoomPhase').textContent = (data.match.room_id ? ('#' + data.match.room_id) : 'None') + ' / ' + (data.match.phase || 'IDLE');
      document.getElementById('botSeatTurnPly').textContent = 'Seat ' + (data.match.my_seat || 0) + ' / Turn ' + (data.match.turn_seat || 0) + ' / Ply ' + (data.match.ply_count || 0);
      document.getElementById('botTtl').textContent = data.match.ttl_remaining_ms ? formatMs(data.match.ttl_remaining_ms) : '59:59';
      if (data.match.board_ascii) {
        document.getElementById('boardAscii').textContent = data.match.board_ascii;
      }
    }

    // Engine
    if (data.engine) {
      const eBadge = document.getElementById('engineStatusBadge');
      if (data.engine.is_searching) {
        eBadge.innerHTML = '<span class="dot dot-green pulsing"></span> SEARCHING...';
        eBadge.className = 'badge badge-green';
      } else {
        eBadge.innerHTML = '<span class="dot dot-amber"></span> IDLE';
        eBadge.className = 'badge badge-amber';
      }
      document.getElementById('liveDepth').textContent = data.engine.live_depth || 0;
      document.getElementById('liveNodes').textContent = (data.engine.live_nodes || 0).toLocaleString();
      document.getElementById('liveScore').textContent = data.engine.live_score || 0;

      if (data.engine.last_move) {
        const lm = data.engine.last_move;
        document.getElementById('lastMoveSummary').textContent = 'depth=' + lm.depth + ' | nodes=' + lm.nodes.toLocaleString() + ' | score=' + lm.eval_score + ' (' + lm.elapsed_ms + 'ms)';
      }

      if (data.engine.move_history && Array.isArray(data.engine.move_history)) {
        renderMoveHistory(data.engine.move_history);
      }
    }
  } catch (err) {
    console.error('pollStatus error', err);
  }
}

function renderMoveHistory(history) {
  const tbody = document.getElementById('moveHistoryBody');
  if (!history || history.length === 0) {
    tbody.innerHTML = '<tr><td colspan="7" style="color:var(--text-dim);text-align:center;">No completed moves in current session.</td></tr>';
    return;
  }
  let html = '';
  for (let i = history.length - 1; i >= 0; --i) {
    const h = history[i];
    const timeStr = h.timestamp_ms ? new Date(h.timestamp_ms).toLocaleTimeString() : '-';
    html += `<tr><td>${h.ply}</td><td style="color:#38bdf8;font-weight:600;">${h.move}</td><td>${h.depth}</td><td>${(h.nodes || 0).toLocaleString()}</td><td>${h.eval_score}</td><td>${h.elapsed_ms}ms</td><td style="color:var(--text-dim);">${timeStr}</td></tr>`;
  }
  tbody.innerHTML = html;
}

async function pollLogs() {
  try {
    const res = await fetch('/api/v1/logs?after_seq=' + lastLogSeq);
    if (!res.ok) return;
    const data = await res.json();
    if (data.logs && data.logs.length > 0) {
      for (const entry of data.logs) {
        cachedLogs.push(entry.line);
        if (entry.seq > lastLogSeq) {
          lastLogSeq = entry.seq;
        }
      }
      // Cap at 5,000 lines
      if (cachedLogs.length > 5000) {
        cachedLogs = cachedLogs.slice(cachedLogs.length - 5000);
      }
      renderLogs();
    }
    document.getElementById('logStats').textContent = 'Lines: ' + cachedLogs.length + ' / 5000 (Latest seq: ' + (data.latest_seq || 0) + ')';
  } catch (err) {
    console.error('pollLogs error', err);
  }
}

function renderLogs() {
  const filter = (document.getElementById('logFilter').value || '').toLowerCase();
  const out = document.getElementById('consoleOutput');
  const filtered = filter ? cachedLogs.filter(l => l.toLowerCase().includes(filter)) : cachedLogs;
  out.textContent = filtered.join('\n');
  if (document.getElementById('chkAutoScroll').checked) {
    out.scrollTop = out.scrollHeight;
  }
}

document.getElementById('logFilter').addEventListener('input', renderLogs);
document.getElementById('btnClearConsole').addEventListener('click', () => {
  cachedLogs = [];
  renderLogs();
});

async function loadConfig() {
  try {
    const res = await fetch('/api/v1/config');
    if (!res.ok) return;
    const data = await res.json();
    const entries = data.entries || {};
    allConfigKeys = Object.keys(entries);

    for (const [k, v] of Object.entries(entries)) {
      const el = document.getElementById('cfg_' + k);
      if (el) {
        el.value = v;
      }
    }
  } catch (err) {
    console.error('loadConfig error', err);
  }
}

async function submitConfig(saveToDisk) {
  const alertEl = document.getElementById('configAlert');
  alertEl.style.display = 'none';

  const updates = {};
  const fieldIds = [
    'time_budget_ms', 'max_search_depth', 'use_iterative_deepening', 'use_transposition_table',
    'tt_size_mb', 'use_quiescence_search', 'null_move_pruning', 'aspiration_windows',
    'weight_material', 'weight_base_distance', 'weight_prong_count', 'weight_super_prong',
    'weight_central_control', 'weight_mobility',
    'table_behavior', 'game_mode', 'bases', 'time_control', 'auto_accept_rematch',
    'max_consecutive_games', 'auto_ready_timeout_ms', 'post_game_leave_delay_ms',
    'move_control', 'publish_move_telemetry', 'announce_move_search_chat',
    'agent_move_timeout_ms', 'ai_cli_backend', 'ai_cli_model', 'verbosity'
  ];

  for (const id of fieldIds) {
    const el = document.getElementById('cfg_' + id);
    if (el && !el.disabled) {
      updates[id] = el.value;
    }
  }

  try {
    const res = await fetch('/api/v1/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ updates, save_to_disk: saveToDisk })
    });
    const result = await res.json();
    alertEl.style.display = 'block';
    if (res.ok && result.ok) {
      alertEl.className = 'alert-toast alert-success';
      alertEl.textContent = (result.message || 'Config applied.') + (saveToDisk ? ' (Saved to disk)' : ' (Memory only)');
    } else {
      alertEl.className = 'alert-toast alert-error';
      alertEl.textContent = 'Error: ' + (result.error || 'Failed to update config');
    }
    setTimeout(() => { alertEl.style.display = 'none'; }, 5000);
  } catch (err) {
    alertEl.style.display = 'block';
    alertEl.className = 'alert-toast alert-error';
    alertEl.textContent = 'Network error submitting configuration: ' + err.message;
  }
}

function formatUptime(sec) {
  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);
  return (d > 0 ? d + 'd ' : '') + h + 'h ' + m + 'm';
}

function formatMs(ms) {
  if (ms <= 0) return '00:00';
  const totalSec = Math.floor(ms / 1000);
  const m = Math.floor(totalSec / 60);
  const s = totalSec % 60;
  return (m < 10 ? '0' : '') + m + ':' + (s < 10 ? '0' : '') + s;
}

// Initial bootstrap
pollStatus();
pollLogs();
loadConfig();
setInterval(pollStatus, 2000);
setInterval(pollLogs, 1000);
</script>
</body>
</html>
)HTML";
    return oss.str();
}

bool OctiBotMcpServer::startHttpServer(int port, const std::string &bindAddr) {
    stopHttpServer();

    _httpServer = std::make_unique<httplib::Server>();
    registerHttpRoutes();

    int actualPort = 0;
    if (port <= 0) {
        actualPort = _httpServer->bind_to_any_port(bindAddr);
        if (actualPort <= 0) {
            return false;
        }
    } else {
        if (!_httpServer->bind_to_port(bindAddr, port)) {
            return false;
        }
        actualPort = port;
    }
    _boundPort = actualPort;

    _httpThread = std::make_unique<std::thread>([this]() {
        _httpServer->listen_after_bind();
    });

    _httpServer->wait_until_ready();

    return true;
}

void OctiBotMcpServer::stopHttpServer() {
    if (_httpServer) {
        _httpServer->stop();
    }
    if (_httpThread && _httpThread->joinable()) {
        _httpThread->join();
        _httpThread.reset();
    }
    _httpServer.reset();
    _boundPort = 0;
}

void OctiBotMcpServer::startAimonBridge() {
    if (_aimonRunning) return;

    if (_boundPort == 0) {
        int webPort = _runner.config().getInt("web_port", 3888);
        std::string webBind = _runner.config().getString("web_bind", "0.0.0.0");
        if (webPort > 0) {
            if (!startHttpServer(webPort, webBind)) {
                startHttpServer(0, webBind);
            }
        } else {
            startHttpServer(0, webBind);
        }
    }
    _runner.setMcpServerPort(_boundPort.load());

    _aimonRunning = true;

    _aimonThread = std::make_unique<std::thread>([this]() {
        std::string host = _runner.config().getString("aimon_host", "127.0.0.1");
        int gwPort = _runner.config().getInt("aimon_gateway_port", 0);
        int aimonPort = _runner.config().getInt("aimon_port", 3883);
        int port = (gwPort > 0) ? gwPort : ((aimonPort == 3883) ? 3885 : aimonPort);

        while (_aimonRunning) {
#if !defined(_WIN32)
            struct addrinfo hints, *res = nullptr;
            std::memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            std::string portStr = std::to_string(port);
            if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
                if (res) {
                    ::freeaddrinfo(res);
                    res = nullptr;
                }
                std::this_thread::sleep_for(std::chrono::seconds(3));
                continue;
            }

            int sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sock < 0) {
                ::freeaddrinfo(res);
                res = nullptr;
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            if (::connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
                ::close(sock);
                ::freeaddrinfo(res);
                res = nullptr;
                std::this_thread::sleep_for(std::chrono::seconds(3));
                continue;
            }
            ::freeaddrinfo(res);
            res = nullptr;

            _aimonSocketFd = sock;
            std::string h = _runner.identity().handle;
            if (h.empty()) h = _runner.config().getString("robot_name", "stock");
            std::string connMsg = "[" + h + "] Connected to aimon gateway at " + host + ":" + std::to_string(port);
            _runner.appendStdoutLine(connMsg, true);

            // Send registration with web_port and priority
            nlohmann::json reg;
            reg["jsonrpc"] = "2.0";
            reg["id"] = 1;
            reg["method"] = "gateway/register";
            reg["params"] = {
                {"subsystem", "octibot"},
                {"display_name", "OCTI Robot (" + h + ")"},
                {"short_name", h},
                {"version", "1.0.3"},
                {"web_port", _boundPort.load()},
                {"web_path", "/"},
                {"priority", 35}
            };
            if (_runner.config().getBool("expose_mcp_tools", true)) {
                reg["params"]["tools"] = buildToolsListJson();
            }
            std::string regStr = reg.dump() + "\n";
            ::send(sock, regStr.c_str(), regStr.size(), 0);

            // Read loop
            std::string buf;
            char readBuf[1024];
            while (_aimonRunning) {
                ssize_t n = ::recv(sock, readBuf, sizeof(readBuf) - 1, 0);
                if (n <= 0) break;
                readBuf[n] = '\0';
                buf.append(readBuf, n);

                size_t nl;
                while ((nl = buf.find('\n')) != std::string::npos) {
                    std::string line = buf.substr(0, nl);
                    buf.erase(0, nl + 1);
                    if (line.empty()) continue;

                    try {
                        auto j = nlohmann::json::parse(line, nullptr, false);
                        if (j.is_object() && !j.contains("method") && (j.contains("result") || j.contains("error"))) {
                            if (j.contains("result") && j["result"].is_object()) {
                                int registeredTools = j["result"].value("registered_tools", 0);
                                if (_runner.verbosity() >= 2) {
                                    std::string regToolsMsg = "[" + h + "] Registered " + std::to_string(registeredTools)
                                        + " MCP tools with aimon gateway (" + host + ":" + std::to_string(port) + ")";
                                    _runner.appendStdoutLine(regToolsMsg, true);
                                }
                            }
                            continue;
                        }
                    } catch (...) {}

                    std::string resp = handleJsonRpc(line);
                    if (!resp.empty()) {
                        resp += "\n";
                        ::send(sock, resp.c_str(), resp.size(), 0);
                    }
                }
            }

            ::close(sock);
            _aimonSocketFd = -1;
            std::this_thread::sleep_for(std::chrono::seconds(2));
#else
            break;
#endif
        }
    });
}

void OctiBotMcpServer::stopAimonBridge() {
    _aimonRunning = false;
#if !defined(_WIN32)
    if (_aimonSocketFd >= 0) {
        ::shutdown(_aimonSocketFd, SHUT_RDWR);
        ::close(_aimonSocketFd);
        _aimonSocketFd = -1;
    }
#endif
    if (_aimonThread && _aimonThread->joinable()) {
        _aimonThread->join();
        _aimonThread.reset();
    }
}

// --- OctiBotAgentCliLauncher Implementation ---

OctiBotAgentCliLauncher::OctiBotAgentCliLauncher(OctiBotRunner &runner,
                                                 OctiBotMcpServer &mcpServer)
    : _runner(runner), _mcpServer(mcpServer) {
}

OctiBotAgentCliLauncher::~OctiBotAgentCliLauncher() {
    stop();
}

bool OctiBotAgentCliLauncher::materializeSandboxWorkspace(const std::string &sandboxDir, int mcpPort) {
#if !defined(_WIN32)
    std::string sdir = sandboxDir.empty() ? "/tmp/octibot-ai-sandbox" : sandboxDir;

    auto mkdir700 = [](const std::string &p) {
        struct stat st;
        if (::stat(p.c_str(), &st) != 0) {
            ::mkdir(p.c_str(), 0700);
        }
        ::chmod(p.c_str(), 0700);
    };

    mkdir700(sdir);
    mkdir700(sdir + "/.agents");
    mkdir700(sdir + "/.agents/plugins");
    mkdir700(sdir + "/.agents/plugins/octibot");
    mkdir700(sdir + "/.agents/plugins/octibot/skills");
    mkdir700(sdir + "/.agents/plugins/octibot/skills/octibot");
    mkdir700(sdir + "/.cursor");
    mkdir700(sdir + "/.cursor/skills");
    mkdir700(sdir + "/.cursor/skills/octibot");

    auto writeFile600 = [](const std::string &path, const std::string &content) {
        std::ofstream f(path, std::ios::out | std::ios::trunc);
        if (!f.is_open()) return false;
        f << content;
        f.flush();
        f.close();
        ::chmod(path.c_str(), 0600);
        return true;
    };

    std::string skillGuide = OctiBotMcpServer::getSkillGuideContent("all");
    std::string mcpPortStr = std::to_string(mcpPort);

    writeFile600(sdir + "/.agents/plugins/octibot/plugin.json", "{\"name\":\"octibot\"}\n");
    writeFile600(sdir + "/.agents/plugins/octibot/mcp_config.json",
                 "{\"mcpServers\":{\"octibot\":{\"url\":\"http://127.0.0.1:" + mcpPortStr + "/mcp\"}}}\n");
    writeFile600(sdir + "/.agents/plugins/octibot/skills/octibot/SKILL.md", skillGuide);

    writeFile600(sdir + "/.cursor/mcp.json",
                 "{\"mcpServers\":{\"octibot\":{\"url\":\"http://127.0.0.1:" + mcpPortStr + "/mcp\"}}}\n");
    writeFile600(sdir + "/.cursor/skills/octibot/SKILL.md", skillGuide);

    return true;
#else
    (void) sandboxDir;
    (void) mcpPort;
    return false;
#endif
}

std::vector<std::string> OctiBotAgentCliLauncher::buildChatCommandArgs(
    const std::string &backend,
    const std::string &executable,
    int mcpPort,
    const std::string &model,
    const std::string &prompt,
    const std::string &sandboxDir) {

    std::vector<std::string> args;
    std::string exe = executable.empty() ? backend : executable;
    args.push_back(exe);

    std::string sdir = sandboxDir.empty() ? "/tmp/octibot-ai-sandbox" : sandboxDir;

    if (backend == "agy") {
        if (!model.empty()) {
            args.push_back("--model");
            args.push_back(model);
        }
        args.push_back("--sandbox");
        args.push_back("--disable-slash-commands");
        args.push_back("--settings");
        args.push_back("{\"allowNonWorkspaceAccess\":false,\"permissions\":{\"allow\":[\"mcp(octibot:octibot_get_*)\",\"mcp(octibot:octibot_analyze_position)\"],\"deny\":[\"command(*)\",\"command(regex:[\\\\s\\\\S]*)\",\"read_file(*)\",\"write_file(*)\",\"read_url(*)\",\"mcp(aimon/*)\",\"mcp(aimon:*)\"]}}");
        args.push_back("-p");
        args.push_back(prompt);
    } else if (backend == "claude") {
        if (!model.empty()) {
            args.push_back("--model");
            args.push_back(model);
        }
        args.push_back("--restricted");
        args.push_back("--strict-mcp-config");
        args.push_back("--tools");
        args.push_back("");
        args.push_back("--disable-slash-commands");
        args.push_back("--no-session-persistence");
        args.push_back("--permission-prompts");
        args.push_back("none");
        args.push_back("--mcp-config");
        args.push_back("{\"mcpServers\":{\"octibot\":{\"type\":\"http\",\"url\":\"http://127.0.0.1:" + std::to_string(mcpPort) + "/mcp\"}}}");
        args.push_back("--allowedTools");
        args.push_back("mcp__octibot__octibot_get_skill_guide,mcp__octibot__octibot_get_status,mcp__octibot__octibot_get_game_state,mcp__octibot__octibot_analyze_position");
        args.push_back("-p");
        args.push_back(prompt);
    } else if (backend == "cursor-agent") {
        if (!model.empty()) {
            args.push_back("--model");
            args.push_back(model);
        }
        args.push_back("--mode");
        args.push_back("ask");
        args.push_back("--sandbox");
        args.push_back("enabled");
        args.push_back("--workspace");
        args.push_back(sdir);
        args.push_back("-p");
        args.push_back(prompt);
    } else {
        if (!model.empty()) {
            args.push_back("--model");
            args.push_back(model);
        }
        args.push_back("-p");
        args.push_back(prompt);
    }

    return args;
}

std::vector<std::string> OctiBotAgentCliLauncher::buildCommandArgs(
    const std::string &backend,
    const std::string &executable,
    int mcpPort,
    const std::string &model,
    const std::string &prompt,
    bool interactive,
    const std::string &workspaceDir) {

    std::vector<std::string> args;
    std::string exe = executable.empty() ? backend : executable;
    args.push_back(exe);

    std::string sdir = workspaceDir.empty() ? "/tmp/octibot-ai-sandbox" : workspaceDir;

    if (backend == "agy") {
        if (!model.empty()) {
            args.push_back("--model");
            args.push_back(model);
        }
        args.push_back("--sandbox");
        args.push_back("--disable-slash-commands");
        args.push_back("--settings");
        args.push_back("{\"allowNonWorkspaceAccess\":false,\"permissions\":{\"allow\":[\"mcp(octibot/*)\",\"mcp(octibot:*)\"],\"deny\":[\"command(*)\",\"command(regex:[\\\\s\\\\S]*)\",\"read_file(*)\",\"write_file(*)\",\"read_url(*)\",\"mcp(aimon/*)\",\"mcp(aimon:*)\"]}}");
        if (interactive) {
            args.push_back("-i");
            args.push_back(prompt);
        } else {
            args.push_back("-p");
            args.push_back(prompt);
        }
    } else if (backend == "claude") {
        if (!model.empty()) {
            args.push_back("--model");
            args.push_back(model);
        }
        args.push_back("--restricted");
        args.push_back("--strict-mcp-config");
        args.push_back("--tools");
        args.push_back("");
        args.push_back("--disable-slash-commands");
        args.push_back("--no-session-persistence");
        args.push_back("--permission-prompts");
        args.push_back("none");
        args.push_back("--mcp-config");
        args.push_back("{\"mcpServers\":{\"octibot\":{\"type\":\"http\",\"url\":\"http://127.0.0.1:" + std::to_string(mcpPort) + "/mcp\"}}}");
        args.push_back("--allowedTools");
        args.push_back("mcp__octibot__*");
        if (interactive) {
            args.push_back(prompt);
        } else {
            args.push_back("-p");
            args.push_back(prompt);
        }
    } else if (backend == "cursor-agent") {
        if (!model.empty()) {
            args.push_back("--model");
            args.push_back(model);
        }
        args.push_back("--mode");
        args.push_back("ask");
        args.push_back("--sandbox");
        args.push_back("enabled");
        args.push_back("--workspace");
        args.push_back(sdir);
        if (interactive) {
            args.push_back(prompt);
        } else {
            args.push_back("-p");
            args.push_back(prompt);
        }
    } else {
        if (!model.empty()) {
            args.push_back("--model");
            args.push_back(model);
        }
        if (!prompt.empty()) {
            args.push_back(prompt);
        }
    }

    return args;
}

bool OctiBotAgentCliLauncher::start() {
    std::string backend = _runner.config().getString("ai_cli_backend", "none");
    if (backend == "none" || backend.empty()) {
        return false;
    }

    if (!_mcpServer.boundHttpPort()) {
        if (!_mcpServer.startHttpServer(0)) {
            return false;
        }
    }

    std::string exe = _runner.config().getString("ai_cli_executable");
    std::string model = _runner.config().getString("ai_cli_model");
    std::string prompt = _runner.config().getString("ai_cli_prompt",
        "Inspect OCTI skill guide with octibot_get_skill_guide and assist with move analysis.");
    bool interactive = _runner.config().getBool("ai_cli_interactive", false);

    std::string sdir = _runner.config().getString("ai_cli_sandbox_dir", "/tmp/octibot-ai-sandbox");
    materializeSandboxWorkspace(sdir, _mcpServer.boundHttpPort());

    std::vector<std::string> args = buildCommandArgs(
        backend, exe, _mcpServer.boundHttpPort(), model, prompt, interactive, sdir);

#if !defined(_WIN32)
    int pipefd[2];
    if (!interactive) {
        if (::pipe(pipefd) < 0) {
            return false;
        }
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        if (!interactive) {
            ::close(pipefd[0]);
            ::close(pipefd[1]);
        }
        return false;
    }

    if (pid == 0) {
        ::setpgid(0, 0);
        ::prctl(PR_SET_PDEATHSIG, SIGTERM);

        if (!interactive) {
            ::close(pipefd[0]);
            ::dup2(pipefd[1], STDOUT_FILENO);
            ::dup2(pipefd[1], STDERR_FILENO);
            ::close(pipefd[1]);
        }

        if (::chdir(sdir.c_str()) != 0) {
            if (::chdir("/tmp") != 0) {}
        }

        std::vector<char *> cArgs;
        for (const auto &a : args) {
            cArgs.push_back(const_cast<char *>(a.c_str()));
        }
        cArgs.push_back(nullptr);

        ::execvp(cArgs[0], cArgs.data());
        ::_exit(127);
    }

    _childPid = pid;
    _childPgid = pid;
    _running = true;

    if (!interactive) {
        ::close(pipefd[1]);
        _stdoutPipeFd = pipefd[0];

        _monitorThread = std::make_unique<std::thread>([this]() {
            char buffer[512];
            std::string line;
            std::string prefix = "[" + _runner.identity().handle + ":ai] ";

            while (_running && _stdoutPipeFd >= 0) {
                ssize_t n = ::read(_stdoutPipeFd, buffer, sizeof(buffer) - 1);
                if (n <= 0) break;
                buffer[n] = '\0';
                line.append(buffer, n);

                size_t nl;
                while ((nl = line.find('\n')) != std::string::npos) {
                    _runner.appendStdoutLine(prefix + line.substr(0, nl), true);
                    line.erase(0, nl + 1);
                }
            }
        });
    }

    return true;
#else
    return false;
#endif
}

void OctiBotAgentCliLauncher::stop() {
    _running = false;
#if !defined(_WIN32)
    if (_childPgid > 0) {
        ::kill(-_childPgid, SIGTERM);
        int status = 0;
        for (int i = 0; i < 20; ++i) {
            pid_t r = ::waitpid(_childPid, &status, WNOHANG);
            if (r == _childPid) {
                _childPid = -1;
                _childPgid = -1;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (_childPid > 0) {
            ::kill(-_childPgid, SIGKILL);
            ::waitpid(_childPid, &status, 0);
            _childPid = -1;
            _childPgid = -1;
        }
    }
    if (_stdoutPipeFd >= 0) {
        ::close(_stdoutPipeFd);
        _stdoutPipeFd = -1;
    }
#endif
    if (_monitorThread && _monitorThread->joinable()) {
        _monitorThread->join();
        _monitorThread.reset();
    }
}

bool OctiBotAgentCliLauncher::isRunning() const {
#if !defined(_WIN32)
    if (_childPid > 0) {
        return ::kill(_childPid, 0) == 0;
    }
#endif
    return false;
}

} // namespace octi

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
