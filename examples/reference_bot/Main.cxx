/*
 * Copyright (C) 2026, Charles Chiou
 */

#include <iostream>
#include <string>
#include "OctiBotConfig.hxx"
#include "OctiBotMcpServer.hxx"
#include "OctiBotRunner.hxx"

int main(int argc, char **argv) {
    bool mcpStdio = false;
    int mcpPort = 0;
    std::string profile = "stock";
    std::string configPath = octi::OctiBotConfigStore::defaultConfigFilePath();
    std::string tokensPath = octi::OctiBotConfigStore::defaultTokensFilePath();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mcp-stdio") {
            mcpStdio = true;
        } else if (arg == "--mcp-port" && i + 1 < argc) {
            mcpPort = std::atoi(argv[++i]);
        } else if (arg == "--profile" && i + 1 < argc) {
            profile = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--tokens-file" && i + 1 < argc) {
            tokensPath = argv[++i];
        }
    }

    if (mcpStdio || mcpPort > 0) {
        octi::OctiBotConfigStore config;
        std::string err;
        config.loadFile(configPath, err);
        config.loadTokensFile(tokensPath, err);

        octi::OctiBotConfigStore profConfig = config.resolveProfile(profile);
        std::string algo = profConfig.getString("algorithm", "alpha_beta");
        auto agent = octi::OctiBotAgentRegistry::instance().createAgent(algo, profConfig);
        auto runner = std::make_unique<octi::OctiBotRunner>(std::move(agent), profConfig);

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
