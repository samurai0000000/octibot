/*
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiBotPolicy.hxx"
#include <algorithm>
#include <cctype>
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

void OctiBotPolicyEngine::configure(const OctiBotConfigStore &config) {
    std::lock_guard<std::mutex> lock(_mutex);

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
    req.timeControlEnabled = _hostTimeControlEnabled;
    req.initialMs = _hostInitialMs;
    req.incrementMs = _hostIncrementMs;
    req.allowRobots = true;
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
    req.allowRobots = true;
    return req;
}

OctiBotConversationEngine::OctiBotConversationEngine(const OctiBotConfigStore &config) {
    configure(config);
}

void OctiBotConversationEngine::configure(const OctiBotConfigStore &config) {
    _greetingLobby = config.getString("greeting_lobby", _greetingLobby);
    _greetingGameStart = config.getString("greeting_game_start", _greetingGameStart);
    _chatGameOverWin = config.getString("chat_game_over_win", _chatGameOverWin);
    _chatGameOverLoss = config.getString("chat_game_over_loss", _chatGameOverLoss);
    _chatGameOverDraw = config.getString("chat_game_over_draw", _chatGameOverDraw);
}

std::uint64_t OctiBotConversationEngine::pushInbox(const std::string &channel,
                                                  const std::string &roomId,
                                                  const std::string &senderHandle,
                                                  const std::string &senderUuid,
                                                  bool isOwner,
                                                  const std::string &text,
                                                  std::int64_t timestampMs) {
    std::lock_guard<std::mutex> lock(_inboxMutex);
    ChatInboxEntry entry;
    entry.id = _nextInboxId++;
    entry.channel = channel;
    entry.roomId = roomId;
    entry.senderHandle = senderHandle;
    entry.senderUuid = senderUuid;
    entry.isOwner = isOwner;
    entry.text = text;
    entry.timestampMs = timestampMs;
    entry.read = false;
    _inbox.push_back(entry);
    return entry.id;
}

std::vector<ChatInboxEntry> OctiBotConversationEngine::getUnreadInbox() const {
    std::lock_guard<std::mutex> lock(_inboxMutex);
    std::vector<ChatInboxEntry> res;
    for (const auto &entry : _inbox) {
        if (!entry.read) {
            res.push_back(entry);
        }
    }
    return res;
}

std::vector<ChatInboxEntry> OctiBotConversationEngine::getAllInbox() const {
    std::lock_guard<std::mutex> lock(_inboxMutex);
    return _inbox;
}

void OctiBotConversationEngine::markInboxRead(std::uint64_t upToId) {
    std::lock_guard<std::mutex> lock(_inboxMutex);
    for (auto &entry : _inbox) {
        if (entry.id <= upToId) {
            entry.read = true;
        }
    }
}

void OctiBotConversationEngine::clearInbox() {
    std::lock_guard<std::mutex> lock(_inboxMutex);
    _inbox.clear();
}

std::string OctiBotConversationEngine::formatLobbyGreeting(const std::string &myHandle) const {
    return _greetingLobby;
}

std::string OctiBotConversationEngine::formatGameStartGreeting(const std::string &myHandle,
                                                              const std::string &opponentHandle) const {
    return _greetingGameStart;
}

std::string OctiBotConversationEngine::formatGameOverChat(const std::string &result,
                                                         const std::string &myHandle) const {
    if (result == "WIN") {
        return _chatGameOverWin;
    }
    if (result == "LOSS") {
        return _chatGameOverLoss;
    }
    return _chatGameOverDraw;
}

ChatCommandResult OctiBotConversationEngine::processLobbyMessage(
    const LobbyChatEntry &msg,
    const std::string &myHandle,
    const std::string &ownerHandle,
    OctiBotPolicyEngine &policy,
    OctiBotConfigStore &config,
    const std::string &currentRoomId,
    const std::string &currentPhase,
    int wins, int losses, int draws,
    const std::string &description,
    bool allowPublicEvalChat,
    const std::string &lastTelemetryStr) {

    ChatCommandResult result;

    // Prevent bot-to-bot chat loops: ignore bots and messages from self
    if (msg.isRobot || caseInsensitiveEqual(msg.handle, myHandle)) {
        return result;
    }

    bool isOwner = caseInsensitiveEqual(msg.handle, ownerHandle);

    // Push all incoming human messages into conversation inbox
    pushInbox("lobby", "", msg.handle, msg.playerUuid, isOwner, msg.text, msg.createdAtMs);

    std::string text = trim(msg.text);
    std::string prefix1 = "@" + toLower(myHandle) + ":";
    std::string prefix2 = "@" + toLower(myHandle);

    std::string cmd;
    std::string lowerText = toLower(text);
    if (lowerText.rfind(prefix1, 0) == 0) {
        cmd = trim(text.substr(prefix1.size()));
    } else if (lowerText.rfind(prefix2, 0) == 0) {
        cmd = trim(text.substr(prefix2.size()));
    } else {
        return result; // Not addressed to this robot
    }

    return executeCommand(cmd, msg.handle, msg.playerUuid, isOwner,
                          myHandle, ownerHandle, policy, config,
                          currentRoomId, currentPhase,
                          wins, losses, draws, description,
                          allowPublicEvalChat, lastTelemetryStr);
}

ChatCommandResult OctiBotConversationEngine::processGameMessage(
    const std::string &senderHandle,
    const std::string &senderUuid,
    bool isRobot,
    const std::string &text,
    const std::string &myHandle,
    const std::string &ownerHandle,
    OctiBotPolicyEngine &policy,
    OctiBotConfigStore &config,
    const std::string &currentRoomId,
    const std::string &currentPhase,
    int wins, int losses, int draws,
    const std::string &description,
    bool allowPublicEvalChat,
    const std::string &lastTelemetryStr) {

    ChatCommandResult result;
    if (isRobot || caseInsensitiveEqual(senderHandle, myHandle)) {
        return result;
    }

    bool isOwner = caseInsensitiveEqual(senderHandle, ownerHandle);

    pushInbox("game", currentRoomId, senderHandle, senderUuid, isOwner, text, 0);

    std::string trimmed = trim(text);
    std::string prefix1 = "@" + toLower(myHandle) + ":";
    std::string prefix2 = "@" + toLower(myHandle);

    std::string cmd;
    std::string lowerText = toLower(trimmed);
    if (lowerText.rfind(prefix1, 0) == 0) {
        cmd = trim(trimmed.substr(prefix1.size()));
    } else if (lowerText.rfind(prefix2, 0) == 0) {
        cmd = trim(trimmed.substr(prefix2.size()));
    } else {
        // In game chat, recognize short commands without @bot prefix
        std::string lowerTrim = toLower(trimmed);
        if (lowerTrim == "help" || lowerTrim == "info" ||
            lowerTrim == "stats" || lowerTrim == "eval") {
            cmd = trimmed;
        } else {
            return result;
        }
    }

    return executeCommand(cmd, senderHandle, senderUuid, isOwner,
                          myHandle, ownerHandle, policy, config,
                          currentRoomId, currentPhase,
                          wins, losses, draws, description,
                          allowPublicEvalChat, lastTelemetryStr);
}

ChatCommandResult OctiBotConversationEngine::executeCommand(
    const std::string &cmdLine,
    const std::string &senderHandle,
    const std::string &senderUuid,
    bool isOwner,
    const std::string &myHandle,
    const std::string &ownerHandle,
    OctiBotPolicyEngine &policy,
    OctiBotConfigStore &config,
    const std::string &currentRoomId,
    const std::string &currentPhase,
    int wins, int losses, int draws,
    const std::string &description,
    bool allowPublicEvalChat,
    const std::string &lastTelemetryStr) {

    ChatCommandResult result;
    std::string trimmed = trim(cmdLine);
    if (trimmed.empty()) {
        return result;
    }

    std::stringstream ss(trimmed);
    std::string verb;
    ss >> verb;
    std::string verbLower = toLower(verb);

    std::string rest;
    std::getline(ss, rest);
    rest = trim(rest);

    if (verbLower == "help" || verbLower == "info") {
        result.action = ChatCommandActionType::REPLY_ONLY;
        result.replyText = "[" + myHandle + "] " + description +
            " | Owner: " + ownerHandle +
            " | Record: " + std::to_string(wins) + "W/" +
            std::to_string(losses) + "L/" + std::to_string(draws) + "D";
        return result;
    }

    if (verbLower == "play" || verbLower == "join") {
        if (policy.isBanned(senderHandle)) {
            result.action = ChatCommandActionType::REPLY_ONLY;
            result.replyText = "Cannot join: user is banned.";
            return result;
        }
        result.action = ChatCommandActionType::JOIN_SENDER_TABLE;
        result.replyText = "Joining table...";
        return result;
    }

    if (verbLower == "eval" || verbLower == "stats") {
        result.action = ChatCommandActionType::REPLY_ONLY;
        if (!allowPublicEvalChat && !isOwner) {
            result.replyText = "Move telemetry is currently disabled.";
        } else if (lastTelemetryStr.empty()) {
            result.replyText = "No search telemetry recorded yet.";
        } else {
            result.replyText = lastTelemetryStr;
        }
        return result;
    }

    // Owner-only commands
    if (!isOwner) {
        result.action = ChatCommandActionType::REPLY_ONLY;
        result.replyText = "Unauthorized: '" + verb + "' is an owner-only command.";
        return result;
    }

    if (verbLower == "status") {
        result.action = ChatCommandActionType::REPLY_ONLY;
        std::string roomStr = currentRoomId.empty() ? "lobby" : currentRoomId;
        std::string phaseStr = currentPhase.empty() ? "IDLE" : currentPhase;
        result.replyText = "[" + myHandle + " Status] room=" + roomStr +
            " phase=" + phaseStr +
            " record=" + std::to_string(wins) + "W/" +
            std::to_string(losses) + "L/" + std::to_string(draws) + "D";
        return result;
    }

    if (verbLower == "host") {
        result.action = ChatCommandActionType::HOST_TABLE;
        result.mode = rest.empty() ? "2P" : rest;
        result.replyText = "Hosting " + result.mode + " table...";
        return result;
    }

    if (verbLower == "joinroom") {
        result.action = ChatCommandActionType::JOIN_ROOM;
        result.targetRoomId = rest;
        result.replyText = "Joining room " + rest + "...";
        return result;
    }

    if (verbLower == "leave") {
        result.action = ChatCommandActionType::LEAVE_ROOM;
        result.replyText = "Leaving table...";
        return result;
    }

    if (verbLower == "friend") {
        result.action = ChatCommandActionType::REPLY_ONLY;
        if (!rest.empty() && rest[0] == '+') {
            std::string h = trim(rest.substr(1));
            policy.addFriend(h);
            result.replyText = "Added friend: " + h;
        } else if (!rest.empty() && rest[0] == '-') {
            std::string h = trim(rest.substr(1));
            policy.removeFriend(h);
            result.replyText = "Removed friend: " + h;
        } else {
            result.replyText = "Usage: friend +<handle> or friend -<handle>";
        }
        return result;
    }

    if (verbLower == "ban") {
        result.action = ChatCommandActionType::REPLY_ONLY;
        if (!rest.empty() && rest[0] == '+') {
            std::string h = trim(rest.substr(1));
            policy.addBan(h);
            result.replyText = "Added ban: " + h;
        } else if (!rest.empty() && rest[0] == '-') {
            std::string h = trim(rest.substr(1));
            policy.removeBan(h);
            result.replyText = "Removed ban: " + h;
        } else {
            result.replyText = "Usage: ban +<handle> or ban -<handle>";
        }
        return result;
    }

    if (verbLower == "set") {
        size_t eqPos = rest.find('=');
        if (eqPos == std::string::npos) {
            result.action = ChatCommandActionType::REPLY_ONLY;
            result.replyText = "Usage: set <key>=<value>";
            return result;
        }
        std::string key = trim(rest.substr(0, eqPos));
        std::string val = trim(rest.substr(eqPos + 1));
        config.set(key, val);
        policy.configure(config);

        result.action = ChatCommandActionType::CONFIG_CHANGED;
        result.configKey = key;
        result.configValue = val;
        result.replyText = "Configuration updated: " + key + " = " + val;
        return result;
    }

    result.action = ChatCommandActionType::REPLY_ONLY;
    result.replyText = "Unknown command '" + verb + "'. Type help for commands.";
    return result;
}

} // namespace octi

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set ft=cpp et ts=4 sts=4 sw=4:
 */
