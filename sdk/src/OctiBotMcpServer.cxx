/*
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
            {"version", "1.0.2"}
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

bool OctiBotMcpServer::startHttpServer(int port, const std::string &bindAddr) {
    stopHttpServer();

    _httpServer = std::make_unique<httplib::Server>();

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
            if (h.empty()) h = "octibot";
            std::cout << "[" << h << "] Connected to aimon gateway at " << host << ":" << port << "\n";

            // Send registration
            nlohmann::json reg;
            reg["jsonrpc"] = "2.0";
            reg["id"] = 1;
            reg["method"] = "gateway/register";
            reg["params"] = {
                {"subsystem", "octibot"},
                {"display_name", "OCTI Robot (" + h + ")"},
                {"short_name", h},
                {"version", "1.0.2"}
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
                                    std::cout << "[" << h << "] Registered " << registeredTools
                                              << " MCP tools with aimon gateway (" << host << ":" << port << ")\n";
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

    if (backend == "agy") {
        std::string sseUrl = "http://127.0.0.1:" + std::to_string(mcpPort) + "/sse";
        std::string settings = "{\"mcpServers\":{\"octibot\":{\"url\":\"" + sseUrl +
            "\"}},\"permissions\":{\"allow\":[\"mcp(octibot/*)\",\"mcp(octibot:*)\"]}}";
        args.push_back("--settings");
        args.push_back(settings);
        if (!model.empty()) {
            args.push_back("--model");
            args.push_back(model);
        }
        if (interactive) {
            args.push_back("-i");
            args.push_back(prompt);
        } else {
            args.push_back("-p");
            args.push_back(prompt);
            args.push_back("--dangerously-skip-permissions");
        }
    } else if (backend == "claude") {
        std::string mcpUrl = "http://127.0.0.1:" + std::to_string(mcpPort) + "/mcp";
        std::string mcpCfg = "{\"mcpServers\":{\"octibot\":{\"type\":\"http\",\"url\":\"" + mcpUrl + "\"}}}";
        args.push_back("--mcp-config");
        args.push_back(mcpCfg);
        args.push_back("--allowedTools");
        args.push_back("mcp__octibot__*");
        if (!model.empty()) {
            args.push_back("--model");
            args.push_back(model);
        }
        if (interactive) {
            args.push_back(prompt);
        } else {
            args.push_back("-p");
            args.push_back(prompt);
        }
    } else if (backend == "cursor-agent") {
        if (!workspaceDir.empty()) {
            args.push_back("--workspace");
            args.push_back(workspaceDir);
        }
        args.push_back("--trust");
        args.push_back("--approve-mcps");
        if (interactive) {
            args.push_back(prompt);
        } else {
            args.push_back("-p");
            args.push_back(prompt);
        }
    } else {
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

    std::vector<std::string> args = buildCommandArgs(
        backend, exe, _mcpServer.boundHttpPort(), model, prompt, interactive, "");

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
                    std::cout << prefix << line.substr(0, nl) << std::endl;
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
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set ft=cpp et ts=4 sts=4 sw=4:
 */
