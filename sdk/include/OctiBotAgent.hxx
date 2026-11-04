/*
 * OctiBotAgent.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_AGENT_HXX
#define OCTIBOT_AGENT_HXX

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "OctiBoard.hxx"
#include "OctiBotConfig.hxx"
#include "OctiBotEngine.hxx"
#include "OctiNetworkClient.hxx"

namespace octi {

struct BotIdentity {
    std::string robotUuid;
    std::string handle;       // Registered Robot Name (e.g. "stock")
    std::string description;  // Registered Robot Description
    std::string ownerUuid;
    std::string ownerHandle;
    std::string activeRoomId; // Non-empty if resuming an active room on connect
    int gamesPlayed = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
};

struct BotMoveTelemetry {
    int depthReached = 0;
    std::int64_t nodesVisited = 0;
    int evalScore = 0;
    std::int64_t elapsedMs = 0;
    bool present = false;
};

class OctiBotAgent {
public:
    virtual ~OctiBotAgent() = default;

    virtual void onConfigure(const OctiBotConfigStore &config) { (void) config; }
    virtual void onConnected(const BotIdentity &identity) { (void) identity; }
    virtual void onLobbyChat(const LobbyChatEntry &msg,
                             std::string &outReply) {
        (void) msg;
        (void) outReply;
    }
    virtual void onGameStarted(const GameViewData &game,
                               int mySeat,
                               std::string &outGreetingChat) {
        (void) game;
        (void) mySeat;
        (void) outGreetingChat;
    }
    virtual std::string computeMove(const OctiBoard &board,
                                    const GameViewData &game,
                                    int mySeat,
                                    std::int64_t remainingClockMs,
                                    BotMoveTelemetry *outTelemetry = nullptr) = 0;
    virtual bool onProposal(const GameProposalInfo &proposal,
                            const GameViewData &game) {
        (void) proposal;
        (void) game;
        return true;
    }
    virtual void onGameChat(const std::string &senderHandle,
                            const std::string &text,
                            std::string &outReply) {
        (void) senderHandle;
        (void) text;
        (void) outReply;
    }
    virtual void onGameOver(const GameViewData &game, int mySeat) {
        (void) game;
        (void) mySeat;
    }
};

class StockAlphaBetaAgent : public OctiBotAgent {
public:
    using ProgressCallback = std::function<void(int depth, std::uint64_t nodes, int score)>;

    StockAlphaBetaAgent() = default;
    explicit StockAlphaBetaAgent(const OctiBotConfigStore &config);

    void onConfigure(const OctiBotConfigStore &config) override;
    std::string computeMove(const OctiBoard &board,
                            const GameViewData &game,
                            int mySeat,
                            std::int64_t remainingClockMs,
                            BotMoveTelemetry *outTelemetry = nullptr) override;

    OctiBotEngine &engine() { return _engine; }
    const BotTuningProfile &profile() const { return _profile; }
    void setProgressCallback(ProgressCallback cb) { _progressCallback = std::move(cb); }

private:
    OctiBotEngine _engine;
    BotTuningProfile _profile;
    int _verbosity = 2;
    ProgressCallback _progressCallback;
};

class CentaurMcpAgent : public StockAlphaBetaAgent {
public:
    using StockAlphaBetaAgent::StockAlphaBetaAgent;
};

class MinimalStarterAgent : public OctiBotAgent {
public:
    MinimalStarterAgent() = default;
    explicit MinimalStarterAgent(const OctiBotConfigStore &config);

    void onConfigure(const OctiBotConfigStore &config) override;
    void onConnected(const BotIdentity &identity) override;
    void onGameStarted(const GameViewData &game, int mySeat,
                       std::string &outGreetingChat) override;
    std::string computeMove(const OctiBoard &board,
                            const GameViewData &game,
                            int mySeat,
                            std::int64_t remainingClockMs,
                            BotMoveTelemetry *outTelemetry = nullptr) override;

private:
    double _aggression = 1.0;
    bool _preferJumps = true;
};

class OctiBotAgentRegistry {
public:
    using AgentFactory = std::function<std::unique_ptr<OctiBotAgent>(const OctiBotConfigStore &)>;

    static OctiBotAgentRegistry &instance();

    void registerAgent(const std::string &algorithmName, AgentFactory factory);
    bool hasAgent(const std::string &algorithmName) const;
    std::unique_ptr<OctiBotAgent> createAgent(const std::string &algorithmName,
                                              const OctiBotConfigStore &config) const;
    std::vector<std::string> registeredAlgorithms() const;

private:
    OctiBotAgentRegistry();
    mutable std::mutex _mutex;
    std::map<std::string, AgentFactory> _factories;
};

} // namespace octi

#endif /* OCTIBOT_AGENT_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
