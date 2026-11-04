/*
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiBotRunner.hxx"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <nlohmann/json.hpp>
#include "OctiDefs.hxx"

namespace octi {

// --- StockAlphaBetaAgent Implementation ---

StockAlphaBetaAgent::StockAlphaBetaAgent(const OctiBotConfigStore &config) {
    onConfigure(config);
}

void StockAlphaBetaAgent::onConfigure(const OctiBotConfigStore &config) {
    applyConfigToTuningProfile(config, _profile);
    _engine.setTuningProfile(_profile);
}

std::string StockAlphaBetaAgent::computeMove(const OctiBoard &board,
                                            const GameViewData &game,
                                            int mySeat,
                                            std::int64_t remainingClockMs,
                                            BotMoveTelemetry *outTelemetry) {
    (void) mySeat;
    (void) remainingClockMs;

    BotTuningProfile searchProfile = _profile;
    if (game.timeControlEnabled && remainingClockMs > 0) {
        int dynamicBudget = static_cast<int>(remainingClockMs / 25) +
                            static_cast<int>(game.incrementMs);
        if (dynamicBudget < 200) {
            dynamicBudget = 200;
        }
        searchProfile.timeBudgetMs = std::min(_profile.timeBudgetMs, dynamicBudget);
    }
    _engine.setTuningProfile(searchProfile);

    BotSearchStats stats;
    std::string moveNotation = _engine.chooseMove(board, &stats);
    if (outTelemetry) {
        outTelemetry->depthReached = stats.depthReached;
        outTelemetry->nodesVisited = stats.nodesVisited;
        outTelemetry->evalScore = stats.bestScore;
        outTelemetry->elapsedMs = stats.elapsedMs;
        outTelemetry->present = true;
    }

    if (moveNotation.empty() && board.getGame()) {
        auto legalMoves = board.getGame()->generateLegalMoves();
        if (!legalMoves.empty()) {
            return legalMoves.front().asString();
        }
    }
    return moveNotation;
}

// --- OctiBotAgentRegistry Implementation ---

OctiBotAgentRegistry &OctiBotAgentRegistry::instance() {
    static OctiBotAgentRegistry s_instance;
    return s_instance;
}

OctiBotAgentRegistry::OctiBotAgentRegistry() {
    registerAgent("alpha_beta", [](const OctiBotConfigStore &config) {
        return std::make_unique<StockAlphaBetaAgent>(config);
    });
    registerAgent("stock", [](const OctiBotConfigStore &config) {
        return std::make_unique<StockAlphaBetaAgent>(config);
    });
    registerAgent("default", [](const OctiBotConfigStore &config) {
        return std::make_unique<StockAlphaBetaAgent>(config);
    });
    registerAgent("centaur_mcp", [](const OctiBotConfigStore &config) {
        return std::make_unique<CentaurMcpAgent>(config);
    });
}

void OctiBotAgentRegistry::registerAgent(const std::string &algorithmName,
                                        AgentFactory factory) {
    std::lock_guard<std::mutex> lock(_mutex);
    _factories[algorithmName] = std::move(factory);
}

bool OctiBotAgentRegistry::hasAgent(const std::string &algorithmName) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _factories.find(algorithmName) != _factories.end();
}

std::unique_ptr<OctiBotAgent> OctiBotAgentRegistry::createAgent(
    const std::string &algorithmName,
    const OctiBotConfigStore &config) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _factories.find(algorithmName);
    if (it != _factories.end()) {
        return it->second(config);
    }
    // Fallback to stock alpha_beta if unknown
    auto stockIt = _factories.find("alpha_beta");
    if (stockIt != _factories.end()) {
        return stockIt->second(config);
    }
    return nullptr;
}

std::vector<std::string> OctiBotAgentRegistry::registeredAlgorithms() const {
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<std::string> res;
    for (const auto &kv : _factories) {
        res.push_back(kv.first);
    }
    return res;
}

// --- OctiBotRunner Implementation ---

OctiBotRunner::OctiBotRunner(std::unique_ptr<OctiBotAgent> agent,
                             const OctiBotConfigStore &config,
                             const std::string &customEndpoint)
    : _agent(std::move(agent)),
      _config(config),
      _policy(config),
      _conversation(config) {

    if (!customEndpoint.empty()) {
        std::string ep = customEndpoint;
        if (ep.rfind("http://", 0) == 0) ep = ep.substr(7);
        if (ep.rfind("https://", 0) == 0) ep = ep.substr(8);
        size_t colon = ep.find(':');
        if (colon != std::string::npos) {
            std::string h = ep.substr(0, colon);
            int p = std::atoi(ep.substr(colon + 1).c_str());
            _client.setEndpoint(h, p);
        } else {
            _client.setEndpoint(ep, 80);
        }
    } else {
        std::string host = _config.getString("server_host", "127.0.0.1");
        int port = _config.getInt("server_port", 8080);
        _client.setEndpoint(host, port);
    }

    _moveControl = _config.getString("move_control", "engine");
    _publishMoveTelemetry = _config.getBool("publish_move_telemetry", false);
    _agentMoveTimeoutMs = _config.getInt("agent_move_timeout_ms", 15000);

    if (_agent) {
        _agent->onConfigure(_config);
    }
}

OctiBotRunner::~OctiBotRunner() {
    stop();
}

bool OctiBotRunner::connectAndAuthenticate(std::string &errorOut) {
    std::string tokenToUse;
    std::string directToken = _config.getString("bot_token");
    std::string robotName = _config.getString("robot_name");

    if (!directToken.empty() && directToken != "auto" &&
        directToken.rfind("octi_bot_", 0) == 0) {
        tokenToUse = directToken;
    } else if (!robotName.empty()) {
        std::string poolToken = _config.getString("bot_token_" + robotName);
        if (!poolToken.empty() && poolToken.rfind("octi_bot_", 0) == 0) {
            tokenToUse = poolToken;
        }
    }

    // Direct token connect
    if (!tokenToUse.empty()) {
        _client.setBotToken(tokenToUse);
        auto res = _client.connectRobot(false);
        if (!res.ok) {
            errorOut = res.errorMessage.empty() ? "Failed to connect robot" : res.errorMessage;
            return false;
        }

        _identity.robotUuid = res.data.robotUuid;
        _identity.handle = res.data.handle;
        _identity.description = res.data.description;
        _identity.ownerUuid = res.data.ownerUuid;
        _identity.ownerHandle = res.data.ownerHandle;
        _identity.activeRoomId = res.data.activeRoomId;
        _identity.gamesPlayed = res.data.gamesPlayed;
        _identity.wins = res.data.wins;
        _identity.losses = res.data.losses;
        _identity.draws = res.data.draws;

        if (!_identity.activeRoomId.empty()) {
            std::lock_guard<std::mutex> lock(_stateMutex);
            _currentRoomId = _identity.activeRoomId;
        }

        if (_agent) {
            _agent->onConnected(_identity);
        }
        return true;
    }

    // Auto pool connect: iterate token pool
    auto pool = _config.tokenPool();
    if (pool.empty()) {
        errorOut = "No robot token specified and token pool is empty";
        return false;
    }

    for (const auto &item : pool) {
        _client.setBotToken(item.second);
        auto res = _client.connectRobot(true); // if_unused = true
        if (res.ok) {
            _identity.robotUuid = res.data.robotUuid;
            _identity.handle = res.data.handle;
            _identity.description = res.data.description;
            _identity.ownerUuid = res.data.ownerUuid;
            _identity.ownerHandle = res.data.ownerHandle;
            _identity.activeRoomId = res.data.activeRoomId;
            _identity.gamesPlayed = res.data.gamesPlayed;
            _identity.wins = res.data.wins;
            _identity.losses = res.data.losses;
            _identity.draws = res.data.draws;

            if (!_identity.activeRoomId.empty()) {
                std::lock_guard<std::mutex> lock(_stateMutex);
                _currentRoomId = _identity.activeRoomId;
            }

            if (_agent) {
                _agent->onConnected(_identity);
            }
            return true;
        }
        if (res.httpStatus == 409 && res.errorCode == "ROBOT_TOKEN_IN_USE") {
            continue; // Try next token
        }
    }

    errorOut = "Failed to claim an unused robot token from pool";
    return false;
}

void OctiBotRunner::start() {
    if (_running) {
        return;
    }
    _running = true;
    _workerThread = std::make_unique<std::thread>(&OctiBotRunner::run, this);
}

void OctiBotRunner::run() {
    _running = true;

    // SSE setup
    std::string initialRoom;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        initialRoom = _currentRoomId;
    }

    if (!initialRoom.empty()) {
        _client.startGameEventStream(initialRoom, 0,
            [this](const SseEvent &ev) {
                handleGameSseEvent(ev.event, ev.dataJson);
            });
    } else {
        _client.startLobbyEventStream(0,
            [this](const SseEvent &ev) {
                handleLobbySseEvent(ev.event, ev.dataJson);
            });
    }

    while (_running && !_superseded) {
        loopStep();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (_client.isAuthenticated() && !_superseded) {
        _client.disconnectRobot();
    }
    _running = false;
}

void OctiBotRunner::stop() {
    _running = false;
    notifyEvent("STOP");
    _client.stopEventStreams();

    if (_workerThread && _workerThread->joinable()) {
        _workerThread->join();
        _workerThread.reset();
    }
}

bool OctiBotRunner::waitForEvent(int timeoutMs, std::string &outReason) {
    std::unique_lock<std::mutex> lock(_eventMutex);
    if (!_hasPendingEvent) {
        if (timeoutMs > 0) {
            _eventCv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                              [this] { return _hasPendingEvent || !_running; });
        } else {
            _eventCv.wait(lock, [this] { return _hasPendingEvent || !_running; });
        }
    }
    if (_hasPendingEvent) {
        outReason = _lastEventReason;
        _hasPendingEvent = false;
        return true;
    }
    return false;
}

void OctiBotRunner::notifyEvent(const std::string &reason) {
    {
        std::lock_guard<std::mutex> lock(_eventMutex);
        _hasPendingEvent = true;
        _lastEventReason = reason;
    }
    _eventCv.notify_all();
}

std::string OctiBotRunner::currentRoomId() const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _currentRoomId;
}

std::string OctiBotRunner::currentPhase() const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _currentPhase;
}

int OctiBotRunner::currentSeat() const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _currentSeat;
}

bool OctiBotRunner::submitAgentMove(const std::string &moveNotation, std::string &errorOut) {
    std::lock_guard<std::mutex> lock(_stateMutex);
    if (_currentRoomId.empty() || _currentPhase != "IN_PROGRESS") {
        errorOut = "No game currently in progress";
        return false;
    }
    _pendingAgentMove = moveNotation;
    _hasAgentMove = true;
    notifyEvent("AGENT_MOVE_READY");
    return true;
}

bool OctiBotRunner::executeGameAction(const std::string &action, const std::string &arg, std::string &errorOut) {
    std::string roomId = currentRoomId();
    if (roomId.empty()) {
        errorOut = "Not in a game room";
        return false;
    }

    if (action == "resign") {
        auto res = _client.resignGame(roomId);
        return res.ok;
    }

    std::string actualAction = action;
    if (action == "offer_draw" || action == "resume" || action == "claim_forfeit") {
        actualAction = action;
    } else if (action == "accept_proposal") {
        actualAction = "accept_" + arg;
    } else if (action == "decline_proposal") {
        actualAction = "decline_" + arg;
    } else if (action == "request_undo") {
        actualAction = "request_undo";
    } else if (action == "request_pause") {
        actualAction = "request_pause";
    } else if (action == "request_rematch") {
        actualAction = "request_rematch";
    }

    auto res = _client.submitGameAction(roomId, actualAction);
    if (!res.ok) {
        errorOut = res.errorMessage;
    }
    return res.ok;
}

void OctiBotRunner::loopStep() {
    if (_superseded) {
        return;
    }

    std::string roomId;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        roomId = _currentRoomId;
    }

    if (roomId.empty()) {
        // Lobby state & autonomous matchmaking
        std::string behavior = _policy.tableBehavior();
        if (behavior == "idle") {
            return;
        }

        if (behavior == "auto" || behavior == "join_only") {
            auto tablesRes = _client.listTables(false);
            if (tablesRes.ok) {
                auto ranked = _policy.rankCompatibleTables(tablesRes.data,
                                                          _identity.ownerHandle,
                                                          _identity.handle);
                if (!ranked.empty()) {
                    auto joinRes = _client.joinTable(ranked.front().roomId);
                    if (joinRes.ok) {
                        std::lock_guard<std::mutex> lock(_stateMutex);
                        _currentRoomId = joinRes.data.roomId;
                        _currentSeat = joinRes.data.seat;
                        _client.stopEventStreams();
                        _client.startGameEventStream(_currentRoomId, 0,
                            [this](const SseEvent &ev) {
                                handleGameSseEvent(ev.event, ev.dataJson);
                            });
                        notifyEvent("ROOM_JOINED");
                        return;
                    }
                }
            }
        }

        if (behavior == "auto" || behavior == "host_only") {
            auto hostReq = _policy.buildHostTableRequest();
            auto createRes = _client.createTable(hostReq);
            if (createRes.ok) {
                std::lock_guard<std::mutex> lock(_stateMutex);
                _currentRoomId = createRes.data.roomId;
                _currentSeat = createRes.data.seat;
                _client.stopEventStreams();
                _client.startGameEventStream(_currentRoomId, 0,
                    [this](const SseEvent &ev) {
                        handleGameSseEvent(ev.event, ev.dataJson);
                    });
                notifyEvent("ROOM_HOSTED");
                return;
            }
        }

        if (behavior == "matchmaking") {
            auto mmReq = _policy.buildMatchmakingRequest();
            _client.enqueueMatchmaking(mmReq);
            auto statusRes = _client.fetchMatchmakingStatus();
            if (statusRes.ok && !statusRes.data.matchedRoomId.empty()) {
                std::lock_guard<std::mutex> lock(_stateMutex);
                _currentRoomId = statusRes.data.matchedRoomId;
                _client.stopEventStreams();
                _client.startGameEventStream(_currentRoomId, 0,
                    [this](const SseEvent &ev) {
                        handleGameSseEvent(ev.event, ev.dataJson);
                    });
                notifyEvent("MATCHMAKING_MATCHED");
                return;
            }
        }
    } else {
        // Room active
        auto gvRes = _client.fetchGameView(roomId);
        if (!gvRes.ok) {
            if (gvRes.httpStatus == 404 || gvRes.httpStatus == 410) {
                std::lock_guard<std::mutex> lock(_stateMutex);
                _currentRoomId = "";
                _currentPhase = "IDLE";
                _client.stopEventStreams();
                _client.startLobbyEventStream(0,
                    [this](const SseEvent &ev) {
                        handleLobbySseEvent(ev.event, ev.dataJson);
                    });
                notifyEvent("ROOM_LEFT");
                return;
            }
            if (gvRes.httpStatus == 409 && gvRes.errorCode == "ROBOT_TOKEN_SUPERSEDED") {
                _superseded = true;
                _running = false;
                notifyEvent("SUPERSEDED");
                return;
            }
            return;
        }

        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            _activeGameView = gvRes.data;
            _currentPhase = gvRes.data.phase;

            for (const auto &seat : gvRes.data.seats) {
                if (seat.handle == _identity.handle ||
                    (!_identity.robotUuid.empty() && seat.robotUuid == _identity.robotUuid)) {
                    _currentSeat = seat.seat;
                    break;
                }
            }
        }

        if (_currentPhase == "WAITING_FOR_PLAYERS") {
            // Check kick policies if host
            if (_activeGameView.hostUuid == _identity.robotUuid ||
                _activeGameView.hostUuid == _identity.ownerUuid) {
                for (const auto &seat : _activeGameView.seats) {
                    if (seat.seat != _currentSeat && !seat.handle.empty()) {
                        TableSeatInfo tsi;
                        tsi.seat = seat.seat;
                        tsi.handle = seat.handle;
                        tsi.isRobot = seat.isRobot;
                        if (_policy.shouldKickSeatedPlayer(tsi, _identity.ownerHandle)) {
                            _client.kickSeat(roomId, seat.seat);
                        }
                    }
                }
            }
        } else if (_currentPhase == "IN_PROGRESS") {
            if (_activeGameView.turnSeat == _currentSeat) {
                playEngineTurn();
            }
        } else if (_currentPhase == "GAME_OVER") {
            notifyEvent("GAME_OVER");
            if (_agent) {
                _agent->onGameOver(_activeGameView, _currentSeat);
            }
            // Transition back to lobby after short delay
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            _client.leaveTable(roomId);
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                _currentRoomId = "";
                _currentPhase = "IDLE";
            }
            _client.stopEventStreams();
            _client.startLobbyEventStream(0,
                [this](const SseEvent &ev) {
                    handleLobbySseEvent(ev.event, ev.dataJson);
                });
            notifyEvent("ROOM_LEFT");
        }
    }
}

void OctiBotRunner::playEngineTurn() {
    std::string roomId;
    int mySeat = 0;
    std::string fen;
    std::int64_t clockMs = 0;
    std::string mode;
    int bases = 3;
    bool superProngs = false;
    bool edgeless = false;

    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        roomId = _currentRoomId;
        mySeat = _currentSeat;
        fen = _activeGameView.fen;
        mode = _activeGameView.mode;
        bases = _activeGameView.bases;
        superProngs = _activeGameView.superProngs;
        edgeless = _activeGameView.edgeless;

        for (const auto &seat : _activeGameView.seats) {
            if (seat.seat == mySeat) {
                clockMs = seat.clockMs;
                break;
            }
        }

        OctiGameMode m = (mode == "4P" ? GAME_MODE_4 : (mode == "OFK" ? GAME_MODE_OFK : GAME_MODE_2));
        OctiGameBases b = (bases == 1 ? GAME_BASES_1 : GAME_BASES_3);
        if (!_activeBoard) {
            _activeBoard = std::make_unique<OctiBoard>(m, superProngs, edgeless, b);
        }
        if (!fen.empty()) {
            _activeBoard->fromPositionString(fen);
        }
    }

    if (_policy.minMoveThinkMs() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(_policy.minMoveThinkMs()));
    }

    std::string chosenMove;
    if (_moveControl == "agent") {
        notifyEvent("BOT_TURN");

        // Wait for external agent move up to timeout
        auto startWait = std::chrono::steady_clock::now();
        while (_running && !_superseded) {
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                if (_hasAgentMove) {
                    chosenMove = _pendingAgentMove;
                    _hasAgentMove = false;
                    _pendingAgentMove.clear();
                    break;
                }
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startWait).count();
            if (elapsed >= _agentMoveTimeoutMs) {
                break; // Fallback to engine
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    // Fallback to engine search if no agent move was submitted
    if (chosenMove.empty() && _agent) {
        chosenMove = _agent->computeMove(*_activeBoard, _activeGameView, mySeat, clockMs, &_lastTelemetry);
    }

    if (!chosenMove.empty()) {
        if (_publishMoveTelemetry && _lastTelemetry.present) {
            MoveTelemetryData telemetry;
            telemetry.present = true;
            telemetry.depth = _lastTelemetry.depthReached;
            telemetry.nodes = _lastTelemetry.nodesVisited;
            telemetry.score = _lastTelemetry.evalScore;
            telemetry.elapsedMs = _lastTelemetry.elapsedMs;
            _client.submitMove(roomId, chosenMove, telemetry);
        } else {
            _client.submitMove(roomId, chosenMove);
        }
        notifyEvent("MOVE_SUBMITTED");
    }
}

void OctiBotRunner::handleLobbySseEvent(const std::string &eventType, const std::string &data) {
    if (eventType == "LOBBY_CHAT") {
        try {
            auto j = nlohmann::json::parse(data);
            LobbyChatEntry msg;
            msg.seq = j.value("seq", 0LL);
            msg.handle = j.value("handle", "");
            msg.playerUuid = j.value("player_uuid", "");
            msg.text = j.value("text", "");
            msg.isRobot = j.value("is_robot", false);
            msg.createdAtMs = j.value("created_at_ms", 0LL);

            std::string lastTeleStr;
            if (_lastTelemetry.present) {
                lastTeleStr = "depth=" + std::to_string(_lastTelemetry.depthReached) +
                    " nodes=" + std::to_string(_lastTelemetry.nodesVisited) +
                    " score=" + std::to_string(_lastTelemetry.evalScore) +
                    " time=" + std::to_string(_lastTelemetry.elapsedMs) + "ms";
            }

            auto cmdRes = _conversation.processLobbyMessage(
                msg, _identity.handle, _identity.ownerHandle,
                _policy, _config, "", "IDLE",
                _identity.wins, _identity.losses, _identity.draws,
                _identity.description,
                _config.getBool("allow_public_eval_chat", true),
                lastTeleStr);

            if (cmdRes.action == ChatCommandActionType::REPLY_ONLY && !cmdRes.replyText.empty()) {
                _client.postLobbyChat(cmdRes.replyText);
            } else if (cmdRes.action == ChatCommandActionType::HOST_TABLE) {
                _policy.setTableBehavior("host_only");
            } else if (cmdRes.action == ChatCommandActionType::JOIN_ROOM) {
                _client.joinTable(cmdRes.targetRoomId);
            }

            if (_agent) {
                std::string reply;
                _agent->onLobbyChat(msg, reply);
                if (!reply.empty()) {
                    _client.postLobbyChat(reply);
                }
            }
            notifyEvent("CHAT_MENTION");
        } catch (...) {}
    }
}

void OctiBotRunner::handleGameSseEvent(const std::string &eventType, const std::string &data) {
    if (eventType == "BOARD_CHAT") {
        try {
            auto j = nlohmann::json::parse(data);
            std::string senderHandle = j.value("handle", "");
            std::string senderUuid = j.value("player_uuid", "");
            std::string text = j.value("text", "");
            bool isRobot = j.value("is_robot", false);

            std::string lastTeleStr;
            if (_lastTelemetry.present) {
                lastTeleStr = "depth=" + std::to_string(_lastTelemetry.depthReached) +
                    " nodes=" + std::to_string(_lastTelemetry.nodesVisited) +
                    " score=" + std::to_string(_lastTelemetry.evalScore) +
                    " time=" + std::to_string(_lastTelemetry.elapsedMs) + "ms";
            }

            auto cmdRes = _conversation.processGameMessage(
                senderHandle, senderUuid, isRobot, text,
                _identity.handle, _identity.ownerHandle,
                _policy, _config, _currentRoomId, _currentPhase,
                _identity.wins, _identity.losses, _identity.draws,
                _identity.description,
                _config.getBool("allow_public_eval_chat", true),
                lastTeleStr);

            if (cmdRes.action == ChatCommandActionType::REPLY_ONLY && !cmdRes.replyText.empty()) {
                _client.postGameChat(_currentRoomId, "ALL", cmdRes.replyText);
            }

            if (_agent) {
                std::string reply;
                _agent->onGameChat(senderHandle, text, reply);
                if (!reply.empty()) {
                    _client.postGameChat(_currentRoomId, "ALL", reply);
                }
            }
            notifyEvent("CHAT_MENTION");
        } catch (...) {}
    } else if (eventType == "GAME_PROPOSAL") {
        try {
            auto j = nlohmann::json::parse(data);
            GameProposalInfo proposal;
            proposal.type = j.value("type", "");
            proposal.proposerUuid = j.value("proposer_uuid", "");

            bool accept = _policy.shouldAcceptProposal(proposal, proposal.proposerUuid,
                                                       _lastTelemetry.evalScore,
                                                       _consecutiveRematches);
            if (accept) {
                if (proposal.type == "DRAW_OFFER") {
                    _client.submitGameAction(_currentRoomId, "accept_draw");
                } else if (proposal.type == "UNDO_REQUEST") {
                    _client.submitGameAction(_currentRoomId, "accept_undo");
                } else if (proposal.type == "PAUSE_REQUEST") {
                    _client.submitGameAction(_currentRoomId, "accept_pause");
                } else if (proposal.type == "REMATCH_REQUEST") {
                    _client.submitGameAction(_currentRoomId, "accept_rematch");
                    _consecutiveRematches++;
                }
            } else {
                if (proposal.type == "DRAW_OFFER") {
                    _client.submitGameAction(_currentRoomId, "decline_draw");
                } else if (proposal.type == "UNDO_REQUEST") {
                    _client.submitGameAction(_currentRoomId, "decline_undo");
                } else if (proposal.type == "PAUSE_REQUEST") {
                    _client.submitGameAction(_currentRoomId, "decline_pause");
                } else if (proposal.type == "REMATCH_REQUEST") {
                    _client.submitGameAction(_currentRoomId, "decline_rematch");
                }
            }
            notifyEvent("PROPOSAL");
        } catch (...) {}
    } else if (eventType == "TURN") {
        notifyEvent("TURN");
    }
}

// --- OctiBotFleetRunner Implementation ---

OctiBotFleetRunner::~OctiBotFleetRunner() {
    stop();
}

bool OctiBotFleetRunner::startProfiles(const OctiBotConfigStore &config,
                                      const std::vector<std::string> &profiles,
                                      const std::string &customEndpoint) {
    for (const auto &pName : profiles) {
        OctiBotConfigStore profConfig = config.resolveProfile(pName);
        std::string algo = profConfig.getString("algorithm", "alpha_beta");
        auto agent = OctiBotAgentRegistry::instance().createAgent(algo, profConfig);
        auto runner = std::make_unique<OctiBotRunner>(std::move(agent), profConfig, customEndpoint);

        std::string err;
        if (!runner->connectAndAuthenticate(err)) {
            std::cerr << "[" << pName << "] Failed to connect: " << err << "\n";
            continue;
        }

        runner->start();
        _runners.push_back(std::move(runner));
    }
    return !_runners.empty();
}

bool OctiBotFleetRunner::startAllProfiles(const OctiBotConfigStore &config,
                                         const std::string &customEndpoint) {
    auto names = config.profileNames();
    if (names.empty()) {
        names.push_back("stock");
    }
    return startProfiles(config, names, customEndpoint);
}

bool OctiBotFleetRunner::startClones(const OctiBotConfigStore &config,
                                    const std::string &baseProfile,
                                    int numInstances,
                                    const std::string &customEndpoint) {
    if (numInstances <= 0) {
        return false;
    }
    OctiBotConfigStore baseConfig = config.resolveProfile(baseProfile);
    std::string algo = baseConfig.getString("algorithm", "alpha_beta");

    for (int i = 0; i < numInstances; ++i) {
        OctiBotConfigStore instConfig = baseConfig;
        instConfig.set("bot_token", "auto"); // Claim from pool
        auto agent = OctiBotAgentRegistry::instance().createAgent(algo, instConfig);
        auto runner = std::make_unique<OctiBotRunner>(std::move(agent), instConfig, customEndpoint);

        std::string err;
        if (!runner->connectAndAuthenticate(err)) {
            std::cerr << "[" << baseProfile << "#" << (i + 1) << "] Connect failed: " << err << "\n";
            continue;
        }
        runner->start();
        _runners.push_back(std::move(runner));
    }
    return !_runners.empty();
}

void OctiBotFleetRunner::stop() {
    for (auto &r : _runners) {
        r->stop();
    }
    _runners.clear();
}

void OctiBotFleetRunner::waitForAll() {
    for (auto &r : _runners) {
        while (r->isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
}

int runFleetCli(int argc, char **argv) {
    std::string configPath = OctiBotConfigStore::defaultConfigFilePath();
    std::string tokensPath = OctiBotConfigStore::defaultTokensFilePath();
    std::string singleProfile;
    std::vector<std::string> multiProfiles;
    bool allProfiles = false;
    int instances = 1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--tokens-file" && i + 1 < argc) {
            tokensPath = argv[++i];
        } else if (arg == "--profile" && i + 1 < argc) {
            singleProfile = argv[++i];
        } else if (arg == "--profiles" && i + 1 < argc) {
            std::string plist = argv[++i];
            std::stringstream ss(plist);
            std::string p;
            while (std::getline(ss, p, ',')) {
                if (!p.empty()) multiProfiles.push_back(p);
            }
        } else if (arg == "--all-profiles") {
            allProfiles = true;
        } else if (arg == "--instances" && i + 1 < argc) {
            instances = std::atoi(argv[++i]);
        }
    }

    OctiBotConfigStore config;
    std::string err;
    if (std::ifstream(configPath).good()) {
        config.loadFile(configPath, err);
    }
    if (std::ifstream(tokensPath).good()) {
        config.loadTokensFile(tokensPath, err);
    }

    OctiBotFleetRunner fleet;
    if (instances > 1 && !singleProfile.empty()) {
        fleet.startClones(config, singleProfile, instances);
    } else if (allProfiles) {
        fleet.startAllProfiles(config);
    } else if (!multiProfiles.empty()) {
        fleet.startProfiles(config, multiProfiles);
    } else {
        std::string prof = singleProfile.empty() ? "stock" : singleProfile;
        fleet.startProfiles(config, {prof});
    }

    fleet.waitForAll();
    return 0;
}

} // namespace octi

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set ft=cpp et ts=4 sts=4 sw=4:
 */
