/*
 * Copyright (C) 2026, Charles Chiou
 */

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "OctiBotConfig.hxx"
#include "OctiBotMcpServer.hxx"
#include "OctiBotRunner.hxx"

int main(int argc, char **argv) {
    bool mcpStdio = false;
    bool mcpStdioExplicit = false;
    int mcpPort = 0;
    bool mcpPortExplicit = false;
    std::string profile = "";
    std::string configPath = octi::OctiBotConfigStore::defaultConfigFilePath();
    std::string tokensPath = octi::OctiBotConfigStore::defaultTokensFilePath();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: octibot [options]\n\n"
                      << "Options:\n"
                      << "  --profile <name>         Run single bot profile\n"
                      << "  --profiles <p1,p2,...>   Run multiple bot profiles\n"
                      << "  --all-profiles           Run all configured profiles\n"
                      << "  --instances <n>          Number of instances per profile\n"
                      << "  --mcp-stdio              Run as MCP server over stdin/stdout\n"
                      << "  --mcp-port <port>        Run as MCP server over HTTP on port\n"
                      << "  --config <path>          Path to octibot.conf\n"
                      << "  --tokens-file <path>     Path to tokens.conf\n"
                      << "  --verbosity <level>      Verbosity (0=quiet, 1=minimal, 2=normal, 3=verbose, 4=trace)\n"
                      << "  -v, --verbose            Shortcut for --verbosity verbose\n"
                      << "  -q, --quiet              Shortcut for --verbosity quiet\n"
                      << "  -h, --help               Show this help message\n";
            return 0;
        } else if (arg == "--mcp-stdio") {
            mcpStdio = true;
            mcpStdioExplicit = true;
        } else if (arg == "--mcp-port" && i + 1 < argc) {
            mcpPort = std::atoi(argv[++i]);
            mcpPortExplicit = true;
        } else if (arg == "--profile" && i + 1 < argc) {
            profile = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--tokens-file" && i + 1 < argc) {
            tokensPath = argv[++i];
        }
    }

    octi::OctiBotConfigStore config;
    std::string err;
    if (std::ifstream(configPath).good()) {
        config.loadFile(configPath, err);
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
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set ft=cpp et ts=4 sts=4 sw=4:
 */
