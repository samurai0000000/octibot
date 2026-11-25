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
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
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
    int compositeTacticalOnlyMinPly  = 0;     // Min ply where composite moves require jump/capture/base/battery (<= 0 = all plies full)
    bool qsearchIncludeBaseThreatProngs = false; // Include dist-1 base-entry +prong insertions in quiesce() at qDepth == 0
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
    int emptyAlliedBasePenalty1B     = 0;     // Penalty per empty uncaptured allied home square when enemy needs 1 base to win (0 = off)

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
    bool fixStackAndHomeEvalMirage   = false; // Exempt 2-pod armed batteries from stackPen, fix (4-freePods) inversion, and require armedPods <= 1 for monopoly

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

    // --- 6. Variant-Specific Multi-Mode, Geometry & Urgency Weights ---
    int ofkPodValScalePct                  = 135;   // OFK armed-pod material multiplier (%) (100 = unscaled)
    int ofkCapturedPodPenalty              = 4800;  // Penalty per captured pod in OFK
    int ofkReserveFreeProngVal             = 180;   // Value per unplaced prong in reserve in OFK
    int fourPlayerEliminatedPenalty        = 20000; // Penalty when an allied/enemy seat is eliminated in 4P
    int fourPlayerWipeoutDangerPct         = 50;    // Scale (%) of wipeoutDangerPenalty for single-seat wipeout risk in 4P
    int fourPlayerPartnerHomeBlockPenalty  = 900;   // Congestion penalty when partner occupies non-eliminated seat's home with freePods > 0
    int fourPlayerPartnerHomeBlockZeroPodsPenalty = 4500; // Escalated penalty when partner blocks home of seat with freePods > 0 and 0 board pods
    int reserveFreeSuperProngVal           = 150;   // Value per unplaced superprong in reserve
    int nonEdgelessBackRankPenalty2P       = 850;   // Penalty for pod behind own home rank in 2P non-edgeless
    int moveOrderBackRankRetreatPenalty2P  = 3500;  // Move-ordering penalty for retreating behind own home rank in 2P non-edgeless
    int multiBaseTwoNeededThreatScalePct   = 35;    // Base-threat & distance scale (%) when 2 bases are still needed to win
    int multiBaseThreeNeededThreatScalePct = 20;    // Base-threat & distance scale (%) when >= 3 bases are still needed to win
    int multiBaseTwoNeededScaleFloorPct    = 60;    // Minimum scale (%) on capturedEnemyBaseBonus / lostAlliedBasePenalty when 2 bases needed
    int multiBaseThreeNeededScaleFloorPct  = 35;    // Minimum scale (%) on capturedEnemyBaseBonus / lostAlliedBasePenalty when >= 3 bases needed
    int emptyAlliedBase1BCloseUrgencyPct   = 175;   // Urgency multiplier (%) on emptyAlliedBasePenalty1B when attacker dist <= 2
    int emptyAlliedBase1BMidUrgencyPct     = 130;   // Urgency multiplier (%) on emptyAlliedBasePenalty1B when attacker dist == 3
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

class OctiLeafEvaluator
{
public:
    virtual ~OctiLeafEvaluator() = default;

    // When false, OctiBotEngine skips evaluateBoard() heuristic computation at
    // search leaves and passes heuristicScore = 0 to evaluateLeafAtPly().
    virtual bool requiresHeuristicScore() const noexcept { return true; }

    // Invoked on the calling worker thread once when scratchBoard/threadBoard
    // is initialized at root (ply == 0) before root move search begins.
    virtual void onSearchRoot(const OctiBoard &rootBoard) const
    {
        (void)rootBoard;
    }

    // Invoked on the calling worker thread immediately before game->move(move)
    // transitions from parentPly to (parentPly + 1).
    virtual void onBeforeMove(const OctiBoard &boardBefore,
                              const OctiMove &move,
                              int parentPly) const
    {
        (void)boardBefore;
        (void)move;
        (void)parentPly;
    }

    // Invoked immediately after game->makeNullMove() transitions to childPly.
    virtual void onPushNullMove(const OctiBoard &boardAfter,
                                int childPly) const
    {
        (void)boardAfter;
        (void)childPly;
    }

    // Invoked immediately after restoreSnapshot() or unmakeNullMove() restores
    // the board back to restoredPly (also invoked via RAII if move() throws).
    virtual void onPopPly(int restoredPly) const
    {
        (void)restoredPly;
    }

    virtual int evaluateLeaf(const OctiBoard &board,
                             OctiPlayer perspective,
                             int heuristicScore) const = 0;

    virtual int evaluateLeafAtPly(const OctiBoard &board,
                                  OctiPlayer perspective,
                                  int heuristicScore,
                                  int ply) const
    {
        (void)ply;
        return evaluateLeaf(board, perspective, heuristicScore);
    }
};

class OctiBotEngine {
public:
    class SearchThreadAffinityScope {
    public:
        virtual ~SearchThreadAffinityScope() = default;
    };

    explicit OctiBotEngine(BotDifficulty difficulty = BotDifficulty::TACTICAL,
                           std::uint64_t rngSeed = 0x5EED2026ULL);
    OctiBotEngine(const OctiBotEngine &other);
    OctiBotEngine &operator=(const OctiBotEngine &other);
    virtual ~OctiBotEngine();

    virtual void resetSearchState(std::uint64_t rngSeed = 0x5EED2026ULL);

    void setDifficulty(BotDifficulty d);
    BotDifficulty difficulty() const;

    void setTuningProfile(const BotTuningProfile &profile);
    const BotTuningProfile &tuningProfile() const;
    static BotTuningProfile defaultProfileForDifficulty(BotDifficulty difficulty);

    void setLeafEvaluator(const OctiLeafEvaluator *evaluator);
    const OctiLeafEvaluator *leafEvaluator() const;

    void setTimeBudgetMs(int maxTimeMs);
    int timeBudgetMs() const;

    virtual int liveDepthReached() const;
    virtual std::uint64_t liveNodesVisited() const;
    virtual int liveBestScore() const;

    virtual std::string chooseMove(const OctiBoard &board,
                                   BotSearchStats *outStats = nullptr);

    virtual std::vector<BookMoveScore> searchTopMovesForBook(
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

    OctiEvalBreakdown evaluateBoardBreakdown(const OctiBoard &board,
                                             OctiPlayer perspective) const;
    OctiEvalBreakdown evaluateBoardBreakdown(const OctiBoard &board,
                                             OctiPlayer perspective,
                                             const BotTuningProfile &profile) const;
    OctiEvalBreakdown evaluateBoardBreakdown(const OctiGame &game,
                                             OctiPlayer perspective) const;
    OctiEvalBreakdown evaluateBoardBreakdown(const OctiGame &game,
                                             OctiPlayer perspective,
                                             const BotTuningProfile &profile) const;
    virtual int evaluateBoard(const OctiBoard &board,
                              OctiPlayer perspective) const;
    virtual int evaluateBoard(const OctiBoard &board,
                              OctiPlayer perspective,
                              const BotTuningProfile &profile) const;
    int evaluateBoard(const OctiGame &game,
                      OctiPlayer perspective) const;
    int evaluateBoard(const OctiGame &game,
                      OctiPlayer perspective,
                      const BotTuningProfile &profile) const;

    static int defaultBudgetForDifficulty(BotDifficulty d);
    static int maxDepthForDifficulty(BotDifficulty d);
    static int rootCapForDifficulty(BotDifficulty d);
    static int branchCapForDifficulty(BotDifficulty d);

    std::uint64_t lookupTranspositionBestMoveKey(const OctiBoard &board) const;
    std::size_t transpositionTableEntryCount() const noexcept { return _tt.size(); }
    BotSearchDiagnostics lastSearchDiagnostics() const noexcept { return _lastDiagnostics; }

protected:
    struct CoalitionPhaseContext {
        int basesNeeded = 1;
        int availPods = 0;
        int capturedPods = 0;
        bool feasible = true;
        int baseThreatScalePct = 100;
    };

    struct EvalPhaseSummary {
        CoalitionPhaseContext allyPhase{};
        CoalitionPhaseContext enemyPhase{};
        std::vector<std::pair<int, int>> allyTargets;
        std::vector<std::pair<int, int>> enemyTargets;
        std::vector<std::pair<int, int>> allyPodCoords;
        std::vector<std::pair<int, int>> enemyPodCoords;
        std::vector<std::pair<int, int>> emptyUncapturedAlliedBases;
        std::vector<std::pair<int, int>> emptyUncapturedEnemyBases;
        int allyOneStepCount = 0;
        int allyOneJumpCount = 0;
        int enemyOneStepCount = 0;
        int enemyOneJumpCount = 0;
        int allyTwoPlyCount = 0;
        int enemyTwoPlyCount = 0;
        int totalAllyPodsOnBoard = 0;
        int totalEnemyPodsOnBoard = 0;
        int allyArmedPods = 0;
        int enemyArmedPods = 0;
        bool enemyEmptyBaseDanger = false;
        bool enemyContests = false;
        std::uint64_t allyJumpThreatMask[2] = {0ULL, 0ULL};
        std::uint64_t enemyJumpThreatMask[2] = {0ULL, 0ULL};
    };

    virtual void generateSearchMoves(const OctiGame &game,
                                     OctiPlayer turn,
                                     int ply,
                                     std::vector<OctiMove> &outMoves) const;

    virtual void appendExtraQuiesceMoves(const OctiBoard &board,
                                         const OctiGame &game,
                                         OctiGameMode mode,
                                         OctiPlayer turn,
                                         int qDepth,
                                         std::vector<OctiMove> &tacticalMoves) const;

    virtual bool hasCustomMoveOrdering() const noexcept { return false; }

    virtual void adjustMoveOrderScores(
        const OctiBoard &board,
        const OctiGame &game,
        OctiPlayer turn,
        int ply,
        bool inQuiesce,
        const std::vector<OctiMove> &moves,
        std::vector<std::pair<int, std::size_t>> &scored) const;

    virtual void adjustEvalBreakdown(const OctiBoard &board,
                                     OctiPlayer perspective,
                                     const EvalPhaseSummary &summary,
                                     OctiEvalBreakdown &bd) const;

    virtual int adjustLmrReduction(const OctiBoard &board,
                                   OctiGameMode mode,
                                   OctiPlayer turn,
                                   int depth,
                                   int ply,
                                   std::size_t mvIdx,
                                   const OctiMove &mv,
                                   bool mvIsTactical,
                                   bool mvIsKiller,
                                   int defaultReduction) const;

    virtual int effectiveMaxSearchThreads(int requestedThreads,
                                          unsigned int hwConcurrency) const;

    virtual std::unique_ptr<SearchThreadAffinityScope> enterSearchThreadAffinity(
        int threadIdx,
        int totalThreads) const;

    static bool isEnemyHomeSquareFor(OctiSquareType sqType,
                                     OctiGameMode mode,
                                     OctiPlayer p);
    static bool isAlliedHomeSquareFor(OctiSquareType sqType,
                                      OctiGameMode mode,
                                      OctiPlayer p);
    static bool squareHasAllyOf(const OctiSquare &sq,
                                OctiGameMode mode,
                                OctiPlayer player);
    static int torusDist(int a, int b, int dim, bool edgeless);
    static int chebyshevDist(int x1, int y1, int x2, int y2,
                             int w, int h, bool edgeless);
    static int minChebyshevToTargets(
        int x, int y,
        const std::vector<std::pair<int, int>> &targets,
        int w, int h, bool edgeless);
    static std::vector<std::pair<int, int>> targetEnemyBases(
        const OctiBoard &board, OctiPlayer p);
    static bool isDistOneBaseThreatProngTarget(const OctiBoard &board,
                                               OctiGameMode mode,
                                               OctiPlayer turn,
                                               bool edgeless,
                                               int w,
                                               int h,
                                               int x0,
                                               int y0,
                                               OctiDir dir);
    static bool isTacticalMoveForSearch(const OctiBoard &board,
                                        OctiGameMode mode,
                                        OctiPlayer turn,
                                        const OctiMove &mv);
    static CoalitionPhaseContext analyzeCoalitionPhase(
        const OctiBoard &board,
        OctiPlayer side,
        const BotTuningProfile &profile);

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
    static OctiEvalBreakdown evaluatePositionBreakdownImpl(
        const OctiBoard &board,
        OctiPlayer perspective,
        const BotTuningProfile &profile,
        const OctiBotEngine *engine);

    template <typename ThreadStatePtr = std::nullptr_t>
    std::vector<OctiMove> orderAndCapMoves(OctiBoard &board,
                                           std::vector<OctiMove> &rawMoves,
                                           OctiPlayer turn,
                                           std::uint64_t ttBestMoveKey,
                                           std::size_t maxBranch,
                                           const BotTuningProfile &profile,
                                           bool includeComposites = true,
                                           ThreadStatePtr threadState = nullptr,
                                           int ply = 0,
                                           bool inQuiesce = false) const;

    void ensureTranspositionTableSize();
    std::uint64_t nextRandom();
    std::uint64_t computePositionHash(const OctiBoard &board) const;

    int evaluateLeafOrHeuristic(const OctiBoard &board,
                                OctiPlayer perspective,
                                int ply = 0) const
    {
        if (_leafEvaluator != nullptr) {
            const int h = _leafEvaluator->requiresHeuristicScore()
                ? evaluateBoard(board, perspective, _profile)
                : 0;
            return _leafEvaluator->evaluateLeafAtPly(board, perspective, h, ply);
        }
        return evaluateBoard(board, perspective, _profile);
    }

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
    const OctiLeafEvaluator *_leafEvaluator = nullptr;
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
