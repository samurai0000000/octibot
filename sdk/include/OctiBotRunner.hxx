/*
 * OctiBotRunner.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_RUNNER_HXX
#define OCTIBOT_RUNNER_HXX

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include "OctiBoard.hxx"
#include "OctiBotAgent.hxx"
#include "OctiBotConfig.hxx"
#include "OctiBotPolicy.hxx"
#include "OctiNetworkClient.hxx"

namespace octi {

enum class ActiveTurnTrust : std::uint8_t {
    UntrustedDefault = 0,
    ProactiveEvent = 1,
    UntrustedChat = 2,
    TrustedOwner = 3
};

#if !defined(OCTIBOT_MINIMAL)
class OctiPersistentCliSession;
#endif

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
    bool hasHostSuperProngs = false;
    bool hostSuperProngs = false;
    bool hasHostEdgeless = false;
    bool hostEdgeless = false;
    int preferredSeat = 0;
    int remainingGames = 1;
};

enum class LeaveRoomMode {
    NONE,
    WAITING_LEFT,
    PLY0_ABORTED,
    FINISHED_LEFT,
    RESIGNED_AND_LEFT,
    PURGED,
    REFUSED_WOULD_RESIGN
};

struct LeaveRoomResult {
    bool ok = false;
    bool resigned = false;
    bool stoodUpPly0 = false;
    bool refusedWouldResign = false;
    LeaveRoomMode mode = LeaveRoomMode::NONE;
    std::string phase;
    int plyCount = 0;
    std::string error;
};

enum class PendingSseActionType {
    APPLY_SIBLING_LOBBY_OVERRIDE,
    APPLY_SIBLING_GAME_OVERRIDE,
    APPLY_COMMAND_JOIN_OVERRIDE,
    APPLY_COMMAND_PLAY_OVERRIDE,
    APPLY_OWNER_DIRECTIVE_OVERRIDE,
    LEAVE_INCOMPATIBLE_WAITING_ROOM,
    HANDLE_GAME_PROPOSAL,
    EMIT_VARIANT_DECLINE,
    EMIT_RESOURCE_MAXED_DECLINE,
    LEAVE_ROOM
};

struct RunnerResourceStatus {
    bool admissionEnabled = false;
    bool admissionMaxedOut = false;
    bool autonomousPaused = false;
    bool oversubscribed = false;
    bool pinningAllowed = true;
    std::size_t assignedSlotOffset = 0;
    std::uint32_t physicalCores = 0;
    std::uint32_t logicalCores = 0;
    std::uint32_t cpuCoreBudget = 0;
    std::uint32_t usedCpuCores = 0;
    int gpuCount = 0;
    int gpuStreamBudget = 0;
    int usedGpuStreams = 0;
    std::string gpuProviderName = "none";
    std::string gpuDeviceName = "none";
    std::uint64_t totalRamMiB = 0;
    std::uint64_t availableRamMiB = 0;
    std::uint64_t ramBudgetMiB = 0;
    std::uint64_t usedRamMiB = 0;
    std::uint64_t gpuVramMiB = 0;
    std::uint64_t vramBudgetMiB = 0;
    std::uint64_t usedVramMiB = 0;
    int activeTableCount = 0;
    BotResourceRequirements selfRequirements;
};

struct PendingSseAction {
    PendingSseActionType type = PendingSseActionType::LEAVE_ROOM;
    BotMatchOverride overrideReq;
    GameProposalInfo proposal;
    OctiVariantSpec declineSpec;
    std::string declineReason;
    std::string declineChannel;
    bool senderIsRobot = false;
    std::string senderHandle;
    std::string originalText;
    std::string roomId;
    std::string replyText;
    std::string parsedPlayer;
    std::string parsedRoom;
};

class OctiBotRunner {
public:
    static constexpr std::size_t kMaxLogLines = 5000;
    static constexpr std::size_t kMaxMoveHistory = 50;
    static constexpr std::size_t kMaxConversationHistory = 500;

    OctiBotRunner(std::unique_ptr<OctiBotAgent> agent,
                  const OctiBotConfigStore &config,
                  const std::string &customEndpoint = "");
    virtual ~OctiBotRunner();

    bool connectAndAuthenticate(std::string &errorOut);
    bool reclaimIfUnused(std::string &errorOut);
    int reclaimSupersededIntervalMs() const;
    bool hasReusableNamedToken() const;
    std::int64_t supersededAtMs() const { return _supersededAtMs.load(); }
    void start();
    void run();
    void stop();

    bool isRunning() const { return _running; }
    bool isSuperseded() const { return _superseded; }
    bool isLobbyOnly() const { return _lobbyOnly; }
    static OctiBotRunner *findActiveRunnerByHandle(const std::string &handle);
    static std::vector<OctiBotRunner *> listActiveRunners();
    static void wakeAllActiveRunners(bool markRoomDirty, bool markLobbyDirty);
    static std::string resolvePrimarySdkBotHandle();
    static std::string resolveCanonicalBotHandle(const std::string &rawHandle);

    // Active chat sender & turn trust tracking
    std::string activeChatSenderHandle() const;
    bool activeChatSenderIsOwner() const;
    ActiveTurnTrust activeTurnTrust() const;
    void setActiveTurnTrust(ActiveTurnTrust trust);
    bool hasOwnerToolPrivilege() const;
    void setActiveChatSender(const std::string &handle, bool isOwner);

    // Table-scoped persistent CLI session lifecycle & inspection
    void closeTableCliSession();
    long tableCliSessionPid() const;
    int tableCliSessionTurnCount() const;
    bool tableCliSessionBootstrapSent() const;
    std::string tableCliSessionStderrTail() const;

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
    const OctiNetworkClient &networkClient() const { return _client; }
    OctiBotAgent *agent() { return _agent.get(); }

    std::string currentRoomId() const;
    std::string currentPhase() const;
    int currentSeat() const;
    int turnSeat() const;
    int plyCount() const;
    std::string currentFen() const;
    std::string renderAsciiBoard() const;
    std::vector<GameSeatInfo> currentSeats() const;
    GameViewData activeGameView() const;
    void updateActiveGameView(const GameViewData &view);
    std::int64_t currentTtlRemainingMs() const;

    void setRoomChatMuted(const std::string &roomId, bool muted);
    bool isRoomChatMuted(const std::string &roomId) const;
    virtual bool shouldReplyToGameChat(const std::string &roomId,
                                       const std::string &senderHandle,
                                       const std::string &messageText,
                                       bool senderIsOwner = false,
                                       bool senderIsRobot = false,
                                       const std::string &senderUuid = "") const;
    virtual bool shouldEmitProactiveChat(const std::string &roomId,
                                         const std::string &eventKind = "") const;

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
    std::map<std::string, std::string> buildEffectiveConfigMap() const;
    bool applyConfigUpdates(const std::map<std::string, std::string> &updates,
                            bool saveToDisk,
                            std::string &errorOut);

    bool submitAgentMove(const std::string &moveNotation, std::string &errorOut);
    bool executeGameAction(const std::string &action, const std::string &arg, std::string &errorOut);
    bool joinRoomById(const std::string &roomId, int &outSeat, std::string &errorOut);
    bool joinRoomById(const std::string &roomId, int preferredSeat, int &outSeat, std::string &errorOut);
    bool hostRoomWithRequest(const CreateTableRequest &req, std::string &outRoomId, int &outSeat, std::string &errorOut);
    LeaveRoomResult leaveRoomWithOptions(const std::string &rawRoomId,
                                         bool allowResign,
                                         bool preferPly0Abort = true);
    bool leaveRoomById(const std::string &roomId, std::string &errorOut);
    void requestLeaveAfterChat(const std::string &roomId,
                               const std::string &replacementBotHandle = "",
                               bool allowResign = false);
    bool takePendingLeaveAfterChat(std::string &outRoomId,
                                   std::string &outReplacementBotHandle);
    bool takePendingLeaveAfterChat(std::string &outRoomId,
                                   std::string &outReplacementBotHandle,
                                   bool &outAllowResign);

    BotMatchOverride matchOverride() const;
    bool applyMatchOverride(const BotMatchOverride &ov, std::string &outSummary, std::string &errorOut);
    void clearMatchOverride();
    bool checkAgentSupportsVariant(const OctiVariantSpec &spec, std::string &outReason) const;
    bool checkPolicyAndAgentSupportsVariant(const OctiVariantSpec &spec, std::string &outReason) const;
    static std::string selectAutoBotHandleForVariant(const OctiVariantSpec &spec,
                                                     const OctiBotRunner *excludeRunner = nullptr,
                                                     const std::string &targetRoomId = "");
    bool leaveAndSummonReplacementBot(const std::string &roomId,
                                      const std::string &replacementBotHandle,
                                      const std::string &opponentHandle,
                                      std::string &outSelectedBot,
                                      std::string &errorOut,
                                      bool allowResign = false,
                                      LeaveRoomResult *outLeaveResult = nullptr);
    bool emitVariantDeclineMessage(const std::string &channel,
                                   const std::string &roomId,
                                   const std::string &senderHandle,
                                   bool senderIsRobot,
                                   const OctiVariantSpec &spec,
                                   const std::string &agentReason = "",
                                   bool waitForCompletion = false);
    bool emitResourceMaxedDeclineMessage(const std::string &channel,
                                         const std::string &roomId,
                                         const std::string &senderHandle,
                                         bool senderIsRobot);

    // Virtual resource admission & status query hooks (upstream-clean permissive defaults)
    virtual bool canCommitToMatch(const std::string &roomId,
                                  const std::string &actionKind,
                                  std::string &outDeclineReason) const;
    virtual bool canSelectForMatch(const std::string &targetRoomId) const;
    virtual std::size_t assignedCpuCoreSlotOffset() const;
    virtual bool shouldAllowPinning() const;
    virtual std::string resourceMaxedDeclineText() const;
    virtual RunnerResourceStatus resourceStatusSnapshot() const;
    void dispatchOwnerPrompt(const std::string &promptText,
                             const std::string &channel = "dashboard",
                             const std::string &roomId = "");
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
    bool isAiChatBusy() const { return _aiChatBusy.load(); }
    void setThinkMumbleIntervalForTest(int intervalMs);
    int effectiveThinkMumbleIntervalMs() const;
    int inGameMumblesSent() const { return _inGameMumblesSent.load(); }
    void resetPerGameProactiveCounters();
    void resetRateLimitsForTest();

protected:
    virtual bool onBeforeCommitMatch(const std::string &roomId,
                                     std::string &outDeclineReason);
    virtual void onForceAdoptInProgressMatch(const std::string &roomId);
    virtual void onCommittedRoomBound(const std::string &oldKey,
                                      const std::string &roomId);
    virtual void onMatchReleased(const std::string &roomId);
    virtual bool shouldStandUpAndUnhostWaitingRoom(const std::string &roomId) const;
    virtual bool isAutonomousMatchmakingPaused() const;
    virtual void onBeforeComputeMove();
    virtual std::string buildResourcePromptContext() const;

    bool standUpFromWaitingRoomWithoutResign(const std::string &rawRoomId,
                                             bool &outTransitionedToInProgress);

private:
    bool joinRoomByIdImpl(const std::string &rawRoomId,
                          int preferredSeat,
                          int &outSeat,
                          std::string &errorOut,
                          bool forceAdopt,
                          bool commitReservation,
                          bool *outResourceMaxedOut);
    bool hostRoomWithRequestImpl(const CreateTableRequest &req,
                                 bool commitImmediately,
                                 std::string &outRoomId,
                                 int &outSeat,
                                 std::string &errorOut,
                                 bool *outResourceMaxedOut);
    bool applyMatchOverrideImpl(const BotMatchOverride &ov,
                                std::string &outSummary,
                                std::string &errorOut,
                                bool *outVariantUnsupported,
                                OctiVariantSpec *outUnsupportedSpec,
                                bool *outResourceMaxedOut = nullptr);
    void loopStep();
    void pollRecentLobbyChat();
    void enqueuePendingSseAction(PendingSseAction action);
    void drainPendingSseActions();
    void handleLobbySseEvent(const std::string &eventType, const std::string &data);
    void handleGameSseEvent(const std::string &eventType, const std::string &data);
    void playEngineTurn();
    void wakeLoop(bool markRoomDirty = false, bool markLobbyDirty = false);
    void waitForLoopWake(int maxWaitMs);
    int computeNextWaitMs() const;
    void abortAndCloseTableCliSession(bool permanentStop);
    void maybePrewarmTableCliSession(const std::string &roomId, const GameViewData &gv);
    void spawnTrackedTask(std::function<void()> fn);
    void joinAllTrackedTasks();
    bool tryAcquireAiChatBusy();
    bool waitAcquireAiChatBusy(int maxWaitMs);
    void releaseAiChatBusy();

    struct ProactiveDispatchOptions {
        bool waitForCompletion = false;
        bool requireSearchEpoch = false;
        std::uint64_t expectedSearchEpoch = 0;
        bool waitBrieflyForBusySlot = false;
        std::string chosenMove;
        BotThinkMilestoneEvent milestone;
        BotMoveTelemetry postMoveTelemetry;
    };
    void dispatchAiProactiveEventImpl(const std::string &eventKind,
                                      const std::string &roomId,
                                      const std::string &opponentHandle,
                                      const std::string &eventSummary,
                                      const ProactiveDispatchOptions &opts);
    void dispatchAiChatReplyImpl(const std::string &channel,
                                 const std::string &roomId,
                                 const std::string &senderHandle,
                                 const std::string &messageText,
                                 bool isTrustedOwner,
                                 bool alreadyHoldsBusy);
    void onSearchThinkMilestone(std::uint64_t capturedEpoch,
                                const std::string &roomId,
                                const GameViewData &gvSnap,
                                int mySeat,
                                const BotThinkMilestoneEvent &ev);
    bool dispatchPostMoveMumble(const std::string &roomId,
                                const GameViewData &gvSnap,
                                int mySeat,
                                const std::string &chosenMove,
                                const BotMoveTelemetry &telem);
    bool areAllSeatedOpponentsRobots(const std::vector<GameSeatInfo> &seats,
                                     int mySeat) const;
    bool hasPendingHumanChat() const;
    void clearPendingHumanChat();
    void clearPendingHumanChatForRoom(const std::string &rawRoomId);
    void drainPendingHumanChatIfIdle();
    bool canDispatchProactiveRateLimit(const std::string &eventKind) const;
    bool tryReserveProactiveRateLimit(const std::string &eventKind);
    bool isProactiveRateWithinCapNow(const std::string &eventKind) const;
    bool tryReserveNonOwnerChatReply();

    std::unique_ptr<OctiBotAgent> _agent;
    OctiBotConfigStore _config;
    std::string _configFilePath;
    OctiNetworkClient _client;
    OctiBotPolicyEngine _policy;
    OctiBotConversationEngine _conversation;
    BotIdentity _identity;

    std::atomic<bool> _running{false};
    std::atomic<bool> _stopping{false};
    std::atomic<bool> _superseded{false};
    std::atomic<std::int64_t> _supersededAtMs{0};
    std::unique_ptr<std::thread> _workerThread;

    std::string _moveControl = "engine"; // "engine" or "agent"
    bool _publishMoveTelemetry = false;
    bool _announceMoveSearchChat = true;
    bool _lobbyOnly = false;
    mutable std::mutex _activeChatSenderMutex;
    std::string _activeChatSenderHandle;
    bool _activeChatSenderIsOwner = false;
    ActiveTurnTrust _activeTurnTrust = ActiveTurnTrust::UntrustedDefault;
    int _agentMoveTimeoutMs = 15000;
    int _verbosity = 3;
    mutable std::mutex _aiChatBusyMutex;
    std::condition_variable _aiChatBusyCv;
    std::atomic<bool> _aiChatBusy{false};
    int _waitingOwnerChatCount = 0;
    std::atomic<int> _mcpServerPort{0};

    struct PendingHumanChatItem {
        bool active = false;
        std::string channel;
        std::string roomId;
        std::string senderHandle;
        std::string messageText;
        bool isTrustedOwner = false;
    };
    mutable std::mutex _pendingHumanChatMutex;
    PendingHumanChatItem _pendingHumanChat;

    mutable std::mutex _rateLimitMutex;
    mutable std::deque<std::int64_t> _proactiveCallTimestampsMs;
    mutable std::deque<std::int64_t> _nonOwnerChatReplyTimestampsMs;

    struct TrackedBgTask {
        std::shared_ptr<std::atomic<bool>> done;
        std::thread worker;
    };
    std::mutex _bgTasksMutex;
    std::vector<TrackedBgTask> _bgTasks;
    std::atomic<bool> _prewarmInFlight{false};
#if !defined(OCTIBOT_MINIMAL)
    mutable std::mutex _tableCliMutex;
    std::unique_ptr<OctiPersistentCliSession> _tableCliSession;
#endif

    // Live search telemetry & epoch guard
    std::atomic<bool> _isSearching{false};
    std::atomic<int> _liveDepth{0};
    std::atomic<std::uint64_t> _liveNodes{0};
    std::atomic<int> _liveScore{0};
    std::atomic<std::uint64_t> _searchTurnEpoch{0};
    std::atomic<int> _inGameMumblesSent{0};
    std::atomic<int> _thinkMumbleIntervalOverrideMsForTest{0};

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
    mutable std::mutex _declineMutex;
    std::map<std::string, std::int64_t> _variantDeclineCooldownsMs;
    std::map<std::string, std::int64_t> _resourceDeclineCooldownsMs;
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
    std::string _matchOverrideRequesterHandle;
    bool _matchOverrideRequesterIsRobot = false;
    std::string _pendingLeaveAfterChatRoomId;
    std::string _pendingLeaveReplacementBot;
    bool _pendingLeaveAllowResign = false;
    std::string _excludedRoomId;
    std::int64_t _excludedRoomUntilMs = 0;
    std::string _excludedPlayerHandle;
    std::int64_t _excludedPlayerUntilMs = 0;
    std::int64_t _lastProcessedLobbyChatSeq = 0;
    std::int64_t _lastLobbyChatPollMs = 0;
    std::deque<PendingSseAction> _pendingSseActions;
    std::set<std::string> _chatMutedRooms;
    std::set<std::string> _lastSeatedOpponents;
    std::int64_t _matchOverrideJoinGraceUntilMs = 0;
    std::int64_t _lastLobbyStreamRestartMs = 0;

    std::mutex _eventMutex;
    std::condition_variable _eventCv;
    bool _hasPendingEvent = false;
    std::string _lastEventReason;

    mutable std::mutex _loopMutex;
    std::condition_variable _loopWakeCv;
    std::uint64_t _loopWakeSeq = 0;
    std::uint64_t _lastObservedWakeSeq = 0;
    std::atomic<bool> _roomStateDirty{true};
    std::atomic<bool> _lobbyTablesDirty{true};
    std::int64_t _lastRoomFetchMs = 0;
    std::int64_t _lastLobbyTablesFetchMs = 0;

    int _consecutiveRematches = 0;
    int _lastLoggedPlyCount = -1;
    std::string _gameOverHandledRoomId;
    std::atomic<std::int64_t> _postGameSilenceStartMs{0};
    int _lastProactiveCommentPly = -100;
    std::atomic<int> _proactiveCommentsInGame{0};
};

#if !defined(OCTIBOT_MINIMAL)
class OctiBotMcpServer;
#endif

class OctiBotFleetRunner {
public:
    OctiBotFleetRunner() = default;
    virtual ~OctiBotFleetRunner();

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
    bool tickSupervisor();
    void waitForAll();

    const std::vector<std::unique_ptr<OctiBotRunner>> &runners() const { return _runners; }
#if !defined(OCTIBOT_MINIMAL)
    const std::vector<std::unique_ptr<OctiBotMcpServer>> &mcpServers() const { return _mcpServers; }
#endif

protected:
    virtual std::unique_ptr<OctiBotRunner> createRunner(
        std::unique_ptr<OctiBotAgent> agent,
        const OctiBotConfigStore &config,
        const std::string &customEndpoint) const;
    virtual void onFleetStarted();

private:
    std::vector<std::unique_ptr<OctiBotRunner>> _runners;
    std::map<OctiBotRunner *, std::int64_t> _lastReclaimAttemptMs;
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
