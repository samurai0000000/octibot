/*
 * OctiBotEngine.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_BOT_ENGINE_HXX
#define OCTI_BOT_ENGINE_HXX

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
#include "OctiBoard.hxx"
#include "OctiDefs.hxx"
#include "OctiGame.hxx"
#include "OctiMove.hxx"

namespace octi {

enum class BotDifficulty {
    CASUAL      = 1, // ~5000ms budget, shallow tactical horizon, slight randomness
    TACTICAL    = 2, // ~10000ms budget, full iterative deepening + threat penalties
    MASTER      = 3, // ~20000ms budget, deep principal variation + capture quiescence
    EXPERT      = 4, // ~40000ms budget, extended search horizon + wider branch caps
    GRANDMASTER = 5  // ~60000ms budget, maximum search horizon + deepest quiescence
};

struct BotSearchStats {
    int depthReached = 0;
    std::uint64_t nodesVisited = 0;
    int bestScore = 0;
    long elapsedMs = 0;
};

struct BotSearchDiagnostics {
    std::uint64_t pvsReSearches = 0;
    std::uint64_t aspirationReSearches = 0;
    std::uint64_t aspirationFailLows = 0;
    std::uint64_t aspirationFailHighs = 0;
    std::uint64_t killerUpdates = 0;
    std::uint64_t killerHits = 0;
    std::uint64_t historyUpdates = 0;
    std::uint64_t lmrReductions = 0;
    std::uint64_t lmrReSearches = 0;
    std::uint64_t nullMoveTries = 0;
    std::uint64_t nullMoveVerifications = 0;
    std::uint64_t nullMoveCutoffs = 0;
    std::uint64_t qsearchTtProbes = 0;
    std::uint64_t qsearchTtHits = 0;
};

struct OpeningBookCandidate {
    const char *move;
    std::int16_t score;
};

struct OpeningBookEntry {
    std::uint64_t canonicalKey;
    std::uint8_t depth;
    std::uint8_t numCandidates;
    OpeningBookCandidate candidates[3];
};

struct BookMoveScore {
    std::string move;
    std::int32_t score = 0;
};

struct BotTuningProfile {
    // --- 1. Search Horizon, Quiescence & Concurrency ---
    int timeBudgetMs                 = 10000; // Search wall-clock budget (ms)
    int maxSearchDepth               = 64;    // Iterative deepening max depth (plies)
    int maxQuiesceDepth              = 2;     // Capture/base-win extension plies at depth <= 0 (0 = off)
    int quiesceBranchCap             = 6;     // Max tactical moves searched per quiescence node
    int rootBranchCap                = 36;    // Max candidate moves kept at root
    int treeBranchCap                = 24;    // Max candidate moves kept at internal nodes
    int lazySmpThreads               = 2;     // Total search threads (1 = single-thread, 4 = Lazy SMP)
    int compositeMoveMaxPly          = 1;     // Max search ply to generate multi-pod composite moves (-1 = off)
    int rootScoreJitter              = 25;    // Random root score noise [-jitter, +jitter] (0 = deterministic)
    int bookSelectionWindow          = 0;     // Opening book candidate score window (0 = deterministic top move)
    bool useOpeningBook              = true;  // Probe precomputed Zobrist opening playbook before live search
    bool usePvs                      = false; // Principal Variation Search null-window + re-search
    int aspirationWindowCp           = 0;     // Root aspiration half-window in eval units (<= 0 = off)
    bool useKillers                  = false; // Per-thread 2-slot killer move heuristic per ply
    bool useHistory                  = false; // Per-thread quiet beta-cutoff history heuristic
    int lmrMinDepth                  = 0;     // Min search depth for Late-Move Reductions (<= 0 = off)
    int lmrMinMoveIndex              = 0;     // Min 0-based move index for LMR (<= 0 = off)
    int nullMoveMinDepth             = 0;     // Min search depth for Null-Move Pruning (<= 0 = off)
    int nullMoveReduction            = 2;     // Depth reduction R in null-move search (depth - 1 - R)
    bool qsearchUseTt                = false; // Probe & store transposition table in quiescence search
    int ttSizeLog2                   = 18;    // Transposition table size = 1 << ttSizeLog2 (clamped 12..26)
    bool lazySmpDiversifyHelpers     = false; // Rotate helper root moves by t and stagger depth 1+((t-1)%4)

    // --- 2. Base Occupation & Distance Weights ---
    int capturedEnemyBaseBonus       = 28000; // Bonus per captured enemy home square (3-base / 4P)
    int lostAlliedBasePenalty        = 32000; // Penalty per lost allied home square
    int neutralOctiSquareBonus       = 4000;  // Bonus for occupying neutral OCTI square
    int closestPodDistWeight         = 320;   // Weight on (30 - d_min1) closest armed pod distance to bases
    int secondClosestPodDistWeight   = 140;   // Weight on (15 - d_min2) 2nd-closest armed pod distance (0 = off)
    int oneStepBaseThreatBonus       = 8500;  // Bonus when a pod has prong to step into enemy base next turn
    int oneJumpBaseThreatBonus       = 12000; // Bonus when a pod has prong + pivot to jump into enemy base
    int twoPlyBaseThreatWeight       = 2400;  // Bonus when a pod is at dist 1 from enemy base & needs 1 prong
    int contestedEnemyBaseThreatScalePct = 100; // Scale (%) on friendly base-threat bonuses when enemy contests (100 = flat)

    // --- 3. Multi-Pod Development & Anti-Monopoly Weights ---
    int podZeroProngVal              = 320;   // Base value of an unarmed pod on the board
    int podOneProngVal               = 1350;  // Base value of a 1-prong pod
    int podTwoProngVal               = 2100;  // Base value of a 2-prong pod
    int podThreePlusProngVal         = 2800;  // Base value of a 3+ prong pod
    int normalProngVal               = 380;   // Value per attached normal prong
    int superProngVal                = 550;   // Value per attached superprong
    int orthDiagVersatilityBonus     = 550;   // Bonus when pod has both orthogonal and diagonal prongs
    int diagOnlyParityPenalty        = 350;   // Penalty when pod has >= 2 diagonal prongs and 0 orthogonal
    int firstArmedPodBonus           = 600;   // Army development bonus for having >= 1 armed pod
    int secondArmedPodBonus          = 1100;  // Additional bonus for arming a 2nd pod
    int thirdArmedPodBonus           = 700;   // Additional bonus for arming a 3rd pod
    int singlePodMonopolyPenalty     = 1000;  // Penalty per prong > 2 on a lone pod while >= 2 home pods have 0 prongs
    int ofkSinglePodMonopolyPenalty  = -1;    // OFK override for singlePodMonopolyPenalty (< 0 = use singlePodMonopolyPenalty)
    int loneOneProngRushPenalty      = 550;   // Penalty when a lone 1-prong pod advances past midline without backup
    int enemyHeavyPodThreatPenalty   = 0;     // Penalty per prong >= 4 on an enemy pod (0 = off)
    int unarmedHomePodPenalty        = 900;   // Penalty per unarmed home pod when >= 2 board pods and >= 6 free prongs
    int canLiberateBonus             = 450;   // Bonus when side can liberate a captured pod
    int reserveFreePodVal            = 450;   // Value per unplaced pod in reserve
    int capturedPodPenalty           = 3800;  // Penalty per captured pod in standard 2P/4P
    int reserveFreeProngVal          = 140;   // Value per unplaced prong in reserve in standard 2P/4P

    // --- 4. Leapfrog / Launchpad Battery & Jump Reachability Weights ---
    int leapfrogBatteryWeight        = 550;   // Bonus for friendly pod + adjacent friendly pivot along a prong dir
    int leapfrogForwardBonus         = 400;   // Extra bonus when the leapfrog jump advances closer to enemy bases
    int jumpReachableSquareWeight    = 130;   // Bonus per unique empty square reachable via jump chain
    int jumpReachStepWeight          = 450;   // Bonus per Chebyshev step closer to enemy base achieved via jump
    int jumpCapturablePodWeight      = 600;   // Bonus per enemy pod currently in jump-capture reach
    int jumpCapturableProngWeight    = 120;   // Bonus per enemy prong currently in jump-capture reach

    // --- 5. Hanging-Pod Safety, OFK Prong-Seizure & Move Ordering ---
    int hangingPodBasePenalty        = 1400;  // Base penalty per pod sitting in enemy jump-capture mask
    int hangingProngWeight           = 220;   // Per-prong hanging penalty in standard 2P/4P
    int ofkProngSeizureWeight        = 450;   // Per-prong hanging penalty in OFK (accounts for 2x seizure swing)
    int opponentTurnHangingPct       = 125;   // Multiplier (%) on hanging penalty when opponent moves next (100 = flat)
    int soleArmedPodDangerPenalty    = 3500;  // Extra penalty when your ONLY armed pod sits in enemy jump mask
    int wipeoutDangerPenalty         = 15000; // Extra penalty when all remaining board pods can be captured
    int moveOrderCaptureBonus        = 16000; // Move-ordering priority for jump-captures
    int moveOrderBaseEntryBonus      = 60000; // Move-ordering priority for stepping/jumping onto enemy base
    int moveOrderLeapfrogBonus       = 1200;  // Move-ordering priority for forming/using a leapfrog battery
    int moveOrderBlunderPenalty      = 7000;  // Move-ordering penalty for landing in enemy jump-capture mask
};

struct OctiEvalBreakdown {
    int terminalScore                   = 0;
    int capturedEnemyBases              = 0;
    int lostAlliedBases                 = 0;
    int neutralOctiSquares              = 0;
    int stackAndCapturedBaseHolding     = 0;
    int podMaterial                     = 0;
    int podAdvanceDistance              = 0;
    int loneOneProngRush                = 0;
    int prongMaterial                   = 0;
    int prongDirectionality             = 0;
    int leapfrogBattery                 = 0;
    int structuralPositional            = 0;
    int orthDiagVersatility             = 0;
    int oneStepBaseThreat               = 0;
    int oneJumpBaseThreat               = 0;
    int twoPlyBaseThreat                = 0;
    int jumpReachability                = 0;
    int jumpCaptureThreats              = 0;
    int armyDevelopment                 = 0;
    int singlePodMonopoly               = 0;
    int overArmAndBackRank              = 0;
    int heavyPodThreat                  = 0;
    int hangingVulnerability            = 0;
    int closestBaseDistance             = 0;
    int reserveAndCapturedMaterial      = 0;
    int undeployedAndUnarmedHomePenalty = 0;
    int liberationBonus                 = 0;
    int totalScore                      = 0;

    int sumTerms() const noexcept
    {
        return terminalScore
             + capturedEnemyBases
             + lostAlliedBases
             + neutralOctiSquares
             + stackAndCapturedBaseHolding
             + podMaterial
             + podAdvanceDistance
             + loneOneProngRush
             + prongMaterial
             + prongDirectionality
             + leapfrogBattery
             + structuralPositional
             + orthDiagVersatility
             + oneStepBaseThreat
             + oneJumpBaseThreat
             + twoPlyBaseThreat
             + jumpReachability
             + jumpCaptureThreats
             + armyDevelopment
             + singlePodMonopoly
             + overArmAndBackRank
             + heavyPodThreat
             + hangingVulnerability
             + closestBaseDistance
             + reserveAndCapturedMaterial
             + undeployedAndUnarmedHomePenalty
             + liberationBonus;
    }
};

class OctiBotEngine {
public:
    explicit OctiBotEngine(BotDifficulty difficulty = BotDifficulty::TACTICAL,
                           std::uint64_t rngSeed = 0x5EED2026ULL);
    OctiBotEngine(const OctiBotEngine &other);
    OctiBotEngine &operator=(const OctiBotEngine &other);
    ~OctiBotEngine();

    void setDifficulty(BotDifficulty d);
    BotDifficulty difficulty() const;

    void setTuningProfile(const BotTuningProfile &profile);
    const BotTuningProfile &tuningProfile() const;
    static BotTuningProfile defaultProfileForDifficulty(BotDifficulty difficulty);

    void setTimeBudgetMs(int maxTimeMs);
    int timeBudgetMs() const;

    int liveDepthReached() const;
    std::uint64_t liveNodesVisited() const;
    int liveBestScore() const;

    std::string chooseMove(const OctiBoard &board,
                           BotSearchStats *outStats = nullptr);

    std::vector<BookMoveScore> searchTopMovesForBook(
        const OctiBoard &board,
        int searchDepth,
        int timeBudgetMs = 0,
        std::size_t topK = 3,
        BotSearchStats *outStats = nullptr);

    static std::uint64_t computePositionHashStatic(const OctiBoard &board);
    static std::uint64_t toggleTurnHashStatic(std::uint64_t hash,
                                              OctiPlayer fromTurn,
                                              OctiPlayer toTurn) noexcept;
    static std::uint64_t computeCanonicalOpeningHash(const OctiBoard &board,
                                                     bool *outMirrored = nullptr);
    static std::string mirrorMoveNotation(const std::string &moveNotation,
                                          int boardWidth);
    bool probeOpeningBook(const OctiBoard &board,
                          std::string &outMove,
                          int *outScore = nullptr,
                          int *outDepth = nullptr);
    static bool probeOpeningBookTable(const OctiBoard &board,
                                      const OpeningBookEntry *entries,
                                      std::size_t entryCount,
                                      const BotTuningProfile &profile,
                                      std::uint64_t &rngState,
                                      std::string &outMove,
                                      int *outScore = nullptr,
                                      int *outDepth = nullptr);

    static OctiEvalBreakdown evaluatePositionBreakdown(const OctiBoard &board,
                                                       OctiPlayer perspective,
                                                       const BotTuningProfile &profile);
    static OctiEvalBreakdown evaluatePositionBreakdown(const OctiBoard &board,
                                                       OctiPlayer perspective,
                                                       const BotTuningProfile *profile = nullptr);
    static OctiEvalBreakdown evaluatePositionBreakdown(const OctiGame &game,
                                                       OctiPlayer perspective,
                                                       const BotTuningProfile &profile);
    static OctiEvalBreakdown evaluatePositionBreakdown(const OctiGame &game,
                                                       OctiPlayer perspective,
                                                       const BotTuningProfile *profile = nullptr);

    static int evaluatePosition(const OctiBoard &board,
                                OctiPlayer perspective,
                                const BotTuningProfile &profile);
    static int evaluatePosition(const OctiBoard &board,
                                OctiPlayer perspective);
    static int evaluatePosition(const OctiGame &game,
                                OctiPlayer perspective,
                                const BotTuningProfile &profile);
    static int evaluatePosition(const OctiGame &game,
                                OctiPlayer perspective);

    static int defaultBudgetForDifficulty(BotDifficulty d);
    static int maxDepthForDifficulty(BotDifficulty d);
    static int rootCapForDifficulty(BotDifficulty d);
    static int branchCapForDifficulty(BotDifficulty d);

    std::uint64_t lookupTranspositionBestMoveKey(const OctiBoard &board) const;
    std::size_t transpositionTableEntryCount() const noexcept { return _tt.size(); }
    BotSearchDiagnostics lastSearchDiagnostics() const noexcept { return _lastDiagnostics; }

private:
    struct SearchThreadState;

    struct TranspositionEntry {
        std::uint64_t key = 0;
        std::uint64_t bestMoveKey = 0;
        std::int32_t score = 0;
        std::int16_t depth = -128;
        std::int8_t flag = 0; // 0 = exact, 1 = lowerbound, 2 = upperbound
        std::uint8_t pad = 0;
    };
    static_assert(std::is_trivially_copyable<TranspositionEntry>::value,
                  "TranspositionEntry must be trivially copyable");

public:
    static constexpr std::size_t transpositionEntrySize() noexcept
    {
        return sizeof(TranspositionEntry);
    }

private:

    void ensureTranspositionTableSize();
    std::uint64_t nextRandom();
    std::uint64_t computePositionHash(const OctiBoard &board) const;

    int quiesce(OctiBoard &scratchBoard,
                int qDepth,
                int ply,
                int alpha,
                int beta,
                OctiPlayer currentPerspective,
                std::uint64_t &nodesVisited,
                long deadlineMs,
                bool &timedOut,
                SearchThreadState &state);

    int negamax(OctiBoard &scratchBoard,
                int depth,
                int ply,
                int alpha,
                int beta,
                OctiPlayer currentPerspective,
                std::uint64_t &nodesVisited,
                long deadlineMs,
                bool &timedOut,
                SearchThreadState &state,
                bool allowNullMove = true);

    BotDifficulty _difficulty;
    BotTuningProfile _profile;
    int _timeBudgetMs;
    bool _customTimeBudget;
    std::uint64_t _rngState;
    int _ttSizeLog2;
    std::size_t _ttMask;
    std::vector<TranspositionEntry> _tt;
    BotSearchDiagnostics _lastDiagnostics;
    std::atomic<bool> _stopSearch{false};
    std::atomic<int> _liveDepthReached{0};
    std::atomic<std::uint64_t> _liveNodesVisited{0};
    std::atomic<int> _liveBestScore{0};
};

std::vector<OctiMove> filterDominatedMoves(const OctiBoard &board,
                                          OctiPlayer turn,
                                          const std::vector<OctiMove> &moves);
std::vector<OctiMove> filterDominatedMoves(const OctiBoard &board,
                                          const std::vector<OctiMove> &moves);
std::vector<OctiMove> filterDominatedMoves(const std::vector<OctiMove> &moves);

} // namespace octi

#endif // OCTI_BOT_ENGINE_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
