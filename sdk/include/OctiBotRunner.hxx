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
    HANDLE_GAME_PROPOSAL,
    LEAVE_ROOM,
    CUSTOM_ACTION
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
    GameProposalInfo proposal;
    bool senderIsRobot = false;
    std::string senderHandle;
    std::string roomId;
    std::function<void()> customAction;
};

class OctiBotRunner {
public:
    static constexpr std::size_t kMaxLogLines = 5000;
    static constexpr std::size_t kMaxMoveHistory = 50;

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
    virtual void stop();

    bool isRunning() const { return _running; }
    bool isSuperseded() const { return _superseded; }
    virtual bool isLobbyOnly() const { return _lobbyOnly; }
    static OctiBotRunner *findActiveRunnerByHandle(const std::string &handle);
    static std::vector<OctiBotRunner *> listActiveRunners();
    static void wakeAllActiveRunners(bool markRoomDirty, bool markLobbyDirty);

    bool waitForEvent(int timeoutMs, std::string &outReason);
    void notifyEvent(const std::string &reason);

    // Getters for runner state & inspection
    const BotIdentity &identity() const { return _identity; }
    const OctiBotConfigStore &config() const { return _config; }
    OctiBotConfigStore &config() { return _config; }
    OctiBotPolicyEngine &policyEngine() { return _policy; }
    const OctiBotPolicyEngine &policyEngine() const { return _policy; }
    OctiNetworkClient &networkClient() { return _client; }
    const OctiNetworkClient &networkClient() const { return _client; }
    OctiBotAgent *agent() { return _agent.get(); }
    const OctiBotAgent *agent() const { return _agent.get(); }

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

    virtual void setRoomChatMuted(const std::string &roomId, bool muted);
    bool isRoomChatMuted(const std::string &roomId) const;
    virtual bool shouldEmitProactiveChat(const std::string &roomId,
                                         const std::string &eventKind = "") const;

    const BotMoveTelemetry &lastTelemetry() const { return _lastTelemetry; }
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

    // Dynamic config hot-reload & disk persistence
    void setConfigFilePath(const std::string &path) { _configFilePath = path; }
    const std::string &configFilePath() const { return _configFilePath; }
    virtual std::map<std::string, std::string> buildEffectiveConfigMap() const;
    virtual bool applyConfigUpdates(const std::map<std::string, std::string> &updates,
                                    bool saveToDisk,
                                    std::string &errorOut);

    bool executeGameAction(const std::string &action, const std::string &arg, std::string &errorOut);
    bool joinRoomById(const std::string &roomId, int &outSeat, std::string &errorOut);
    bool joinRoomById(const std::string &roomId, int preferredSeat, int &outSeat, std::string &errorOut);
    bool hostRoomWithRequest(const CreateTableRequest &req, std::string &outRoomId, int &outSeat, std::string &errorOut);
    LeaveRoomResult leaveRoomWithOptions(const std::string &rawRoomId,
                                         bool allowResign,
                                         bool preferPly0Abort = true);
    bool leaveRoomById(const std::string &roomId, std::string &errorOut);

    BotMatchOverride matchOverride() const;
    bool applyMatchOverride(const BotMatchOverride &ov, std::string &outSummary, std::string &errorOut);
    void clearMatchOverride();
    bool checkAgentSupportsVariant(const OctiVariantSpec &spec, std::string &outReason) const;
    bool checkPolicyAndAgentSupportsVariant(const OctiVariantSpec &spec, std::string &outReason) const;
    virtual bool emitVariantDeclineMessage(const std::string &channel,
                                           const std::string &roomId,
                                           const std::string &senderHandle,
                                           bool senderIsRobot,
                                           const OctiVariantSpec &spec,
                                           const std::string &agentReason = "",
                                           bool waitForCompletion = false);
    virtual bool emitResourceMaxedDeclineMessage(const std::string &channel,
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

    // Extension lifecycle, SSE, chat & search hooks
    virtual void onRunnerStarting();
    virtual void onRunnerLoopStarted();
    virtual void onRunnerReclaimed();
    virtual void onSuperseded();
    virtual void onRoomSwitched(const std::string &departedRoomId,
                                const std::string &newRoomId);
    virtual void onRoomClosedOrLeft(const std::string &roomId);
    virtual void onGamePhaseReset(const std::string &prevRoomId,
                                  const std::string &newRoomId);
    virtual void onOpponentChangedInRoom(const std::string &roomId,
                                         const GameViewData &view);
    virtual void onRoomStateRefreshed(const std::string &roomId,
                                      const GameViewData &view);
    virtual void onGameStartedHandled(const std::string &roomId,
                                      const GameViewData &view,
                                      int mySeat,
                                      const std::string &opponentHandle);
    virtual void onGameOverHandled(const std::string &roomId,
                                   const GameViewData &view,
                                   int mySeat,
                                   const std::string &result,
                                   bool hasConnectedOpponent,
                                   const std::string &opponentHandle);
    virtual void onGameOverSseReceived();
    virtual bool isPostGameLingerBlocked() const;
    virtual void onPostGameLingerElapsed(const std::string &roomId,
                                         const std::string &opponentHandle,
                                         int lingerSec);
    virtual bool isKnownRobotHandle(const std::string &handle) const;
    virtual bool tryHandleIdleWaitingRoomTables(
        const std::string &currentRoomId,
        const std::vector<TableSummaryInfo> &tables);
    virtual bool onLobbyChatReceived(const LobbyChatEntry &msg,
                                     bool isOwner,
                                     const std::string &senderOwnerUuid,
                                     const std::string &senderOwnerHandle);
    virtual bool onGameChatReceived(const std::string &roomId,
                                    const std::string &senderHandle,
                                    const std::string &senderUuid,
                                    const std::string &text,
                                    bool isOwner,
                                    bool senderIsRobot,
                                    const std::string &senderOwnerUuid,
                                    const std::string &senderOwnerHandle);
    virtual bool tryComputeExternalMove(const OctiBoard &board,
                                        const GameViewData &view,
                                        int mySeat,
                                        std::string &outMove,
                                        bool &outAborted);
    virtual void onSearchTurnBegin(const std::string &roomId,
                                   const GameViewData &view,
                                   int mySeat);
    virtual void onSearchTurnEnd(const BotMoveTelemetry &telem);
    virtual void onMoveSubmitted(const std::string &roomId,
                                 const GameViewData &view,
                                 int mySeat,
                                 const std::string &move,
                                 const BotMoveTelemetry &telem,
                                 bool hasPrevEval,
                                 int prevEvalScore);

    void enqueueWorkerAction(std::function<void()> fn);
    void wakeLoop(bool markRoomDirty = false, bool markLobbyDirty = false);
    bool isRunnerStopping() const { return _stopping.load(); }
    void resetPostGameSilenceTimer();
    bool tryAcquireVariantDeclineCooldown(const std::string &channel,
                                          const std::string &roomId,
                                          const std::string &senderHandle,
                                          const std::string &variantLabel);
    bool tryAcquireResourceDeclineCooldown(const std::string &channel,
                                           const std::string &roomId,
                                           const std::string &senderHandle);
    bool applyMatchOverrideWithDetails(const BotMatchOverride &ov,
                                       std::string &outSummary,
                                       std::string &errorOut,
                                       bool *outVariantUnsupported = nullptr,
                                       OctiVariantSpec *outUnsupportedSpec = nullptr,
                                       bool *outResourceMaxedOut = nullptr);
    void armMatchOverrideWithGrace(const BotMatchOverride &ov,
                                   int graceMs,
                                   const std::string &requesterHandle,
                                   bool requesterIsRobot);

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
    void waitForLoopWake(int maxWaitMs);
    int computeNextWaitMs() const;

    std::unique_ptr<OctiBotAgent> _agent;
    OctiBotConfigStore _config;
    std::string _configFilePath;
    OctiNetworkClient _client;
    OctiBotPolicyEngine _policy;
    BotIdentity _identity;

    std::atomic<bool> _running{false};
    std::atomic<bool> _stopping{false};
    std::atomic<bool> _superseded{false};
    std::atomic<std::int64_t> _supersededAtMs{0};
    std::unique_ptr<std::thread> _workerThread;

    bool _publishMoveTelemetry = false;
    bool _announceMoveSearchChat = true;
    bool _lobbyOnly = false;
    int _verbosity = 3;

    // Live search telemetry
    std::atomic<bool> _isSearching{false};
    std::atomic<int> _liveDepth{0};
    std::atomic<std::uint64_t> _liveNodes{0};
    std::atomic<int> _liveScore{0};

    // Rolling 50-move history
    mutable std::mutex _historyMutex;
    std::deque<BotMoveHistoryEntry> _moveHistory;

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
    BotMatchOverride _matchOverride;
    std::string _matchOverrideRequesterHandle;
    bool _matchOverrideRequesterIsRobot = false;
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
    std::string _gameStartedHandledRoomId;
    std::string _resumedActiveRoomId;
    bool _agentEmittedGameStartGreeting = false;
    std::string _gameOverHandledRoomId;
    std::atomic<std::int64_t> _postGameSilenceStartMs{0};
};

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

    virtual void stop();
    virtual bool tickSupervisor();
    void waitForAll();

    const std::vector<std::unique_ptr<OctiBotRunner>> &runners() const { return _runners; }

protected:
    virtual bool shouldStartProfile(const std::string &profileLabel,
                                    const OctiBotConfigStore &resolvedConfig,
                                    const std::string &algorithmName,
                                    const OctiBotAgent *agent) const;
    virtual std::unique_ptr<OctiBotRunner> createRunner(
        std::unique_ptr<OctiBotAgent> agent,
        const OctiBotConfigStore &config,
        const std::string &customEndpoint) const;
    virtual void onBeforeRunnerStart(OctiBotRunner &runner,
                                     const OctiBotConfigStore &resolvedConfig);
    virtual void onFleetStarted();

private:
    std::vector<std::unique_ptr<OctiBotRunner>> _runners;
    std::map<OctiBotRunner *, std::int64_t> _lastReclaimAttemptMs;
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
