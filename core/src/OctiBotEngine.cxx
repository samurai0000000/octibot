/*
 * OctiBotEngine.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiBotEngine.hxx"
#include "OctiOpeningBookData.hxx"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>
#include <vector>

namespace octi {

namespace {

constexpr int WIN_SCORE = 1000000;
constexpr int INF_SCORE = 2000000;
constexpr std::size_t TT_SIZE = 262144; // 256K entries (1 << 18)
constexpr std::size_t TT_MUTEX_COUNT = 1024;

long steadyClockMs()
{
    using namespace std::chrono;
    return static_cast<long>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// ---------------------------------------------------------------------------
// 64-bit Zobrist Binary Hashing Tables
// ---------------------------------------------------------------------------

struct ZobristTables {
    std::uint64_t piece[81][7][4];
    std::uint64_t prong[81][7][8][3];
    std::uint64_t baseCaptured[81][5];
    std::uint64_t freePods[5][8];
    std::uint64_t capturedPods[5][8];
    std::uint64_t freeProngs[5][33];
    std::uint64_t freeSuperProngs[5][9];
    std::uint64_t turn[5];
    std::uint64_t mode[4];
    std::uint64_t basesToWin[5];
    std::uint64_t edgeless[2];
    std::uint64_t superProngs[2];

    ZobristTables()
    {
        std::uint64_t state = 0x9E3779B97F4A7C15ULL;
        auto splitmix64 = [&state]() -> std::uint64_t {
            std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        };

        for (int sq = 0; sq < 81; ++sq) {
            for (int p = 0; p < 7; ++p) {
                for (int t = 0; t < 4; ++t) {
                    piece[sq][p][t] = splitmix64();
                }
                for (int d = 0; d < 8; ++d) {
                    for (int pt = 0; pt < 3; ++pt) {
                        prong[sq][p][d][pt] = splitmix64();
                    }
                }
            }
            for (int c = 0; c < 5; ++c) {
                baseCaptured[sq][c] = splitmix64();
            }
        }
        for (int pl = 0; pl < 5; ++pl) {
            for (int i = 0; i < 8; ++i) {
                freePods[pl][i] = splitmix64();
                capturedPods[pl][i] = splitmix64();
            }
            for (int i = 0; i < 33; ++i) {
                freeProngs[pl][i] = splitmix64();
            }
            for (int i = 0; i < 9; ++i) {
                freeSuperProngs[pl][i] = splitmix64();
            }
            turn[pl] = splitmix64();
        }
        for (int m = 0; m < 4; ++m) {
            mode[m] = splitmix64();
        }
        for (int b = 0; b < 5; ++b) {
            basesToWin[b] = splitmix64();
        }
        for (int e = 0; e < 2; ++e) {
            edgeless[e] = splitmix64();
        }
        for (int s = 0; s < 2; ++s) {
            superProngs[s] = splitmix64();
        }
    }
};

static const ZobristTables g_zobrist;

struct alignas(64) AlignedTtMutex {
    std::mutex mtx;
    void lock() { mtx.lock(); }
    void unlock() { mtx.unlock(); }
    bool try_lock() { return mtx.try_lock(); }
};

static std::array<AlignedTtMutex, TT_MUTEX_COUNT> g_ttMutexes;

// ---------------------------------------------------------------------------
// Board Square & Target Queries
// ---------------------------------------------------------------------------

bool isEnemyHomeSquareFor(OctiSquareType sqType, OctiGameMode mode, OctiPlayer p)
{
    if (mode == GAME_MODE_2 || mode == GAME_MODE_OFK) {
        if (p == PLAYER_A) {
            return sqType == B_HOME_SQUARE;
        }
        if (p == PLAYER_B) {
            return sqType == A_HOME_SQUARE;
        }
        return false;
    }
    if (mode == GAME_MODE_4) {
        if (p == PLAYER_A || p == PLAYER_C) {
            return sqType == B_HOME_SQUARE || sqType == D_HOME_SQUARE;
        }
        if (p == PLAYER_B || p == PLAYER_D) {
            return sqType == A_HOME_SQUARE || sqType == C_HOME_SQUARE;
        }
    }
    return false;
}

bool isAlliedHomeSquareFor(OctiSquareType sqType, OctiGameMode mode, OctiPlayer p)
{
    if (mode == GAME_MODE_2 || mode == GAME_MODE_OFK) {
        if (p == PLAYER_A) {
            return sqType == A_HOME_SQUARE;
        }
        if (p == PLAYER_B) {
            return sqType == B_HOME_SQUARE;
        }
        return false;
    }
    if (mode == GAME_MODE_4) {
        if (p == PLAYER_A || p == PLAYER_C) {
            return sqType == A_HOME_SQUARE || sqType == C_HOME_SQUARE;
        }
        if (p == PLAYER_B || p == PLAYER_D) {
            return sqType == B_HOME_SQUARE || sqType == D_HOME_SQUARE;
        }
    }
    return false;
}

std::vector<std::pair<int, int>> targetEnemyBases(const OctiBoard &board,
                                                  OctiPlayer p)
{
    std::vector<std::pair<int, int>> uncaptured;
    std::vector<std::pair<int, int>> allEnemy;
    OctiGameMode mode = board.getGameMode();
    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const OctiSquare &sq = board.squareAt(x, y);
            if (isEnemyHomeSquareFor(sq.getType(), mode, p)) {
                allEnemy.push_back({x, y});
                if (!sq.isCapturedByTeamOf(p)) {
                    uncaptured.push_back({x, y});
                }
            }
        }
    }
    return !uncaptured.empty() ? uncaptured : allEnemy;
}

bool isOrthogonalDir(OctiDir dir)
{
    return dir == DIR_N || dir == DIR_E || dir == DIR_S || dir == DIR_W;
}

bool isDiagonalDir(OctiDir dir)
{
    return dir == DIR_NE || dir == DIR_SE || dir == DIR_SW || dir == DIR_NW;
}

bool hasUnarmedPodOnHomeSquare(const OctiBoard &board, OctiPlayer turn)
{
    OctiGameMode mode = board.getGameMode();
    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    OctiPodType podType = playerToPodType(turn);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const OctiSquare &sq = board.squareAt(x, y);
            if (!isAlliedHomeSquareFor(sq.getType(), mode, turn)) {
                continue;
            }
            for (int i = 0; i < sq.getNumPods(); ++i) {
                const OctiPod &pod = sq.getPodAt(i);
                if (pod.getType() == podType && pod.getNumProngs() < 2) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool squareHasAllyOf(const OctiSquare &sq, OctiGameMode mode, OctiPlayer player)
{
    if (sq.isEmpty()) {
        return false;
    }
    return areAlliedPlayers(mode, podTypeToPlayer(sq.getPodAt(0).getType()), player);
}

int torusDist(int a, int b, int dim, bool edgeless)
{
    int d = std::abs(a - b);
    if (edgeless && dim > 0) {
        d = std::min(d, dim - d);
    }
    return d;
}

int chebyshevDist(int x1, int y1, int x2, int y2, int w, int h, bool edgeless)
{
    int dx = torusDist(x1, x2, w, edgeless);
    int dy = torusDist(y1, y2, h, edgeless);
    return std::max(dx, dy);
}

int minChebyshevToTargets(int x, int y,
                          const std::vector<std::pair<int, int>> &targets,
                          int w, int h, bool edgeless)
{
    if (targets.empty()) {
        return 0;
    }
    int best = 999;
    for (const auto &t : targets) {
        int dist = chebyshevDist(x, y, t.first, t.second, w, h, edgeless);
        if (dist < best) {
            best = dist;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// 2001 Jump-Capture Reachability & Threat Evaluation
// ---------------------------------------------------------------------------

struct JumpMobilityThreat {
    int reachableSquares = 0;
    int capturablePods = 0;
    int capturableProngs = 0;
    int minJumpDistToTargets = 999;
    std::uint64_t capturedSqMask[2] = {0ULL, 0ULL};
};

void evaluateJumpCaptureRecursive(const OctiBoard &board,
                                  bool edgeless,
                                  int w,
                                  int h,
                                  int x,
                                  int y,
                                  int startX,
                                  int startY,
                                  int depth,
                                  OctiPlayer podOwner,
                                  const OctiPod &pod,
                                  const std::vector<std::pair<int, int>> &targets,
                                  std::uint64_t visitedDestMask[2],
                                  const std::uint64_t jumpedMask[2],
                                  JumpMobilityThreat &threat)
{
    if (depth > 4) {
        return;
    }

    for (int d = 0; d < DIR_MAX; ++d) {
        OctiDir dir = static_cast<OctiDir>(d);
        if (pod.prongAt(dir) == NO_PRONG) {
            continue;
        }

        int dx = dirDeltaX(dir);
        int dy = dirDeltaY(dir);

        int jx = edgeless ? (((x + dx) % w + w) % w) : (x + dx);
        int jy = edgeless ? (((y + dy) % h + h) % h) : (y + dy);
        int destX = edgeless ? (((x + 2 * dx) % w + w) % w) : (x + 2 * dx);
        int destY = edgeless ? (((y + 2 * dy) % h + h) % h) : (y + 2 * dy);

        if (!board.isValidCoord(jx, jy) || !board.isValidCoord(destX, destY)) {
            continue;
        }

        int jumpIdx = jy * 9 + jx;
        int jumpWord = (jumpIdx >> 6) & 1;
        std::uint64_t jumpBit = 1ULL << (jumpIdx & 63);
        if ((jumpedMask[jumpWord] & jumpBit) != 0ULL) {
            continue;
        }

        const OctiSquare &midSq = board.squareAt(jx, jy);
        const OctiSquare &destSq = board.squareAt(destX, destY);

        bool canLand = destSq.isEmpty() || (destX == startX && destY == startY);
        if (midSq.isEmpty() || !canLand) {
            continue;
        }

        int destIdx = destY * 9 + destX;
        int destWord = (destIdx >> 6) & 1;
        std::uint64_t destBit = 1ULL << (destIdx & 63);
        if ((visitedDestMask[destWord] & destBit) != 0ULL) {
            continue;
        }
        visitedDestMask[destWord] |= destBit;
        threat.reachableSquares++;
        threat.minJumpDistToTargets = std::min(threat.minJumpDistToTargets,
                                               minChebyshevToTargets(destX, destY, targets, w, h, edgeless));

        if (midSq.hasEnemyOf(podOwner)) {
            int midIdx = jy * 9 + jx;
            int midWord = (midIdx >> 6) & 1;
            std::uint64_t midBit = 1ULL << (midIdx & 63);
            if ((threat.capturedSqMask[midWord] & midBit) == 0ULL) {
                threat.capturedSqMask[midWord] |= midBit;
                threat.capturablePods += midSq.getNumPods();
                for (int pi = 0; pi < midSq.getNumPods(); ++pi) {
                    const OctiPod &p = midSq.getPodAt(pi);
                    threat.capturableProngs += p.getNumProngs();
                }
            }
        }

        std::uint64_t nextJumpedMask[2] = {jumpedMask[0], jumpedMask[1]};
        nextJumpedMask[jumpWord] |= jumpBit;

        evaluateJumpCaptureRecursive(board, edgeless, w, h, destX, destY, startX, startY, depth + 1,
                                     podOwner, pod, targets, visitedDestMask, nextJumpedMask, threat);
    }
}

JumpMobilityThreat evaluatePodJumpThreat(const OctiBoard &board,
                                        int x,
                                        int y,
                                        const OctiPod &pod,
                                        const std::vector<std::pair<int, int>> &targets)
{
    JumpMobilityThreat threat;
    const OctiGame *g = board.getGame();
    if (!g) {
        return threat;
    }
    bool edgeless = g->useEdgelessBoard();
    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    OctiPlayer owner = podTypeToPlayer(pod.getType());

    std::uint64_t visitedDestMask[2] = {0ULL, 0ULL};
    std::uint64_t jumpedMask[2] = {0ULL, 0ULL};

    evaluateJumpCaptureRecursive(board, edgeless, w, h, x, y, x, y, 0,
                                 owner, pod, targets, visitedDestMask, jumpedMask, threat);
    return threat;
}

// ---------------------------------------------------------------------------
// Dominated Move Filtering
// ---------------------------------------------------------------------------

std::vector<OctiMove> filterDominatedMoves(const std::vector<OctiMove> &moves)
{
    if (moves.size() <= 1) {
        return moves;
    }

    using HopCoordVec = std::vector<std::pair<int, int>>;
    using PathKey = std::tuple<int, int, std::uint32_t, HopCoordVec>;
    std::set<PathKey> hasCapturingVariant;

    for (const auto &m : moves) {
        const auto &ops = m.getOps();
        if (ops.size() != 1 || ops[0].hops.empty()) {
            continue;
        }
        const OctiMoveOp &op = ops[0];
        bool anyCap = false;
        HopCoordVec coords;
        coords.reserve(op.hops.size());
        for (const auto &h : op.hops) {
            coords.push_back({h.x, h.y});
            if (h.capture) {
                anyCap = true;
            }
        }
        if (anyCap) {
            hasCapturingVariant.insert(
                PathKey(op.x0, op.y0, op.pod.rawConfig() & 0xFFFFu, coords));
        }
    }

    if (hasCapturingVariant.empty()) {
        return moves;
    }

    std::vector<OctiMove> filtered;
    filtered.reserve(moves.size());
    for (const auto &m : moves) {
        const auto &ops = m.getOps();
        if (ops.size() == 1 && !ops[0].hops.empty()) {
            const OctiMoveOp &op = ops[0];
            bool anyCap = false;
            HopCoordVec coords;
            coords.reserve(op.hops.size());
            for (const auto &h : op.hops) {
                coords.push_back({h.x, h.y});
                if (h.capture) {
                    anyCap = true;
                }
            }
            if (!anyCap) {
                PathKey key(op.x0, op.y0, op.pod.rawConfig() & 0xFFFFu, coords);
                if (hasCapturingVariant.count(key) > 0) {
                    continue;
                }
            }
        }
        filtered.push_back(m);
    }

    return filtered.empty() ? moves : filtered;
}

// ---------------------------------------------------------------------------
// Composite Multi-Pod Move Generation
// ---------------------------------------------------------------------------

void appendCompositeMoves(OctiBoard &scratchBoard,
                          std::vector<OctiMove> &moves,
                          OctiPlayer turn)
{
    OctiGame *game = scratchBoard.getGame();
    if (!game || game->getMode() == GAME_MODE_OFK || moves.empty()) {
        return;
    }

    int w = scratchBoard.getBoardWidth();
    int h = scratchBoard.getBoardHeight();
    OctiPodType friendlyPodType = playerToPodType(turn);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const OctiSquare &sq = scratchBoard.squareAt(x, y);
            if (sq.getNumPods() < 2) {
                continue;
            }

            std::vector<OctiPod> friendlyPods;
            for (int i = 0; i < sq.getNumPods(); ++i) {
                const OctiPod &p = sq.getPodAt(i);
                if (p.getType() == friendlyPodType && p.getNumProngs() > 0) {
                    friendlyPods.push_back(p);
                }
            }

            if (friendlyPods.size() < 2) {
                continue;
            }

            // Group single-pod movement operations from (x, y) by matching pod config
            std::vector<std::vector<OctiMoveOp>> podMoves(friendlyPods.size());
            for (std::size_t k = 0; k < friendlyPods.size(); ++k) {
                std::uint32_t targetCfg = friendlyPods[k].rawConfig() & 0xFFFFu;
                for (const auto &m : moves) {
                    const auto &ops = m.getOps();
                    if (ops.size() == 1 && ops[0].x0 == x && ops[0].y0 == y &&
                        !ops[0].hops.empty()) {
                        if ((ops[0].pod.rawConfig() & 0xFFFFu) == targetCfg) {
                            podMoves[k].push_back(ops[0]);
                        }
                    }
                }
            }

            int validPodsCount = 0;
            for (const auto &pm : podMoves) {
                if (!pm.empty()) {
                    ++validPodsCount;
                }
            }
            if (validPodsCount < 2) {
                continue;
            }

            std::size_t compositeAddedForSquare = 0;
            constexpr std::size_t MAX_COMPOSITE_PER_SQUARE = 24;

            // Generate 2-pod composite moves
            for (std::size_t i = 0; i < friendlyPods.size() && compositeAddedForSquare < MAX_COMPOSITE_PER_SQUARE; ++i) {
                if (podMoves[i].empty()) continue;
                for (std::size_t j = i + 1; j < friendlyPods.size() && compositeAddedForSquare < MAX_COMPOSITE_PER_SQUARE; ++j) {
                    if (podMoves[j].empty()) continue;

                    for (const auto &op1 : podMoves[i]) {
                        if (compositeAddedForSquare >= MAX_COMPOSITE_PER_SQUARE) break;
                        for (const auto &op2 : podMoves[j]) {
                            if (compositeAddedForSquare >= MAX_COMPOSITE_PER_SQUARE) break;

                            // Duplicate capture check across pods
                            bool dupCapture = false;
                            for (const auto &h1 : op1.hops) {
                                if (h1.capture) {
                                    for (const auto &h2 : op2.hops) {
                                        if (h2.capture && h1.x == h2.x && h1.y == h2.y) {
                                            dupCapture = true;
                                            break;
                                        }
                                    }
                                }
                                if (dupCapture) break;
                            }
                            if (dupCapture) {
                                continue;
                            }

                            OctiGameSnapshot snap = game->captureSnapshot();
                            try {
                                OctiMove cand(std::vector<OctiMoveOp>{op1, op2});
                                game->move(cand);
                                moves.push_back(cand);
                                ++compositeAddedForSquare;
                            } catch (...) {
                            }
                            game->restoreSnapshot(snap);
                        }
                    }
                }
            }

            // If 3+ pods with moves, generate 3-pod composite moves
            if (friendlyPods.size() >= 3 && validPodsCount >= 3 &&
                compositeAddedForSquare < MAX_COMPOSITE_PER_SQUARE) {
                for (std::size_t i = 0; i < friendlyPods.size() && compositeAddedForSquare < MAX_COMPOSITE_PER_SQUARE; ++i) {
                    if (podMoves[i].empty()) continue;
                    for (std::size_t j = i + 1; j < friendlyPods.size() && compositeAddedForSquare < MAX_COMPOSITE_PER_SQUARE; ++j) {
                        if (podMoves[j].empty()) continue;
                        for (std::size_t k = j + 1; k < friendlyPods.size() && compositeAddedForSquare < MAX_COMPOSITE_PER_SQUARE; ++k) {
                            if (podMoves[k].empty()) continue;

                            for (const auto &op1 : podMoves[i]) {
                                if (compositeAddedForSquare >= MAX_COMPOSITE_PER_SQUARE) break;
                                for (const auto &op2 : podMoves[j]) {
                                    if (compositeAddedForSquare >= MAX_COMPOSITE_PER_SQUARE) break;
                                    for (const auto &op3 : podMoves[k]) {
                                        if (compositeAddedForSquare >= MAX_COMPOSITE_PER_SQUARE) break;

                                        // Capture conflict check
                                        bool dupCap = false;
                                        std::vector<std::pair<int, int>> capCoords;
                                        for (const auto *op : {&op1, &op2, &op3}) {
                                            for (const auto &hp : op->hops) {
                                                if (hp.capture) {
                                                    std::pair<int, int> c{hp.x, hp.y};
                                                    if (std::find(capCoords.begin(), capCoords.end(), c) != capCoords.end()) {
                                                        dupCap = true;
                                                        break;
                                                    }
                                                    capCoords.push_back(c);
                                                }
                                            }
                                            if (dupCap) break;
                                        }
                                        if (dupCap) continue;

                                        OctiGameSnapshot snap = game->captureSnapshot();
                                        try {
                                            OctiMove cand(std::vector<OctiMoveOp>{op1, op2, op3});
                                            game->move(cand);
                                            moves.push_back(cand);
                                            ++compositeAddedForSquare;
                                        } catch (...) {
                                        }
                                        game->restoreSnapshot(snap);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Move Ordering & Heuristic Scoring
// ---------------------------------------------------------------------------

struct CoalitionPhaseContext {
    int basesNeeded = 1;
    int availPods = 0;
    int capturedPods = 0;
    bool feasible = true;
    int baseThreatScalePct = 100;
};

static CoalitionPhaseContext analyzeCoalitionPhase(const OctiBoard &board, OctiPlayer side)
{
    CoalitionPhaseContext ctx;
    const OctiGame *game = board.getGame();
    if (!game || side == PLAYER_UNDEF) {
        return ctx;
    }
    OctiGameMode mode = board.getGameMode();
    int basesToWin = game->getBasesToWin();
    int w = board.getBoardWidth();
    int h = board.getBoardHeight();

    if (mode == GAME_MODE_OFK || basesToWin <= 1) {
        ctx.basesNeeded = 1;
    } else {
        int capturedEnemyBases = 0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const OctiSquare &sq = board.squareAt(x, y);
                if (isEnemyHomeSquareFor(sq.getType(), mode, side) &&
                    sq.isCapturedByTeamOf(side)) {
                    ++capturedEnemyBases;
                }
            }
        }
        ctx.basesNeeded = std::max(1, basesToWin - capturedEnemyBases);
    }

    int totalBoardPods = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const OctiSquare &sq = board.squareAt(x, y);
            if (squareHasAllyOf(sq, mode, side)) {
                totalBoardPods += sq.getNumPods();
            }
        }
    }

    int allyFreePods = 0;
    int allyCapturedPods = 0;
    bool allyCanLiberate = false;
    int numPlayers = (mode == GAME_MODE_4) ? 4 : 2;
    for (int pIdx = 1; pIdx <= numPlayers; ++pIdx) {
        OctiPlayer p = static_cast<OctiPlayer>(pIdx);
        if (areAlliedPlayers(mode, p, side)) {
            allyFreePods += game->getFreePods(p);
            allyCapturedPods += game->getCapturedPods(p);
            if (board.canLiberate(p)) {
                allyCanLiberate = true;
            }
        }
    }

    ctx.capturedPods = allyCapturedPods;
    ctx.availPods = totalBoardPods + allyFreePods + (allyCanLiberate ? allyCapturedPods : 0);
    ctx.feasible = (ctx.availPods >= ctx.basesNeeded);

    if (!ctx.feasible) {
        ctx.baseThreatScalePct = (ctx.capturedPods > 0) ? 20 : 5;
    } else if (ctx.basesNeeded <= 1) {
        ctx.baseThreatScalePct = 100;
    } else if (ctx.basesNeeded == 2) {
        ctx.baseThreatScalePct = 35;
    } else {
        ctx.baseThreatScalePct = 20;
    }

    return ctx;
}

int scoreMoveHeuristic(const OctiBoard &board,
                       const OctiMove &move,
                       OctiPlayer turn,
                       const std::vector<std::pair<int, int>> &enemyTargets,
                       const BotTuningProfile &profile,
                       const std::uint64_t enemyThreatMask[2],
                       int baseThreatScalePct = 100)
{
    const OctiGame *game = board.getGame();
    OctiGameMode mode = board.getGameMode();
    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    bool edgeless = game ? game->useEdgelessBoard() : false;
    bool earlyDevelopment = game && game->getFreeProngs(turn) >= 8;

    int score = 0;

    // Prioritize composite moves
    if (move.getOps().size() > 1) {
        score += 8000 + static_cast<int>(move.getOps().size() - 1) * 4000;
    }

    for (const auto &op : move.getOps()) {
        if (!op.hops.empty()) {
            int startDist = minChebyshevToTargets(op.x0, op.y0, enemyTargets, w, h, edgeless);
            const OctiHop &lastHop = op.hops.back();
            int endDist = minChebyshevToTargets(lastHop.x, lastHop.y, enemyTargets, w, h, edgeless);

            score += (startDist - endDist) * 550;

            bool anyCapture = false;
            bool isJump = (op.hops.size() > 1);
            int prevX = op.x0;
            int prevY = op.y0;
            for (const auto &hop : op.hops) {
                int dx = hop.x - prevX;
                int dy = hop.y - prevY;
                if (edgeless) {
                    for (int d = -2; d <= 2; d++) {
                        if (((prevX + d) % w + w) % w == hop.x) { dx = d; break; }
                    }
                    for (int d = -2; d <= 2; d++) {
                        if (((prevY + d) % h + h) % h == hop.y) { dy = d; break; }
                    }
                }
                if (std::abs(dx) == 2 || std::abs(dy) == 2) {
                    isJump = true;
                    int mx = edgeless ? (((prevX + dx / 2) % w + w) % w) : (prevX + dx / 2);
                    int my = edgeless ? (((prevY + dy / 2) % h + h) % h) : (prevY + dy / 2);
                    if (board.isValidCoord(mx, my)) {
                        const OctiSquare &midSq = board.squareAt(mx, my);
                        if (squareHasAllyOf(midSq, mode, turn)) {
                            score += profile.moveOrderLeapfrogBonus;
                        }
                        if (hop.capture) {
                            anyCapture = true;
                            int capturedPodsOnMid = midSq.getNumPods();
                            int capturedProngsOnMid = 0;
                            for (int pi = 0; pi < capturedPodsOnMid; ++pi) {
                                capturedProngsOnMid += midSq.getPodAt(pi).getNumProngs();
                            }
                            score += profile.moveOrderCaptureBonus * capturedPodsOnMid + 600 * capturedProngsOnMid;
                        }
                    } else if (hop.capture) {
                        anyCapture = true;
                        score += profile.moveOrderCaptureBonus;
                    }
                } else if (hop.capture) {
                    anyCapture = true;
                    score += profile.moveOrderCaptureBonus;
                }
                if (!hop.capture) {
                    score += 250;
                }
                prevX = hop.x;
                prevY = hop.y;
            }

            const OctiSquare &destSq = board.squareAt(lastHop.x, lastHop.y);
            if (isEnemyHomeSquareFor(destSq.getType(), mode, turn) &&
                !destSq.isCapturedByTeamOf(turn)) {
                if (anyCapture) {
                    score += profile.moveOrderBaseEntryBonus;
                } else {
                    score += (profile.moveOrderBaseEntryBonus * baseThreatScalePct) / 100;
                }
            }
            if (destSq.getType() == NEUTRAL_OCTI_SQUARE && destSq.isEmpty()) {
                score += 3000;
            }

            // Blunder penalty if ending square sits in enemy jump threat mask
            int destBitIdx = lastHop.y * 9 + lastHop.x;
            int destWord = (destBitIdx >> 6) & 1;
            std::uint64_t destBit = 1ULL << (destBitIdx & 63);
            if (!anyCapture && enemyThreatMask && (enemyThreatMask[destWord] & destBit) != 0ULL) {
                score -= profile.moveOrderBlunderPenalty;
            }

            const OctiSquare &startSq = board.squareAt(op.x0, op.y0);
            if (isEnemyHomeSquareFor(startSq.getType(), mode, turn)) {
                if (startSq.getNumPods() <= 1 &&
                    !isEnemyHomeSquareFor(destSq.getType(), mode, turn)) {
                    score -= 22000;
                }
            }

            // Avoid stacking multiple friendly pods on non-enemy-base squares
            if (!isEnemyHomeSquareFor(destSq.getType(), mode, turn) &&
                destSq.getNumPods() >= 1) {
                score -= 1200 * destSq.getNumPods();
            }

            // Penalize premature 1-prong lone-pod rushes during development
            if (!anyCapture && !isJump && earlyDevelopment && op.pod.getNumProngs() <= 1 && endDist <= 3) {
                score -= 3200;
            }
        } else if (op.addProngDir != DIR_UNDEF && op.rmProngDir == DIR_UNDEF) {
            score += 650;
            if (op.superProng) {
                score += 200;
            }
            int nx = op.x0 + dirDeltaX(op.addProngDir);
            int ny = op.y0 + dirDeltaY(op.addProngDir);
            int curDist = minChebyshevToTargets(op.x0, op.y0, enemyTargets, w, h, edgeless);
            int nextDist = minChebyshevToTargets(nx, ny, enemyTargets, w, h, edgeless);
            if (nextDist < curDist) {
                score += 600;
            }
            if (op.pod.getNumProngs() == 0) {
                score += 900; // Strongly prioritize arming 0-prong pods first!
            } else if (op.pod.getNumProngs() < 3) {
                score += 450;
            }

            bool hasOrth = isOrthogonalDir(op.addProngDir);
            bool hasDiag = isDiagonalDir(op.addProngDir);
            for (int d = 0; d < DIR_MAX; ++d) {
                OctiDir dir = static_cast<OctiDir>(d);
                if (op.pod.prongAt(dir) != NO_PRONG) {
                    if (isOrthogonalDir(dir)) hasOrth = true;
                    if (isDiagonalDir(dir)) hasDiag = true;
                }
            }
            if (hasOrth && hasDiag) {
                score += 650;
            }

            if (turn == PLAYER_A || turn == PLAYER_C) {
                if (op.addProngDir == DIR_N) {
                    score += 1100;
                } else if (op.addProngDir == DIR_NE || op.addProngDir == DIR_NW) {
                    score += 750;
                } else if (op.addProngDir == DIR_E || op.addProngDir == DIR_W) {
                    score += 400;
                }
            } else if (turn == PLAYER_B || turn == PLAYER_D) {
                if (op.addProngDir == DIR_S) {
                    score += 1100;
                } else if (op.addProngDir == DIR_SE || op.addProngDir == DIR_SW) {
                    score += 750;
                } else if (op.addProngDir == DIR_E || op.addProngDir == DIR_W) {
                    score += 400;
                }
            }
        } else if (op.addPod) {
            if (op.liberate) {
                score += 6500;
            } else {
                const OctiSquare &sq = board.squareAt(op.x0, op.y0);
                if (sq.getNumPods() > 0 || hasUnarmedPodOnHomeSquare(board, turn)) {
                    score -= 6000;
                } else {
                    score += 150;
                }
            }
        } else if (op.rmProngDir != DIR_UNDEF) {
            score += 60;
        }
    }
    return score;
}

std::vector<OctiMove> orderAndCapMoves(OctiBoard &board,
                                       std::vector<OctiMove> &rawMoves,
                                       OctiPlayer turn,
                                       const std::string &ttBestMove,
                                       std::size_t maxBranch,
                                       const BotTuningProfile &profile,
                                       bool includeComposites = true)
{
    if (includeComposites && (!board.getGame() || board.getGame()->getMode() != GAME_MODE_OFK)) {
        appendCompositeMoves(board, rawMoves, turn);
    }
    std::vector<OctiMove> moves = filterDominatedMoves(rawMoves);
    std::vector<std::pair<int, int>> targets = targetEnemyBases(board, turn);

    // Compute enemy threat mask for blunder penalty in move ordering
    std::uint64_t enemyThreatMask[2] = {0ULL, 0ULL};
    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    OctiGameMode mode = board.getGameMode();
    bool edgeless = board.getGame() ? board.getGame()->useEdgelessBoard() : false;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const OctiSquare &sq = board.squareAt(x, y);
            if (sq.hasEnemyOf(turn)) {
                for (int i = 0; i < sq.getNumPods(); ++i) {
                    const OctiPod &p = sq.getPodAt(i);
                    if (!areAlliedPlayers(mode, podTypeToPlayer(p.getType()), turn) && p.getNumProngs() > 0) {
                        JumpMobilityThreat threat = evaluatePodJumpThreat(board, x, y, p, targets);
                        enemyThreatMask[0] |= threat.capturedSqMask[0];
                        enemyThreatMask[1] |= threat.capturedSqMask[1];

                        // Mark 1-hop jump capture lanes: if landing square is empty,
                        // any pod moving into (mx, my) will be jumped over and captured!
                        for (int d = 0; d < DIR_MAX; ++d) {
                            OctiDir dir = static_cast<OctiDir>(d);
                            if (p.prongAt(dir) != NO_PRONG) {
                                int dx = dirDeltaX(dir);
                                int dy = dirDeltaY(dir);
                                int mx = edgeless ? (((x + dx) % w + w) % w) : (x + dx);
                                int my = edgeless ? (((y + dy) % h + h) % h) : (y + dy);
                                int lx = edgeless ? (((x + 2 * dx) % w + w) % w) : (x + 2 * dx);
                                int ly = edgeless ? (((y + 2 * dy) % h + h) % h) : (y + 2 * dy);
                                if (board.isValidCoord(mx, my) && board.isValidCoord(lx, ly)) {
                                    const OctiSquare &landSq = board.squareAt(lx, ly);
                                    if (landSq.isEmpty()) {
                                        int midIdx = my * 9 + mx;
                                        int midWord = (midIdx >> 6) & 1;
                                        enemyThreatMask[midWord] |= (1ULL << (midIdx & 63));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    CoalitionPhaseContext turnPhase = analyzeCoalitionPhase(board, turn);
    int baseThreatScalePct = turnPhase.baseThreatScalePct;

    std::vector<std::pair<int, std::size_t>> scored;
    scored.reserve(moves.size());
    for (std::size_t i = 0; i < moves.size(); ++i) {
        int s = scoreMoveHeuristic(board, moves[i], turn, targets, profile, enemyThreatMask, baseThreatScalePct);
        if (!ttBestMove.empty() && moves[i].asString() == ttBestMove) {
            s += 200000;
        }
        scored.push_back({s, i});
    }

    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto &a, const auto &b) {
                         return a.first > b.first;
                     });

    std::size_t limit = std::min(moves.size(), maxBranch);
    std::vector<OctiMove> result;
    result.reserve(limit);
    for (std::size_t i = 0; i < limit; ++i) {
        result.push_back(moves[scored[i].second]);
    }
    return result;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction & Difficulty Levels
// ---------------------------------------------------------------------------

OctiBotEngine::OctiBotEngine(BotDifficulty difficulty, std::uint64_t rngSeed)
    : _difficulty(difficulty)
    , _profile(defaultProfileForDifficulty(difficulty))
    , _timeBudgetMs(_profile.timeBudgetMs)
    , _customTimeBudget(false)
    , _rngState(rngSeed ? rngSeed : 0x5EED2026ULL)
    , _tt(TT_SIZE)
{
}

OctiBotEngine::OctiBotEngine(const OctiBotEngine &other)
    : _difficulty(other._difficulty)
    , _profile(other._profile)
    , _timeBudgetMs(other._timeBudgetMs)
    , _customTimeBudget(other._customTimeBudget)
    , _rngState(other._rngState)
    , _tt(other._tt)
    , _stopSearch(false)
    , _liveDepthReached(other._liveDepthReached.load(std::memory_order_relaxed))
    , _liveNodesVisited(other._liveNodesVisited.load(std::memory_order_relaxed))
    , _liveBestScore(other._liveBestScore.load(std::memory_order_relaxed))
{
}

OctiBotEngine &OctiBotEngine::operator=(const OctiBotEngine &other)
{
    if (this != &other) {
        _difficulty = other._difficulty;
        _profile = other._profile;
        _timeBudgetMs = other._timeBudgetMs;
        _customTimeBudget = other._customTimeBudget;
        _rngState = other._rngState;
        _tt = other._tt;
        _stopSearch.store(false, std::memory_order_relaxed);
        _liveDepthReached.store(other._liveDepthReached.load(std::memory_order_relaxed), std::memory_order_relaxed);
        _liveNodesVisited.store(other._liveNodesVisited.load(std::memory_order_relaxed), std::memory_order_relaxed);
        _liveBestScore.store(other._liveBestScore.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    return *this;
}

OctiBotEngine::~OctiBotEngine() = default;

void OctiBotEngine::setDifficulty(BotDifficulty d)
{
    _difficulty = d;
    int oldBudget = _timeBudgetMs;
    bool wasCustom = _customTimeBudget;
    _profile = defaultProfileForDifficulty(d);
    if (wasCustom) {
        _timeBudgetMs = oldBudget;
        _profile.timeBudgetMs = oldBudget;
        _customTimeBudget = true;
    } else {
        _timeBudgetMs = _profile.timeBudgetMs;
    }
}

BotDifficulty OctiBotEngine::difficulty() const
{
    return _difficulty;
}

void OctiBotEngine::setTuningProfile(const BotTuningProfile &profile)
{
    _profile = profile;
    _timeBudgetMs = profile.timeBudgetMs;
}

const BotTuningProfile &OctiBotEngine::tuningProfile() const
{
    return _profile;
}

BotTuningProfile OctiBotEngine::defaultProfileForDifficulty(BotDifficulty difficulty)
{
    BotTuningProfile p;
    switch (difficulty) {
    case BotDifficulty::CASUAL:
        p.timeBudgetMs = 5000;
        p.maxSearchDepth = 3;
        p.useOpeningBook = false;
        p.maxQuiesceDepth = 0;
        p.quiesceBranchCap = 0;
        p.rootBranchCap = 24;
        p.treeBranchCap = 16;
        p.lazySmpThreads = 1;
        p.compositeMoveMaxPly = -1;
        p.rootScoreJitter = 160;
        p.closestPodDistWeight = 280;
        p.secondClosestPodDistWeight = 0;
        p.oneStepBaseThreatBonus = 7500;
        p.oneJumpBaseThreatBonus = 10000;
        p.twoPlyBaseThreatWeight = 0;
        p.firstArmedPodBonus = 500;
        p.secondArmedPodBonus = 400;
        p.thirdArmedPodBonus = 200;
        p.singlePodMonopolyPenalty = 300;
        p.leapfrogBatteryWeight = 200;
        p.leapfrogForwardBonus = 150;
        p.jumpReachableSquareWeight = 60;
        p.jumpReachStepWeight = 150;
        p.hangingPodBasePenalty = 1200;
        p.hangingProngWeight = 180;
        p.ofkProngSeizureWeight = 320;
        p.opponentTurnHangingPct = 100;
        p.soleArmedPodDangerPenalty = 1000;
        p.moveOrderBlunderPenalty = 2000;
        break;
    case BotDifficulty::TACTICAL:
        p.timeBudgetMs = 10000;
        p.maxSearchDepth = 64;
        p.maxQuiesceDepth = 2;
        p.quiesceBranchCap = 6;
        p.rootBranchCap = 40;
        p.treeBranchCap = 24;
        p.lazySmpThreads = 2;
        p.compositeMoveMaxPly = 1;
        p.rootScoreJitter = 20;
        p.closestPodDistWeight = 320;
        p.secondClosestPodDistWeight = 150;
        p.oneStepBaseThreatBonus = 8500;
        p.oneJumpBaseThreatBonus = 12000;
        p.twoPlyBaseThreatWeight = 2400;
        p.firstArmedPodBonus = 600;
        p.secondArmedPodBonus = 1100;
        p.thirdArmedPodBonus = 600;
        p.singlePodMonopolyPenalty = 1000;
        p.leapfrogBatteryWeight = 550;
        p.leapfrogForwardBonus = 400;
        p.jumpReachableSquareWeight = 130;
        p.jumpReachStepWeight = 450;
        p.hangingPodBasePenalty = 1400;
        p.hangingProngWeight = 220;
        p.ofkProngSeizureWeight = 450;
        p.opponentTurnHangingPct = 125;
        p.soleArmedPodDangerPenalty = 3500;
        p.moveOrderBlunderPenalty = 7000;
        break;
    case BotDifficulty::MASTER:
        p.timeBudgetMs = 20000;
        p.maxSearchDepth = 64;
        p.maxQuiesceDepth = 4;
        p.quiesceBranchCap = 10;
        p.rootBranchCap = 56;
        p.treeBranchCap = 32;
        p.lazySmpThreads = 4;
        p.compositeMoveMaxPly = 2;
        p.rootScoreJitter = 0;
        p.closestPodDistWeight = 350;
        p.secondClosestPodDistWeight = 220;
        p.oneStepBaseThreatBonus = 9500;
        p.oneJumpBaseThreatBonus = 14000;
        p.twoPlyBaseThreatWeight = 4200;
        p.firstArmedPodBonus = 700;
        p.secondArmedPodBonus = 1500;
        p.thirdArmedPodBonus = 900;
        p.singlePodMonopolyPenalty = 1400;
        p.leapfrogBatteryWeight = 800;
        p.leapfrogForwardBonus = 600;
        p.jumpReachableSquareWeight = 180;
        p.jumpReachStepWeight = 650;
        p.hangingPodBasePenalty = 1600;
        p.hangingProngWeight = 260;
        p.ofkProngSeizureWeight = 550;
        p.opponentTurnHangingPct = 140;
        p.soleArmedPodDangerPenalty = 6000;
        p.moveOrderBlunderPenalty = 11000;
        break;
    }
    return p;
}

void OctiBotEngine::setTimeBudgetMs(int maxTimeMs)
{
    _timeBudgetMs = std::max(10, maxTimeMs);
    _profile.timeBudgetMs = _timeBudgetMs;
    _customTimeBudget = true;
}

int OctiBotEngine::timeBudgetMs() const
{
    return _timeBudgetMs;
}

int OctiBotEngine::liveDepthReached() const
{
    return _liveDepthReached.load(std::memory_order_relaxed);
}

std::uint64_t OctiBotEngine::liveNodesVisited() const
{
    return _liveNodesVisited.load(std::memory_order_relaxed);
}

int OctiBotEngine::liveBestScore() const
{
    return _liveBestScore.load(std::memory_order_relaxed);
}

int OctiBotEngine::defaultBudgetForDifficulty(BotDifficulty d)
{
    return defaultProfileForDifficulty(d).timeBudgetMs;
}

int OctiBotEngine::maxDepthForDifficulty(BotDifficulty d)
{
    return defaultProfileForDifficulty(d).maxSearchDepth;
}

int OctiBotEngine::rootCapForDifficulty(BotDifficulty d)
{
    return defaultProfileForDifficulty(d).rootBranchCap;
}

int OctiBotEngine::branchCapForDifficulty(BotDifficulty d)
{
    return defaultProfileForDifficulty(d).treeBranchCap;
}

std::uint64_t OctiBotEngine::nextRandom()
{
    std::uint64_t x = _rngState;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    _rngState = x;
    return x;
}

// ---------------------------------------------------------------------------
// Fast Deterministic 64-bit Zobrist Hashing (Zero Heap Allocations)
// ---------------------------------------------------------------------------

namespace {

OctiDir mirrorDirHorizontal(OctiDir d)
{
    switch (d) {
    case DIR_N:  return DIR_N;
    case DIR_NE: return DIR_NW;
    case DIR_E:  return DIR_W;
    case DIR_SE: return DIR_SW;
    case DIR_S:  return DIR_S;
    case DIR_SW: return DIR_SE;
    case DIR_W:  return DIR_E;
    case DIR_NW: return DIR_NE;
    default:     return DIR_UNDEF;
    }
}

std::uint64_t computePositionHashInternal(const OctiBoard &board,
                                          bool mirrorHorizontal)
{
    std::uint64_t h = 0;
    const OctiGame *game = board.getGame();
    if (!game) {
        return h;
    }

    int w = board.getBoardWidth();
    int hgt = board.getBoardHeight();
    OctiGameMode mode = board.getGameMode();
    h ^= g_zobrist.mode[static_cast<int>(mode) % 4];
    h ^= g_zobrist.turn[static_cast<int>(game->getTurn()) % 5];
    int btw = std::min(4, std::max(0, game->getBasesToWin()));
    h ^= g_zobrist.basesToWin[btw];
    h ^= g_zobrist.edgeless[game->useEdgelessBoard() ? 1 : 0];
    h ^= g_zobrist.superProngs[game->useSuperProngs() ? 1 : 0];

    for (int y = 0; y < hgt; ++y) {
        for (int x = 0; x < w; ++x) {
            int effX = mirrorHorizontal ? ((w - 1) - x) : x;
            int sqIdx = y * 9 + effX;
            if (sqIdx < 0 || sqIdx >= 81) {
                continue;
            }
            const OctiSquare &sq = board.squareAt(x, y);
            if (sq.isCaptured()) {
                for (int pl = 1; pl <= 4; ++pl) {
                    if (sq.isCapturedBy(static_cast<OctiPlayer>(pl))) {
                        h ^= g_zobrist.baseCaptured[sqIdx][pl];
                        break;
                    }
                }
            }
            int numPods = std::min(7, sq.getNumPods());
            for (int p = 0; p < numPods; ++p) {
                const OctiPod &pod = sq.getPodAt(p);
                int pType = static_cast<int>(pod.getType()) - 1;
                if (pType >= 0 && pType < 4) {
                    h ^= g_zobrist.piece[sqIdx][p][pType];
                }
                for (int d = 0; d < 8; ++d) {
                    OctiDir srcDir = static_cast<OctiDir>(d);
                    OctiDir effDir = mirrorHorizontal
                        ? mirrorDirHorizontal(srcDir)
                        : srcDir;
                    OctiProngType pt = pod.prongAt(srcDir);
                    int ptIdx = static_cast<int>(pt);
                    int dIdx = static_cast<int>(effDir);
                    if (ptIdx >= 0 && ptIdx < 3 && dIdx >= 0 && dIdx < 8) {
                        h ^= g_zobrist.prong[sqIdx][p][dIdx][ptIdx];
                    }
                }
            }
        }
    }

    int numPlayers = (mode == GAME_MODE_4) ? 4 : 2;
    for (int pl = 1; pl <= numPlayers; ++pl) {
        OctiPlayer p = static_cast<OctiPlayer>(pl);
        int fp = std::min(7, std::max(0, game->getFreePods(p)));
        int cp = std::min(7, std::max(0, game->getCapturedPods(p)));
        int fpr = std::min(32, std::max(0, game->getFreeProngs(p)));
        int fspr = std::min(8, std::max(0, game->getFreeSuperProngs(p)));
        h ^= g_zobrist.freePods[pl][fp];
        h ^= g_zobrist.capturedPods[pl][cp];
        h ^= g_zobrist.freeProngs[pl][fpr];
        h ^= g_zobrist.freeSuperProngs[pl][fspr];
    }

    return h;
}

} // namespace

std::uint64_t OctiBotEngine::computePositionHashStatic(const OctiBoard &board)
{
    return computePositionHashInternal(board, false);
}

std::uint64_t OctiBotEngine::computePositionHash(const OctiBoard &board) const
{
    return computePositionHashStatic(board);
}

std::uint64_t OctiBotEngine::computeCanonicalOpeningHash(const OctiBoard &board,
                                                         bool *outMirrored)
{
    std::uint64_t hDirect = computePositionHashInternal(board, false);
    if (board.getGameMode() == GAME_MODE_4) {
        if (outMirrored) {
            *outMirrored = false;
        }
        return hDirect;
    }

    std::uint64_t hMirror = computePositionHashInternal(board, true);
    if (hMirror < hDirect) {
        if (outMirrored) {
            *outMirrored = true;
        }
        return hMirror;
    }
    if (outMirrored) {
        *outMirrored = false;
    }
    return hDirect;
}

std::string OctiBotEngine::mirrorMoveNotation(const std::string &moveNotation,
                                              int boardWidth)
{
    if (boardWidth < 1 || boardWidth > 9) {
        return "";
    }
    try {
        OctiMove parsed(moveNotation);
        if (parsed.isEmpty()) {
            return "";
        }
        std::vector<OctiMoveOp> mirroredOps = parsed.getOps();
        for (auto &op : mirroredOps) {
            int mx0 = (boardWidth - 1) - op.x0;
            if (mx0 < 0 || mx0 >= boardWidth) {
                return "";
            }
            op.x0 = mx0;

            OctiPod mirroredPod(op.pod.getType());
            for (int d = 0; d < 8; ++d) {
                OctiDir dir = static_cast<OctiDir>(d);
                OctiProngType pt = op.pod.prongAt(dir);
                if (pt != NO_PRONG) {
                    mirroredPod.addProng(mirrorDirHorizontal(dir),
                                         pt == SUPER_PRONG);
                }
            }
            op.pod = mirroredPod;

            if (op.addProngDir != DIR_UNDEF) {
                op.addProngDir = mirrorDirHorizontal(op.addProngDir);
            }
            if (op.rmProngDir != DIR_UNDEF) {
                op.rmProngDir = mirrorDirHorizontal(op.rmProngDir);
            }
            for (auto &h : op.hops) {
                int mhx = (boardWidth - 1) - h.x;
                if (mhx < 0 || mhx >= boardWidth) {
                    return "";
                }
                h.x = mhx;
            }
        }
        OctiMove mirroredMove(mirroredOps);
        return mirroredMove.asString();
    } catch (...) {
        return "";
    }
}

bool OctiBotEngine::probeOpeningBookTable(const OctiBoard &board,
                                          const OpeningBookEntry *entries,
                                          std::size_t entryCount,
                                          const BotTuningProfile &profile,
                                          std::uint64_t &rngState,
                                          std::string &outMove,
                                          int *outScore,
                                          int *outDepth)
{
    outMove.clear();
    if (!profile.useOpeningBook || !entries || entryCount == 0) {
        return false;
    }

    const OctiGame *srcGame = board.getGame();
    if (!srcGame || !srcGame->isGameStarted() || srcGame->isGameOver() ||
        srcGame->getTurn() == PLAYER_UNDEF) {
        return false;
    }

    bool mirrored = false;
    std::uint64_t key = computeCanonicalOpeningHash(board, &mirrored);

    std::size_t lo = 0;
    std::size_t hi = entryCount;
    const OpeningBookEntry *found = nullptr;
    while (lo < hi) {
        std::size_t mid = lo + (hi - lo) / 2;
        if (entries[mid].canonicalKey < key) {
            lo = mid + 1;
        } else if (entries[mid].canonicalKey > key) {
            hi = mid;
        } else {
            found = &entries[mid];
            break;
        }
    }

    if (!found || found->numCandidates == 0) {
        return false;
    }

    std::vector<OctiMove> legalMoves = srcGame->generateLegalMoves();
    if (legalMoves.empty()) {
        return false;
    }

    int w = board.getBoardWidth();
    int maxCands = std::min<int>(3, found->numCandidates);

    struct ValidCand {
        std::string moveStr;
        int score;
    };
    std::vector<ValidCand> validCands;
    validCands.reserve(static_cast<std::size_t>(maxCands));

    for (int i = 0; i < maxCands; ++i) {
        const char *rawMove = found->candidates[i].move;
        if (!rawMove || rawMove[0] == '\0') {
            continue;
        }
        std::string candMoveStr = mirrored
            ? mirrorMoveNotation(rawMove, w)
            : std::string(rawMove);
        if (candMoveStr.empty()) {
            continue;
        }
        try {
            OctiMove parsed(candMoveStr);
            std::string canonicalStr = parsed.asString();
            bool isLegal = false;
            for (const auto &lm : legalMoves) {
                if (lm == parsed || lm.asString() == canonicalStr) {
                    isLegal = true;
                    candMoveStr = lm.asString();
                    break;
                }
            }
            if (!isLegal && parsed.getOps().size() > 1 &&
                board.getGameMode() != GAME_MODE_OFK) {
                try {
                    OctiBoard testBoard;
                    testBoard.suppressBoardUpdateNotification(true);
                    testBoard.fromPositionString(board.toPositionString());
                    if (testBoard.getGame()) {
                        testBoard.getGame()->setSearchMode(true);
                        testBoard.getGame()->move(parsed);
                        isLegal = true;
                        candMoveStr = canonicalStr;
                    }
                } catch (...) {
                    isLegal = false;
                }
            }
            if (isLegal) {
                validCands.push_back(
                    {candMoveStr, static_cast<int>(found->candidates[i].score)});
            }
        } catch (...) {
            continue;
        }
    }

    if (validCands.empty()) {
        return false;
    }

    int bestScore = validCands[0].score;
    int window = (profile.rootScoreJitter > 0) ? profile.rootScoreJitter : 15;
    std::size_t eligibleCount = 1;
    for (std::size_t i = 1; i < validCands.size(); ++i) {
        if (validCands[i].score >= bestScore - window) {
            ++eligibleCount;
        }
    }

    std::size_t chosenIdx = 0;
    if (eligibleCount > 1 && profile.rootScoreJitter > 0) {
        std::uint64_t x = rngState ? rngState : 0x5EED2026ULL;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        rngState = x;
        chosenIdx = static_cast<std::size_t>(x % eligibleCount);
    }

    outMove = validCands[chosenIdx].moveStr;
    if (outScore) {
        *outScore = validCands[chosenIdx].score;
    }
    if (outDepth) {
        *outDepth = static_cast<int>(found->depth);
    }
    return true;
}

bool OctiBotEngine::probeOpeningBook(const OctiBoard &board,
                                     std::string &outMove,
                                     int *outScore,
                                     int *outDepth)
{
    return probeOpeningBookTable(board,
                                 g_openingBookEntries,
                                 g_openingBookEntryCount,
                                 _profile,
                                 _rngState,
                                 outMove,
                                 outScore,
                                 outDepth);
}

// ---------------------------------------------------------------------------
// Hybrid 2001 + 2026 Evaluation Function
// ---------------------------------------------------------------------------

int OctiBotEngine::evaluatePosition(const OctiBoard &board,
                                    OctiPlayer perspective)
{
    return evaluatePosition(board, perspective, defaultProfileForDifficulty(BotDifficulty::MASTER));
}

int OctiBotEngine::evaluatePosition(const OctiBoard &board,
                                    OctiPlayer perspective,
                                    const BotTuningProfile &profile)
{
    const OctiGame *game = board.getGame();
    if (!game || perspective == PLAYER_UNDEF) {
        return 0;
    }

    OctiGameMode mode = board.getGameMode();
    if (game->isGameOver()) {
        OctiPlayer w = game->getWinner();
        if (w == PLAYER_UNDEF) {
            return 0;
        }
        return areAlliedPlayers(mode, w, perspective) ? WIN_SCORE : -WIN_SCORE;
    }

    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    bool edgeless = game->useEdgelessBoard();

    std::vector<std::pair<int, int>> allyTargets = targetEnemyBases(board, perspective);
    OctiPlayer sampleEnemy = (perspective == PLAYER_A || perspective == PLAYER_C)
        ? PLAYER_B : PLAYER_A;
    std::vector<std::pair<int, int>> enemyTargets = targetEnemyBases(board, sampleEnemy);

    int totalScore = 0;
    int totalAllyPodsOnBoard = 0;
    int totalEnemyPodsOnBoard = 0;
    int allyArmedPods = 0;
    int enemyArmedPods = 0;
    int allyUnarmedHomePods = 0;
    int enemyUnarmedHomePods = 0;

    int playerBoardPods[5] = {0};
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const OctiSquare &sq = board.squareAt(x, y);
            int numPods = sq.getNumPods();
            if (numPods > 0) {
                OctiPlayer sqOwner = podTypeToPlayer(sq.getPodAt(0).getType());
                playerBoardPods[static_cast<int>(sqOwner)] += numPods;
            }
        }
    }

    CoalitionPhaseContext allyPhase = analyzeCoalitionPhase(board, perspective);
    CoalitionPhaseContext enemyPhase = analyzeCoalitionPhase(board, sampleEnemy);

    int allyBaseThreatScalePct = allyPhase.baseThreatScalePct;
    int enemyBaseThreatScalePct = enemyPhase.baseThreatScalePct;
    bool allyBaseWinFeasible = allyPhase.feasible;
    bool enemyBaseWinFeasible = enemyPhase.feasible;
    int allyCapturedPods = allyPhase.capturedPods;
    int enemyCapturedPods = enemyPhase.capturedPods;

    std::vector<std::pair<int, int>> allyPodCoords;
    std::vector<std::pair<int, int>> enemyPodCoords;
    std::uint64_t allyJumpThreatMask[2] = {0ULL, 0ULL};
    std::uint64_t enemyJumpThreatMask[2] = {0ULL, 0ULL};

    int podZeroVal = profile.podZeroProngVal;
    int podOneVal = (mode == GAME_MODE_OFK) ? (profile.podOneProngVal * 135) / 100 : profile.podOneProngVal;
    int podTwoVal = (mode == GAME_MODE_OFK) ? (profile.podTwoProngVal * 135) / 100 : profile.podTwoProngVal;
    int podThreePlusVal = (mode == GAME_MODE_OFK) ? (profile.podThreePlusProngVal * 135) / 100 : profile.podThreePlusProngVal;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const OctiSquare &sq = board.squareAt(x, y);
            OctiSquareType sqType = sq.getType();

            int numBases = std::max(1, game->getBasesToWin());
            if (isEnemyHomeSquareFor(sqType, mode, perspective)) {
                if (sq.isCapturedByTeamOf(perspective)) {
                    int baseBonus = ((profile.capturedEnemyBaseBonus / numBases) * allyBaseThreatScalePct) / 100;
                    totalScore += baseBonus;
                }
            } else if (isAlliedHomeSquareFor(sqType, mode, perspective)) {
                if (sq.isCaptured() && !sq.isCapturedByTeamOf(perspective)) {
                    int basePen = ((profile.lostAlliedBasePenalty / numBases) * enemyBaseThreatScalePct) / 100;
                    totalScore -= basePen;
                }
            } else if (sqType == NEUTRAL_OCTI_SQUARE) {
                if (sq.isCapturedByTeamOf(perspective)) {
                    totalScore += profile.neutralOctiSquareBonus;
                } else if (sq.isCaptured()) {
                    totalScore -= profile.neutralOctiSquareBonus;
                }
            }

            int numPods = sq.getNumPods();
            if (numPods > 0) {
                OctiPlayer sqOwner = podTypeToPlayer(sq.getPodAt(0).getType());
                bool sqAlly = areAlliedPlayers(mode, sqOwner, perspective);
                if (sqAlly) {
                    totalAllyPodsOnBoard += numPods;
                } else {
                    totalEnemyPodsOnBoard += numPods;
                }

                bool onCapturedEnemyBase = isEnemyHomeSquareFor(sqType, mode, sqOwner) &&
                                           sq.isCapturedByTeamOf(sqOwner);
                bool onAlliedHome = isAlliedHomeSquareFor(sqType, mode, sqOwner);
                int unarmedCount = 0;
                for (int i = 0; i < numPods; ++i) {
                    const OctiPod &p = sq.getPodAt(i);
                    if (p.getNumProngs() == 0) {
                        ++unarmedCount;
                    }
                    if (onAlliedHome && p.getNumProngs() == 0) {
                        if (sqAlly) {
                            ++allyUnarmedHomePods;
                        } else {
                            ++enemyUnarmedHomePods;
                        }
                    }
                }

                if (numPods > 1 && !onCapturedEnemyBase) {
                    int stackPen = (numPods - 1) * 450 + unarmedCount * 950;
                    totalScore += sqAlly ? -stackPen : stackPen;
                }
            }

            for (int i = 0; i < numPods; ++i) {
                const OctiPod &pod = sq.getPodAt(i);
                OctiPlayer owner = podTypeToPlayer(pod.getType());
                bool isAlly = areAlliedPlayers(mode, owner, perspective);
                const auto &targets = isAlly ? allyTargets : enemyTargets;
                int numProngs = pod.getNumProngs();

                if (numProngs > 0) {
                    if (isAlly) {
                        allyPodCoords.push_back({x, y});
                        ++allyArmedPods;
                    } else {
                        enemyPodCoords.push_back({x, y});
                        ++enemyArmedPods;
                    }
                }

                int dist = minChebyshevToTargets(x, y, targets, w, h, edgeless);
                bool onCapturedEnemyBase = isEnemyHomeSquareFor(sqType, mode, owner) &&
                                           sq.isCapturedByTeamOf(owner);

                int scalePct = isAlly ? allyBaseThreatScalePct : enemyBaseThreatScalePct;
                int podVal = 0;
                if (numProngs == 0 && !onCapturedEnemyBase && sqType != NEUTRAL_OCTI_SQUARE) {
                    podVal = podZeroVal;
                } else if (numProngs == 1) {
                    int distBonus = (std::max(0, (10 - dist)) * 110 * scalePct) / 100;
                    podVal = podOneVal + distBonus;
                    if (!isAlliedHomeSquareFor(sqType, mode, owner) && dist <= 4 && dist >= 2) {
                        podVal -= profile.loneOneProngRushPenalty;
                    }
                } else if (numProngs == 2) {
                    int distBonus = (std::max(0, (10 - dist)) * 180 * scalePct) / 100;
                    podVal = podTwoVal + distBonus;
                } else {
                    int distBonus = (std::max(0, (10 - dist)) * 210 * scalePct) / 100;
                    podVal = podThreePlusVal + distBonus;
                }

                bool hasOrth = false;
                bool hasDiag = false;
                bool hasOneStepBaseThreat = false;
                bool hasOneJumpBaseThreat = false;

                for (int d = 0; d < DIR_MAX; ++d) {
                    OctiDir dir = static_cast<OctiDir>(d);
                    OctiProngType pt = pod.prongAt(dir);
                    if (pt == NO_PRONG) {
                        continue;
                    }
                    podVal += (pt == SUPER_PRONG) ? profile.superProngVal : profile.normalProngVal;
                    if (isOrthogonalDir(dir)) hasOrth = true;
                    if (isDiagonalDir(dir)) hasDiag = true;

                    if (owner == PLAYER_A || owner == PLAYER_C) {
                        if (dir == DIR_N) {
                            podVal += 380;
                        } else if (dir == DIR_NE || dir == DIR_NW) {
                            podVal += 260;
                        } else if (dir == DIR_E || dir == DIR_W) {
                            podVal += 120;
                        }
                    } else if (owner == PLAYER_B || owner == PLAYER_D) {
                        if (dir == DIR_S) {
                            podVal += 380;
                        } else if (dir == DIR_SE || dir == DIR_SW) {
                            podVal += 260;
                        } else if (dir == DIR_E || dir == DIR_W) {
                            podVal += 120;
                        }
                    }

                    int nx = edgeless ? (((x + dirDeltaX(dir)) % w + w) % w) : (x + dirDeltaX(dir));
                    int ny = edgeless ? (((y + dirDeltaY(dir)) % h + h) % h) : (y + dirDeltaY(dir));
                    if (board.isValidCoord(nx, ny)) {
                        int ndist = minChebyshevToTargets(nx, ny, targets, w, h, edgeless);
                        if (ndist < dist) {
                            podVal += 140;
                        }
                        const OctiSquare &nsq = board.squareAt(nx, ny);
                        bool canStepBase = (mode == GAME_MODE_OFK) ? nsq.isEmpty() : !nsq.hasEnemyOf(owner);
                        if (ndist == 0 && canStepBase) {
                            hasOneStepBaseThreat = true;
                        }
                        // Leapfrog battery detection
                        if (squareHasAllyOf(nsq, mode, owner)) {
                            int jx = edgeless ? (((x + 2 * dirDeltaX(dir)) % w + w) % w) : (x + 2 * dirDeltaX(dir));
                            int jy = edgeless ? (((y + 2 * dirDeltaY(dir)) % h + h) % h) : (y + 2 * dirDeltaY(dir));
                            if (board.isValidCoord(jx, jy)) {
                                const OctiSquare &destSq = board.squareAt(jx, jy);
                                bool canJumpBase = (mode == GAME_MODE_OFK) ? destSq.isEmpty() : !destSq.hasEnemyOf(owner);
                                if (destSq.isEmpty() || canJumpBase) {
                                    podVal += profile.leapfrogBatteryWeight;
                                    int jdist = minChebyshevToTargets(jx, jy, targets, w, h, edgeless);
                                    if (jdist < dist) {
                                        podVal += profile.leapfrogForwardBonus;
                                    }
                                }
                            }
                        }
                    }

                    int jx = edgeless ? (((x + 2 * dirDeltaX(dir)) % w + w) % w) : (x + 2 * dirDeltaX(dir));
                    int jy = edgeless ? (((y + 2 * dirDeltaY(dir)) % h + h) % h) : (y + 2 * dirDeltaY(dir));
                    if (board.isValidCoord(nx, ny) && board.isValidCoord(jx, jy)) {
                        const OctiSquare &midSq = board.squareAt(nx, ny);
                        const OctiSquare &destSq = board.squareAt(jx, jy);
                        if (!midSq.isEmpty() && destSq.isEmpty()) {
                            int jdist = minChebyshevToTargets(jx, jy, targets, w, h, edgeless);
                            if (jdist == 0) {
                                hasOneJumpBaseThreat = true;
                            }
                        }
                    }
                }

                if (hasOneStepBaseThreat) {
                    podVal += (profile.oneStepBaseThreatBonus * scalePct) / 100;
                }
                if (hasOneJumpBaseThreat) {
                    podVal += (profile.oneJumpBaseThreatBonus * scalePct) / 100;
                }

                // 2-ply base threat check
                if (dist == 1 && profile.twoPlyBaseThreatWeight > 0) {
                    podVal += (profile.twoPlyBaseThreatWeight * scalePct) / 100;
                }

                if (hasOrth && hasDiag) {
                    podVal += profile.orthDiagVersatilityBonus;
                } else if (numProngs >= 2 && hasDiag && !hasOrth) {
                    podVal -= profile.diagOnlyParityPenalty;
                }

                JumpMobilityThreat threat;
                if (numProngs > 0) {
                    threat = evaluatePodJumpThreat(board, x, y, pod, targets);
                    podVal += threat.reachableSquares * profile.jumpReachableSquareWeight;
                    podVal += threat.capturablePods * profile.jumpCapturablePodWeight;
                    podVal += threat.capturableProngs * profile.jumpCapturableProngWeight;
                    if (threat.minJumpDistToTargets < dist) {
                        podVal += ((dist - threat.minJumpDistToTargets) * profile.jumpReachStepWeight * scalePct) / 100;
                    }
                    if (isAlly) {
                        allyJumpThreatMask[0] |= threat.capturedSqMask[0];
                        allyJumpThreatMask[1] |= threat.capturedSqMask[1];
                    } else {
                        enemyJumpThreatMask[0] |= threat.capturedSqMask[0];
                        enemyJumpThreatMask[1] |= threat.capturedSqMask[1];
                    }
                }

                totalScore += isAlly ? podVal : -podVal;
            }
        }
    }

    // Army Development Bonuses (Armed Pods)
    if (allyArmedPods >= 1) totalScore += profile.firstArmedPodBonus;
    if (allyArmedPods >= 2) totalScore += profile.secondArmedPodBonus;
    if (allyArmedPods >= 3) totalScore += profile.thirdArmedPodBonus;

    if (enemyArmedPods >= 1) totalScore -= profile.firstArmedPodBonus;
    if (enemyArmedPods >= 2) totalScore -= profile.secondArmedPodBonus;
    if (enemyArmedPods >= 3) totalScore -= profile.thirdArmedPodBonus;

    // Single-Pod Monopoly Penalty
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const OctiSquare &sq = board.squareAt(x, y);
            for (int i = 0; i < sq.getNumPods(); ++i) {
                const OctiPod &pod = sq.getPodAt(i);
                int prongs = pod.getNumProngs();
                if (prongs > 2) {
                    bool isAlly = areAlliedPlayers(mode, podTypeToPlayer(pod.getType()), perspective);
                    if (isAlly && allyUnarmedHomePods >= 2) {
                        totalScore -= (prongs - 2) * profile.singlePodMonopolyPenalty;
                    } else if (!isAlly && enemyUnarmedHomePods >= 2) {
                        totalScore += (prongs - 2) * profile.singlePodMonopolyPenalty;
                    }
                }
            }
        }
    }

    // Hanging-Pod Vulnerability Penalty
    bool opponentTurnNext = (game->getTurn() != perspective);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const OctiSquare &sq = board.squareAt(x, y);
            int numPods = sq.getNumPods();
            if (numPods == 0) {
                continue;
            }
            int sqBit = y * 9 + x;
            int wordIdx = (sqBit >> 6) & 1;
            std::uint64_t bitMask = 1ULL << (sqBit & 63);
            OctiPlayer sqOwner = podTypeToPlayer(sq.getPodAt(0).getType());
            bool sqAlly = areAlliedPlayers(mode, sqOwner, perspective);

            int prongsOnSq = 0;
            for (int i = 0; i < numPods; ++i) {
                prongsOnSq += sq.getPodAt(i).getNumProngs();
            }

            int prongWeight = (mode == GAME_MODE_OFK)
                ? profile.ofkProngSeizureWeight
                : profile.hangingProngWeight;

            if (sqAlly && (enemyJumpThreatMask[wordIdx] & bitMask) != 0ULL) {
                int vulnPen = numPods * profile.hangingPodBasePenalty + prongsOnSq * prongWeight;
                if (opponentTurnNext) {
                    vulnPen = (vulnPen * profile.opponentTurnHangingPct) / 100;
                }
                if (prongsOnSq > 0 && allyArmedPods == 1) {
                    vulnPen += profile.soleArmedPodDangerPenalty;
                }
                if (numPods >= totalAllyPodsOnBoard) {
                    vulnPen += profile.wipeoutDangerPenalty;
                } else if (mode == GAME_MODE_4 && numPods >= playerBoardPods[static_cast<int>(sqOwner)] && game->getFreePods(sqOwner) == 0) {
                    vulnPen += profile.wipeoutDangerPenalty / 2;
                }
                totalScore -= vulnPen;
            } else if (!sqAlly && (allyJumpThreatMask[wordIdx] & bitMask) != 0ULL) {
                int vulnPen = numPods * profile.hangingPodBasePenalty + prongsOnSq * prongWeight;
                if (!opponentTurnNext) {
                    vulnPen = (vulnPen * profile.opponentTurnHangingPct) / 100;
                }
                if (prongsOnSq > 0 && enemyArmedPods == 1) {
                    vulnPen += profile.soleArmedPodDangerPenalty;
                }
                if (numPods >= totalEnemyPodsOnBoard) {
                    vulnPen += profile.wipeoutDangerPenalty;
                } else if (mode == GAME_MODE_4 && numPods >= playerBoardPods[static_cast<int>(sqOwner)] && game->getFreePods(sqOwner) == 0) {
                    vulnPen += profile.wipeoutDangerPenalty / 2;
                }
                totalScore += vulnPen;
            }
        }
    }

    if (totalAllyPodsOnBoard == 0) {
        totalScore -= (profile.wipeoutDangerPenalty + profile.soleArmedPodDangerPenalty);
    }
    if (totalEnemyPodsOnBoard == 0) {
        totalScore += (profile.wipeoutDangerPenalty + profile.soleArmedPodDangerPenalty);
    }

    // 2001 Multi-Base Minimum Chebyshev Distance: closest (d_min1) and second-closest (d_min2)
    int allyDistScalePct = (allyBaseWinFeasible ? 100 : (allyCapturedPods > 0 ? 40 : 15));
    int enemyDistScalePct = (enemyBaseWinFeasible ? 100 : (enemyCapturedPods > 0 ? 40 : 15));

    allyDistScalePct = (allyDistScalePct * allyBaseThreatScalePct) / 100;
    enemyDistScalePct = (enemyDistScalePct * enemyBaseThreatScalePct) / 100;

    int allyClosestPodDistWeight = (profile.closestPodDistWeight * allyDistScalePct) / 100;
    int allySecondClosestPodDistWeight = (profile.secondClosestPodDistWeight * allyDistScalePct) / 100;
    int enemyClosestPodDistWeight = (profile.closestPodDistWeight * enemyDistScalePct) / 100;
    int enemySecondClosestPodDistWeight = (profile.secondClosestPodDistWeight * enemyDistScalePct) / 100;

    if (!allyTargets.empty()) {
        for (const auto &tb : allyTargets) {
            const OctiSquare &tsq = board.squareAt(tb.first, tb.second);
            if (!tsq.isCapturedByTeamOf(perspective)) {
                int d_min1 = 999;
                int d_min2 = 999;
                for (const auto &ap : allyPodCoords) {
                    int d = chebyshevDist(ap.first, ap.second, tb.first, tb.second, w, h, edgeless);
                    if (d < d_min1) {
                        d_min2 = d_min1;
                        d_min1 = d;
                    } else if (d < d_min2) {
                        d_min2 = d;
                    }
                }
                if (d_min1 < 999) {
                    totalScore += std::max(0, 10 - d_min1) * allyClosestPodDistWeight;
                }
                if (d_min2 < 999 && allySecondClosestPodDistWeight > 0) {
                    totalScore += std::max(0, 10 - d_min2) * allySecondClosestPodDistWeight;
                }
            }
        }
    }

    if (!enemyTargets.empty()) {
        for (const auto &tb : enemyTargets) {
            const OctiSquare &tsq = board.squareAt(tb.first, tb.second);
            if (!tsq.isCapturedByTeamOf(sampleEnemy)) {
                int d_min1 = 999;
                int d_min2 = 999;
                for (const auto &ep : enemyPodCoords) {
                    int d = chebyshevDist(ep.first, ep.second, tb.first, tb.second, w, h, edgeless);
                    if (d < d_min1) {
                        d_min2 = d_min1;
                        d_min1 = d;
                    } else if (d < d_min2) {
                        d_min2 = d;
                    }
                }
                if (d_min1 < 999) {
                    totalScore -= std::max(0, 10 - d_min1) * enemyClosestPodDistWeight;
                }
                if (d_min2 < 999 && enemySecondClosestPodDistWeight > 0) {
                    totalScore -= std::max(0, 10 - d_min2) * enemySecondClosestPodDistWeight;
                }
            }
        }
    }

    // Reserves and material bonuses
    int numPlayers = (mode == GAME_MODE_4) ? 4 : 2;
    for (int pIdx = 1; pIdx <= numPlayers; ++pIdx) {
        OctiPlayer p = static_cast<OctiPlayer>(pIdx);
        bool isAlly = areAlliedPlayers(mode, p, perspective);
        int freePods = game->getFreePods(p);
        int capturedPodPen = (mode == GAME_MODE_OFK) ? 4800 : 3800;
        int freeProngVal = (mode == GAME_MODE_OFK) ? 180 : 140;
        int resVal = freePods * 450
                   - game->getCapturedPods(p) * capturedPodPen
                   + game->getFreeProngs(p) * freeProngVal
                   + game->getFreeSuperProngs(p) * 150;
        if (mode != GAME_MODE_OFK && freePods < 4 &&
            (isAlly ? (allyUnarmedHomePods >= 2 && totalAllyPodsOnBoard >= 2)
                    : (enemyUnarmedHomePods >= 2 && totalEnemyPodsOnBoard >= 2)) &&
            game->getFreeProngs(p) >= 6) {
            resVal -= (4 - freePods) * 1500;
        }
        if (mode == GAME_MODE_4 && game->isPlayerEliminated(p)) {
            resVal -= 20000;
        }
        if (board.canLiberate(p) && game->getCapturedPods(p) > 0) {
            resVal += 2500;
        }
        totalScore += isAlly ? resVal : -resVal;
    }

    return totalScore;
}

// ---------------------------------------------------------------------------
// Quiescence Search with Stand-Pat & Tactical Capture/Base Chains
// ---------------------------------------------------------------------------

int OctiBotEngine::quiesce(OctiBoard &scratchBoard,
                           int qDepth,
                           int ply,
                           int alpha,
                           int beta,
                           OctiPlayer currentPerspective,
                           std::uint64_t &nodesVisited,
                           long deadlineMs,
                           bool &timedOut)
{
    ++nodesVisited;
    if ((nodesVisited & 63ULL) == 0ULL) {
        _liveNodesVisited.store(nodesVisited, std::memory_order_relaxed);
        if (_stopSearch.load(std::memory_order_relaxed) ||
            (deadlineMs < std::numeric_limits<long>::max() &&
             steadyClockMs() >= deadlineMs)) {
            timedOut = true;
            return evaluatePosition(scratchBoard, currentPerspective, _profile);
        }
    }

    OctiGame *game = scratchBoard.getGame();
    OctiGameMode mode = scratchBoard.getGameMode();
    if (game->isGameOver()) {
        OctiPlayer w = game->getWinner();
        if (w == PLAYER_UNDEF) {
            return 0;
        }
        return areAlliedPlayers(mode, w, currentPerspective)
            ? (WIN_SCORE - ply)
            : (-WIN_SCORE + ply);
    }

    int standPat = evaluatePosition(scratchBoard, currentPerspective, _profile);
    if (standPat >= beta) {
        return beta;
    }
    if (standPat > alpha) {
        alpha = standPat;
    }

    if (qDepth >= _profile.maxQuiesceDepth) {
        return standPat;
    }

    std::vector<OctiMove> rawMoves = game->generateLegalMoves();
    if (rawMoves.empty()) {
        return standPat;
    }

    // Filter to tactical moves only: captures and uncaptured enemy base entries
    OctiPlayer turn = game->getTurn();
    std::vector<OctiMove> tacticalMoves;
    for (const auto &m : rawMoves) {
        bool isTactical = false;
        for (const auto &op : m.getOps()) {
            for (const auto &hop : op.hops) {
                if (hop.capture) {
                    isTactical = true;
                    break;
                }
            }
            if (!op.hops.empty()) {
                const OctiHop &lastHop = op.hops.back();
                const OctiSquare &destSq = scratchBoard.squareAt(lastHop.x, lastHop.y);
                if (isEnemyHomeSquareFor(destSq.getType(), mode, turn) &&
                    !destSq.isCapturedByTeamOf(turn)) {
                    isTactical = true;
                }
            }
            if (isTactical) break;
        }
        if (isTactical) {
            tacticalMoves.push_back(m);
        }
    }

    if (tacticalMoves.empty()) {
        return standPat;
    }

    std::size_t qCap = (_profile.quiesceBranchCap > 0)
        ? static_cast<std::size_t>(_profile.quiesceBranchCap)
        : tacticalMoves.size();
    std::vector<OctiMove> ordered = orderAndCapMoves(
        scratchBoard, tacticalMoves, turn, "", qCap, _profile, false);

    OctiGameSnapshot snap = game->captureSnapshot();

    for (const auto &mv : ordered) {
        try {
            game->move(mv);
        } catch (...) {
            game->restoreSnapshot(snap);
            continue;
        }

        int val = 0;
        if (game->isGameOver()) {
            OctiPlayer w = game->getWinner();
            val = areAlliedPlayers(mode, w, turn)
                ? (WIN_SCORE - (ply + 1))
                : (-WIN_SCORE + (ply + 1));
        } else {
            OctiPlayer nextTurn = game->getTurn();
            bool sameTeamNext = areAlliedPlayers(mode, turn, nextTurn);
            if (sameTeamNext) {
                val = quiesce(scratchBoard, qDepth + 1, ply + 1,
                              alpha, beta, turn, nodesVisited, deadlineMs, timedOut);
            } else {
                OctiPlayer oppPerspective = (nextTurn != PLAYER_UNDEF) ? nextTurn :
                    ((turn == PLAYER_A || turn == PLAYER_C) ? PLAYER_B : PLAYER_A);
                val = -quiesce(scratchBoard, qDepth + 1, ply + 1,
                               -beta, -alpha, oppPerspective, nodesVisited, deadlineMs, timedOut);
            }
        }

        game->restoreSnapshot(snap);

        if (timedOut) {
            break;
        }

        if (val >= beta) {
            return beta;
        }
        if (val > alpha) {
            alpha = val;
        }
    }

    return alpha;
}

// ---------------------------------------------------------------------------
// Negamax Search with Transposition Table
// ---------------------------------------------------------------------------

int OctiBotEngine::negamax(OctiBoard &scratchBoard,
                           int depth,
                           int ply,
                           int alpha,
                           int beta,
                           OctiPlayer currentPerspective,
                           std::uint64_t &nodesVisited,
                           long deadlineMs,
                           bool &timedOut)
{
    ++nodesVisited;
    if ((nodesVisited & 63ULL) == 0ULL) {
        _liveNodesVisited.store(nodesVisited, std::memory_order_relaxed);
        if (_stopSearch.load(std::memory_order_relaxed) ||
            (deadlineMs < std::numeric_limits<long>::max() &&
             steadyClockMs() >= deadlineMs)) {
            timedOut = true;
            return evaluatePosition(scratchBoard, currentPerspective, _profile);
        }
    }

    OctiGame *game = scratchBoard.getGame();
    OctiGameMode mode = scratchBoard.getGameMode();
    if (game->isGameOver()) {
        OctiPlayer w = game->getWinner();
        if (w == PLAYER_UNDEF) {
            return 0;
        }
        return areAlliedPlayers(mode, w, currentPerspective)
            ? (WIN_SCORE - ply)
            : (-WIN_SCORE + ply);
    }

    if (depth <= 0) {
        if (_profile.maxQuiesceDepth > 0) {
            return quiesce(scratchBoard, 0, ply, alpha, beta, currentPerspective,
                           nodesVisited, deadlineMs, timedOut);
        }
        return evaluatePosition(scratchBoard, currentPerspective, _profile);
    }

    const bool useTtLock = (_profile.lazySmpThreads > 1);
    std::uint64_t hashKey = computePositionHash(scratchBoard);
    std::size_t ttIdx = hashKey % TT_SIZE;
    std::string ttMove;
    {
        std::unique_lock<std::mutex> lock(
            g_ttMutexes[ttIdx % TT_MUTEX_COUNT].mtx, std::defer_lock);
        if (useTtLock) {
            lock.lock();
        }
        const TranspositionEntry &ttEntry = _tt[ttIdx];
        if (ttEntry.key == hashKey && ttEntry.depth >= depth) {
            int ttScore = ttEntry.score;
            if (ttScore >= WIN_SCORE - 1000) {
                ttScore -= ply;
            } else if (ttScore <= -WIN_SCORE + 1000) {
                ttScore += ply;
            }

            if (ttEntry.flag == 0) {
                return ttScore;
            }
            if (ttEntry.flag == 1 && ttScore > alpha) {
                alpha = ttScore;
            } else if (ttEntry.flag == 2 && ttScore < beta) {
                beta = ttScore;
            }
            if (alpha >= beta) {
                return ttScore;
            }
            ttMove = ttEntry.bestMove;
        }
    }

    std::vector<OctiMove> rawMoves = game->generateLegalMoves();
    if (rawMoves.empty()) {
        return evaluatePosition(scratchBoard, currentPerspective, _profile);
    }

    OctiPlayer turn = game->getTurn();
    std::size_t maxBranch = (_profile.treeBranchCap > 0)
        ? static_cast<std::size_t>(_profile.treeBranchCap)
        : 24;
    std::vector<OctiMove> ordered = orderAndCapMoves(
        scratchBoard, rawMoves, turn, ttMove, maxBranch, _profile,
        (ply <= _profile.compositeMoveMaxPly && depth >= 2));

    int origAlpha = alpha;
    int bestVal = -INF_SCORE;
    std::string bestMoveStr = ordered[0].asString();
    OctiGameSnapshot snap = game->captureSnapshot();

    for (const auto &mv : ordered) {
        try {
            game->move(mv);
        } catch (...) {
            game->restoreSnapshot(snap);
            continue;
        }

        int val = 0;
        if (game->isGameOver()) {
            OctiPlayer w = game->getWinner();
            val = areAlliedPlayers(mode, w, turn)
                ? (WIN_SCORE - (ply + 1))
                : (-WIN_SCORE + (ply + 1));
        } else {
            OctiPlayer nextTurn = game->getTurn();
            bool sameTeamNext = areAlliedPlayers(mode, turn, nextTurn);
            if (sameTeamNext) {
                val = negamax(scratchBoard, depth - 1, ply + 1,
                              alpha, beta, turn, nodesVisited, deadlineMs, timedOut);
            } else {
                OctiPlayer oppPerspective = (nextTurn != PLAYER_UNDEF) ? nextTurn :
                    ((turn == PLAYER_A || turn == PLAYER_C) ? PLAYER_B : PLAYER_A);
                val = -negamax(scratchBoard, depth - 1, ply + 1,
                               -beta, -alpha, oppPerspective, nodesVisited, deadlineMs, timedOut);
            }
        }

        game->restoreSnapshot(snap);

        if (timedOut) {
            break;
        }

        if (val > bestVal) {
            bestVal = val;
            bestMoveStr = mv.asString();
        }
        if (val > alpha) {
            alpha = val;
        }
        if (alpha >= beta) {
            break;
        }
    }

    if (!timedOut && bestVal > -INF_SCORE) {
        std::unique_lock<std::mutex> lock(
            g_ttMutexes[ttIdx % TT_MUTEX_COUNT].mtx, std::defer_lock);
        if (useTtLock) {
            lock.lock();
        }
        TranspositionEntry &ttEntry = _tt[ttIdx];
        if (ttEntry.key == 0 || depth >= ttEntry.depth - 1) {
            ttEntry.key = hashKey;
            ttEntry.depth = depth;
            int storeScore = bestVal;
            if (storeScore >= WIN_SCORE - 1000) {
                storeScore += ply;
            } else if (storeScore <= -WIN_SCORE + 1000) {
                storeScore -= ply;
            }
            ttEntry.score = storeScore;
            ttEntry.bestMove = bestMoveStr;
            if (bestVal <= origAlpha) {
                ttEntry.flag = 2;
            } else if (bestVal >= beta) {
                ttEntry.flag = 1;
            } else {
                ttEntry.flag = 0;
            }
        }
    }

    return bestVal;
}

// ---------------------------------------------------------------------------
// chooseMove with Multi-Core Lazy SMP
// ---------------------------------------------------------------------------

std::string OctiBotEngine::chooseMove(const OctiBoard &board,
                                      BotSearchStats *outStats)
{
    _liveDepthReached.store(0, std::memory_order_relaxed);
    _liveNodesVisited.store(0, std::memory_order_relaxed);
    _liveBestScore.store(0, std::memory_order_relaxed);

    if (outStats) {
        *outStats = BotSearchStats();
    }

    const OctiGame *srcGame = board.getGame();
    if (!srcGame || !srcGame->isGameStarted() || srcGame->isGameOver() ||
        srcGame->getTurn() == PLAYER_UNDEF) {
        return "";
    }

    long startMs = steadyClockMs();
    long deadlineMs = startMs + _timeBudgetMs;

    const std::string rootFen = board.toPositionString();
    OctiBoard scratchBoard;
    scratchBoard.suppressBoardUpdateNotification(true);
    scratchBoard.fromPositionString(rootFen);
    OctiGame *game = scratchBoard.getGame();
    if (!game) {
        return "";
    }
    game->setSearchMode(true);

    std::vector<OctiMove> legalMoves = game->generateLegalMoves();
    if (legalMoves.empty()) {
        return "";
    }

    OctiPlayer rootTurn = game->getTurn();
    OctiGameMode mode = scratchBoard.getGameMode();
    std::uint64_t nodesVisited = 0;

    // 1. Check for immediate 1-ply victory across ALL legal moves first
    OctiGameSnapshot rootSnap = game->captureSnapshot();
    for (const auto &mv : legalMoves) {
        ++nodesVisited;
        _liveNodesVisited.store(nodesVisited, std::memory_order_relaxed);
        try {
            game->move(mv);
            if (game->isGameOver() &&
                areAlliedPlayers(mode, game->getWinner(), rootTurn)) {
                game->restoreSnapshot(rootSnap);
                _liveDepthReached.store(1, std::memory_order_relaxed);
                _liveBestScore.store(WIN_SCORE, std::memory_order_relaxed);
                if (outStats) {
                    outStats->depthReached = 1;
                    outStats->nodesVisited = nodesVisited;
                    outStats->bestScore = WIN_SCORE;
                    outStats->elapsedMs = std::max(0L, steadyClockMs() - startMs);
                }
                return mv.asString();
            }
        } catch (...) {
        }
        game->restoreSnapshot(rootSnap);
    }

    // 2. Probe precomputed Zobrist opening playbook if enabled
    if (_profile.useOpeningBook) {
        std::string bookMove;
        int bookScore = 0;
        int bookDepth = 0;
        if (probeOpeningBook(scratchBoard, bookMove, &bookScore, &bookDepth)) {
            _liveDepthReached.store(bookDepth, std::memory_order_relaxed);
            _liveNodesVisited.store(1, std::memory_order_relaxed);
            _liveBestScore.store(bookScore, std::memory_order_relaxed);
            if (outStats) {
                outStats->depthReached = bookDepth;
                outStats->nodesVisited = 1;
                outStats->bestScore = bookScore;
                outStats->elapsedMs = std::max(0L, steadyClockMs() - startMs);
            }
            return bookMove;
        }
    }

    std::size_t rootCap = (_profile.rootBranchCap > 0)
        ? static_cast<std::size_t>(_profile.rootBranchCap)
        : 40;
    std::vector<OctiMove> rootCandidates = orderAndCapMoves(
        scratchBoard, legalMoves, rootTurn, "", rootCap, _profile);

    std::string bestMoveStr = rootCandidates[0].asString();
    int bestScore = -INF_SCORE;
    int maxTargetDepth = _profile.maxSearchDepth;
    int completedDepth = 0;

    // Lazy SMP: spawn helper threads if budget >= 500ms and lazySmpThreads > 1
    _stopSearch.store(false, std::memory_order_relaxed);
    std::vector<std::thread> helpers;

    if (_timeBudgetMs >= 500 && _profile.lazySmpThreads > 1) {
        unsigned int hw = std::thread::hardware_concurrency();
        int numThreads = std::min(_profile.lazySmpThreads, static_cast<int>(hw > 0 ? hw : 2));
        for (int t = 1; t < numThreads; ++t) {
            int depthOffset = (t % 2 == 1) ? 1 : 2;
            helpers.emplace_back([this, rootFen, deadlineMs, depthOffset, maxTargetDepth, rootTurn]() {
                OctiBoard threadBoard;
                threadBoard.suppressBoardUpdateNotification(true);
                threadBoard.fromPositionString(rootFen);
                OctiGame *tGame = threadBoard.getGame();
                if (!tGame) return;
                tGame->setSearchMode(true);
                std::uint64_t tNodes = 0;

                for (int d = 1 + depthOffset;
                     d <= maxTargetDepth && !_stopSearch.load(std::memory_order_relaxed);
                     ++d) {
                    if (steadyClockMs() >= deadlineMs ||
                        _stopSearch.load(std::memory_order_relaxed)) {
                        break;
                    }
                    bool tTimedOut = false;
                    negamax(threadBoard, d, 0, -INF_SCORE, INF_SCORE,
                            rootTurn, tNodes, deadlineMs, tTimedOut);
                    if (tTimedOut || steadyClockMs() >= deadlineMs) break;
                }
            });
        }
    }

    for (int d = 1; d <= maxTargetDepth; ++d) {
        bool timedOut = false;
        int alpha = -INF_SCORE;
        int beta = INF_SCORE;
        std::string iterBestMove = bestMoveStr;
        int iterBestScore = -INF_SCORE;

        for (const auto &mv : rootCandidates) {
            try {
                game->move(mv);
            } catch (...) {
                game->restoreSnapshot(rootSnap);
                continue;
            }

            int score = 0;
            if (game->isGameOver()) {
                OctiPlayer w = game->getWinner();
                score = areAlliedPlayers(mode, w, rootTurn)
                    ? (WIN_SCORE - 1)
                    : (-WIN_SCORE + 1);
            } else {
                OctiPlayer nextTurn = game->getTurn();
                bool sameTeamNext = areAlliedPlayers(mode, rootTurn, nextTurn);
                if (sameTeamNext) {
                    score = negamax(scratchBoard, d - 1, 1,
                                    alpha, beta, rootTurn, nodesVisited,
                                    deadlineMs, timedOut);
                } else {
                    OctiPlayer oppPerspective = (nextTurn != PLAYER_UNDEF) ? nextTurn :
                        ((rootTurn == PLAYER_A || rootTurn == PLAYER_C) ? PLAYER_B : PLAYER_A);
                    score = -negamax(scratchBoard, d - 1, 1,
                                     -beta, -alpha, oppPerspective, nodesVisited,
                                     deadlineMs, timedOut);
                }
            }

            game->restoreSnapshot(rootSnap);
            _liveNodesVisited.store(nodesVisited, std::memory_order_relaxed);

            if (timedOut && d > 1) {
                break;
            }

            if (score > alpha) {
                if (_profile.rootScoreJitter > 0 && std::abs(score) < WIN_SCORE / 2) {
                    int half = _profile.rootScoreJitter;
                    int jitter = static_cast<int>(nextRandom() % (2 * half + 1)) - half;
                    score += jitter;
                }
                iterBestScore = score;
                iterBestMove = mv.asString();
                alpha = score;
            }
        }

        if (!timedOut || d == 1) {
            bestMoveStr = iterBestMove;
            bestScore = iterBestScore;
            completedDepth = d;
            _liveDepthReached.store(completedDepth, std::memory_order_relaxed);
            _liveBestScore.store(bestScore, std::memory_order_relaxed);
            if (outStats) {
                outStats->depthReached = completedDepth;
                outStats->nodesVisited = nodesVisited;
                outStats->bestScore = bestScore;
                outStats->elapsedMs = std::max(0L, steadyClockMs() - startMs);
            }
            auto it = std::find_if(rootCandidates.begin(), rootCandidates.end(),
                                   [&](const OctiMove &m) {
                                       return m.asString() == bestMoveStr;
                                   });
            if (it != rootCandidates.end() && it != rootCandidates.begin()) {
                std::rotate(rootCandidates.begin(), it, it + 1);
            }
        }

        if (timedOut ||
            (deadlineMs < std::numeric_limits<long>::max() &&
             steadyClockMs() >= deadlineMs) ||
            std::abs(bestScore) >= WIN_SCORE - 100) {
            break;
        }
    }

    _stopSearch.store(true, std::memory_order_relaxed);
    for (auto &hTh : helpers) {
        if (hTh.joinable()) {
            hTh.join();
        }
    }

    _liveDepthReached.store(std::max(1, completedDepth), std::memory_order_relaxed);
    _liveNodesVisited.store(nodesVisited, std::memory_order_relaxed);
    _liveBestScore.store(bestScore, std::memory_order_relaxed);

    if (outStats) {
        outStats->depthReached = std::max(1, completedDepth);
        outStats->nodesVisited = nodesVisited;
        outStats->bestScore = bestScore;
        outStats->elapsedMs = std::max(0L, steadyClockMs() - startMs);
    }

    return bestMoveStr;
}

std::vector<BookMoveScore> OctiBotEngine::searchTopMovesForBook(
    const OctiBoard &board,
    int searchDepth,
    int timeBudgetMs,
    std::size_t topK,
    BotSearchStats *outStats)
{
    _liveDepthReached.store(0, std::memory_order_relaxed);
    _liveNodesVisited.store(0, std::memory_order_relaxed);
    _liveBestScore.store(0, std::memory_order_relaxed);

    if (outStats) {
        *outStats = BotSearchStats();
    }

    if (topK == 0) {
        return {};
    }

    const OctiGame *srcGame = board.getGame();
    if (!srcGame || !srcGame->isGameStarted() || srcGame->isGameOver() ||
        srcGame->getTurn() == PLAYER_UNDEF) {
        return {};
    }

    BotTuningProfile savedProfile = _profile;
    int savedBudget = _timeBudgetMs;
    bool savedCustom = _customTimeBudget;

    struct ProfileRestorer {
        OctiBotEngine *self;
        BotTuningProfile prof;
        int budget;
        bool custom;
        ~ProfileRestorer() {
            self->_profile = prof;
            self->_timeBudgetMs = budget;
            self->_customTimeBudget = custom;
        }
    } restorer{this, savedProfile, savedBudget, savedCustom};

    int maxTargetDepth = std::max(1, searchDepth);
    int effBudgetMs = (timeBudgetMs > 0) ? std::max(10, timeBudgetMs) : 0;
    _profile.useOpeningBook = false;
    _profile.rootScoreJitter = 0;
    _profile.maxSearchDepth = maxTargetDepth;
    _profile.timeBudgetMs = effBudgetMs;
    _timeBudgetMs = effBudgetMs;

    long startMs = steadyClockMs();
    long deadlineMs = (timeBudgetMs > 0)
        ? (startMs + effBudgetMs)
        : std::numeric_limits<long>::max();

    const std::string rootFen = board.toPositionString();
    OctiBoard scratchBoard;
    scratchBoard.suppressBoardUpdateNotification(true);
    scratchBoard.fromPositionString(rootFen);
    OctiGame *game = scratchBoard.getGame();
    if (!game) {
        return {};
    }
    game->setSearchMode(true);

    std::vector<OctiMove> legalMoves = game->generateLegalMoves();
    if (legalMoves.empty()) {
        return {};
    }

    OctiPlayer rootTurn = game->getTurn();
    OctiGameMode mode = scratchBoard.getGameMode();
    std::uint64_t nodesVisited = 0;

    std::size_t rootCap = std::max<std::size_t>(
        topK,
        (_profile.rootBranchCap > 0)
            ? static_cast<std::size_t>(_profile.rootBranchCap)
            : 40);
    std::vector<OctiMove> rootCandidates = orderAndCapMoves(
        scratchBoard, legalMoves, rootTurn, "", rootCap, _profile);
    if (rootCandidates.empty()) {
        return {};
    }

    OctiGameSnapshot rootSnap = game->captureSnapshot();

    auto clampScore16 = [](int s) -> std::int16_t {
        if (s > 32000) return 32000;
        if (s < -32000) return -32000;
        return static_cast<std::int16_t>(s);
    };

    std::vector<BookMoveScore> bestResults;
    std::size_t targetCount = std::min(topK, rootCandidates.size());
    bestResults.reserve(targetCount);
    for (std::size_t i = 0; i < targetCount; ++i) {
        bestResults.push_back({rootCandidates[i].asString(), 0});
    }

    int completedDepth = 0;
    int bestScore = -INF_SCORE;

    _stopSearch.store(false, std::memory_order_relaxed);
    std::atomic<std::uint64_t> helperNodesVisited{0};
    std::vector<std::thread> helpers;

    if ((timeBudgetMs <= 0 || effBudgetMs >= 200) && _profile.lazySmpThreads > 1) {
        unsigned int hw = std::thread::hardware_concurrency();
        int numThreads = std::min(_profile.lazySmpThreads,
                                  static_cast<int>(hw > 0 ? hw : 2));
        const std::vector<OctiMove> initialCandidates = rootCandidates;
        for (int t = 1; t < numThreads; ++t) {
            helpers.emplace_back([this, rootFen, initialCandidates, deadlineMs,
                                  t, maxTargetDepth, rootTurn, mode,
                                  &helperNodesVisited]() {
                OctiBoard threadBoard;
                threadBoard.suppressBoardUpdateNotification(true);
                threadBoard.fromPositionString(rootFen);
                OctiGame *tGame = threadBoard.getGame();
                if (!tGame) return;
                tGame->setSearchMode(true);
                OctiGameSnapshot tRootSnap = tGame->captureSnapshot();
                std::uint64_t tNodes = 0;

                int startD = std::min(maxTargetDepth, 2 + ((t - 1) % 2));
                for (int d = startD;
                     d <= maxTargetDepth &&
                     !_stopSearch.load(std::memory_order_relaxed);
                     ++d) {
                    for (std::size_t s = 0;
                         s < initialCandidates.size() &&
                         !_stopSearch.load(std::memory_order_relaxed);
                         ++s) {
                        if (deadlineMs < std::numeric_limits<long>::max() &&
                            steadyClockMs() >= deadlineMs) {
                            break;
                        }
                        const OctiMove &mv =
                            initialCandidates[(static_cast<std::size_t>(t) + s) %
                                              initialCandidates.size()];
                        try {
                            tGame->move(mv);
                        } catch (...) {
                            tGame->restoreSnapshot(tRootSnap);
                            continue;
                        }
                        bool tTimedOut = false;
                        if (!tGame->isGameOver() && d > 1) {
                            OctiPlayer nextTurn = tGame->getTurn();
                            bool sameTeamNext =
                                areAlliedPlayers(mode, rootTurn, nextTurn);
                            OctiPlayer persp = sameTeamNext
                                ? rootTurn
                                : ((nextTurn != PLAYER_UNDEF)
                                       ? nextTurn
                                       : ((rootTurn == PLAYER_A ||
                                           rootTurn == PLAYER_C)
                                              ? PLAYER_B
                                              : PLAYER_A));
                            negamax(threadBoard, d - 1, 1, -INF_SCORE, INF_SCORE,
                                    persp, tNodes, deadlineMs, tTimedOut);
                        }
                        tGame->restoreSnapshot(tRootSnap);
                        if (tTimedOut) break;
                    }
                }
                helperNodesVisited.fetch_add(tNodes, std::memory_order_relaxed);
            });
        }
    }

    for (int d = 1; d <= maxTargetDepth; ++d) {
        bool timedOut = false;
        int kthAlpha = -INF_SCORE;
        int beta = INF_SCORE;

        struct ScoredCand {
            OctiMove move;
            std::string moveStr;
            int score;
        };
        std::vector<ScoredCand> iterScored;
        iterScored.reserve(rootCandidates.size());
        std::vector<int> topScoresSoFar;
        topScoresSoFar.reserve(targetCount + 1);

        for (const auto &mv : rootCandidates) {
            try {
                game->move(mv);
            } catch (...) {
                game->restoreSnapshot(rootSnap);
                continue;
            }

            int score = 0;
            if (game->isGameOver()) {
                OctiPlayer w = game->getWinner();
                score = areAlliedPlayers(mode, w, rootTurn)
                    ? (WIN_SCORE - 1)
                    : (-WIN_SCORE + 1);
            } else {
                OctiPlayer nextTurn = game->getTurn();
                bool sameTeamNext = areAlliedPlayers(mode, rootTurn, nextTurn);
                bool useNullWindow = (d >= 3 &&
                                      topScoresSoFar.size() == targetCount &&
                                      kthAlpha > -INF_SCORE / 2 &&
                                      kthAlpha < INF_SCORE / 2);
                if (sameTeamNext) {
                    if (useNullWindow) {
                        score = negamax(scratchBoard, d - 1, 1,
                                        kthAlpha, kthAlpha + 1, rootTurn,
                                        nodesVisited, deadlineMs, timedOut);
                        if (!timedOut && score > kthAlpha) {
                            score = negamax(scratchBoard, d - 1, 1,
                                            kthAlpha, beta, rootTurn,
                                            nodesVisited, deadlineMs, timedOut);
                        }
                    } else {
                        score = negamax(scratchBoard, d - 1, 1,
                                        kthAlpha, beta, rootTurn, nodesVisited,
                                        deadlineMs, timedOut);
                    }
                } else {
                    OctiPlayer oppPerspective = (nextTurn != PLAYER_UNDEF) ? nextTurn :
                        ((rootTurn == PLAYER_A || rootTurn == PLAYER_C) ? PLAYER_B : PLAYER_A);
                    if (useNullWindow) {
                        score = -negamax(scratchBoard, d - 1, 1,
                                         -(kthAlpha + 1), -kthAlpha, oppPerspective,
                                         nodesVisited, deadlineMs, timedOut);
                        if (!timedOut && score > kthAlpha) {
                            score = -negamax(scratchBoard, d - 1, 1,
                                             -beta, -kthAlpha, oppPerspective,
                                             nodesVisited, deadlineMs, timedOut);
                        }
                    } else {
                        score = -negamax(scratchBoard, d - 1, 1,
                                         -beta, -kthAlpha, oppPerspective,
                                         nodesVisited, deadlineMs, timedOut);
                    }
                }
            }

            game->restoreSnapshot(rootSnap);
            _liveNodesVisited.store(nodesVisited, std::memory_order_relaxed);

            if (timedOut && d > 1) {
                break;
            }

            iterScored.push_back({mv, mv.asString(), score});

            auto insIt = std::lower_bound(topScoresSoFar.begin(), topScoresSoFar.end(),
                                          score, std::greater<int>());
            topScoresSoFar.insert(insIt, score);
            if (topScoresSoFar.size() > targetCount) {
                topScoresSoFar.resize(targetCount);
            }
            if (topScoresSoFar.size() == targetCount) {
                kthAlpha = topScoresSoFar.back();
            }
        }

        if ((!timedOut || d == 1) && !iterScored.empty()) {
            std::stable_sort(iterScored.begin(), iterScored.end(),
                             [](const ScoredCand &a, const ScoredCand &b) {
                                 return a.score > b.score;
                             });

            rootCandidates.clear();
            for (const auto &sc : iterScored) {
                rootCandidates.push_back(sc.move);
            }

            bestResults.clear();
            std::size_t count = std::min(targetCount, iterScored.size());
            for (std::size_t i = 0; i < count; ++i) {
                bestResults.push_back({iterScored[i].moveStr,
                                       clampScore16(iterScored[i].score)});
            }

            bestScore = iterScored[0].score;
            completedDepth = d;
            _liveDepthReached.store(completedDepth, std::memory_order_relaxed);
            _liveBestScore.store(bestScore, std::memory_order_relaxed);
            if (outStats) {
                outStats->depthReached = completedDepth;
                outStats->nodesVisited = nodesVisited;
                outStats->bestScore = bestScore;
                outStats->elapsedMs = std::max(0L, steadyClockMs() - startMs);
            }
        }

        if (timedOut ||
            (deadlineMs < std::numeric_limits<long>::max() &&
             steadyClockMs() >= deadlineMs) ||
            std::abs(bestScore) >= WIN_SCORE - 100) {
            break;
        }
    }

    _stopSearch.store(true, std::memory_order_relaxed);
    for (auto &hTh : helpers) {
        if (hTh.joinable()) {
            hTh.join();
        }
    }
    nodesVisited += helperNodesVisited.load(std::memory_order_relaxed);

    _liveDepthReached.store(std::max(1, completedDepth), std::memory_order_relaxed);
    _liveNodesVisited.store(nodesVisited, std::memory_order_relaxed);
    _liveBestScore.store(bestScore, std::memory_order_relaxed);

    if (outStats) {
        outStats->depthReached = std::max(1, completedDepth);
        outStats->nodesVisited = nodesVisited;
        outStats->bestScore = bestScore;
        outStats->elapsedMs = std::max(0L, steadyClockMs() - startMs);
    }

    return bestResults;
}

} // namespace octi

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
