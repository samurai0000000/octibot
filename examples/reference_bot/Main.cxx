/*
 * Main.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#if defined(_WIN32)
#include <windows.h>
#endif
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "OctiBotConfig.hxx"
#include "OctiBotMcpServer.hxx"
#include "OctiBotRunner.hxx"
#include "OctiDefs.hxx"

int main(int argc, char **argv) {
#if defined(_WIN32)
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);
#endif
    bool mcpStdio = false;
    bool mcpStdioExplicit = false;
    int mcpPort = 0;
    bool mcpPortExplicit = false;
    int cliAllowSupersede = -1;
    std::string profile = "";
    std::string configPath = octi::OctiBotConfigStore::defaultConfigFilePath();
    std::string tokensPath = octi::OctiBotConfigStore::defaultTokensFilePath();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--help" || arg == "-h" || arg == "--version" || arg == "-V") {
            return octi::runFleetCli(argc, argv);
        } else if (arg == "--mcp-stdio") {
            mcpStdio = true;
            mcpStdioExplicit = true;
        } else if (arg == "--mcp-port") {
            if (i + 1 >= argc) {
                std::cerr << "[OCTIBOT] Missing argument for option '--mcp-port'. Try '--help' for usage."
                          << std::endl;
                return 1;
            }
            mcpPort = std::atoi(argv[++i]);
            mcpPortExplicit = true;
        } else if (arg == "--supersede") {
            cliAllowSupersede = 1;
        } else if (arg == "--no-supersede") {
            cliAllowSupersede = 0;
        } else if (arg == "--profile" && i + 1 < argc) {
            profile = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--tokens-file" && i + 1 < argc) {
            tokensPath = argv[++i];
        } else if (arg == "--profiles" && i + 1 < argc && argv[i + 1][0] != '-') {
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                ++i;
            }
        } else if (arg == "--all-profiles" || arg == "--verbose" || arg == "-v" ||
                   arg == "--quiet" || arg == "-q" || arg == "--ai-interactive") {
            // Handled by runFleetCli
        } else if ((arg == "--instances" || arg == "--verbosity" || arg == "--ai-cli" ||
                    arg == "--ai-model" || arg == "--ai-prompt") && i + 1 < argc) {
            ++i;
        } else {
            return octi::runFleetCli(argc, argv);
        }
    }

    octi::OctiBotConfigStore config;
    std::string err;
    if (std::ifstream(configPath).good()) {
        if (!config.loadFile(configPath, err)) {
            std::cerr << "[OCTIBOT] Config load error (" << configPath << "): " << err << std::endl;
            return 1;
        }
    }
    if (std::ifstream(tokensPath).good()) {
        config.loadTokensFile(tokensPath, err);
    }

    if (!mcpStdioExplicit && config.getBool("mcp_stdio", false)) {
        mcpStdio = true;
    }
    if (!mcpPortExplicit && config.hasKey("mcp_port")) {
        mcpPort = config.getInt("mcp_port", 0);
    }

    if (mcpStdio || mcpPort > 0) {
        if (profile.empty()) {
            profile = config.getString("default_profile", "");
        }
        if (profile.empty()) {
            auto names = config.profileNames();
            if (!names.empty()) {
                profile = names.front();
            } else {
                profile = "stock";
            }
        }

        octi::OctiBotConfigStore profConfig = config.resolveProfile(profile);
        if (mcpStdio) {
            profConfig.set("verbosity", "quiet");
        }
        if (cliAllowSupersede >= 0) {
            profConfig.set("allow_supersede", (cliAllowSupersede > 0) ? "true" : "false");
        }
        std::string algo = profConfig.getString("algorithm", "alpha_beta");
        auto agent = octi::OctiBotAgentRegistry::instance().createAgent(algo, profConfig);
        auto runner = std::make_unique<octi::OctiBotRunner>(std::move(agent), profConfig);
        if (mcpStdio) {
            runner->setVerbosity(0);
        }

        if (!runner->connectAndAuthenticate(err)) {
            std::cerr << "Authentication error: " << err << std::endl;
            return 1;
        }

        runner->start();
        octi::OctiBotMcpServer mcpServer(*runner);

        if (mcpPort > 0) {
            mcpServer.startHttpServer(mcpPort);
            std::cout << "MCP HTTP server running on port " << mcpServer.boundHttpPort() << std::endl;
            while (runner->isRunning()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } else {
            mcpServer.runStdio(std::cin, std::cout);
        }

        runner->stop();
        return 0;
    }

    return octi::runFleetCli(argc, argv);
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
