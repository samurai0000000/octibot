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

int parseMakeflagsThreads(const std::string &mf, int hwThreads)
{
    int fallbackHw = std::max(1, hwThreads);
    std::istringstream iss(mf);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) {
        tokens.push_back(tok);
    }
    if (tokens.empty()) {
        return fallbackHw;
    }

    auto isAllDigits = [](const std::string &s) -> bool {
        if (s.empty()) {
            return false;
        }
        for (char c : s) {
            if (c < '0' || c > '9') {
                return false;
            }
        }
        return true;
    };

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string &t = tokens[i];
        if (t == "-j" || t == "j") {
            if (i + 1 < tokens.size() && isAllDigits(tokens[i + 1])) {
                int val = std::atoi(tokens[i + 1].c_str());
                if (val >= 1) {
                    return val;
                }
            }
            return fallbackHw;
        }
        if (t.size() > 2 && t.compare(0, 2, "-j") == 0) {
            std::string numPart = t.substr(2);
            if (isAllDigits(numPart)) {
                int val = std::atoi(numPart.c_str());
                if (val >= 1) {
                    return val;
                }
            }
        }
    }

    return 1;
}

int detectDefaultThreads()
{
    int hw = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    const char *mf = std::getenv("MAKEFLAGS");
    if (!mf) {
        return hw;
    }
    return parseMakeflagsThreads(mf, hw);
}

struct BookGenConfig {
    std::string mode = "all";
    bool superProngs = false;
    bool edgeless = false;
    int maxPly = 6;
    int searchDepth = 9;
    int timeBudgetMs = 0;
    int botBranch = 3;
    int oppBranch = 6;
    int threads = detectDefaultThreads();
    int threadsPerWorker = 0;
    int checkpointInterval = 10;
    bool noResume = false;
    std::string outHeader = ".book/OctiOpeningBookData.hxx";
    std::string outJson = ".book/opening_book.json";
    std::string outJava = ".book/OctiOpeningBookData.java";
    std::vector<std::string> outJs;
    std::string fromJson;
    std::set<std::uint64_t> reSearchKeys;
    std::vector<std::uint64_t> reSearchKeyOrder;
    bool reSearchOnly = false;
    bool explicitMode = false;
    bool explicitSuperProngs = false;
    bool explicitEdgeless = false;
    bool explicitMaxPly = false;
    bool explicitSearchDepth = false;
    bool explicitTimeBudgetMs = false;
    bool explicitBotBranch = false;
    bool explicitOppBranch = false;
    bool explicitOutHeader = false;
    bool explicitOutJson = false;
    bool explicitOutJava = false;
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
    std::vector<std::string> expandedMoves;
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
        << "                               all        = 2P (3-Base & 1-Base), OFK (7x7), 4P (default)\n"
        << "                               all13      = All 13 variants (2P 3B/1B & 4P x SP/ED + OFK)\n"
        << "                               new9       = All 9 non-standard SP/ED variants\n"
        << "                               2P / 2P_3B = 2-Player 3-Base standard\n"
        << "                               2P_3B_SP   = 2-Player 3-Base with superprongs\n"
        << "                               2P_3B_ED   = 2-Player 3-Base on edgeless board\n"
        << "                               2P_3B_SPED = 2-Player 3-Base with superprongs + edgeless\n"
        << "                               2P_1B      = 2-Player 1-Base fast victory\n"
        << "                               2P_1B_SP   = 2-Player 1-Base with superprongs\n"
        << "                               2P_1B_ED   = 2-Player 1-Base on edgeless board\n"
        << "                               2P_1B_SPED = 2-Player 1-Base with superprongs + edgeless\n"
        << "                               2P_ALL     = Both 2P 3-Base and 2P 1-Base\n"
        << "                               4P         = 4-Player 2v2 Coalitions\n"
        << "                               4P_SP      = 4-Player with superprongs\n"
        << "                               4P_ED      = 4-Player on edgeless board\n"
        << "                               4P_SPED    = 4-Player with superprongs + edgeless\n"
        << "                               OFK        = OCTI for Kids (6x7)\n"
        << "  --super-prongs             Enable superprongs rule for base mode(s) (default: off)\n"
        << "  --edgeless                 Enable edgeless wraparound board for base mode(s) (default: off)\n"
        << "  --max-ply <int>            Number of opening plies to expand (default: 6)\n"
        << "  --search-depth <int>       Alpha-beta search depth per position (default: 9)\n"
        << "  --time-budget-ms <int>     Max wall-clock budget per position in ms, 0=unlimited (default: 0)\n"
        << "  --bot-branch <int>         Top candidate moves stored per book entry (default: 3)\n"
        << "  --opp-branch <int>         Top replies expanded into next ply frontier (default: 6)\n"
        << "  --threads <int>            Total CPU worker threads (default: from MAKEFLAGS -jN or nproc)\n"
        << "  --threads-per-worker <N>   Fixed Lazy SMP threads per worker, 0=auto-split (default: 0)\n"
        << "  --checkpoint-interval <N>  Intra-ply checkpoint interval in nodes (default: 10)\n"
        << "  --resume                   Resume from existing --out-json checkpoint if present (default)\n"
        << "  --no-resume, --fresh       Ignore existing --out-json and start fresh from ply 0\n"
        << "  --out-header <path>        Output C++ header path (default: .book/OctiOpeningBookData.hxx)\n"
        << "  --out-json <path>          Output JSON playbook path (default: .book/opening_book.json)\n"
        << "  --out-java <path>          Output Java class path (default: .book/OctiOpeningBookData.java)\n"
        << "  --out-js <path>            Repeatable JS/HTML/C++ file path with BEGIN/END_OPENING_BOOK_DATA markers\n"
        << "  --from-json <path>         Load existing JSON playbook and write requested outputs without searching\n"
        << "  --re-search-key <hex>      Repeatable canonical key to force re-searching at --search-depth\n"
        << "  --re-search-keys-file <f>  File with one hex canonical key per line to force re-searching\n"
        << "  --re-search-only           Re-search existing matching book entries in place without expanding new children\n\n"
        << "Recommended High-Core Profiles:\n"
        << "  1. Standard Book (6 plies, depth 9, pure depth-bound):\n"
        << "     ./core/build/octi_book_gen --mode all --max-ply 6 --search-depth 9 \\\n"
        << "         --bot-branch 3 --opp-branch 6\n\n"
        << "  2. Deep Tournament Book (8 plies, depth 10):\n"
        << "     ./core/build/octi_book_gen --mode all --max-ply 8 --search-depth 10 \\\n"
        << "         --bot-branch 3 --opp-branch 8\n\n"
        << "  3. Ultra-Deep Book (8 plies, depth 12):\n"
        << "     ./core/build/octi_book_gen --mode all --max-ply 8 --search-depth 12 \\\n"
        << "         --bot-branch 3 --opp-branch 10\n";
}

bool parseArgs(int argc, char **argv, BookGenConfig &cfg, bool &helpRequested)
{
    helpRequested = false;
    std::error_code cwdEc;
    std::filesystem::path cwd = std::filesystem::current_path(cwdEc);
    if (!cwdEc && (cwd.filename() == ".book" ||
                   (std::filesystem::exists("opening_book.json", cwdEc) &&
                    !std::filesystem::exists(".book/opening_book.json", cwdEc)))) {
        cfg.outHeader = "OctiOpeningBookData.hxx";
        cfg.outJson = "opening_book.json";
        cfg.outJava = "OctiOpeningBookData.java";
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            helpRequested = true;
            return true;
        } else if (arg == "--mode" && i + 1 < argc) {
            cfg.mode = argv[++i];
            cfg.explicitMode = true;
        } else if (arg == "--super-prongs") {
            cfg.superProngs = true;
            cfg.explicitSuperProngs = true;
        } else if (arg == "--edgeless") {
            cfg.edgeless = true;
            cfg.explicitEdgeless = true;
        } else if (arg == "--max-ply" && i + 1 < argc) {
            cfg.maxPly = std::atoi(argv[++i]);
            cfg.explicitMaxPly = true;
        } else if (arg == "--search-depth" && i + 1 < argc) {
            cfg.searchDepth = std::atoi(argv[++i]);
            cfg.explicitSearchDepth = true;
        } else if (arg == "--time-budget-ms" && i + 1 < argc) {
            cfg.timeBudgetMs = std::atoi(argv[++i]);
            cfg.explicitTimeBudgetMs = true;
        } else if (arg == "--bot-branch" && i + 1 < argc) {
            cfg.botBranch = std::atoi(argv[++i]);
            cfg.explicitBotBranch = true;
        } else if (arg == "--opp-branch" && i + 1 < argc) {
            cfg.oppBranch = std::atoi(argv[++i]);
            cfg.explicitOppBranch = true;
        } else if (arg == "--threads" && i + 1 < argc) {
            cfg.threads = std::atoi(argv[++i]);
        } else if (arg == "--threads-per-worker" && i + 1 < argc) {
            cfg.threadsPerWorker = std::atoi(argv[++i]);
        } else if (arg == "--checkpoint-interval" && i + 1 < argc) {
            cfg.checkpointInterval = std::atoi(argv[++i]);
        } else if (arg == "--resume") {
            cfg.noResume = false;
        } else if (arg == "--no-resume" || arg == "--fresh") {
            cfg.noResume = true;
        } else if (arg == "--out-header" && i + 1 < argc) {
            cfg.outHeader = argv[++i];
            cfg.explicitOutHeader = true;
        } else if (arg == "--out-json" && i + 1 < argc) {
            cfg.outJson = argv[++i];
            cfg.explicitOutJson = true;
        } else if (arg == "--out-java" && i + 1 < argc) {
            cfg.outJava = argv[++i];
            cfg.explicitOutJava = true;
        } else if (arg == "--out-js" && i + 1 < argc) {
            cfg.outJs.push_back(argv[++i]);
        } else if (arg == "--from-json" && i + 1 < argc) {
            cfg.fromJson = argv[++i];
        } else if (arg == "--re-search-key" && i + 1 < argc) {
            std::string kStr = argv[++i];
            try {
                std::uint64_t k = std::stoull(kStr, nullptr, 16);
                if (cfg.reSearchKeys.insert(k).second) {
                    cfg.reSearchKeyOrder.push_back(k);
                }
            } catch (...) {
                std::cerr << "Error: Invalid hex key for --re-search-key: '" << kStr << "'\n";
                return false;
            }
        } else if (arg == "--re-search-keys-file" && i + 1 < argc) {
            std::string fPath = argv[++i];
            std::ifstream kf(fPath);
            if (!kf.is_open()) {
                std::cerr << "Error: Failed to open --re-search-keys-file: '" << fPath << "'\n";
                return false;
            }
            std::string line;
            while (std::getline(kf, line)) {
                std::istringstream iss(line);
                std::string tok;
                if (iss >> tok && !tok.empty() && tok[0] != '#') {
                    try {
                        std::uint64_t k = std::stoull(tok, nullptr, 16);
                        if (cfg.reSearchKeys.insert(k).second) {
                            cfg.reSearchKeyOrder.push_back(k);
                        }
                    } catch (...) {
                        std::cerr << "Error: Invalid hex key in '" << fPath << "': '" << tok << "'\n";
                        return false;
                    }
                }
            }
        } else if (arg == "--re-search-only") {
            cfg.reSearchOnly = true;
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
    if (cfg.timeBudgetMs < 0) {
        std::cerr << "Error: --time-budget-ms must be >= 0 (0 = unlimited)\n";
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
    static const std::set<std::string> kValidModes = {
        "ALL", "ALL13", "NEW9",
        "2P", "2P_3B", "2P_3B_SP", "2P_3B_ED", "2P_3B_SPED",
        "2P_1B", "2P_1B_SP", "2P_1B_ED", "2P_1B_SPED",
        "2P_ALL",
        "4P", "4P_SP", "4P_ED", "4P_SPED",
        "OFK"
    };
    if (kValidModes.find(m) == kValidModes.end()) {
        std::cerr << "Error: Unsupported --mode '" << cfg.mode << "'\n";
        return false;
    }
    if (m == "OFK" && (cfg.superProngs || cfg.edgeless)) {
        std::cerr << "Error: OFK mode does not support --super-prongs or --edgeless\n";
        return false;
    }

    return true;
}

struct VariantSpec {
    const char *code;
    const char *label;
    octi::OctiGameMode mode;
    octi::OctiGameBases bases;
    bool superProngs;
    bool edgeless;
};

const std::vector<VariantSpec> &allVariantSpecs()
{
    static const std::vector<VariantSpec> kSpecs = {
        { "2P_3B",      "2P 3-Base",  octi::GAME_MODE_2,   octi::GAME_BASES_3, false, false },
        { "2P_3B_SP",   "2P_3B_SP",   octi::GAME_MODE_2,   octi::GAME_BASES_3, true,  false },
        { "2P_3B_ED",   "2P_3B_ED",   octi::GAME_MODE_2,   octi::GAME_BASES_3, false, true  },
        { "2P_3B_SPED", "2P_3B_SPED", octi::GAME_MODE_2,   octi::GAME_BASES_3, true,  true  },
        { "2P_1B",      "2P 1-Base",  octi::GAME_MODE_2,   octi::GAME_BASES_1, false, false },
        { "2P_1B_SP",   "2P_1B_SP",   octi::GAME_MODE_2,   octi::GAME_BASES_1, true,  false },
        { "2P_1B_ED",   "2P_1B_ED",   octi::GAME_MODE_2,   octi::GAME_BASES_1, false, true  },
        { "2P_1B_SPED", "2P_1B_SPED", octi::GAME_MODE_2,   octi::GAME_BASES_1, true,  true  },
        { "4P",         "4P",         octi::GAME_MODE_4,   octi::GAME_BASES_3, false, false },
        { "4P_SP",      "4P_SP",      octi::GAME_MODE_4,   octi::GAME_BASES_3, true,  false },
        { "4P_ED",      "4P_ED",      octi::GAME_MODE_4,   octi::GAME_BASES_3, false, true  },
        { "4P_SPED",    "4P_SPED",    octi::GAME_MODE_4,   octi::GAME_BASES_3, true,  true  },
        { "OFK",        "OFK",        octi::GAME_MODE_OFK, octi::GAME_BASES_1, false, false }
    };
    return kSpecs;
}

bool specMatchesConfig(const VariantSpec &spec, const BookGenConfig &cfg)
{
    std::string m = toUpperAscii(cfg.mode);
    if (m == "ALL13") {
        if (cfg.explicitSuperProngs && spec.superProngs != cfg.superProngs) {
            return false;
        }
        if (cfg.explicitEdgeless && spec.edgeless != cfg.edgeless) {
            return false;
        }
        return true;
    }
    if (m == "NEW9") {
        return spec.superProngs || spec.edgeless;
    }
    if (m == "ALL") {
        return spec.superProngs == cfg.superProngs &&
               spec.edgeless == cfg.edgeless;
    }
    if (m == "2P" || m == "2P_3B") {
        return spec.mode == octi::GAME_MODE_2 &&
               spec.bases == octi::GAME_BASES_3 &&
               spec.superProngs == cfg.superProngs &&
               spec.edgeless == cfg.edgeless;
    }
    if (m == "2P_1B") {
        return spec.mode == octi::GAME_MODE_2 &&
               spec.bases == octi::GAME_BASES_1 &&
               spec.superProngs == cfg.superProngs &&
               spec.edgeless == cfg.edgeless;
    }
    if (m == "2P_ALL") {
        return spec.mode == octi::GAME_MODE_2 &&
               spec.superProngs == cfg.superProngs &&
               spec.edgeless == cfg.edgeless;
    }
    if (m == "4P") {
        return spec.mode == octi::GAME_MODE_4 &&
               spec.superProngs == cfg.superProngs &&
               spec.edgeless == cfg.edgeless;
    }
    if (m == "OFK") {
        return spec.mode == octi::GAME_MODE_OFK &&
               !cfg.superProngs && !cfg.edgeless;
    }
    return m == spec.code;
}

bool variantMatchesConfig(const std::string &variantLabel,
                          const BookGenConfig &cfg)
{
    if (variantLabel.empty()) {
        return true;
    }
    for (const auto &spec : allVariantSpecs()) {
        if (variantLabel == spec.label || variantLabel == spec.code) {
            return specMatchesConfig(spec, cfg);
        }
    }
    return false;
}

std::vector<FrontierNode> buildInitialSeeds(const BookGenConfig &cfg)
{
    std::vector<FrontierNode> seeds;

    for (const auto &spec : allVariantSpecs()) {
        if (!specMatchesConfig(spec, cfg)) {
            continue;
        }
        octi::OctiBoard board(spec.mode, spec.superProngs, spec.edgeless, spec.bases);
        board.suppressBoardUpdateNotification(true);
        octi::OctiGame *game = board.getGame();
        if (!game) {
            continue;
        }
        game->start(octi::PLAYER_A);

        FrontierNode node;
        node.canonicalKey = octi::OctiBotEngine::computeCanonicalOpeningHash(
            board, &node.mirrored);
        node.ply = 0;
        node.variantLabel = spec.label;
        node.positionStr = board.toPositionString();
        node.moveHistoryDesc = "initial";
        seeds.push_back(node);
    }

    return seeds;
}

std::vector<FrontierNode> expandEntryChildren(
    const FrontierNode &node,
    const GeneratedBookEntry &entry,
    std::size_t expandCount)
{
    std::vector<FrontierNode> children;
    const std::string &posStr =
        !entry.positionStr.empty() ? entry.positionStr : node.positionStr;
    if (posStr.empty() || expandCount == 0) {
        return children;
    }

    octi::OctiBoard baseBoard;
    baseBoard.suppressBoardUpdateNotification(true);
    try {
        baseBoard.fromPositionString(posStr);
    } catch (...) {
        return children;
    }
    octi::OctiGame *baseGame = baseBoard.getGame();
    if (!baseGame || baseGame->isGameOver()) {
        return children;
    }

    std::vector<std::string> movesToExpand;
    if (!entry.expandedMoves.empty()) {
        movesToExpand = entry.expandedMoves;
    } else {
        bool isMirrored = false;
        octi::OctiBotEngine::computeCanonicalOpeningHash(baseBoard, &isMirrored);
        int boardWidth = baseBoard.getBoardWidth();
        for (const auto &cand : entry.candidates) {
            std::string boardMove = isMirrored
                ? octi::OctiBotEngine::mirrorMoveNotation(cand.move, boardWidth)
                : cand.move;
            if (!boardMove.empty()) {
                movesToExpand.push_back(boardMove);
            }
        }
    }

    const std::string &variantLabel =
        !node.variantLabel.empty() ? node.variantLabel : entry.variantLabel;
    const std::string &historyBase =
        !node.moveHistoryDesc.empty() ? node.moveHistoryDesc : entry.moveHistoryDesc;

    std::size_t lim = std::min(expandCount, movesToExpand.size());
    for (std::size_t i = 0; i < lim; ++i) {
        const std::string &moveStr = movesToExpand[i];
        if (moveStr.empty()) {
            continue;
        }
        octi::OctiBoard childBoard;
        childBoard.suppressBoardUpdateNotification(true);
        try {
            childBoard.fromPositionString(posStr);
        } catch (...) {
            continue;
        }
        octi::OctiGame *childGame = childBoard.getGame();
        if (!childGame) {
            continue;
        }
        try {
            childGame->move(moveStr);
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
        childNode.ply = node.ply + 1;
        childNode.variantLabel = variantLabel;
        childNode.positionStr = childBoard.toPositionString();
        childNode.moveHistoryDesc =
            (historyBase.empty() || historyBase == "initial")
                ? ("after " + moveStr)
                : (historyBase + " " + moveStr);
        children.push_back(childNode);
    }

    return children;
}

bool atomicWriteTextFile(const std::string &path,
                         const std::string &content,
                         const char *label)
{
    if (path.empty()) {
        return true;
    }

    std::filesystem::path fsPath(path);
    if (fsPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(fsPath.parent_path(), ec);
    }

    std::string tmpPath = path + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "Error: Failed to open " << label
                      << " output path: " << path << "\n";
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out.good()) {
            std::cerr << "Error: Failed to write " << label
                      << " output path: " << path << "\n";
            out.close();
            std::error_code rmEc;
            std::filesystem::remove(tmpPath, rmEc);
            return false;
        }
    }

    std::error_code renEc;
    std::filesystem::rename(tmpPath, path, renEc);
    if (renEc) {
        std::error_code cpEc;
        std::filesystem::copy_file(
            tmpPath, path,
            std::filesystem::copy_options::overwrite_existing, cpEc);
        std::error_code rmEc;
        std::filesystem::remove(tmpPath, rmEc);
        if (cpEc) {
            std::cerr << "Error: Failed to commit " << label
                      << " output path: " << path << "\n";
            return false;
        }
    }
    return true;
}

std::int16_t clampScore16(std::int32_t s)
{
    if (s > 32000) return 32000;
    if (s < -32000) return -32000;
    return static_cast<std::int16_t>(s);
}

bool writeHeaderFile(const std::string &path,
                     const std::map<std::uint64_t, GeneratedBookEntry> &bookMap)
{
    if (path.empty()) {
        return true;
    }

    std::ostringstream out;
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
                    << clampScore16(e.candidates[c].score) << " }";
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

    return atomicWriteTextFile(path, out.str(), "C++ header");
}

bool writeJsonFile(const std::string &path,
                   const BookGenConfig &cfg,
                   const std::map<std::uint64_t, GeneratedBookEntry> &bookMap,
                   long elapsedMs)
{
    if (path.empty()) {
        return true;
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
            cj["score"] = clampScore16(e.candidates[c].score);
            candsArr.push_back(cj);
        }
        item["candidates"] = candsArr;
        if (!e.expandedMoves.empty()) {
            item["expandedMoves"] = e.expandedMoves;
        }
        entriesArr.push_back(item);
    }
    root["entries"] = entriesArr;

    return atomicWriteTextFile(path, root.dump(2) + "\n", "JSON");
}

bool writeJavaFile(const std::string &path,
                   const std::map<std::uint64_t, GeneratedBookEntry> &bookMap)
{
    if (path.empty()) {
        return true;
    }

    static constexpr std::size_t kChunkSize = 550;
    const std::size_t total = bookMap.size();
    const std::size_t numChunks = (total == 0) ? 1 : ((total + kChunkSize - 1) / kChunkSize);

    std::ostringstream out;
    out << "/*\n"
        << " * OctiOpeningBookData.java\n"
        << " *\n"
        << " * Copyright (C) 2026, Charles Chiou\n"
        << " */\n\n"
        << "package com.selfso.octi;\n\n"
        << "public final class OctiOpeningBookData {\n\n"
        << "    private OctiOpeningBookData() {\n"
        << "    }\n\n"
        << "    public static final OctiBotEngine.OpeningBookEntry[] ENTRIES = buildEntries();\n\n"
        << "    private static OctiBotEngine.OpeningBookEntry[] buildEntries() {\n";

    for (std::size_t ch = 0; ch < numChunks; ++ch) {
        out << "        OctiBotEngine.OpeningBookEntry[] c" << ch << " = chunk" << ch << "();\n";
    }
    out << "        OctiBotEngine.OpeningBookEntry[] all =\n"
        << "                new OctiBotEngine.OpeningBookEntry[";
    for (std::size_t ch = 0; ch < numChunks; ++ch) {
        out << (ch > 0 ? " + " : "") << "c" << ch << ".length";
    }
    out << "];\n"
        << "        int pos = 0;\n";
    for (std::size_t ch = 0; ch < numChunks; ++ch) {
        out << "        System.arraycopy(c" << ch << ", 0, all, pos, c" << ch << ".length);\n"
            << "        pos += c" << ch << ".length;\n";
    }
    out << "        return all;\n"
        << "    }\n\n";

    auto it = bookMap.begin();
    for (std::size_t ch = 0; ch < numChunks; ++ch) {
        std::size_t chunkStart = ch * kChunkSize;
        std::size_t chunkEnd = std::min(total, chunkStart + kChunkSize);
        out << "    private static OctiBotEngine.OpeningBookEntry[] chunk" << ch << "() {\n"
            << "        return new OctiBotEngine.OpeningBookEntry[] {\n";
        for (std::size_t idx = chunkStart; idx < chunkEnd && it != bookMap.end(); ++idx, ++it) {
            const GeneratedBookEntry &e = it->second;
            std::size_t numCands = std::min<std::size_t>(3, e.candidates.size());
            out << "        // " << e.variantLabel << " ply " << e.ply
                << " (" << e.moveHistoryDesc << ")\n";
            out << "        new OctiBotEngine.OpeningBookEntry("
                << formatHexKey(e.canonicalKey) << "L, " << e.depth;
            for (std::size_t c = 0; c < numCands; ++c) {
                out << ",\n            new OctiBotEngine.OpeningBookCandidate(\""
                    << e.candidates[c].move << "\", "
                    << clampScore16(e.candidates[c].score) << ")";
            }
            out << ")" << (idx + 1 < chunkEnd ? "," : "") << "\n";
        }
        out << "        };\n"
            << "    }\n"
            << (ch + 1 < numChunks ? "\n" : "");
    }
    out << "}\n";

    return atomicWriteTextFile(path, out.str(), "Java");
}

bool updateJsFileWithMarkers(
    const std::string &path,
    const std::map<std::uint64_t, GeneratedBookEntry> &bookMap)
{
    if (path.empty()) {
        return true;
    }
    if (!std::filesystem::exists(path)) {
        if (path.find("www/") != std::string::npos) {
            std::cout << "Note: Skipping non-existent optional web path: "
                      << path << "\n";
            return true;
        }
        std::cerr << "Error: Target JS file does not exist: " << path << "\n";
        return false;
    }

    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Error: Failed to open JS file for reading: " << path << "\n";
        return false;
    }
    std::ostringstream contentsOss;
    contentsOss << in.rdbuf();
    std::string content = contentsOss.str();
    in.close();

    const std::string beginMarker = "// BEGIN_OPENING_BOOK_DATA";
    const std::string endMarker = "// END_OPENING_BOOK_DATA";
    std::size_t beginPos = content.find(beginMarker);
    std::size_t endPos = content.find(endMarker);
    if (beginPos == std::string::npos || endPos == std::string::npos ||
        beginPos >= endPos) {
        std::cerr << "Error: Missing or invalid OPENING_BOOK_DATA markers in: "
                  << path << "\n";
        return false;
    }

    std::ostringstream jsBlock;
    jsBlock << beginMarker << "\n"
            << "var OPENING_BOOK = {\n";
    std::size_t idx = 0;
    const std::size_t total = bookMap.size();
    for (const auto &kv : bookMap) {
        const GeneratedBookEntry &e = kv.second;
        std::size_t numCands = std::min<std::size_t>(3, e.candidates.size());
        if (!e.variantLabel.empty()) {
            jsBlock << "  // " << e.variantLabel << " ply " << e.ply
                    << " (" << e.moveHistoryDesc << ")\n";
        }
        jsBlock << "  \"" << formatHexKey(e.canonicalKey) << "\": {\n"
                << "    depth: " << e.depth << ",\n"
                << "    candidates: [\n";
        for (std::size_t c = 0; c < numCands; ++c) {
            jsBlock << "      { move: \"" << e.candidates[c].move
                    << "\", score: " << clampScore16(e.candidates[c].score) << " }"
                    << (c + 1 < numCands ? "," : "") << "\n";
        }
        jsBlock << "    ]\n"
                << "  }" << (idx + 1 < total ? "," : "") << "\n";
        ++idx;
    }
    jsBlock << "};\n" << endMarker;

    std::string updated = content.substr(0, beginPos) +
                          jsBlock.str() +
                          content.substr(endPos + endMarker.size());

    return atomicWriteTextFile(path, updated, "JS");
}

bool loadJsonFile(const std::string &path,
                  std::map<std::uint64_t, GeneratedBookEntry> &bookMap,
                  BookGenConfig *inoutCfg = nullptr,
                  long *outElapsedMs = nullptr)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "Error: Failed to open input JSON playbook: " << path << "\n";
        return false;
    }
    nlohmann::json root;
    try {
        in >> root;
    } catch (const std::exception &ex) {
        std::cerr << "Error: Failed to parse JSON playbook '" << path
                  << "': " << ex.what() << "\n";
        return false;
    }
    if (!root.is_object() || !root.contains("entries") ||
        !root["entries"].is_array()) {
        std::cerr << "Error: Invalid JSON playbook structure in: " << path << "\n";
        return false;
    }

    if (outElapsedMs) {
        *outElapsedMs = root.value("elapsedMs", 0L);
    }
    if (inoutCfg) {
        if (!inoutCfg->explicitMode && root.contains("mode") &&
            root["mode"].is_string()) {
            inoutCfg->mode = root["mode"].get<std::string>();
        }
        if (!inoutCfg->explicitMaxPly && root.contains("maxPly") &&
            root["maxPly"].is_number_integer()) {
            int v = root["maxPly"].get<int>();
            if (v >= 1 && v <= 20) {
                inoutCfg->maxPly = v;
            }
        }
        if (!inoutCfg->explicitSearchDepth && root.contains("searchDepth") &&
            root["searchDepth"].is_number_integer()) {
            int v = root["searchDepth"].get<int>();
            if (v >= 1 && v <= 30) {
                inoutCfg->searchDepth = v;
            }
        }
        if (!inoutCfg->explicitTimeBudgetMs && root.contains("timeBudgetMs") &&
            root["timeBudgetMs"].is_number_integer()) {
            int v = root["timeBudgetMs"].get<int>();
            if (v >= 0) {
                inoutCfg->timeBudgetMs = v;
            }
        }
        if (!inoutCfg->explicitBotBranch && root.contains("botBranch") &&
            root["botBranch"].is_number_integer()) {
            int v = root["botBranch"].get<int>();
            if (v >= 1 && v <= 16) {
                inoutCfg->botBranch = v;
            }
        }
        if (!inoutCfg->explicitOppBranch && root.contains("oppBranch") &&
            root["oppBranch"].is_number_integer()) {
            int v = root["oppBranch"].get<int>();
            if (v >= 1 && v <= 64) {
                inoutCfg->oppBranch = v;
            }
        }
    }

    bookMap.clear();
    for (const auto &item : root["entries"]) {
        if (!item.is_object() || !item.contains("canonicalKey")) {
            continue;
        }
        GeneratedBookEntry e;
        std::string keyStr = item["canonicalKey"].get<std::string>();
        try {
            e.canonicalKey = std::stoull(keyStr, nullptr, 16);
        } catch (...) {
            continue;
        }
        e.depth = item.value("depth", 1);
        e.ply = item.value("ply", 0);
        e.variantLabel = item.value("variant", std::string(""));
        e.moveHistoryDesc = item.value("history", std::string(""));
        e.positionStr = item.value("position", std::string(""));
        if (!e.positionStr.empty()) {
            try {
                octi::OctiBoard b;
                b.suppressBoardUpdateNotification(true);
                b.fromPositionString(e.positionStr);
                if (b.getGame()) {
                    e.canonicalKey =
                        octi::OctiBotEngine::computeCanonicalOpeningHash(b);
                }
            } catch (...) {
            }
        }
        if (item.contains("candidates") && item["candidates"].is_array()) {
            for (const auto &cj : item["candidates"]) {
                if (!cj.is_object() || !cj.contains("move")) {
                    continue;
                }
                octi::BookMoveScore bms;
                bms.move = cj["move"].get<std::string>();
                bms.score = clampScore16(cj.value("score", 0));
                e.candidates.push_back(bms);
            }
        }
        if (item.contains("expandedMoves") && item["expandedMoves"].is_array()) {
            for (const auto &mj : item["expandedMoves"]) {
                if (mj.is_string()) {
                    std::string ms = mj.get<std::string>();
                    if (!ms.empty()) {
                        e.expandedMoves.push_back(ms);
                    }
                }
            }
        }
        if (!e.candidates.empty()) {
            bookMap[e.canonicalKey] = e;
        }
    }
    return true;
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

    if (!cfg.fromJson.empty()) {
        std::map<std::uint64_t, GeneratedBookEntry> bookMap;
        long loadedElapsedMs = 0;
        if (!loadJsonFile(cfg.fromJson, bookMap, &cfg, &loadedElapsedMs)) {
            return 1;
        }
        if (cfg.explicitOutHeader && !writeHeaderFile(cfg.outHeader, bookMap)) {
            return 1;
        }
        if (cfg.explicitOutJson &&
            !writeJsonFile(cfg.outJson, cfg, bookMap, loadedElapsedMs)) {
            return 1;
        }
        if (cfg.explicitOutJava && !writeJavaFile(cfg.outJava, bookMap)) {
            return 1;
        }
        for (const auto &jsPath : cfg.outJs) {
            if (!updateJsFileWithMarkers(jsPath, bookMap)) {
                return 1;
            }
        }
        std::cout << "Synced " << bookMap.size()
                  << " opening book entries from " << cfg.fromJson << "\n";
        return 0;
    }

    std::map<std::uint64_t, GeneratedBookEntry> bookMap;
    long priorElapsedMs = 0;
    bool resumedFromCheckpoint = false;
    if (!cfg.noResume && !cfg.outJson.empty() &&
        std::filesystem::exists(cfg.outJson)) {
        if (loadJsonFile(cfg.outJson, bookMap, &cfg, &priorElapsedMs) &&
            !bookMap.empty()) {
            resumedFromCheckpoint = true;
        }
    }

    long totalStartMs = steadyClockMs();
    std::cout << "====================================================================\n"
              << "OCTI Offline Opening Playbook Generator (octi_book_gen)\n"
              << "  Mode           : " << cfg.mode << "\n"
              << "  Max Ply        : " << cfg.maxPly << "\n"
              << "  Search Depth   : " << cfg.searchDepth << "\n"
              << "  Time Budget    : "
              << (cfg.timeBudgetMs > 0
                      ? (std::to_string(cfg.timeBudgetMs) + " ms/node")
                      : std::string("unlimited (depth-bound)")) << "\n"
              << "  Bot Branch     : " << cfg.botBranch << "\n"
              << "  Opp Branch     : " << cfg.oppBranch << "\n"
              << "  CPU Threads    : " << cfg.threads << "\n"
              << "  Resume State   : "
              << (resumedFromCheckpoint
                      ? ("resumed " + std::to_string(bookMap.size()) +
                         " entries (" + std::to_string(priorElapsedMs) +
                         " ms prior) from " + cfg.outJson)
                      : std::string("fresh start")) << "\n"
              << "  Out Header     : " << cfg.outHeader << "\n"
              << "  Out JSON       : " << cfg.outJson << "\n"
              << "  Out Java       : " << cfg.outJava << "\n"
              << "====================================================================\n"
              << std::flush;

    std::vector<FrontierNode> frontier = buildInitialSeeds(cfg);
    if (frontier.empty()) {
        std::cerr << "Error: No initial board positions seeded for mode '"
                  << cfg.mode << "'\n";
        return 1;
    }

    std::set<std::uint64_t> visitedKeys;

    for (int ply = 0; ply < cfg.maxPly; ++ply) {
        std::vector<FrontierNode> currentFrontier;
        currentFrontier.reserve(frontier.size() + bookMap.size());

        // Seed any already-checkpointed entries at this ply first so resume
        // preserves the full saved frontier even if parent entries stored fewer
        // candidates than oppBranch.
        for (const auto &kv : bookMap) {
            const GeneratedBookEntry &e = kv.second;
            if (e.ply == ply && !e.positionStr.empty() &&
                variantMatchesConfig(e.variantLabel, cfg)) {
                if (visitedKeys.find(e.canonicalKey) == visitedKeys.end()) {
                    visitedKeys.insert(e.canonicalKey);
                    FrontierNode node;
                    node.canonicalKey = e.canonicalKey;
                    node.ply = e.ply;
                    node.variantLabel = e.variantLabel;
                    node.positionStr = e.positionStr;
                    node.moveHistoryDesc = e.moveHistoryDesc;
                    currentFrontier.push_back(node);
                }
            }
        }

        for (const auto &node : frontier) {
            if (!cfg.reSearchOnly &&
                visitedKeys.find(node.canonicalKey) == visitedKeys.end()) {
                visitedKeys.insert(node.canonicalKey);
                currentFrontier.push_back(node);
            }
        }

        if (currentFrontier.empty()) {
            std::cout << "[Ply " << ply << "/" << cfg.maxPly
                      << "] Frontier empty; stopping expansion.\n";
            break;
        }

        bool isNonLeafPly = (ply + 1 < cfg.maxPly);
        bool expandChildren = isNonLeafPly && !cfg.reSearchOnly;
        std::size_t storeCount = static_cast<std::size_t>(std::min(3, cfg.botBranch));
        std::size_t expandCount = isNonLeafPly
            ? static_cast<std::size_t>(std::max(cfg.botBranch, cfg.oppBranch))
            : storeCount;
        std::size_t searchTopK = std::max(storeCount, expandCount);

        std::vector<FrontierNode> cachedNodes;
        std::vector<FrontierNode> pendingNodes;
        cachedNodes.reserve(currentFrontier.size());
        pendingNodes.reserve(currentFrontier.size());

        for (const auto &node : currentFrontier) {
            auto it = bookMap.find(node.canonicalKey);
            bool forceReSearch = !cfg.reSearchKeys.empty()
                ? (cfg.reSearchKeys.find(node.canonicalKey) != cfg.reSearchKeys.end())
                : cfg.reSearchOnly;
            if (!forceReSearch && it != bookMap.end() &&
                !it->second.candidates.empty()) {
                cachedNodes.push_back(node);
            } else {
                pendingNodes.push_back(node);
            }
        }

        std::vector<FrontierNode> nextFrontierRaw;
        if (expandChildren) {
            for (const auto &cnode : cachedNodes) {
                auto it = bookMap.find(cnode.canonicalKey);
                if (it != bookMap.end()) {
                    std::vector<FrontierNode> ch =
                        expandEntryChildren(cnode, it->second, expandCount);
                    for (auto &c : ch) {
                        nextFrontierRaw.push_back(std::move(c));
                    }
                }
            }
        }

        if (pendingNodes.empty()) {
            std::cout << "[Ply " << ply << "/" << cfg.maxPly << "] Resumed "
                      << cachedNodes.size() << "/" << currentFrontier.size()
                      << " cached position(s); 0 remaining to search.\n"
                      << std::flush;
            frontier.swap(nextFrontierRaw);
            continue;
        }

        // Schedule heaviest variants (OFK, 2P 1-Base, 2P 3-Base) before fast 4P
        // positions (Longest-Processing-Time-First) to prevent end-of-ply
        // single-thread stragglers from starving the other worker threads.
        auto variantCostRank = [](const std::string &v) -> int {
            if (v == "OFK") return 0;
            if (v.find("2P_1B") != std::string::npos || v == "2P 1-Base") return 1;
            if (v.find("2P_3B") != std::string::npos || v == "2P 3-Base") return 2;
            return 3;
        };
        if (!cfg.reSearchKeyOrder.empty()) {
            std::map<std::uint64_t, std::size_t> keyOrderIdx;
            for (std::size_t i = 0; i < cfg.reSearchKeyOrder.size(); ++i) {
                keyOrderIdx[cfg.reSearchKeyOrder[i]] = i;
            }
            std::stable_sort(pendingNodes.begin(), pendingNodes.end(),
                             [&](const FrontierNode &a, const FrontierNode &b) {
                                 auto itA = keyOrderIdx.find(a.canonicalKey);
                                 auto itB = keyOrderIdx.find(b.canonicalKey);
                                 std::size_t idxA = (itA != keyOrderIdx.end()) ? itA->second : cfg.reSearchKeyOrder.size();
                                 std::size_t idxB = (itB != keyOrderIdx.end()) ? itB->second : cfg.reSearchKeyOrder.size();
                                 return idxA < idxB;
                             });
        } else {
            std::stable_sort(pendingNodes.begin(), pendingNodes.end(),
                             [&](const FrontierNode &a, const FrontierNode &b) {
                                 return variantCostRank(a.variantLabel) <
                                        variantCostRank(b.variantLabel);
                             });
        }

        long plyStartMs = steadyClockMs();
        long lastCheckpointMs = plyStartMs;
        std::size_t numNodes = pendingNodes.size();
        int numWorkers = 1;
        int smpPerWorker = 1;
        int extraSmpWorkers = 0;
        if (cfg.threadsPerWorker > 0) {
            int maxWorkers = std::max(1, cfg.threads / cfg.threadsPerWorker);
            numWorkers = std::min<int>(maxWorkers, static_cast<int>(numNodes));
            smpPerWorker = cfg.threadsPerWorker;
            extraSmpWorkers = 0;
        } else {
            numWorkers = std::min<int>(cfg.threads, static_cast<int>(numNodes));
            smpPerWorker = std::max(1, cfg.threads / numWorkers);
            extraSmpWorkers = (numWorkers > 0) ? (cfg.threads % numWorkers) : 0;
        }

        if (!cachedNodes.empty()) {
            std::cout << "[Ply " << ply << "/" << cfg.maxPly << "] Resumed "
                      << cachedNodes.size() << "/" << currentFrontier.size()
                      << " cached position(s); evaluating " << numNodes
                      << " remaining across " << numWorkers << " worker(s) x "
                      << smpPerWorker << " Lazy SMP thread(s) (depth="
                      << cfg.searchDepth << ", topK=" << searchTopK << ")...\n"
                      << std::flush;
        } else {
            std::cout << "[Ply " << ply << "/" << cfg.maxPly << "] Evaluating "
                      << numNodes << " unique canonical position(s) across "
                      << numWorkers << " worker(s) x " << smpPerWorker
                      << " Lazy SMP thread(s) (depth=" << cfg.searchDepth
                      << ", topK=" << searchTopK << ")...\n" << std::flush;
        }

        std::atomic<std::size_t> nextIdx{0};
        std::atomic<std::size_t> completedNodes{0};
        std::mutex resultMutex;
        std::mutex ioMutex;

        auto workerFn = [&](int workerId) {
            const int workerSmpThreads =
                smpPerWorker + (workerId < extraSmpWorkers ? 1 : 0);

            while (true) {
                std::size_t idx = nextIdx.fetch_add(1);
                if (idx >= numNodes) {
                    break;
                }

                const FrontierNode &node = pendingNodes[idx];
                octi::OctiBoard board;
                board.suppressBoardUpdateNotification(true);
                board.fromPositionString(node.positionStr);
                octi::OctiGame *game = board.getGame();
                if (!game || game->isGameOver()) {
                    continue;
                }

                octi::OctiBotEngine bot(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
                octi::BotTuningProfile prof = bot.tuningProfile();
                prof.useOpeningBook = false;
                prof.rootScoreJitter = 0;
                prof.bookSelectionWindow = 0;
                prof.lazySmpThreads = workerSmpThreads;
                prof.maxSearchDepth = cfg.searchDepth;
                prof.timeBudgetMs = cfg.timeBudgetMs;
                bot.setTuningProfile(prof);

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
                        entry.candidates.push_back(
                            {canonMove, clampScore16(topMoves[i].score)});
                    }
                }

                std::size_t lim = std::min(expandCount, topMoves.size());
                for (std::size_t i = 0; i < lim; ++i) {
                    if (!topMoves[i].move.empty()) {
                        entry.expandedMoves.push_back(topMoves[i].move);
                    }
                }

                std::vector<FrontierNode> localChildren;
                if (expandChildren) {
                    localChildren = expandEntryChildren(node, entry, expandCount);
                }

                std::size_t doneCount = completedNodes.fetch_add(1) + 1;
                std::string bestCandMove =
                    entry.candidates.empty() ? "-" : entry.candidates[0].move;
                std::int16_t bestCandScore =
                    entry.candidates.empty() ? 0 : clampScore16(entry.candidates[0].score);
                int entryDepth = entry.depth;
                bool didIntraCheckpoint = false;
                std::size_t currentBookSize = 0;
                {
                    std::lock_guard<std::mutex> lock(resultMutex);
                    if (!entry.candidates.empty()) {
                        bookMap[entry.canonicalKey] = std::move(entry);
                    }
                    for (auto &ch : localChildren) {
                        nextFrontierRaw.push_back(std::move(ch));
                    }
                    long nowMs = steadyClockMs();
                    if (doneCount < numNodes &&
                        cfg.checkpointInterval > 0 &&
                        (doneCount % static_cast<std::size_t>(cfg.checkpointInterval) == 0 ||
                         nowMs - lastCheckpointMs >= 60000)) {
                        lastCheckpointMs = nowMs;
                        long ckptElapsedMs =
                            priorElapsedMs + (nowMs - totalStartMs);
                        writeJsonFile(cfg.outJson, cfg, bookMap, ckptElapsedMs);
                        writeHeaderFile(cfg.outHeader, bookMap);
                        writeJavaFile(cfg.outJava, bookMap);
                        didIntraCheckpoint = true;
                        currentBookSize = bookMap.size();
                    }
                }
                if (doneCount == numNodes || doneCount % 5 == 0 ||
                    numNodes <= 10 || didIntraCheckpoint) {
                    std::ostringstream logOss;
                    logOss << "  [" << doneCount << "/" << numNodes << "] "
                           << node.variantLabel << " ply " << ply
                           << " (" << node.moveHistoryDesc << ") -> "
                           << bestCandMove
                           << " (score=" << bestCandScore
                           << ", d=" << entryDepth
                           << ", nodes=" << stats.nodesVisited
                           << ", " << stats.elapsedMs << "ms"
                           << (didIntraCheckpoint
                                   ? (", checkpointed " +
                                      std::to_string(currentBookSize) + " entries")
                                   : std::string(""))
                           << ")\n";
                    std::lock_guard<std::mutex> ioLock(ioMutex);
                    std::cout << logOss.str() << std::flush;
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

        long plyElapsedMs = steadyClockMs() - plyStartMs;
        long checkpointElapsedMs =
            priorElapsedMs + (steadyClockMs() - totalStartMs);
        if (!writeJsonFile(cfg.outJson, cfg, bookMap, checkpointElapsedMs) ||
            !writeHeaderFile(cfg.outHeader, bookMap) ||
            !writeJavaFile(cfg.outJava, bookMap)) {
            return 1;
        }

        std::cout << "[Ply " << ply << "/" << cfg.maxPly << "] Completed in "
                  << plyElapsedMs << " ms (cumulative book entries: "
                  << bookMap.size() << ", checkpointed)\n" << std::flush;

        frontier.swap(nextFrontierRaw);
    }

    long totalElapsedMs = priorElapsedMs + (steadyClockMs() - totalStartMs);

    if (!writeHeaderFile(cfg.outHeader, bookMap)) {
        return 1;
    }
    if (!writeJsonFile(cfg.outJson, cfg, bookMap, totalElapsedMs)) {
        return 1;
    }
    if (!writeJavaFile(cfg.outJava, bookMap)) {
        return 1;
    }
    for (const auto &jsPath : cfg.outJs) {
        if (!updateJsFileWithMarkers(jsPath, bookMap)) {
            return 1;
        }
    }

    std::cout << "====================================================================\n"
              << "Playbook Generation Complete!\n"
              << "  Total Unique Canonical Entries : " << bookMap.size() << "\n"
              << "  Total Elapsed Time             : " << totalElapsedMs << " ms\n"
              << "  C++ Header Written             : " << cfg.outHeader << "\n"
              << "  JSON Artifact Written          : " << cfg.outJson << "\n";
    if (!cfg.outJava.empty()) {
        std::cout << "  Java Class Written             : " << cfg.outJava << "\n";
    }
    for (const auto &jsPath : cfg.outJs) {
        std::cout << "  JS/Web File Updated            : " << jsPath << "\n";
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
