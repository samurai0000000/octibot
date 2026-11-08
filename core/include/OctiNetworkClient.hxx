/*
 * OctiNetworkClient.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_CORE_OCTI_NETWORK_CLIENT_HXX
#define OCTI_CORE_OCTI_NETWORK_CLIENT_HXX

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "OctiDefs.hxx"

#if defined(QT_CORE_LIB) || defined(QT_VERSION) || defined(QT_WIDGETS_LIB)
#include <QString>
#endif

namespace octi {

template <typename T>
struct ApiResult {
    bool ok = false;
    int httpStatus = 0;
    std::string errorCode;
    std::string errorMessage;
    int retryAfterSec = 0;
    T data{};
};

struct HealthInfo {
    std::string status;
    std::string version;
    std::int64_t uptimeSec = 0;
    std::int64_t activeTables = 0;
    std::int64_t onlinePlayers = 0;
    int protocolVersion = OCTI_PROTOCOL_VERSION;
    int minProtocolVersion = OCTI_MIN_PROTOCOL_VERSION;
};

struct AuthProviderInfo {
    std::string name;
    std::string displayName;
    std::vector<std::string> clientIds;
};

struct LinkedIdentityInfo {
    std::string provider;
    std::string providerEmail;
    std::string displayHint;
    std::int64_t linkedAt = 0;
    std::int64_t lastLoginAt = 0;
};

struct ArchivedGamePlayer {
    int seat = 0;
    std::string handle;
    std::string playerUuid;
    bool isRobot = false;
    std::string robotUuid;
};

struct ArchivedGameSummary {
    std::string gameId;
    std::string mode;
    int bases = 3;
    bool superProngs = false;
    bool edgeless = false;
    std::string timeControl;
    std::vector<ArchivedGamePlayer> players;
    std::string result;
    int winnerSeat = 0;
    std::string termination;
    int plyCount = 0;
    std::int64_t startedAt = 0;
    std::int64_t endedAt = 0;
    std::string finalFen;
    std::string octiTranscript;
};

struct ArchivedGamesPage {
    std::int64_t total = 0;
    int limit = 20;
    int offset = 0;
    std::vector<ArchivedGameSummary> games;
};

struct UploadTranscriptRequest {
    std::string transcriptJson;
    std::string clientId;
};

struct UploadTranscriptResponse {
    std::string gameId;
    int plyCount = 0;
};

struct CrashReportRequest {
    std::string crashId;
    std::int64_t createdAt = 0;
    std::string platform;
    std::string appVersion;
    std::string osVersion;
    std::string exceptionType;
    std::string exceptionMessage;
    std::string threadName;
    std::string stackTrace;
    std::string contextJson = "{}";
    std::string installId;
};

struct CrashReportResponse {
    bool ok = false;
    int httpStatus = 0;
    std::string crashId;
    bool deduplicated = false;
    std::string errorMessage;
};

struct PlayerProfileInfo {
    std::string uuid;
    std::string handle;
    std::string displayName;
    std::string avatarUrl;
    std::string bio;
    std::string location;
    int elo2p = 1200;
    int elo4p = 1200;
    int eloOfk = 1200;
    int gamesPlayed = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
    std::int64_t createdAt = 0;
    std::int64_t lastSeenAt = 0;
    bool isAdmin = false;
    std::vector<LinkedIdentityInfo> identities;
    std::vector<ArchivedGameSummary> recentGames;
    bool isRobot = false;
    std::string robotUuid;
    std::string description;
    std::string ownerUuid;
    std::string ownerHandle;
    int robotGamesPlayed = 0;
    int robotWins = 0;
    int robotLosses = 0;
    int robotDraws = 0;
};

struct SsoLoginData {
    std::string sessionToken;
    std::int64_t expiresAt = 0;
    bool isNewAccount = false;
    PlayerProfileInfo profile;
    std::vector<LinkedIdentityInfo> identities;
};

struct LinkIdentityData {
    std::string status;
    std::vector<LinkedIdentityInfo> identities;
    bool conflictAlreadyLinked = false;
    PlayerProfileInfo existingProfile;
};

struct UnlinkIdentityData {
    std::string status;
    std::vector<LinkedIdentityInfo> identities;
};

struct LeaderboardRow {
    int rank = 0;
    std::string uuid;
    std::string handle;
    std::string displayName;
    int elo = 1200;
    int gamesPlayed = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
};

struct LobbyPlayerInfo {
    std::string playerUuid;
    std::string handle;
    std::string avatarUrl;
    std::string status;
    std::string roomId;
    std::vector<std::string> clients;
    std::int64_t onlineSec = 0;
    std::int64_t idleSec = 0;
    bool isRobot = false;
    std::string robotUuid;
    std::string description;
    std::string ownerUuid;
    std::string ownerHandle;
    int robotGamesPlayed = 0;
    int robotWins = 0;
    int robotLosses = 0;
    int robotDraws = 0;
};

struct LobbyChatEntry {
    std::int64_t seq = 0;
    std::int64_t atMs = 0;
    std::string playerUuid;
    std::string handle;
    std::string text;
    std::int64_t createdAtMs = 0;
    bool isRobot = false;
    std::string robotUuid;
    std::string ownerUuid;
    std::string ownerHandle;
};
using LobbyChatMessage = LobbyChatEntry;

struct TableSeatInfo {
    int seat = 0;
    std::string playerUuid;
    std::string handle;
    int rating = 1500;
    bool connected = false;
    bool isRobot = false;
    std::string robotUuid;
    std::string description;
    std::string ownerUuid;
    std::string ownerHandle;
    std::string avatarUrl;
    int robotGamesPlayed = 0;
    int robotWins = 0;
    int robotLosses = 0;
    int robotDraws = 0;
};

struct TableSummaryInfo {
    std::string roomId;
    std::string mode = "2P";
    int bases = 3;
    bool superProngs = false;
    bool edgeless = false;
    bool timeControlEnabled = false;
    std::int64_t initialMs = 0;
    std::int64_t incrementMs = 0;
    std::int64_t disconnectGraceMs = 90000;
    bool rated = true;
    bool allowTakeback = false;
    std::string observerPolicy = "OPEN";
    std::string phase = "WAITING_FOR_PLAYERS";
    bool hasPasscode = false;
    std::string hostUuid;
    std::string hostHandle;
    std::int64_t createdAtMs = 0;
    std::int64_t lastMoveAtMs = 0;
    std::int64_t idleMs = 0;
    std::int64_t ttlRemainingMs = -1;
    int seatsTotal = 2;
    int seatsFilled = 0;
    std::vector<TableSeatInfo> seats;
    std::vector<std::string> players;
    int observerCount = 0;
    bool allowRobots = true;
};

struct LobbyStateData {
    std::vector<LobbyPlayerInfo> players;
    std::vector<TableSummaryInfo> tables;
    std::vector<LobbyChatEntry> recentChat;
    std::int64_t latestSeq = 0;
};

struct CreateTableRequest {
    std::string mode = "2P";
    int bases = 3;
    bool superProngs = false;
    bool edgeless = false;
    bool timeControlEnabled = false;
    std::int64_t initialMs = 600000;
    std::int64_t incrementMs = 5000;
    std::int64_t disconnectGraceMs = 90000;
    bool rated = true;
    bool allowTakeback = false;
    std::string observerPolicy = "OPEN";
    std::string passcode;
    bool allowRobots = true;
    int seat = 1;
};

struct CreateTableResponse {
    std::string roomId;
    int seat = 1;
};

struct JoinTableResponse {
    std::string roomId;
    int seat = 0;
    bool gameStarted = false;
};

struct MatchmakingRequest {
    std::string mode = "2P";
    int bases = 3;
    bool superProngs = false;
    bool edgeless = false;
    bool timeControlEnabled = false;
    std::int64_t initialMs = 0;
    std::int64_t incrementMs = 0;
    bool allowRobots = true;
};

struct MatchmakingStatusData {
    bool queued = false;
    std::int64_t waitedMs = 0;
    int window = 0;
    int rating = 1200;
    std::string matchedRoomId;
};

struct GameSeatInfo {
    int seat = 0;
    std::string playerUuid;
    std::string handle;
    int rating = 1200;
    bool connected = true;
    std::int64_t disconnectedForMs = 0;
    bool forfeitClaimable = false;
    std::int64_t clockMs = -1;
    bool eliminated = false;
    bool isRobot = false;
    std::string robotUuid;
    std::string description;
    std::string ownerUuid;
    std::string ownerHandle;
    std::string avatarUrl;
    int robotGamesPlayed = 0;
    int robotWins = 0;
    int robotLosses = 0;
    int robotDraws = 0;
};

struct GameProposalInfo {
    std::string type;
    std::string proposerUuid;
    std::vector<std::string> acceptedBy;
    std::vector<std::string> awaiting;
    std::int64_t expiresAtMs = 0;
};

struct GameViewData {
    bool live = true;
    std::string roomId;
    std::string mode = "2P";
    int bases = 3;
    bool superProngs = false;
    bool edgeless = false;
    bool timeControlEnabled = false;
    std::int64_t initialMs = 0;
    std::int64_t incrementMs = 0;
    std::int64_t disconnectGraceMs = 90000;
    bool rated = true;
    bool allowTakeback = false;
    std::string observerPolicy = "OPEN";
    std::string phase = "WAITING_FOR_PLAYERS";
    bool paused = false;
    bool hasPasscode = false;
    std::string hostUuid;
    int viewerSeat = 0;
    std::vector<GameSeatInfo> seats;
    std::vector<std::string> observers;
    int observerCount = 0;
    std::string fen;
    int turnSeat = 0;
    int plyCount = 0;
    std::vector<std::string> moves;
    std::string result;
    std::string termination;
    int winnerSeat = 0;
    std::vector<GameProposalInfo> proposals;
    std::int64_t seq = 0;
    bool archived = false;
    std::string rematchRoomId;
    std::int64_t createdAtMs = 0;
    std::int64_t lastMoveAtMs = 0;
    std::int64_t idleMs = 0;
    std::int64_t ttlRemainingMs = -1;
    bool allowRobots = true;
};

struct RobotConnectData {
    std::string leaseId;
    std::string robotUuid;
    std::string handle;
    std::string description;
    std::string ownerUuid;
    std::string ownerHandle;
    std::string avatarUrl;
    std::string activeRoomId;
    int gamesPlayed = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
};

struct RobotDirectoryEntry {
    std::string robotUuid;
    std::string handle;
    std::string description;
    std::string ownerUuid;
    std::string ownerHandle;
    std::string status = "Active";
    int gamesPlayed = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
    double winRate = 0.0;
    int elo2p = 1500;
};

struct MoveTelemetryData {
    bool present = false;
    int depth = 0;
    std::int64_t nodes = 0;
    int score = 0;
    std::int64_t elapsedMs = 0;
};

struct MoveSubmitData {
    std::string move;
    std::int64_t clockMs = -1;
    std::int64_t elapsedMs = 0;
    bool gameOver = false;
    std::int64_t seq = 0;
};

struct GameActionData {
    std::string action;
    std::string roomId;
    bool gameOver = false;
    std::int64_t seq = 0;
};

struct RoomSettingsData {
    std::string observerPolicy;
    std::int64_t seq = 0;
};

struct SseEvent {
    std::uint64_t id = 0;
    std::string event;
    std::string dataJson;
};

/*
 * Ephemeral local HTTP callback listener bound strictly to 127.0.0.1:0 for
 * system-browser OAuth SSO sign-in redirects.
 */
class OAuthLoopbackListener {
public:
    OAuthLoopbackListener();
    ~OAuthLoopbackListener();

    OAuthLoopbackListener(const OAuthLoopbackListener &) = delete;
    OAuthLoopbackListener &operator=(const OAuthLoopbackListener &) = delete;

    bool start(
        std::function<void(const std::string &provider,
                           const std::string &token)> onCallback,
        std::string &errorOut);
    void stop();

    bool isRunning() const;
    int boundPort() const;
    std::string callbackUrl() const;

    bool waitForCallback(int timeoutMs,
                         std::string &outProvider,
                         std::string &outToken);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/*
 * REST + Server-Sent Events (SSE) client for octi.selfso.com:443 (with fallback to trumpet.selfso.com:6874).
 *
 * Production instances created via OctiNetworkClient() connect to
 * OCTI_SERVER_HOST ("octi.selfso.com") and OCTI_SERVER_PORT (443).
 */
class OctiNetworkClient {
public:
    static constexpr const char *canonicalHost() { return OCTI_SERVER_HOST; }
    static constexpr int canonicalPort() { return OCTI_SERVER_PORT; }

    OctiNetworkClient();
    ~OctiNetworkClient();

    OctiNetworkClient(const OctiNetworkClient &) = delete;
    OctiNetworkClient &operator=(const OctiNetworkClient &) = delete;

    static std::unique_ptr<OctiNetworkClient> createForLoopbackTest(
        int loopbackPort);
    static std::unique_ptr<OctiNetworkClient> forHermeticTest(
        int loopbackPort)
    {
        return createForLoopbackTest(loopbackPort);
    }

    const std::string &host() const;
    int port() const;
    void setEndpoint(const std::string &host, int port);
    std::string buildUrl(const std::string &path = "") const;

    void setSessionToken(const std::string &token);
    std::string sessionToken() const;
    void clearSessionToken();
    bool isAuthenticated() const { return !sessionToken().empty(); }

    PlayerProfileInfo currentProfile() const;
    void setCurrentProfile(const PlayerProfileInfo &profile);

    void setBotToken(const std::string &token);
    bool isRobotToken() const;

    void setClientType(const std::string &clientType);
    std::string clientType() const;

    void setRobotLeaseId(const std::string &leaseId);
    std::string robotLeaseId() const;
    void clearRobotLeaseId();

    void setInstallId(const std::string &installId);
    std::string installId() const;

    /* ---- 1..10: Health, Auth, Profile & Leaderboard ---- */
    ApiResult<HealthInfo> fetchHealth();
    ApiResult<std::vector<AuthProviderInfo>> fetchAuthProviders();
    ApiResult<std::vector<AuthProviderInfo>> fetchProviders()
    {
        return fetchAuthProviders();
    }
    ApiResult<SsoLoginData> loginWithSso(
        const std::string &provider,
        const std::string &token,
        const std::string &desiredHandle = "",
        bool isAccessToken = false);
    ApiResult<LinkIdentityData> linkIdentity(
        const std::string &provider,
        const std::string &token,
        bool isAccessToken = false,
        bool forceRelink = false);
    ApiResult<LinkIdentityData> linkIdentity(
        const std::string &provider,
        const std::string &idToken,
        const std::string &accessToken,
        bool forceRelink = false);
    ApiResult<UnlinkIdentityData> unlinkIdentity(const std::string &provider);
    ApiResult<bool> logout();
    ApiResult<PlayerProfileInfo> fetchMyProfile();
    ApiResult<PlayerProfileInfo> updateMyProfile(
        const std::string &handle,
        const std::string &avatarUrl,
        const std::string &displayName,
        const std::string &bio,
        const std::string &location,
        bool clearAvatar = false);
    ApiResult<PlayerProfileInfo> updateMyProfile(
        const std::string &handle,
        const std::string &avatarUrl = "",
        const std::string &displayName = "");
    bool fetchAvatarBytes(
        const std::string &avatarUrl,
        std::string &outBytes,
        std::string &outContentType);
    ApiResult<PlayerProfileInfo> fetchPlayerProfile(
        const std::string &playerUuid);
    ApiResult<std::vector<LeaderboardRow>> fetchLeaderboard(
        const std::string &mode = "2P",
        int limit = 50);

    /* ---- 11..22: Lobby, Tables & Matchmaking ---- */
    ApiResult<LobbyStateData> fetchLobbyState();
    ApiResult<LobbyStateData> refreshLobby()
    {
        return fetchLobbyState();
    }
    ApiResult<std::int64_t> postLobbyChat(const std::string &text);
    ApiResult<std::vector<TableSummaryInfo>> listTables(
        bool includeFinished = false);
    ApiResult<CreateTableResponse> createTable(
        const CreateTableRequest &req = CreateTableRequest());
    ApiResult<JoinTableResponse> joinTable(
        const std::string &roomId,
        int seat = 0,
        const std::string &passcode = "");
    ApiResult<bool> leaveTable(const std::string &roomId);
    ApiResult<bool> purgeTable(const std::string &roomId);
    ApiResult<bool> kickSeat(
        const std::string &roomId,
        const std::string &targetPlayerUuid);
    ApiResult<bool> kickSeat(const std::string &roomId, int seat);
    ApiResult<bool> observeTable(
        const std::string &roomId,
        const std::string &passcode = "");
    ApiResult<bool> unobserveTable(const std::string &roomId);
    ApiResult<bool> enqueueMatchmaking(
        const MatchmakingRequest &req = MatchmakingRequest());
    ApiResult<bool> dequeueMatchmaking();
    ApiResult<MatchmakingStatusData> fetchMatchmakingStatus();

    /* ---- 23..30: Live Game, Chat & Moderation ---- */
    ApiResult<GameViewData> fetchGameView(const std::string &roomId);
    ApiResult<GameViewData> fetchRoomState(const std::string &roomId)
    {
        return fetchGameView(roomId);
    }
    ApiResult<MoveSubmitData> submitMove(
        const std::string &roomId,
        const std::string &moveStr);
    ApiResult<MoveSubmitData> submitMove(
        const std::string &roomId,
        const std::string &moveStr,
        const MoveTelemetryData &telemetry);
    ApiResult<bool> resignGame(const std::string &roomId);
    ApiResult<GameActionData> submitGameAction(
        const std::string &roomId,
        const std::string &action);
    ApiResult<GameActionData> submitAction(
        const std::string &roomId,
        const std::string &action)
    {
        return submitGameAction(roomId, action);
    }
    ApiResult<std::int64_t> postGameChat(
        const std::string &roomId,
        const std::string &scope,
        const std::string &text);
    ApiResult<bool> banObserver(
        const std::string &roomId,
        const std::string &targetPlayerUuid,
        const std::string &reason = "");
    ApiResult<bool> muteParticipant(
        const std::string &roomId,
        const std::string &targetPlayerUuid,
        bool muteChat);
    ApiResult<RoomSettingsData> updateRoomSettings(
        const std::string &roomId,
        const std::string &observerPolicy);

    /* ---- 33..35: Game Archive & .octi Transcript ---- */
    ApiResult<ArchivedGamesPage> listArchivedGames(
        const std::string &mode = "",
        const std::string &playerUuid = "",
        int limit = 20,
        int offset = 0);
    ApiResult<ArchivedGamesPage> listArchivedGames(
        const std::string &mode,
        const std::string &playerUuid,
        const std::string &robotUuid,
        int limit = 20,
        int offset = 0);
    ApiResult<ArchivedGameSummary> fetchArchivedGame(
        const std::string &gameId);
    ApiResult<bool> purgeArchivedGame(
        const std::string &gameId);
    ApiResult<std::string> downloadOctiTranscript(
        const std::string &gameId);
    ApiResult<UploadTranscriptResponse> uploadTrainingTranscript(
        const UploadTranscriptRequest &req);
    CrashReportResponse uploadCrashReport(
        const CrashReportRequest &req);
    ApiResult<int> deleteInstallArchiveData(
        const std::string &installId);

    /* ---- 36..38: Robot Protocol & Administration ---- */
    ApiResult<RobotConnectData> connectRobot(bool ifUnused = false);
    ApiResult<bool> disconnectRobot();
    ApiResult<std::string> exportRobotTokensConf();
    ApiResult<std::vector<RobotDirectoryEntry>> fetchRobotsDirectory(
        const std::string &query = "");

    /* ---- Poll & Server-Sent Events (SSE) Live Streams ---- */
    ApiResult<std::vector<SseEvent>> pollLobbyEvents(
        std::uint64_t sinceSeq = 0,
        int limit = 100);
    ApiResult<std::vector<SseEvent>> pollGameEvents(
        const std::string &roomId,
        std::uint64_t sinceSeq = 0,
        int limit = 100);
    ApiResult<std::vector<SseEvent>> pollRoomEvents(
        const std::string &roomId,
        std::int64_t sinceSeq = 0,
        int limit = 100)
    {
        return pollGameEvents(
            roomId,
            sinceSeq < 0 ? 0 : static_cast<std::uint64_t>(sinceSeq),
            limit);
    }

#if defined(QT_CORE_LIB) || defined(QT_VERSION) || defined(QT_WIDGETS_LIB)
    ApiResult<bool> observeTable(const QString &roomId,
                                 const QString &passcode = QString())
    {
        return observeTable(roomId.toStdString(), passcode.toStdString());
    }
    ApiResult<bool> unobserveTable(const QString &roomId)
    {
        return unobserveTable(roomId.toStdString());
    }
    ApiResult<GameViewData> fetchRoomState(const QString &roomId)
    {
        return fetchGameView(roomId.toStdString());
    }
    ApiResult<std::vector<SseEvent>> pollRoomEvents(
        const QString &roomId,
        std::int64_t sinceSeq = 0,
        int limit = 100)
    {
        return pollRoomEvents(roomId.toStdString(), sinceSeq, limit);
    }
    ApiResult<LinkIdentityData> linkIdentity(
        const QString &provider,
        const QString &idToken,
        const QString &accessToken = QString(),
        bool forceRelink = false)
    {
        return linkIdentity(provider.toStdString(),
                            idToken.toStdString(),
                            accessToken.toStdString(),
                            forceRelink);
    }
#endif

    void startLobbyEventStream(
        std::uint64_t sinceSeq,
        std::function<void(const SseEvent &)> onEvent);
    void stopLobbyEventStream();
    bool isLobbyEventStreamActive() const;
    std::uint64_t lastLobbyEventId() const;

    void startGameEventStream(
        const std::string &roomId,
        std::uint64_t sinceSeq,
        std::function<void(const SseEvent &)> onEvent);
    void stopGameEventStream();
    bool isGameEventStreamActive() const;
    std::uint64_t lastGameEventId() const;

    void stopEventStreams();

    static std::vector<SseEvent> parseSseBuffer(std::string &buffer);

private:
    OctiNetworkClient(const std::string &host, int port);

    struct Impl;
    std::unique_ptr<Impl> _impl;
    std::string _host;
    int _port;
};

} // namespace octi

#endif // OCTI_CORE_OCTI_NETWORK_CLIENT_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
