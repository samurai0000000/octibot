/*
 * OctiBotMcpServer.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_MCP_SERVER_HXX
#define OCTIBOT_MCP_SERVER_HXX

#include <atomic>
#include <chrono>
#include <cstdint>
#include <istream>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>
#include "OctiBotRunner.hxx"

namespace httplib {
class Server;
}

namespace octi {

class OctiBotMcpServer {
public:
    explicit OctiBotMcpServer(OctiBotRunner &runner);
    ~OctiBotMcpServer();

    std::string handleJsonRpc(const std::string &jsonRequest);

    void runStdio(std::istream &in, std::ostream &out);
    bool startHttpServer(int port = 0, const std::string &bindAddr = "127.0.0.1");
    void stopHttpServer();
    int boundHttpPort() const { return _boundPort; }

    void startAimonBridge();
    void stopAimonBridge();

    nlohmann::json buildToolsListJson() const;

    static std::string getSkillGuideContent(const std::string &topic = "all");

    std::string renderDashboardHtml() const;
    nlohmann::json buildStatusJson();
    nlohmann::json buildConfigJson() const;

private:
    std::string callTool(const std::string &name, const std::string &argumentsJson);
    void registerHttpRoutes();

    OctiBotRunner &_runner;
    OctiBotEngine _engineAnalyzer;

    // HTTP / SSE server
    std::unique_ptr<httplib::Server> _httpServer;
    std::unique_ptr<std::thread> _httpThread;
    std::atomic<int> _boundPort{0};

    // Aimon gateway bridge
    std::atomic<bool> _aimonRunning{false};
    std::unique_ptr<std::thread> _aimonThread;
    int _aimonSocketFd = -1;

    // Health cache (3s TTL)
    mutable std::mutex _healthCacheMutex;
    std::chrono::steady_clock::time_point _lastHealthCheckTime{};
    nlohmann::json _cachedHealthJson;
};

class OctiBotAgentCliLauncher {
public:
    OctiBotAgentCliLauncher(OctiBotRunner &runner, OctiBotMcpServer &mcpServer);
    ~OctiBotAgentCliLauncher();

    static bool materializeSandboxWorkspace(const std::string &sandboxDir, int mcpPort);

    static std::vector<std::string> buildChatCommandArgs(
        const std::string &backend,
        const std::string &executable,
        int mcpPort,
        const std::string &model,
        const std::string &prompt,
        const std::string &sandboxDir);

    static std::vector<std::string> buildCommandArgs(
        const std::string &backend,
        const std::string &executable,
        int mcpPort,
        const std::string &model,
        const std::string &prompt,
        bool interactive,
        const std::string &workspaceDir);

    bool start();
    void stop();
    bool isRunning() const;

private:
    OctiBotRunner &_runner;
    OctiBotMcpServer &_mcpServer;

    std::atomic<bool> _running{false};
    std::unique_ptr<std::thread> _monitorThread;
#if !defined(_WIN32)
    pid_t _childPid = -1;
    pid_t _childPgid = -1;
    int _stdoutPipeFd = -1;
#else
    void *_jobHandle = nullptr;
    void *_processHandle = nullptr;
#endif
};

} // namespace octi

#endif /* OCTIBOT_MCP_SERVER_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
