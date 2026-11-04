/*
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_POLICY_HXX
#define OCTIBOT_POLICY_HXX

#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "OctiBotConfig.hxx"
#include "OctiNetworkClient.hxx"

namespace octi {

enum class ChatCommandActionType {
    NONE,
    REPLY_ONLY,
    JOIN_SENDER_TABLE,
    HOST_TABLE,
    JOIN_ROOM,
    LEAVE_ROOM,
    CONFIG_CHANGED
};

struct ChatCommandResult {
    ChatCommandActionType action = ChatCommandActionType::NONE;
    std::string replyText;
    std::string targetRoomId;
    std::string mode = "2P";
    std::string configKey;
    std::string configValue;
};

struct ChatInboxEntry {
    std::uint64_t id = 0;
    std::string channel;
    std::string roomId;
    std::string senderHandle;
    std::string senderUuid;
    bool isOwner = false;
    std::string text;
    std::int64_t timestampMs = 0;
    bool read = false;
};

class OctiBotPolicyEngine {
public:
    OctiBotPolicyEngine() = default;
    explicit OctiBotPolicyEngine(const OctiBotConfigStore &config);

    void configure(const OctiBotConfigStore &config);

    bool isFriend(const std::string &handle) const;
    bool isBanned(const std::string &handle) const;
    void addFriend(const std::string &handle);
    void removeFriend(const std::string &handle);
    void addBan(const std::string &handle);
    void removeBan(const std::string &handle);

    const std::set<std::string> &friends() const { return _friendList; }
    const std::set<std::string> &banned() const { return _banList; }

    bool isTableCompatible(const TableSummaryInfo &table,
                           const std::string &ownerHandle,
                           const std::string &myHandle) const;

    std::vector<TableSummaryInfo> rankCompatibleTables(
        const std::vector<TableSummaryInfo> &tables,
        const std::string &ownerHandle,
        const std::string &myHandle) const;

    bool shouldKickSeatedPlayer(const TableSeatInfo &seat,
                                const std::string &ownerHandle) const;

    bool shouldAcceptProposal(const GameProposalInfo &proposal,
                              const std::string &proposerHandle,
                              int currentEvalScore,
                              int consecutiveRematches) const;

    CreateTableRequest buildHostTableRequest() const;
    MatchmakingRequest buildMatchmakingRequest() const;

    const std::string &tableBehavior() const { return _tableBehavior; }
    void setTableBehavior(const std::string &b) { _tableBehavior = b; }

    bool friendsOnly() const { return _friendsOnly; }
    void setFriendsOnly(bool fo) { _friendsOnly = fo; }

    int claimForfeitDelaySec() const { return _claimForfeitDelaySec; }
    int minMoveThinkMs() const { return _minMoveThinkMs; }
    int maxConsecutiveRematches() const { return _maxConsecutiveRematches; }

private:
    mutable std::mutex _mutex;
    std::string _tableBehavior = "auto";
    std::set<std::string> _acceptModes = {"2P", "4P", "OFK"};
    std::string _acceptBases = "any";
    std::string _acceptSuperProngs = "any";
    std::string _acceptEdgeless = "any";
    std::string _acceptTimeControl = "any";
    int _minInitialTimeSec = 0;
    int _maxInitialTimeSec = 14400;
    int _minIncrementSec = 0;
    std::string _acceptTakeback = "any";

    std::set<std::string> _friendList;
    std::set<std::string> _banList;
    bool _friendsOnly = false;
    std::string _opponentFilter = "any";

    int _acceptDrawEvalThreshold = -1500;
    std::string _acceptUndo = "friends_only";
    std::string _acceptPause = "always";
    std::string _acceptRematch = "always";
    int _maxConsecutiveRematches = 5;
    int _claimForfeitDelaySec = 10;
    int _minMoveThinkMs = 0;

    // Hosting defaults
    std::string _hostMode = "2P";
    int _hostBases = 3;
    bool _hostSuperProngs = false;
    bool _hostEdgeless = false;
    bool _hostTimeControlEnabled = false;
    std::int64_t _hostInitialMs = 600000;
    std::int64_t _hostIncrementMs = 5000;
};

class OctiBotConversationEngine {
public:
    OctiBotConversationEngine() = default;
    explicit OctiBotConversationEngine(const OctiBotConfigStore &config);

    void configure(const OctiBotConfigStore &config);

    std::uint64_t pushInbox(const std::string &channel,
                            const std::string &roomId,
                            const std::string &senderHandle,
                            const std::string &senderUuid,
                            bool isOwner,
                            const std::string &text,
                            std::int64_t timestampMs);

    std::vector<ChatInboxEntry> getUnreadInbox() const;
    std::vector<ChatInboxEntry> getAllInbox() const;
    void markInboxRead(std::uint64_t upToId);
    void clearInbox();

    ChatCommandResult processLobbyMessage(
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
        const std::string &lastTelemetryStr);

    ChatCommandResult processGameMessage(
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
        const std::string &lastTelemetryStr);

    std::string formatLobbyGreeting(const std::string &myHandle) const;
    std::string formatGameStartGreeting(const std::string &myHandle,
                                        const std::string &opponentHandle) const;
    std::string formatGameOverChat(const std::string &result,
                                   const std::string &myHandle) const;

private:
    ChatCommandResult executeCommand(
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
        const std::string &lastTelemetryStr);

    mutable std::mutex _inboxMutex;
    std::uint64_t _nextInboxId = 1;
    std::vector<ChatInboxEntry> _inbox;

    std::string _greetingLobby = "Hello! I am an automated OCTI robot player.";
    std::string _greetingGameStart = "Good luck and have fun!";
    std::string _chatGameOverWin = "Good game!";
    std::string _chatGameOverLoss = "Good game, well played!";
    std::string _chatGameOverDraw = "Good game, well fought!";
};

} // namespace octi

#endif /* OCTIBOT_POLICY_HXX */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set ft=cpp et ts=4 sts=4 sw=4:
 */
