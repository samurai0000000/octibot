/*
 * OctiBotRunner.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiBotRunner.hxx"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <csignal>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <thread>
#include <nlohmann/json.hpp>
#include "OctiDefs.hxx"

namespace octi {

static std::mutex s_logMutex;
static std::atomic<bool> s_stopRequested{false};
static OctiBotRunner *s_primaryRunner = nullptr;
static std::mutex s_activeRunnersMutex;
static std::set<OctiBotRunner *> s_activeRunners;
class OctiBotFleetRunner;
static OctiBotFleetRunner *s_activeFleet = nullptr;

static void handleFleetSignal(int sig) {
    (void) sig;
    s_stopRequested.store(true);
    if (s_activeFleet) {
        // Stop will wake waitForAll
    }
}

static inline std::int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void OctiBotRunner::appendStdoutLine(const std::string &line, bool printToConsole) {
    if (printToConsole) {
        std::lock_guard<std::mutex> lock(s_logMutex);
        std::cout << line << std::endl;
    }
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(_logMutex);
    BotLogEntry entry;
    entry.seq = _nextLogSeq++;
    entry.timestampMs = ms;
    entry.line = line;
    _logBuffer.push_back(std::move(entry));
    while (_logBuffer.size() > kMaxLogLines) {
        _logBuffer.pop_front();
    }
}

std::vector<BotLogEntry> OctiBotRunner::getLogsAfter(std::uint64_t afterSeq,
                                                    std::uint64_t *outOldestSeq,
                                                    std::uint64_t *outLatestSeq) const {
    std::lock_guard<std::mutex> lock(_logMutex);
    if (_logBuffer.empty()) {
        if (outOldestSeq) *outOldestSeq = 0;
        if (outLatestSeq) *outLatestSeq = 0;
        return {};
    }
    if (outOldestSeq) *outOldestSeq = _logBuffer.front().seq;
    if (outLatestSeq) *outLatestSeq = _logBuffer.back().seq;

    std::vector<BotLogEntry> res;
    for (const auto &entry : _logBuffer) {
        if (entry.seq > afterSeq) {
            res.push_back(entry);
        }
    }
    return res;
}

void OctiBotRunner::logMsg(int minLevel, const std::string &msg) const {
    if (_verbosity < minLevel) {
        return;
    }
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &tt);
#else
    localtime_r(&tt, &tmBuf);
#endif
    char timeStr[32];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tmBuf);

    std::string tag = _identity.handle.empty() ? _config.getString("robot_name", "stock") : _identity.handle;
    std::string formatted = "[" + std::string(timeStr) + "] [" + tag + "] " + msg;
    const_cast<OctiBotRunner *>(this)->appendStdoutLine(formatted, true);
}

void OctiBotRunner::recordMoveHistory(int ply, const std::string &move, const BotMoveTelemetry &telem) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(_historyMutex);
    BotMoveHistoryEntry entry;
    entry.ply = ply;
    entry.move = move;
    entry.depthReached = telem.depthReached;
    entry.nodesVisited = telem.nodesVisited;
    entry.evalScore = telem.evalScore;
    entry.elapsedMs = telem.elapsedMs;
    entry.timestampMs = ms;

    _moveHistory.push_back(std::move(entry));
    while (_moveHistory.size() > kMaxMoveHistory) {
        _moveHistory.pop_front();
    }
}

std::vector<BotMoveHistoryEntry> OctiBotRunner::getMoveHistory() const {
    std::lock_guard<std::mutex> lock(_historyMutex);
    return std::vector<BotMoveHistoryEntry>(_moveHistory.begin(), _moveHistory.end());
}

void OctiBotRunner::updateSearchProgress(bool searching, int depth, std::uint64_t nodes, int score) {
    _isSearching.store(searching);
    _liveDepth.store(depth);
    _liveNodes.store(nodes);
    _liveScore.store(score);
}

int OctiBotRunner::turnSeat() const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _activeGameView.turnSeat;
}

int OctiBotRunner::plyCount() const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _activeGameView.plyCount;
}

std::string OctiBotRunner::currentFen() const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _activeGameView.fen;
}

std::string OctiBotRunner::renderAsciiBoard() const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    if (!_activeBoard) return "";
    std::ostringstream oss;
    oss << *_activeBoard;
    return oss.str();
}

std::vector<GameSeatInfo> OctiBotRunner::currentSeats() const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _activeGameView.seats;
}

std::int64_t OctiBotRunner::currentTtlRemainingMs() const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _activeGameView.ttlRemainingMs;
}

static bool isBooleanConfigKey(const std::string &key) {
    static const std::set<std::string> kBoolKeys = {
        "qsearch_include_base_threat_prongs",
        "use_opening_book",
        "use_pvs",
        "use_killers",
        "use_history",
        "qsearch_use_tt",
        "lazy_smp_diversify_helpers",
        "lazy_smp_stagger_helpers",
        "fix_stack_and_home_eval_mirage",
        "fix_4p_coalition_eval",
        "friends_only",
        "host_super_prongs",
        "host_edgeless",
        "host_time_control_enabled",
        "publish_move_telemetry",
        "announce_move_search_chat",
        "timed_auto_pace",
        "accept_tournament_matches",
        "tournament_suppress_casual_chat",
        "server_use_ssl",
        "allow_supersede",
        "lobby_only"
    };
    return kBoolKeys.count(key) > 0;
}

static bool parseStrictBoolToken(const std::string &raw, bool &outVal) {
    size_t s = 0;
    while (s < raw.size() && std::isspace(static_cast<unsigned char>(raw[s]))) {
        ++s;
    }
    size_t e = raw.size();
    while (e > s && std::isspace(static_cast<unsigned char>(raw[e - 1]))) {
        --e;
    }
    std::string lower = raw.substr(s, e - s);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
        outVal = true;
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
        outVal = false;
        return true;
    }
    return false;
}

static bool configValueEquals(const std::string &key,
                              const std::string &a,
                              const std::string &b) {
    if (a == b) {
        return true;
    }
    if (key == "verbosity") {
        int la = OctiBotConfigStore::parseVerbosityLevel(a, -1);
        int lb = OctiBotConfigStore::parseVerbosityLevel(b, -2);
        return la >= 0 && la == lb;
    }
    if (isBooleanConfigKey(key)) {
        bool ba = false;
        bool bb = false;
        if (parseStrictBoolToken(a, ba) && parseStrictBoolToken(b, bb)) {
            return ba == bb;
        }
    }
    return false;
}

static std::map<std::string, std::string> buildEffectiveConfigMapImpl(
    const OctiBotConfigStore &cfg,
    bool lobbyOnlyFlag,
    bool pubTelemetry,
    bool announceSearchChat,
    int verbosityLevel) {
    std::map<std::string, std::string> effectiveDefaults;

    // 1. Seed all 85 BotTuningProfile parameters with real C++ defaults + store overrides
    BotTuningProfile prof;
    applyConfigToTuningProfile(cfg, prof);
    serializeTuningProfileToMap(prof, effectiveDefaults);

    TimedClockBudgetPolicy timedPol;
    applyConfigToTimedClockPolicy(cfg, timedPol);
    serializeTimedClockPolicyToMap(timedPol, effectiveDefaults);

    // 2. Policy defaults
    effectiveDefaults["table_behavior"] = cfg.getString("table_behavior", "auto");
    effectiveDefaults["accept_modes"] = cfg.getString("accept_modes", "2P, 4P, OFK");
    effectiveDefaults["accept_bases"] = cfg.getString("accept_bases", "any");
    effectiveDefaults["accept_super_prongs"] = cfg.getString("accept_super_prongs", "any");
    effectiveDefaults["accept_edgeless"] = cfg.getString("accept_edgeless", "any");
    effectiveDefaults["accept_time_control"] = cfg.getString("accept_time_control", "any");
    effectiveDefaults["min_initial_time_sec"] = std::to_string(cfg.getInt("min_initial_time_sec", 0));
    effectiveDefaults["max_initial_time_sec"] = std::to_string(cfg.getInt("max_initial_time_sec", 14400));
    effectiveDefaults["min_increment_sec"] = std::to_string(cfg.getInt("min_increment_sec", 0));
    effectiveDefaults["accept_takeback"] = cfg.getString("accept_takeback", "any");
    effectiveDefaults["friends_only"] = cfg.getBool("friends_only", false) ? "true" : "false";
    effectiveDefaults["opponent_filter"] = cfg.getString("opponent_filter", "any");
    effectiveDefaults["friend_list"] = cfg.getString("friend_list", "");
    effectiveDefaults["ban_list"] = cfg.getString("ban_list", "");

    effectiveDefaults["host_mode"] = cfg.getString("host_mode", "2P");
    effectiveDefaults["host_bases"] = std::to_string(cfg.getInt("host_bases", 3));
    effectiveDefaults["host_super_prongs"] = cfg.getBool("host_super_prongs", false) ? "true" : "false";
    effectiveDefaults["host_edgeless"] = cfg.getBool("host_edgeless", false) ? "true" : "false";
    effectiveDefaults["host_time_control_enabled"] = cfg.getBool("host_time_control_enabled", false) ? "true" : "false";
    effectiveDefaults["host_initial_ms"] = std::to_string(cfg.getInt64("host_initial_ms", 600000));
    effectiveDefaults["host_increment_ms"] = std::to_string(cfg.getInt64("host_increment_ms", 5000));
    effectiveDefaults["host_seat"] = cfg.getString("host_seat", "wait_for_human");

    effectiveDefaults["accept_draw_eval_threshold"] = std::to_string(cfg.getInt("accept_draw_eval_threshold", -1500));
    effectiveDefaults["accept_undo"] = cfg.getString("accept_undo", "friends_only");
    effectiveDefaults["accept_pause"] = cfg.getString("accept_pause", "always");
    effectiveDefaults["accept_rematch"] = cfg.getString("accept_rematch", "always");
    effectiveDefaults["max_consecutive_rematches"] = std::to_string(cfg.getInt("max_consecutive_rematches", 5));
    effectiveDefaults["claim_forfeit_delay_sec"] = std::to_string(cfg.getInt("claim_forfeit_delay_sec", 10));
    effectiveDefaults["min_move_think_ms"] = std::to_string(cfg.getInt("min_move_think_ms", 0));
    effectiveDefaults["accept_tournament_matches"] = cfg.getBool("accept_tournament_matches", true) ? "true" : "false";
    effectiveDefaults["tournament_post_game_linger_sec"] =
        std::to_string(std::max(0, cfg.getInt("tournament_post_game_linger_sec", 2)));
    effectiveDefaults["tournament_suppress_casual_chat"] =
        cfg.getBool("tournament_suppress_casual_chat", true) ? "true" : "false";

    // 3. Runner & locked defaults
    effectiveDefaults["algorithm"] = cfg.getString("algorithm", "alpha_beta");
    effectiveDefaults["publish_move_telemetry"] = pubTelemetry ? "true" : "false";
    effectiveDefaults["announce_move_search_chat"] = announceSearchChat ? "true" : "false";
    effectiveDefaults["greeting_lobby"] = cfg.getString("greeting_lobby", "Hello! I am an automated OCTI robot player.");
    effectiveDefaults["greeting_game_start"] = cfg.getString("greeting_game_start", "Good luck and have fun!");
    effectiveDefaults["chat_game_over_win"] = cfg.getString("chat_game_over_win", "Good game!");
    effectiveDefaults["chat_game_over_loss"] = cfg.getString("chat_game_over_loss", "Good game, well played!");
    effectiveDefaults["chat_game_over_draw"] = cfg.getString("chat_game_over_draw", "Good game, well fought!");
    effectiveDefaults["post_game_linger_sec"] = std::to_string(cfg.getInt("post_game_linger_sec", 60));
    effectiveDefaults["chat_goodbye"] = cfg.getString(
        "chat_goodbye",
        "Thanks for the game! Since things are quiet here, I'll head back to the lobby to host a new table\xe2\x80\x94see you next time!");
    effectiveDefaults["verbosity"] = std::to_string(verbosityLevel);

    effectiveDefaults["lobby_only"] = lobbyOnlyFlag ? "true" : "false";

    effectiveDefaults["server_host"] = cfg.getString("server_host", "octi.selfso.com");
    effectiveDefaults["server_port"] = std::to_string(cfg.getInt("server_port", 443));
    effectiveDefaults["server_use_ssl"] = cfg.getBool("server_use_ssl", true) ? "true" : "false";
    effectiveDefaults["allow_supersede"] = cfg.getBool("allow_supersede", false) ? "true" : "false";
    effectiveDefaults["default_profile"] = cfg.getString("default_profile", "stock");
    effectiveDefaults["robot_name"] = cfg.getString("robot_name", "stock");

    // 4. Overlay explicit entries from cfg.entries() (excluding tokens)
    for (const auto &kv : cfg.entries()) {
        if (kv.first == "bot_token" || kv.first.rfind("bot_token_", 0) == 0) {
            continue;
        }
        effectiveDefaults[kv.first] = kv.second;
    }

    return effectiveDefaults;
}

std::map<std::string, std::string> OctiBotRunner::buildEffectiveConfigMap() const {
    return buildEffectiveConfigMapImpl(
        _config,
        isLobbyOnly(),
        _publishMoveTelemetry,
        _announceMoveSearchChat,
        _verbosity);
}

bool OctiBotRunner::applyConfigUpdates(const std::map<std::string, std::string> &updates,
                                      bool saveToDisk,
                                      std::string &errorOut) {
    static const std::set<std::string> lockedKeys = {
        "server_host", "server_port", "server_use_ssl", "default_profile",
        "robot_name", "lobby_only", "config_path", "tokens_path"
    };

    for (const auto &kv : updates) {
        if (lockedKeys.count(kv.first) > 0 ||
            kv.first == "bot_token" ||
            kv.first.rfind("bot_token_", 0) == 0) {
            errorOut = "Configuration key '" + kv.first + "' is locked and cannot be modified at runtime.";
            return false;
        }

        if (kv.first == "host_variants") {
            std::vector<HostVariantSpec> specs;
            if (!OctiBotPolicyEngine::parseHostVariants(kv.second, specs, errorOut)) {
                return false;
            }
        }

        if (isLobbyOnly()) {
            if (kv.first == "table_behavior" && kv.second != "lobby_only" && kv.second != "idle") {
                errorOut = "Cannot change table_behavior to '" + kv.second + "' on a lobby_only robot.";
                return false;
            }
        }
    }

    const std::string activeProf = _config.getString(
        "profile",
        !_config.activeProfile().empty()
            ? _config.activeProfile()
            : _config.getString("robot_name", ""));

    // Snapshot pre-update effective values before mutating _config so untouched
    // inherited/default keys submitted by callers never bloat [activeProf].
    const std::map<std::string, std::string> prevEffective = buildEffectiveConfigMap();

    for (const auto &kv : updates) {
        const std::string &k = kv.first;
        std::string v = kv.second;

        auto prevIt = prevEffective.find(k);
        const bool valueChanged = (prevIt == prevEffective.end()) ||
                                  !configValueEquals(k, prevIt->second, v);

        _config.set(k, v);
        if (!activeProf.empty() && _config.hasProfile(activeProf) &&
            (_config.hasProfileKey(activeProf, k) || valueChanged)) {
            _config.setProfileEntry(activeProf, k, v);
        }

        if (k == "verbosity") {
            _verbosity = OctiBotConfigStore::parseVerbosityLevel(v, _verbosity);
        } else if (k == "publish_move_telemetry") {
            _publishMoveTelemetry = _config.getBool("publish_move_telemetry", false);
        } else if (k == "announce_move_search_chat") {
            _announceMoveSearchChat = _config.getBool("announce_move_search_chat", true);
        } else if (k == "table_behavior") {
            _policy.setTableBehavior(v);
        }
    }

    _policy.configure(_config);

    if (_agent) {
        _agent->onConfigure(_config);
    }

    if (saveToDisk) {
        std::string savePath = _configFilePath;
        if (savePath.empty()) {
            savePath = _config.getString("config_path", OctiBotConfigStore::defaultConfigFilePath());
        }
        if (!savePath.empty()) {
            OctiBotConfigStore diskConfig;
            std::string loadErr;
            if (std::ifstream(savePath).good() && diskConfig.loadFile(savePath, loadErr)) {
                if (!activeProf.empty() && diskConfig.hasProfile(activeProf)) {
                    const OctiBotConfigStore diskResolved = diskConfig.resolveProfile(activeProf);
                    const std::string diskBeh = diskResolved.getString("table_behavior", "auto");
                    const bool diskLobbyOnly =
                        diskResolved.getBool("lobby_only", false) ||
                        diskBeh == "lobby_only"
                        ;
                    const std::map<std::string, std::string> diskEffective =
                        buildEffectiveConfigMapImpl(
                            diskResolved,
                            diskLobbyOnly,
                            diskResolved.getBool("publish_move_telemetry", false),
                            diskResolved.getBool("announce_move_search_chat", true),
                            diskResolved.getVerbosityLevel(3));

                    for (const auto &kv : updates) {
                        std::string v = kv.second;
                        auto dIt = diskEffective.find(kv.first);
                        const bool changedFromDisk = (dIt == diskEffective.end()) ||
                                                     !configValueEquals(kv.first, dIt->second, v);
                        if (diskConfig.hasProfileKey(activeProf, kv.first) || changedFromDisk) {
                            diskConfig.setProfileEntry(activeProf, kv.first, v);
                        }
                    }
                } else {
                    const std::string diskBeh = diskConfig.getString("table_behavior", "auto");
                    const bool diskLobbyOnly =
                        diskConfig.getBool("lobby_only", false) ||
                        diskBeh == "lobby_only"
                        ;
                    const std::map<std::string, std::string> diskEffective =
                        buildEffectiveConfigMapImpl(
                            diskConfig,
                            diskLobbyOnly,
                            diskConfig.getBool("publish_move_telemetry", false),
                            diskConfig.getBool("announce_move_search_chat", true),
                            diskConfig.getVerbosityLevel(3));

                    for (const auto &kv : updates) {
                        std::string v = kv.second;
                        auto dIt = diskEffective.find(kv.first);
                        const bool changedFromDisk = (dIt == diskEffective.end()) ||
                                                     !configValueEquals(kv.first, dIt->second, v);
                        if (diskConfig.hasKey(kv.first) || changedFromDisk) {
                            diskConfig.set(kv.first, v);
                        }
                    }
                }
                if (!diskConfig.saveFile(savePath, errorOut)) {
                    return false;
                }
            } else {
                if (!_config.saveFile(savePath, errorOut)) {
                    return false;
                }
            }
            logMsg(2, "Configuration saved to disk: " + savePath);
        }
    }

    wakeLoop(true, true);
    return true;
}

// --- StockAlphaBetaAgent Implementation ---

std::unique_ptr<OctiBotEngine> StockAlphaBetaAgent::createEngine() const {
    return std::make_unique<OctiBotEngine>();
}

OctiBotEngine &StockAlphaBetaAgent::ensureEngine() {
    if (!_engine) {
        _engine = createEngine();
        if (!_engine) {
            _engine = std::make_unique<OctiBotEngine>();
        }
        _engine->setTuningProfile(_profile);
    }
    return *_engine;
}

StockAlphaBetaAgent::StockAlphaBetaAgent(const OctiBotConfigStore &config) {
    onConfigure(config);
}

void StockAlphaBetaAgent::onConfigure(const OctiBotConfigStore &config) {
    applyConfigToTuningProfile(config, _profile);
    _timedClockPolicy = TimedClockBudgetPolicy{};
    applyConfigToTimedClockPolicy(config, _timedClockPolicy);
    _verbosity = config.getVerbosityLevel(2);
    if (_engine) {
        _engine->setTuningProfile(_profile);
    }
}

BotResourceRequirements StockAlphaBetaAgent::resourceRequirements() const {
    BotResourceRequirements req;
    req.cpuThreads = std::max(1, _profile.lazySmpThreads);
    req.usesGpu = false;
    req.gpuStreams = 0;
    const int ttLog2 = std::clamp(_profile.ttSizeLog2, 12, 26);
    const std::uint64_t ttBytes =
        (std::uint64_t{1} << ttLog2) *
        static_cast<std::uint64_t>(OctiBotEngine::transpositionEntrySize());
    req.staticRamMiB = (ttBytes + (1ULL << 20) - 1ULL) >> 20;
    req.scratchRamMiB = static_cast<std::uint64_t>(req.cpuThreads) * 8ULL;
    req.staticVramMiB = 0;
    req.scratchVramMiB = 0;
    req.backendLabel = "alpha_beta(cpu:" + std::to_string(req.cpuThreads) + "T)";
    return req;
}

std::string StockAlphaBetaAgent::computeMove(const OctiBoard &board,
                                            const GameViewData &game,
                                            int mySeat,
                                            std::int64_t remainingClockMs,
                                            BotMoveTelemetry *outTelemetry) {
    (void) mySeat;

    BotTuningProfile searchProfile = _profile;
    const int currentPly = std::max(
        0, std::max(game.plyCount, static_cast<int>(game.moves.size())));
    const TimedMoveBudgetBreakdown budget = computeClockManagedMoveBudget(
        _timedClockPolicy, _profile.timeBudgetMs, game, remainingClockMs, currentPly);
    searchProfile.timeBudgetMs = budget.allocatedBudgetMs;
    OctiBotEngine &eng = ensureEngine();
    eng.setTuningProfile(searchProfile);

    std::atomic<bool> searching{true};
    std::thread monitorThread;
    if (_progressCallback) {
        _progressCallback(0, 0, 0);
    }
    if (_verbosity >= 3 || _progressCallback) {
        monitorThread = std::thread([this, &eng, &searching]() {
            int lastDepth = 0;
            while (searching.load()) {
                for (int sl = 0; sl < 5 && searching.load(); ++sl) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                if (!searching.load()) break;
                int d = eng.liveDepthReached();
                std::uint64_t n = eng.liveNodesVisited();
                int s = eng.liveBestScore();
                if (d > lastDepth && d > 0) {
                    lastDepth = d;
                    if (_progressCallback) {
                        _progressCallback(d, n, s);
                    } else if (_verbosity >= 3) {
                        std::lock_guard<std::mutex> lock(s_logMutex);
                        std::cout << "  [Search progress] depth=" << d << " | nodes=" << n << " | live_score=" << s << std::endl;
                    }
                }
            }
        });
    }

    BotSearchStats stats;
    std::string moveNotation = eng.chooseMove(board, &stats);
    searching.store(false);
    if (monitorThread.joinable()) {
        monitorThread.join();
    }

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
}

void OctiBotAgentRegistry::registerAgent(const std::string &algorithmName,
                                        AgentFactory factory) {
    std::lock_guard<std::mutex> lock(_mutex);
    _factories[algorithmName] = std::move(factory);
}

bool OctiBotAgentRegistry::unregisterAgent(const std::string &algorithmName) {
    std::lock_guard<std::mutex> lock(_mutex);
    return _factories.erase(algorithmName) > 0;
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
      _policy(config)
{

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

    _publishMoveTelemetry = _config.getBool("publish_move_telemetry", false);
    _announceMoveSearchChat = _config.getBool("announce_move_search_chat", true);
    _verbosity = _config.getVerbosityLevel(3);

    if (_agent) {
        _agent->onConfigure(_config);
    }

    std::string beh = _config.getString("table_behavior", "auto");
    if (_config.getBool("lobby_only", false) ||
        beh == "lobby_only"
        ) {
        _lobbyOnly = true;
        _policy.setTableBehavior("lobby_only");
    }

    if (!s_primaryRunner) {
        s_primaryRunner = this;
    }

    {
        std::lock_guard<std::mutex> lock(s_activeRunnersMutex);
        s_activeRunners.insert(this);
    }

    logMsg(3, "Resolved configuration: profile=" + _config.getString("robot_name", "stock") +
           ", verbosity=" + std::to_string(_verbosity) +
           ", algorithm=" + _config.getString("algorithm", "alpha_beta") +
           ", time_budget_ms=" + std::to_string(_config.getInt("time_budget_ms", 1500)) +
           ", max_search_depth=" + std::to_string(_config.getInt("max_search_depth", 6)) +
           ", table_behavior=" + _config.getString("table_behavior", "auto") +
           ", publish_move_telemetry=" + (_publishMoveTelemetry ? "true" : "false") +
           ", announce_move_search_chat=" + (_announceMoveSearchChat ? "true" : "false"));
}

OctiBotRunner::~OctiBotRunner() {
    OctiBotRunner::stop();
    {
        std::lock_guard<std::mutex> lock(s_activeRunnersMutex);
        s_activeRunners.erase(this);
    }
    if (s_primaryRunner == this) {
        s_primaryRunner = nullptr;
    }
}

bool OctiBotRunner::connectAndAuthenticate(std::string &errorOut) {
    logMsg(2, "Connecting to server " + _client.host() + ":" + std::to_string(_client.port()) + "...");

    std::string tokenToUse;
    std::string directToken = _config.getString("bot_token");
    std::string robotName = _config.getString("robot_name");

    if (!robotName.empty()) {
        std::string poolToken = _config.getString("bot_token_" + robotName);
        if (!poolToken.empty() && poolToken.rfind("octi_bot_", 0) == 0) {
            tokenToUse = poolToken;
        }
    }
    if (tokenToUse.empty() && !directToken.empty() && directToken != "auto" &&
        directToken.rfind("octi_bot_", 0) == 0) {
        tokenToUse = directToken;
    }

    // Direct token connect
    if (!tokenToUse.empty()) {
        _client.setBotToken(tokenToUse);
        bool allowSupersede = _config.getBool("allow_supersede", false);
        auto res = _client.connectRobot(!allowSupersede);
        if (!res.ok) {
            if (res.httpStatus == 409 && res.errorCode == "ROBOT_TOKEN_IN_USE") {
                errorOut = "ROBOT_TOKEN_IN_USE: " +
                    (res.errorMessage.empty()
                        ? std::string("Robot token is already in use on another connection")
                        : res.errorMessage) +
                    " (pass --supersede or set allow_supersede = true to take over)";
            } else if (!res.errorMessage.empty()) {
                errorOut = res.errorMessage;
            } else if (!res.errorCode.empty()) {
                errorOut = res.errorCode;
            } else {
                errorOut = "Failed to connect robot";
            }
            logMsg(1, "Authentication failed: " + errorOut);
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
            if (isLobbyOnly()) {
                logMsg(2, "Leaving stale table #" + _identity.activeRoomId + " because robot is lobby_only");
                _client.leaveTable(_identity.activeRoomId);
                _identity.activeRoomId.clear();
            } else {
                std::lock_guard<std::mutex> lock(_stateMutex);
                _currentRoomId = _identity.activeRoomId;
                _resumedActiveRoomId = _identity.activeRoomId;
            }
        }

        _superseded = false;
        _supersededAtMs.store(0);

        logMsg(2, "Authenticated robot @" + _identity.handle + " (uuid=" + _identity.robotUuid +
               ", owner=@" + _identity.ownerHandle + ", record=" + std::to_string(_identity.wins) + "W/" +
               std::to_string(_identity.losses) + "L/" + std::to_string(_identity.draws) + "D" +
               (_identity.activeRoomId.empty() ? "" : ", active_room=" + _identity.activeRoomId) + ")");

        if (_agent) {
            _agent->onConnected(_identity);
        }
        return true;
    }

    // Auto pool connect: iterate token pool
    auto pool = _config.tokenPool();
    if (pool.empty()) {
        errorOut = "No robot token specified and token pool is empty";
        logMsg(1, "Authentication failed: " + errorOut);
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
                if (isLobbyOnly()) {
                    logMsg(2, "Leaving stale table #" + _identity.activeRoomId + " because robot is lobby_only");
                    _client.leaveTable(_identity.activeRoomId);
                    _identity.activeRoomId.clear();
                } else {
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    _currentRoomId = _identity.activeRoomId;
                    _resumedActiveRoomId = _identity.activeRoomId;
                }
            }

            _superseded = false;
            _supersededAtMs.store(0);

            logMsg(2, "Authenticated robot @" + _identity.handle + " (uuid=" + _identity.robotUuid +
                   ", owner=@" + _identity.ownerHandle + ", record=" + std::to_string(_identity.wins) + "W/" +
                   std::to_string(_identity.losses) + "L/" + std::to_string(_identity.draws) + "D" +
                   (_identity.activeRoomId.empty() ? "" : ", active_room=" + _identity.activeRoomId) + ")");

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
    logMsg(1, "Authentication failed: " + errorOut);
    return false;
}

int OctiBotRunner::reclaimSupersededIntervalMs() const {
    return std::max(0, _config.getInt("reclaim_superseded_ms", 15000));
}

bool OctiBotRunner::hasReusableNamedToken() const {
    if (!_client.isRobotToken()) {
        return false;
    }
    std::string directToken = _config.getString("bot_token");
    std::string lowerDirect = directToken;
    std::transform(lowerDirect.begin(), lowerDirect.end(), lowerDirect.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowerDirect == "auto") {
        return false;
    }
    return !_config.resolveTokenForRobot(_config.getString("robot_name")).empty();
}

bool OctiBotRunner::reclaimIfUnused(std::string &errorOut) {
    if (_running.load()) {
        if (!_superseded.load()) {
            return true;
        }
        errorOut = "Runner worker thread is still shutting down";
        return false;
    }

    if (_workerThread && _workerThread->joinable()) {
        if (_workerThread->get_id() == std::this_thread::get_id()) {
            errorOut = "Cannot reclaim session from worker thread";
            return false;
        }
        _workerThread->join();
        _workerThread.reset();
    }

    _client.stopEventStreams();

    if (!_client.isRobotToken()) {
        std::string directToken = _config.getString("bot_token");
        std::string lowerDirect = directToken;
        std::transform(lowerDirect.begin(), lowerDirect.end(), lowerDirect.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lowerDirect == "auto") {
            errorOut = "Auto-pooled robot tokens do not use named standby reclaim";
            return false;
        }
        std::string resolved = _config.resolveTokenForRobot(_config.getString("robot_name"));
        if (resolved.empty()) {
            errorOut = "No reusable robot token configured";
            return false;
        }
        _client.setBotToken(resolved);
    }

    auto res = _client.connectRobot(true); // if_unused = true
    if (!res.ok) {
        if (!res.errorMessage.empty()) {
            errorOut = res.errorMessage;
        } else if (!res.errorCode.empty()) {
            errorOut = res.errorCode;
        } else {
            errorOut = "Robot token is currently in use";
        }
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

    if (!_identity.activeRoomId.empty() && isLobbyOnly()) {
        logMsg(2, "Leaving stale table #" + _identity.activeRoomId + " because robot is lobby_only");
        _client.leaveTable(_identity.activeRoomId);
        _identity.activeRoomId.clear();
    }

    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        _currentRoomId = _identity.activeRoomId;
        _resumedActiveRoomId = _identity.activeRoomId;
        if (_gameStartedHandledRoomId != _identity.activeRoomId) {
            _gameStartedHandledRoomId.clear();
        }
        _currentPhase = "IDLE";
        _currentSeat = 0;
        _activeGameView = GameViewData();
        _activeGameViewFetchedMs = 0;
        _pendingMatchFoundRoomId.clear();
        _explicitAllowRobotsRoomId.clear();
        _pendingSseActions.clear();
        _chatMutedRooms.clear();
        _lastSeatedOpponents.clear();
        _gameOverHandledRoomId.clear();
        _lastLoggedPlyCount = -1;
        _postGameSilenceStartMs.store(0);
    }
    _stopping.store(false);
    onRunnerReclaimed();

    _superseded = false;
    _supersededAtMs.store(0);

    if (_agent) {
        _agent->onConnected(_identity);
    }
    notifyEvent("RECLAIMED");
    return true;
}

void OctiBotRunner::start() {
    if (_running) {
        return;
    }
    if (_workerThread && _workerThread->joinable()) {
        if (_workerThread->get_id() == std::this_thread::get_id()) {
            return;
        }
        _workerThread->join();
        _workerThread.reset();
    }
    _stopping.store(false);
    onRunnerStarting();
    _superseded = false;
    _supersededAtMs.store(0);
    _running = true;
    _workerThread = std::make_unique<std::thread>(&OctiBotRunner::run, this);
}

void OctiBotRunner::run() {
    _running = true;
    onRunnerLoopStarted();

    // Seed initial lobby chat sequence so historical messages before startup are not re-executed
    std::uint64_t initialLobbySeq = 0;
    auto initLobby = _client.fetchLobbyState();
    if (initLobby.ok) {
        if (initLobby.data.latestSeq > 0) {
            initialLobbySeq = static_cast<std::uint64_t>(initLobby.data.latestSeq);
        }
        std::int64_t maxChatSeq = initLobby.data.latestSeq;
        for (const auto &c : initLobby.data.recentChat) {
            if (c.seq > maxChatSeq) {
                maxChatSeq = c.seq;
            }
        }
        std::lock_guard<std::mutex> lock(_stateMutex);
        if (maxChatSeq > _lastProcessedLobbyChatSeq) {
            _lastProcessedLobbyChatSeq = maxChatSeq;
        }
        _lastLobbyChatPollMs = nowMs();
    }

    // SSE setup: always start lobby stream so lobby mentions/invites are received even when seated
    std::string initialRoom;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        _pendingSseActions.clear();
        if (isLobbyOnly() && !_currentRoomId.empty()) {
            _client.leaveTable(_currentRoomId);
            _currentRoomId.clear();
        }
        initialRoom = _currentRoomId;
        _lastLobbyStreamRestartMs = nowMs();
    }

    _roomStateDirty.store(true);
    _lobbyTablesDirty.store(true);

    logMsg(3, "Starting lobby SSE stream");
    _client.startLobbyEventStream(initialLobbySeq,
        [this](const SseEvent &ev) {
            handleLobbySseEvent(ev.event, ev.dataJson);
        });

    if (!initialRoom.empty()) {
        logMsg(3, "Starting game SSE stream for room #" + initialRoom);
        _client.startGameEventStream(initialRoom, 0,
            [this](const SseEvent &ev) {
                handleGameSseEvent(ev.event, ev.dataJson);
            });
    }

    while (_running && !_superseded) {
        {
            std::lock_guard<std::mutex> lock(_loopMutex);
            _lastObservedWakeSeq = _loopWakeSeq;
        }
        loopStep();
        waitForLoopWake(computeNextWaitMs());
    }

    _client.stopEventStreams();
    if (_client.isAuthenticated() && !_superseded) {
        _client.disconnectRobot();
    }
    logMsg(2, "Disconnected cleanly from server.");
    _running = false;
}

void OctiBotRunner::stop() {
    _stopping.store(true);
    _running = false;
    notifyEvent("STOP");
    wakeLoop(true, true);
    logMsg(3, "Stopping runner and SSE streams");
    _client.stopEventStreams();

    if (_workerThread && _workerThread->joinable()) {
        if (_workerThread->get_id() != std::this_thread::get_id()) {
            _workerThread->join();
            _workerThread.reset();
        }
    }

    onMatchReleased("");

    if (_client.isAuthenticated() && !_client.robotLeaseId().empty() && !_superseded) {
        _client.disconnectRobot();
    }

}

void OctiBotRunner::wakeLoop(bool markRoomDirty, bool markLobbyDirty) {
    if (markRoomDirty) {
        _roomStateDirty.store(true);
    }
    if (markLobbyDirty) {
        _lobbyTablesDirty.store(true);
    }
    {
        std::lock_guard<std::mutex> lock(_loopMutex);
        ++_loopWakeSeq;
    }
    _loopWakeCv.notify_all();
}

void OctiBotRunner::waitForLoopWake(int maxWaitMs) {
    if (!_running || _superseded || maxWaitMs <= 0) {
        std::lock_guard<std::mutex> lock(_loopMutex);
        _lastObservedWakeSeq = _loopWakeSeq;
        return;
    }
    std::unique_lock<std::mutex> lock(_loopMutex);
    if (_loopWakeSeq != _lastObservedWakeSeq ||
        !_running || _superseded) {
        _lastObservedWakeSeq = _loopWakeSeq;
        return;
    }
    _loopWakeCv.wait_for(lock, std::chrono::milliseconds(maxWaitMs), [this] {
        return _loopWakeSeq != _lastObservedWakeSeq ||
               !_running ||
               _superseded;
    });
    _lastObservedWakeSeq = _loopWakeSeq;
}

int OctiBotRunner::computeNextWaitMs() const {
    if (!_running || _superseded) {
        return 0;
    }
    {
        std::lock_guard<std::mutex> lock(_loopMutex);
        if (_loopWakeSeq != _lastObservedWakeSeq) {
            return 0;
        }
    }
    bool lobbyStreamOk = _client.isLobbyEventStreamActive();
    bool gameStreamOk = _client.isGameEventStreamActive();
    std::string roomId;
    std::string phase;
    bool ovActive = false;
    bool isTourneyRoom = false;
    std::int64_t graceUntilMs = 0;
    bool hasPendingSse = false;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        roomId = _currentRoomId;
        phase = _currentPhase;
        ovActive = _matchOverride.active;
        isTourneyRoom = _activeGameView.isTournamentRoom;
        graceUntilMs = _matchOverrideJoinGraceUntilMs;
        hasPendingSse = !_pendingSseActions.empty();
    }
    if (hasPendingSse || _pendingTournamentPreemptCheck.load()) {
        return 0;
    }
    if (isLobbyOnly() || _policy.tableBehavior() == "lobby_only") {
        return lobbyStreamOk ? 5000 : 1000;
    }
    if (roomId.empty()) {
        if (ovActive && graceUntilMs > 0) {
            std::int64_t remGrace = graceUntilMs - nowMs();
            if (remGrace <= 0) {
                return 25;
            }
            return static_cast<int>(std::clamp<std::int64_t>(remGrace, 25, 250));
        }
        return lobbyStreamOk ? 2000 : 500;
    }
    if (phase == "WAITING_FOR_PLAYERS") {
        if (_lobbyTablesDirty.load()) {
            std::int64_t remThrottle = 2000 - (nowMs() - _lastLobbyTablesFetchMs);
            if (remThrottle <= 25) {
                return 25;
            }
            return static_cast<int>(std::min<std::int64_t>(remThrottle, 2000));
        }
        return (gameStreamOk && lobbyStreamOk) ? 5000 : 500;
    }
    if (phase == "IN_PROGRESS") {
        return gameStreamOk ? 1000 : 250;
    }
    if (phase == "FINISHED" || phase == "GAME_OVER" || phase == "ABANDONED") {
        int lingerSec = isTourneyRoom
            ? std::max(0, _policy.tournamentPostGameLingerSec())
            : std::max(5, _config.getInt("post_game_linger_sec", 60));
        if (lingerSec <= 0) {
            return 25;
        }
        std::int64_t startMs = _postGameSilenceStartMs.load();
        if (startMs > 0) {
            std::int64_t remMs = (static_cast<std::int64_t>(lingerSec) * 1000) - (nowMs() - startMs);
            if (remMs <= 50) {
                return 50;
            }
            if (remMs < 2000) {
                return static_cast<int>(remMs);
            }
        }
        return 2000;
    }
    return (gameStreamOk && lobbyStreamOk) ? 2000 : 250;
}

bool OctiBotRunner::waitForEvent(int timeoutMs, std::string &outReason) {
    std::unique_lock<std::mutex> lock(_eventMutex);
    if (!_hasPendingEvent) {
        if (timeoutMs > 0) {
            _eventCv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                              [this] { return _hasPendingEvent || !_running || _superseded; });
        } else {
            _eventCv.wait(lock, [this] { return _hasPendingEvent || !_running || _superseded; });
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

bool OctiBotRunner::executeGameAction(const std::string &action, const std::string &arg, std::string &errorOut) {
    if (isLobbyOnly()) {
        errorOut = "Robot is configured as lobby_only and cannot join/host/play games";
        return false;
    }
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

namespace {
std::string normalizeHandleOrId(const std::string &s, char prefixChar) {
    std::string out = s;
    while (!out.empty() && (out.front() == ' ' || out.front() == '\t' || out.front() == prefixChar)) {
        out.erase(out.begin());
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '\t' || out.back() == '\r' || out.back() == '\n')) {
        out.pop_back();
    }
    return out;
}

bool iequalsStr(const std::string &a, const std::string &b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string firstTargetHandle(const std::string &raw) {
    size_t comma = raw.find(',');
    std::string head = (comma == std::string::npos) ? raw : raw.substr(0, comma);
    return normalizeHandleOrId(head, '@');
}

std::string popFirstTargetHandle(const std::string &raw) {
    size_t comma = raw.find(',');
    if (comma == std::string::npos) {
        return normalizeHandleOrId(raw, '@');
    }
    return normalizeHandleOrId(raw.substr(comma + 1), '@');
}

int countTargetHandles(const std::string &raw) {
    if (raw.empty()) return 0;
    int count = 0;
    std::stringstream ss(raw);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!normalizeHandleOrId(item, '@').empty()) {
            ++count;
        }
    }
    return count;
}

bool matchesAnyTargetHandle(const std::string &raw, const std::string &candidate) {
    if (raw.empty() || candidate.empty()) return false;
    std::string normCand = normalizeHandleOrId(candidate, '@');
    if (normCand.empty()) return false;
    std::stringstream ss(raw);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string t = normalizeHandleOrId(item, '@');
        if (!t.empty() && iequalsStr(t, normCand)) {
            return true;
        }
    }
    return false;
}

bool matchesExplicitOverrideVariant(const OctiVariantSpec &tSpec,
                                    const BotMatchOverride &ov) {
    if (!ov.hostMode.empty() && !iequalsStr(tSpec.mode, ov.hostMode)) {
        return false;
    }
    if (ov.hostBases > 0 && tSpec.mode == "2P" && tSpec.bases != ov.hostBases) {
        return false;
    }
    if (ov.hasHostSuperProngs && tSpec.mode != "OFK" &&
        tSpec.superProngs != ov.hostSuperProngs) {
        return false;
    }
    if (ov.hasHostEdgeless && tSpec.mode != "OFK" &&
        tSpec.edgeless != ov.hostEdgeless) {
        return false;
    }
    return true;
}

bool normalizeStrictVariantSpec(const OctiVariantSpec &in,
                                OctiVariantSpec &out,
                                std::string &outErr) {
    outErr.clear();
    std::string u = in.mode;
    while (!u.empty() && (u.front() == ' ' || u.front() == '\t')) {
        u.erase(u.begin());
    }
    while (!u.empty() && (u.back() == ' ' || u.back() == '\t' || u.back() == '\r' || u.back() == '\n')) {
        u.pop_back();
    }
    for (char &c : u) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    if (u.empty() || u == "2P" || u == "2") {
        if (in.bases != 1 && in.bases != 3) {
            outErr = "Unsupported bases count " + std::to_string(in.bases) + " for 2P.";
            return false;
        }
        out.mode = "2P";
        out.bases = in.bases;
        out.superProngs = in.superProngs;
        out.edgeless = in.edgeless;
        return true;
    }
    if (u == "4P" || u == "4") {
        if (in.bases != 0 && in.bases != 1 && in.bases != 3) {
            outErr = "Unsupported bases count " + std::to_string(in.bases) + " for 4P.";
            return false;
        }
        out.mode = "4P";
        out.bases = 3;
        out.superProngs = in.superProngs;
        out.edgeless = in.edgeless;
        return true;
    }
    if (u == "OFK") {
        out.mode = "OFK";
        out.bases = 1;
        out.superProngs = false;
        out.edgeless = false;
        return true;
    }
    OctiGameMode pm = GAME_MODE_2;
    int pb = 3;
    bool psp = false;
    bool ped = false;
    if (parseOctiVariantCode(u, pm, pb, psp, ped)) {
        out.mode = (pm == GAME_MODE_4) ? "4P" : ((pm == GAME_MODE_OFK) ? "OFK" : "2P");
        out.bases = pb;
        out.superProngs = psp;
        out.edgeless = ped;
        return true;
    }
    outErr = "Unknown OCTI variant mode '" + in.mode + "'.";
    return false;
}

std::string sanitizeDeclineTextNoRoomIds(const std::string &raw) {
    std::string out;
    out.reserve(raw.size());
    std::size_t i = 0;
    while (i < raw.size()) {
        std::size_t tokenStart = i;
        bool hasHash = (raw[i] == '#');
        std::size_t gIdx = hasHash ? (i + 1) : i;
        if (gIdx + 2 < raw.size() &&
            (raw[gIdx] == 'g' || raw[gIdx] == 'G') &&
            raw[gIdx + 1] == '-' &&
            std::isalnum(static_cast<unsigned char>(raw[gIdx + 2]))) {
            bool leftBoundary = (tokenStart == 0) ||
                !std::isalnum(static_cast<unsigned char>(raw[tokenStart - 1]));
            if (leftBoundary) {
                std::size_t endIdx = gIdx + 2;
                while (endIdx < raw.size() &&
                       (std::isalnum(static_cast<unsigned char>(raw[endIdx])) ||
                        raw[endIdx] == '-')) {
                    ++endIdx;
                }
                out += "that table";
                i = endIdx;
                continue;
            }
        }
        out.push_back(raw[i]);
        ++i;
    }
    return out;
}

bool isObserverInGameView(const GameViewData &gv,
                          const std::string &handle,
                          const std::string &uuid) {
    const std::string cleanHandle = normalizeHandleOrId(handle, '@');
    const std::size_t n = std::max(gv.observers.size(), gv.observerHandles.size());
    for (std::size_t i = 0; i < n; ++i) {
        const std::string obsUuid = (i < gv.observers.size()) ? gv.observers[i] : std::string();
        const std::string obsHandle = (i < gv.observerHandles.size())
            ? normalizeHandleOrId(gv.observerHandles[i], '@')
            : std::string();
        const std::string cleanObsUuid = normalizeHandleOrId(obsUuid, '@');
        if (!cleanHandle.empty() &&
            ((!obsHandle.empty() && iequalsStr(obsHandle, cleanHandle)) ||
             (!cleanObsUuid.empty() && iequalsStr(cleanObsUuid, cleanHandle)))) {
            return true;
        }
        if (!uuid.empty() &&
            ((!obsUuid.empty() && iequalsStr(obsUuid, uuid)) ||
             (!obsHandle.empty() && iequalsStr(obsHandle, uuid)))) {
            return true;
        }
    }
    return false;
}

void upsertGameViewObserver(GameViewData &gv,
                            const std::string &obsUuid,
                            const std::string &obsHandle) {
    const std::string cleanHandle = normalizeHandleOrId(obsHandle, '@');
    if (obsUuid.empty() && cleanHandle.empty()) {
        return;
    }
    const std::size_t n = std::max(gv.observers.size(), gv.observerHandles.size());
    gv.observers.resize(n);
    gv.observerHandles.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::string curUuid = gv.observers[i];
        const std::string curCleanUuid = normalizeHandleOrId(curUuid, '@');
        const std::string curHandle = normalizeHandleOrId(gv.observerHandles[i], '@');
        const bool matchUuid = !obsUuid.empty() &&
            ((!curUuid.empty() && iequalsStr(curUuid, obsUuid)) ||
             (!curHandle.empty() && iequalsStr(curHandle, obsUuid)));
        const bool matchHandle = !cleanHandle.empty() &&
            ((!curHandle.empty() && iequalsStr(curHandle, cleanHandle)) ||
             (!curCleanUuid.empty() && iequalsStr(curCleanUuid, cleanHandle)));
        if (matchUuid || matchHandle) {
            if (!obsUuid.empty() &&
                (gv.observers[i].empty() ||
                 (!cleanHandle.empty() && iequalsStr(curCleanUuid, cleanHandle)))) {
                gv.observers[i] = obsUuid;
            }
            if (!cleanHandle.empty() && gv.observerHandles[i].empty()) {
                gv.observerHandles[i] = cleanHandle;
            }
            gv.observerCount = std::max(gv.observerCount, static_cast<int>(gv.observers.size()));
            return;
        }
    }
    gv.observers.push_back(obsUuid);
    gv.observerHandles.push_back(cleanHandle);
    gv.observerCount = std::max(gv.observerCount + 1, static_cast<int>(gv.observers.size()));
}

void removeGameViewObserver(GameViewData &gv,
                            const std::string &obsUuid,
                            const std::string &obsHandle) {
    const std::string cleanHandle = normalizeHandleOrId(obsHandle, '@');
    if (obsUuid.empty() && cleanHandle.empty()) {
        return;
    }
    const std::size_t n = std::max(gv.observers.size(), gv.observerHandles.size());
    std::vector<std::string> keptUuids;
    std::vector<std::string> keptHandles;
    keptUuids.reserve(n);
    keptHandles.reserve(n);
    std::size_t removed = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::string curUuid = (i < gv.observers.size()) ? gv.observers[i] : std::string();
        const std::string curCleanUuid = normalizeHandleOrId(curUuid, '@');
        const std::string curHandle = (i < gv.observerHandles.size())
            ? normalizeHandleOrId(gv.observerHandles[i], '@')
            : std::string();
        const bool matchUuid = !obsUuid.empty() &&
            ((!curUuid.empty() && iequalsStr(curUuid, obsUuid)) ||
             (!curHandle.empty() && iequalsStr(curHandle, obsUuid)));
        const bool matchHandle = !cleanHandle.empty() &&
            ((!curHandle.empty() && iequalsStr(curHandle, cleanHandle)) ||
             (!curCleanUuid.empty() && iequalsStr(curCleanUuid, cleanHandle)));
        if (matchUuid || matchHandle) {
            ++removed;
            continue;
        }
        keptUuids.push_back(curUuid);
        keptHandles.push_back(curHandle);
    }
    gv.observers = std::move(keptUuids);
    gv.observerHandles = std::move(keptHandles);
    if (removed > 0) {
        gv.observerCount = static_cast<int>(gv.observers.size());
    } else if (gv.observerCount > static_cast<int>(gv.observers.size())) {
        gv.observerCount = std::max(static_cast<int>(gv.observers.size()), gv.observerCount - 1);
    }
}
} // namespace

bool OctiBotRunner::canCommitToMatch(const std::string &roomId,
                                     const std::string &actionKind,
                                     std::string &outDeclineReason) const {
    (void) roomId;
    (void) actionKind;
    outDeclineReason.clear();
    return true;
}

bool OctiBotRunner::canSelectForMatch(const std::string &targetRoomId) const {
    (void) targetRoomId;
    return true;
}

std::size_t OctiBotRunner::assignedCpuCoreSlotOffset() const {
    return 0;
}

bool OctiBotRunner::shouldAllowPinning() const {
    return true;
}

std::string OctiBotRunner::resourceMaxedDeclineText() const {
    return "Sorry, my hardware resources are maxed out at the moment and I can't play a match.";
}

RunnerResourceStatus OctiBotRunner::resourceStatusSnapshot() const {
    RunnerResourceStatus status;
    if (isLobbyOnly()) {
        status.selfRequirements.cpuThreads = 0;
        status.selfRequirements.usesGpu = false;
        status.selfRequirements.gpuStreams = 0;
        status.selfRequirements.backendLabel = "lobby_only";
    } else if (_agent) {
        status.selfRequirements = _agent->resourceRequirements();
    }
    return status;
}

bool OctiBotRunner::onBeforeCommitMatch(const std::string &roomId,
                                        std::string &outDeclineReason) {
    (void) roomId;
    outDeclineReason.clear();
    return true;
}

void OctiBotRunner::onForceAdoptInProgressMatch(const std::string &roomId) {
    (void) roomId;
}

void OctiBotRunner::onCommittedRoomBound(const std::string &oldKey,
                                         const std::string &roomId) {
    (void) oldKey;
    (void) roomId;
}

void OctiBotRunner::onMatchReleased(const std::string &roomId) {
    (void) roomId;
}

bool OctiBotRunner::shouldStandUpAndUnhostWaitingRoom(const std::string &roomId) const {
    (void) roomId;
    return false;
}

bool OctiBotRunner::isAutonomousMatchmakingPaused() const {
    return false;
}

void OctiBotRunner::onBeforeComputeMove() {
}

std::string OctiBotRunner::buildResourcePromptContext() const {
    return "";
}

void OctiBotRunner::onRunnerStarting() {
}

void OctiBotRunner::onRunnerLoopStarted() {
}

void OctiBotRunner::onRunnerReclaimed() {
}

void OctiBotRunner::onSuperseded() {
}

void OctiBotRunner::onRoomSwitched(const std::string &departedRoomId,
                                   const std::string &newRoomId) {
    (void) newRoomId;
    (void) departedRoomId;
}

void OctiBotRunner::onRoomClosedOrLeft(const std::string &roomId) {
    (void) roomId;
}

void OctiBotRunner::onGamePhaseReset(const std::string &prevRoomId,
                                     const std::string &newRoomId) {
    (void) prevRoomId;
    (void) newRoomId;
}

void OctiBotRunner::onOpponentChangedInRoom(const std::string &roomId,
                                            const GameViewData &view) {
    (void) roomId;
    (void) view;
}

void OctiBotRunner::onRoomStateRefreshed(const std::string &roomId,
                                         const GameViewData &view) {
    (void) roomId;
    (void) view;
}

void OctiBotRunner::onGameStartedHandled(const std::string &roomId,
                                         const GameViewData &view,
                                         int mySeat,
                                         const std::string &opponentHandle) {
    (void) mySeat;
    (void) opponentHandle;
    if (view.isTournamentRoom && _policy.tournamentSuppressCasualChat()) {
        return;
    }
    if (!_agentEmittedGameStartGreeting && shouldEmitProactiveChat(roomId, "game_start")) {
        std::string gMsg = _config.getString("greeting_game_start", "Good luck and have fun!");
        if (!gMsg.empty()) {
            _client.postGameChat(roomId, "ALL", gMsg);
        }
    }
}

void OctiBotRunner::onGameOverHandled(const std::string &roomId,
                                      const GameViewData &view,
                                      int mySeat,
                                      const std::string &result,
                                      bool hasConnectedOpponent,
                                      const std::string &opponentHandle) {
    (void) mySeat;
    (void) hasConnectedOpponent;
    (void) opponentHandle;
    if (view.isTournamentRoom && _policy.tournamentSuppressCasualChat()) {
        return;
    }
    if (shouldEmitProactiveChat(roomId, "game_over")) {
        std::string goMsg;
        if (result == "WIN") {
            goMsg = _config.getString("chat_game_over_win", "Good game!");
        } else if (result == "LOSS") {
            goMsg = _config.getString("chat_game_over_loss", "Good game, well played!");
        } else {
            goMsg = _config.getString("chat_game_over_draw", "Good game, well fought!");
        }
        if (!goMsg.empty()) {
            _client.postGameChat(roomId, "ALL", goMsg);
        }
    }
}

void OctiBotRunner::onGameOverSseReceived() {
}

bool OctiBotRunner::isPostGameLingerBlocked() const {
    return false;
}

void OctiBotRunner::onPostGameLingerElapsed(const std::string &roomId,
                                            const std::string &opponentHandle,
                                            int lingerSec) {
    (void) lingerSec;
    if (activeGameView().isTournamentRoom && _policy.tournamentSuppressCasualChat()) {
        return;
    }
    if (shouldEmitProactiveChat(roomId, "goodbye")) {
        std::string byeMsg = _config.getString(
            "chat_goodbye",
            "Thanks for the game! Since things are quiet here, I'll head back to the lobby to host a new table\xe2\x80\x94see you next time!");
        if (!opponentHandle.empty()) {
            std::size_t pos = byeMsg.find("{opponent}");
            if (pos != std::string::npos) {
                byeMsg.replace(pos, 10, opponentHandle);
            }
            pos = byeMsg.find("@opponent");
            if (pos != std::string::npos) {
                byeMsg.replace(pos, 9, "@" + opponentHandle);
            }
        }
        if (!byeMsg.empty()) {
            _client.postGameChat(roomId, "ALL", byeMsg);
        }
    }
}

bool OctiBotRunner::isKnownRobotHandle(const std::string &handle) const {
    if (handle.empty()) {
        return false;
    }
    if (findActiveRunnerByHandle(handle) != nullptr) {
        return true;
    }
    return false;
}

void OctiBotRunner::enqueueWorkerAction(std::function<void()> fn) {
    if (!fn) {
        return;
    }
    PendingSseAction act;
    act.type = PendingSseActionType::CUSTOM_ACTION;
    act.customAction = std::move(fn);
    enqueuePendingSseAction(std::move(act));
}

void OctiBotRunner::resetPostGameSilenceTimer() {
    _postGameSilenceStartMs.store(nowMs());
}

bool OctiBotRunner::tryAcquireVariantDeclineCooldown(const std::string &channel,
                                                     const std::string &roomId,
                                                     const std::string &senderHandle,
                                                     const std::string &variantLabel) {
    const std::string normSender = normalizeHandleOrId(senderHandle, '@');
    std::string lowerSenderKey = normSender.empty()
        ? normalizeHandleOrId(roomId, '#')
        : normSender;
    for (char &c : lowerSenderKey) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const std::string cooldownKey = channel + "|" + lowerSenderKey + "|" + variantLabel;
    const std::int64_t cooldownTtlMs =
        std::max<std::int64_t>(0, _config.getInt("variant_decline_cooldown_ms", 60000));
    const std::int64_t now = nowMs();
    std::lock_guard<std::mutex> lock(_declineMutex);
    auto it = _variantDeclineCooldownsMs.find(cooldownKey);
    if (it != _variantDeclineCooldownsMs.end() && now < it->second) {
        logMsg(3, "[VariantDecline] Suppressing duplicate decline for key='" + cooldownKey + "'");
        return false;
    }
    _variantDeclineCooldownsMs[cooldownKey] = now + cooldownTtlMs;
    return true;
}

bool OctiBotRunner::tryAcquireResourceDeclineCooldown(const std::string &channel,
                                                      const std::string &roomId,
                                                      const std::string &senderHandle) {
    const std::string normSender = normalizeHandleOrId(senderHandle, '@');
    const std::string normRoom = normalizeHandleOrId(roomId, '#');
    std::string lowerSenderKey = normSender.empty() ? normRoom : normSender;
    for (char &c : lowerSenderKey) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const std::string cooldownKey = channel + "|" + normRoom + "|" + lowerSenderKey;
    const std::int64_t cooldownTtlMs =
        std::max<std::int64_t>(0, _config.getInt("resource_decline_cooldown_ms", 10000));
    const std::int64_t now = nowMs();
    if (cooldownTtlMs > 0) {
        std::lock_guard<std::mutex> lock(_declineMutex);
        auto it = _resourceDeclineCooldownsMs.find(cooldownKey);
        if (it != _resourceDeclineCooldownsMs.end() && now < it->second) {
            logMsg(3, "[ResourceDecline] Suppressing duplicate decline for key='" + cooldownKey + "'");
            return false;
        }
        _resourceDeclineCooldownsMs[cooldownKey] = now + cooldownTtlMs;
    }
    return true;
}

bool OctiBotRunner::applyMatchOverrideWithDetails(const BotMatchOverride &ov,
                                                  std::string &outSummary,
                                                  std::string &errorOut,
                                                  bool *outVariantUnsupported,
                                                  OctiVariantSpec *outUnsupportedSpec,
                                                  bool *outResourceMaxedOut) {
    return applyMatchOverrideImpl(
        ov, outSummary, errorOut, outVariantUnsupported, outUnsupportedSpec, outResourceMaxedOut);
}

void OctiBotRunner::armMatchOverrideWithGrace(const BotMatchOverride &ov,
                                              int graceMs,
                                              const std::string &requesterHandle,
                                              bool requesterIsRobot) {
    const std::string curTarget = firstTargetHandle(ov.targetPlayerHandle);
    const std::string normTargetRoom = normalizeHandleOrId(ov.targetRoomId, '#');
    std::lock_guard<std::mutex> lock(_stateMutex);
    _matchOverride = ov;
    _matchOverride.targetPlayerHandle = normalizeHandleOrId(ov.targetPlayerHandle, '@');
    _matchOverride.targetRoomId = normTargetRoom;
    _matchOverrideJoinGraceUntilMs = nowMs() + std::max(0, graceMs);
    _matchOverrideRequesterHandle = requesterHandle;
    _matchOverrideRequesterIsRobot = requesterIsRobot;
    if (!normTargetRoom.empty() && _excludedRoomId == normTargetRoom) {
        _excludedRoomId.clear();
        _excludedRoomUntilMs = 0;
    }
    if (!curTarget.empty() && iequalsStr(_excludedPlayerHandle, curTarget)) {
        _excludedPlayerHandle.clear();
        _excludedPlayerUntilMs = 0;
    }
}

GameViewData OctiBotRunner::activeGameView() const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _activeGameView;
}

void OctiBotRunner::updateActiveGameView(const GameViewData &view) {
    bool shouldCloseSession = false;
    bool shouldResetPerGameCounters = false;
    std::string prevRoomSnap;
    std::string targetRoom;
    GameViewData gvCopy;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        const std::string prevRoom = _currentRoomId;
        const std::string prevPhase = _currentPhase;
        prevRoomSnap = prevRoom;
        _activeGameView = view;
        _activeGameViewFetchedMs = nowMs();
        if (!view.roomId.empty()) {
            _currentRoomId = normalizeHandleOrId(view.roomId, '#');
            _activeGameView.roomId = _currentRoomId;
        }
        if (!prevRoom.empty() && !_currentRoomId.empty() && prevRoom != _currentRoomId) {
            shouldCloseSession = true;
            shouldResetPerGameCounters = true;
            _lastSeatedOpponents.clear();
        }
        if (!view.phase.empty()) {
            _currentPhase = view.phase;
        }
        if (prevPhase != _currentPhase &&
            (_currentPhase == "IN_PROGRESS" ||
             _currentPhase == "FINISHED" ||
             _currentPhase == "GAME_OVER" ||
             _currentPhase == "ABANDONED" ||
             (prevPhase == "IN_PROGRESS" && _currentPhase == "WAITING_FOR_PLAYERS"))) {
            shouldResetPerGameCounters = true;
        }
        for (const auto &seat : _activeGameView.seats) {
            if ((!_identity.handle.empty() && iequalsStr(seat.handle, _identity.handle)) ||
                (!_identity.robotUuid.empty() && seat.robotUuid == _identity.robotUuid)) {
                _currentSeat = seat.seat;
                break;
            }
        }
        std::set<std::string> seatedOpps;
        for (const auto &seat : _activeGameView.seats) {
            if (seat.seat == _currentSeat) {
                continue;
            }
            std::string h = normalizeHandleOrId(seat.handle, '@');
            if (!h.empty() && (_identity.handle.empty() || !iequalsStr(h, _identity.handle))) {
                for (char &c : h) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                seatedOpps.insert(h);
            }
        }
        if (!_lastSeatedOpponents.empty() && seatedOpps != _lastSeatedOpponents &&
            (_currentPhase == "WAITING_FOR_PLAYERS" || !seatedOpps.empty())) {
            shouldCloseSession = true;
        }
        _lastSeatedOpponents = std::move(seatedOpps);
        targetRoom = _currentRoomId;
        gvCopy = _activeGameView;
    }
    if (shouldResetPerGameCounters) {
        onGamePhaseReset(prevRoomSnap, targetRoom);
    }
    if (shouldCloseSession) {
        onOpponentChangedInRoom(targetRoom, gvCopy);
    }
    if (!targetRoom.empty()) {
        onRoomStateRefreshed(targetRoom, gvCopy);
    }
}

void OctiBotRunner::setRoomChatMuted(const std::string &rawRoomId, bool muted) {
    std::string roomId = normalizeHandleOrId(rawRoomId, '#');
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        if (roomId.empty()) {
            roomId = _currentRoomId;
        }
        if (roomId.empty()) {
            return;
        }
        if (muted) {
            _chatMutedRooms.insert(roomId);
        } else {
            _chatMutedRooms.erase(roomId);
        }
    }
}

bool OctiBotRunner::isRoomChatMuted(const std::string &rawRoomId) const {
    std::string roomId = normalizeHandleOrId(rawRoomId, '#');
    std::lock_guard<std::mutex> lock(_stateMutex);
    if (roomId.empty()) {
        roomId = _currentRoomId;
    }
    if (roomId.empty()) {
        return false;
    }
    return _chatMutedRooms.count(roomId) > 0;
}

bool OctiBotRunner::shouldEmitProactiveChat(const std::string &rawRoomId,
                                            const std::string &eventKind) const {
    (void) eventKind;
    return !isRoomChatMuted(rawRoomId);
}

bool OctiBotRunner::joinRoomById(const std::string &rawRoomId, int &outSeat, std::string &errorOut) {
    return joinRoomById(rawRoomId, 0, outSeat, errorOut);
}

bool OctiBotRunner::joinRoomById(const std::string &rawRoomId, int preferredSeat, int &outSeat, std::string &errorOut) {
    if (!joinRoomByIdImpl(rawRoomId, preferredSeat, outSeat, errorOut, false, true, nullptr)) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        _explicitAllowRobotsRoomId = _currentRoomId;
    }
    return true;
}

bool OctiBotRunner::joinRoomByIdImpl(const std::string &rawRoomId,
                                     int preferredSeat,
                                     int &outSeat,
                                     std::string &errorOut,
                                     bool forceAdopt,
                                     bool commitReservation,
                                     bool *outResourceMaxedOut) {
    if (outResourceMaxedOut) {
        *outResourceMaxedOut = false;
    }
    if (isLobbyOnly()) {
        errorOut = "Robot is configured as lobby_only and cannot join/host/play games";
        return false;
    }
    std::string roomId = normalizeHandleOrId(rawRoomId, '#');
    bool reservedHere = false;
    if (!forceAdopt && commitReservation) {
        std::string declineReason;
        if (!onBeforeCommitMatch(roomId, declineReason)) {
            errorOut = declineReason.empty() ? resourceMaxedDeclineText() : declineReason;
            if (outResourceMaxedOut) {
                *outResourceMaxedOut = true;
            }
            return false;
        }
        reservedHere = true;
    }
    auto joinRes = (preferredSeat > 0)
        ? _client.joinTable(roomId, preferredSeat)
        : _client.joinTable(roomId);
    if (!joinRes.ok && preferredSeat > 0) {
        joinRes = _client.joinTable(roomId);
    }
    std::string assignedRoomId = roomId;
    int assignedSeat = 0;
    if (joinRes.ok) {
        assignedRoomId = joinRes.data.roomId;
        assignedSeat = joinRes.data.seat;
    } else {
        auto gvRes = _client.fetchGameView(roomId);
        if (gvRes.ok && !_identity.handle.empty()) {
            for (const auto &s : gvRes.data.seats) {
                if (((!_identity.handle.empty() && iequalsStr(s.handle, _identity.handle)) ||
                     (!_identity.robotUuid.empty() && s.robotUuid == _identity.robotUuid)) &&
                    s.seat > 0) {
                    assignedSeat = s.seat;
                    assignedRoomId = gvRes.data.roomId.empty() ? roomId : gvRes.data.roomId;
                    break;
                }
            }
        }
        if (assignedSeat <= 0) {
            if (reservedHere) {
                onMatchReleased(roomId);
            }
            errorOut = joinRes.errorMessage;
            return false;
        }
    }
    if (forceAdopt) {
        onForceAdoptInProgressMatch(assignedRoomId);
    } else if (reservedHere && assignedRoomId != roomId) {
        onCommittedRoomBound(roomId, assignedRoomId);
    }
    bool roomChanged = false;
    std::string departedRoom;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        if (!_currentRoomId.empty() && _currentRoomId != assignedRoomId) {
            departedRoom = _currentRoomId;
            _chatMutedRooms.erase(_currentRoomId);
            roomChanged = true;
        }
        if (roomChanged) {
            _lastSeatedOpponents.clear();
        }
        _chatMutedRooms.erase(assignedRoomId);
        _currentRoomId = assignedRoomId;
        _currentSeat = assignedSeat;
        _lastLoggedPlyCount = -1;
    }
    onRoomSwitched(roomChanged ? departedRoom : std::string(), assignedRoomId);
    outSeat = assignedSeat;
    _client.stopGameEventStream();
    if (!_client.isLobbyEventStreamActive()) {
        _client.startLobbyEventStream(0,
            [this](const SseEvent &ev) {
                handleLobbySseEvent(ev.event, ev.dataJson);
            });
    }
    _client.startGameEventStream(assignedRoomId, 0,
        [this](const SseEvent &ev) {
            handleGameSseEvent(ev.event, ev.dataJson);
        });
    logMsg(2, "Joined table #" + assignedRoomId + " (seat " + std::to_string(assignedSeat) + ")");
    notifyEvent("ROOM_JOINED");
    wakeLoop(true, false);
    return true;
}

bool OctiBotRunner::hostRoomWithRequest(const CreateTableRequest &req, std::string &outRoomId, int &outSeat, std::string &errorOut) {
    if (!hostRoomWithRequestImpl(req, true, outRoomId, outSeat, errorOut, nullptr)) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        if (req.allowRobots) {
            _explicitAllowRobotsRoomId = outRoomId;
        } else if (_explicitAllowRobotsRoomId == outRoomId) {
            _explicitAllowRobotsRoomId.clear();
        }
    }
    return true;
}

bool OctiBotRunner::hostRoomWithRequestImpl(const CreateTableRequest &req,
                                            bool commitImmediately,
                                            std::string &outRoomId,
                                            int &outSeat,
                                            std::string &errorOut,
                                            bool *outResourceMaxedOut) {
    if (outResourceMaxedOut) {
        *outResourceMaxedOut = false;
    }
    if (isLobbyOnly()) {
        errorOut = "Robot is configured as lobby_only and cannot join/host/play games";
        return false;
    }
    std::string pendingKey;
    if (commitImmediately) {
        std::ostringstream pkOss;
        pkOss << "__pending__:" << std::hex << reinterpret_cast<std::uintptr_t>(this);
        pendingKey = pkOss.str();
        std::string declineReason;
        if (!onBeforeCommitMatch(pendingKey, declineReason)) {
            errorOut = declineReason.empty() ? resourceMaxedDeclineText() : declineReason;
            if (outResourceMaxedOut) {
                *outResourceMaxedOut = true;
            }
            return false;
        }
    } else {
        std::string declineReason;
        if (!canCommitToMatch("", "autonomous_host", declineReason)) {
            errorOut = declineReason.empty() ? resourceMaxedDeclineText() : declineReason;
            if (outResourceMaxedOut) {
                *outResourceMaxedOut = true;
            }
            return false;
        }
    }
    auto createRes = _client.createTable(req);
    if (!createRes.ok && req.seat == 0 && createRes.httpStatus == 400) {
        CreateTableRequest retryReq = req;
        int seatCount = (req.mode == "4P") ? 4 : 2;
        thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(1, seatCount);
        retryReq.seat = dist(rng);
        createRes = _client.createTable(retryReq);
    }
    if (!createRes.ok) {
        if (!pendingKey.empty()) {
            onMatchReleased(pendingKey);
        }
        errorOut = createRes.errorMessage;
        return false;
    }
    if (!pendingKey.empty()) {
        onCommittedRoomBound(pendingKey, createRes.data.roomId);
    }
    bool roomChanged = false;
    std::string departedRoom;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        if (!_currentRoomId.empty() && _currentRoomId != createRes.data.roomId) {
            departedRoom = _currentRoomId;
            _chatMutedRooms.erase(_currentRoomId);
            roomChanged = true;
        }
        _lastSeatedOpponents.clear();
        _chatMutedRooms.erase(createRes.data.roomId);
        _currentRoomId = createRes.data.roomId;
        _currentSeat = createRes.data.seat;
        _lastLoggedPlyCount = -1;
    }
    onRoomSwitched(roomChanged ? departedRoom : std::string(), createRes.data.roomId);
    outRoomId = createRes.data.roomId;
    outSeat = createRes.data.seat;
    _client.stopGameEventStream();
    if (!_client.isLobbyEventStreamActive()) {
        _client.startLobbyEventStream(0,
            [this](const SseEvent &ev) {
                handleLobbySseEvent(ev.event, ev.dataJson);
            });
    }
    _client.startGameEventStream(createRes.data.roomId, 0,
        [this](const SseEvent &ev) {
            handleGameSseEvent(ev.event, ev.dataJson);
        });
    logMsg(2, "Hosted table #" + createRes.data.roomId + " (seat " + std::to_string(createRes.data.seat) +
           ", mode=" + req.mode + ", bases=" + std::to_string(req.bases) +
           ", super_prongs=" + (req.superProngs ? "true" : "false") +
           ", edgeless=" + (req.edgeless ? "true" : "false") +
           ", allow_robots=" + (req.allowRobots ? "true" : "false") + ")");
    notifyEvent("ROOM_HOSTED");
    wakeLoop(true, false);
    return true;
}

bool OctiBotRunner::standUpFromWaitingRoomWithoutResign(const std::string &rawRoomId,
                                                        bool &outTransitionedToInProgress) {
    outTransitionedToInProgress = false;
    std::string roomId = normalizeHandleOrId(rawRoomId, '#');
    bool wasCurrent = false;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        if (roomId.empty()) {
            roomId = _currentRoomId;
        }
        if (roomId == _currentRoomId && !roomId.empty()) {
            wasCurrent = true;
            if (_currentPhase == "IN_PROGRESS" || _activeGameView.phase == "IN_PROGRESS") {
                outTransitionedToInProgress = true;
                return false;
            }
        }
    }
    if (roomId.empty()) {
        return false;
    }

    auto adoptInProgressRoom = [&](const GameViewData &gv) -> bool {
        outTransitionedToInProgress = true;
        int mySeat = 0;
        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            if (_currentRoomId == roomId || _currentRoomId.empty()) {
                _currentRoomId = roomId;
                _activeGameView = gv;
                _activeGameViewFetchedMs = nowMs();
                _currentPhase = gv.phase;
                for (const auto &seat : gv.seats) {
                    if ((!_identity.handle.empty() && seat.handle == _identity.handle) ||
                        (!_identity.robotUuid.empty() && seat.robotUuid == _identity.robotUuid)) {
                        _currentSeat = seat.seat;
                        mySeat = seat.seat;
                        break;
                    }
                }
            }
        }
        (void) mySeat;
        if (_running && !_superseded && !_client.isGameEventStreamActive()) {
            _client.startGameEventStream(roomId, 0,
                [this](const SseEvent &ev) {
                    handleGameSseEvent(ev.event, ev.dataJson);
                });
        }
        onForceAdoptInProgressMatch(roomId);
        logMsg(2, "Aborted waiting-room stand-up for #" + roomId +
               ": table transitioned to IN_PROGRESS; staying to finish match.");
        wakeLoop(true, false);
        return false;
    };

    // Pre-check server phase before calling leaveTable, because GameRoomManager::leaveTable
    // returns 200 OK (disconnecting the seat) on IN_PROGRESS rooms rather than HTTP 400/409.
    auto gvBefore = _client.fetchGameView(roomId);
    if (gvBefore.ok && gvBefore.data.phase == "IN_PROGRESS") {
        return adoptInProgressRoom(gvBefore.data);
    }

    bool leftOk = (gvBefore.httpStatus == 404 || gvBefore.httpStatus == 410);
    if (!leftOk) {
        auto r = _client.leaveTable(roomId);
        leftOk = r.ok || r.httpStatus == 404 || r.httpStatus == 410;
    }

    if (gvBefore.httpStatus != 404 && gvBefore.httpStatus != 410) {
        auto gvRes = _client.fetchGameView(roomId);
        if (gvRes.ok) {
            if (gvRes.data.phase == "IN_PROGRESS") {
                // Microsecond race between gvBefore and leaveTable: re-attach seat and adopt.
                _client.joinTable(roomId);
                return adoptInProgressRoom(gvRes.data);
            }
            if (!leftOk) {
                if (gvRes.data.phase == "WAITING_FOR_PLAYERS") {
                    auto pRes = _client.purgeTable(roomId);
                    if (pRes.ok) {
                        leftOk = true;
                    } else {
                        auto r2 = _client.leaveTable(roomId);
                        leftOk = r2.ok || r2.httpStatus == 404 || r2.httpStatus == 410;
                        if (!leftOk) {
                            bool stillSeatedOrHost =
                                (!gvRes.data.hostUuid.empty() &&
                                 (gvRes.data.hostUuid == _identity.robotUuid ||
                                  gvRes.data.hostUuid == _identity.ownerUuid));
                            for (const auto &s : gvRes.data.seats) {
                                if ((!_identity.handle.empty() && iequalsStr(s.handle, _identity.handle)) ||
                                    (!_identity.robotUuid.empty() && s.robotUuid == _identity.robotUuid)) {
                                    stillSeatedOrHost = true;
                                    break;
                                }
                            }
                            if (!stillSeatedOrHost) {
                                leftOk = true;
                            }
                        }
                    }
                } else if (gvRes.data.phase == "FINISHED" ||
                           gvRes.data.phase == "GAME_OVER" ||
                           gvRes.data.phase == "ABANDONED") {
                    auto r2 = _client.leaveTable(roomId);
                    leftOk = r2.ok || r2.httpStatus == 404 || r2.httpStatus == 410;
                }
            }
        }
    }
    if (!leftOk) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        _chatMutedRooms.erase(roomId);
        if (_explicitAllowRobotsRoomId == roomId) {
            _explicitAllowRobotsRoomId.clear();
        }
        if (_currentRoomId == roomId) {
            _currentRoomId = "";
            _currentSeat = 0;
            _currentPhase = "IDLE";
            _activeGameView = GameViewData();
            _activeGameViewFetchedMs = 0;
            _lastSeatedOpponents.clear();
            _gameOverHandledRoomId = "";
            _gameStartedHandledRoomId.clear();
            _resumedActiveRoomId.clear();
            _lastLoggedPlyCount = -1;
            _postGameSilenceStartMs.store(0);
            wasCurrent = true;
        }
    }
    onRoomClosedOrLeft(roomId);
    onMatchReleased(roomId);
    if (wasCurrent) {
        _client.stopGameEventStream();
        if (_running && !_superseded && !_client.isLobbyEventStreamActive()) {
            std::uint64_t resumeSeq = 0;
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                if (_lastProcessedLobbyChatSeq > 0) {
                    resumeSeq = static_cast<std::uint64_t>(_lastProcessedLobbyChatSeq);
                }
                _lastLobbyStreamRestartMs = nowMs();
            }
            _client.startLobbyEventStream(resumeSeq,
                [this](const SseEvent &ev) {
                    handleLobbySseEvent(ev.event, ev.dataJson);
                });
        }
        logMsg(2, "Stood up from waiting room #" + roomId + " without resigning.");
        notifyEvent("ROOM_LEFT");
        wakeLoop(true, true);
    }
    return true;
}

LeaveRoomResult OctiBotRunner::leaveRoomWithOptions(const std::string &rawRoomId,
                                                    bool allowResign,
                                                    bool preferPly0Abort) {
    LeaveRoomResult out;
    std::string roomId = normalizeHandleOrId(rawRoomId, '#');
    bool wasCurrent = false;
    std::string cachedPhase = "WAITING_FOR_PLAYERS";
    int cachedPlyCount = 0;
    std::string oppHandle;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        if (roomId.empty()) {
            roomId = _currentRoomId;
        }
        if (roomId == _currentRoomId && !roomId.empty()) {
            wasCurrent = true;
            if (!_activeGameView.phase.empty() && _activeGameView.roomId == roomId) {
                cachedPhase = _activeGameView.phase;
            } else if (!_currentPhase.empty() && _currentPhase != "IDLE") {
                cachedPhase = _currentPhase;
            }
            if (_activeGameView.roomId == roomId) {
                cachedPlyCount = std::max(_activeGameView.plyCount,
                                          static_cast<int>(_activeGameView.moves.size()));
            }
            for (const auto &s : _activeGameView.seats) {
                if (s.seat != _currentSeat && !s.handle.empty() && !s.isRobot) {
                    oppHandle = s.handle;
                    break;
                }
            }
        } else if (!roomId.empty() && _activeGameView.roomId == roomId) {
            if (!_activeGameView.phase.empty()) {
                cachedPhase = _activeGameView.phase;
            }
            cachedPlyCount = std::max(_activeGameView.plyCount,
                                      static_cast<int>(_activeGameView.moves.size()));
        }
    }
    if (roomId.empty()) {
        out.error = "Not currently in a room";
        return out;
    }

    auto finalizeLocalLeave = [&](LeaveRoomMode mode,
                                  bool resigned,
                                  bool stoodUpPly0,
                                  const std::string &finalPhase,
                                  int finalPlyCount) -> LeaveRoomResult {
        bool clearedCurrent = wasCurrent;
        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            _excludedRoomId = roomId;
            _excludedRoomUntilMs = nowMs() + 120000;
            if (!oppHandle.empty()) {
                _excludedPlayerHandle = oppHandle;
                _excludedPlayerUntilMs = nowMs() + 60000;
            }
            _chatMutedRooms.erase(roomId);
            if (_explicitAllowRobotsRoomId == roomId) {
                _explicitAllowRobotsRoomId.clear();
            }
            if (_currentRoomId == roomId) {
                _currentRoomId = "";
                _currentSeat = 0;
                _currentPhase = "IDLE";
                _activeGameView = GameViewData();
                _lastSeatedOpponents.clear();
                _gameOverHandledRoomId = "";
                _gameStartedHandledRoomId.clear();
                _resumedActiveRoomId.clear();
                _lastLoggedPlyCount = -1;
                _postGameSilenceStartMs.store(0);
                clearedCurrent = true;
            }
        }
        onRoomClosedOrLeft(roomId);
        onMatchReleased(roomId);
        if (clearedCurrent) {
            _client.stopGameEventStream();
            if (_running && !_superseded && !_client.isLobbyEventStreamActive()) {
                std::uint64_t resumeSeq = 0;
                {
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    if (_lastProcessedLobbyChatSeq > 0) {
                        resumeSeq = static_cast<std::uint64_t>(_lastProcessedLobbyChatSeq);
                    }
                    _lastLobbyStreamRestartMs = nowMs();
                }
                _client.startLobbyEventStream(resumeSeq,
                    [this](const SseEvent &ev) {
                        handleLobbySseEvent(ev.event, ev.dataJson);
                    });
            }
            if (stoodUpPly0) {
                logMsg(2, "Stood up from table #" + roomId + " at Ply 0 without resigning.");
            } else {
                logMsg(2, "Left table #" + roomId + (resigned ? " (resigned active game)" : ""));
            }
            notifyEvent("ROOM_LEFT");
            wakeLoop(true, true);
        }
        LeaveRoomResult done;
        done.ok = true;
        done.resigned = resigned;
        done.stoodUpPly0 = stoodUpPly0;
        done.refusedWouldResign = false;
        done.mode = mode;
        done.phase = finalPhase;
        done.plyCount = finalPlyCount;
        return done;
    };

    auto restartStreamIfStillCurrent = [&]() {
        if (wasCurrent && _running && !_superseded) {
            bool stillCurrent = false;
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                stillCurrent = (_currentRoomId == roomId);
            }
            if (stillCurrent && !_client.isGameEventStreamActive()) {
                _client.startGameEventStream(roomId, 0,
                    [this](const SseEvent &ev) {
                        handleGameSseEvent(ev.event, ev.dataJson);
                    });
            }
        }
    };

    auto resignAndLeaveActiveRoom = [&](const std::string &phaseLabel,
                                        int plyLabel) -> LeaveRoomResult {
        if (wasCurrent) {
            _client.stopGameEventStream();
        }
        auto resRes = _client.resignGame(roomId);
        auto lRes = _client.leaveTable(roomId);
        if (resRes.ok || lRes.ok || lRes.httpStatus == 404 || lRes.httpStatus == 410) {
            return finalizeLocalLeave(
                resRes.ok ? LeaveRoomMode::RESIGNED_AND_LEFT : LeaveRoomMode::FINISHED_LEFT,
                resRes.ok,
                /*stoodUpPly0=*/false,
                phaseLabel,
                plyLabel);
        }
        restartStreamIfStillCurrent();
        LeaveRoomResult failRes;
        failRes.ok = false;
        failRes.phase = phaseLabel;
        failRes.plyCount = plyLabel;
        failRes.error = !lRes.errorMessage.empty() ? lRes.errorMessage : resRes.errorMessage;
        return failRes;
    };

    // Fetch authoritative live server state BEFORE stopping SSE or mutating seat state.
    auto gvRes = _client.fetchGameView(roomId);
    if (gvRes.httpStatus == 404 || gvRes.httpStatus == 410) {
        return finalizeLocalLeave(LeaveRoomMode::FINISHED_LEFT, false, false, "FINISHED", 0);
    }

    std::string livePhase = cachedPhase;
    int livePlyCount = cachedPlyCount;
    if (gvRes.ok) {
        livePhase = gvRes.data.phase;
        livePlyCount = std::max(gvRes.data.plyCount,
                                static_cast<int>(gvRes.data.moves.size()));
        if (oppHandle.empty()) {
            for (const auto &s : gvRes.data.seats) {
                if (!s.handle.empty() && !s.isRobot &&
                    !iequalsStr(s.handle, _identity.handle)) {
                    oppHandle = s.handle;
                    break;
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            if (_currentRoomId == roomId) {
                _activeGameView = gvRes.data;
                _activeGameViewFetchedMs = nowMs();
                if (!gvRes.data.phase.empty()) {
                    _currentPhase = gvRes.data.phase;
                }
            }
        }
    }

    if (livePhase == "WAITING_FOR_PLAYERS") {
        auto r = _client.leaveTable(roomId, /*abortIfPly0=*/preferPly0Abort);
        if (r.ok || r.httpStatus == 404 || r.httpStatus == 410) {
            return finalizeLocalLeave(LeaveRoomMode::WAITING_LEFT, false, false, livePhase, 0);
        }
        // Check whether the table filled (and possibly advanced past Ply 0) in a race.
        auto gvRace = _client.fetchGameView(roomId);
        if (gvRace.httpStatus == 404 || gvRace.httpStatus == 410) {
            return finalizeLocalLeave(LeaveRoomMode::FINISHED_LEFT, false, false, "FINISHED", 0);
        }
        if (gvRace.ok && gvRace.data.phase == "IN_PROGRESS") {
            int racePly = std::max(gvRace.data.plyCount,
                                   static_cast<int>(gvRace.data.moves.size()));
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                if (_currentRoomId == roomId) {
                    _activeGameView = gvRace.data;
                    _activeGameViewFetchedMs = nowMs();
                    _currentPhase = gvRace.data.phase;
                }
            }
            if (preferPly0Abort && racePly == 0) {
                auto abortRes = _client.abortGame(roomId);
                if (abortRes.ok && abortRes.data.stoodUpPly0) {
                    return finalizeLocalLeave(
                        LeaveRoomMode::PLY0_ABORTED, false, true, "IN_PROGRESS", 0);
                }
            }
            if (!allowResign) {
                LeaveRoomResult refused;
                refused.ok = false;
                refused.refusedWouldResign = true;
                refused.mode = LeaveRoomMode::REFUSED_WOULD_RESIGN;
                refused.phase = "IN_PROGRESS";
                refused.plyCount = racePly;
                refused.error = !r.errorMessage.empty()
                    ? r.errorMessage
                    : "Cannot leave active game after moves have been played without resigning";
                return refused;
            }
            return resignAndLeaveActiveRoom("IN_PROGRESS", racePly);
        }
        if (gvRace.ok && (gvRace.data.phase == "FINISHED" ||
                          gvRace.data.phase == "GAME_OVER" ||
                          gvRace.data.phase == "ABANDONED")) {
            auto rFin = _client.leaveTable(roomId, /*abortIfPly0=*/false);
            if (rFin.ok || rFin.httpStatus == 404 || rFin.httpStatus == 410) {
                return finalizeLocalLeave(
                    LeaveRoomMode::FINISHED_LEFT, false, false, gvRace.data.phase,
                    std::max(gvRace.data.plyCount, static_cast<int>(gvRace.data.moves.size())));
            }
        }
        auto pRes = _client.purgeTable(roomId);
        if (pRes.ok) {
            return finalizeLocalLeave(LeaveRoomMode::PURGED, false, false, livePhase, 0);
        }
        out.ok = false;
        out.phase = livePhase;
        out.plyCount = 0;
        out.error = r.errorMessage;
        return out;
    }

    if (livePhase == "IN_PROGRESS") {
        if (preferPly0Abort && livePlyCount == 0) {
            auto abortRes = _client.abortGame(roomId);
            if (abortRes.ok && abortRes.data.stoodUpPly0) {
                return finalizeLocalLeave(
                    LeaveRoomMode::PLY0_ABORTED, false, true, "IN_PROGRESS", 0);
            }
            if (abortRes.httpStatus == 409) {
                auto gvAfter = _client.fetchGameView(roomId);
                if (gvAfter.httpStatus == 404 || gvAfter.httpStatus == 410) {
                    return finalizeLocalLeave(
                        LeaveRoomMode::FINISHED_LEFT, false, false, "FINISHED", 0);
                }
                if (gvAfter.ok) {
                    livePhase = gvAfter.data.phase;
                    livePlyCount = std::max(gvAfter.data.plyCount,
                                            static_cast<int>(gvAfter.data.moves.size()));
                    {
                        std::lock_guard<std::mutex> lock(_stateMutex);
                        if (_currentRoomId == roomId) {
                            _activeGameView = gvAfter.data;
                            _activeGameViewFetchedMs = nowMs();
                            if (!gvAfter.data.phase.empty()) {
                                _currentPhase = gvAfter.data.phase;
                            }
                        }
                    }
                    if (livePhase == "FINISHED" || livePhase == "GAME_OVER" ||
                        livePhase == "ABANDONED") {
                        auto rFin = _client.leaveTable(roomId, /*abortIfPly0=*/false);
                        if (rFin.ok || rFin.httpStatus == 404 || rFin.httpStatus == 410) {
                            return finalizeLocalLeave(
                                LeaveRoomMode::FINISHED_LEFT, false, false, livePhase, livePlyCount);
                        }
                    }
                }
            }
            if (!allowResign) {
                LeaveRoomResult refused;
                refused.ok = false;
                refused.refusedWouldResign = true;
                refused.mode = LeaveRoomMode::REFUSED_WOULD_RESIGN;
                refused.phase = "IN_PROGRESS";
                refused.plyCount = livePlyCount;
                refused.error = !abortRes.errorMessage.empty()
                    ? abortRes.errorMessage
                    : "Cannot stand up without resigning (moves have been played or server rejected Ply-0 abort)";
                return refused;
            }
            return resignAndLeaveActiveRoom("IN_PROGRESS", livePlyCount);
        }

        if (!allowResign) {
            LeaveRoomResult refused;
            refused.ok = false;
            refused.refusedWouldResign = true;
            refused.mode = LeaveRoomMode::REFUSED_WOULD_RESIGN;
            refused.phase = "IN_PROGRESS";
            refused.plyCount = livePlyCount;
            refused.error = "Cannot leave active game at ply " +
                            std::to_string(livePlyCount) +
                            " without resigning (allow_resign=false)";
            return refused;
        }
        return resignAndLeaveActiveRoom("IN_PROGRESS", livePlyCount);
    }

    // FINISHED / GAME_OVER / ABANDONED (or other non-active phase)
    auto r = _client.leaveTable(roomId, /*abortIfPly0=*/false);
    bool purgedOk = false;
    if (!r.ok && r.httpStatus != 404 && r.httpStatus != 410) {
        auto pRes = _client.purgeTable(roomId);
        purgedOk = pRes.ok;
    }
    if (r.ok || purgedOk || r.httpStatus == 404 || r.httpStatus == 410) {
        return finalizeLocalLeave(
            purgedOk ? LeaveRoomMode::PURGED : LeaveRoomMode::FINISHED_LEFT,
            false,
            false,
            livePhase,
            livePlyCount);
    }
    out.ok = false;
    out.phase = livePhase;
    out.plyCount = livePlyCount;
    out.error = r.errorMessage;
    return out;
}

bool OctiBotRunner::leaveRoomById(const std::string &rawRoomId, std::string &errorOut) {
    LeaveRoomResult res = leaveRoomWithOptions(
        rawRoomId, /*allowResign=*/true, /*preferPly0Abort=*/false);
    if (!res.ok) {
        errorOut = res.error;
        return false;
    }
    return true;
}

OctiBotRunner *OctiBotRunner::findActiveRunnerByHandle(const std::string &handle) {
    if (handle.empty()) {
        return nullptr;
    }
    std::string target = normalizeHandleOrId(handle, '@');
    if (target.empty()) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(s_activeRunnersMutex);
    for (auto *runner : s_activeRunners) {
        if (!runner) continue;
        std::string h = runner->identity().handle;
        if (!h.empty() && iequalsStr(h, target)) {
            return runner;
        }
        std::string cfgName = runner->config().getString("robot_name");
        if (!cfgName.empty() && iequalsStr(cfgName, target)) {
            return runner;
        }
    }
    return nullptr;
}

std::vector<OctiBotRunner *> OctiBotRunner::listActiveRunners() {
    std::vector<std::pair<std::string, OctiBotRunner *>> keyed;
    {
        std::lock_guard<std::mutex> lock(s_activeRunnersMutex);
        keyed.reserve(s_activeRunners.size());
        for (auto *runner : s_activeRunners) {
            if (!runner) continue;
            std::string key = runner->identity().handle;
            if (key.empty()) {
                key = runner->config().getString("robot_name");
            }
            for (char &c : key) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            keyed.emplace_back(std::move(key), runner);
        }
    }
    std::sort(keyed.begin(), keyed.end(),
              [](const std::pair<std::string, OctiBotRunner *> &a,
                 const std::pair<std::string, OctiBotRunner *> &b) {
                  if (a.first != b.first) {
                      return a.first < b.first;
                  }
                  return a.second < b.second;
              });
    std::vector<OctiBotRunner *> out;
    out.reserve(keyed.size());
    for (const auto &item : keyed) {
        out.push_back(item.second);
    }
    return out;
}

void OctiBotRunner::wakeAllActiveRunners(bool markRoomDirty, bool markLobbyDirty) {
    std::lock_guard<std::mutex> lock(s_activeRunnersMutex);
    for (auto *runner : s_activeRunners) {
        if (runner) {
            runner->wakeLoop(markRoomDirty, markLobbyDirty);
        }
    }
}

BotMatchOverride OctiBotRunner::matchOverride() const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _matchOverride;
}

void OctiBotRunner::clearMatchOverride() {
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        _matchOverride = BotMatchOverride();
        _matchOverrideRequesterHandle.clear();
        _matchOverrideRequesterIsRobot = false;
        _matchOverrideJoinGraceUntilMs = 0;
    }
    wakeLoop(true, true);
    logMsg(2, "[MatchOverride] Cleared match override; reverted to normal policy.");
}

bool OctiBotRunner::checkAgentSupportsVariant(const OctiVariantSpec &spec,
                                              std::string &outReason) const {
    OctiVariantSpec normSpec;
    if (!normalizeStrictVariantSpec(spec, normSpec, outReason)) {
        return false;
    }
    if (!_agent) {
        outReason = "No bot agent configured.";
        return false;
    }
    std::string agentReason;
    if (_agent->supportsVariant(normSpec, agentReason)) {
        outReason.clear();
        return true;
    }
    if (!agentReason.empty()) {
        outReason = agentReason;
    } else {
        std::string myH = _identity.handle.empty()
            ? _config.getString("robot_name", "octibot")
            : _identity.handle;
        outReason = "@" + myH + " is not configured or trained to play " +
                    formatVariantLabel(normSpec) + ".";
    }
    return false;
}

bool OctiBotRunner::checkPolicyAndAgentSupportsVariant(const OctiVariantSpec &spec,
                                                       std::string &outReason) const {
    OctiVariantSpec normSpec;
    if (!normalizeStrictVariantSpec(spec, normSpec, outReason)) {
        return false;
    }

    auto trimLocal = [](const std::string &s) -> std::string {
        std::size_t a = 0;
        while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        std::size_t b = s.size();
        while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
        return s.substr(a, b - a);
    };
    auto lowerLocal = [](std::string s) -> std::string {
        for (char &c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    };

    std::string rawModes = _config.getString("accept_modes", "2P, 4P, OFK");
    if (!trimLocal(rawModes).empty()) {
        bool modeMatched = false;
        std::stringstream mss(rawModes);
        std::string item;
        while (std::getline(mss, item, ',')) {
            std::string t = trimLocal(item);
            if (!t.empty() && iequalsStr(t, normSpec.mode)) {
                modeMatched = true;
                break;
            }
        }
        if (!modeMatched) {
            outReason = "Policy accept_modes rejects " + normSpec.mode + ".";
            return false;
        }
    }

    std::string acceptBases = lowerLocal(trimLocal(_config.getString("accept_bases", "any")));
    if (!acceptBases.empty() && acceptBases != "any") {
        bool basesOk = (std::to_string(normSpec.bases) == acceptBases) ||
                       (normSpec.mode == "4P" && (acceptBases == "1" || acceptBases == "3"));
        if (!basesOk) {
            outReason = "Policy accept_bases rejects " + formatVariantLabel(normSpec) + ".";
            return false;
        }
    }

    std::string acceptSp = lowerLocal(trimLocal(_config.getString("accept_super_prongs", "any")));
    if (!acceptSp.empty() && acceptSp != "any") {
        bool wantSp = (acceptSp == "true" || acceptSp == "1" || acceptSp == "yes");
        if (normSpec.superProngs != wantSp) {
            outReason = "Policy accept_super_prongs rejects " + formatVariantLabel(normSpec) + ".";
            return false;
        }
    }

    std::string acceptEd = lowerLocal(trimLocal(_config.getString("accept_edgeless", "any")));
    if (!acceptEd.empty() && acceptEd != "any") {
        bool wantEd = (acceptEd == "true" || acceptEd == "1" || acceptEd == "yes");
        if (normSpec.edgeless != wantEd) {
            outReason = "Policy accept_edgeless rejects " + formatVariantLabel(normSpec) + ".";
            return false;
        }
    }

    return checkAgentSupportsVariant(normSpec, outReason);
}

bool OctiBotRunner::applyMatchOverride(const BotMatchOverride &rawOv,
                                       std::string &outSummary,
                                       std::string &errorOut) {
    return applyMatchOverrideImpl(rawOv, outSummary, errorOut, nullptr, nullptr, nullptr);
}

bool OctiBotRunner::applyMatchOverrideImpl(const BotMatchOverride &rawOv,
                                           std::string &outSummary,
                                           std::string &errorOut,
                                           bool *outVariantUnsupported,
                                           OctiVariantSpec *outUnsupportedSpec,
                                           bool *outResourceMaxedOut) {
    if (outVariantUnsupported) {
        *outVariantUnsupported = false;
    }
    if (outResourceMaxedOut) {
        *outResourceMaxedOut = false;
    }
    if (_superseded.load()) {
        errorOut = "Robot session is superseded / disconnected";
        return false;
    }
    if (isLobbyOnly()) {
        errorOut = "Robot is configured as lobby_only and cannot join/host/play games";
        return false;
    }
    std::lock_guard<std::mutex> ovLock(_overrideMutex);
    BotMatchOverride ov = rawOv;
    ov.active = true;
    ov.targetPlayerHandle = normalizeHandleOrId(ov.targetPlayerHandle, '@');
    ov.targetRoomId = normalizeHandleOrId(ov.targetRoomId, '#');
    const std::string selfHandle = !_identity.handle.empty()
        ? normalizeHandleOrId(_identity.handle, '@')
        : normalizeHandleOrId(_config.getString("robot_name", ""), '@');
    if (!selfHandle.empty() && !ov.targetPlayerHandle.empty() &&
        matchesAnyTargetHandle(ov.targetPlayerHandle, selfHandle)) {
        errorOut = "Cannot target self (@" + selfHandle + ") as match opponent";
        return false;
    }
    int numTargets = countTargetHandles(ov.targetPlayerHandle);
    if (ov.remainingGames <= 0) {
        ov.remainingGames = 1;
    }
    if (ov.hostMode != "4P" && ov.targetRoomId.empty() && ov.remainingGames < numTargets) {
        ov.remainingGames = numTargets;
    }
    std::string curTarget = firstTargetHandle(ov.targetPlayerHandle);

    auto failUnsupported = [&](const OctiVariantSpec &badSpec, const std::string &reason) -> bool {
        errorOut = reason;
        if (outVariantUnsupported) {
            *outVariantUnsupported = true;
        }
        if (outUnsupportedSpec) {
            *outUnsupportedSpec = badSpec;
        }
        clearMatchOverride();
        return false;
    };
    auto failResourceMaxed = [&](const std::string &reason) -> bool {
        errorOut = reason.empty() ? resourceMaxedDeclineText() : reason;
        if (outResourceMaxedOut) {
            *outResourceMaxedOut = true;
        }
        clearMatchOverride();
        return false;
    };

    // Pre-validate explicit override variant rules before arming _matchOverride or leaving curRoom
    auto checkExplicitOverrideHints = [&]() -> bool {
        if (!ov.hostMode.empty() || ov.hostBases > 0 || ov.hasHostSuperProngs || ov.hasHostEdgeless) {
            OctiVariantSpec explicitSpec;
            explicitSpec.mode = ov.hostMode.empty() ? _config.getString("host_mode", "2P") : ov.hostMode;
            explicitSpec.bases = (ov.hostBases > 0)
                ? ov.hostBases
                : ((explicitSpec.mode == "4P" || explicitSpec.mode == "OFK")
                       ? 1
                       : _config.getInt("host_bases", 3));
            explicitSpec.superProngs = ov.hasHostSuperProngs
                ? ov.hostSuperProngs
                : _config.getBool("host_super_prongs", false);
            explicitSpec.edgeless = ov.hasHostEdgeless
                ? ov.hostEdgeless
                : _config.getBool("host_edgeless", false);
            if (explicitSpec.mode == "OFK") {
                explicitSpec.bases = 1;
                explicitSpec.superProngs = false;
                explicitSpec.edgeless = false;
            }
            std::string varErr;
            if (!checkAgentSupportsVariant(explicitSpec, varErr)) {
                return failUnsupported(explicitSpec, varErr);
            }
        }
        return true;
    };
    if (ov.targetRoomId.empty() && !checkExplicitOverrideHints()) {
        return false;
    }

    if (!curTarget.empty() && !ov.allowRobots) {
        std::string lowerTarget = ov.targetPlayerHandle;
        std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(), ::tolower);
        if (lowerTarget.find("octibot") != std::string::npos ||
            lowerTarget.find("bot") != std::string::npos) {
            ov.allowRobots = true;
        } else if (_running) {
            auto lobbyRes = _client.fetchLobbyState();
            if (lobbyRes.ok) {
                for (const auto &p : lobbyRes.data.players) {
                    if (iequalsStr(p.handle, curTarget) && p.isRobot) {
                        ov.allowRobots = true;
                        break;
                    }
                }
            }
        }
    }

    std::string curRoom;
    std::string curPhase;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        curRoom = _currentRoomId;
        curPhase = _currentPhase;
    }

    // If a specific targetRoomId is requested and we are connected, validate its variant BEFORE mutating state or leaving curRoom
    if (!ov.targetRoomId.empty() && (_running || _client.isAuthenticated())) {
        auto gvRes = _client.fetchGameView(ov.targetRoomId);
        if (gvRes.ok) {
            OctiVariantSpec roomSpec = OctiVariantSpec::fromGameView(gvRes.data);
            std::string varErr;
            if (!checkAgentSupportsVariant(roomSpec, varErr)) {
                return failUnsupported(roomSpec, varErr);
            }
        } else {
            bool foundRoomInList = false;
            auto tablesRes = _client.listTables(false);
            if (tablesRes.ok) {
                for (const auto &t : tablesRes.data) {
                    if (t.roomId == ov.targetRoomId) {
                        foundRoomInList = true;
                        OctiVariantSpec roomSpec = OctiVariantSpec::fromTable(t);
                        std::string varErr;
                        if (!checkAgentSupportsVariant(roomSpec, varErr)) {
                            return failUnsupported(roomSpec, varErr);
                        }
                        break;
                    }
                }
            }
            if (!foundRoomInList && !checkExplicitOverrideHints()) {
                return false;
            }
        }
    }

    auto armOverrideState = [&]() {
        std::lock_guard<std::mutex> lock(_stateMutex);
        _matchOverride = ov;
        _matchOverrideJoinGraceUntilMs = 0;
        if (!ov.targetRoomId.empty() && _excludedRoomId == ov.targetRoomId) {
            _excludedRoomId.clear();
            _excludedRoomUntilMs = 0;
        }
        if (!curTarget.empty() && iequalsStr(_excludedPlayerHandle, curTarget)) {
            _excludedPlayerHandle.clear();
            _excludedPlayerUntilMs = 0;
        }
    };

    if (!ov.targetRoomId.empty() && curRoom == ov.targetRoomId) {
        if (curPhase != "IN_PROGRESS") {
            armOverrideState();
        }
        wakeLoop(true, true);
        outSummary = "Already in requested table #" + ov.targetRoomId + ".";
        return true;
    }

    if (curPhase == "IN_PROGRESS") {
        armOverrideState();
        logMsg(2, "[MatchOverride] Queued override during active game in #" + curRoom);
        wakeLoop(true, true);
        outSummary = "Match override queued for after current game in #" + curRoom + ".";
        return true;
    }

    if (!_running) {
        std::string resReason;
        if (!canCommitToMatch(ov.targetRoomId, "match_override", resReason)) {
            return failResourceMaxed(resReason);
        }
        armOverrideState();
        wakeLoop(true, true);
        outSummary = "Match override activated: target_player=" +
                     (ov.targetPlayerHandle.empty() ? "(any)" : ("@" + ov.targetPlayerHandle)) +
                     ", target_room=" + (ov.targetRoomId.empty() ? "(any)" : ("#" + ov.targetRoomId));
        return true;
    }

    auto buildOverrideHostRequest = [&]() -> CreateTableRequest {
        auto hostReq = _policy.buildHostTableRequest();
        hostReq.allowRobots = ov.allowRobots;
        if (!ov.hostMode.empty()) {
            hostReq.mode = ov.hostMode;
            if ((ov.hostMode == "4P" || ov.hostMode == "OFK") && ov.hostBases <= 0) {
                hostReq.bases = 1;
            }
        }
        if (ov.hostBases > 0) hostReq.bases = ov.hostBases;
        if (ov.hasHostSuperProngs) hostReq.superProngs = ov.hostSuperProngs;
        if (ov.hasHostEdgeless) hostReq.edgeless = ov.hostEdgeless;
        if (hostReq.mode == "OFK") {
            hostReq.bases = 1;
            hostReq.superProngs = false;
            hostReq.edgeless = false;
        }
        if (ov.preferredSeat > 0) {
            hostReq.seat = ov.preferredSeat;
        } else if (ov.allowRobots && (hostReq.seat == 0 || !curTarget.empty())) {
            hostReq.seat = 1;
        }
        return hostReq;
    };

    // 1. Specific target room requested
    if (!ov.targetRoomId.empty()) {
        std::string resReason;
        if (!canCommitToMatch(ov.targetRoomId, "match_override", resReason)) {
            return failResourceMaxed(resReason);
        }
        armOverrideState();
        logMsg(2, "[MatchOverride] Activated room override: target_room=#" + ov.targetRoomId);
        if (curRoom == ov.targetRoomId) {
            wakeLoop(true, true);
            outSummary = "Already in requested table #" + ov.targetRoomId + ".";
            return true;
        }
        if (!curRoom.empty()) {
            std::string dummyErr;
            leaveRoomById(curRoom, dummyErr);
        }
        int joinedSeat = 0;
        bool joinMaxed = false;
        if (!joinRoomByIdImpl(ov.targetRoomId, ov.preferredSeat, joinedSeat, errorOut, false, true, &joinMaxed)) {
            if (joinMaxed) {
                return failResourceMaxed(errorOut);
            }
            wakeLoop(true, true);
            return false;
        }
        if (!curTarget.empty()) {
            _client.postGameChat(
                ov.targetRoomId, "ALL",
                "@" + curTarget +
                " Joined your table #" + ov.targetRoomId + " for our match!");
        }
        outSummary = "Joined requested table #" + ov.targetRoomId + " (seat " + std::to_string(joinedSeat) + ").";
        return true;
    }

    // 2. Specific target player (human or robot) requested
    if (!curTarget.empty()) {
        std::string targetWaitingRoomId;
        const TableSummaryInfo *compatibleJoinTable = nullptr;
        bool sawUnsupportedTargetWaitingTable = false;
        OctiVariantSpec unsupportedTargetSpec;
        std::string unsupportedTargetReason;

        auto tablesRes = _client.listTables(false);
        if (tablesRes.ok) {
            for (const auto &t : tablesRes.data) {
                bool hasTarget = iequalsStr(t.hostHandle, curTarget);
                for (const auto &s : t.seats) {
                    if (iequalsStr(s.handle, curTarget)) {
                        hasTarget = true;
                        break;
                    }
                }
                if (!hasTarget) {
                    continue;
                }
                if (t.phase == "WAITING_FOR_PLAYERS") {
                    OctiVariantSpec tSpec = OctiVariantSpec::fromTable(t);
                    if (!matchesExplicitOverrideVariant(tSpec, ov)) {
                        if (targetWaitingRoomId.empty() && t.roomId != curRoom) {
                            targetWaitingRoomId = t.roomId;
                        }
                        continue;
                    }
                    std::string tReason;
                    if (!checkAgentSupportsVariant(tSpec, tReason)) {
                        sawUnsupportedTargetWaitingTable = true;
                        unsupportedTargetSpec = tSpec;
                        unsupportedTargetReason = tReason;
                        continue;
                    }
                    if (t.seatsFilled < t.seatsTotal && t.allowRobots) {
                        compatibleJoinTable = &t;
                        break;
                    }
                    if (targetWaitingRoomId.empty() && t.roomId != curRoom) {
                        targetWaitingRoomId = t.roomId;
                    }
                }
            }
        }

        if (compatibleJoinTable != nullptr) {
            std::string resReason;
            if (!canCommitToMatch(compatibleJoinTable->roomId, "match_override_join", resReason)) {
                return failResourceMaxed(resReason);
            }
            armOverrideState();
            if (curRoom != compatibleJoinTable->roomId) {
                if (!curRoom.empty()) {
                    std::string dummyErr;
                    leaveRoomById(curRoom, dummyErr);
                }
                int joinedSeat = 0;
                bool joinMaxed = false;
                if (joinRoomByIdImpl(compatibleJoinTable->roomId, ov.preferredSeat, joinedSeat, errorOut, false, true, &joinMaxed)) {
                    _client.postGameChat(
                        compatibleJoinTable->roomId, "ALL",
                        "@" + curTarget +
                        " Joined your table #" + compatibleJoinTable->roomId + " for our match!");
                    outSummary = "Joined @" + curTarget + "'s open table #" +
                                 compatibleJoinTable->roomId + " (seat " + std::to_string(joinedSeat) + ").";
                    return true;
                }
                if (joinMaxed) {
                    return failResourceMaxed(errorOut);
                }
                clearMatchOverride();
                return false;
            }
            outSummary = "Waiting in table #" + curRoom + " with @" + curTarget + ".";
            return true;
        }

        if (sawUnsupportedTargetWaitingTable) {
            return failUnsupported(unsupportedTargetSpec, unsupportedTargetReason);
        }

        auto hostReq = buildOverrideHostRequest();
        OctiVariantSpec hostSpec = OctiVariantSpec::fromHostRequest(hostReq);
        std::string hostVarErr;
        if (!checkAgentSupportsVariant(hostSpec, hostVarErr)) {
            return failUnsupported(hostSpec, hostVarErr);
        }
        std::string resReason;
        if (!canCommitToMatch("", "match_override_host", resReason)) {
            return failResourceMaxed(resReason);
        }

        armOverrideState();
        if (!curRoom.empty()) {
            std::string dummyErr;
            leaveRoomById(curRoom, dummyErr);
        }
        std::string hostedRoomId;
        int hostedSeat = 0;
        bool hostMaxed = false;
        if (!hostRoomWithRequestImpl(hostReq, true, hostedRoomId, hostedSeat, errorOut, &hostMaxed)) {
            if (hostMaxed) {
                return failResourceMaxed(errorOut);
            }
            clearMatchOverride();
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            _matchOverride.targetRoomId = hostedRoomId;
        }
        std::string inviteMsg = "@" + curTarget +
                                " Let's play a match! Join my table #" + hostedRoomId;
        _client.postGameChat(hostedRoomId, "ALL", inviteMsg);
        _client.postLobbyChat(inviteMsg);
        if (!targetWaitingRoomId.empty() && targetWaitingRoomId != hostedRoomId) {
            _client.observeTable(targetWaitingRoomId);
            _client.postGameChat(targetWaitingRoomId, "ALL", inviteMsg);
            _client.unobserveTable(targetWaitingRoomId);
        }
        outSummary = "Hosted table #" + hostedRoomId + " (allow_robots=" +
                     (hostReq.allowRobots ? "true" : "false") + ") and invited @" +
                     curTarget + ".";
        return true;
    }

    // 3. General rule/robot override without a specific target player or room
    auto hostReq = buildOverrideHostRequest();
    OctiVariantSpec hostSpec = OctiVariantSpec::fromHostRequest(hostReq);
    std::string hostVarErr;
    if (!checkAgentSupportsVariant(hostSpec, hostVarErr)) {
        return failUnsupported(hostSpec, hostVarErr);
    }
    std::string resReason;
    if (!canCommitToMatch("", "match_override_host", resReason)) {
        return failResourceMaxed(resReason);
    }

    armOverrideState();
    if (!curRoom.empty()) {
        std::string dummyErr;
        leaveRoomById(curRoom, dummyErr);
    }
    std::string hostedRoomId;
    int hostedSeat = 0;
    bool hostMaxed = false;
    if (!hostRoomWithRequestImpl(hostReq, true, hostedRoomId, hostedSeat, errorOut, &hostMaxed)) {
        if (hostMaxed) {
            return failResourceMaxed(errorOut);
        }
        clearMatchOverride();
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        _matchOverride.targetRoomId = hostedRoomId;
    }
    outSummary = "Hosted override table #" + hostedRoomId + " (mode=" + hostReq.mode +
                 ", bases=" + std::to_string(hostReq.bases) +
                 ", allow_robots=" + (hostReq.allowRobots ? "true" : "false") + ").";
    return true;
}

void OctiBotRunner::pollRecentLobbyChat() {
    if (_client.isLobbyEventStreamActive()) {
        return;
    }
    std::int64_t now = nowMs();
    std::int64_t lastSeq = 0;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        if (_currentPhase == "IN_PROGRESS") {
            return;
        }
        if (now - _lastLobbyChatPollMs < 2000) {
            return;
        }
        _lastLobbyChatPollMs = now;
        lastSeq = _lastProcessedLobbyChatSeq;
    }

    auto lobbyRes = _client.fetchLobbyState();
    if (!lobbyRes.ok) {
        return;
    }
    for (const auto &c : lobbyRes.data.recentChat) {
        if (c.seq > lastSeq) {
            nlohmann::json j = {
                {"seq", c.seq},
                {"handle", c.handle},
                {"player_uuid", c.playerUuid},
                {"text", c.text},
                {"is_robot", c.isRobot},
                {"created_at_ms", c.createdAtMs},
                {"owner_uuid", c.ownerUuid},
                {"owner_handle", c.ownerHandle}
            };
            handleLobbySseEvent("LOBBY_CHAT", j.dump());
        }
    }
}

void OctiBotRunner::enqueuePendingSseAction(PendingSseAction action) {
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        _pendingSseActions.push_back(std::move(action));
    }
    wakeLoop(true, true);
}

void OctiBotRunner::drainPendingSseActions() {
    while (_running && !_superseded) {
        PendingSseAction act;
        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            if (_pendingSseActions.empty()) {
                break;
            }
            act = std::move(_pendingSseActions.front());
            _pendingSseActions.pop_front();
        }

        switch (act.type) {
        case PendingSseActionType::HANDLE_GAME_PROPOSAL: {
            std::string curRoom = act.roomId.empty() ? currentRoomId() : act.roomId;
            if (curRoom.empty()) {
                break;
            }
            GameViewData gvSnap;
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                gvSnap = _activeGameView;
            }
            if (gvSnap.roomId != curRoom) {
                auto gvRes = _client.fetchGameView(curRoom);
                if (gvRes.ok) {
                    gvSnap = gvRes.data;
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    if (_currentRoomId == curRoom) {
                        _activeGameView = gvRes.data;
                        _activeGameViewFetchedMs = nowMs();
                        _currentPhase = gvRes.data.phase;
                    }
                }
            }

            std::string proposerHandle = act.senderHandle;
            bool proposerIsRobot = act.senderIsRobot;
            if (proposerHandle.empty() || proposerHandle == act.proposal.proposerUuid) {
                for (const auto &s : gvSnap.seats) {
                    if (!act.proposal.proposerUuid.empty() &&
                        (s.playerUuid == act.proposal.proposerUuid ||
                         s.robotUuid == act.proposal.proposerUuid)) {
                        if (!s.handle.empty()) {
                            proposerHandle = s.handle;
                        }
                        proposerIsRobot = s.isRobot;
                        break;
                    }
                }
            }
            if (proposerHandle.empty()) {
                proposerHandle = act.proposal.proposerUuid;
            }

            if (act.proposal.type == "REMATCH_REQUEST") {
                OctiVariantSpec remSpec = OctiVariantSpec::fromGameView(gvSnap);
                std::string remReason;
                if (!checkAgentSupportsVariant(remSpec, remReason)) {
                    logMsg(2, "Declining REMATCH_REQUEST in #" + curRoom +
                           " (unsupported variant " + formatVariantLabel(remSpec) + ")");
                    _client.submitGameAction(curRoom, "decline_rematch");
                    emitVariantDeclineMessage("game", curRoom, proposerHandle, proposerIsRobot,
                                              remSpec, remReason, true);
                    clearMatchOverride();
                    std::string leaveErr;
                    leaveRoomById(curRoom, leaveErr);
                    break;
                }
                std::string resReason;
                if (!canCommitToMatch(curRoom, "rematch", resReason)) {
                    logMsg(2, "Declining REMATCH_REQUEST in #" + curRoom +
                           " (hardware resources maxed out)");
                    _client.submitGameAction(curRoom, "decline_rematch");
                    emitResourceMaxedDeclineMessage("game", curRoom, proposerHandle, proposerIsRobot);
                    clearMatchOverride();
                    std::string leaveErr;
                    leaveRoomById(curRoom, leaveErr);
                    break;
                }
            }

            bool accept = _policy.shouldAcceptProposal(act.proposal, proposerHandle,
                                                       _lastTelemetry.evalScore,
                                                       _consecutiveRematches);
            if (accept && _agent && !_agent->onProposal(act.proposal, gvSnap)) {
                accept = false;
            }
            logMsg(2, "Received proposal " + act.proposal.type + " from " + proposerHandle +
                   " -> Decision: " + (accept ? "ACCEPT" : "DECLINE"));
            if (accept) {
                if (act.proposal.type == "DRAW_OFFER") {
                    _client.submitGameAction(curRoom, "accept_draw");
                } else if (act.proposal.type == "UNDO_REQUEST") {
                    _client.submitGameAction(curRoom, "accept_undo");
                } else if (act.proposal.type == "PAUSE_REQUEST") {
                    _client.submitGameAction(curRoom, "accept_pause");
                } else if (act.proposal.type == "REMATCH_REQUEST") {
                    std::string commitReason;
                    if (!onBeforeCommitMatch(curRoom, commitReason)) {
                        logMsg(2, "Declining REMATCH_REQUEST in #" + curRoom +
                               " (hardware resources maxed out on commit)");
                        _client.submitGameAction(curRoom, "decline_rematch");
                        emitResourceMaxedDeclineMessage("game", curRoom, proposerHandle, proposerIsRobot);
                        clearMatchOverride();
                        std::string leaveErr;
                        leaveRoomById(curRoom, leaveErr);
                        break;
                    }
                    auto remRes = _client.submitGameAction(curRoom, "accept_rematch");
                    if (!remRes.ok) {
                        onMatchReleased(curRoom);
                    } else {
                        _consecutiveRematches++;
                    }
                }
            } else {
                if (act.proposal.type == "DRAW_OFFER") {
                    _client.submitGameAction(curRoom, "decline_draw");
                } else if (act.proposal.type == "UNDO_REQUEST") {
                    _client.submitGameAction(curRoom, "decline_undo");
                } else if (act.proposal.type == "PAUSE_REQUEST") {
                    _client.submitGameAction(curRoom, "decline_pause");
                } else if (act.proposal.type == "REMATCH_REQUEST") {
                    _client.submitGameAction(curRoom, "decline_rematch");
                }
            }
            break;
        }
        case PendingSseActionType::LEAVE_ROOM: {
            std::string targetRoom = act.roomId.empty() ? currentRoomId() : act.roomId;
            if (!targetRoom.empty()) {
                leaveRoomWithOptions(targetRoom, /*allowResign=*/true, /*preferPly0Abort=*/true);
            }
            break;
        }
        case PendingSseActionType::CUSTOM_ACTION: {
            if (act.customAction) {
                act.customAction();
            }
            break;
        }
        }
    }
}

void OctiBotRunner::loopStep() {
    if (_superseded) {
        return;
    }

    if (isLobbyOnly()) {
        std::string staleRoom;
        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            staleRoom = _currentRoomId;
        }
        if (!staleRoom.empty()) {
            _client.leaveTable(staleRoom);
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                _currentRoomId.clear();
                _currentPhase = "IDLE";
                _lastSeatedOpponents.clear();
            }
            onRoomClosedOrLeft(staleRoom);
        }
    }

    pollRecentLobbyChat();
    drainPendingSseActions();

    if (_running && !_superseded && !_client.isLobbyEventStreamActive()) {
        std::int64_t nowCheck = nowMs();
        bool shouldRestartLobby = false;
        std::uint64_t resumeSeq = 0;
        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            if (nowCheck - _lastLobbyStreamRestartMs >= 2000) {
                _lastLobbyStreamRestartMs = nowCheck;
                shouldRestartLobby = true;
                if (_lastProcessedLobbyChatSeq > 0) {
                    resumeSeq = static_cast<std::uint64_t>(_lastProcessedLobbyChatSeq);
                }
            }
        }
        if (shouldRestartLobby) {
            logMsg(3, "Self-healing lobby SSE stream from seq=" + std::to_string(resumeSeq));
            _client.startLobbyEventStream(resumeSeq,
                [this](const SseEvent &ev) {
                    handleLobbySseEvent(ev.event, ev.dataJson);
                });
        }
    }

    if (isLobbyOnly() || _policy.tableBehavior() == "lobby_only") {
        _roomStateDirty.store(false);
        _lobbyTablesDirty.store(false);
        return;
    }

    std::string roomId;
    bool explicitAllowRobotsForRoom = false;
    BotMatchOverride ov;
    std::string exclRoom;
    std::int64_t exclRoomUntil = 0;
    std::string exclPlayer;
    std::int64_t exclPlayerUntil = 0;
    std::int64_t now = nowMs();
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        roomId = _currentRoomId;
        explicitAllowRobotsForRoom =
            (!_currentRoomId.empty() && _explicitAllowRobotsRoomId == _currentRoomId);
        ov = _matchOverride;
        exclRoom = _excludedRoomId;
        exclRoomUntil = _excludedRoomUntilMs;
        exclPlayer = _excludedPlayerHandle;
        exclPlayerUntil = _excludedPlayerUntilMs;
    }

    if (roomId.empty()) {
        _roomStateDirty.store(false);
        bool lobbyStreamOk = _client.isLobbyEventStreamActive();
        bool shouldScanLobby = _lobbyTablesDirty.exchange(false) ||
                               !lobbyStreamOk ||
                               ov.active ||
                               (now - _lastLobbyTablesFetchMs >= 10000);
        if (!shouldScanLobby) {
            return;
        }
        if (ov.active) {
            std::unique_lock<std::mutex> ovTryLock(_overrideMutex, std::try_to_lock);
            if (!ovTryLock.owns_lock()) {
                return;
            }
            std::int64_t graceUntilMs = 0;
            std::string ovRequesterHandle;
            bool ovRequesterIsRobot = false;
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                if (!_currentRoomId.empty()) {
                    return;
                }
                ov = _matchOverride;
                graceUntilMs = _matchOverrideJoinGraceUntilMs;
                ovRequesterHandle = _matchOverrideRequesterHandle;
                ovRequesterIsRobot = _matchOverrideRequesterIsRobot;
            }
            if (!ov.active) {
                return;
            }
            std::string curTarget = firstTargetHandle(ov.targetPlayerHandle);
            auto declineOverrideAndClear = [&](const OctiVariantSpec &badSpec,
                                               const std::string &badReason) {
                std::string who = !ovRequesterHandle.empty() ? ovRequesterHandle : curTarget;
                bool whoIsRobot = !ovRequesterHandle.empty()
                    ? ovRequesterIsRobot
                    : isKnownRobotHandle(who);
                clearMatchOverride();
                if (!who.empty()) {
                    emitVariantDeclineMessage("lobby", "", who, whoIsRobot, badSpec, badReason);
                }
            };
            auto declineResourceMaxedAndClear = [&]() {
                std::string who = !ovRequesterHandle.empty() ? ovRequesterHandle : curTarget;
                bool whoIsRobot = !ovRequesterHandle.empty()
                    ? ovRequesterIsRobot
                    : isKnownRobotHandle(who);
                clearMatchOverride();
                if (!who.empty()) {
                    emitResourceMaxedDeclineMessage("lobby", "", who, whoIsRobot);
                }
            };

            if (!ov.targetRoomId.empty()) {
                auto gvTarget = _client.fetchGameView(ov.targetRoomId);
                if (gvTarget.ok) {
                    OctiVariantSpec rSpec = OctiVariantSpec::fromGameView(gvTarget.data);
                    std::string rReason;
                    if (!checkAgentSupportsVariant(rSpec, rReason)) {
                        declineOverrideAndClear(rSpec, rReason);
                        return;
                    }
                }
                int joinedSeat = 0;
                std::string joinErr;
                bool joinMaxed = false;
                if (joinRoomByIdImpl(ov.targetRoomId, ov.preferredSeat, joinedSeat, joinErr, false, true, &joinMaxed)) {
                    {
                        std::lock_guard<std::mutex> lock(_stateMutex);
                        _matchOverrideJoinGraceUntilMs = 0;
                    }
                    if (!curTarget.empty()) {
                        _client.postGameChat(
                            ov.targetRoomId, "ALL",
                            "@" + curTarget +
                            " Joined your table #" + ov.targetRoomId + " for our match!");
                        _client.postLobbyChat(
                            "On my way! Joined @" + curTarget + " in table #" + ov.targetRoomId + ".");
                    }
                    return;
                }
                if (joinMaxed) {
                    declineResourceMaxedAndClear();
                    return;
                }
            }
            std::string targetWaitingRoomId;
            bool sawUnsupportedTargetTable = false;
            OctiVariantSpec unsupportedTargetSpec;
            std::string unsupportedTargetReason;
            auto tablesRes = _client.listTables(false);
            _lastLobbyTablesFetchMs = nowMs();
            if (tablesRes.ok && !curTarget.empty()) {
                for (const auto &t : tablesRes.data) {
                    if (t.phase != "WAITING_FOR_PLAYERS") {
                        continue;
                    }
                    bool hasTarget = iequalsStr(t.hostHandle, curTarget);
                    for (const auto &s : t.seats) {
                        if (iequalsStr(s.handle, curTarget)) {
                            hasTarget = true;
                            break;
                        }
                    }
                    if (!hasTarget) {
                        continue;
                    }
                    OctiVariantSpec tSpec = OctiVariantSpec::fromTable(t);
                    if (!matchesExplicitOverrideVariant(tSpec, ov)) {
                        if (targetWaitingRoomId.empty()) {
                            targetWaitingRoomId = t.roomId;
                        }
                        continue;
                    }
                    std::string tReason;
                    if (!checkAgentSupportsVariant(tSpec, tReason)) {
                        sawUnsupportedTargetTable = true;
                        unsupportedTargetSpec = tSpec;
                        unsupportedTargetReason = tReason;
                        continue;
                    }
                    if (t.seatsFilled >= t.seatsTotal) {
                        continue;
                    }
                    if (t.allowRobots) {
                        int joinedSeat = 0;
                        std::string joinErr;
                        bool joinMaxed = false;
                        if (joinRoomByIdImpl(t.roomId, ov.preferredSeat, joinedSeat, joinErr, false, true, &joinMaxed)) {
                            {
                                std::lock_guard<std::mutex> lock(_stateMutex);
                                _matchOverrideJoinGraceUntilMs = 0;
                            }
                            _client.postGameChat(
                                t.roomId, "ALL",
                                "@" + curTarget +
                                " Joined your table #" + t.roomId + " for our match!");
                            _client.postLobbyChat(
                                "On my way! Joined @" + curTarget + " in table #" + t.roomId + ".");
                            return;
                        }
                        if (joinMaxed) {
                            declineResourceMaxedAndClear();
                            return;
                        }
                    } else if (targetWaitingRoomId.empty()) {
                        targetWaitingRoomId = t.roomId;
                    }
                }
            }
            if (sawUnsupportedTargetTable) {
                declineOverrideAndClear(unsupportedTargetSpec, unsupportedTargetReason);
                return;
            }

            std::int64_t nowAfterScan = nowMs();
            if (!curTarget.empty() && graceUntilMs > 0 && nowAfterScan < graceUntilMs) {
                bool targetOnline = false;
                if (tablesRes.ok) {
                    for (const auto &t : tablesRes.data) {
                        if (iequalsStr(t.hostHandle, curTarget)) {
                            targetOnline = true;
                            break;
                        }
                        for (const auto &s : t.seats) {
                            if (iequalsStr(s.handle, curTarget) && s.connected) {
                                targetOnline = true;
                                break;
                            }
                        }
                        if (targetOnline) {
                            break;
                        }
                    }
                }
                if (!targetOnline) {
                    auto lobbyRes = _client.fetchLobbyState();
                    if (lobbyRes.ok) {
                        for (const auto &p : lobbyRes.data.players) {
                            if (iequalsStr(p.handle, curTarget)) {
                                targetOnline = true;
                                break;
                            }
                        }
                        if (!targetOnline) {
                            for (const auto &t : lobbyRes.data.tables) {
                                if (iequalsStr(t.hostHandle, curTarget)) {
                                    targetOnline = true;
                                    break;
                                }
                                for (const auto &s : t.seats) {
                                    if (iequalsStr(s.handle, curTarget) && s.connected) {
                                        targetOnline = true;
                                        break;
                                    }
                                }
                                if (targetOnline) {
                                    break;
                                }
                            }
                        }
                    }
                }
                if (targetOnline) {
                    return;
                }
            }

            if (graceUntilMs > 0) {
                std::lock_guard<std::mutex> lock(_stateMutex);
                _matchOverrideJoinGraceUntilMs = 0;
            }

            auto hostReq = _policy.buildHostTableRequest();
            hostReq.allowRobots = ov.allowRobots;
            if (!ov.hostMode.empty()) {
                hostReq.mode = ov.hostMode;
                if ((ov.hostMode == "4P" || ov.hostMode == "OFK") && ov.hostBases <= 0) {
                    hostReq.bases = 1;
                }
            }
            if (ov.hostBases > 0) hostReq.bases = ov.hostBases;
            if (ov.hasHostSuperProngs) hostReq.superProngs = ov.hostSuperProngs;
            if (ov.hasHostEdgeless) hostReq.edgeless = ov.hostEdgeless;
            if (hostReq.mode == "OFK") {
                hostReq.bases = 1;
                hostReq.superProngs = false;
                hostReq.edgeless = false;
            }
            if (ov.preferredSeat > 0) {
                hostReq.seat = ov.preferredSeat;
            } else if (ov.allowRobots && hostReq.seat == 0) {
                hostReq.seat = 1;
            }
            OctiVariantSpec hostSpec = OctiVariantSpec::fromHostRequest(hostReq);
            std::string hostVarReason;
            if (!checkAgentSupportsVariant(hostSpec, hostVarReason)) {
                declineOverrideAndClear(hostSpec, hostVarReason);
                return;
            }
            std::string hostResReason;
            if (!canCommitToMatch("", "match_override_host", hostResReason)) {
                declineResourceMaxedAndClear();
                return;
            }
            std::string hostedRoomId;
            int hostedSeat = 0;
            std::string hostErr;
            bool hostMaxed = false;
            if (hostRoomWithRequestImpl(hostReq, true, hostedRoomId, hostedSeat, hostErr, &hostMaxed)) {
                {
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    if (_matchOverride.active) {
                        _matchOverride.targetRoomId = hostedRoomId;
                    }
                }
                if (!curTarget.empty()) {
                    std::string inviteMsg = "@" + curTarget +
                                            " Let's play a match! Join my table #" + hostedRoomId;
                    _client.postGameChat(hostedRoomId, "ALL", inviteMsg);
                    _client.postLobbyChat(inviteMsg);
                    if (!targetWaitingRoomId.empty() && targetWaitingRoomId != hostedRoomId) {
                        _client.observeTable(targetWaitingRoomId);
                        _client.postGameChat(targetWaitingRoomId, "ALL", inviteMsg);
                        _client.unobserveTable(targetWaitingRoomId);
                    }
                }
                return;
            }
            if (hostMaxed) {
                declineResourceMaxedAndClear();
                return;
            }
        }

        // Lobby state & autonomous matchmaking
        std::string behavior = _policy.tableBehavior();
        if (behavior == "idle") {
            return;
        }

        if (behavior == "auto" || behavior == "join_only" || behavior == "host_only") {
            _pendingTournamentPreemptCheck.store(false);
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                _pendingMatchFoundRoomId.clear();
            }
            auto tablesRes = _client.listTables(false);
            _lastLobbyTablesFetchMs = nowMs();
            if (tablesRes.ok) {
                // Pass 1: Adopt/join any table where we are already seated (prioritizing tournament rooms first)
                for (int tourneyPass = 0; tourneyPass < 2; ++tourneyPass) {
                    const bool wantTourney = (tourneyPass == 0);
                    for (const auto &t : tablesRes.data) {
                        if (t.isTournamentRoom != wantTourney) {
                            continue;
                        }
                        if (t.isTournamentRoom && !_policy.acceptTournamentMatches()) {
                            continue;
                        }
                        if (t.phase != "IN_PROGRESS" && t.phase != "WAITING_FOR_PLAYERS") {
                            continue;
                        }
                        if (!t.isTournamentRoom && !exclRoom.empty() &&
                            t.roomId == exclRoom && now < exclRoomUntil &&
                            (!ov.active || ov.targetRoomId != t.roomId)) {
                            continue;
                        }
                        bool selfSeated = false;
                        bool onlyRobotOpponents = true;
                        bool hasAnyOpponent = false;
                        for (const auto &s : t.seats) {
                            if ((!_identity.handle.empty() && iequalsStr(s.handle, _identity.handle)) ||
                                (!_identity.robotUuid.empty() && s.robotUuid == _identity.robotUuid)) {
                                selfSeated = true;
                            } else if (!s.handle.empty()) {
                                hasAnyOpponent = true;
                                if (!s.isRobot) {
                                    onlyRobotOpponents = false;
                                }
                            }
                        }
                        const bool exemptTournament =
                            t.isTournamentRoom && _policy.acceptTournamentMatches();
                        const bool blockedByCasualHumansOnly =
                            !exemptTournament &&
                            (_policy.opponentFilter() == "humans_only" && !ov.allowRobots &&
                             hasAnyOpponent && onlyRobotOpponents);
                        if (selfSeated && !blockedByCasualHumansOnly) {
                            if (exemptTournament && !_identity.handle.empty()) {
                                for (const auto &stale : tablesRes.data) {
                                    if (stale.roomId != t.roomId && !stale.isTournamentRoom &&
                                        stale.phase == "WAITING_FOR_PLAYERS" &&
                                        stale.seatsFilled == 0 &&
                                        iequalsStr(stale.hostHandle, _identity.handle)) {
                                        _client.purgeTable(stale.roomId);
                                    }
                                }
                            }
                            int joinedSeat = 0;
                            std::string joinErr;
                            if (t.phase == "IN_PROGRESS") {
                                if (joinRoomByIdImpl(t.roomId, 0, joinedSeat, joinErr, true, false, nullptr)) {
                                    return;
                                }
                            } else {
                                if (!exemptTournament && !ov.active &&
                                    (shouldStandUpAndUnhostWaitingRoom(t.roomId) ||
                                     (!hasAnyOpponent && isAutonomousMatchmakingPaused()))) {
                                    bool transitioned = false;
                                    standUpFromWaitingRoomWithoutResign(t.roomId, transitioned);
                                    if (transitioned) {
                                        joinRoomByIdImpl(t.roomId, 0, joinedSeat, joinErr, true, false, nullptr);
                                        return;
                                    }
                                    continue;
                                }
                                bool commitNow = (hasAnyOpponent || ov.active || exemptTournament);
                                bool joinMaxed = false;
                                if (joinRoomByIdImpl(t.roomId, 0, joinedSeat, joinErr, false, commitNow, &joinMaxed)) {
                                    return;
                                }
                                if (joinMaxed && !exemptTournament) {
                                    bool transitioned = false;
                                    standUpFromWaitingRoomWithoutResign(t.roomId, transitioned);
                                    if (transitioned) {
                                        joinRoomByIdImpl(t.roomId, 0, joinedSeat, joinErr, true, false, nullptr);
                                        return;
                                    }
                                    continue;
                                }
                            }
                        }
                    }
                }

                // Pass 2: Recover unseated casual hosted WAITING_FOR_PLAYERS tables
                for (const auto &t : tablesRes.data) {
                    if (t.isTournamentRoom || t.phase != "WAITING_FOR_PLAYERS") {
                        continue;
                    }
                    if (!exclRoom.empty() && t.roomId == exclRoom && now < exclRoomUntil &&
                        (!ov.active || ov.targetRoomId != t.roomId)) {
                        continue;
                    }
                    bool selfSeated = false;
                    for (const auto &s : t.seats) {
                        if ((!_identity.handle.empty() && iequalsStr(s.handle, _identity.handle)) ||
                            (!_identity.robotUuid.empty() && s.robotUuid == _identity.robotUuid)) {
                            selfSeated = true;
                            break;
                        }
                    }
                    if (!selfSeated && !_identity.handle.empty() &&
                        iequalsStr(t.hostHandle, _identity.handle) &&
                        t.phase == "WAITING_FOR_PLAYERS") {
                        std::string unseatedReason;
                        if (!checkAgentSupportsVariant(OctiVariantSpec::fromTable(t), unseatedReason)) {
                            logMsg(2, "Purging unsupported unseated hosted table #" + t.roomId +
                                   " (" + unseatedReason + ")");
                            _client.purgeTable(t.roomId);
                            _client.leaveTable(t.roomId);
                            continue;
                        }
                        if (!ov.active && (isAutonomousMatchmakingPaused() ||
                                           shouldStandUpAndUnhostWaitingRoom(t.roomId))) {
                            bool transitioned = false;
                            standUpFromWaitingRoomWithoutResign(t.roomId, transitioned);
                            if (!transitioned) {
                                continue;
                            }
                        }
                        bool unseatedRoomChanged = false;
                        std::string unseatedDepartedRoom;
                        {
                            std::lock_guard<std::mutex> lock(_stateMutex);
                            if (!_currentRoomId.empty() && _currentRoomId != t.roomId) {
                                unseatedDepartedRoom = _currentRoomId;
                                unseatedRoomChanged = true;
                            }
                            _lastSeatedOpponents.clear();
                            _currentRoomId = t.roomId;
                            _currentSeat = 0;
                            _lastLoggedPlyCount = -1;
                        }
                        if (unseatedRoomChanged) {
                            onRoomSwitched(unseatedDepartedRoom, t.roomId);
                        }
                        _client.stopGameEventStream();
                        if (!_client.isLobbyEventStreamActive()) {
                            _client.startLobbyEventStream(0,
                                [this](const SseEvent &ev) {
                                    handleLobbySseEvent(ev.event, ev.dataJson);
                                });
                        }
                        _client.startGameEventStream(t.roomId, 0,
                            [this](const SseEvent &ev) {
                                handleGameSseEvent(ev.event, ev.dataJson);
                            });
                        logMsg(2, "Recovered unseated hosted table #" + t.roomId);
                        notifyEvent("ROOM_HOSTED");
                        wakeLoop(true, false);
                        return;
                    }
                }

                if (behavior == "auto" || behavior == "join_only") {
                    auto ranked = _policy.rankCompatibleTables(tablesRes.data,
                                                              _identity.ownerHandle,
                                                              _identity.handle);
                    logMsg(3, "Lobby scan: found " + std::to_string(tablesRes.data.size()) +
                           " table(s), " + std::to_string(ranked.size()) + " compatible");
                    for (const auto &cand : ranked) {
                        std::string candVarReason;
                        if (!checkAgentSupportsVariant(OctiVariantSpec::fromTable(cand), candVarReason)) {
                            logMsg(3, "Lobby scan: skipping #" + cand.roomId +
                                   " (agent unsupported variant: " + candVarReason + ")");
                            continue;
                        }
                        if (!exclRoom.empty() && cand.roomId == exclRoom && now < exclRoomUntil &&
                            (!ov.active || ov.targetRoomId != cand.roomId)) {
                            continue;
                        }
                        if (!exclPlayer.empty() && now < exclPlayerUntil &&
                            (!ov.active || !iequalsStr(firstTargetHandle(ov.targetPlayerHandle), exclPlayer))) {
                            bool hasExclPlayer = iequalsStr(cand.hostHandle, exclPlayer);
                            for (const auto &s : cand.seats) {
                                if (iequalsStr(s.handle, exclPlayer)) {
                                    hasExclPlayer = true;
                                    break;
                                }
                            }
                            if (hasExclPlayer) {
                                continue;
                            }
                        }
                        std::string candResReason;
                        if (!canCommitToMatch(cand.roomId, "autonomous_join", candResReason)) {
                            continue;
                        }
                        int joinedSeat = 0;
                        std::string joinErr;
                        if (joinRoomByIdImpl(cand.roomId, 0, joinedSeat, joinErr, false, true, nullptr)) {
                            return;
                        }
                    }
                }
            }
        }

        if ((behavior == "auto" || behavior == "host_only") && !isAutonomousMatchmakingPaused()) {
            std::size_t maxAttempts = std::max<std::size_t>(1, _policy.hostVariants().size());
            for (std::size_t attempt = 0; attempt < maxAttempts; ++attempt) {
                auto hostReq = _policy.buildHostTableRequest();
                std::string hostVarReason;
                if (!checkAgentSupportsVariant(OctiVariantSpec::fromHostRequest(hostReq), hostVarReason)) {
                    logMsg(3, "Skipping autonomous host variant " +
                           formatVariantLabel(OctiVariantSpec::fromHostRequest(hostReq)) +
                           ": " + hostVarReason);
                    continue;
                }
                std::string hostedRoomId;
                int hostedSeat = 0;
                std::string hostErr;
                if (hostRoomWithRequestImpl(hostReq, false, hostedRoomId, hostedSeat, hostErr, nullptr)) {
                    return;
                }
                break;
            }
        }

        if (behavior == "matchmaking" && !isAutonomousMatchmakingPaused()) {
            auto mmReq = _policy.buildMatchmakingRequest();
            std::string mmVarReason;
            if (!checkAgentSupportsVariant(OctiVariantSpec::fromMatchmaking(mmReq), mmVarReason)) {
                logMsg(3, "Skipping autonomous matchmaking enqueue: " + mmVarReason);
                return;
            }
            _client.enqueueMatchmaking(mmReq);
            auto statusRes = _client.fetchMatchmakingStatus();
            if (statusRes.ok && !statusRes.data.matchedRoomId.empty()) {
                auto gvMatched = _client.fetchGameView(statusRes.data.matchedRoomId);
                if (gvMatched.ok &&
                    !checkAgentSupportsVariant(OctiVariantSpec::fromGameView(gvMatched.data), mmVarReason)) {
                    logMsg(2, "Leaving unsupported matchmaking room #" + statusRes.data.matchedRoomId +
                           ": " + mmVarReason);
                    if (gvMatched.data.phase == "IN_PROGRESS") {
                        _client.resignGame(statusRes.data.matchedRoomId);
                    }
                    _client.leaveTable(statusRes.data.matchedRoomId);
                    return;
                }
                if (gvMatched.ok && gvMatched.data.phase == "IN_PROGRESS") {
                    onForceAdoptInProgressMatch(statusRes.data.matchedRoomId);
                } else {
                    std::string mmResReason;
                    if (!onBeforeCommitMatch(statusRes.data.matchedRoomId, mmResReason)) {
                        bool transitioned = false;
                        standUpFromWaitingRoomWithoutResign(statusRes.data.matchedRoomId, transitioned);
                        if (!transitioned) {
                            return;
                        }
                    }
                }
                bool mmRoomChanged = false;
                std::string mmDepartedRoom;
                {
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    if (!_currentRoomId.empty() && _currentRoomId != statusRes.data.matchedRoomId) {
                        mmDepartedRoom = _currentRoomId;
                        _chatMutedRooms.erase(_currentRoomId);
                        mmRoomChanged = true;
                    }
                    if (mmRoomChanged) {
                        _lastSeatedOpponents.clear();
                    }
                    _chatMutedRooms.erase(statusRes.data.matchedRoomId);
                    _currentRoomId = statusRes.data.matchedRoomId;
                }
                onRoomSwitched(mmRoomChanged ? mmDepartedRoom : std::string(),
                               statusRes.data.matchedRoomId);
                _client.stopGameEventStream();
                _client.startGameEventStream(statusRes.data.matchedRoomId, 0,
                    [this](const SseEvent &ev) {
                        handleGameSseEvent(ev.event, ev.dataJson);
                    });
                logMsg(2, "Matchmaking matched table #" + statusRes.data.matchedRoomId);
                notifyEvent("MATCHMAKING_MATCHED");
                wakeLoop(true, false);
                return;
            }
        }
    } else {
        // Room active
        bool gameStreamOk = _client.isGameEventStreamActive();
        bool needRoomFetch = _roomStateDirty.exchange(false) || !gameStreamOk;
        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            if (_activeGameView.roomId != roomId || _currentPhase == "IDLE" || _currentPhase.empty()) {
                needRoomFetch = true;
            } else if (_currentPhase == "IN_PROGRESS" && (now - _lastRoomFetchMs >= 2000)) {
                needRoomFetch = true;
            }
        }

        if (needRoomFetch) {
            auto gvRes = _client.fetchGameView(roomId);
            _lastRoomFetchMs = nowMs();
            if (!gvRes.ok) {
                if (gvRes.httpStatus == 404 || gvRes.httpStatus == 410) {
                    {
                        std::lock_guard<std::mutex> lock(_stateMutex);
                        _chatMutedRooms.erase(roomId);
                        if (_currentRoomId != roomId) {
                            return;
                        }
                        _currentRoomId = "";
                        _currentPhase = "IDLE";
                        _lastSeatedOpponents.clear();
                        _gameStartedHandledRoomId.clear();
                        _resumedActiveRoomId.clear();
                    }
                    onRoomClosedOrLeft(roomId);
                    onMatchReleased(roomId);
                    _client.stopGameEventStream();
                    if (!_client.isLobbyEventStreamActive()) {
                        _client.startLobbyEventStream(0,
                            [this](const SseEvent &ev) {
                                handleLobbySseEvent(ev.event, ev.dataJson);
                            });
                    }
                    wakeLoop(false, true);
                    logMsg(2, "Room #" + roomId + " closed; returned to lobby.");
                    notifyEvent("ROOM_LEFT");
                    return;
                }
                if (gvRes.httpStatus == 409 && gvRes.errorCode == "ROBOT_TOKEN_SUPERSEDED") {
                    logMsg(1, "Token superseded: another instance connected with this token.");
                    onSuperseded();
                    onMatchReleased(roomId);
                    _supersededAtMs.store(nowMs());
                    _superseded = true;
                    notifyEvent("SUPERSEDED");
                    wakeLoop(true, true);
                    return;
                }
                return;
            }

            std::string oldPhase;
            bool opponentChangedInRoom = false;
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                if (_currentRoomId != roomId) {
                    return;
                }
                oldPhase = _currentPhase;
                _activeGameView = gvRes.data;
                _activeGameViewFetchedMs = _lastRoomFetchMs;
                _currentPhase = gvRes.data.phase;

                int foundSeat = 0;
                for (const auto &seat : gvRes.data.seats) {
                    if ((!_identity.handle.empty() && seat.handle == _identity.handle) ||
                        (!_identity.robotUuid.empty() && seat.robotUuid == _identity.robotUuid)) {
                        foundSeat = seat.seat;
                        break;
                    }
                }
                _currentSeat = foundSeat;

                std::set<std::string> seatedOpps;
                for (const auto &seat : gvRes.data.seats) {
                    if (seat.seat == _currentSeat) {
                        continue;
                    }
                    std::string h = normalizeHandleOrId(seat.handle, '@');
                    if (!h.empty() && (_identity.handle.empty() || !iequalsStr(h, _identity.handle))) {
                        for (char &c : h) {
                            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        }
                        seatedOpps.insert(h);
                    }
                }
                if (!_lastSeatedOpponents.empty() && seatedOpps != _lastSeatedOpponents &&
                    (_currentPhase == "WAITING_FOR_PLAYERS" || !seatedOpps.empty())) {
                    opponentChangedInRoom = true;
                }
                _lastSeatedOpponents = std::move(seatedOpps);
            }

            if (opponentChangedInRoom ||
                (oldPhase == "IN_PROGRESS" && _currentPhase == "WAITING_FOR_PLAYERS")) {
                onOpponentChangedInRoom(roomId, gvRes.data);
            }
            onRoomStateRefreshed(roomId, gvRes.data);

            if (!oldPhase.empty() && oldPhase != _currentPhase) {
                logMsg(2, "Room #" + roomId + " phase transition: " + oldPhase + " -> " + _currentPhase);
                if (_currentPhase == "IN_PROGRESS") {
                    _gameOverHandledRoomId.clear();
                    _postGameSilenceStartMs.store(0);
                } else if (oldPhase == "IN_PROGRESS" && _currentPhase == "WAITING_FOR_PLAYERS") {
                    _lastLoggedPlyCount = -1;
                    _gameStartedHandledRoomId.clear();
                    _resumedActiveRoomId.clear();
                    onGamePhaseReset(roomId, roomId);
                    onMatchReleased(roomId);
                }
            }

            if (_currentPhase == "WAITING_FOR_PLAYERS" && _resumedActiveRoomId == roomId) {
                _resumedActiveRoomId.clear();
            }

            if (_currentPhase == "IN_PROGRESS" && _gameStartedHandledRoomId != roomId) {
                const bool isMidGameReconnect =
                    (_resumedActiveRoomId == roomId && _activeGameView.plyCount > 0);
                _gameStartedHandledRoomId = roomId;
                if (_resumedActiveRoomId == roomId) {
                    _resumedActiveRoomId.clear();
                }
                _gameOverHandledRoomId.clear();
                _postGameSilenceStartMs.store(0);

                std::string oppHandle;
                for (const auto &seat : _activeGameView.seats) {
                    if (seat.seat != _currentSeat && !seat.handle.empty()) {
                        oppHandle = seat.handle;
                        break;
                    }
                }

                std::string agentGreeting;
                if (_agent) {
                    _agent->onGameStarted(_activeGameView, _currentSeat, agentGreeting);
                }
                if (!isMidGameReconnect) {
                    bool postedAgentGreeting = false;
                    if (!agentGreeting.empty() && shouldEmitProactiveChat(roomId, "game_start")) {
                        _client.postGameChat(roomId, "ALL", agentGreeting);
                        postedAgentGreeting = true;
                    }
                    _agentEmittedGameStartGreeting = postedAgentGreeting;
                    onGameStartedHandled(roomId, _activeGameView, _currentSeat, oppHandle);
                    _agentEmittedGameStartGreeting = false;
                }
            }
        }

        if (_currentPhase == "WAITING_FOR_PLAYERS") {
            {
                OctiVariantSpec waitSpec = OctiVariantSpec::fromGameView(_activeGameView);
                std::string waitReason;
                if (!checkAgentSupportsVariant(waitSpec, waitReason)) {
                    std::string oppH;
                    bool oppRob = false;
                    for (const auto &s : _activeGameView.seats) {
                        if (s.seat != _currentSeat && !s.handle.empty() &&
                            !iequalsStr(s.handle, _identity.handle)) {
                            oppH = s.handle;
                            oppRob = s.isRobot;
                            if (!s.isRobot) break;
                        }
                    }
                    logMsg(2, "Leaving incompatible waiting room #" + roomId + ": " + waitReason);
                    if (!oppH.empty()) {
                        emitVariantDeclineMessage("game", roomId, oppH, oppRob, waitSpec, waitReason, true);
                    }
                    std::string leaveErr;
                    leaveRoomById(roomId, leaveErr);
                    clearMatchOverride();
                    return;
                }
            }
            bool hasValidSeatedOpponent = false;
            bool isHost = (_activeGameView.hostUuid == _identity.robotUuid ||
                           _activeGameView.hostUuid == _identity.ownerUuid);
            for (const auto &seat : _activeGameView.seats) {
                if (seat.seat != _currentSeat && !seat.handle.empty() &&
                    seat.handle != _identity.handle) {
                    TableSeatInfo tsi;
                    tsi.seat = seat.seat;
                    tsi.handle = seat.handle;
                    tsi.isRobot = seat.isRobot;
                    bool shouldKick = false;
                    std::string curTarget = firstTargetHandle(ov.targetPlayerHandle);
                    if (ov.active) {
                        bool isAllowedTarget = curTarget.empty() ||
                                               matchesAnyTargetHandle(ov.targetPlayerHandle, seat.handle) ||
                                               (ov.allowRobots && seat.isRobot);
                        if (!isAllowedTarget) {
                            shouldKick = true;
                        } else if (!ov.allowRobots && seat.isRobot &&
                                   (curTarget.empty() || !matchesAnyTargetHandle(ov.targetPlayerHandle, seat.handle))) {
                            shouldKick = true;
                        }
                    } else if (explicitAllowRobotsForRoom && seat.isRobot && !_policy.isBanned(seat.handle)) {
                        shouldKick = false;
                    } else {
                        shouldKick = _policy.shouldKickSeatedPlayer(
                            tsi, _identity.ownerHandle, _activeGameView.isTournamentRoom);
                    }
                    if (isHost && shouldKick) {
                        if (needRoomFetch) {
                            _client.kickSeat(roomId, seat.seat);
                        }
                    } else {
                        hasValidSeatedOpponent = true;
                    }
                }
            }

            // Waiting-room resource admission & non-resigning eviction
            const std::string ovTarget = firstTargetHandle(ov.targetPlayerHandle);
            const bool overrideTargetsDifferentRoom =
                ov.active && !hasValidSeatedOpponent &&
                ((!ov.targetRoomId.empty() && ov.targetRoomId != roomId) ||
                 (ov.targetRoomId.empty() && !ovTarget.empty()));
            if (shouldStandUpAndUnhostWaitingRoom(roomId)) {
                if (hasValidSeatedOpponent) {
                    std::string oppH;
                    bool oppRob = false;
                    for (const auto &s : _activeGameView.seats) {
                        if (s.seat != _currentSeat && !s.handle.empty() &&
                            !iequalsStr(s.handle, _identity.handle)) {
                            oppH = s.handle;
                            oppRob = s.isRobot;
                            if (!s.isRobot) break;
                        }
                    }
                    if (!oppH.empty()) {
                        emitResourceMaxedDeclineMessage("game", roomId, oppH, oppRob);
                    }
                }
                bool transitionedToInProgress = false;
                standUpFromWaitingRoomWithoutResign(roomId, transitionedToInProgress);
                if (!transitionedToInProgress && !overrideTargetsDifferentRoom) {
                    clearMatchOverride();
                }
                return;
            }
            if (hasValidSeatedOpponent || (ov.active && !overrideTargetsDifferentRoom)) {
                std::string commitReason;
                if (!onBeforeCommitMatch(roomId, commitReason)) {
                    std::string oppH;
                    bool oppRob = false;
                    for (const auto &s : _activeGameView.seats) {
                        if (s.seat != _currentSeat && !s.handle.empty() &&
                            !iequalsStr(s.handle, _identity.handle)) {
                            oppH = s.handle;
                            oppRob = s.isRobot;
                            if (!s.isRobot) break;
                        }
                    }
                    if (!oppH.empty()) {
                        emitResourceMaxedDeclineMessage("game", roomId, oppH, oppRob);
                    }
                    bool transitionedToInProgress = false;
                    standUpFromWaitingRoomWithoutResign(roomId, transitionedToInProgress);
                    if (!transitionedToInProgress && !overrideTargetsDifferentRoom) {
                        clearMatchOverride();
                    }
                    return;
                }
            }
            bool forceLobbyCheckNow = false;
            if (!hasValidSeatedOpponent && !_activeGameView.isTournamentRoom &&
                _policy.acceptTournamentMatches() &&
                _pendingTournamentPreemptCheck.exchange(false)) {
                forceLobbyCheckNow = true;
                std::string pendingMatchRoom;
                {
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    pendingMatchRoom = _pendingMatchFoundRoomId;
                    _pendingMatchFoundRoomId.clear();
                }
                if (!pendingMatchRoom.empty() && pendingMatchRoom != roomId) {
                    auto gvMatch = _client.fetchGameView(pendingMatchRoom);
                    if (gvMatch.ok && gvMatch.data.isTournamentRoom &&
                        (gvMatch.data.phase == "IN_PROGRESS" ||
                         gvMatch.data.phase == "WAITING_FOR_PLAYERS")) {
                        bool selfInMatch = false;
                        for (const auto &s : gvMatch.data.seats) {
                            if ((!_identity.handle.empty() && iequalsStr(s.handle, _identity.handle)) ||
                                (!_identity.robotUuid.empty() && s.robotUuid == _identity.robotUuid)) {
                                selfInMatch = true;
                                break;
                            }
                        }
                        if (selfInMatch) {
                            logMsg(2, "Preempting casual waiting room #" + roomId +
                                   " for assigned tournament match #" + pendingMatchRoom);
                            if (isHost) {
                                _client.purgeTable(roomId);
                            }
                            bool transitioned = false;
                            standUpFromWaitingRoomWithoutResign(roomId, transitioned);
                            if (transitioned) {
                                return;
                            }
                            int joinedSeat = 0;
                            std::string joinErr;
                            const bool inProg = (gvMatch.data.phase == "IN_PROGRESS");
                            if (joinRoomByIdImpl(pendingMatchRoom, 0, joinedSeat, joinErr,
                                                 inProg, !inProg, nullptr)) {
                                return;
                            }
                        }
                    }
                }
            }

            const bool lobbyDirtyOrUnstreamed =
                _lobbyTablesDirty.load() || !_client.isLobbyEventStreamActive() || forceLobbyCheckNow;
            const bool throttleElapsed =
                forceLobbyCheckNow || ov.active || (now - _lastLobbyTablesFetchMs >= 2000);
            if (!hasValidSeatedOpponent && lobbyDirtyOrUnstreamed && throttleElapsed) {
                _lobbyTablesDirty.store(false);
                auto tablesRes = _client.listTables(false);
                _lastLobbyTablesFetchMs = nowMs();
                if (tablesRes.ok) {
                    if (!_activeGameView.isTournamentRoom && _policy.acceptTournamentMatches()) {
                        for (const auto &t : tablesRes.data) {
                            if (t.roomId == roomId || !t.isTournamentRoom ||
                                (t.phase != "IN_PROGRESS" && t.phase != "WAITING_FOR_PLAYERS")) {
                                continue;
                            }
                            bool selfInTourney = false;
                            for (const auto &s : t.seats) {
                                if ((!_identity.handle.empty() && iequalsStr(s.handle, _identity.handle)) ||
                                    (!_identity.robotUuid.empty() && s.robotUuid == _identity.robotUuid)) {
                                    selfInTourney = true;
                                    break;
                                }
                            }
                            if (!selfInTourney) {
                                continue;
                            }
                            logMsg(2, "Preempting casual waiting room #" + roomId +
                                   " for tournament room #" + t.roomId);
                            if (isHost) {
                                _client.purgeTable(roomId);
                            }
                            bool transitioned = false;
                            standUpFromWaitingRoomWithoutResign(roomId, transitioned);
                            if (transitioned) {
                                return;
                            }
                            int joinedSeat = 0;
                            std::string joinErr;
                            const bool inProg = (t.phase == "IN_PROGRESS");
                            if (joinRoomByIdImpl(t.roomId, 0, joinedSeat, joinErr,
                                                 inProg, !inProg, nullptr)) {
                                return;
                            }
                        }
                    }
                    std::string curTarget = firstTargetHandle(ov.targetPlayerHandle);
                    if (ov.active && !curTarget.empty()) {
                        for (const auto &t : tablesRes.data) {
                            if (t.roomId != roomId && t.phase == "WAITING_FOR_PLAYERS" &&
                                t.seatsFilled < t.seatsTotal && t.allowRobots) {
                                bool hasTarget = iequalsStr(t.hostHandle, curTarget);
                                bool targetIsRobot = isKnownRobotHandle(curTarget);
                                for (const auto &s : t.seats) {
                                    if (iequalsStr(s.handle, curTarget)) {
                                        hasTarget = true;
                                        targetIsRobot = s.isRobot;
                                        break;
                                    }
                                }
                                if (hasTarget && (!targetIsRobot || !_activeGameView.allowRobots || roomId > t.roomId)) {
                                    OctiVariantSpec hopSpec = OctiVariantSpec::fromTable(t);
                                    if (!matchesExplicitOverrideVariant(hopSpec, ov)) {
                                        continue;
                                    }
                                    std::string hopReason;
                                    if (!checkAgentSupportsVariant(hopSpec, hopReason)) {
                                        std::string reqH;
                                        bool reqRob = targetIsRobot;
                                        {
                                            std::lock_guard<std::mutex> lock(_stateMutex);
                                            reqH = _matchOverrideRequesterHandle.empty()
                                                ? curTarget
                                                : _matchOverrideRequesterHandle;
                                            if (!_matchOverrideRequesterHandle.empty()) {
                                                reqRob = _matchOverrideRequesterIsRobot;
                                            }
                                        }
                                        clearMatchOverride();
                                        emitVariantDeclineMessage("lobby", "", reqH, reqRob, hopSpec, hopReason);
                                        return;
                                    }
                                    std::string hopResReason;
                                    if (!canCommitToMatch(t.roomId, "hop_join", hopResReason)) {
                                        std::string reqH;
                                        bool reqRob = targetIsRobot;
                                        {
                                            std::lock_guard<std::mutex> lock(_stateMutex);
                                            reqH = _matchOverrideRequesterHandle.empty()
                                                ? curTarget
                                                : _matchOverrideRequesterHandle;
                                            if (!_matchOverrideRequesterHandle.empty()) {
                                                reqRob = _matchOverrideRequesterIsRobot;
                                            }
                                        }
                                        clearMatchOverride();
                                        emitResourceMaxedDeclineMessage("lobby", "", reqH, reqRob);
                                        return;
                                    }
                                    std::string dummyErr;
                                    leaveRoomById(roomId, dummyErr);
                                    int joinedSeat = 0;
                                    if (joinRoomByIdImpl(t.roomId, 0, joinedSeat, dummyErr, false, true, nullptr)) {
                                        {
                                            std::lock_guard<std::mutex> lock(_stateMutex);
                                            _matchOverrideJoinGraceUntilMs = 0;
                                        }
                                        _client.postGameChat(
                                            t.roomId, "ALL",
                                            "@" + curTarget +
                                            " Joined your table #" + t.roomId + " for our match!");
                                        _client.postLobbyChat(
                                            "On my way! Joined @" + curTarget + " in table #" + t.roomId + ".");
                                    }
                                    return;
                                }
                            }
                        }
                    } else if (!ov.active && tryHandleIdleWaitingRoomTables(roomId, tablesRes.data)) {
                        return;
                    }
                }
            }
            if (_currentSeat == 0 && hasValidSeatedOpponent && currentRoomId() == roomId) {
                auto joinRes = _client.joinTable(roomId, 0);
                if (joinRes.ok) {
                    {
                        std::lock_guard<std::mutex> lock(_stateMutex);
                        _currentSeat = joinRes.data.seat;
                    }
                    wakeLoop(true, false);
                    logMsg(2, "Joined remaining seat " + std::to_string(joinRes.data.seat) +
                           " in hosted table #" + roomId);
                    notifyEvent("ROOM_JOINED");
                }
            }
        } else if (_currentPhase == "IN_PROGRESS") {
            _lobbyTablesDirty.store(false);
            onForceAdoptInProgressMatch(roomId);
            {
                OctiVariantSpec activeSpec = OctiVariantSpec::fromGameView(_activeGameView);
                std::string activeVarReason;
                if (!checkAgentSupportsVariant(activeSpec, activeVarReason)) {
                    std::string oppH;
                    bool oppRob = false;
                    for (const auto &s : _activeGameView.seats) {
                        if (s.seat != _currentSeat && !s.handle.empty() &&
                            !iequalsStr(s.handle, _identity.handle)) {
                            oppH = s.handle;
                            oppRob = s.isRobot;
                            if (!s.isRobot) break;
                        }
                    }
                    logMsg(1, "Seated in unsupported IN_PROGRESS room #" + roomId +
                           " (" + activeVarReason + "); resigning and leaving.");
                    emitVariantDeclineMessage("game", roomId, oppH, oppRob,
                                              activeSpec, activeVarReason, true);
                    std::string leaveErr;
                    leaveRoomById(roomId, leaveErr);
                    clearMatchOverride();
                    return;
                }
            }
            bool allowRobotsNow =
                (_activeGameView.isTournamentRoom && _policy.acceptTournamentMatches()) ||
                explicitAllowRobotsForRoom ||
                (ov.active && (ov.allowRobots || !ov.targetPlayerHandle.empty() || !ov.targetRoomId.empty()));
            if (_policy.opponentFilter() == "humans_only" && !allowRobotsNow) {
                bool hasHumanOpponent = false;
                bool hasRobotOpponent = false;
                for (const auto &s : _activeGameView.seats) {
                    if (s.seat != _currentSeat && !s.handle.empty()) {
                        if (s.isRobot) {
                            hasRobotOpponent = true;
                        } else {
                            hasHumanOpponent = true;
                        }
                    }
                }
                if (hasRobotOpponent && !hasHumanOpponent) {
                    logMsg(2, "Aborting robot-vs-robot game in room #" + roomId + " (opponent_filter=humans_only).");
                    _client.resignGame(roomId);
                    std::string leaveErr;
                    leaveRoomById(roomId, leaveErr);
                    return;
                }
            }
            if (_activeGameView.plyCount != _lastLoggedPlyCount) {
                _lastLoggedPlyCount = _activeGameView.plyCount;
                std::string latestMove = _activeGameView.moves.empty() ? "(none)" : _activeGameView.moves.back();
                std::string turnStr = "Seat " + std::to_string(_activeGameView.turnSeat);
                for (const auto &s : _activeGameView.seats) {
                    if (s.seat == _activeGameView.turnSeat && !s.handle.empty()) {
                        turnStr += " (@" + s.handle + ")";
                        break;
                    }
                }
                std::string clocksStr = "[";
                for (size_t si = 0; si < _activeGameView.seats.size(); ++si) {
                    if (si > 0) clocksStr += ", ";
                    clocksStr += "S" + std::to_string(_activeGameView.seats[si].seat) + ": " +
                                 std::to_string(_activeGameView.seats[si].clockMs) + "ms";
                }
                clocksStr += "]";
                std::string ttlStr = "";
                if (_activeGameView.ttlRemainingMs >= 0) {
                    int64_t sec = _activeGameView.ttlRemainingMs / 1000;
                    ttlStr = " | TTL: " + std::to_string(_activeGameView.ttlRemainingMs) + "ms (" +
                             std::to_string(sec / 60) + ":" + (sec % 60 < 10 ? "0" : "") + std::to_string(sec % 60) + ")";
                }
                logMsg(3, "[RoomState] Room #" + roomId + " | Ply: " + std::to_string(_activeGameView.plyCount) +
                       " | Latest move: " + latestMove + " | Turn: " + turnStr + " | Clocks: " + clocksStr + ttlStr);
            }
            if (_activeGameView.turnSeat == _currentSeat) {
                playEngineTurn();
            }
        } else if (_currentPhase == "FINISHED" || _currentPhase == "GAME_OVER" || _currentPhase == "ABANDONED") {
            _lobbyTablesDirty.store(false);
            bool hasConnectedOpponent = false;
            bool oppIsRobot = false;
            std::string oppHandle;
            for (const auto &seat : _activeGameView.seats) {
                if (seat.seat != _currentSeat && !seat.handle.empty() && seat.connected) {
                    hasConnectedOpponent = true;
                    oppHandle = seat.handle;
                    oppIsRobot = seat.isRobot;
                    break;
                }
            }

            // 1. First time seeing game over for this roomId
            if (_gameOverHandledRoomId != roomId) {
                _gameOverHandledRoomId = roomId;
                _gameStartedHandledRoomId.clear();
                _resumedActiveRoomId.clear();
                _postGameSilenceStartMs.store(nowMs());
                onMatchReleased(roomId);
                notifyEvent("GAME_OVER");

                bool overrideExpired = false;
                {
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    if (_matchOverride.active) {
                        _matchOverride.remainingGames--;
                        if (_matchOverride.remainingGames <= 0) {
                            _matchOverride = BotMatchOverride();
                            _matchOverrideJoinGraceUntilMs = 0;
                            overrideExpired = true;
                        } else if (_matchOverride.targetPlayerHandle.find(',') != std::string::npos) {
                            _matchOverride.targetPlayerHandle =
                                popFirstTargetHandle(_matchOverride.targetPlayerHandle);
                        }
                    }
                }
                if (overrideExpired) {
                    logMsg(2, "[MatchOverride] Completed requested match(es); reverting to normal policy.");
                }

                std::string result = "DRAW";
                if (_activeGameView.winnerSeat == _currentSeat && _currentSeat > 0) {
                    result = "WIN";
                    _identity.wins++;
                    _identity.gamesPlayed++;
                } else if (_activeGameView.winnerSeat > 0) {
                    result = "LOSS";
                    _identity.losses++;
                    _identity.gamesPlayed++;
                } else if (_currentPhase != "ABANDONED") {
                    _identity.draws++;
                    _identity.gamesPlayed++;
                }

                if (_agent) {
                    _agent->onGameOver(_activeGameView, _currentSeat);
                }

                onGameOverHandled(roomId, _activeGameView, _currentSeat,
                                  result, hasConnectedOpponent, oppHandle);

                if (overrideExpired && oppIsRobot && _policy.opponentFilter() == "humans_only") {
                    std::string leaveErr;
                    leaveRoomById(roomId, leaveErr);
                    return;
                }

                const int tourneyLingerCfg = std::max(0, _policy.tournamentPostGameLingerSec());
                if (hasConnectedOpponent &&
                    !(_activeGameView.isTournamentRoom && tourneyLingerCfg == 0)) {
                    return;
                }
            }

            // 2. Subsequent ticks while finished
            if (!hasConnectedOpponent) {
                _client.stopGameEventStream();
                _client.leaveTable(roomId);
                {
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    _chatMutedRooms.erase(roomId);
                    _currentRoomId = "";
                    _currentSeat = 0;
                    _currentPhase = "IDLE";
                    _activeGameView = GameViewData();
                    _activeGameViewFetchedMs = 0;
                    _lastSeatedOpponents.clear();
                    _gameOverHandledRoomId = "";
                    _gameStartedHandledRoomId.clear();
                    _resumedActiveRoomId.clear();
                    _lastLoggedPlyCount = -1;
                    _postGameSilenceStartMs.store(0);
                }
                onRoomClosedOrLeft(roomId);
                onMatchReleased(roomId);
                if (!_client.isLobbyEventStreamActive()) {
                    std::uint64_t resumeSeq = 0;
                    {
                        std::lock_guard<std::mutex> lock(_stateMutex);
                        if (_lastProcessedLobbyChatSeq > 0) {
                            resumeSeq = static_cast<std::uint64_t>(_lastProcessedLobbyChatSeq);
                        }
                        _lastLobbyStreamRestartMs = nowMs();
                    }
                    _client.startLobbyEventStream(resumeSeq,
                        [this](const SseEvent &ev) {
                            handleLobbySseEvent(ev.event, ev.dataJson);
                        });
                }
                wakeLoop(false, true);
                logMsg(2, "Opponent left finished room #" + roomId + "; leaving table and returning to lobby.");
                notifyEvent("ROOM_LEFT");
                return;
            }

            if (isPostGameLingerBlocked()) {
                return;
            }

            int lingerSec = _activeGameView.isTournamentRoom
                ? std::max(0, _policy.tournamentPostGameLingerSec())
                : std::max(5, _config.getInt("post_game_linger_sec", 60));
            std::int64_t silentMs = nowMs() - _postGameSilenceStartMs.load();
            if (silentMs >= static_cast<std::int64_t>(lingerSec) * 1000) {
                onPostGameLingerElapsed(roomId, oppHandle, lingerSec);

                _client.stopGameEventStream();
                _client.leaveTable(roomId);
                {
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    _chatMutedRooms.erase(roomId);
                    _currentRoomId = "";
                    _currentSeat = 0;
                    _currentPhase = "IDLE";
                    _activeGameView = GameViewData();
                    _activeGameViewFetchedMs = 0;
                    _lastSeatedOpponents.clear();
                    _gameOverHandledRoomId = "";
                    _gameStartedHandledRoomId.clear();
                    _resumedActiveRoomId.clear();
                    _lastLoggedPlyCount = -1;
                    _postGameSilenceStartMs.store(0);
                }
                onRoomClosedOrLeft(roomId);
                onMatchReleased(roomId);
                if (!_client.isLobbyEventStreamActive()) {
                    std::uint64_t resumeSeq = 0;
                    {
                        std::lock_guard<std::mutex> lock(_stateMutex);
                        if (_lastProcessedLobbyChatSeq > 0) {
                            resumeSeq = static_cast<std::uint64_t>(_lastProcessedLobbyChatSeq);
                        }
                        _lastLobbyStreamRestartMs = nowMs();
                    }
                    _client.startLobbyEventStream(resumeSeq,
                        [this](const SseEvent &ev) {
                            handleLobbySseEvent(ev.event, ev.dataJson);
                        });
                }
                wakeLoop(false, true);
                logMsg(2, "Post-game linger (" + std::to_string(lingerSec) + "s silence) elapsed in room #" +
                       roomId + "; said goodbye, left table, and returned to lobby.");
                notifyEvent("ROOM_LEFT");
            }
        }
    }
}

bool OctiBotRunner::tryHandleIdleWaitingRoomTables(
    const std::string &currentRoomId,
    const std::vector<TableSummaryInfo> &tables) {
    (void) currentRoomId;
    (void) tables;
    return false;
}

bool OctiBotRunner::tryComputeExternalMove(const OctiBoard &board,
                                           const GameViewData &view,
                                           int mySeat,
                                           std::string &outMove,
                                           bool &outAborted) {
    (void) board;
    (void) view;
    (void) mySeat;
    outMove.clear();
    outAborted = false;
    return false;
}

void OctiBotRunner::onSearchTurnBegin(const std::string &roomId,
                                      const GameViewData &view,
                                      int mySeat) {
    (void) roomId;
    (void) view;
    (void) mySeat;
}

void OctiBotRunner::onSearchTurnEnd(const BotMoveTelemetry &telem) {
    (void) telem;
}

void OctiBotRunner::onMoveSubmitted(const std::string &roomId,
                                    const GameViewData &view,
                                    int mySeat,
                                    const std::string &move,
                                    const BotMoveTelemetry &telem,
                                    bool hasPrevEval,
                                    int prevEvalScore) {
    (void) roomId;
    (void) view;
    (void) mySeat;
    (void) move;
    (void) telem;
    (void) hasPrevEval;
    (void) prevEvalScore;
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
    GameViewData gameViewSnap;
    std::int64_t gameViewFetchedMs = 0;

    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        roomId = _currentRoomId;
        mySeat = _currentSeat;
        gameViewSnap = _activeGameView;
        gameViewFetchedMs = _activeGameViewFetchedMs;
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
        if (!_activeBoard ||
            _activeBoard->getGameMode() != m ||
            !_activeBoard->getGame() ||
            _activeBoard->getGame()->getBases() != b ||
            _activeBoard->getGame()->useSuperProngs() != superProngs ||
            _activeBoard->getGame()->useEdgelessBoard() != edgeless) {
            _activeBoard = std::make_unique<OctiBoard>(m, superProngs, edgeless, b);
        }
        if (!fen.empty()) {
            _activeBoard->fromPositionString(fen);
            if (_activeBoard->getGame()) {
                _activeBoard->getGame()->setNoProgressPlies(_activeGameView.noProgressPlies);
            }
        }
    }

    auto resignAndLeaveUnsupportedOrEmptyMove = [&](const std::string &reason) {
        std::string oppH;
        bool oppRob = false;
        for (const auto &s : gameViewSnap.seats) {
            if (s.seat != mySeat && !s.handle.empty() &&
                !iequalsStr(s.handle, _identity.handle)) {
                oppH = s.handle;
                oppRob = s.isRobot;
                if (!s.isRobot) break;
            }
        }
        OctiVariantSpec turnSpec = OctiVariantSpec::fromGameView(gameViewSnap);
        emitVariantDeclineMessage("game", roomId, oppH, oppRob, turnSpec, reason, true);
        std::string leaveErr;
        leaveRoomById(roomId, leaveErr);
        clearMatchOverride();
    };

    {
        OctiVariantSpec turnSpec = OctiVariantSpec::fromGameView(gameViewSnap);
        std::string turnVarReason;
        if (!checkAgentSupportsVariant(turnSpec, turnVarReason)) {
            logMsg(1, "Refusing engine turn in #" + roomId + ": " + turnVarReason);
            resignAndLeaveUnsupportedOrEmptyMove(turnVarReason);
            return;
        }
    }

    bool gameAlreadyOver = (_activeBoard && _activeBoard->getGame())
        ? _activeBoard->getGame()->isGameOver()
        : false;
    size_t legalCount = (_activeBoard && _activeBoard->getGame() && !gameAlreadyOver)
        ? _activeBoard->getGame()->generateLegalMoves().size()
        : 0;
    logMsg(3, "Turn start: seat=" + std::to_string(mySeat) + ", clock=" + std::to_string(clockMs) +
           "ms, legal_moves=" + std::to_string(legalCount) + ", fen=" + fen);

    if (_policy.minMoveThinkMs() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(_policy.minMoveThinkMs()));
    }

    const std::int64_t baseTurnElapsedMs =
        std::max<std::int64_t>(0, gameViewSnap.turnElapsedMs);
    auto refreshTurnClockSnapshot = [&]() {
        const std::int64_t turnNowMs = nowMs();
        gameViewSnap.turnElapsedMs = baseTurnElapsedMs;
        clockMs = computeEffectiveRemainingClockMs(
            gameViewSnap, mySeat, gameViewFetchedMs, turnNowMs);
        if (gameViewSnap.timeControlEnabled &&
            gameViewSnap.phase == "IN_PROGRESS" &&
            !gameViewSnap.paused &&
            gameViewSnap.turnSeat == mySeat &&
            gameViewFetchedMs > 0 &&
            turnNowMs > gameViewFetchedMs) {
            gameViewSnap.turnElapsedMs =
                baseTurnElapsedMs + (turnNowMs - gameViewFetchedMs);
        }
    };
    refreshTurnClockSnapshot();

    std::string chosenMove;
    bool extAborted = false;
    if (tryComputeExternalMove(*_activeBoard, gameViewSnap, mySeat, chosenMove, extAborted)) {
        if (extAborted) {
            return;
        }
    }

    // Fallback to engine search if no agent move was submitted
    if (chosenMove.empty() && _agent) {
        logMsg(3, "Computing move for seat " + std::to_string(mySeat) + " (budget=" +
               std::to_string(_config.getInt("time_budget_ms", 1500)) + "ms, max_depth=" +
               std::to_string(_config.getInt("max_search_depth", 6)) + ")...");
        if (auto *stockAgent = dynamic_cast<StockAlphaBetaAgent *>(_agent.get())) {
            stockAgent->setProgressCallback([this](int depth, std::uint64_t nodes, int score) {
                updateSearchProgress(true, depth, nodes, score);
                if (depth > 0) {
                    std::string line = "  [Search progress] depth=" + std::to_string(depth) +
                                       " | nodes=" + std::to_string(nodes) +
                                       " | live_score=" + std::to_string(score);
                    appendStdoutLine(line, true);
                }
            });
        }
        _lastTelemetry = BotMoveTelemetry{};
        updateSearchProgress(true, 0, 0, 0);
        onSearchTurnBegin(roomId, gameViewSnap, mySeat);
        struct SearchTurnProgressGuard {
            OctiBotRunner &runner;
            explicit SearchTurnProgressGuard(OctiBotRunner &r) : runner(r) {}
            ~SearchTurnProgressGuard() {
                runner.onSearchTurnEnd(runner._lastTelemetry);
                runner.updateSearchProgress(
                    false,
                    runner._lastTelemetry.depthReached,
                    runner._lastTelemetry.nodesVisited,
                    runner._lastTelemetry.evalScore);
            }
        } progressGuard{*this};
        onBeforeComputeMove();
        refreshTurnClockSnapshot();
        chosenMove = _agent->computeMove(*_activeBoard, gameViewSnap, mySeat, clockMs, &_lastTelemetry);
    }

    if (chosenMove.empty() && !gameAlreadyOver && legalCount > 0) {
        logMsg(1, "Agent computeMove returned empty move in active room #" + roomId +
               " with " + std::to_string(legalCount) + " legal move(s); resigning and leaving to prevent spin.");
        resignAndLeaveUnsupportedOrEmptyMove("");
        return;
    }

    if (!chosenMove.empty()) {
        logMsg(2, "Move selected: " + chosenMove + " | depth=" + std::to_string(_lastTelemetry.depthReached) +
               " | nodes=" + std::to_string(_lastTelemetry.nodesVisited) +
               " | eval_score=" + std::to_string(_lastTelemetry.evalScore) +
               " | elapsed=" + std::to_string(_lastTelemetry.elapsedMs) + "ms");
        bool ok = false;
        std::string moveErr;
        if (_publishMoveTelemetry && _lastTelemetry.present) {
            MoveTelemetryData telemetry;
            telemetry.present = true;
            telemetry.depth = _lastTelemetry.depthReached;
            telemetry.nodes = _lastTelemetry.nodesVisited;
            telemetry.score = _lastTelemetry.evalScore;
            telemetry.elapsedMs = _lastTelemetry.elapsedMs;
            auto res = _client.submitMove(roomId, chosenMove, telemetry);
            ok = res.ok;
            moveErr = res.errorMessage;
        } else {
            auto res = _client.submitMove(roomId, chosenMove);
            ok = res.ok;
            moveErr = res.errorMessage;
        }
        if (ok) {
            wakeLoop(true, false);
            logMsg(3, "Move " + chosenMove + " accepted by server in room #" + roomId);
            bool hasPrevEval = false;
            int prevEvalScore = 0;
            {
                std::lock_guard<std::mutex> lock(_historyMutex);
                if (!_moveHistory.empty() && _moveHistory.back().ply < _activeGameView.plyCount) {
                    hasPrevEval = true;
                    prevEvalScore = _moveHistory.back().evalScore;
                }
            }

            if (_lastTelemetry.present) {
                recordMoveHistory(_activeGameView.plyCount, chosenMove, _lastTelemetry);
                if (_announceMoveSearchChat && shouldEmitProactiveChat(roomId, "move_search_telemetry")) {
                    std::string searchChat = "depth=" + std::to_string(_lastTelemetry.depthReached) +
                        " | nodes=" + std::to_string(_lastTelemetry.nodesVisited) +
                        " | eval_score=" + std::to_string(_lastTelemetry.evalScore) +
                        " | elapsed=" + std::to_string(_lastTelemetry.elapsedMs) + "ms";
                    _client.postGameChat(roomId, "ALL", searchChat);
                }
            }

            onMoveSubmitted(roomId, gameViewSnap, mySeat, chosenMove,
                            _lastTelemetry, hasPrevEval, prevEvalScore);
        } else {
            logMsg(1, "Move submission failed in room #" + roomId + ": " + moveErr);
        }
        notifyEvent("MOVE_SUBMITTED");
    }
}

bool OctiBotRunner::emitVariantDeclineMessage(const std::string &channel,
                                              const std::string &roomId,
                                              const std::string &senderHandle,
                                              bool senderIsRobot,
                                              const OctiVariantSpec &spec,
                                              const std::string &agentReason,
                                              bool waitForCompletion) {
    (void) waitForCompletion;
    OctiVariantSpec normSpec = spec;
    std::string dummyNormErr;
    if (!normalizeStrictVariantSpec(spec, normSpec, dummyNormErr)) {
        normSpec = spec;
    }
    const std::string label = formatVariantLabel(normSpec);
    if (!tryAcquireVariantDeclineCooldown(channel, roomId, senderHandle, label)) {
        return false;
    }
    const std::string normSender = normalizeHandleOrId(senderHandle, '@');

    const std::string myHandle = _identity.handle.empty()
        ? _config.getString("robot_name", "bot")
        : _identity.handle;
    std::string fallbackText;
    if (!senderIsRobot && !normSender.empty()) {
        fallbackText = "@" + normSender + " ";
    }
    fallbackText += "Sorry, @" + myHandle + " is not configured to play " + label + ".";
    if (!agentReason.empty()) {
        fallbackText += " " + sanitizeDeclineTextNoRoomIds(agentReason);
    }
    fallbackText = sanitizeDeclineTextNoRoomIds(fallbackText);
    if (channel == "game") {
        if (!roomId.empty()) {
            const bool posted = _client.postGameChat(roomId, "ALL", fallbackText).ok;
            resetPostGameSilenceTimer();
            return posted;
        }
        return false;
    }
    return _client.postLobbyChat(fallbackText).ok;
}

bool OctiBotRunner::emitResourceMaxedDeclineMessage(const std::string &channel,
                                                    const std::string &roomId,
                                                    const std::string &senderHandle,
                                                    bool senderIsRobot) {
    if (!tryAcquireResourceDeclineCooldown(channel, roomId, senderHandle)) {
        return false;
    }
    const std::string normSender = normalizeHandleOrId(senderHandle, '@');
    const std::string normRoom = normalizeHandleOrId(roomId, '#');

    std::string textToPost;
    if (!senderIsRobot && !normSender.empty()) {
        textToPost = "@" + normSender + " ";
    }
    textToPost += resourceMaxedDeclineText();

    bool posted = false;
    if (channel == "game") {
        if (!normRoom.empty()) {
            posted = _client.postGameChat(normRoom, "ALL", textToPost).ok;
            resetPostGameSilenceTimer();
        }
    } else {
        posted = _client.postLobbyChat(textToPost).ok;
    }
    return posted;
}

bool OctiBotRunner::onLobbyChatReceived(const LobbyChatEntry &msg,
                                        bool isOwner,
                                        const std::string &senderOwnerUuid,
                                        const std::string &senderOwnerHandle) {
    (void) msg;
    (void) isOwner;
    (void) senderOwnerUuid;
    (void) senderOwnerHandle;
    return false;
}

bool OctiBotRunner::onGameChatReceived(const std::string &roomId,
                                       const std::string &senderHandle,
                                       const std::string &senderUuid,
                                       const std::string &text,
                                       bool isOwner,
                                       bool senderIsRobot,
                                       const std::string &senderOwnerUuid,
                                       const std::string &senderOwnerHandle) {
    (void) roomId;
    (void) senderHandle;
    (void) senderUuid;
    (void) text;
    (void) isOwner;
    (void) senderIsRobot;
    (void) senderOwnerUuid;
    (void) senderOwnerHandle;
    return false;
}

void OctiBotRunner::handleLobbySseEvent(const std::string &eventType, const std::string &data) {
    if (eventType != "LOBBY_CHAT") {
        logMsg(3, "[SSE] event=" + eventType + " data=" + data);
    }

    if (eventType == "closed") {
        if (data.find("ROBOT_TOKEN_SUPERSEDED") != std::string::npos) {
            logMsg(1, "Token superseded via lobby SSE: another instance connected with this token.");
            onSuperseded();
            onMatchReleased("");
            _supersededAtMs.store(nowMs());
            _superseded = true;
            notifyEvent("SUPERSEDED");
            wakeLoop(true, true);
            return;
        }
        wakeLoop(false, true);
        return;
    }

    if (eventType == "TABLE_UPDATED" || eventType == "TABLE_CLOSED" || eventType == "MATCH_FOUND") {
        if (eventType == "MATCH_FOUND") {
            try {
                auto j = nlohmann::json::parse(data);
                std::string evRoom = normalizeHandleOrId(j.value("room_id", ""), '#');
                std::string evHandle = j.value("handle", "");
                std::string evRobotUuid = j.value("robot_uuid", "");
                const bool forSelf =
                    (evHandle.empty() && evRobotUuid.empty()) ||
                    (!_identity.handle.empty() && iequalsStr(evHandle, _identity.handle)) ||
                    (!_identity.robotUuid.empty() && evRobotUuid == _identity.robotUuid);
                if (forSelf) {
                    if (!evRoom.empty()) {
                        std::lock_guard<std::mutex> lock(_stateMutex);
                        _pendingMatchFoundRoomId = evRoom;
                    }
                    _pendingTournamentPreemptCheck.store(true);
                }
            } catch (...) {
                _pendingTournamentPreemptCheck.store(true);
            }
            wakeLoop(true, true);
            return;
        }
        std::string curRoom = currentRoomId();
        if (curRoom.empty()) {
            wakeLoop(false, true);
        } else {
            try {
                auto j = nlohmann::json::parse(data);
                std::string evRoom = j.value("room_id", "");
                if (evRoom.empty() || evRoom != curRoom) {
                    wakeLoop(false, true);
                }
            } catch (...) {
                wakeLoop(false, true);
            }
        }
        return;
    }

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
            std::string senderOwnerUuid = j.value("owner_uuid", "");
            std::string senderOwnerHandle = j.value("owner_handle", "");

            if (msg.seq > 0) {
                std::lock_guard<std::mutex> lock(_stateMutex);
                if (msg.seq <= _lastProcessedLobbyChatSeq) {
                    return;
                }
                _lastProcessedLobbyChatSeq = msg.seq;
            }

            logMsg(3, "[SSE] event=" + eventType + " data=" + data);

            if (msg.handle == _identity.handle) {
                return;
            }

            bool isOwner = !msg.isRobot &&
                ((!msg.playerUuid.empty() && !_identity.ownerUuid.empty() && msg.playerUuid == _identity.ownerUuid) ||
                 (!msg.handle.empty() && !_identity.ownerHandle.empty() && msg.handle == _identity.ownerHandle));

            if (onLobbyChatReceived(msg, isOwner, senderOwnerUuid, senderOwnerHandle)) {
                return;
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
    logMsg(3, "[SSE] event=" + eventType + " data=" + data);

    if (eventType == "BOARD_CHAT" || eventType == "CHAT") {
        try {
            auto j = nlohmann::json::parse(data);
            std::string senderHandle = j.value("handle", "");
            std::string senderUuid = j.value("player_uuid", "");
            std::string text = j.value("text", "");
            bool isRobot = j.value("is_robot", false);
            std::string senderOwnerUuid = j.value("owner_uuid", "");
            std::string senderOwnerHandle = j.value("owner_handle", "");

            if (!senderHandle.empty() && !_identity.handle.empty() &&
                iequalsStr(senderHandle, _identity.handle)) {
                return;
            }

            resetPostGameSilenceTimer();

            std::string curRoom = currentRoomId();
            if (curRoom.empty()) {
                curRoom = normalizeHandleOrId(j.value("room_id", ""), '#');
            }

            if (!isRobot && !curRoom.empty() && _client.isAuthenticated()) {
                bool needViewRefresh = false;
                {
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    if (_activeGameView.roomId != curRoom || _roomStateDirty.load()) {
                        needViewRefresh = true;
                    } else if (!senderHandle.empty()) {
                        bool knownInView = false;
                        for (const auto &s : _activeGameView.seats) {
                            if (iequalsStr(s.handle, senderHandle)) {
                                knownInView = true;
                                break;
                            }
                        }
                        if (!knownInView) {
                            knownInView = isObserverInGameView(_activeGameView, senderHandle, senderUuid);
                        }
                        if (!knownInView) {
                            needViewRefresh = true;
                        }
                    }
                }
                if (needViewRefresh) {
                    auto gvRes = _client.fetchGameView(curRoom);
                    if (gvRes.ok) {
                        std::lock_guard<std::mutex> lock(_stateMutex);
                        if (_currentRoomId == curRoom || _currentRoomId.empty()) {
                            _activeGameView = gvRes.data;
                            _activeGameViewFetchedMs = nowMs();
                            if (!gvRes.data.phase.empty()) {
                                _currentPhase = gvRes.data.phase;
                            }
                        }
                    }
                }
            }

            bool isOwner = !isRobot &&
                ((!senderHandle.empty() && !_identity.ownerHandle.empty() && iequalsStr(senderHandle, _identity.ownerHandle)) ||
                 (!senderUuid.empty() && !_identity.ownerUuid.empty() && senderUuid == _identity.ownerUuid));

            if (onGameChatReceived(curRoom, senderHandle, senderUuid, text,
                                   isOwner, isRobot, senderOwnerUuid, senderOwnerHandle)) {
                return;
            }

            if (_agent) {
                std::string reply;
                _agent->onGameChat(senderHandle, text, reply);
                if (!reply.empty() && !curRoom.empty() && !isRoomChatMuted(curRoom)) {
                    _client.postGameChat(curRoom, "ALL", reply);
                }
            }
            notifyEvent("CHAT_MENTION");
            wakeLoop(true, false);
        } catch (...) {}
        return;
    } else if (eventType == "GAME_PROPOSAL" || eventType == "PROPOSAL_OFFERED") {
        try {
            auto j = nlohmann::json::parse(data);
            GameProposalInfo proposal;
            if (j.contains("proposal") && j["proposal"].is_string() &&
                !j["proposal"].get<std::string>().empty()) {
                proposal.type = j["proposal"].get<std::string>();
            } else {
                proposal.type = j.value("type", "");
            }
            proposal.proposerUuid = j.value("proposer_uuid", j.value("player_uuid", ""));
            std::string proposerHandle = j.value("handle", "");
            bool proposerIsRobot = j.value("is_robot", false);

            if ((!proposerHandle.empty() && iequalsStr(proposerHandle, _identity.handle)) ||
                (!proposal.proposerUuid.empty() && !_identity.robotUuid.empty() &&
                 proposal.proposerUuid == _identity.robotUuid)) {
                return;
            }

            PendingSseAction act;
            act.type = PendingSseActionType::HANDLE_GAME_PROPOSAL;
            act.proposal = proposal;
            act.senderHandle = proposerHandle;
            act.senderIsRobot = proposerIsRobot;
            act.roomId = currentRoomId();
            enqueuePendingSseAction(std::move(act));
            notifyEvent("PROPOSAL");
        } catch (...) {}
        wakeLoop(true, false);
        return;
    } else if (eventType == "TURN" || eventType == "MOVE" || eventType == "GAME_STARTED") {
        notifyEvent("TURN");
        wakeLoop(true, false);
        return;
    } else if (eventType == "GAME_OVER") {
        onGameOverSseReceived();
        notifyEvent("GAME_OVER");
        wakeLoop(true, false);
        return;
    } else if (eventType == "closed" || eventType == "ROOM_PURGED" || eventType == "ROOM_CLOSED") {
        if (eventType == "closed" && data.find("ROBOT_TOKEN_SUPERSEDED") != std::string::npos) {
            logMsg(1, "Token superseded via game SSE: another instance connected with this token.");
            onSuperseded();
            onMatchReleased("");
            _supersededAtMs.store(nowMs());
            _superseded = true;
            notifyEvent("SUPERSEDED");
            wakeLoop(true, true);
            return;
        }
        std::string closedRoom;
        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            closedRoom = _currentRoomId;
            if (!closedRoom.empty()) {
                _chatMutedRooms.erase(closedRoom);
            }
            _currentRoomId = "";
            _currentSeat = 0;
            _currentPhase = "IDLE";
            _activeGameView = GameViewData();
            _activeGameViewFetchedMs = 0;
            _lastSeatedOpponents.clear();
            _gameOverHandledRoomId = "";
            _gameStartedHandledRoomId.clear();
            _resumedActiveRoomId.clear();
            _lastLoggedPlyCount = -1;
            _postGameSilenceStartMs.store(0);
        }
        onRoomClosedOrLeft(closedRoom);
        if (!closedRoom.empty()) {
            onMatchReleased(closedRoom);
            logMsg(2, "Room #" + closedRoom + " stream closed (" + eventType + "); returning to lobby.");
            _client.stopGameEventStream();
            notifyEvent("ROOM_LEFT");
        }
        wakeLoop(true, true);
        return;
    } else if (eventType == "SEAT_LEFT" || eventType == "PLAYER_DISCONNECTED") {
        std::string curRoom;
        std::string curPhase;
        int curSeat = 0;
        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            curRoom = _currentRoomId;
            curPhase = _currentPhase;
            curSeat = _currentSeat;
        }
        if ((curPhase == "FINISHED" || curPhase == "GAME_OVER" || curPhase == "ABANDONED") &&
            !curRoom.empty()) {
            auto gvRes = _client.fetchGameView(curRoom);
            if (gvRes.ok) {
                bool hasOpponent = false;
                for (const auto &seat : gvRes.data.seats) {
                    if (seat.seat != curSeat && !seat.handle.empty() && seat.connected) {
                        hasOpponent = true;
                        break;
                    }
                }
                if (!hasOpponent) {
                    logMsg(2, "Opponent left table #" + curRoom + "; leaving table and returning to lobby.");
                    PendingSseAction act;
                    act.type = PendingSseActionType::LEAVE_ROOM;
                    act.roomId = curRoom;
                    enqueuePendingSseAction(std::move(act));
                    return;
                }
            }
        }
        wakeLoop(true, false);
        return;
    } else if (eventType == "ROOM_CREATED") {
        return;
    } else if (eventType == "SEAT_JOINED") {
        std::string curRoom;
        GameViewData gvSnap;
        bool shouldCheckPrewarm = false;
        try {
            auto j = nlohmann::json::parse(data);
            std::string h = j.value("handle", "");
            std::string pu = j.value("player_uuid", "");
            std::string ru = j.value("robot_uuid", "");
            bool isRob = j.value("is_robot", false);
            int s = j.value("seat", -1);
            if (!h.empty() && h == _identity.handle && s > 0 && s == currentSeat() &&
                currentPhase() == "WAITING_FOR_PLAYERS") {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                curRoom = _currentRoomId;
                gvSnap = _activeGameView;
            }
            if (!curRoom.empty() && (!h.empty() || !pu.empty() || !ru.empty())) {
                GameSeatInfo joinedSeat;
                joinedSeat.seat = (s > 0) ? s : static_cast<int>(gvSnap.seats.size() + 1);
                joinedSeat.handle = h;
                joinedSeat.playerUuid = pu;
                joinedSeat.robotUuid = ru;
                joinedSeat.isRobot = isRob;
                joinedSeat.connected = true;
                bool updatedExisting = false;
                for (auto &existing : gvSnap.seats) {
                    if (s > 0 && existing.seat == s) {
                        existing = joinedSeat;
                        updatedExisting = true;
                        break;
                    }
                }
                if (!updatedExisting) {
                    gvSnap.seats.push_back(std::move(joinedSeat));
                }
                shouldCheckPrewarm = true;
            }
        } catch (...) {}
        if (shouldCheckPrewarm) {
            onRoomStateRefreshed(curRoom, gvSnap);
        }
        wakeLoop(true, false);
        return;
    } else if (eventType == "OBSERVER_JOINED") {
        std::string curRoom;
        GameViewData gvSnap;
        bool shouldCheckPrewarm = false;
        try {
            auto j = nlohmann::json::parse(data);
            std::string obsId = j.value("player_uuid", "");
            std::string obsHandle = j.value("handle", "");
            if (!obsId.empty() || !obsHandle.empty()) {
                std::lock_guard<std::mutex> lock(_stateMutex);
                upsertGameViewObserver(_activeGameView, obsId, obsHandle);
                curRoom = _currentRoomId;
                gvSnap = _activeGameView;
                shouldCheckPrewarm = !curRoom.empty();
            }
        } catch (...) {}
        if (shouldCheckPrewarm) {
            onRoomStateRefreshed(curRoom, gvSnap);
        }
        wakeLoop(true, false);
        return;
    } else if (eventType == "OBSERVER_LEFT") {
        try {
            auto j = nlohmann::json::parse(data);
            std::string obsId = j.value("player_uuid", "");
            std::string obsHandle = j.value("handle", "");
            if (!obsId.empty() || !obsHandle.empty()) {
                std::lock_guard<std::mutex> lock(_stateMutex);
                removeGameViewObserver(_activeGameView, obsId, obsHandle);
            }
        } catch (...) {}
        wakeLoop(true, false);
        return;
    } else {
        wakeLoop(true, false);
    }
}

// --- OctiBotFleetRunner Implementation ---

OctiBotFleetRunner::~OctiBotFleetRunner() {
    stop();
}

std::unique_ptr<OctiBotRunner> OctiBotFleetRunner::createRunner(
    std::unique_ptr<OctiBotAgent> agent,
    const OctiBotConfigStore &config,
    const std::string &customEndpoint) const {
    return std::make_unique<OctiBotRunner>(std::move(agent), config, customEndpoint);
}

void OctiBotFleetRunner::onBeforeRunnerStart(OctiBotRunner &runner,
                                             const OctiBotConfigStore &resolvedConfig) {
    (void) runner;
    (void) resolvedConfig;
}

void OctiBotFleetRunner::onFleetStarted() {
}

bool OctiBotFleetRunner::shouldStartProfile(const std::string &profileLabel,
                                            const OctiBotConfigStore &resolvedConfig,
                                            const std::string &algorithmName,
                                            const OctiBotAgent *agent) const {
    if (agent) {
        return true;
    }
    const std::string rawBeh = resolvedConfig.getString("table_behavior", "auto");
    std::size_t f = 0;
    while (f < rawBeh.size() && std::isspace(static_cast<unsigned char>(rawBeh[f]))) {
        ++f;
    }
    std::size_t l = rawBeh.size();
    while (l > f && std::isspace(static_cast<unsigned char>(rawBeh[l - 1]))) {
        --l;
    }
    std::string beh = rawBeh.substr(f, l - f);
    for (char &c : beh) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (resolvedConfig.getBool("lobby_only", false) || beh == "lobby_only") {
        return true;
    }
    std::cerr << "[" << profileLabel << "] no agent for algorithm '" << algorithmName
              << "' in this build; profile not started\n";
    return false;
}

bool OctiBotFleetRunner::startProfiles(const OctiBotConfigStore &config,
                                      const std::vector<std::string> &profiles,
                                      const std::string &customEndpoint) {
    for (const auto &pName : profiles) {
        OctiBotConfigStore profConfig = config.resolveProfile(pName);
        std::string algo = profConfig.getString("algorithm", "alpha_beta");
        auto agent = OctiBotAgentRegistry::instance().createAgent(algo, profConfig);
        if (!shouldStartProfile(pName, profConfig, algo, agent.get())) {
            continue;
        }
        auto runner = createRunner(std::move(agent), profConfig, customEndpoint);
        if (!runner) {
            continue;
        }

        std::string err;
        if (!runner->connectAndAuthenticate(err)) {
            std::cerr << "[" << pName << "] Failed to connect: " << err << "\n";
            continue;
        }

        runner->setConfigFilePath(profConfig.getString("config_path", ""));
        onBeforeRunnerStart(*runner, profConfig);
        runner->start();
        _runners.push_back(std::move(runner));
    }
    if (!_runners.empty()) {
        onFleetStarted();
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
        const std::string instLabel = baseProfile + "#" + std::to_string(i + 1);
        if (!shouldStartProfile(instLabel, instConfig, algo, agent.get())) {
            continue;
        }
        auto runner = createRunner(std::move(agent), instConfig, customEndpoint);
        if (!runner) {
            continue;
        }

        std::string err;
        if (!runner->connectAndAuthenticate(err)) {
            std::cerr << "[" << baseProfile << "#" << (i + 1) << "] Connect failed: " << err << "\n";
            continue;
        }
        runner->setConfigFilePath(instConfig.getString("config_path", ""));
        onBeforeRunnerStart(*runner, instConfig);
        runner->start();
        _runners.push_back(std::move(runner));
    }
    if (!_runners.empty()) {
        onFleetStarted();
    }
    return !_runners.empty();
}

void OctiBotFleetRunner::stop() {
    for (auto &r : _runners) {
        r->stop();
    }
    _lastReclaimAttemptMs.clear();
    _runners.clear();
}

bool OctiBotFleetRunner::tickSupervisor() {
    bool anyActiveOrStandby = false;
    const std::int64_t now = nowMs();

    for (auto &r : _runners) {
        if (!r) {
            continue;
        }
        if (r->isRunning()) {
            anyActiveOrStandby = true;
            continue;
        }
        if (r->isSuperseded() &&
            r->hasReusableNamedToken() &&
            r->reclaimSupersededIntervalMs() > 0) {
            anyActiveOrStandby = true;
            const std::int64_t intervalMs = r->reclaimSupersededIntervalMs();
            const std::int64_t baselineMs = std::max(
                _lastReclaimAttemptMs[r.get()],
                r->supersededAtMs());
            if (now - baselineMs >= intervalMs) {
                _lastReclaimAttemptMs[r.get()] = now;
                std::string err;
                if (r->reclaimIfUnused(err)) {
                    r->start();
                    std::string handle = r->identity().handle.empty()
                        ? r->config().getString("robot_name", "stock")
                        : r->identity().handle;
                    r->logMsg(1, "[SUPERVISOR] Reclaimed idle robot session for @" + handle);
                }
            }
        }
    }
    return anyActiveOrStandby;
}

void OctiBotFleetRunner::waitForAll() {
    while (!s_stopRequested.load() && tickSupervisor()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int runFleetCli(int argc, char **argv) {
    std::string configPath = OctiBotConfigStore::defaultConfigFilePath();
    std::string tokensPath = OctiBotConfigStore::defaultTokensFilePath();
    std::string singleProfile;
    std::vector<std::string> multiProfiles;
    bool allProfiles = false;
    int instances = 1;
    int cliVerbosity = -1;
    int cliAllowSupersede = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--help" || arg == "-h") {
            std::cout << "OCTIBOT v" << OCTI_APP_VERSION << "\n"
                      << "Usage: octibot [options]\n\n"
                      << "Options:\n"
                      << "  --profile <name>         Run single bot profile\n"
                      << "  --profiles <p1,p2,...>   Run multiple bot profiles\n"
                      << "  --all-profiles           Run all configured profiles\n"
                      << "  --instances <n>          Number of instances per profile\n"
                      << "  --supersede              Allow superseding an active session using the same token\n"
                      << "  --no-supersede           Fail if token is already in use (default)\n"
                      << "  --config <path>          Path to octibot.conf\n"
                      << "  --tokens-file <path>     Path to tokens.conf\n"
                      << "  --verbosity <level>      Verbosity (0=quiet, 1=error, 2=info, 3=verbose, 4=trace)\n"
                      << "  -v, --verbose            Shortcut for --verbosity verbose\n"
                      << "  -q, --quiet              Shortcut for --verbosity quiet\n"
                      << "  -V, --version            Show version and exit\n"
                      << "  -h, --help               Show this help message and exit\n";
            return 0;
        } else if (arg == "--version" || arg == "-V") {
            std::cout << "OCTIBOT v" << OCTI_APP_VERSION << std::endl;
            return 0;
        } else if (arg == "--supersede") {
            cliAllowSupersede = 1;
        } else if (arg == "--no-supersede") {
            cliAllowSupersede = 0;
        } else if (arg == "--config") {
            if (i + 1 >= argc) {
                std::cerr << "[OCTIBOT] Missing argument for option '" << arg
                          << "'. Try '--help' for usage." << std::endl;
                return 1;
            }
            configPath = argv[++i];
        } else if (arg == "--tokens-file") {
            if (i + 1 >= argc) {
                std::cerr << "[OCTIBOT] Missing argument for option '" << arg
                          << "'. Try '--help' for usage." << std::endl;
                return 1;
            }
            tokensPath = argv[++i];
        } else if (arg == "--profile") {
            if (i + 1 >= argc) {
                std::cerr << "[OCTIBOT] Missing argument for option '" << arg
                          << "'. Try '--help' for usage." << std::endl;
                return 1;
            }
            std::string p = argv[++i];
            if (singleProfile.empty() && multiProfiles.empty()) {
                singleProfile = p;
            } else {
                if (!singleProfile.empty() && multiProfiles.empty()) {
                    multiProfiles.push_back(singleProfile);
                    singleProfile.clear();
                }
                multiProfiles.push_back(p);
            }
        } else if (arg == "--profiles") {
            if (i + 1 >= argc || argv[i + 1][0] == '-') {
                std::cerr << "[OCTIBOT] Missing argument for option '" << arg
                          << "'. Try '--help' for usage." << std::endl;
                return 1;
            }
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                std::string plist = argv[++i];
                std::stringstream ss(plist);
                std::string p;
                while (std::getline(ss, p, ',')) {
                    size_t first = p.find_first_not_of(" \t\r\n");
                    size_t last = p.find_last_not_of(" \t\r\n");
                    if (first != std::string::npos && last != std::string::npos) {
                        multiProfiles.push_back(p.substr(first, last - first + 1));
                    }
                }
            }
        } else if (arg == "--all-profiles") {
            allProfiles = true;
        } else if (arg == "--instances") {
            if (i + 1 >= argc) {
                std::cerr << "[OCTIBOT] Missing argument for option '" << arg
                          << "'. Try '--help' for usage." << std::endl;
                return 1;
            }
            instances = std::atoi(argv[++i]);
        } else if (arg == "--verbosity") {
            if (i + 1 >= argc) {
                std::cerr << "[OCTIBOT] Missing argument for option '" << arg
                          << "'. Try '--help' for usage." << std::endl;
                return 1;
            }
            cliVerbosity = OctiBotConfigStore::parseVerbosityLevel(argv[++i]);
        } else if (arg == "--verbose" || arg == "-v") {
            cliVerbosity = 3;
        } else if (arg == "--quiet" || arg == "-q") {
            cliVerbosity = 0;
        } else {
            std::cerr << "[OCTIBOT] Unknown option '" << arg
                      << "'. Try '--help' for usage." << std::endl;
            return 1;
        }
    }

    OctiBotConfigStore config;
    std::string err;
    if (std::ifstream(configPath).good()) {
        if (!config.loadFile(configPath, err)) {
            std::cerr << "[OCTIBOT] Config load error (" << configPath << "): " << err << std::endl;
            return 1;
        }
    }
    if (std::ifstream(tokensPath).good()) {
        config.loadTokensFile(tokensPath, err);
    }
    config.set("config_path", configPath);
    if (cliVerbosity >= 0) {
        const std::string verbStr = std::to_string(cliVerbosity);
        config.set("verbosity", verbStr);
        for (const auto &pName : config.profileNames()) {
            config.setProfileEntry(pName, "verbosity", verbStr);
        }
    }
    if (cliAllowSupersede >= 0) {
        const std::string supStr = (cliAllowSupersede > 0) ? "true" : "false";
        config.set("allow_supersede", supStr);
        for (const auto &pName : config.profileNames()) {
            config.setProfileEntry(pName, "allow_supersede", supStr);
        }
    }

    if (config.getVerbosityLevel(3) >= 2) {
        std::cout << "OCTIBOT v" << OCTI_APP_VERSION << " (config=" << configPath
                  << ", tokens=" << tokensPath << ")" << std::endl;
    }

    OctiBotFleetRunner fleet;
    s_stopRequested.store(false);
    s_activeFleet = &fleet;
    std::signal(SIGINT, handleFleetSignal);
    std::signal(SIGTERM, handleFleetSignal);

    if (singleProfile.empty() && multiProfiles.empty() && config.hasKey("profiles")) {
        std::string plist = config.getString("profiles", "");
        std::stringstream ss(plist);
        std::string p;
        while (std::getline(ss, p, ',')) {
            size_t first = p.find_first_not_of(" \t\r\n");
            size_t last = p.find_last_not_of(" \t\r\n");
            if (first != std::string::npos && last != std::string::npos) {
                multiProfiles.push_back(p.substr(first, last - first + 1));
            }
        }
    }
    if (singleProfile.empty()) {
        singleProfile = config.getString("default_profile", "");
    }
    if (!allProfiles && config.getBool("all_profiles", false)) {
        allProfiles = true;
    }
    if (instances <= 1 && config.hasKey("instances")) {
        instances = config.getInt("instances", 1);
    }

    if (instances > 1 && !singleProfile.empty()) {
        fleet.startClones(config, singleProfile, instances);
    } else if (allProfiles) {
        fleet.startAllProfiles(config);
    } else if (!multiProfiles.empty()) {
        fleet.startProfiles(config, multiProfiles);
    } else {
        std::string prof = singleProfile;
        if (prof.empty()) {
            auto names = config.profileNames();
            if (!names.empty()) {
                prof = names.front();
            } else {
                prof = "stock";
            }
        }
        fleet.startProfiles(config, {prof});
    }

    fleet.waitForAll();
    s_activeFleet = nullptr;
    std::cout << "[OCTIBOT] Clean shutdown complete." << std::endl;
    return 0;
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
