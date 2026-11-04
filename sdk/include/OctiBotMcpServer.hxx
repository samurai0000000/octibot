/*
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_MCP_SERVER_HXX
#define OCTIBOT_MCP_SERVER_HXX

#include <atomic>
#include <cstdint>
#include <istream>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>
#include <vector>
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

    static std::string getSkillGuideContent(const std::string &topic = "all");

private:
    std::string callTool(const std::string &name, const std::string &argumentsJson);

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
};

class OctiBotAgentCliLauncher {
public:
    OctiBotAgentCliLauncher(OctiBotRunner &runner, OctiBotMcpServer &mcpServer);
    ~OctiBotAgentCliLauncher();

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
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set ft=cpp et ts=4 sts=4 sw=4:
 */
