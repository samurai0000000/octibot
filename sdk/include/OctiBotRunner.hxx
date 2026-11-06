/*
 * OctiBotRunner.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_RUNNER_HXX
#define OCTIBOT_RUNNER_HXX

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "OctiBoard.hxx"
#include "OctiBotAgent.hxx"
#include "OctiBotConfig.hxx"
#include "OctiBotPolicy.hxx"
#include "OctiNetworkClient.hxx"

namespace octi {

struct BotLogEntry {
    std::uint64_t seq = 0;
    std::int64_t timestampMs = 0;
    std::string line;
};

struct BotMoveHistoryEntry {
    int ply = 0;
    std::string move;
    int depthReached = 0;
    std::uint64_t nodesVisited = 0;
    int evalScore = 0;
    std::int64_t elapsedMs = 0;
    std::int64_t timestampMs = 0;
};

struct BotConversationLogEntry {
    std::uint64_t seq = 0;
    std::int64_t timestampMs = 0;
    std::string channel;      // "lobby" or "game" or "dashboard"
    std::string roomId;
    std::string senderHandle;
    std::string seenText;
    std::string replyText;
    std::string replySource;  // "ai_cli:agy", "command", "greeting", "seen"
};

struct BotMatchOverride {
    bool active = false;
    std::string targetPlayerHandle;
    std::string targetRoomId;
    bool allowRobots = false;
    std::string hostMode;
    int hostBases = 0;
    int remainingGames = 1;
};

class OctiBotRunner {
public:
    static constexpr std::size_t kMaxLogLines = 5000;
    static constexpr std::size_t kMaxMoveHistory = 50;
    static constexpr std::size_t kMaxConversationHistory = 500;

    OctiBotRunner(std::unique_ptr<OctiBotAgent> agent,
                  const OctiBotConfigStore &config,
                  const std::string &customEndpoint = "");
    ~OctiBotRunner();

    bool connectAndAuthenticate(std::string &errorOut);
    void start();
    void run();
    void stop();

    bool isRunning() const { return _running; }
    bool isSuperseded() const { return _superseded; }
    bool isLobbyOnly() const { return _lobbyOnly; }
    static OctiBotRunner *findActiveRunnerByHandle(const std::string &handle);

    // Active chat sender tracking
    std::string activeChatSenderHandle() const;
    bool activeChatSenderIsOwner() const;
    void setActiveChatSender(const std::string &handle, bool isOwner);

    // Multi-chunk chat reply splitting
    static std::vector<std::string> splitChatReplyIntoChunks(
        const std::string &text,
        std::size_t maxChunkBytes = 450,
        std::size_t maxChunks = 3);

    bool waitForEvent(int timeoutMs, std::string &outReason);
    void notifyEvent(const std::string &reason);

    // Getters for MCP server, Web Dashboard & inspection
    const BotIdentity &identity() const { return _identity; }
    const OctiBotConfigStore &config() const { return _config; }
    OctiBotConfigStore &config() { return _config; }
    OctiBotPolicyEngine &policyEngine() { return _policy; }
    OctiBotConversationEngine &conversationEngine() { return _conversation; }
    OctiNetworkClient &networkClient() { return _client; }
    OctiBotAgent *agent() { return _agent.get(); }

    std::string currentRoomId() const;
    std::string currentPhase() const;
    int currentSeat() const;
    int turnSeat() const;
    int plyCount() const;
    std::string currentFen() const;
    std::string renderAsciiBoard() const;
    std::vector<GameSeatInfo> currentSeats() const;
    std::int64_t currentTtlRemainingMs() const;

    const BotMoveTelemetry &lastTelemetry() const { return _lastTelemetry; }
    std::string moveControl() const { return _moveControl; }
    void setMoveControl(const std::string &mc) { _moveControl = mc; }
    bool publishMoveTelemetry() const { return _publishMoveTelemetry; }
    void setPublishMoveTelemetry(bool val) { _publishMoveTelemetry = val; }
    bool announceMoveSearchChat() const { return _announceMoveSearchChat; }
    void setAnnounceMoveSearchChat(bool val) { _announceMoveSearchChat = val; }
    int verbosity() const { return _verbosity; }
    void setVerbosity(int level) { _verbosity = level; }
    void logMsg(int minLevel, const std::string &msg) const;

    // 5,000-line stdout ring buffer
    void appendStdoutLine(const std::string &line, bool printToConsole = true);
    std::vector<BotLogEntry> getLogsAfter(std::uint64_t afterSeq,
                                         std::uint64_t *outOldestSeq = nullptr,
                                         std::uint64_t *outLatestSeq = nullptr) const;

    // Search telemetry & move history
    void recordMoveHistory(int ply, const std::string &move, const BotMoveTelemetry &telem);
    std::vector<BotMoveHistoryEntry> getMoveHistory() const;
    bool isSearching() const { return _isSearching.load(); }
    int liveDepth() const { return _liveDepth.load(); }
    std::uint64_t liveNodes() const { return _liveNodes.load(); }
    int liveScore() const { return _liveScore.load(); }
    void updateSearchProgress(bool searching, int depth = 0, std::uint64_t nodes = 0, int score = 0);

    // Conversation history
    std::uint64_t recordConversation(const std::string &channel,
                                     const std::string &roomId,
                                     const std::string &senderHandle,
                                     const std::string &seenText,
                                     const std::string &replyText,
                                     const std::string &replySource);
    void updateConversationReply(std::uint64_t seq,
                                 const std::string &replyText,
                                 const std::string &replySource);
    std::vector<BotConversationLogEntry> getConversationHistory() const;

    // MCP server port registration
    void setMcpServerPort(int port) { _mcpServerPort = port; }
    int mcpServerPort() const { return _mcpServerPort.load(); }

    // Dynamic config hot-reload & disk persistence
    void setConfigFilePath(const std::string &path) { _configFilePath = path; }
    const std::string &configFilePath() const { return _configFilePath; }
    bool applyConfigUpdates(const std::map<std::string, std::string> &updates,
                            bool saveToDisk,
                            std::string &errorOut);

    bool submitAgentMove(const std::string &moveNotation, std::string &errorOut);
    bool executeGameAction(const std::string &action, const std::string &arg, std::string &errorOut);
    bool joinRoomById(const std::string &roomId, int &outSeat, std::string &errorOut);
    bool hostRoomWithRequest(const CreateTableRequest &req, std::string &outRoomId, int &outSeat, std::string &errorOut);
    bool leaveRoomById(const std::string &roomId, std::string &errorOut);
    void requestLeaveAfterChat(const std::string &roomId, const std::string &replacementBotHandle = "");
    bool takePendingLeaveAfterChat(std::string &outRoomId, std::string &outReplacementBotHandle);

    BotMatchOverride matchOverride() const;
    bool applyMatchOverride(const BotMatchOverride &ov, std::string &outSummary, std::string &errorOut);
    void clearMatchOverride();
    void dispatchOwnerPrompt(const std::string &promptText,
                             const std::string &channel = "dashboard",
                             const std::string &roomId = "");

private:
    void loopStep();
    void pollRecentLobbyChat();
    void handleLobbySseEvent(const std::string &eventType, const std::string &data);
    void handleGameSseEvent(const std::string &eventType, const std::string &data);
    void playEngineTurn();
    void dispatchAiChatReply(const std::string &channel,
                             const std::string &roomId,
                             const std::string &senderHandle,
                             const std::string &messageText,
                             bool isTrustedOwner = false);
    void dispatchAiProactiveEvent(const std::string &eventKind,
                                  const std::string &roomId,
                                  const std::string &opponentHandle,
                                  const std::string &eventSummary,
                                  bool waitForCompletion = false);

    std::unique_ptr<OctiBotAgent> _agent;
    OctiBotConfigStore _config;
    std::string _configFilePath;
    OctiNetworkClient _client;
    OctiBotPolicyEngine _policy;
    OctiBotConversationEngine _conversation;
    BotIdentity _identity;

    std::atomic<bool> _running{false};
    std::atomic<bool> _superseded{false};
    std::unique_ptr<std::thread> _workerThread;

    std::string _moveControl = "engine"; // "engine" or "agent"
    bool _publishMoveTelemetry = false;
    bool _announceMoveSearchChat = true;
    bool _lobbyOnly = false;
    mutable std::mutex _activeChatSenderMutex;
    std::string _activeChatSenderHandle;
    bool _activeChatSenderIsOwner = false;
    int _agentMoveTimeoutMs = 15000;
    int _verbosity = 3;
    std::atomic<bool> _aiChatBusy{false};
    std::atomic<int> _mcpServerPort{0};

    // Live search telemetry
    std::atomic<bool> _isSearching{false};
    std::atomic<int> _liveDepth{0};
    std::atomic<std::uint64_t> _liveNodes{0};
    std::atomic<int> _liveScore{0};

    // Rolling 50-move history
    mutable std::mutex _historyMutex;
    std::deque<BotMoveHistoryEntry> _moveHistory;

    // Rolling 500-conversation history
    mutable std::mutex _conversationMutex;
    std::deque<BotConversationLogEntry> _conversationHistory;
    std::uint64_t _nextConversationSeq = 1;

    // 5,000-line stdout ring buffer
    mutable std::mutex _logMutex;
    std::deque<BotLogEntry> _logBuffer;
    std::uint64_t _nextLogSeq = 1;

    mutable std::mutex _overrideMutex;
    mutable std::mutex _stateMutex;
    std::string _currentRoomId;
    std::string _currentPhase = "IDLE";
    int _currentSeat = 0;
    GameViewData _activeGameView;
    std::unique_ptr<OctiBoard> _activeBoard;
    BotMoveTelemetry _lastTelemetry;
    std::string _pendingAgentMove;
    bool _hasAgentMove = false;
    BotMatchOverride _matchOverride;
    std::string _pendingLeaveAfterChatRoomId;
    std::string _pendingLeaveReplacementBot;
    std::string _excludedRoomId;
    std::int64_t _excludedRoomUntilMs = 0;
    std::string _excludedPlayerHandle;
    std::int64_t _excludedPlayerUntilMs = 0;
    std::int64_t _lastProcessedLobbyChatSeq = 0;
    std::int64_t _lastLobbyChatPollMs = 0;

    std::mutex _eventMutex;
    std::condition_variable _eventCv;
    bool _hasPendingEvent = false;
    std::string _lastEventReason;
    int _consecutiveRematches = 0;
    int _lastLoggedPlyCount = -1;
    std::string _gameOverHandledRoomId;
    std::atomic<std::int64_t> _postGameSilenceStartMs{0};
    int _lastProactiveCommentPly = -100;
    int _proactiveCommentsInGame = 0;
};

#if !defined(OCTIBOT_MINIMAL)
class OctiBotMcpServer;
#endif

class OctiBotFleetRunner {
public:
    OctiBotFleetRunner() = default;
    ~OctiBotFleetRunner();

    bool startProfiles(const OctiBotConfigStore &config,
                       const std::vector<std::string> &profiles,
                       const std::string &customEndpoint = "");
    bool startAllProfiles(const OctiBotConfigStore &config,
                          const std::string &customEndpoint = "");
    bool startClones(const OctiBotConfigStore &config,
                     const std::string &baseProfile,
                     int numInstances,
                     const std::string &customEndpoint = "");

    void stop();
    void waitForAll();

    const std::vector<std::unique_ptr<OctiBotRunner>> &runners() const { return _runners; }
#if !defined(OCTIBOT_MINIMAL)
    const std::vector<std::unique_ptr<OctiBotMcpServer>> &mcpServers() const { return _mcpServers; }
#endif

private:
    std::vector<std::unique_ptr<OctiBotRunner>> _runners;
#if !defined(OCTIBOT_MINIMAL)
    std::vector<std::unique_ptr<OctiBotMcpServer>> _mcpServers;
#endif
};

int runFleetCli(int argc, char **argv);

} // namespace octi

#endif /* OCTIBOT_RUNNER_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
