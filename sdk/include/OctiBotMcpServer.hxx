/*
 * OctiBotMcpServer.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_MCP_SERVER_HXX
#define OCTIBOT_MCP_SERVER_HXX

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <istream>
#include <map>
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

    std::string handleJsonRpc(const std::string &rawBody, const std::string &defaultProtocolVersion = "2025-03-26");

    void runStdio(std::istream &in, std::ostream &out);
    bool startHttpServer(int port = 0, const std::string &bindAddr = "127.0.0.1");
    void stopHttpServer();
    int boundHttpPort() const { return _boundPort; }

    void startAimonBridge();
    void stopAimonBridge();

    nlohmann::json buildToolsListJson() const;

    static std::string getSkillGuideContent(const std::string &topic = "all");
    static std::string sanitizePublicOutput(const std::string &raw);
    static bool isAllowedRepoRelativePath(const std::string &relPath, std::string &reasonOut);
    static bool resolveJailedRepoFile(const std::string &repoRootDir,
                                      const std::string &relPath,
                                      std::string &canonicalPathOut,
                                      std::string &errorOut);
    static bool appendPlayerFeedbackEntry(const std::string &feedbackFilePath,
                                          const std::string &playerHandle,
                                          const std::string &category,
                                          const std::string &title,
                                          const std::string &summary,
                                          const std::string &playerQuote,
                                          const std::string &relatedArea,
                                          const std::string &clientPlatform,
                                          std::string &errorOut);

    std::string callTool(const std::string &name, const std::string &argumentsJson);

    std::string renderDashboardHtml() const;
    nlohmann::json buildStatusJson();
    nlohmann::json buildConfigJson() const;

private:
    std::string handleSingleJsonRpcObject(const nlohmann::json &req, const std::string &defaultProtocolVersion);
    void registerHttpRoutes();

    OctiBotRunner &_runner;
    OctiBotEngine _engineAnalyzer;

    // HTTP / SSE server
    std::unique_ptr<httplib::Server> _httpServer;
    std::unique_ptr<std::thread> _httpThread;
    std::atomic<int> _boundPort{0};
    std::atomic<bool> _httpRunning{false};

    // MCP Streamable HTTP session state
    struct McpSessionState {
        std::string sessionId;
        int64_t createdAtMs = 0;
        int64_t lastActivityMs = 0;
        std::vector<std::string> pendingMessages;
        bool closed = false;
    };

    std::string createMcpSession();

    mutable std::mutex _mcpSessionsMutex;
    std::condition_variable _mcpSseCv;
    std::map<std::string, McpSessionState> _mcpSessions;

    // Aimon gateway bridge
    std::atomic<bool> _aimonRunning{false};
    std::unique_ptr<std::thread> _aimonThread;
    std::atomic<std::intptr_t> _aimonSocketFd{-1};

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
        const std::string &sandboxDir,
        bool allowControlTools = false);

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
