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
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>
#include <vector>

namespace octi {

namespace {

constexpr int WIN_SCORE = 1000000;
constexpr int INF_SCORE = 2000000;
constexpr int DEFAULT_TT_SIZE_LOG2 = 18; // 256K entries (1 << 18)
constexpr int MIN_TT_SIZE_LOG2 = 12;     // 4K entries
constexpr int MAX_TT_SIZE_LOG2 = 26;     // 64M entries
constexpr std::size_t TT_MUTEX_COUNT = 16384;
constexpr int MAX_SEARCH_PLY = 128;
constexpr std::size_t HISTORY_SIZE = 4096;
constexpr std::size_t HISTORY_MASK = HISTORY_SIZE - 1;
constexpr int MAX_HISTORY_VAL = 4096;

constexpr int clampTtSizeLog2(int log2Val) noexcept
{
    return (log2Val < MIN_TT_SIZE_LOG2)
        ? MIN_TT_SIZE_LOG2
        : ((log2Val > MAX_TT_SIZE_LOG2) ? MAX_TT_SIZE_LOG2 : log2Val);
}

void accumulateSearchDiagnostics(BotSearchDiagnostics &dst,
                                 const BotSearchDiagnostics &src) noexcept
{
    dst.pvsReSearches += src.pvsReSearches;
    dst.aspirationReSearches += src.aspirationReSearches;
    dst.aspirationFailLows += src.aspirationFailLows;
    dst.aspirationFailHighs += src.aspirationFailHighs;
    dst.killerUpdates += src.killerUpdates;
    dst.killerHits += src.killerHits;
    dst.historyUpdates += src.historyUpdates;
    dst.lmrReductions += src.lmrReductions;
    dst.lmrReSearches += src.lmrReSearches;
    dst.nullMoveTries += src.nullMoveTries;
    dst.nullMoveVerifications += src.nullMoveVerifications;
    dst.nullMoveCutoffs += src.nullMoveCutoffs;
    dst.qsearchTtProbes += src.qsearchTtProbes;
    dst.qsearchTtHits += src.qsearchTtHits;
}

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

} // namespace

// ---------------------------------------------------------------------------
// Dominated Move Filtering
// ---------------------------------------------------------------------------

namespace {

inline std::uint64_t mixPathKey64(std::uint64_t z)
{
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

bool sameMovePathIgnoringCapture(const std::vector<OctiMoveOp> &a,
                                 const std::vector<OctiMoveOp> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const OctiMoveOp &oa = a[i];
        const OctiMoveOp &ob = b[i];
        if (oa.x0 != ob.x0 ||
            oa.y0 != ob.y0 ||
            (oa.pod.rawConfig() & 0xFFFFu) != (ob.pod.rawConfig() & 0xFFFFu) ||
            oa.addPod != ob.addPod ||
            oa.liberate != ob.liberate ||
            oa.addProngDir != ob.addProngDir ||
            oa.rmProngDir != ob.rmProngDir ||
            oa.superProng != ob.superProng ||
            oa.isSuperProngBonusMove != ob.isSuperProngBonusMove ||
            oa.hops.size() != ob.hops.size()) {
            return false;
        }
        for (std::size_t h = 0; h < oa.hops.size(); ++h) {
            if (oa.hops[h].x != ob.hops[h].x ||
                oa.hops[h].y != ob.hops[h].y) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

std::vector<OctiMove> filterDominatedMoves(const OctiBoard &board,
                                          OctiPlayer turn,
                                          const std::vector<OctiMove> &moves)
{
    if (moves.size() <= 1) {
        return moves;
    }

    int boardW = board.getBoardWidth();
    int boardH = board.getBoardHeight();
    bool edgeless = board.getGame() ? board.getGame()->useEdgelessBoard() : false;
    int freeProngs = board.getGame() ? board.getGame()->getFreeProngs(turn) : 0;

    struct MoveDomInfo {
        std::uint64_t pathKey = 0;
        std::uint32_t friendlyCapMask = 0;
        std::uint32_t enemyCapMask = 0;
        bool hasAnyHops = false;
        bool pointlessSelfCap = false;
    };

    MoveDomInfo stackInfos[128];
    std::vector<MoveDomInfo> heapInfos;
    MoveDomInfo *infos = stackInfos;
    if (moves.size() > 128) {
        heapInfos.resize(moves.size());
        infos = heapInfos.data();
    }

    bool anyPointlessSelfCap = false;
    bool anyEnemyCap = false;
    std::size_t validHopIndicesStack[128];
    std::vector<std::size_t> validHopIndicesHeap;
    std::size_t *validHopIndices = validHopIndicesStack;
    if (moves.size() > 128) {
        validHopIndicesHeap.resize(moves.size());
        validHopIndices = validHopIndicesHeap.data();
    }
    std::size_t numValidHopMoves = 0;

    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto &ops = moves[i].getOps();
        MoveDomInfo info;

        // Quick skip for single-op non-hop moves (pod add, prong add, reposition)
        if (ops.size() == 1 && ops[0].hops.empty()) {
            infos[i] = info;
            continue;
        }

        std::uint64_t h = mixPathKey64(static_cast<std::uint64_t>(ops.size()));
        std::size_t globalHopIdx = 0;
        bool badSelfCap = false;

        for (const auto &op : ops) {
            h = mixPathKey64(h ^ static_cast<std::uint64_t>(op.x0 & 0xFF));
            h = mixPathKey64(h ^ (static_cast<std::uint64_t>(op.y0 & 0xFF) << 8));
            h = mixPathKey64(h ^ static_cast<std::uint64_t>(op.pod.rawConfig() & 0xFFFFu));
            h = mixPathKey64(h ^ ((op.addPod ? 1ULL : 0ULL) |
                                  ((op.liberate ? 1ULL : 0ULL) << 1) |
                                  ((static_cast<std::uint64_t>(op.addProngDir) & 0xFFULL) << 2) |
                                  ((static_cast<std::uint64_t>(op.rmProngDir) & 0xFFULL) << 10) |
                                  ((op.superProng ? 1ULL : 0ULL) << 18) |
                                  ((op.isSuperProngBonusMove ? 1ULL : 0ULL) << 19)));
            h = mixPathKey64(h ^ static_cast<std::uint64_t>(op.hops.size()));

            int cx = op.x0;
            int cy = op.y0;
            for (std::size_t hIdx = 0; hIdx < op.hops.size(); ++hIdx) {
                info.hasAnyHops = true;
                const auto &hopItem = op.hops[hIdx];
                h = mixPathKey64(h ^ (static_cast<std::uint64_t>(hopItem.x & 0xFF) |
                                      (static_cast<std::uint64_t>(hopItem.y & 0xFF) << 8)));

                int dx = hopItem.x - cx;
                int dy = hopItem.y - cy;
                if (edgeless) {
                    for (int d = -2; d <= 2; d++) {
                        if (((cx + d) % boardW + boardW) % boardW == hopItem.x) { dx = d; break; }
                    }
                    for (int d = -2; d <= 2; d++) {
                        if (((cy + d) % boardH + boardH) % boardH == hopItem.y) { dy = d; break; }
                    }
                }
                bool isJump = (std::abs(dx) == 2 || std::abs(dy) == 2);
                if (isJump && hopItem.capture) {
                    int mx = edgeless ? (((cx + dx / 2) % boardW + boardW) % boardW) : (cx + dx / 2);
                    int my = edgeless ? (((cy + dy / 2) % boardH + boardH) % boardH) : (cy + dy / 2);
                    if (board.isValidCoord(mx, my)) {
                        const OctiSquare &midSq = board.squareAt(mx, my);
                        if (midSq.hasEnemyOf(turn)) {
                            info.enemyCapMask |= (1U << (globalHopIdx & 31U));
                        } else {
                            if (freeProngs > 0) {
                                int prongsOnMid = 0;
                                for (int pi = 0; pi < midSq.getNumPods(); ++pi) {
                                    prongsOnMid += midSq.getPodAt(pi).getNumProngs();
                                }
                                if (prongsOnMid == 0) {
                                    badSelfCap = true;
                                    break;
                                }
                            }
                            info.friendlyCapMask |= (1U << (globalHopIdx & 31U));
                        }
                    }
                }
                cx = hopItem.x;
                cy = hopItem.y;
                ++globalHopIdx;
            }
            if (badSelfCap) {
                break;
            }
        }

        if (badSelfCap) {
            info.pointlessSelfCap = true;
            anyPointlessSelfCap = true;
            infos[i] = info;
            continue;
        }

        info.pathKey = h;
        infos[i] = info;
        if (info.hasAnyHops) {
            validHopIndices[numValidHopMoves++] = i;
            if (info.enemyCapMask != 0) {
                anyEnemyCap = true;
            }
        }
    }

    if (!anyPointlessSelfCap && !anyEnemyCap) {
        return moves;
    }

    std::vector<OctiMove> filtered;
    filtered.reserve(moves.size());
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const MoveDomInfo &info = infos[i];
        if (info.pointlessSelfCap) {
            continue;
        }
        if (anyEnemyCap && info.hasAnyHops) {
            bool isDominated = false;
            for (std::size_t k = 0; k < numValidHopMoves; ++k) {
                std::size_t j = validHopIndices[k];
                const MoveDomInfo &other = infos[j];
                if (other.enemyCapMask != info.enemyCapMask &&
                    (other.enemyCapMask & info.enemyCapMask) == info.enemyCapMask &&
                    other.pathKey == info.pathKey &&
                    other.friendlyCapMask == info.friendlyCapMask &&
                    sameMovePathIgnoringCapture(moves[i].getOps(), moves[j].getOps())) {
                    isDominated = true;
                    break;
                }
            }
            if (isDominated) {
                continue;
            }
        }
        filtered.push_back(moves[i]);
    }

    return filtered.empty() ? moves : filtered;
}

std::vector<OctiMove> filterDominatedMoves(const OctiBoard &board,
                                          const std::vector<OctiMove> &moves)
{
    OctiPlayer turn = board.getGame() ? board.getGame()->getTurn() : PLAYER_A;
    return filterDominatedMoves(board, turn, moves);
}

std::vector<OctiMove> filterDominatedMoves(const std::vector<OctiMove> &moves)
{
    return moves;
}

namespace {

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

static bool doesStepVacateEnemyCaptureLanding(const OctiBoard &board,
                                              int x0, int y0,
                                              int destX, int destY,
                                              OctiPlayer turn)
{
    if (!board.isValidCoord(x0, y0) || !board.isValidCoord(destX, destY)) {
        return false;
    }
    const OctiSquare &startSq = board.squareAt(x0, y0);
    if (startSq.getNumPods() > 1) {
        return false;
    }

    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    OctiGameMode mode = board.getGameMode();
    bool edgeless = board.getGame() ? board.getGame()->useEdgelessBoard() : false;

    for (int d = 0; d < DIR_MAX; ++d) {
        OctiDir dir = static_cast<OctiDir>(d);
        int dx = dirDeltaX(dir);
        int dy = dirDeltaY(dir);

        int landX = edgeless ? (((destX + dx) % w + w) % w) : (destX + dx);
        int landY = edgeless ? (((destY + dy) % h + h) % h) : (destY + dy);
        if (landX == x0 && landY == y0) {
            int ex = edgeless ? (((destX - dx) % w + w) % w) : (destX - dx);
            int ey = edgeless ? (((destY - dy) % h + h) % h) : (destY - dy);
            if (board.isValidCoord(ex, ey)) {
                const OctiSquare &esq = board.squareAt(ex, ey);
                if (esq.hasEnemyOf(turn)) {
                    for (int pi = 0; pi < esq.getNumPods(); ++pi) {
                        const OctiPod &epod = esq.getPodAt(pi);
                        if (!areAlliedPlayers(mode, podTypeToPlayer(epod.getType()), turn) &&
                            epod.prongAt(dir) != NO_PRONG) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

static bool isSquareThreatenedByEnemyProng(const OctiBoard &board,
                                           int x, int y,
                                           OctiGameMode mode,
                                           OctiPlayer allyPlayer)
{
    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    bool edgeless = board.getGame() ? board.getGame()->useEdgelessBoard() : false;
    for (int d = 0; d < DIR_MAX; ++d) {
        OctiDir dir = static_cast<OctiDir>(d);
        int dx = dirDeltaX(dir);
        int dy = dirDeltaY(dir);
        int ex = edgeless ? (((x - dx) % w + w) % w) : (x - dx);
        int ey = edgeless ? (((y - dy) % h + h) % h) : (y - dy);
        if (board.isValidCoord(ex, ey)) {
            const OctiSquare &esq = board.squareAt(ex, ey);
            if (esq.hasEnemyOf(allyPlayer)) {
                for (int pi = 0; pi < esq.getNumPods(); ++pi) {
                    const OctiPod &epod = esq.getPodAt(pi);
                    if (!areAlliedPlayers(mode, podTypeToPlayer(epod.getType()), allyPlayer) &&
                        epod.prongAt(dir) != NO_PRONG) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static bool canEnemyCaptureSquareNextTurn(const OctiBoard &board,
                                          int x0, int y0,
                                          int destX, int destY,
                                          OctiGameMode mode,
                                          OctiPlayer turn,
                                          const std::uint64_t enemyThreatMask[2])
{
    if (!board.isValidCoord(x0, y0)) {
        return false;
    }

    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    bool edgeless = board.getGame() ? board.getGame()->useEdgelessBoard() : false;

    // If (x0, y0) is currently threatened in an enemy jump-capture lane,
    // evacuating the threatened pod must NEVER be penalized.
    int startBitIdx = y0 * 9 + x0;
    if (enemyThreatMask != nullptr) {
        int startWord = (startBitIdx >> 6) & 1;
        std::uint64_t startBit = 1ULL << (startBitIdx & 63);
        if ((enemyThreatMask[startWord] & startBit) != 0ULL) {
            return false;
        }
    }

    // Direct check across all 8 directions whether an enemy pod can jump over (x0, y0)
    // and capture the pod if it stays on (x0, y0).
    for (int d = 0; d < DIR_MAX; ++d) {
        OctiDir dir = static_cast<OctiDir>(d);
        int dx = dirDeltaX(dir);
        int dy = dirDeltaY(dir);
        int ex = edgeless ? (((x0 - dx) % w + w) % w) : (x0 - dx);
        int ey = edgeless ? (((y0 - dy) % h + h) % h) : (y0 - dy);
        int lx = edgeless ? (((x0 + dx) % w + w) % w) : (x0 + dx);
        int ly = edgeless ? (((y0 + dy) % h + h) % h) : (y0 + dy);
        if (board.isValidCoord(ex, ey) && board.isValidCoord(lx, ly)) {
            const OctiSquare &esq = board.squareAt(ex, ey);
            const OctiSquare &lsq = board.squareAt(lx, ly);
            bool landEmpty = lsq.isEmpty() && !(lx == destX && ly == destY);
            if (landEmpty && esq.hasEnemyOf(turn)) {
                for (int pi = 0; pi < esq.getNumPods(); ++pi) {
                    const OctiPod &epod = esq.getPodAt(pi);
                    if (!areAlliedPlayers(mode, podTypeToPlayer(epod.getType()), turn) &&
                        epod.prongAt(dir) != NO_PRONG) {
                        return false;
                    }
                }
            }
        }
    }

    // Next, check whether any enemy pod can actually land on (x0, y0) in 1 move if (x0, y0) is vacated:
    for (int d = 0; d < DIR_MAX; ++d) {
        OctiDir dir = static_cast<OctiDir>(d);
        int dx = dirDeltaX(dir);
        int dy = dirDeltaY(dir);

        // 1-square step: enemy at (adjX, adjY) steps into (x0, y0)
        int adjX = edgeless ? (((x0 - dx) % w + w) % w) : (x0 - dx);
        int adjY = edgeless ? (((y0 - dy) % h + h) % h) : (y0 - dy);
        if (board.isValidCoord(adjX, adjY) && !(adjX == destX && adjY == destY)) {
            const OctiSquare &asq = board.squareAt(adjX, adjY);
            if (asq.hasEnemyOf(turn)) {
                for (int pi = 0; pi < asq.getNumPods(); ++pi) {
                    const OctiPod &epod = asq.getPodAt(pi);
                    if (!areAlliedPlayers(mode, podTypeToPlayer(epod.getType()), turn) &&
                        epod.prongAt(dir) != NO_PRONG) {
                        return true;
                    }
                }
            }
        }

        // 2-square jump: enemy at (jmpX, jmpY) jumps over (adjX, adjY) into (x0, y0)
        int jmpX = edgeless ? (((x0 - 2 * dx) % w + w) % w) : (x0 - 2 * dx);
        int jmpY = edgeless ? (((y0 - 2 * dy) % h + h) % h) : (y0 - 2 * dy);
        bool midOccupied = (adjX == destX && adjY == destY) ||
                           (board.isValidCoord(adjX, adjY) &&
                            !board.squareAt(adjX, adjY).isEmpty() &&
                            !(adjX == x0 && adjY == y0));
        if (midOccupied && board.isValidCoord(jmpX, jmpY) && !(jmpX == destX && jmpY == destY)) {
            const OctiSquare &jsq = board.squareAt(jmpX, jmpY);
            if (jsq.hasEnemyOf(turn)) {
                for (int pi = 0; pi < jsq.getNumPods(); ++pi) {
                    const OctiPod &epod = jsq.getPodAt(pi);
                    if (!areAlliedPlayers(mode, podTypeToPlayer(epod.getType()), turn) &&
                        epod.prongAt(dir) != NO_PRONG) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

int prongDirectionBonus(const OctiBoard &board,
                        OctiGameMode mode,
                        OctiPlayer seat,
                        OctiDir dir,
                        int primaryBonus,
                        int diagBonus,
                        int sideBonus)
{
    if (mode != GAME_MODE_4) {
        if (seat == PLAYER_A || seat == PLAYER_C) {
            if (dir == DIR_N) {
                return primaryBonus;
            } else if (dir == DIR_NE || dir == DIR_NW) {
                return diagBonus;
            } else if (dir == DIR_E || dir == DIR_W) {
                return sideBonus;
            }
        } else if (seat == PLAYER_B || seat == PLAYER_D) {
            if (dir == DIR_S) {
                return primaryBonus;
            } else if (dir == DIR_SE || dir == DIR_SW) {
                return diagBonus;
            } else if (dir == DIR_E || dir == DIR_W) {
                return sideBonus;
            }
        }
        return 0;
    }

    OctiSquareType homeType = NEUTRAL_SQUARE;
    switch (seat) {
    case PLAYER_A: homeType = A_HOME_SQUARE; break;
    case PLAYER_B: homeType = B_HOME_SQUARE; break;
    case PLAYER_C: homeType = C_HOME_SQUARE; break;
    case PLAYER_D: homeType = D_HOME_SQUARE; break;
    default:       return 0;
    }

    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    int hx = -1;
    int hy = -1;
    for (int y = 0; y < h && hx < 0; ++y) {
        for (int x = 0; x < w; ++x) {
            if (board.squareAt(x, y).getType() == homeType) {
                hx = x;
                hy = y;
                break;
            }
        }
    }
    if (hx < 0 || hy < 0) {
        return 0;
    }

    int cx = w / 2;
    int cy = h / 2;
    int fx = (hx < cx) ? 1 : ((hx > cx) ? -1 : 0);
    int fy = (hy < cy) ? 1 : ((hy > cy) ? -1 : 0);
    int dx = dirDeltaX(dir);
    int dy = dirDeltaY(dir);

    if ((dx == 0 && dy == fy) || (dx == fx && dy == 0)) {
        return primaryBonus;
    }
    if (dx == fx && dy == fy) {
        return diagBonus;
    }
    if (dx != 0 && dy != 0 && (dx == fx || dy == fy)) {
        return sideBonus;
    }
    return 0;
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

    const auto &ops = move.getOps();
    std::size_t regularOps = 0;
    std::size_t preludeOps = 0;
    for (const auto &op : ops) {
        if (op.isSuperProngBonusMove) {
            ++preludeOps;
        } else {
            ++regularOps;
        }
    }

    // Prioritize multi-pod composite moves (multiple regular ops), not superprong preludes
    if (regularOps > 1) {
        score += 8000 + static_cast<int>(regularOps - 1) * 4000;
    }

    // Pre-scan move-wide enemy capture status across all ops (preludes + regular)
    bool moveHasEnemyCapture = false;
    for (const auto &op : ops) {
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
            if ((std::abs(dx) == 2 || std::abs(dy) == 2) && hop.capture) {
                int mx = edgeless ? (((prevX + dx / 2) % w + w) % w) : (prevX + dx / 2);
                int my = edgeless ? (((prevY + dy / 2) % h + h) % h) : (prevY + dy / 2);
                if (!board.isValidCoord(mx, my) || board.squareAt(mx, my).hasEnemyOf(turn)) {
                    moveHasEnemyCapture = true;
                }
            } else if (hop.capture) {
                moveHasEnemyCapture = true;
            }
            prevX = hop.x;
            prevY = hop.y;
        }
    }

    std::uint64_t capturedSqMask[2] = {0ULL, 0ULL};

    for (std::size_t opIdx = 0; opIdx < ops.size(); ++opIdx) {
        const auto &op = ops[opIdx];
        if (!op.hops.empty()) {
            bool isInitialSegmentForPod = true;
            for (std::size_t k = 0; k < opIdx; ++k) {
                if (!ops[k].hops.empty() &&
                    ops[k].hops.back().x == op.x0 &&
                    ops[k].hops.back().y == op.y0) {
                    isInitialSegmentForPod = false;
                    break;
                }
            }

            const OctiHop &lastHop = op.hops.back();
            bool isFinalSegmentForPod = true;
            int chainFinalX = lastHop.x;
            int chainFinalY = lastHop.y;
            for (std::size_t j = opIdx + 1; j < ops.size(); ++j) {
                if (!ops[j].hops.empty() &&
                    ops[j].x0 == chainFinalX &&
                    ops[j].y0 == chainFinalY) {
                    if (j == opIdx + 1 || (chainFinalX == lastHop.x && chainFinalY == lastHop.y)) {
                        isFinalSegmentForPod = false;
                    }
                    chainFinalX = ops[j].hops.back().x;
                    chainFinalY = ops[j].hops.back().y;
                }
            }

            int startDist = minChebyshevToTargets(op.x0, op.y0, enemyTargets, w, h, edgeless);
            int endDist = minChebyshevToTargets(lastHop.x, lastHop.y, enemyTargets, w, h, edgeless);

            score += (startDist - endDist) * 550;

            bool opEnemyCapture = false;
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
                            int midBitIdx = my * 9 + mx;
                            int midWord = (midBitIdx >> 6) & 1;
                            std::uint64_t midBit = 1ULL << (midBitIdx & 63);
                            bool alreadyScoredMid = ((capturedSqMask[midWord] & midBit) != 0ULL);
                            capturedSqMask[midWord] |= midBit;

                            int capturedPodsOnMid = midSq.getNumPods();
                            int capturedProngsOnMid = 0;
                            for (int pi = 0; pi < capturedPodsOnMid; ++pi) {
                                capturedProngsOnMid += midSq.getPodAt(pi).getNumProngs();
                            }
                            if (midSq.hasEnemyOf(turn)) {
                                opEnemyCapture = true;
                                if (!alreadyScoredMid) {
                                    score += profile.moveOrderCaptureBonus * capturedPodsOnMid + 600 * capturedProngsOnMid;
                                }
                            } else if (!alreadyScoredMid) {
                                int freeProngs = board.getGame() ? board.getGame()->getFreeProngs(turn) : 0;
                                if (capturedProngsOnMid == 0 || mode == GAME_MODE_4) {
                                    score -= profile.moveOrderCaptureBonus;
                                } else if (freeProngs <= 2 && capturedProngsOnMid > 0) {
                                    score += 300 * capturedProngsOnMid - 800;
                                } else {
                                    score -= (profile.moveOrderCaptureBonus / 2);
                                }
                            }
                        }
                    } else if (hop.capture) {
                        opEnemyCapture = true;
                        score += profile.moveOrderCaptureBonus;
                    }
                } else if (hop.capture) {
                    opEnemyCapture = true;
                    score += profile.moveOrderCaptureBonus;
                }
                if (!hop.capture) {
                    score += 250;
                }
                prevX = hop.x;
                prevY = hop.y;
            }

            if (op.isSuperProngBonusMove && !opEnemyCapture && endDist >= startDist && isFinalSegmentForPod) {
                score -= 600;
            }

            if (isFinalSegmentForPod) {
                const OctiSquare &destSq = board.squareAt(lastHop.x, lastHop.y);
                if (isEnemyHomeSquareFor(destSq.getType(), mode, turn) &&
                    !destSq.isCapturedByTeamOf(turn)) {
                    if (moveHasEnemyCapture) {
                        score += profile.moveOrderBaseEntryBonus;
                    } else {
                        int effScale = std::max(55, baseThreatScalePct);
                        score += (profile.moveOrderBaseEntryBonus * effScale) / 100;
                    }
                }
                if (destSq.getType() == NEUTRAL_OCTI_SQUARE && destSq.isEmpty()) {
                    score += 3000;
                }

                // For prelude-bearing moves, reward landing on a square that creates an immediate
                // 1-step or 1-jump threat against an uncaptured enemy base
                if (preludeOps > 0 && !isEnemyHomeSquareFor(destSq.getType(), mode, turn)) {
                    bool hasOneStepBaseThreat = false;
                    bool hasOneJumpBaseThreat = false;
                    for (int d = 0; d < DIR_MAX; ++d) {
                        OctiDir dir = static_cast<OctiDir>(d);
                        if (op.pod.prongAt(dir) == NO_PRONG) {
                            continue;
                        }
                        int dx = dirDeltaX(dir);
                        int dy = dirDeltaY(dir);
                        int nx1 = edgeless ? (((lastHop.x + dx) % w + w) % w) : (lastHop.x + dx);
                        int ny1 = edgeless ? (((lastHop.y + dy) % h + h) % h) : (lastHop.y + dy);
                        if (board.isValidCoord(nx1, ny1)) {
                            const OctiSquare &sq1 = board.squareAt(nx1, ny1);
                            if (isEnemyHomeSquareFor(sq1.getType(), mode, turn) &&
                                !sq1.isCapturedByTeamOf(turn) &&
                                (mode != GAME_MODE_OFK || sq1.isEmpty())) {
                                hasOneStepBaseThreat = true;
                            }
                            int nx2 = edgeless ? (((lastHop.x + 2 * dx) % w + w) % w) : (lastHop.x + 2 * dx);
                            int ny2 = edgeless ? (((lastHop.y + 2 * dy) % h + h) % h) : (lastHop.y + 2 * dy);
                            if (board.isValidCoord(nx2, ny2) && !sq1.isEmpty()) {
                                const OctiSquare &sq2 = board.squareAt(nx2, ny2);
                                if (isEnemyHomeSquareFor(sq2.getType(), mode, turn) &&
                                    !sq2.isCapturedByTeamOf(turn) &&
                                    sq2.isEmpty()) {
                                    hasOneJumpBaseThreat = true;
                                }
                            }
                        }
                    }
                    if (hasOneStepBaseThreat) {
                        score += 6000;
                    } else if (hasOneJumpBaseThreat) {
                        score += 4500;
                    }
                }

                // Blunder penalty if ending square sits in enemy jump threat mask
                int destBitIdx = lastHop.y * 9 + lastHop.x;
                int destWord = (destBitIdx >> 6) & 1;
                std::uint64_t destBit = 1ULL << (destBitIdx & 63);
                if (!moveHasEnemyCapture && enemyThreatMask && (enemyThreatMask[destWord] & destBit) != 0ULL) {
                    score -= profile.moveOrderBlunderPenalty;
                } else if (!moveHasEnemyCapture && doesStepVacateEnemyCaptureLanding(board, op.x0, op.y0, lastHop.x, lastHop.y, turn)) {
                    score -= profile.moveOrderBlunderPenalty;
                } else if (preludeOps > 0 && moveHasEnemyCapture && enemyThreatMask && (enemyThreatMask[destWord] & destBit) != 0ULL) {
                    score -= (profile.moveOrderBlunderPenalty / 2);
                }

                // Avoid stacking multiple friendly pods on non-enemy-base squares
                if (!isEnemyHomeSquareFor(destSq.getType(), mode, turn) &&
                    destSq.getNumPods() >= 1) {
                    if (moveHasEnemyCapture) {
                        score -= 2800 * destSq.getNumPods();
                    } else {
                        score -= 5500 * destSq.getNumPods();
                    }
                }

                // Behind-home-rank retreat penalty in 2-player non-edgeless mode
                if (!opEnemyCapture && endDist >= startDist && mode == GAME_MODE_2 && !edgeless) {
                    if ((turn == PLAYER_A && lastHop.y < 2) || (turn == PLAYER_B && lastHop.y > 6)) {
                        if (endDist > startDist || op.isSuperProngBonusMove) {
                            score -= 3500;
                        }
                    }
                }

                // Penalize premature 1-prong lone-pod rushes during development
                if (!moveHasEnemyCapture && !isJump && earlyDevelopment && op.pod.getNumProngs() <= 1 && endDist <= 3) {
                    score -= 3200;
                }
            }

            if (isInitialSegmentForPod) {
                const OctiSquare &startSq = board.squareAt(op.x0, op.y0);
                const OctiSquare &chainFinalDestSq = board.squareAt(chainFinalX, chainFinalY);
                if (isEnemyHomeSquareFor(startSq.getType(), mode, turn)) {
                    if (startSq.getNumPods() <= 1 &&
                        !isEnemyHomeSquareFor(chainFinalDestSq.getType(), mode, turn)) {
                        score -= 35000;
                    }
                } else if (isAlliedHomeSquareFor(startSq.getType(), mode, turn)) {
                    if (startSq.getNumPods() <= 1 &&
                        !moveHasEnemyCapture &&
                        !isEnemyHomeSquareFor(chainFinalDestSq.getType(), mode, turn)) {
                        if (canEnemyCaptureSquareNextTurn(board, op.x0, op.y0,
                                                          chainFinalX, chainFinalY,
                                                          mode, turn, enemyThreatMask)) {
                            score -= 14000;
                        }
                    }
                }
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

            if (isAlliedHomeSquareFor(board.squareAt(op.x0, op.y0).getType(), mode, turn) &&
                op.pod.getNumProngs() >= 1 && nextDist >= curDist) {
                bool hasExistingForwardProng = false;
                for (int d = 0; d < DIR_MAX; ++d) {
                    OctiDir dDir = static_cast<OctiDir>(d);
                    if (op.pod.prongAt(dDir) != NO_PRONG) {
                        int pnx = op.x0 + dirDeltaX(dDir);
                        int pny = op.y0 + dirDeltaY(dDir);
                        if (minChebyshevToTargets(pnx, pny, enemyTargets, w, h, edgeless) < curDist) {
                            hasExistingForwardProng = true;
                            break;
                        }
                    }
                }
                if (!hasExistingForwardProng) {
                    score -= 2200;
                }
            }
            if (op.pod.getNumProngs() == 0) {
                score += 900; // Strongly prioritize arming 0-prong pods first!
            } else if (op.pod.getNumProngs() < 3) {
                score += 450;
            } else if (op.pod.getNumProngs() == 3) {
                if (curDist >= 4) {
                    score -= 700;
                }
            } else {
                score -= 1800 * (op.pod.getNumProngs() - 3);
                if (op.pod.getNumProngs() >= 5) {
                    score -= 2500;
                }
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

            score += prongDirectionBonus(board, mode, turn, op.addProngDir, 1100, 750, 400);
        } else if (op.addPod) {
            if (op.liberate) {
                const OctiSquare &sq = board.squareAt(op.x0, op.y0);
                int unarmedOnSq = 0;
                for (int pi = 0; pi < sq.getNumPods(); ++pi) {
                    if (sq.getPodAt(pi).getNumProngs() == 0) {
                        ++unarmedOnSq;
                    }
                }
                if (sq.getNumPods() >= 2) {
                    score -= 12000 + 6000 * unarmedOnSq;
                } else if (unarmedOnSq == 0 && sq.getNumPods() == 1) {
                    score += 1800;
                } else {
                    score -= 6000 * (unarmedOnSq + 1);
                }
            } else {
                const OctiSquare &sq = board.squareAt(op.x0, op.y0);
                int podBitIdx = op.y0 * 9 + op.x0;
                int podWord = (podBitIdx >> 6) & 1;
                std::uint64_t podBit = 1ULL << (podBitIdx & 63);
                if (sq.getNumPods() >= 2) {
                    score -= 12000;
                } else if (sq.getNumPods() > 0 || hasUnarmedPodOnHomeSquare(board, turn)) {
                    score -= 6000;
                } else if (enemyThreatMask && (enemyThreatMask[podWord] & podBit) != 0ULL) {
                    score -= profile.moveOrderBlunderPenalty;
                } else {
                    score += 150;
                }
            }
        } else if (op.rmProngDir != DIR_UNDEF) {
            score -= 4500 + 800 * op.pod.getNumProngs();
            if (op.addProngDir != DIR_UNDEF && game && game->getFreeProngs(turn) > 0) {
                score -= 12000;
            }
        }
    }
    return score;
}

bool isTacticalMoveForSearch(const OctiBoard &board,
                             OctiGameMode mode,
                             OctiPlayer turn,
                             const OctiMove &mv)
{
    const auto &mOps = mv.getOps();
    for (std::size_t i = 0; i < mOps.size(); ++i) {
        const OctiMoveOp &op = mOps[i];
        for (const auto &h : op.hops) {
            if (h.capture) {
                return true;
            }
        }
        if (!op.hops.empty()) {
            const OctiHop &lastHop = op.hops.back();
            bool continuedLater = false;
            for (std::size_t j = i + 1; j < mOps.size(); ++j) {
                if (!mOps[j].hops.empty() &&
                    mOps[j].x0 == lastHop.x &&
                    mOps[j].y0 == lastHop.y) {
                    continuedLater = true;
                    break;
                }
            }
            if (!continuedLater && board.isValidCoord(lastHop.x, lastHop.y)) {
                const OctiSquare &destSq = board.squareAt(lastHop.x, lastHop.y);
                if (isEnemyHomeSquareFor(destSq.getType(), mode, turn) &&
                    !destSq.isCapturedByTeamOf(turn)) {
                    return true;
                }
            }
        }
    }
    return false;
}

inline std::size_t historyBucketForMoveKey(std::uint64_t moveKey) noexcept
{
    return static_cast<std::size_t>(mixPathKey64(moveKey)) & HISTORY_MASK;
}

} // namespace

struct OctiBotEngine::SearchThreadState {
    std::array<std::array<std::uint64_t, 2>, MAX_SEARCH_PLY> killers{};
    std::array<std::array<std::int32_t, HISTORY_SIZE>, 5> history{};
    BotSearchDiagnostics diag{};
};

namespace {

template <typename ThreadStatePtr = std::nullptr_t>
std::vector<OctiMove> orderAndCapMoves(OctiBoard &board,
                                       std::vector<OctiMove> &rawMoves,
                                       OctiPlayer turn,
                                       std::uint64_t ttBestMoveKey,
                                       std::size_t maxBranch,
                                       const BotTuningProfile &profile,
                                       bool includeComposites = true,
                                       ThreadStatePtr threadState = nullptr,
                                       int ply = 0)
{
    if (includeComposites && (!board.getGame() || board.getGame()->getMode() != GAME_MODE_OFK)) {
        appendCompositeMoves(board, rawMoves, turn);
    }
    std::vector<OctiMove> moves = filterDominatedMoves(board, turn, rawMoves);

    // Hard dominance pruning of single-op +x-y prong rearrangements when free prongs exist.
    // If freeProngs > 0, +x-y is strictly dominated by +x.
    if (board.getGame() && board.getGame()->getFreeProngs(turn) > 0) {
        std::vector<OctiMove> prunedMoves;
        prunedMoves.reserve(moves.size());
        for (const auto &m : moves) {
            const auto &ops = m.getOps();
            bool isDominatedRearrange = (ops.size() == 1 &&
                                         ops[0].hops.empty() &&
                                         !ops[0].addPod &&
                                         ops[0].addProngDir != DIR_UNDEF &&
                                         ops[0].rmProngDir != DIR_UNDEF);
            if (!isDominatedRearrange ||
                (ttBestMoveKey != 0ULL && m.structuralKey() == ttBestMoveKey)) {
                prunedMoves.push_back(m);
            }
        }
        if (!prunedMoves.empty()) {
            moves = std::move(prunedMoves);
        }
    }

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

    std::uint64_t killer0 = 0ULL;
    std::uint64_t killer1 = 0ULL;
    bool applyKillers = false;
    bool applyHistory = false;
    if constexpr (!std::is_same<ThreadStatePtr, std::nullptr_t>::value) {
        if (threadState != nullptr) {
            if (profile.useKillers && ply >= 0 && ply < MAX_SEARCH_PLY) {
                applyKillers = true;
                killer0 = threadState->killers[ply][0];
                killer1 = threadState->killers[ply][1];
            }
            if (profile.useHistory &&
                static_cast<int>(turn) >= 0 &&
                static_cast<int>(turn) < 5) {
                applyHistory = true;
            }
        }
    }

    std::vector<std::pair<int, std::size_t>> scored;
    scored.reserve(moves.size());
    for (std::size_t i = 0; i < moves.size(); ++i) {
        int s = scoreMoveHeuristic(board, moves[i], turn, targets, profile, enemyThreatMask, baseThreatScalePct);
        const std::uint64_t mKey = (ttBestMoveKey != 0ULL || applyKillers || applyHistory)
            ? moves[i].structuralKey()
            : 0ULL;
        if (ttBestMoveKey != 0ULL && mKey == ttBestMoveKey) {
            s += 200000;
        } else if constexpr (!std::is_same<ThreadStatePtr, std::nullptr_t>::value) {
            if ((applyKillers || applyHistory) && mKey != 0ULL &&
                !isTacticalMoveForSearch(board, mode, turn, moves[i])) {
                if (applyKillers && mKey == killer0) {
                    s += 12000;
                    ++threadState->diag.killerHits;
                } else if (applyKillers && mKey == killer1) {
                    s += 10500;
                    ++threadState->diag.killerHits;
                }
                if (applyHistory) {
                    std::int32_t hv = threadState->history[static_cast<int>(turn)][historyBucketForMoveKey(mKey)];
                    if (hv > 0) {
                        s += std::min(6000, static_cast<int>(hv) * 8);
                    }
                }
            }
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
    if (moves.size() <= limit) {
        for (std::size_t i = 0; i < limit; ++i) {
            result.push_back(std::move(moves[scored[i].second]));
        }
        return result;
    }

    // Two-tier prelude diversity selection: prevent a single superprong prelude
    // from exhausting maxBranch with dozens of redundant +prong or quiet 2-prelude
    // continuations before other distinct captures, steps, or root moves are tried.
    using FirstPreludeKey = std::tuple<int, int, int, int, bool>;
    std::map<FirstPreludeKey, int> quietProngCountByPrelude;
    std::map<FirstPreludeKey, int> quietStepCountByPrelude;
    std::map<FirstPreludeKey, int> twoPreludeQuietCount;
    std::vector<std::size_t> deferredIndices;

    for (std::size_t i = 0; i < scored.size() && result.size() < limit; ++i) {
        std::size_t mIdx = scored[i].second;
        const OctiMove &m = moves[mIdx];
        const auto &mOps = m.getOps();
        const std::uint64_t mKey = (ttBestMoveKey != 0ULL || applyKillers)
            ? m.structuralKey()
            : 0ULL;
        const bool isPriorityKey =
            (ttBestMoveKey != 0ULL && mKey == ttBestMoveKey) ||
            (applyKillers && mKey != 0ULL && (mKey == killer0 || mKey == killer1));
        if (mOps.size() <= 1 || !mOps[0].isSuperProngBonusMove || isPriorityKey) {
            result.push_back(m);
            continue;
        }

        const OctiMoveOp &op0 = mOps[0];
        int end0X = op0.hops.empty() ? op0.x0 : op0.hops.back().x;
        int end0Y = op0.hops.empty() ? op0.y0 : op0.hops.back().y;
        bool op0Captures = false;
        for (const auto &h : op0.hops) {
            if (h.capture) {
                op0Captures = true;
                break;
            }
        }
        FirstPreludeKey pKey(op0.x0, op0.y0, end0X, end0Y, op0Captures);

        bool followupTactical = false;
        for (std::size_t k = 1; k < mOps.size(); ++k) {
            for (const auto &h : mOps[k].hops) {
                if (h.capture) {
                    followupTactical = true;
                    break;
                }
            }
            if (!mOps[k].hops.empty()) {
                const OctiHop &lh = mOps[k].hops.back();
                const OctiSquare &dSq = board.squareAt(lh.x, lh.y);
                if (isEnemyHomeSquareFor(dSq.getType(), mode, turn) &&
                    !dSq.isCapturedByTeamOf(turn)) {
                    followupTactical = true;
                }
            }
        }

        if (followupTactical) {
            result.push_back(m);
            continue;
        }

        const OctiMoveOp &regOp = mOps.back();
        if (mOps.size() >= 3) {
            if (twoPreludeQuietCount[pKey] < 2) {
                twoPreludeQuietCount[pKey]++;
                result.push_back(m);
            } else {
                deferredIndices.push_back(mIdx);
            }
        } else if (regOp.hops.empty()) {
            if (quietProngCountByPrelude[pKey] < 2) {
                quietProngCountByPrelude[pKey]++;
                result.push_back(m);
            } else {
                deferredIndices.push_back(mIdx);
            }
        } else {
            if (quietStepCountByPrelude[pKey] < 3) {
                quietStepCountByPrelude[pKey]++;
                result.push_back(m);
            } else {
                deferredIndices.push_back(mIdx);
            }
        }
    }

    for (std::size_t mIdx : deferredIndices) {
        if (result.size() >= limit) {
            break;
        }
        result.push_back(moves[mIdx]);
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
    , _ttSizeLog2(clampTtSizeLog2(_profile.ttSizeLog2))
    , _ttMask((std::size_t{1} << _ttSizeLog2) - 1)
    , _tt(std::size_t{1} << _ttSizeLog2)
    , _lastDiagnostics()
{
}

OctiBotEngine::OctiBotEngine(const OctiBotEngine &other)
    : _difficulty(other._difficulty)
    , _profile(other._profile)
    , _timeBudgetMs(other._timeBudgetMs)
    , _customTimeBudget(other._customTimeBudget)
    , _rngState(other._rngState)
    , _ttSizeLog2(other._ttSizeLog2)
    , _ttMask(other._ttMask)
    , _tt(other._tt)
    , _lastDiagnostics(other._lastDiagnostics)
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
        _ttSizeLog2 = other._ttSizeLog2;
        _ttMask = other._ttMask;
        _tt = other._tt;
        _lastDiagnostics = other._lastDiagnostics;
        _stopSearch.store(false, std::memory_order_relaxed);
        _liveDepthReached.store(other._liveDepthReached.load(std::memory_order_relaxed), std::memory_order_relaxed);
        _liveNodesVisited.store(other._liveNodesVisited.load(std::memory_order_relaxed), std::memory_order_relaxed);
        _liveBestScore.store(other._liveBestScore.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    return *this;
}

OctiBotEngine::~OctiBotEngine() = default;

void OctiBotEngine::ensureTranspositionTableSize()
{
    int clampedLog2 = clampTtSizeLog2(_profile.ttSizeLog2);
    std::size_t desiredSize = (std::size_t{1} << clampedLog2);
    if (_tt.size() != desiredSize || _ttSizeLog2 != clampedLog2) {
        _tt.assign(desiredSize, TranspositionEntry{});
        _ttSizeLog2 = clampedLog2;
        _ttMask = desiredSize - 1;
    }
}

std::uint64_t OctiBotEngine::lookupTranspositionBestMoveKey(const OctiBoard &board) const
{
    if (_tt.empty()) {
        return 0ULL;
    }
    std::uint64_t hashKey = computePositionHash(board);
    std::size_t ttIdx = hashKey & _ttMask;
    std::unique_lock<std::mutex> lock(
        g_ttMutexes[ttIdx & (TT_MUTEX_COUNT - 1)].mtx, std::defer_lock);
    if (_profile.lazySmpThreads > 1) {
        lock.lock();
    }
    const TranspositionEntry &ttEntry = _tt[ttIdx];
    if (ttEntry.key == hashKey && ttEntry.depth > -128) {
        return ttEntry.bestMoveKey;
    }
    return 0ULL;
}

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
    ensureTranspositionTableSize();
}

BotDifficulty OctiBotEngine::difficulty() const
{
    return _difficulty;
}

void OctiBotEngine::setTuningProfile(const BotTuningProfile &profile)
{
    _profile = profile;
    _timeBudgetMs = profile.timeBudgetMs;
    ensureTranspositionTableSize();
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
        p.bookSelectionWindow = 160;
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
        p.bookSelectionWindow = 25;
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
    case BotDifficulty::EXPERT:
        p.timeBudgetMs = 40000;
        p.maxSearchDepth = 64;
        p.maxQuiesceDepth = 5;
        p.quiesceBranchCap = 12;
        p.rootBranchCap = 64;
        p.treeBranchCap = 36;
        p.lazySmpThreads = 4;
        p.compositeMoveMaxPly = 2;
        p.rootScoreJitter = 0;
        p.bookSelectionWindow = 25;
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
    case BotDifficulty::GRANDMASTER:
        p.timeBudgetMs = 60000;
        p.maxSearchDepth = 64;
        p.maxQuiesceDepth = 6;
        p.quiesceBranchCap = 14;
        p.rootBranchCap = 72;
        p.treeBranchCap = 40;
        p.lazySmpThreads = 4;
        p.compositeMoveMaxPly = 3;
        p.rootScoreJitter = 0;
        p.bookSelectionWindow = 25;
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
    case BotDifficulty::MASTER:
    default:
        p.timeBudgetMs = 20000;
        p.maxSearchDepth = 64;
        p.maxQuiesceDepth = 4;
        p.quiesceBranchCap = 10;
        p.rootBranchCap = 56;
        p.treeBranchCap = 32;
        p.lazySmpThreads = 4;
        p.compositeMoveMaxPly = 2;
        p.rootScoreJitter = 0;
        p.bookSelectionWindow = 25;
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
                                          bool mirrorHorizontal,
                                          bool exclude4PBases = false)
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
    if (!exclude4PBases || mode != GAME_MODE_4) {
        int btw = std::min(4, std::max(0, game->getBasesToWin()));
        h ^= g_zobrist.basesToWin[btw];
    }
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

std::uint64_t OctiBotEngine::toggleTurnHashStatic(std::uint64_t hash,
                                                  OctiPlayer fromTurn,
                                                  OctiPlayer toTurn) noexcept
{
    int fIdx = static_cast<int>(fromTurn) % 5;
    int tIdx = static_cast<int>(toTurn) % 5;
    if (fIdx < 0) fIdx = 0;
    if (tIdx < 0) tIdx = 0;
    return hash ^ g_zobrist.turn[fIdx] ^ g_zobrist.turn[tIdx];
}

std::uint64_t OctiBotEngine::computePositionHash(const OctiBoard &board) const
{
    return computePositionHashStatic(board);
}

std::uint64_t OctiBotEngine::computeCanonicalOpeningHash(const OctiBoard &board,
                                                         bool *outMirrored)
{
    std::uint64_t hDirect = computePositionHashInternal(board, false, true);
    if (board.getGameMode() == GAME_MODE_4) {
        if (outMirrored) {
            *outMirrored = false;
        }
        return hDirect;
    }

    std::uint64_t hMirror = computePositionHashInternal(board, true, true);
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
    int window = (profile.bookSelectionWindow > 0) ? profile.bookSelectionWindow : 0;
    std::size_t eligibleCount = 1;
    if (window > 0) {
        for (std::size_t i = 1; i < validCands.size(); ++i) {
            if (validCands[i].score >= bestScore - window) {
                ++eligibleCount;
            }
        }
    }

    std::size_t chosenIdx = 0;
    if (eligibleCount > 1 && window > 0) {
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

OctiEvalBreakdown OctiBotEngine::evaluatePositionBreakdown(
    const OctiBoard &board,
    OctiPlayer perspective,
    const BotTuningProfile *profile)
{
    if (profile) {
        return evaluatePositionBreakdown(board, perspective, *profile);
    }
    return evaluatePositionBreakdown(
        board, perspective, defaultProfileForDifficulty(BotDifficulty::MASTER));
}

OctiEvalBreakdown OctiBotEngine::evaluatePositionBreakdown(
    const OctiGame &game,
    OctiPlayer perspective,
    const BotTuningProfile &profile)
{
    const OctiBoard *board = game.getBoard();
    if (!board) {
        return OctiEvalBreakdown();
    }
    return evaluatePositionBreakdown(*board, perspective, profile);
}

OctiEvalBreakdown OctiBotEngine::evaluatePositionBreakdown(
    const OctiGame &game,
    OctiPlayer perspective,
    const BotTuningProfile *profile)
{
    const OctiBoard *board = game.getBoard();
    if (!board) {
        return OctiEvalBreakdown();
    }
    return evaluatePositionBreakdown(*board, perspective, profile);
}

int OctiBotEngine::evaluatePosition(const OctiBoard &board,
                                    OctiPlayer perspective)
{
    return evaluatePosition(board, perspective, defaultProfileForDifficulty(BotDifficulty::MASTER));
}

int OctiBotEngine::evaluatePosition(const OctiGame &game,
                                    OctiPlayer perspective,
                                    const BotTuningProfile &profile)
{
    const OctiBoard *board = game.getBoard();
    if (!board) {
        return 0;
    }
    return evaluatePosition(*board, perspective, profile);
}

int OctiBotEngine::evaluatePosition(const OctiGame &game,
                                    OctiPlayer perspective)
{
    const OctiBoard *board = game.getBoard();
    if (!board) {
        return 0;
    }
    return evaluatePosition(*board, perspective);
}

int OctiBotEngine::evaluatePosition(const OctiBoard &board,
                                    OctiPlayer perspective,
                                    const BotTuningProfile &profile)
{
    return evaluatePositionBreakdown(board, perspective, profile).totalScore;
}

OctiEvalBreakdown OctiBotEngine::evaluatePositionBreakdown(
    const OctiBoard &board,
    OctiPlayer perspective,
    const BotTuningProfile &profile)
{
    OctiEvalBreakdown bd;
    const OctiGame *game = board.getGame();
    if (!game || perspective == PLAYER_UNDEF) {
        return bd;
    }

    OctiGameMode mode = board.getGameMode();
    if (game->isGameOver()) {
        OctiPlayer w = game->getWinner();
        if (w != PLAYER_UNDEF) {
            bd.terminalScore = areAlliedPlayers(mode, w, perspective) ? WIN_SCORE : -WIN_SCORE;
            bd.totalScore = bd.terminalScore;
        }
        return bd;
    }

    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    bool edgeless = game->useEdgelessBoard();

    std::vector<std::pair<int, int>> allyTargets = targetEnemyBases(board, perspective);
    OctiPlayer sampleEnemy = (perspective == PLAYER_A || perspective == PLAYER_C)
        ? PLAYER_B : PLAYER_A;
    std::vector<std::pair<int, int>> enemyTargets = targetEnemyBases(board, sampleEnemy);

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
    std::uint64_t allyImmobilizedMask[2] = {0ULL, 0ULL};
    std::uint64_t enemyImmobilizedMask[2] = {0ULL, 0ULL};

    int allyOneStepBaseThreatRaw = 0;
    int enemyOneStepBaseThreatRaw = 0;
    int allyOneJumpBaseThreatRaw = 0;
    int enemyOneJumpBaseThreatRaw = 0;
    int allyTwoPlyBaseThreatRaw = 0;
    int enemyTwoPlyBaseThreatRaw = 0;

    int podZeroVal = profile.podZeroProngVal;
    int podOneVal = (mode == GAME_MODE_OFK) ? (profile.podOneProngVal * 135) / 100 : profile.podOneProngVal;
    int podTwoVal = (mode == GAME_MODE_OFK) ? (profile.podTwoProngVal * 135) / 100 : profile.podTwoProngVal;
    int podThreePlusVal = (mode == GAME_MODE_OFK) ? (profile.podThreePlusProngVal * 135) / 100 : profile.podThreePlusProngVal;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const OctiSquare &sq = board.squareAt(x, y);
            OctiSquareType sqType = sq.getType();

            if (isEnemyHomeSquareFor(sqType, mode, perspective)) {
                if (sq.isCapturedByTeamOf(perspective)) {
                    int effScale = (allyPhase.basesNeeded <= 1) ? 100 : ((allyPhase.basesNeeded == 2) ? std::max(60, allyBaseThreatScalePct) : std::max(35, allyBaseThreatScalePct));
                    int baseBonus = (profile.capturedEnemyBaseBonus * effScale) / 100;
                    bd.capturedEnemyBases += baseBonus;
                }
            } else if (isAlliedHomeSquareFor(sqType, mode, perspective)) {
                if (sq.isCaptured() && !sq.isCapturedByTeamOf(perspective)) {
                    int effScale = (enemyPhase.basesNeeded <= 1) ? 100 : ((enemyPhase.basesNeeded == 2) ? std::max(60, enemyBaseThreatScalePct) : std::max(35, enemyBaseThreatScalePct));
                    int basePen = (profile.lostAlliedBasePenalty * effScale) / 100;
                    bd.lostAlliedBases -= basePen;
                }
            } else if (sqType == NEUTRAL_OCTI_SQUARE) {
                if (sq.isCapturedByTeamOf(perspective)) {
                    bd.neutralOctiSquares += profile.neutralOctiSquareBonus;
                } else if (sq.isCaptured()) {
                    bd.neutralOctiSquares -= profile.neutralOctiSquareBonus;
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
                    int stackPen = (numPods - 1) * 2800 + unarmedCount * 950;
                    bd.stackAndCapturedBaseHolding += sqAlly ? -stackPen : stackPen;
                } else if (onCapturedEnemyBase) {
                    int capBasePen = 0;
                    if (unarmedCount >= 1) {
                        // First unarmed pod holds the base (1800 pen if solo holder, 2800 if stacked with armed pod)
                        capBasePen += (numPods == 1) ? 1800 : 2800;
                        // Every additional unarmed pod beyond the first is a wasted liberation turn (> 3800 capturedPodPen)
                        if (unarmedCount > 1) {
                            capBasePen += (unarmedCount - 1) * 4800;
                        }
                    }
                    if (numPods >= 2) {
                        capBasePen += (numPods - 1) * 2200;
                    }
                    if (numPods > 2) {
                        capBasePen += (numPods - 2) * 4500;
                    }
                    bd.stackAndCapturedBaseHolding += sqAlly ? -capBasePen : capBasePen;
                }
            }

            for (int i = 0; i < numPods; ++i) {
                const OctiPod &pod = sq.getPodAt(i);
                OctiPlayer owner = podTypeToPlayer(pod.getType());
                bool isAlly = areAlliedPlayers(mode, owner, perspective);
                const int sign = isAlly ? 1 : -1;
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
                int advanceScalePct = (isAlly ? allyBaseWinFeasible : enemyBaseWinFeasible)
                    ? std::max(scalePct, 65)
                    : scalePct;
                if (numProngs == 0 && !onCapturedEnemyBase && sqType != NEUTRAL_OCTI_SQUARE) {
                    bd.podMaterial += sign * podZeroVal;
                } else if (numProngs == 1) {
                    int distBonus = (std::max(0, (10 - dist)) * 110 * advanceScalePct) / 100;
                    bd.podMaterial += sign * podOneVal;
                    bd.podAdvanceDistance += sign * distBonus;
                    if (!isAlliedHomeSquareFor(sqType, mode, owner) && dist <= 4 && dist >= 2) {
                        bd.loneOneProngRush -= sign * profile.loneOneProngRushPenalty;
                    }
                } else if (numProngs == 2) {
                    int distBonus = (std::max(0, (10 - dist)) * 180 * advanceScalePct) / 100;
                    bd.podMaterial += sign * podTwoVal;
                    bd.podAdvanceDistance += sign * distBonus;
                } else {
                    int distBonus = (std::max(0, (10 - dist)) * 210 * advanceScalePct) / 100;
                    bd.podMaterial += sign * podThreePlusVal;
                    bd.podAdvanceDistance += sign * distBonus;
                }

                bool hasOrth = false;
                bool hasDiag = false;
                bool hasOneStepBaseThreat = false;
                bool hasOneJumpBaseThreat = false;
                bool hasForwardProng = false;
                int legalMoveDirs = 0;

                for (int d = 0; d < DIR_MAX; ++d) {
                    OctiDir dir = static_cast<OctiDir>(d);
                    OctiProngType pt = pod.prongAt(dir);
                    if (pt == NO_PRONG) {
                        continue;
                    }
                    bd.prongMaterial += sign * ((pt == SUPER_PRONG) ? profile.superProngVal : profile.normalProngVal);
                    if (isOrthogonalDir(dir)) hasOrth = true;
                    if (isDiagonalDir(dir)) hasDiag = true;

                    bd.prongDirectionality += sign * prongDirectionBonus(board, mode, owner, dir, 380, 260, 120);

                    int nx = edgeless ? (((x + dirDeltaX(dir)) % w + w) % w) : (x + dirDeltaX(dir));
                    int ny = edgeless ? (((y + dirDeltaY(dir)) % h + h) % h) : (y + dirDeltaY(dir));
                    int jx = edgeless ? (((x + 2 * dirDeltaX(dir)) % w + w) % w) : (x + 2 * dirDeltaX(dir));
                    int jy = edgeless ? (((y + 2 * dirDeltaY(dir)) % h + h) % h) : (y + 2 * dirDeltaY(dir));

                    bool canStep = false;
                    bool canJump = false;

                    if (board.isValidCoord(nx, ny)) {
                        const OctiSquare &nsq = board.squareAt(nx, ny);
                        canStep = (mode == GAME_MODE_OFK) ? nsq.isEmpty() : !nsq.hasEnemyOf(owner);
                        if (!nsq.isEmpty() && board.isValidCoord(jx, jy)) {
                            const OctiSquare &destSq = board.squareAt(jx, jy);
                            if (destSq.isEmpty()) {
                                canJump = true;
                            }
                        }

                        if (canStep || canJump) {
                            ++legalMoveDirs;
                        }

                        int ndist = minChebyshevToTargets(nx, ny, targets, w, h, edgeless);
                        if (ndist < dist && (canStep || canJump)) {
                            bd.prongDirectionality += sign * 140;
                        }
                        bool canStepBase = (mode == GAME_MODE_OFK) ? nsq.isEmpty() : !nsq.hasEnemyOf(owner);
                        if (ndist == 0 && canStepBase) {
                            hasOneStepBaseThreat = true;
                        }
                        // Leapfrog battery detection (forward only)
                        if (squareHasAllyOf(nsq, mode, owner)) {
                            if (board.isValidCoord(jx, jy)) {
                                const OctiSquare &destSq = board.squareAt(jx, jy);
                                bool canJumpBase = (mode == GAME_MODE_OFK) ? destSq.isEmpty() : !destSq.hasEnemyOf(owner);
                                if (destSq.isEmpty() || canJumpBase) {
                                    int jdist = minChebyshevToTargets(jx, jy, targets, w, h, edgeless);
                                    if (jdist < dist) {
                                        bd.leapfrogBattery += sign * (profile.leapfrogBatteryWeight + profile.leapfrogForwardBonus);
                                    }
                                }
                            }
                        }
                    }

                    int ndist = minChebyshevToTargets(nx, ny, targets, w, h, edgeless);
                    bool isForward = (ndist < dist);
                    if (!isForward && mode != GAME_MODE_4) {
                        if ((owner == PLAYER_A && (dir == DIR_N || dir == DIR_NE || dir == DIR_NW)) ||
                            (owner == PLAYER_B && (dir == DIR_S || dir == DIR_SE || dir == DIR_SW))) {
                            isForward = true;
                        }
                    }
                    if (isForward) {
                        hasForwardProng = true;
                    }

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

                if (isAlliedHomeSquareFor(sqType, mode, owner) && numProngs >= 2 && !hasForwardProng) {
                    bd.structuralPositional -= sign * 900;
                }

                if (mode == GAME_MODE_2 && !edgeless) {
                    if ((owner == PLAYER_A && y < 2) || (owner == PLAYER_B && y > 6)) {
                        bd.structuralPositional -= sign * 850;
                    }
                }

                if (numProngs > 0 && !onCapturedEnemyBase && legalMoveDirs == 0) {
                    bd.structuralPositional -= sign * 900;
                    int sqBit = y * 9 + x;
                    int wordIdx = (sqBit >> 6) & 1;
                    if (isAlly) {
                        allyImmobilizedMask[wordIdx] |= (1ULL << (sqBit & 63));
                    } else {
                        enemyImmobilizedMask[wordIdx] |= (1ULL << (sqBit & 63));
                    }
                }

                if (hasOneStepBaseThreat) {
                    int stepBonus = (profile.oneStepBaseThreatBonus * scalePct) / 100;
                    if (isAlly) {
                        allyOneStepBaseThreatRaw += stepBonus;
                    } else {
                        enemyOneStepBaseThreatRaw += stepBonus;
                    }
                }
                if (hasOneJumpBaseThreat) {
                    int jumpBonus = (profile.oneJumpBaseThreatBonus * scalePct) / 100;
                    if (isAlly) {
                        allyOneJumpBaseThreatRaw += jumpBonus;
                    } else {
                        enemyOneJumpBaseThreatRaw += jumpBonus;
                    }
                }

                // 2-ply base threat check
                if (dist == 1 && profile.twoPlyBaseThreatWeight > 0) {
                    int twoPlyBonus = (profile.twoPlyBaseThreatWeight * scalePct) / 100;
                    if (isAlly) {
                        allyTwoPlyBaseThreatRaw += twoPlyBonus;
                    } else {
                        enemyTwoPlyBaseThreatRaw += twoPlyBonus;
                    }
                }

                if (hasOrth && hasDiag && (!isAlliedHomeSquareFor(sqType, mode, owner) || numProngs < 2 || hasForwardProng)) {
                    bd.orthDiagVersatility += sign * profile.orthDiagVersatilityBonus;
                } else if (numProngs >= 2 && hasDiag && !hasOrth) {
                    bd.orthDiagVersatility -= sign * profile.diagOnlyParityPenalty;
                }

                JumpMobilityThreat threat;
                if (numProngs > 0) {
                    threat = evaluatePodJumpThreat(board, x, y, pod, targets);
                    bd.jumpReachability += sign * (threat.reachableSquares * profile.jumpReachableSquareWeight);
                    bd.jumpCaptureThreats += sign * (threat.capturablePods * profile.jumpCapturablePodWeight +
                                                     threat.capturableProngs * profile.jumpCapturableProngWeight);
                    if (threat.minJumpDistToTargets < dist) {
                        bd.jumpReachability += sign * (((dist - threat.minJumpDistToTargets) * profile.jumpReachStepWeight * advanceScalePct) / 100);
                    }
                    if (isAlly) {
                        allyJumpThreatMask[0] |= threat.capturedSqMask[0];
                        allyJumpThreatMask[1] |= threat.capturedSqMask[1];
                    } else {
                        enemyJumpThreatMask[0] |= threat.capturedSqMask[0];
                        enemyJumpThreatMask[1] |= threat.capturedSqMask[1];
                    }
                }
            }
        }
    }

    int contestedScalePct = std::clamp(profile.contestedEnemyBaseThreatScalePct, 0, 100);
    bool enemyContests = (enemyOneStepBaseThreatRaw > 0 ||
                          enemyOneJumpBaseThreatRaw > 0 ||
                          enemyTwoPlyBaseThreatRaw > 0 ||
                          bd.lostAlliedBases < 0 ||
                          enemyJumpThreatMask[0] != 0ULL ||
                          enemyJumpThreatMask[1] != 0ULL);
    if (contestedScalePct != 100 && enemyContests) {
        allyOneStepBaseThreatRaw = (allyOneStepBaseThreatRaw * contestedScalePct) / 100;
        allyOneJumpBaseThreatRaw = (allyOneJumpBaseThreatRaw * contestedScalePct) / 100;
        allyTwoPlyBaseThreatRaw = (allyTwoPlyBaseThreatRaw * contestedScalePct) / 100;
    }
    bd.oneStepBaseThreat = allyOneStepBaseThreatRaw - enemyOneStepBaseThreatRaw;
    bd.oneJumpBaseThreat = allyOneJumpBaseThreatRaw - enemyOneJumpBaseThreatRaw;
    bd.twoPlyBaseThreat = allyTwoPlyBaseThreatRaw - enemyTwoPlyBaseThreatRaw;

    // Army Development Bonuses (Armed Pods)
    if (allyArmedPods >= 1) bd.armyDevelopment += profile.firstArmedPodBonus;
    if (allyArmedPods >= 2) bd.armyDevelopment += profile.secondArmedPodBonus;
    if (allyArmedPods >= 3) bd.armyDevelopment += profile.thirdArmedPodBonus;

    if (enemyArmedPods >= 1) bd.armyDevelopment -= profile.firstArmedPodBonus;
    if (enemyArmedPods >= 2) bd.armyDevelopment -= profile.secondArmedPodBonus;
    if (enemyArmedPods >= 3) bd.armyDevelopment -= profile.thirdArmedPodBonus;

    int effMonopolyWeight = (mode == GAME_MODE_OFK && profile.ofkSinglePodMonopolyPenalty >= 0)
        ? profile.ofkSinglePodMonopolyPenalty
        : profile.singlePodMonopolyPenalty;

    // Single-Pod Monopoly, Heavy-Pod Threat & Back-Rank Prong-Hoarding Penalty
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const OctiSquare &sq = board.squareAt(x, y);
            for (int i = 0; i < sq.getNumPods(); ++i) {
                const OctiPod &pod = sq.getPodAt(i);
                int prongs = pod.getNumProngs();
                if (prongs > 2) {
                    OctiPlayer podOwner = podTypeToPlayer(pod.getType());
                    bool isAlly = areAlliedPlayers(mode, podOwner, perspective);
                    int unarmedHome = isAlly ? allyUnarmedHomePods : enemyUnarmedHomePods;
                    int armedPods = isAlly ? allyArmedPods : enemyArmedPods;

                    bool triggerMonopoly = (unarmedHome >= 2) ||
                        (prongs > 3 && armedPods <= 1 && (unarmedHome + game->getFreePods(podOwner)) >= 2);
                    if (triggerMonopoly) {
                        int monPen = (prongs - 2) * effMonopolyWeight;
                        bd.singlePodMonopoly += isAlly ? -monPen : monPen;
                    }

                    if (!isAlly && prongs >= 4 && profile.enemyHeavyPodThreatPenalty != 0) {
                        bd.heavyPodThreat -= (prongs - 3) * profile.enemyHeavyPodThreatPenalty;
                    }

                    if (prongs >= 5) {
                        int overArmPen = (prongs - 4) * 650 + (prongs >= 6 ? (prongs - 5) * 900 : 0);
                        bd.overArmAndBackRank += isAlly ? -overArmPen : overArmPen;
                    }

                    if (prongs > 3) {
                        const auto &targets = isAlly ? allyTargets : enemyTargets;
                        int d = minChebyshevToTargets(x, y, targets, w, h, edgeless);
                        if (d >= 4 && armedPods >= 3) {
                            int backRankPen = (prongs - 3) * 320;
                            bd.overArmAndBackRank += isAlly ? -backRankPen : backRankPen;
                        }
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

            bool inJumpThreat = ((sqAlly ? enemyJumpThreatMask[wordIdx] : allyJumpThreatMask[wordIdx]) & bitMask) != 0ULL;
            bool isImmobilized = ((sqAlly ? allyImmobilizedMask[wordIdx] : enemyImmobilizedMask[wordIdx]) & bitMask) != 0ULL;
            bool isAdjacentThreat = isImmobilized && isSquareThreatenedByEnemyProng(board, x, y, mode, sqOwner);

            if (sqAlly && (inJumpThreat || isAdjacentThreat)) {
                int vulnPen = numPods * profile.hangingPodBasePenalty + prongsOnSq * prongWeight;
                if (isImmobilized) {
                    vulnPen += 1500;
                }
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
                bd.hangingVulnerability -= vulnPen;
            } else if (!sqAlly && (inJumpThreat || isAdjacentThreat)) {
                int vulnPen = numPods * profile.hangingPodBasePenalty + prongsOnSq * prongWeight;
                if (isImmobilized) {
                    vulnPen += 1500;
                }
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
                bd.hangingVulnerability += vulnPen;
            }
        }
    }

    if (totalAllyPodsOnBoard == 0) {
        bd.hangingVulnerability -= (profile.wipeoutDangerPenalty + profile.soleArmedPodDangerPenalty);
    }
    if (totalEnemyPodsOnBoard == 0) {
        bd.hangingVulnerability += (profile.wipeoutDangerPenalty + profile.soleArmedPodDangerPenalty);
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
                    bd.closestBaseDistance += std::max(0, 10 - d_min1) * allyClosestPodDistWeight;
                }
                if (d_min2 < 999 && allySecondClosestPodDistWeight > 0) {
                    bd.closestBaseDistance += std::max(0, 10 - d_min2) * allySecondClosestPodDistWeight;
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
                    bd.closestBaseDistance -= std::max(0, 10 - d_min1) * enemyClosestPodDistWeight;
                }
                if (d_min2 < 999 && enemySecondClosestPodDistWeight > 0) {
                    bd.closestBaseDistance -= std::max(0, 10 - d_min2) * enemySecondClosestPodDistWeight;
                }
            }
        }
    }

    // Reserves and material bonuses
    int numPlayers = (mode == GAME_MODE_4) ? 4 : 2;
    for (int pIdx = 1; pIdx <= numPlayers; ++pIdx) {
        OctiPlayer p = static_cast<OctiPlayer>(pIdx);
        bool isAlly = areAlliedPlayers(mode, p, perspective);
        const int sign = isAlly ? 1 : -1;
        int freePods = game->getFreePods(p);
        int capturedPodPen = (mode == GAME_MODE_OFK) ? 4800 : profile.capturedPodPenalty;
        int freeProngVal = (mode == GAME_MODE_OFK) ? 180 : profile.reserveFreeProngVal;
        int resMat = freePods * profile.reserveFreePodVal
                   - game->getCapturedPods(p) * capturedPodPen
                   + game->getFreeProngs(p) * freeProngVal
                   + game->getFreeSuperProngs(p) * 150;
        if (mode == GAME_MODE_4 && game->isPlayerEliminated(p)) {
            resMat -= 20000;
        }
        bd.reserveAndCapturedMaterial += sign * resMat;

        if (mode != GAME_MODE_OFK && freePods < 4 &&
            (isAlly ? (allyUnarmedHomePods >= 2 && totalAllyPodsOnBoard >= 2)
                    : (enemyUnarmedHomePods >= 2 && totalEnemyPodsOnBoard >= 2)) &&
            game->getFreeProngs(p) >= 6) {
            bd.undeployedAndUnarmedHomePenalty -= sign * ((4 - freePods) * 1500);
        }
        if (mode != GAME_MODE_OFK &&
            (isAlly ? (allyUnarmedHomePods >= 1 && totalAllyPodsOnBoard >= 2)
                    : (enemyUnarmedHomePods >= 1 && totalEnemyPodsOnBoard >= 2)) &&
            game->getFreeProngs(p) >= 6) {
            int unarmedHome = isAlly ? allyUnarmedHomePods : enemyUnarmedHomePods;
            bd.undeployedAndUnarmedHomePenalty -= sign * (unarmedHome * profile.unarmedHomePodPenalty);
        }
        if (board.canLiberate(p) && game->getCapturedPods(p) > 0) {
            bd.liberationBonus += sign * profile.canLiberateBonus;
        }
    }

    bd.totalScore = bd.sumTerms();
    return bd;
}

// ---------------------------------------------------------------------------
// Quiescence Search with Stand-Pat & Tactical Capture/Base Chains
// ---------------------------------------------------------------------------

namespace {

inline void recordBatchNodeVisit(std::uint64_t nodesVisited,
                                 std::atomic<std::uint64_t> &liveNodes)
{
    if (nodesVisited == 1ULL) {
        liveNodes.fetch_add(1ULL, std::memory_order_relaxed);
    } else if ((nodesVisited & 4095ULL) == 0ULL) {
        liveNodes.fetch_add((nodesVisited == 4096ULL) ? 4095ULL : 4096ULL,
                            std::memory_order_relaxed);
    }
}

inline std::uint64_t unbatchedNodeRemainder(std::uint64_t nodesVisited)
{
    if (nodesVisited == 0ULL) {
        return 0ULL;
    }
    if (nodesVisited < 4096ULL) {
        return nodesVisited - 1ULL;
    }
    return nodesVisited & 4095ULL;
}

} // namespace

int OctiBotEngine::quiesce(OctiBoard &scratchBoard,
                           int qDepth,
                           int ply,
                           int alpha,
                           int beta,
                           OctiPlayer currentPerspective,
                           std::uint64_t &nodesVisited,
                           long deadlineMs,
                           bool &timedOut,
                           SearchThreadState &state)
{
    ++nodesVisited;
    recordBatchNodeVisit(nodesVisited, _liveNodesVisited);
    if ((nodesVisited & 63ULL) == 0ULL) {
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

    const bool useQsearchTt = _profile.qsearchUseTt;
    const bool useTtLock = (_profile.lazySmpThreads > 1);
    const int qTtDepth = std::max(-120, -qDepth);
    std::uint64_t hashKey = 0ULL;
    std::size_t ttIdx = 0;
    std::uint64_t ttMoveKey = 0ULL;
    int origAlpha = alpha;

    auto storeQuiesceTt = [&](int storeDepth, int val, std::int8_t flag, std::uint64_t bestKey) {
        std::unique_lock<std::mutex> lock(
            g_ttMutexes[ttIdx & (TT_MUTEX_COUNT - 1)].mtx, std::defer_lock);
        if (useTtLock) {
            lock.lock();
        }
        TranspositionEntry &ttEntry = _tt[ttIdx];
        if (ttEntry.key == 0 || storeDepth >= static_cast<int>(ttEntry.depth)) {
            const bool sameKey = (ttEntry.key == hashKey);
            ttEntry.key = hashKey;
            ttEntry.depth = static_cast<std::int16_t>(storeDepth);
            int storeScore = val;
            if (storeScore >= WIN_SCORE - 1000) {
                storeScore += ply;
            } else if (storeScore <= -WIN_SCORE + 1000) {
                storeScore -= ply;
            }
            ttEntry.score = storeScore;
            if (bestKey != 0ULL) {
                ttEntry.bestMoveKey = bestKey;
            } else if (!sameKey) {
                ttEntry.bestMoveKey = 0ULL;
            }
            ttEntry.flag = flag;
        }
    };

    if (useQsearchTt) {
        ++state.diag.qsearchTtProbes;
        hashKey = computePositionHash(scratchBoard);
        ttIdx = hashKey & _ttMask;
        std::unique_lock<std::mutex> lock(
            g_ttMutexes[ttIdx & (TT_MUTEX_COUNT - 1)].mtx, std::defer_lock);
        if (useTtLock) {
            lock.lock();
        }
        const TranspositionEntry &ttEntry = _tt[ttIdx];
        if (ttEntry.key == hashKey && ttEntry.depth > -128) {
            ttMoveKey = ttEntry.bestMoveKey;
            if (ttEntry.depth >= qTtDepth) {
                int ttScore = ttEntry.score;
                if (ttScore >= WIN_SCORE - 1000) {
                    ttScore -= ply;
                } else if (ttScore <= -WIN_SCORE + 1000) {
                    ttScore += ply;
                }
                if (ttEntry.flag == 0) {
                    ++state.diag.qsearchTtHits;
                    return ttScore;
                }
                if (ttEntry.flag == 1 && ttScore >= beta) {
                    ++state.diag.qsearchTtHits;
                    return ttScore;
                }
                if (ttEntry.flag == 2 && ttScore <= alpha) {
                    ++state.diag.qsearchTtHits;
                    return ttScore;
                }
            }
        }
    }

    int standPat = evaluatePosition(scratchBoard, currentPerspective, _profile);
    if (standPat >= beta) {
        if (useQsearchTt) {
            storeQuiesceTt(0, standPat, 1, 0ULL);
        }
        return beta;
    }
    if (standPat > alpha) {
        alpha = standPat;
    }

    if (qDepth >= _profile.maxQuiesceDepth) {
        return standPat;
    }

    std::vector<OctiMove> tacticalMoves;
    game->generateTacticalMoves(tacticalMoves);
    if (tacticalMoves.empty()) {
        if (useQsearchTt) {
            storeQuiesceTt(0, standPat, (standPat > origAlpha) ? 0 : 2, 0ULL);
        }
        return standPat;
    }

    OctiPlayer turn = game->getTurn();

    std::size_t qCap = (_profile.quiesceBranchCap > 0)
        ? static_cast<std::size_t>(_profile.quiesceBranchCap)
        : tacticalMoves.size();
    std::vector<OctiMove> ordered = orderAndCapMoves(
        scratchBoard, tacticalMoves, turn, ttMoveKey, qCap, _profile, false);

    int bestVal = standPat;
    std::uint64_t bestMoveKey = 0ULL;
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
                              alpha, beta, turn, nodesVisited, deadlineMs,
                              timedOut, state);
            } else {
                OctiPlayer oppPerspective = (nextTurn != PLAYER_UNDEF) ? nextTurn :
                    ((turn == PLAYER_A || turn == PLAYER_C) ? PLAYER_B : PLAYER_A);
                val = -quiesce(scratchBoard, qDepth + 1, ply + 1,
                               -beta, -alpha, oppPerspective, nodesVisited,
                               deadlineMs, timedOut, state);
            }
        }

        game->restoreSnapshot(snap);

        if (timedOut) {
            break;
        }

        if (val > bestVal) {
            bestVal = val;
            bestMoveKey = mv.structuralKey();
        }
        if (val >= beta) {
            if (useQsearchTt) {
                storeQuiesceTt(qTtDepth, val, 1, bestMoveKey);
            }
            return beta;
        }
        if (val > alpha) {
            alpha = val;
        }
    }

    if (useQsearchTt && !timedOut) {
        storeQuiesceTt(qTtDepth, alpha, (alpha > origAlpha) ? 0 : 2, bestMoveKey);
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
                           bool &timedOut,
                           SearchThreadState &state,
                           bool allowNullMove)
{
    ++nodesVisited;
    recordBatchNodeVisit(nodesVisited, _liveNodesVisited);
    if ((nodesVisited & 63ULL) == 0ULL) {
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
                           nodesVisited, deadlineMs, timedOut, state);
        }
        return evaluatePosition(scratchBoard, currentPerspective, _profile);
    }

    const bool useTtLock = (_profile.lazySmpThreads > 1);
    const bool useShallowTtMove = (_profile.usePvs ||
                                   _profile.useKillers ||
                                   _profile.useHistory ||
                                   _profile.lmrMinDepth > 0);
    std::uint64_t hashKey = computePositionHash(scratchBoard);
    std::size_t ttIdx = hashKey & _ttMask;
    std::uint64_t ttMoveKey = 0ULL;
    {
        std::unique_lock<std::mutex> lock(
            g_ttMutexes[ttIdx & (TT_MUTEX_COUNT - 1)].mtx, std::defer_lock);
        if (useTtLock) {
            lock.lock();
        }
        const TranspositionEntry &ttEntry = _tt[ttIdx];
        if (ttEntry.key == hashKey && ttEntry.depth > -128) {
            if (useShallowTtMove) {
                ttMoveKey = ttEntry.bestMoveKey;
            }
            if (ttEntry.depth >= depth) {
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
                ttMoveKey = ttEntry.bestMoveKey;
            }
        }
    }

    OctiPlayer turn = game->getTurn();
    const bool isOpenPvWindow = (_profile.usePvs && beta > alpha + 1) ||
                                (alpha > -INF_SCORE + 1 && beta > alpha + 1);

    // Null-Move Pruning (search-mode only, disabled in GAME_MODE_4 and low-pod endgames)
    if (allowNullMove &&
        _profile.nullMoveMinDepth > 0 &&
        mode != GAME_MODE_4 &&
        ply > 0 &&
        depth >= _profile.nullMoveMinDepth &&
        !isOpenPvWindow &&
        beta < WIN_SCORE - 2000 &&
        beta > -WIN_SCORE + 2000) {
        bool canNullMoveMaterial = (game->getFreePods(turn) > 0);
        if (!canNullMoveMaterial) {
            int armedPods = 0;
            int bw = scratchBoard.getBoardWidth();
            int bh = scratchBoard.getBoardHeight();
            OctiPodType turnPodType = playerToPodType(turn);
            for (int y = 0; y < bh && armedPods < 2; ++y) {
                for (int x = 0; x < bw; ++x) {
                    const OctiSquare &sq = scratchBoard.squareAt(x, y);
                    for (int pi = 0; pi < sq.getNumPods(); ++pi) {
                        const OctiPod &pod = sq.getPodAt(pi);
                        if (pod.getType() == turnPodType && pod.getNumProngs() > 0) {
                            if (++armedPods >= 2) {
                                break;
                            }
                        }
                    }
                    if (armedPods >= 2) {
                        break;
                    }
                }
            }
            canNullMoveMaterial = (armedPods >= 2);
        }

        if (canNullMoveMaterial) {
            int staticEval = evaluatePosition(scratchBoard, currentPerspective, _profile);
            if (staticEval >= beta) {
                int r = std::max(1, _profile.nullMoveReduction);
                int nullDepth = std::max(0, depth - 1 - r);
                std::uint64_t nullHash = hashKey;
                if (game->makeNullMove(&nullHash)) {
                    ++state.diag.nullMoveTries;
                    OctiPlayer oppTurn = game->getTurn();
                    int nullVal = -negamax(scratchBoard, nullDepth, ply + 1,
                                           -beta, -beta + 1, oppTurn,
                                           nodesVisited, deadlineMs, timedOut,
                                           state, false);
                    game->unmakeNullMove(&nullHash);
                    if (timedOut) {
                        return staticEval;
                    }
                    if (nullVal >= beta) {
                        ++state.diag.nullMoveVerifications;
                        int verifyVal = negamax(scratchBoard, nullDepth, ply,
                                                beta - 1, beta, currentPerspective,
                                                nodesVisited, deadlineMs, timedOut,
                                                state, false);
                        if (timedOut) {
                            return staticEval;
                        }
                        if (verifyVal >= beta) {
                            ++state.diag.nullMoveCutoffs;
                            return beta;
                        }
                    }
                }
            }
        }
    }

    std::vector<OctiMove> rawMoves = game->generateLegalMoves();
    if (rawMoves.empty()) {
        return evaluatePosition(scratchBoard, currentPerspective, _profile);
    }

    std::size_t maxBranch = (_profile.treeBranchCap > 0)
        ? static_cast<std::size_t>(_profile.treeBranchCap)
        : std::numeric_limits<std::size_t>::max();
    std::vector<OctiMove> ordered = orderAndCapMoves(
        scratchBoard, rawMoves, turn, ttMoveKey, maxBranch, _profile,
        (ply <= _profile.compositeMoveMaxPly && depth >= 2),
        (_profile.useKillers || _profile.useHistory) ? &state : static_cast<SearchThreadState *>(nullptr),
        ply);

    int origAlpha = alpha;
    int bestVal = -INF_SCORE;
    std::size_t bestIdx = 0;
    OctiGameSnapshot snap = game->captureSnapshot();

    const std::uint64_t killer0 = (_profile.useKillers && ply >= 0 && ply < MAX_SEARCH_PLY)
        ? state.killers[ply][0]
        : 0ULL;
    const std::uint64_t killer1 = (_profile.useKillers && ply >= 0 && ply < MAX_SEARCH_PLY)
        ? state.killers[ply][1]
        : 0ULL;
    const bool lmrEnabled = (_profile.lmrMinDepth > 0 &&
                             _profile.lmrMinMoveIndex > 0 &&
                             depth >= _profile.lmrMinDepth &&
                             (depth - 1) >= 2);
    const bool needMoveClass = (_profile.useKillers || _profile.useHistory || lmrEnabled);

    for (std::size_t mvIdx = 0; mvIdx < ordered.size(); ++mvIdx) {
        const OctiMove &mv = ordered[mvIdx];
        const bool mvIsTactical = needMoveClass
            ? isTacticalMoveForSearch(scratchBoard, mode, turn, mv)
            : false;
        const std::uint64_t mvKey = (lmrEnabled || (_profile.useKillers && !mvIsTactical) || (_profile.useHistory && !mvIsTactical))
            ? mv.structuralKey()
            : 0ULL;
        const bool mvIsKiller = (_profile.useKillers && mvKey != 0ULL &&
                                 (mvKey == killer0 || mvKey == killer1));

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
            int childDepth = depth - 1;
            if (sameTeamNext) {
                if (_profile.usePvs && mvIdx > 0 && beta > alpha + 1) {
                    val = negamax(scratchBoard, childDepth, ply + 1,
                                  alpha, alpha + 1, turn, nodesVisited,
                                  deadlineMs, timedOut, state, true);
                    if (!timedOut && val > alpha) {
                        ++state.diag.pvsReSearches;
                        val = negamax(scratchBoard, childDepth, ply + 1,
                                      alpha, beta, turn, nodesVisited,
                                      deadlineMs, timedOut, state, true);
                    }
                } else {
                    val = negamax(scratchBoard, childDepth, ply + 1,
                                  alpha, beta, turn, nodesVisited,
                                  deadlineMs, timedOut, state, true);
                }
            } else {
                OctiPlayer oppPerspective = (nextTurn != PLAYER_UNDEF) ? nextTurn :
                    ((turn == PLAYER_A || turn == PLAYER_C) ? PLAYER_B : PLAYER_A);

                int lmrRed = 0;
                if (lmrEnabled &&
                    mvIdx >= static_cast<std::size_t>(_profile.lmrMinMoveIndex) &&
                    (ttMoveKey == 0ULL || mvKey != ttMoveKey) &&
                    !mvIsTactical &&
                    !mvIsKiller) {
                    lmrRed = 1;
                    if (depth >= _profile.lmrMinDepth + 2 &&
                        mvIdx >= static_cast<std::size_t>(_profile.lmrMinMoveIndex) * 3) {
                        lmrRed = 2;
                    }
                    lmrRed = std::min(lmrRed, childDepth - 1);
                }

                if (lmrRed > 0) {
                    ++state.diag.lmrReductions;
                    val = -negamax(scratchBoard, childDepth - lmrRed, ply + 1,
                                   -(alpha + 1), -alpha, oppPerspective,
                                   nodesVisited, deadlineMs, timedOut, state, true);
                    if (!timedOut && val > alpha) {
                        ++state.diag.lmrReSearches;
                        if (_profile.usePvs && mvIdx > 0 && beta > alpha + 1) {
                            val = -negamax(scratchBoard, childDepth, ply + 1,
                                           -(alpha + 1), -alpha, oppPerspective,
                                           nodesVisited, deadlineMs, timedOut, state, true);
                            if (!timedOut && val > alpha) {
                                ++state.diag.pvsReSearches;
                                val = -negamax(scratchBoard, childDepth, ply + 1,
                                               -beta, -alpha, oppPerspective,
                                               nodesVisited, deadlineMs, timedOut, state, true);
                            }
                        } else {
                            val = -negamax(scratchBoard, childDepth, ply + 1,
                                           -beta, -alpha, oppPerspective,
                                           nodesVisited, deadlineMs, timedOut, state, true);
                        }
                    }
                } else if (_profile.usePvs && mvIdx > 0 && beta > alpha + 1) {
                    val = -negamax(scratchBoard, childDepth, ply + 1,
                                   -(alpha + 1), -alpha, oppPerspective,
                                   nodesVisited, deadlineMs, timedOut, state, true);
                    if (!timedOut && val > alpha) {
                        ++state.diag.pvsReSearches;
                        val = -negamax(scratchBoard, childDepth, ply + 1,
                                       -beta, -alpha, oppPerspective,
                                       nodesVisited, deadlineMs, timedOut, state, true);
                    }
                } else {
                    val = -negamax(scratchBoard, childDepth, ply + 1,
                                   -beta, -alpha, oppPerspective,
                                   nodesVisited, deadlineMs, timedOut, state, true);
                }
            }
        }

        game->restoreSnapshot(snap);

        if (timedOut) {
            break;
        }

        if (val > bestVal) {
            bestVal = val;
            bestIdx = mvIdx;
        }
        if (val > alpha) {
            alpha = val;
        }
        if (alpha >= beta) {
            if (!mvIsTactical && mvKey != 0ULL) {
                if (_profile.useKillers && ply >= 0 && ply < MAX_SEARCH_PLY) {
                    if (state.killers[ply][0] != mvKey) {
                        state.killers[ply][1] = state.killers[ply][0];
                        state.killers[ply][0] = mvKey;
                        ++state.diag.killerUpdates;
                    }
                }
                if (_profile.useHistory) {
                    int pSlot = static_cast<int>(turn);
                    if (pSlot >= 0 && pSlot < 5) {
                        int bonus = std::min(depth * depth, 400);
                        std::int32_t &hEntry =
                            state.history[pSlot][historyBucketForMoveKey(mvKey)];
                        hEntry += bonus - (hEntry * bonus) / MAX_HISTORY_VAL;
                        ++state.diag.historyUpdates;
                    }
                }
            }
            break;
        }
    }

    if (!timedOut && bestVal > -INF_SCORE) {
        const std::uint64_t bestMoveKey = ordered[bestIdx].structuralKey();
        std::unique_lock<std::mutex> lock(
            g_ttMutexes[ttIdx & (TT_MUTEX_COUNT - 1)].mtx, std::defer_lock);
        if (useTtLock) {
            lock.lock();
        }
        TranspositionEntry &ttEntry = _tt[ttIdx];
        if (ttEntry.key == 0 || depth >= static_cast<int>(ttEntry.depth) - 1) {
            const bool preserveExistingMove =
                useShallowTtMove &&
                bestVal <= origAlpha &&
                ttEntry.key == hashKey &&
                ttEntry.bestMoveKey != 0ULL;
            ttEntry.key = hashKey;
            ttEntry.depth = static_cast<std::int16_t>(depth);
            int storeScore = bestVal;
            if (storeScore >= WIN_SCORE - 1000) {
                storeScore += ply;
            } else if (storeScore <= -WIN_SCORE + 1000) {
                storeScore -= ply;
            }
            ttEntry.score = storeScore;
            if (!preserveExistingMove) {
                ttEntry.bestMoveKey = bestMoveKey;
            }
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
    ensureTranspositionTableSize();
    _lastDiagnostics = BotSearchDiagnostics();
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
        recordBatchNodeVisit(nodesVisited, _liveNodesVisited);
        try {
            game->move(mv);
            if (game->isGameOver() &&
                areAlliedPlayers(mode, game->getWinner(), rootTurn)) {
                game->restoreSnapshot(rootSnap);
                _liveNodesVisited.fetch_add(unbatchedNodeRemainder(nodesVisited),
                                            std::memory_order_relaxed);
                std::uint64_t totalNodes =
                    _liveNodesVisited.load(std::memory_order_relaxed);
                _liveDepthReached.store(1, std::memory_order_relaxed);
                _liveBestScore.store(WIN_SCORE, std::memory_order_relaxed);
                if (outStats) {
                    outStats->depthReached = 1;
                    outStats->nodesVisited = totalNodes;
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
        : std::numeric_limits<std::size_t>::max();
    std::vector<OctiMove> rootCandidates = orderAndCapMoves(
        scratchBoard, legalMoves, rootTurn, 0ULL, rootCap, _profile,
        (_profile.compositeMoveMaxPly >= 0));

    std::string bestMoveStr = rootCandidates[0].asString();
    int bestScore = -INF_SCORE;
    int maxTargetDepth = _profile.maxSearchDepth;
    int completedDepth = 0;

    std::unique_ptr<SearchThreadState> mainState = std::make_unique<SearchThreadState>();
    std::vector<std::unique_ptr<SearchThreadState>> helperStates;

    // Lazy SMP: spawn helper threads if budget >= 500ms and lazySmpThreads > 1
    _stopSearch.store(false, std::memory_order_relaxed);
    std::vector<std::thread> helpers;

    if (_timeBudgetMs >= 500 && _profile.lazySmpThreads > 1) {
        unsigned int hw = std::thread::hardware_concurrency();
        int numThreads = std::min(_profile.lazySmpThreads, static_cast<int>(hw > 0 ? hw : 2));
        if (numThreads > 1) {
            helperStates.reserve(static_cast<std::size_t>(numThreads - 1));
            for (int t = 1; t < numThreads; ++t) {
                helperStates.push_back(std::make_unique<SearchThreadState>());
            }
            const std::vector<OctiMove> initialCandidates = rootCandidates;
            const bool diversifyHelpers = _profile.lazySmpDiversifyHelpers;

            for (int t = 1; t < numThreads; ++t) {
                SearchThreadState *hState = helperStates[static_cast<std::size_t>(t - 1)].get();
                if (!diversifyHelpers) {
                    int depthOffset = (t % 2 == 1) ? 1 : 2;
                    helpers.emplace_back([this, rootFen, deadlineMs, depthOffset,
                                          maxTargetDepth, rootTurn, hState]() {
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
                                    rootTurn, tNodes, deadlineMs, tTimedOut, *hState, true);
                            if (tTimedOut || steadyClockMs() >= deadlineMs) break;
                        }
                        _liveNodesVisited.fetch_add(unbatchedNodeRemainder(tNodes),
                                                    std::memory_order_relaxed);
                    });
                } else {
                    helpers.emplace_back([this, rootFen, initialCandidates, deadlineMs,
                                          t, maxTargetDepth, rootTurn, mode, hState]() {
                        OctiBoard threadBoard;
                        threadBoard.suppressBoardUpdateNotification(true);
                        threadBoard.fromPositionString(rootFen);
                        OctiGame *tGame = threadBoard.getGame();
                        if (!tGame || initialCandidates.empty()) return;
                        tGame->setSearchMode(true);
                        OctiGameSnapshot tRootSnap = tGame->captureSnapshot();
                        std::uint64_t tNodes = 0;

                        int startD = std::min(maxTargetDepth, 1 + ((t - 1) % 4));
                        for (int d = startD;
                             d <= maxTargetDepth && !_stopSearch.load(std::memory_order_relaxed);
                             ++d) {
                            if (steadyClockMs() >= deadlineMs ||
                                _stopSearch.load(std::memory_order_relaxed)) {
                                break;
                            }
                            int tAlpha = -INF_SCORE;
                            int tBeta = INF_SCORE;
                            bool tTimedOut = false;
                            for (std::size_t s = 0;
                                 s < initialCandidates.size() &&
                                 !_stopSearch.load(std::memory_order_relaxed);
                                 ++s) {
                                if (steadyClockMs() >= deadlineMs) {
                                    tTimedOut = true;
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

                                int score = 0;
                                if (tGame->isGameOver()) {
                                    OctiPlayer w = tGame->getWinner();
                                    score = areAlliedPlayers(mode, w, rootTurn)
                                        ? (WIN_SCORE - 1)
                                        : (-WIN_SCORE + 1);
                                } else {
                                    OctiPlayer nextTurn = tGame->getTurn();
                                    bool sameTeamNext = areAlliedPlayers(mode, rootTurn, nextTurn);
                                    if (sameTeamNext) {
                                        if (_profile.usePvs && s > 0 &&
                                            tAlpha > -INF_SCORE && tBeta > tAlpha + 1) {
                                            score = negamax(threadBoard, d - 1, 1,
                                                            tAlpha, tAlpha + 1, rootTurn,
                                                            tNodes, deadlineMs, tTimedOut, *hState, true);
                                            if (!tTimedOut && score > tAlpha) {
                                                ++hState->diag.pvsReSearches;
                                                score = negamax(threadBoard, d - 1, 1,
                                                                tAlpha, tBeta, rootTurn,
                                                                tNodes, deadlineMs, tTimedOut, *hState, true);
                                            }
                                        } else {
                                            score = negamax(threadBoard, d - 1, 1,
                                                            tAlpha, tBeta, rootTurn,
                                                            tNodes, deadlineMs, tTimedOut, *hState, true);
                                        }
                                    } else {
                                        OctiPlayer oppPerspective = (nextTurn != PLAYER_UNDEF) ? nextTurn :
                                            ((rootTurn == PLAYER_A || rootTurn == PLAYER_C) ? PLAYER_B : PLAYER_A);
                                        if (_profile.usePvs && s > 0 &&
                                            tAlpha > -INF_SCORE && tBeta > tAlpha + 1) {
                                            score = -negamax(threadBoard, d - 1, 1,
                                                             -(tAlpha + 1), -tAlpha, oppPerspective,
                                                             tNodes, deadlineMs, tTimedOut, *hState, true);
                                            if (!tTimedOut && score > tAlpha) {
                                                ++hState->diag.pvsReSearches;
                                                score = -negamax(threadBoard, d - 1, 1,
                                                                 -tBeta, -tAlpha, oppPerspective,
                                                                 tNodes, deadlineMs, tTimedOut, *hState, true);
                                            }
                                        } else {
                                            score = -negamax(threadBoard, d - 1, 1,
                                                             -tBeta, -tAlpha, oppPerspective,
                                                             tNodes, deadlineMs, tTimedOut, *hState, true);
                                        }
                                    }
                                }

                                tGame->restoreSnapshot(tRootSnap);
                                if (tTimedOut) break;
                                if (score > tAlpha) {
                                    tAlpha = score;
                                }
                            }
                            if (tTimedOut || steadyClockMs() >= deadlineMs) break;
                        }
                        _liveNodesVisited.fetch_add(unbatchedNodeRemainder(tNodes),
                                                    std::memory_order_relaxed);
                    });
                }
            }
        }
    }

    auto searchSingleRootMove = [&](const OctiMove &mv,
                                    int d,
                                    std::size_t mvIdx,
                                    int alpha,
                                    int beta,
                                    bool &timedOut,
                                    int &outScore) -> bool {
        try {
            game->move(mv);
        } catch (...) {
            game->restoreSnapshot(rootSnap);
            return false;
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
            const bool rootNullWin = (_profile.usePvs &&
                                      _profile.rootScoreJitter == 0 &&
                                      mvIdx > 0 &&
                                      alpha > -INF_SCORE &&
                                      beta > alpha + 1);
            if (sameTeamNext) {
                if (rootNullWin) {
                    score = negamax(scratchBoard, d - 1, 1,
                                    alpha, alpha + 1, rootTurn, nodesVisited,
                                    deadlineMs, timedOut, *mainState, true);
                    if (!timedOut && score > alpha) {
                        ++mainState->diag.pvsReSearches;
                        score = negamax(scratchBoard, d - 1, 1,
                                        alpha, beta, rootTurn, nodesVisited,
                                        deadlineMs, timedOut, *mainState, true);
                    }
                } else {
                    score = negamax(scratchBoard, d - 1, 1,
                                    alpha, beta, rootTurn, nodesVisited,
                                    deadlineMs, timedOut, *mainState, true);
                }
            } else {
                OctiPlayer oppPerspective = (nextTurn != PLAYER_UNDEF) ? nextTurn :
                    ((rootTurn == PLAYER_A || rootTurn == PLAYER_C) ? PLAYER_B : PLAYER_A);
                if (rootNullWin) {
                    score = -negamax(scratchBoard, d - 1, 1,
                                     -(alpha + 1), -alpha, oppPerspective, nodesVisited,
                                     deadlineMs, timedOut, *mainState, true);
                    if (!timedOut && score > alpha) {
                        ++mainState->diag.pvsReSearches;
                        score = -negamax(scratchBoard, d - 1, 1,
                                         -beta, -alpha, oppPerspective, nodesVisited,
                                         deadlineMs, timedOut, *mainState, true);
                    }
                } else {
                    score = -negamax(scratchBoard, d - 1, 1,
                                     -beta, -alpha, oppPerspective, nodesVisited,
                                     deadlineMs, timedOut, *mainState, true);
                }
            }
        }

        game->restoreSnapshot(rootSnap);
        outScore = score;
        return true;
    };

    for (int d = 1; d <= maxTargetDepth; ++d) {
        bool timedOut = false;
        std::string iterBestMove = bestMoveStr;
        int iterBestScore = -INF_SCORE;

        const bool useAspiration = (d >= 2 &&
                                    _profile.aspirationWindowCp > 0 &&
                                    _profile.rootScoreJitter == 0 &&
                                    bestScore > -WIN_SCORE + 2000 &&
                                    bestScore < WIN_SCORE - 2000);

        if (!useAspiration) {
            int alpha = -INF_SCORE;
            int beta = INF_SCORE;
            for (std::size_t mvIdx = 0; mvIdx < rootCandidates.size(); ++mvIdx) {
                const OctiMove &mv = rootCandidates[mvIdx];
                int score = 0;
                if (!searchSingleRootMove(mv, d, mvIdx, alpha, beta, timedOut, score)) {
                    continue;
                }
                if (timedOut) {
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
        } else {
            int delta = _profile.aspirationWindowCp;
            int aspAlpha = std::max(-INF_SCORE, bestScore - delta);
            int aspBeta = std::min(INF_SCORE, bestScore + delta);
            int widenCount = 0;
            std::vector<OctiMove> aspCandidates = rootCandidates;

            while (true) {
                int alpha = aspAlpha;
                int beta = aspBeta;
                std::string passBestMove = aspCandidates[0].asString();
                int passBestScore = -INF_SCORE;
                std::size_t passBestIdx = 0;

                for (std::size_t mvIdx = 0; mvIdx < aspCandidates.size(); ++mvIdx) {
                    const OctiMove &mv = aspCandidates[mvIdx];
                    int score = 0;
                    if (!searchSingleRootMove(mv, d, mvIdx, alpha, beta, timedOut, score)) {
                        continue;
                    }
                    if (timedOut) {
                        break;
                    }
                    if (score > passBestScore) {
                        passBestScore = score;
                        passBestMove = mv.asString();
                        passBestIdx = mvIdx;
                    }
                    if (score > alpha) {
                        alpha = score;
                    }
                    if (alpha >= beta) {
                        break;
                    }
                }

                if (timedOut) {
                    break;
                }

                if (passBestScore <= aspAlpha && aspAlpha > -INF_SCORE) {
                    ++mainState->diag.aspirationFailLows;
                    ++mainState->diag.aspirationReSearches;
                    ++widenCount;
                    if (widenCount >= 3 || aspAlpha - delta <= -WIN_SCORE) {
                        aspAlpha = -INF_SCORE;
                    } else {
                        aspAlpha = std::max(-INF_SCORE, aspAlpha - delta);
                        delta *= 2;
                    }
                    continue;
                }

                if (passBestScore >= aspBeta && aspBeta < INF_SCORE) {
                    if (passBestIdx > 0 && passBestIdx < aspCandidates.size()) {
                        std::rotate(aspCandidates.begin(),
                                    aspCandidates.begin() + static_cast<std::ptrdiff_t>(passBestIdx),
                                    aspCandidates.begin() + static_cast<std::ptrdiff_t>(passBestIdx) + 1);
                    }
                    ++mainState->diag.aspirationFailHighs;
                    ++mainState->diag.aspirationReSearches;
                    ++widenCount;
                    if (widenCount >= 3 || aspBeta + delta >= WIN_SCORE) {
                        aspBeta = INF_SCORE;
                    } else {
                        aspBeta = std::min(INF_SCORE, aspBeta + delta);
                        delta *= 2;
                    }
                    continue;
                }

                iterBestScore = passBestScore;
                iterBestMove = passBestMove;
                rootCandidates = std::move(aspCandidates);
                break;
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
                outStats->nodesVisited =
                    _liveNodesVisited.load(std::memory_order_relaxed) +
                    unbatchedNodeRemainder(nodesVisited);
                outStats->bestScore = bestScore;
                outStats->elapsedMs = std::max(0L, steadyClockMs() - startMs);
            }
            std::uint64_t bestRootKey = OctiMove(bestMoveStr).structuralKey();
            auto it = std::find_if(rootCandidates.begin(), rootCandidates.end(),
                                   [&](const OctiMove &m) {
                                       return m.structuralKey() == bestRootKey;
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

    accumulateSearchDiagnostics(_lastDiagnostics, mainState->diag);
    for (const auto &hs : helperStates) {
        if (hs) {
            accumulateSearchDiagnostics(_lastDiagnostics, hs->diag);
        }
    }

    _liveNodesVisited.fetch_add(unbatchedNodeRemainder(nodesVisited),
                                std::memory_order_relaxed);
    std::uint64_t totalNodes =
        _liveNodesVisited.load(std::memory_order_relaxed);

    _liveDepthReached.store(std::max(1, completedDepth), std::memory_order_relaxed);
    _liveBestScore.store(bestScore, std::memory_order_relaxed);

    if (outStats) {
        outStats->depthReached = std::max(1, completedDepth);
        outStats->nodesVisited = totalNodes;
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
    ensureTranspositionTableSize();
    _lastDiagnostics = BotSearchDiagnostics();
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
            self->ensureTranspositionTableSize();
        }
    } restorer{this, savedProfile, savedBudget, savedCustom};

    int maxTargetDepth = std::max(1, searchDepth);
    int effBudgetMs = (timeBudgetMs > 0) ? std::max(10, timeBudgetMs) : 0;
    _profile.useOpeningBook = false;
    _profile.rootScoreJitter = 0;
    _profile.bookSelectionWindow = 0;
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

    std::size_t rootCap = (_profile.rootBranchCap > 0)
        ? std::max<std::size_t>(topK, static_cast<std::size_t>(_profile.rootBranchCap))
        : std::numeric_limits<std::size_t>::max();
    std::vector<OctiMove> rootCandidates = orderAndCapMoves(
        scratchBoard, legalMoves, rootTurn, 0ULL, rootCap, _profile,
        (_profile.compositeMoveMaxPly >= 0));
    if (rootCandidates.empty()) {
        return {};
    }

    OctiGameSnapshot rootSnap = game->captureSnapshot();

    std::vector<BookMoveScore> bestResults;
    std::size_t targetCount = std::min(topK, rootCandidates.size());
    bestResults.reserve(targetCount);
    for (std::size_t i = 0; i < targetCount; ++i) {
        bestResults.push_back({rootCandidates[i].asString(), 0});
    }

    int completedDepth = 0;
    int bestScore = -INF_SCORE;

    std::unique_ptr<SearchThreadState> mainState = std::make_unique<SearchThreadState>();
    std::vector<std::unique_ptr<SearchThreadState>> helperStates;

    _stopSearch.store(false, std::memory_order_relaxed);
    std::vector<std::thread> helpers;

    if ((timeBudgetMs <= 0 || effBudgetMs >= 200) && _profile.lazySmpThreads > 1) {
        unsigned int hw = std::thread::hardware_concurrency();
        int numThreads = std::min(_profile.lazySmpThreads,
                                  static_cast<int>(hw > 0 ? hw : 2));
        if (numThreads > 1) {
            helperStates.reserve(static_cast<std::size_t>(numThreads - 1));
            for (int t = 1; t < numThreads; ++t) {
                helperStates.push_back(std::make_unique<SearchThreadState>());
            }
            const std::vector<OctiMove> initialCandidates = rootCandidates;
            const bool diversifyHelpers = _profile.lazySmpDiversifyHelpers;
            for (int t = 1; t < numThreads; ++t) {
                SearchThreadState *hState = helperStates[static_cast<std::size_t>(t - 1)].get();
                helpers.emplace_back([this, rootFen, initialCandidates, deadlineMs,
                                      t, maxTargetDepth, rootTurn, mode,
                                      diversifyHelpers, hState]() {
                    OctiBoard threadBoard;
                    threadBoard.suppressBoardUpdateNotification(true);
                    threadBoard.fromPositionString(rootFen);
                    OctiGame *tGame = threadBoard.getGame();
                    if (!tGame) return;
                    tGame->setSearchMode(true);
                    OctiGameSnapshot tRootSnap = tGame->captureSnapshot();
                    std::uint64_t tNodes = 0;

                    int startD = diversifyHelpers
                        ? std::min(maxTargetDepth, 1 + ((t - 1) % 4))
                        : std::min(maxTargetDepth, 2 + ((t - 1) % 2));
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
                            if (!tGame->isGameOver() && (d > 1 || diversifyHelpers)) {
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
                                        persp, tNodes, deadlineMs, tTimedOut,
                                        *hState, true);
                            }
                            tGame->restoreSnapshot(tRootSnap);
                            if (tTimedOut) break;
                        }
                    }
                    _liveNodesVisited.fetch_add(unbatchedNodeRemainder(tNodes),
                                                std::memory_order_relaxed);
                });
            }
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
                                        nodesVisited, deadlineMs, timedOut,
                                        *mainState, true);
                        if (!timedOut && score > kthAlpha) {
                            score = negamax(scratchBoard, d - 1, 1,
                                            kthAlpha, beta, rootTurn,
                                            nodesVisited, deadlineMs, timedOut,
                                            *mainState, true);
                        }
                    } else {
                        score = negamax(scratchBoard, d - 1, 1,
                                        kthAlpha, beta, rootTurn, nodesVisited,
                                        deadlineMs, timedOut,
                                        *mainState, true);
                    }
                } else {
                    OctiPlayer oppPerspective = (nextTurn != PLAYER_UNDEF) ? nextTurn :
                        ((rootTurn == PLAYER_A || rootTurn == PLAYER_C) ? PLAYER_B : PLAYER_A);
                    if (useNullWindow) {
                        score = -negamax(scratchBoard, d - 1, 1,
                                         -(kthAlpha + 1), -kthAlpha, oppPerspective,
                                         nodesVisited, deadlineMs, timedOut,
                                         *mainState, true);
                        if (!timedOut && score > kthAlpha) {
                            score = -negamax(scratchBoard, d - 1, 1,
                                             -beta, -kthAlpha, oppPerspective,
                                             nodesVisited, deadlineMs, timedOut,
                                             *mainState, true);
                        }
                    } else {
                        score = -negamax(scratchBoard, d - 1, 1,
                                         -beta, -kthAlpha, oppPerspective,
                                         nodesVisited, deadlineMs, timedOut,
                                         *mainState, true);
                    }
                }
            }

            game->restoreSnapshot(rootSnap);

            if (timedOut) {
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
                                       static_cast<std::int32_t>(iterScored[i].score)});
            }

            bestScore = iterScored[0].score;
            completedDepth = d;
            _liveDepthReached.store(completedDepth, std::memory_order_relaxed);
            _liveBestScore.store(bestScore, std::memory_order_relaxed);
            if (outStats) {
                outStats->depthReached = completedDepth;
                outStats->nodesVisited =
                    _liveNodesVisited.load(std::memory_order_relaxed) +
                    unbatchedNodeRemainder(nodesVisited);
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

    accumulateSearchDiagnostics(_lastDiagnostics, mainState->diag);
    for (const auto &hs : helperStates) {
        if (hs) {
            accumulateSearchDiagnostics(_lastDiagnostics, hs->diag);
        }
    }

    _liveNodesVisited.fetch_add(unbatchedNodeRemainder(nodesVisited),
                                std::memory_order_relaxed);
    std::uint64_t totalNodes =
        _liveNodesVisited.load(std::memory_order_relaxed);

    _liveDepthReached.store(std::max(1, completedDepth), std::memory_order_relaxed);
    _liveBestScore.store(bestScore, std::memory_order_relaxed);

    if (outStats) {
        outStats->depthReached = std::max(1, completedDepth);
        outStats->nodesVisited = totalNodes;
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
