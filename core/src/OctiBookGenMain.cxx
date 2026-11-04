/*
 * OctiBookGenMain.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>
#include "OctiBoard.hxx"
#include "OctiBotEngine.hxx"
#include "OctiDefs.hxx"
#include "OctiGame.hxx"
#include "OctiMove.hxx"

namespace {

struct BookGenConfig {
    std::string mode = "all";
    int maxPly = 6;
    int searchDepth = 8;
    int timeBudgetMs = 30000;
    int botBranch = 3;
    int oppBranch = 8;
    int threads = static_cast<int>(
        std::max(1u, std::thread::hardware_concurrency()));
    std::string outHeader = "core/include/OctiOpeningBookData.hxx";
    std::string outJson = "plan/plan_ai_opening_playbook.artifacts/opening_book.json";
    std::string outJava;
};

struct FrontierNode {
    std::uint64_t canonicalKey = 0;
    bool mirrored = false;
    int ply = 0;
    std::string variantLabel;
    std::string positionStr;
    std::string moveHistoryDesc;
};

struct GeneratedBookEntry {
    std::uint64_t canonicalKey = 0;
    int depth = 0;
    int ply = 0;
    std::string variantLabel;
    std::string positionStr;
    std::string moveHistoryDesc;
    std::vector<octi::BookMoveScore> candidates;
};

long steadyClockMs()
{
    using namespace std::chrono;
    return static_cast<long>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

std::string toUpperAscii(std::string s)
{
    for (char &c : s) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return s;
}

std::string formatHexKey(std::uint64_t key)
{
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex
        << std::setw(16) << std::setfill('0') << key;
    return oss.str();
}

void printUsage(const char *progName)
{
    std::cout
        << "Usage: " << progName << " [options]\n\n"
        << "Offline Zobrist Opening Playbook Generator for OCTI (C++ / Android / Web)\n"
        << "Expands unique canonical opening states layer-by-layer (deduplicating\n"
        << "commuting prong/pod move transpositions and horizontal mirror symmetry)\n"
        << "and evaluates each state with deep multi-threaded alpha-beta search.\n\n"
        << "Options:\n"
        << "  -h, --help                 Show this help message and exit\n"
        << "  --mode <variant>           Game variant(s) to expand:\n"
        << "                               all     = 2P (3-Base & 1-Base), OFK (7x7), 4P (default)\n"
        << "                               2P      = 2-Player 3-Base standard\n"
        << "                               2P_1B   = 2-Player 1-Base fast victory\n"
        << "                               2P_ALL  = Both 2P 3-Base and 2P 1-Base\n"
        << "                               OFK     = OCTI for Kids (6x7)\n"
        << "                               4P      = 4-Player 2v2 Coalitions\n"
        << "  --max-ply <int>            Number of opening plies to expand (default: 6)\n"
        << "  --search-depth <int>       Alpha-beta search depth per position (default: 8)\n"
        << "  --time-budget-ms <int>     Max wall-clock budget per position in ms (default: 30000)\n"
        << "  --bot-branch <int>         Top candidate moves stored per book entry (default: 3)\n"
        << "  --opp-branch <int>         Top replies expanded into next ply frontier (default: 8)\n"
        << "  --threads <int>            Total CPU worker threads (default: hardware concurrency)\n"
        << "  --out-header <path>        Output C++ header path\n"
        << "                             (default: core/include/OctiOpeningBookData.hxx)\n"
        << "  --out-json <path>          Output JSON playbook path\n"
        << "                             (default: plan/plan_ai_opening_playbook.artifacts/opening_book.json)\n"
        << "  --out-java <path>          Optional Java snippet output path for Android\n\n"
        << "Recommended 72-Thread Builder Profiles:\n"
        << "  1. Standard Book (~5-15 min on 72 threads, 6 plies, depth 8):\n"
        << "     ./core/build/octi_book_gen --mode all --max-ply 6 --search-depth 8 \\\n"
        << "         --bot-branch 3 --opp-branch 8 --threads 72\n\n"
        << "  2. Deep Tournament Book (~45-90 min on 72 threads, 8 plies, depth 10):\n"
        << "     ./core/build/octi_book_gen --mode all --max-ply 8 --search-depth 10 \\\n"
        << "         --time-budget-ms 60000 --bot-branch 3 --opp-branch 10 --threads 72\n\n"
        << "  3. Ultra-Deep Overnight Book (8 plies, depth 12, 120s per node):\n"
        << "     ./core/build/octi_book_gen --mode all --max-ply 8 --search-depth 12 \\\n"
        << "         --time-budget-ms 120000 --bot-branch 3 --opp-branch 12 --threads 72\n";
}

bool parseArgs(int argc, char **argv, BookGenConfig &cfg, bool &helpRequested)
{
    helpRequested = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            helpRequested = true;
            return true;
        } else if (arg == "--mode" && i + 1 < argc) {
            cfg.mode = argv[++i];
        } else if (arg == "--max-ply" && i + 1 < argc) {
            cfg.maxPly = std::atoi(argv[++i]);
        } else if (arg == "--search-depth" && i + 1 < argc) {
            cfg.searchDepth = std::atoi(argv[++i]);
        } else if (arg == "--time-budget-ms" && i + 1 < argc) {
            cfg.timeBudgetMs = std::atoi(argv[++i]);
        } else if (arg == "--bot-branch" && i + 1 < argc) {
            cfg.botBranch = std::atoi(argv[++i]);
        } else if (arg == "--opp-branch" && i + 1 < argc) {
            cfg.oppBranch = std::atoi(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            cfg.threads = std::atoi(argv[++i]);
        } else if (arg == "--out-header" && i + 1 < argc) {
            cfg.outHeader = argv[++i];
        } else if (arg == "--out-json" && i + 1 < argc) {
            cfg.outJson = argv[++i];
        } else if (arg == "--out-java" && i + 1 < argc) {
            cfg.outJava = argv[++i];
        } else {
            std::cerr << "Error: Unknown or incomplete option '" << arg << "'\n\n";
            return false;
        }
    }

    if (cfg.maxPly < 1 || cfg.maxPly > 20) {
        std::cerr << "Error: --max-ply must be between 1 and 20\n";
        return false;
    }
    if (cfg.searchDepth < 1 || cfg.searchDepth > 30) {
        std::cerr << "Error: --search-depth must be between 1 and 30\n";
        return false;
    }
    if (cfg.timeBudgetMs < 10) {
        std::cerr << "Error: --time-budget-ms must be >= 10\n";
        return false;
    }
    if (cfg.botBranch < 1 || cfg.botBranch > 16) {
        std::cerr << "Error: --bot-branch must be between 1 and 16\n";
        return false;
    }
    if (cfg.oppBranch < 1 || cfg.oppBranch > 64) {
        std::cerr << "Error: --opp-branch must be between 1 and 64\n";
        return false;
    }
    if (cfg.threads < 1) {
        cfg.threads = 1;
    }

    std::string m = toUpperAscii(cfg.mode);
    if (m != "ALL" && m != "2P" && m != "2P_3B" && m != "2P_1B" &&
        m != "2P_ALL" && m != "OFK" && m != "4P") {
        std::cerr << "Error:Unsupported --mode '" << cfg.mode
                  << "' (expected all, 2P, 2P_1B, 2P_ALL, OFK, or 4P)\n";
        return false;
    }

    return true;
}

std::vector<FrontierNode> buildInitialSeeds(const std::string &modeFilter)
{
    std::string m = toUpperAscii(modeFilter);
    std::vector<FrontierNode> seeds;

    auto addSeed = [&seeds](octi::OctiGameMode mode,
                            octi::OctiGameBases bases,
                            const std::string &label) {
        octi::OctiBoard board(mode, false, false, bases);
        board.suppressBoardUpdateNotification(true);
        octi::OctiGame *game = board.getGame();
        if (!game) {
            return;
        }
        game->start(octi::PLAYER_A);

        FrontierNode node;
        node.canonicalKey = octi::OctiBotEngine::computeCanonicalOpeningHash(
            board, &node.mirrored);
        node.ply = 0;
        node.variantLabel = label;
        node.positionStr = board.toPositionString();
        node.moveHistoryDesc = "initial";
        seeds.push_back(node);
    };

    if (m == "ALL" || m == "2P" || m == "2P_3B" || m == "2P_ALL") {
        addSeed(octi::GAME_MODE_2, octi::GAME_BASES_3, "2P 3-Base");
    }
    if (m == "ALL" || m == "2P_1B" || m == "2P_ALL") {
        addSeed(octi::GAME_MODE_2, octi::GAME_BASES_1, "2P 1-Base");
    }
    if (m == "ALL" || m == "OFK") {
        addSeed(octi::GAME_MODE_OFK, octi::GAME_BASES_1, "OFK");
    }
    if (m == "ALL" || m == "4P") {
        addSeed(octi::GAME_MODE_4, octi::GAME_BASES_3, "4P");
    }

    return seeds;
}

bool writeHeaderFile(const std::string &path,
                     const std::map<std::uint64_t, GeneratedBookEntry> &bookMap)
{
    std::filesystem::path fsPath(path);
    if (fsPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(fsPath.parent_path(), ec);
    }

    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: Failed to open C++ header output path: " << path << "\n";
        return false;
    }

    out << "/*\n"
        << " * OctiOpeningBookData.hxx\n"
        << " *\n"
        << " * Copyright (C) 2026, Charles Chiou\n"
        << " */\n\n"
        << "#ifndef OCTI_OPENING_BOOK_DATA_HXX\n"
        << "#define OCTI_OPENING_BOOK_DATA_HXX\n\n"
        << "#include <cstddef>\n"
        << "#include <cstdint>\n"
        << "#include \"OctiBotEngine.hxx\"\n\n"
        << "namespace octi {\n\n"
        << "static constexpr OpeningBookEntry g_openingBookEntries[] = {\n";

    std::size_t idx = 0;
    const std::size_t total = bookMap.size();
    for (const auto &kv : bookMap) {
        const GeneratedBookEntry &e = kv.second;
        std::size_t numCands = std::min<std::size_t>(3, e.candidates.size());
        out << "    // " << e.variantLabel << " ply " << e.ply
            << " (" << e.moveHistoryDesc << ")\n";
        out << "    { " << formatHexKey(e.canonicalKey) << "ULL, "
            << e.depth << ", " << numCands << ", { ";
        for (std::size_t c = 0; c < 3; ++c) {
            if (c > 0) {
                out << ", ";
            }
            if (c < numCands) {
                out << "{ \"" << e.candidates[c].move << "\", "
                    << e.candidates[c].score << " }";
            } else {
                out << "{ nullptr, 0 }";
            }
        }
        out << " } }" << (idx + 1 < total ? "," : "") << "\n";
        ++idx;
    }

    out << "};\n\n"
        << "static constexpr std::size_t g_openingBookEntryCount =\n"
        << "    sizeof(g_openingBookEntries) / sizeof(g_openingBookEntries[0]);\n\n"
        << "} // namespace octi\n\n"
        << "#endif // OCTI_OPENING_BOOK_DATA_HXX\n\n"
        << "/*\n"
        << " * Local variables:\n"
        << " * mode: C++\n"
        << " * c-file-style: \"BSD\"\n"
        << " * c-basic-offset: 4\n"
        << " * tab-width: 4\n"
        << " * indent-tabs-mode: nil\n"
        << " * End:\n"
        << " */\n";

    out.flush();
    return out.good();
}

bool writeJsonFile(const std::string &path,
                   const BookGenConfig &cfg,
                   const std::map<std::uint64_t, GeneratedBookEntry> &bookMap,
                   long elapsedMs)
{
    if (path.empty()) {
        return true;
    }

    std::filesystem::path fsPath(path);
    if (fsPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(fsPath.parent_path(), ec);
    }

    nlohmann::json root;
    root["generator"] = "octi_book_gen";
    root["mode"] = cfg.mode;
    root["maxPly"] = cfg.maxPly;
    root["searchDepth"] = cfg.searchDepth;
    root["timeBudgetMs"] = cfg.timeBudgetMs;
    root["botBranch"] = cfg.botBranch;
    root["oppBranch"] = cfg.oppBranch;
    root["threads"] = cfg.threads;
    root["elapsedMs"] = elapsedMs;
    root["entryCount"] = bookMap.size();

    nlohmann::json entriesArr = nlohmann::json::array();
    for (const auto &kv : bookMap) {
        const GeneratedBookEntry &e = kv.second;
        nlohmann::json item;
        item["canonicalKey"] = formatHexKey(e.canonicalKey);
        item["depth"] = e.depth;
        item["ply"] = e.ply;
        item["variant"] = e.variantLabel;
        item["history"] = e.moveHistoryDesc;
        item["position"] = e.positionStr;

        nlohmann::json candsArr = nlohmann::json::array();
        std::size_t numCands = std::min<std::size_t>(3, e.candidates.size());
        for (std::size_t c = 0; c < numCands; ++c) {
            nlohmann::json cj;
            cj["move"] = e.candidates[c].move;
            cj["score"] = e.candidates[c].score;
            candsArr.push_back(cj);
        }
        item["candidates"] = candsArr;
        entriesArr.push_back(item);
    }
    root["entries"] = entriesArr;

    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: Failed to open JSON output path: " << path << "\n";
        return false;
    }
    out << root.dump(2) << "\n";
    out.flush();
    return out.good();
}

bool writeJavaFile(const std::string &path,
                   const std::map<std::uint64_t, GeneratedBookEntry> &bookMap)
{
    if (path.empty()) {
        return true;
    }

    std::filesystem::path fsPath(path);
    if (fsPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(fsPath.parent_path(), ec);
    }

    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: Failed to open Java output path: " << path << "\n";
        return false;
    }

    out << "// Auto-generated by octi_book_gen\n"
        << "// Copyright (C) 2026, Charles Chiou\n\n"
        << "private static final OpeningBookEntry[] OPENING_BOOK = new OpeningBookEntry[] {\n";

    std::size_t idx = 0;
    const std::size_t total = bookMap.size();
    for (const auto &kv : bookMap) {
        const GeneratedBookEntry &e = kv.second;
        std::size_t numCands = std::min<std::size_t>(3, e.candidates.size());
        out << "    // " << e.variantLabel << " ply " << e.ply
            << " (" << e.moveHistoryDesc << ")\n";
        out << "    new OpeningBookEntry(" << formatHexKey(e.canonicalKey) << "L, "
            << e.depth << ", new String[]{";
        for (std::size_t c = 0; c < numCands; ++c) {
            if (c > 0) out << ", ";
            out << "\"" << e.candidates[c].move << "\"";
        }
        out << "}, new int[]{";
        for (std::size_t c = 0; c < numCands; ++c) {
            if (c > 0) out << ", ";
            out << e.candidates[c].score;
        }
        out << "})" << (idx + 1 < total ? "," : "") << "\n";
        ++idx;
    }
    out << "};\n";
    out.flush();
    return out.good();
}

} // namespace

int main(int argc, char **argv)
{
    BookGenConfig cfg;
    bool helpRequested = false;
    if (!parseArgs(argc, argv, cfg, helpRequested)) {
        printUsage(argv[0]);
        return 1;
    }
    if (helpRequested) {
        printUsage(argv[0]);
        return 0;
    }

    long totalStartMs = steadyClockMs();
    std::cout << "====================================================================\n"
              << "OCTI Offline Opening Playbook Generator (octi_book_gen)\n"
              << "  Mode           : " << cfg.mode << "\n"
              << "  Max Ply        : " << cfg.maxPly << "\n"
              << "  Search Depth   : " << cfg.searchDepth << "\n"
              << "  Time Budget    : " << cfg.timeBudgetMs << " ms/node\n"
              << "  Bot Branch     : " << cfg.botBranch << "\n"
              << "  Opp Branch     : " << cfg.oppBranch << "\n"
              << "  CPU Threads    : " << cfg.threads << "\n"
              << "  Out Header     : " << cfg.outHeader << "\n"
              << "  Out JSON       : " << cfg.outJson << "\n"
              << "====================================================================\n"
              << std::flush;

    std::vector<FrontierNode> frontier = buildInitialSeeds(cfg.mode);
    if (frontier.empty()) {
        std::cerr << "Error: No initial board positions seeded for mode '"
                  << cfg.mode << "'\n";
        return 1;
    }

    std::map<std::uint64_t, GeneratedBookEntry> bookMap;
    std::set<std::uint64_t> visitedKeys;

    for (int ply = 0; ply < cfg.maxPly; ++ply) {
        std::vector<FrontierNode> currentFrontier;
        currentFrontier.reserve(frontier.size());
        for (const auto &node : frontier) {
            if (visitedKeys.find(node.canonicalKey) == visitedKeys.end()) {
                visitedKeys.insert(node.canonicalKey);
                currentFrontier.push_back(node);
            }
        }

        if (currentFrontier.empty()) {
            std::cout << "[Ply " << ply << "/" << cfg.maxPly
                      << "] Frontier empty; stopping expansion.\n";
            break;
        }

        long plyStartMs = steadyClockMs();
        std::size_t numNodes = currentFrontier.size();
        int numWorkers = std::min<int>(cfg.threads, static_cast<int>(numNodes));
        int smpPerWorker = std::max(1, cfg.threads / numWorkers);

        bool expandChildren = (ply + 1 < cfg.maxPly);
        std::size_t storeCount = static_cast<std::size_t>(std::min(3, cfg.botBranch));
        std::size_t expandCount = expandChildren
            ? static_cast<std::size_t>(std::max(cfg.botBranch, cfg.oppBranch))
            : storeCount;
        std::size_t searchTopK = std::max(storeCount, expandCount);

        std::cout << "[Ply " << ply << "/" << cfg.maxPly << "] Evaluating "
                  << numNodes << " unique canonical position(s) across "
                  << numWorkers << " worker(s) x " << smpPerWorker
                  << " Lazy SMP thread(s) (depth=" << cfg.searchDepth
                  << ", topK=" << searchTopK << ")...\n" << std::flush;

        std::atomic<std::size_t> nextIdx{0};
        std::atomic<std::size_t> completedNodes{0};
        std::mutex resultMutex;
        std::vector<GeneratedBookEntry> plyEntries;
        std::vector<FrontierNode> nextFrontierRaw;

        auto workerFn = [&](int workerId) {
            octi::OctiBotEngine bot(octi::BotDifficulty::MASTER,
                                    0x5EED2026ULL + static_cast<std::uint64_t>(workerId) * 0x9E3779B9ULL);
            octi::BotTuningProfile prof = bot.tuningProfile();
            prof.useOpeningBook = false;
            prof.rootScoreJitter = 0;
            prof.lazySmpThreads = smpPerWorker;
            prof.maxSearchDepth = cfg.searchDepth;
            prof.timeBudgetMs = cfg.timeBudgetMs;
            bot.setTuningProfile(prof);

            while (true) {
                std::size_t idx = nextIdx.fetch_add(1);
                if (idx >= numNodes) {
                    break;
                }

                const FrontierNode &node = currentFrontier[idx];
                octi::OctiBoard board;
                board.suppressBoardUpdateNotification(true);
                board.fromPositionString(node.positionStr);
                octi::OctiGame *game = board.getGame();
                if (!game || game->isGameOver()) {
                    continue;
                }

                bool isMirrored = false;
                std::uint64_t verifyKey =
                    octi::OctiBotEngine::computeCanonicalOpeningHash(board, &isMirrored);

                octi::BotSearchStats stats;
                std::vector<octi::BookMoveScore> topMoves =
                    bot.searchTopMovesForBook(board,
                                              cfg.searchDepth,
                                              cfg.timeBudgetMs,
                                              searchTopK,
                                              &stats);
                if (topMoves.empty()) {
                    continue;
                }

                int boardWidth = board.getBoardWidth();
                GeneratedBookEntry entry;
                entry.canonicalKey = verifyKey;
                entry.depth = std::max(1, stats.depthReached);
                entry.ply = ply;
                entry.variantLabel = node.variantLabel;
                entry.positionStr = node.positionStr;
                entry.moveHistoryDesc = node.moveHistoryDesc;

                for (std::size_t i = 0;
                     i < topMoves.size() && entry.candidates.size() < storeCount;
                     ++i) {
                    std::string canonMove = isMirrored
                        ? octi::OctiBotEngine::mirrorMoveNotation(topMoves[i].move, boardWidth)
                        : topMoves[i].move;
                    if (canonMove.empty()) {
                        continue;
                    }
                    bool dup = false;
                    for (const auto &existing : entry.candidates) {
                        if (existing.move == canonMove) {
                            dup = true;
                            break;
                        }
                    }
                    if (!dup) {
                        entry.candidates.push_back({canonMove, topMoves[i].score});
                    }
                }

                std::vector<FrontierNode> localChildren;
                if (expandChildren) {
                    std::size_t lim = std::min(expandCount, topMoves.size());
                    for (std::size_t i = 0; i < lim; ++i) {
                        octi::OctiBoard childBoard;
                        childBoard.suppressBoardUpdateNotification(true);
                        childBoard.fromPositionString(node.positionStr);
                        octi::OctiGame *childGame = childBoard.getGame();
                        if (!childGame) {
                            continue;
                        }
                        try {
                            childGame->move(topMoves[i].move);
                        } catch (...) {
                            continue;
                        }
                        if (childGame->isGameOver()) {
                            continue;
                        }
                        FrontierNode childNode;
                        childNode.canonicalKey =
                            octi::OctiBotEngine::computeCanonicalOpeningHash(
                                childBoard, &childNode.mirrored);
                        childNode.ply = ply + 1;
                        childNode.variantLabel = node.variantLabel;
                        childNode.positionStr = childBoard.toPositionString();
                        childNode.moveHistoryDesc =
                            (node.moveHistoryDesc == "initial")
                                ? ("after " + topMoves[i].move)
                                : (node.moveHistoryDesc + " " + topMoves[i].move);
                        localChildren.push_back(childNode);
                    }
                }

                std::size_t doneCount = completedNodes.fetch_add(1) + 1;
                {
                    std::lock_guard<std::mutex> lock(resultMutex);
                    if (!entry.candidates.empty()) {
                        plyEntries.push_back(entry);
                    }
                    for (const auto &ch : localChildren) {
                        nextFrontierRaw.push_back(ch);
                    }
                    if (doneCount == numNodes || doneCount % 5 == 0 || numNodes <= 10) {
                        std::cout << "  [" << doneCount << "/" << numNodes << "] "
                                  << node.variantLabel << " ply " << ply
                                  << " (" << node.moveHistoryDesc << ") -> "
                                  << (entry.candidates.empty() ? "-" : entry.candidates[0].move)
                                  << " (score="
                                  << (entry.candidates.empty() ? 0 : entry.candidates[0].score)
                                  << ", d=" << entry.depth
                                  << ", nodes=" << stats.nodesVisited
                                  << ", " << stats.elapsedMs << "ms)\n"
                                  << std::flush;
                    }
                }
            }
        };

        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(numWorkers));
        for (int w = 0; w < numWorkers; ++w) {
            workers.emplace_back(workerFn, w);
        }
        for (auto &th : workers) {
            if (th.joinable()) {
                th.join();
            }
        }

        for (const auto &e : plyEntries) {
            bookMap[e.canonicalKey] = e;
        }

        long plyElapsedMs = steadyClockMs() - plyStartMs;
        std::cout << "[Ply " << ply << "/" << cfg.maxPly << "] Completed in "
                  << plyElapsedMs << " ms (cumulative book entries: "
                  << bookMap.size() << ")\n" << std::flush;

        frontier.swap(nextFrontierRaw);
    }

    long totalElapsedMs = steadyClockMs() - totalStartMs;

    if (!writeHeaderFile(cfg.outHeader, bookMap)) {
        return 1;
    }
    if (!writeJsonFile(cfg.outJson, cfg, bookMap, totalElapsedMs)) {
        return 1;
    }
    if (!writeJavaFile(cfg.outJava, bookMap)) {
        return 1;
    }

    std::cout << "====================================================================\n"
              << "Playbook Generation Complete!\n"
              << "  Total Unique Canonical Entries : " << bookMap.size() << "\n"
              << "  Total Elapsed Time             : " << totalElapsedMs << " ms\n"
              << "  C++ Header Written             : " << cfg.outHeader << "\n"
              << "  JSON Artifact Written          : " << cfg.outJson << "\n";
    if (!cfg.outJava.empty()) {
        std::cout << "  Java Snippet Written           : " << cfg.outJava << "\n";
    }
    std::cout << "====================================================================\n"
              << std::flush;

    return 0;
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
