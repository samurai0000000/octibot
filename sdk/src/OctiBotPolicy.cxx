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

    // Time control filter
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

    // Takeback filter
    if (_acceptTakeback != "any") {
        bool wantTakeback = (_acceptTakeback == "true" || _acceptTakeback == "1" || _acceptTakeback == "yes");
        if (table.allowTakeback != wantTakeback) {
            return false;
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
            bool aOwner = caseInsensitiveEqual(a.hostHandle, ownerHandle);
            bool bOwner = caseInsensitiveEqual(b.hostHandle, ownerHandle);
            if (aOwner != bOwner) {
                return aOwner; // Owner tables first
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
                                                const std::string &ownerHandle) const {
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
