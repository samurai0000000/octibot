/*
 * OctiBotPolicy.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiBotPolicy.hxx"
#include <algorithm>
#include <cctype>
#include <random>
#include <sstream>

namespace octi {

namespace {

std::string trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return s.substr(start, end - start);
}

std::string toLower(const std::string &s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return res;
}

std::set<std::string> splitSet(const std::string &input, bool lowerCase = false) {
    std::set<std::string> res;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string t = trim(item);
        if (!t.empty()) {
            res.insert(lowerCase ? toLower(t) : t);
        }
    }
    return res;
}

bool caseInsensitiveEqual(const std::string &a, const std::string &b) {
    return toLower(a) == toLower(b);
}

} // namespace

OctiBotPolicyEngine::OctiBotPolicyEngine(const OctiBotConfigStore &config) {
    configure(config);
}

bool OctiBotPolicyEngine::parseHostVariants(const std::string &raw,
                                            std::vector<HostVariantSpec> &outSpecs,
                                            std::string &errorOut) {
    outSpecs.clear();
    errorOut.clear();
    std::string trimmed = trim(raw);
    if (trimmed.empty()) {
        return true;
    }
    static const HostVariantSpec kKnownVariants[13] = {
        {"2P_3B",      "2P",  3, false, false},
        {"2P_3B_SP",   "2P",  3, true,  false},
        {"2P_3B_ED",   "2P",  3, false, true},
        {"2P_3B_SPED", "2P",  3, true,  true},
        {"2P_1B",      "2P",  1, false, false},
        {"2P_1B_SP",   "2P",  1, true,  false},
        {"2P_1B_ED",   "2P",  1, false, true},
        {"2P_1B_SPED", "2P",  1, true,  true},
        {"4P",         "4P",  1, false, false},
        {"4P_SP",      "4P",  1, true,  false},
        {"4P_ED",      "4P",  1, false, true},
        {"4P_SPED",    "4P",  1, true,  true},
        {"OFK",        "OFK", 1, false, false}
    };
    std::size_t start = 0;
    while (start <= trimmed.size()) {
        std::size_t comma = trimmed.find(',', start);
        std::string item = (comma == std::string::npos)
            ? trim(trimmed.substr(start))
            : trim(trimmed.substr(start, comma - start));
        if (item.empty()) {
            outSpecs.clear();
            errorOut = "Empty variant name in host_variants";
            return false;
        }
        const HostVariantSpec *matched = nullptr;
        for (std::size_t i = 0; i < 13; ++i) {
            if (item == kKnownVariants[i].name) {
                matched = &kKnownVariants[i];
                break;
            }
        }
        if (!matched) {
            outSpecs.clear();
            errorOut = "Invalid variant name '" + item + "' in host_variants";
            return false;
        }
        outSpecs.push_back(*matched);
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return true;
}

void OctiBotPolicyEngine::configure(const OctiBotConfigStore &config) {
    std::string dummyErr;
    configure(config, dummyErr);
}

bool OctiBotPolicyEngine::configure(const OctiBotConfigStore &config, std::string &errorOut) {
    std::lock_guard<std::mutex> lock(_mutex);
    errorOut.clear();

    _tableBehavior = config.getString("table_behavior", "auto");
    _acceptModes = splitSet(config.getString("accept_modes", "2P, 4P, OFK"));
    _acceptBases = config.getString("accept_bases", "any");
    _acceptSuperProngs = config.getString("accept_super_prongs", "any");
    _acceptEdgeless = config.getString("accept_edgeless", "any");
    _acceptTimeControl = config.getString("accept_time_control", "any");
    _minInitialTimeSec = config.getInt("min_initial_time_sec", 0);
    _maxInitialTimeSec = config.getInt("max_initial_time_sec", 14400);
    _minIncrementSec = config.getInt("min_increment_sec", 0);
    _acceptTakeback = config.getString("accept_takeback", "any");

    _friendList = splitSet(config.getString("friend_list", ""), true);
    _banList = splitSet(config.getString("ban_list", ""), true);
    _friendsOnly = config.getBool("friends_only", false);
    _opponentFilter = config.getString("opponent_filter", "any");

    _acceptDrawEvalThreshold = config.getInt("accept_draw_eval_threshold", -1500);
    _acceptUndo = config.getString("accept_undo", "friends_only");
    _acceptPause = config.getString("accept_pause", "always");
    _acceptRematch = config.getString("accept_rematch", "always");
    _maxConsecutiveRematches = config.getInt("max_consecutive_rematches", 5);
    _claimForfeitDelaySec = config.getInt("claim_forfeit_delay_sec", 10);
    _minMoveThinkMs = config.getInt("min_move_think_ms", 0);

    _acceptTournamentMatches = config.getBool("accept_tournament_matches", true);
    _tournamentPostGameLingerSec = std::max(0, config.getInt("tournament_post_game_linger_sec", 2));
    _tournamentSuppressCasualChat = config.getBool("tournament_suppress_casual_chat", true);
    _timedClockPolicy = TimedClockBudgetPolicy{};
    applyConfigToTimedClockPolicy(config, _timedClockPolicy);

    _hostMode = config.getString("host_mode", "2P");
    _hostBases = config.getInt("host_bases", 3);
    _hostSuperProngs = config.getBool("host_super_prongs", false);
    _hostEdgeless = config.getBool("host_edgeless", false);
    _hostTimeControlEnabled = config.getBool("host_time_control_enabled", false);
    _hostInitialMs = config.getInt64("host_initial_ms", 600000);
    _hostIncrementMs = config.getInt64("host_increment_ms", 5000);
    _hostAllowRobots = config.getBool("host_allow_robots", _opponentFilter != "humans_only");
    _hostSeat = trim(config.getString("host_seat", "wait_for_human"));

    std::vector<HostVariantSpec> parsedVariants;
    if (!parseHostVariants(config.getString("host_variants", ""), parsedVariants, errorOut)) {
        _hostVariants.clear();
        _nextHostVariantIndex = 0;
        return false;
    }
    _hostVariants = std::move(parsedVariants);
    _nextHostVariantIndex = 0;
    return true;
}

bool OctiBotPolicyEngine::isFriend(const std::string &handle) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _friendList.count(toLower(handle)) > 0;
}

bool OctiBotPolicyEngine::isBanned(const std::string &handle) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _banList.count(toLower(handle)) > 0;
}

void OctiBotPolicyEngine::addFriend(const std::string &handle) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!handle.empty()) {
        _friendList.insert(toLower(handle));
    }
}

void OctiBotPolicyEngine::removeFriend(const std::string &handle) {
    std::lock_guard<std::mutex> lock(_mutex);
    _friendList.erase(toLower(handle));
}

void OctiBotPolicyEngine::addBan(const std::string &handle) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!handle.empty()) {
        _banList.insert(toLower(handle));
    }
}

void OctiBotPolicyEngine::removeBan(const std::string &handle) {
    std::lock_guard<std::mutex> lock(_mutex);
    _banList.erase(toLower(handle));
}

bool OctiBotPolicyEngine::isTableCompatible(const TableSummaryInfo &table,
                                           const std::string &ownerHandle,
                                           const std::string &myHandle) const {
    std::lock_guard<std::mutex> lock(_mutex);

    if (table.phase != "WAITING_FOR_PLAYERS") {
        return false;
    }
    if (table.isTournamentRoom && !_acceptTournamentMatches) {
        return false;
    }
    if (table.hasPasscode) {
        return false;
    }
    if (table.seatsFilled >= table.seatsTotal) {
        return false;
    }
    if (!table.allowRobots) {
        return false;
    }
    if (!myHandle.empty() && caseInsensitiveEqual(table.hostHandle, myHandle)) {
        return false;
    }

    // Mode filter
    if (!_acceptModes.empty() && _acceptModes.count(table.mode) == 0) {
        return false;
    }

    // Bases filter
    if (_acceptBases != "any" && std::to_string(table.bases) != _acceptBases) {
        return false;
    }

    // Super prongs filter
    if (_acceptSuperProngs != "any") {
        bool wantSuper = (_acceptSuperProngs == "true" || _acceptSuperProngs == "1" || _acceptSuperProngs == "yes");
        if (table.superProngs != wantSuper) {
            return false;
        }
    }

    // Edgeless filter
    if (_acceptEdgeless != "any") {
        bool wantEdgeless = (_acceptEdgeless == "true" || _acceptEdgeless == "1" || _acceptEdgeless == "yes");
        if (table.edgeless != wantEdgeless) {
            return false;
        }
    }

    // Casual time-control and takeback filters (tournament rooms follow TD rules)
    if (!table.isTournamentRoom) {
        if (_acceptTimeControl == "timed_only" && !table.timeControlEnabled) {
            return false;
        }
        if (_acceptTimeControl == "untimed_only" && table.timeControlEnabled) {
            return false;
        }
        if (table.timeControlEnabled) {
            int initialSec = static_cast<int>(table.initialMs / 1000);
            int incSec = static_cast<int>(table.incrementMs / 1000);
            if (initialSec < _minInitialTimeSec || initialSec > _maxInitialTimeSec) {
                return false;
            }
            if (incSec < _minIncrementSec) {
                return false;
            }
        }

        if (_acceptTakeback != "any") {
            bool wantTakeback = (_acceptTakeback == "true" || _acceptTakeback == "1" || _acceptTakeback == "yes");
            if (table.allowTakeback != wantTakeback) {
                return false;
            }
        }
    }

    // Check seated players
    for (const auto &seat : table.seats) {
        if (seat.handle.empty()) {
            continue;
        }
        if (caseInsensitiveEqual(seat.handle, myHandle)) {
            return false; // Already in table
        }
        if (!table.isTournamentRoom) {
            if (_banList.count(toLower(seat.handle)) > 0) {
                return false; // Banned player seated
            }
            if (_friendsOnly) {
                if (_friendList.count(toLower(seat.handle)) == 0 &&
                    !caseInsensitiveEqual(seat.handle, ownerHandle)) {
                    return false;
                }
            }
            if (_opponentFilter == "humans_only" && seat.isRobot) {
                return false;
            }
            if (_opponentFilter == "robots_only" && !seat.isRobot &&
                !caseInsensitiveEqual(seat.handle, ownerHandle)) {
                return false;
            }
            if (_opponentFilter == "owner_only" &&
                !caseInsensitiveEqual(seat.handle, ownerHandle)) {
                return false;
            }
        }
    }

    return true;
}

std::vector<TableSummaryInfo> OctiBotPolicyEngine::rankCompatibleTables(
    const std::vector<TableSummaryInfo> &tables,
    const std::string &ownerHandle,
    const std::string &myHandle) const {

    std::vector<TableSummaryInfo> compatible;
    for (const auto &t : tables) {
        if (isTableCompatible(t, ownerHandle, myHandle)) {
            compatible.push_back(t);
        }
    }

    std::sort(compatible.begin(), compatible.end(),
        [this, &ownerHandle](const TableSummaryInfo &a, const TableSummaryInfo &b) {
            if (a.isTournamentRoom != b.isTournamentRoom) {
                return a.isTournamentRoom; // Tournament assignments first
            }

            bool aOwner = caseInsensitiveEqual(a.hostHandle, ownerHandle);
            bool bOwner = caseInsensitiveEqual(b.hostHandle, ownerHandle);
            if (aOwner != bOwner) {
                return aOwner; // Owner tables next
            }

            bool aFriend = isFriend(a.hostHandle);
            bool bFriend = isFriend(b.hostHandle);
            if (aFriend != bFriend) {
                return aFriend; // Friend tables next
            }

            if (a.seatsFilled != b.seatsFilled) {
                return a.seatsFilled > b.seatsFilled; // More filled seats first
            }

            return a.createdAtMs < b.createdAtMs; // Older tables first
        });

    return compatible;
}

bool OctiBotPolicyEngine::shouldKickSeatedPlayer(const TableSeatInfo &seat,
                                                const std::string &ownerHandle,
                                                bool isTournamentRoom) const {
    if (isTournamentRoom) {
        return false;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    if (_banList.count(toLower(seat.handle)) > 0) {
        return true;
    }
    if (_opponentFilter == "humans_only" && seat.isRobot) {
        return true;
    }
    if (_friendsOnly) {
        if (_friendList.count(toLower(seat.handle)) == 0 &&
            !caseInsensitiveEqual(seat.handle, ownerHandle)) {
            return true;
        }
    }
    return false;
}

bool OctiBotPolicyEngine::shouldAcceptProposal(const GameProposalInfo &proposal,
                                              const std::string &proposerHandle,
                                              int currentEvalScore,
                                              int consecutiveRematches) const {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_banList.count(toLower(proposerHandle)) > 0) {
        return false;
    }

    bool friendProposer = (_friendList.count(toLower(proposerHandle)) > 0);

    if (proposal.type == "DRAW_OFFER") {
        return currentEvalScore <= _acceptDrawEvalThreshold;
    }
    if (proposal.type == "UNDO_REQUEST") {
        if (_acceptUndo == "always") {
            return true;
        }
        if (_acceptUndo == "friends_only") {
            return friendProposer;
        }
        return false;
    }
    if (proposal.type == "PAUSE_REQUEST") {
        if (_acceptPause == "always") {
            return true;
        }
        if (_acceptPause == "friends_only") {
            return friendProposer;
        }
        return false;
    }
    if (proposal.type == "REMATCH_REQUEST") {
        if (consecutiveRematches >= _maxConsecutiveRematches) {
            return false;
        }
        if (_acceptRematch == "always") {
            return true;
        }
        if (_acceptRematch == "friends_only") {
            return friendProposer;
        }
        return false;
    }

    return false;
}

CreateTableRequest OctiBotPolicyEngine::buildHostTableRequest() const {
    std::lock_guard<std::mutex> lock(_mutex);
    CreateTableRequest req;
    req.mode = _hostMode;
    req.bases = _hostBases;
    req.superProngs = _hostSuperProngs;
    req.edgeless = _hostEdgeless;
    if (!_hostVariants.empty()) {
        const HostVariantSpec &v = _hostVariants[_nextHostVariantIndex % _hostVariants.size()];
        _nextHostVariantIndex = (_nextHostVariantIndex + 1) % _hostVariants.size();
        req.mode = v.mode;
        req.bases = v.bases;
        req.superProngs = v.superProngs;
        req.edgeless = v.edgeless;
    }
    req.timeControlEnabled = _hostTimeControlEnabled;
    req.initialMs = _hostInitialMs;
    req.incrementMs = _hostIncrementMs;
    req.allowRobots = _hostAllowRobots && (_opponentFilter != "humans_only");

    int seatCount = (req.mode == "4P") ? 4 : 2;
    std::string hs = toLower(trim(_hostSeat));
    if (hs == "wait_for_human" || hs == "0" || hs == "unseated") {
        req.seat = 0;
    } else if (hs == "1" || hs == "a") {
        req.seat = 1;
    } else if (hs == "2" || hs == "b") {
        req.seat = 2;
    } else if ((hs == "3" || hs == "c") && seatCount == 4) {
        req.seat = 3;
    } else if ((hs == "4" || hs == "d") && seatCount == 4) {
        req.seat = 4;
    } else {
        thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(1, seatCount);
        req.seat = dist(rng);
    }
    return req;
}

MatchmakingRequest OctiBotPolicyEngine::buildMatchmakingRequest() const {
    std::lock_guard<std::mutex> lock(_mutex);
    MatchmakingRequest req;
    req.mode = _hostMode;
    req.bases = _hostBases;
    req.superProngs = _hostSuperProngs;
    req.edgeless = _hostEdgeless;
    req.timeControlEnabled = _hostTimeControlEnabled;
    req.initialMs = _hostInitialMs;
    req.incrementMs = _hostIncrementMs;
    req.allowRobots = _hostAllowRobots && (_opponentFilter != "humans_only");
    return req;
}

void applyConfigToTimedClockPolicy(const OctiBotConfigStore &config,
                                   TimedClockBudgetPolicy &policy) {
    policy.timedAutoPace =
        config.getBool("timed_auto_pace", policy.timedAutoPace);
    policy.timedTimeBudgetMinMs =
        config.getInt("timed_time_budget_min_ms", policy.timedTimeBudgetMinMs);
    policy.timedTimeBudgetMaxMs =
        config.getInt("timed_time_budget_max_ms", policy.timedTimeBudgetMaxMs);
    policy.clockSafetyMarginMs =
        config.getInt("clock_safety_margin_ms", policy.clockSafetyMarginMs);
    policy.timedOpeningTaperMaxPly =
        config.getInt("timed_opening_taper_max_ply", policy.timedOpeningTaperMaxPly);
    policy.timedOpeningTaperPct =
        config.getInt("timed_opening_taper_pct", policy.timedOpeningTaperPct);
    policy.timedMidgameHorizonMoves =
        config.getInt("timed_midgame_horizon_moves", policy.timedMidgameHorizonMoves);
    policy.timedEndgameHorizonMoves =
        config.getInt("timed_endgame_horizon_moves", policy.timedEndgameHorizonMoves);
    policy.timedEndgameMinPly =
        config.getInt("timed_endgame_min_ply", policy.timedEndgameMinPly);
    policy.timedIncrementWeightPct =
        config.getInt("timed_increment_weight_pct", policy.timedIncrementWeightPct);
    policy.timedMaxBankFractionPct =
        config.getInt("timed_max_bank_fraction_pct", policy.timedMaxBankFractionPct);
    policy.timedEmergencyFloorMs =
        config.getInt("timed_emergency_floor_ms", policy.timedEmergencyFloorMs);
}

void serializeTimedClockPolicyToMap(const TimedClockBudgetPolicy &policy,
                                    std::map<std::string, std::string> &out) {
    out["timed_auto_pace"] = policy.timedAutoPace ? "true" : "false";
    out["timed_time_budget_min_ms"] = std::to_string(policy.timedTimeBudgetMinMs);
    out["timed_time_budget_max_ms"] = std::to_string(policy.timedTimeBudgetMaxMs);
    out["clock_safety_margin_ms"] = std::to_string(policy.clockSafetyMarginMs);
    out["timed_opening_taper_max_ply"] = std::to_string(policy.timedOpeningTaperMaxPly);
    out["timed_opening_taper_pct"] = std::to_string(policy.timedOpeningTaperPct);
    out["timed_midgame_horizon_moves"] = std::to_string(policy.timedMidgameHorizonMoves);
    out["timed_endgame_horizon_moves"] = std::to_string(policy.timedEndgameHorizonMoves);
    out["timed_endgame_min_ply"] = std::to_string(policy.timedEndgameMinPly);
    out["timed_increment_weight_pct"] = std::to_string(policy.timedIncrementWeightPct);
    out["timed_max_bank_fraction_pct"] = std::to_string(policy.timedMaxBankFractionPct);
    out["timed_emergency_floor_ms"] = std::to_string(policy.timedEmergencyFloorMs);
}

TimedMoveBudgetBreakdown computeClockManagedMoveBudget(
    const TimedClockBudgetPolicy &policy,
    int configuredUntimedBudgetMs,
    const GameViewData &game,
    std::int64_t remainingClockMs,
    int currentPly,
    int extraEngineStopOverheadMs) {
    constexpr std::int64_t kMaxSafeMs = 86400000LL * 365;
    const int safeUntimedMs = static_cast<int>(
        std::clamp<std::int64_t>(configuredUntimedBudgetMs, 1, 36000000LL));
    const int midHorizon = std::clamp(policy.timedMidgameHorizonMoves, 1, 200);
    const int endHorizon = std::clamp(policy.timedEndgameHorizonMoves, 1, 200);
    const int endMinPly = std::max(0, policy.timedEndgameMinPly);
    const int horizon = (currentPly >= endMinPly) ? endHorizon : midHorizon;

    TimedMoveBudgetBreakdown bd;
    bd.allocatedBudgetMs = safeUntimedMs;
    bd.hardClockCeilingMs = safeUntimedMs;
    bd.usableClockMs = 0;
    bd.horizonMoves = horizon;
    bd.usedDynamicClock = false;

    if (!game.timeControlEnabled) {
        return bd;
    }

    bd.usedDynamicClock = true;
    const std::int64_t remMs =
        std::clamp<std::int64_t>(remainingClockMs, -kMaxSafeMs, kMaxSafeMs);
    const std::int64_t incMs =
        std::clamp<std::int64_t>(game.incrementMs, 0, kMaxSafeMs);
    const std::int64_t baseSafetyMs =
        std::clamp<std::int64_t>(policy.clockSafetyMarginMs, 50, 36000000LL);
    const std::int64_t extraStopMs =
        std::clamp<std::int64_t>(extraEngineStopOverheadMs, 0, 36000000LL);
    const std::int64_t effectiveSafetyMs = baseSafetyMs + extraStopMs;
    const std::int64_t emergencyFloorMs =
        std::clamp<std::int64_t>(policy.timedEmergencyFloorMs, 50, 36000000LL);

    if (remMs <= effectiveSafetyMs) {
        const std::int64_t thirdRem = (remMs > 0) ? (remMs / 3) : 0;
        const int emergMs = static_cast<int>(
            std::clamp<std::int64_t>(thirdRem, 50, emergencyFloorMs));
        bd.usableClockMs = 0;
        bd.allocatedBudgetMs = emergMs;
        bd.hardClockCeilingMs = emergMs;
        bd.clampedBySafetyCeiling = true;
        bd.inEmergencyTimeTrouble = true;
        return bd;
    }

    const std::int64_t usableMs = remMs - effectiveSafetyMs;
    bd.usableClockMs = usableMs;

    const std::int64_t openingPct =
        (currentPly >= 0 && currentPly < policy.timedOpeningTaperMaxPly)
            ? std::clamp<std::int64_t>(policy.timedOpeningTaperPct, 1, 200)
            : 100;
    const std::int64_t incWeightPct =
        std::clamp<std::int64_t>(policy.timedIncrementWeightPct, 0, 100);
    const std::int64_t maxBankPct =
        std::clamp<std::int64_t>(policy.timedMaxBankFractionPct, 1, 100);

    const std::int64_t bankTargetMs =
        (usableMs * openingPct) / (static_cast<std::int64_t>(horizon) * 100);
    const std::int64_t incTargetMs = (incMs * incWeightPct) / 100;
    const std::int64_t rawTargetMs = bankTargetMs + incTargetMs;

    const std::int64_t effMinMs =
        std::clamp<std::int64_t>(policy.timedTimeBudgetMinMs, 10, 36000000LL);
    const std::int64_t configuredMaxMs = policy.timedAutoPace
        ? std::clamp<std::int64_t>(policy.timedTimeBudgetMaxMs, 10, 36000000LL)
        : static_cast<std::int64_t>(safeUntimedMs);
    const std::int64_t effMaxMs = std::max<std::int64_t>(effMinMs, configuredMaxMs);

    std::int64_t softTargetMs = rawTargetMs;
    if (softTargetMs < effMinMs) {
        softTargetMs = effMinMs;
        bd.clampedByMin = true;
    }
    if (softTargetMs > effMaxMs) {
        softTargetMs = effMaxMs;
        bd.clampedByMax = true;
    }

    const std::int64_t bankFractionCapMs = (usableMs * maxBankPct) / 100;
    const std::int64_t hardCeiling64 = std::min<std::int64_t>(
        usableMs,
        std::max<std::int64_t>(emergencyFloorMs, bankFractionCapMs));
    const std::int64_t clampedHardCeiling64 =
        std::clamp<std::int64_t>(hardCeiling64, 1, 36000000LL);
    bd.hardClockCeilingMs = static_cast<int>(clampedHardCeiling64);

    std::int64_t finalMs = softTargetMs;
    if (finalMs > clampedHardCeiling64) {
        finalMs = clampedHardCeiling64;
        bd.clampedBySafetyCeiling = true;
    }
    bd.allocatedBudgetMs = static_cast<int>(finalMs);
    return bd;
}

std::int64_t computeEffectiveRemainingClockMs(
    const GameViewData &game,
    int mySeat,
    std::int64_t snapshotFetchedAtMs,
    std::int64_t nowAtTurnStartMs) {
    std::int64_t rawClockMs = -1;
    bool foundSeat = false;
    for (const auto &s : game.seats) {
        if (s.seat == mySeat) {
            rawClockMs = s.clockMs;
            foundSeat = true;
            break;
        }
    }
    if (!foundSeat) {
        return 0;
    }
    if (!game.timeControlEnabled || rawClockMs < 0) {
        return rawClockMs;
    }
    if (game.phase != "IN_PROGRESS" || game.paused || game.turnSeat != mySeat) {
        return std::max<std::int64_t>(0, rawClockMs);
    }

    const std::int64_t localSinceFetchMs =
        (snapshotFetchedAtMs > 0 && nowAtTurnStartMs > snapshotFetchedAtMs)
            ? (nowAtTurnStartMs - snapshotFetchedAtMs)
            : 0;

    // On octi-server, liveClockLocked already subtracts turnElapsedMs from seat.clockMs
    // at snapshot generation time. Only deduct turnElapsedMs if a caller supplied an
    // un-decremented full initial bank on a player's opening turn.
    const int firstTurnPlies = (game.mode == "4P") ? 4 : 2;
    const std::int64_t turnElapsed = std::max<std::int64_t>(0, game.turnElapsedMs);
    const std::int64_t unaccountedServerTurnElapsedMs =
        (game.plyCount >= 0 && game.plyCount < firstTurnPlies &&
         game.initialMs > 0 && rawClockMs >= game.initialMs && turnElapsed > 0)
            ? turnElapsed
            : 0;

    return std::max<std::int64_t>(
        0, rawClockMs - unaccountedServerTurnElapsedMs - localSinceFetchMs);
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
