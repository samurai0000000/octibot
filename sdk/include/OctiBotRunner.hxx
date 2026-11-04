/*
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_RUNNER_HXX
#define OCTIBOT_RUNNER_HXX

#include <atomic>
#include <condition_variable>
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

class OctiBotRunner {
public:
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

    bool waitForEvent(int timeoutMs, std::string &outReason);
    void notifyEvent(const std::string &reason);

    // Getters for MCP server & inspection
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
    const BotMoveTelemetry &lastTelemetry() const { return _lastTelemetry; }
    std::string moveControl() const { return _moveControl; }
    void setMoveControl(const std::string &mc) { _moveControl = mc; }
    int verbosity() const { return _verbosity; }
    void setVerbosity(int level) { _verbosity = level; }
    void logMsg(int minLevel, const std::string &msg) const;

    bool submitAgentMove(const std::string &moveNotation, std::string &errorOut);
    bool executeGameAction(const std::string &action, const std::string &arg, std::string &errorOut);

private:
    void loopStep();
    void handleLobbySseEvent(const std::string &eventType, const std::string &data);
    void handleGameSseEvent(const std::string &eventType, const std::string &data);
    void playEngineTurn();

    std::unique_ptr<OctiBotAgent> _agent;
    OctiBotConfigStore _config;
    OctiNetworkClient _client;
    OctiBotPolicyEngine _policy;
    OctiBotConversationEngine _conversation;
    BotIdentity _identity;

    std::atomic<bool> _running{false};
    std::atomic<bool> _superseded{false};
    std::unique_ptr<std::thread> _workerThread;

    std::string _moveControl = "engine"; // "engine" or "agent"
    bool _publishMoveTelemetry = false;
    int _agentMoveTimeoutMs = 15000;
    int _verbosity = 3;

    mutable std::mutex _stateMutex;
    std::string _currentRoomId;
    std::string _currentPhase = "IDLE";
    int _currentSeat = 0;
    GameViewData _activeGameView;
    std::unique_ptr<OctiBoard> _activeBoard;
    BotMoveTelemetry _lastTelemetry;
    std::string _pendingAgentMove;
    bool _hasAgentMove = false;

    std::mutex _eventMutex;
    std::condition_variable _eventCv;
    bool _hasPendingEvent = false;
    std::string _lastEventReason;
    int _consecutiveRematches = 0;
    int _lastLoggedPlyCount = -1;
};

class OctiBotMcpServer;

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
    const std::vector<std::unique_ptr<OctiBotMcpServer>> &mcpServers() const { return _mcpServers; }

private:
    std::vector<std::unique_ptr<OctiBotRunner>> _runners;
    std::vector<std::unique_ptr<OctiBotMcpServer>> _mcpServers;
};

int runFleetCli(int argc, char **argv);

} // namespace octi

#endif /* OCTIBOT_RUNNER_HXX */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set ft=cpp et ts=4 sts=4 sw=4:
 */
