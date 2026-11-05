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
#include <vector>
#include "OctiBoard.hxx"
#include "OctiDefs.hxx"
#include "OctiGame.hxx"
#include "OctiMove.hxx"

namespace octi {

enum class BotDifficulty {
    CASUAL   = 1,
    TACTICAL = 2,
    MASTER   = 3,
};

struct BotSearchStats {
    int depthReached = 0;
    std::uint64_t nodesVisited = 0;
    int bestScore = 0;
    long elapsedMs = 0;
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
    std::int16_t score = 0;
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
    bool useOpeningBook              = true;  // Probe precomputed Zobrist opening playbook before live search

    // --- 2. Base Occupation & Distance Weights ---
    int capturedEnemyBaseBonus       = 28000; // Bonus per captured enemy home square (3-base / 4P)
    int lostAlliedBasePenalty        = 32000; // Penalty per lost allied home square
    int neutralOctiSquareBonus       = 4000;  // Bonus for occupying neutral OCTI square
    int closestPodDistWeight         = 320;   // Weight on (30 - d_min1) closest armed pod distance to bases
    int secondClosestPodDistWeight   = 140;   // Weight on (15 - d_min2) 2nd-closest armed pod distance (0 = off)
    int oneStepBaseThreatBonus       = 8500;  // Bonus when a pod has prong to step into enemy base next turn
    int oneJumpBaseThreatBonus       = 12000; // Bonus when a pod has prong + pivot to jump into enemy base
    int twoPlyBaseThreatWeight       = 2400;  // Bonus when a pod is at dist 1 from enemy base & needs 1 prong

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
    int loneOneProngRushPenalty      = 550;   // Penalty when a lone 1-prong pod advances past midline without backup

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

    static int evaluatePosition(const OctiBoard &board,
                                OctiPlayer perspective,
                                const BotTuningProfile &profile);
    static int evaluatePosition(const OctiBoard &board,
                                OctiPlayer perspective);

    static int defaultBudgetForDifficulty(BotDifficulty d);
    static int maxDepthForDifficulty(BotDifficulty d);
    static int rootCapForDifficulty(BotDifficulty d);
    static int branchCapForDifficulty(BotDifficulty d);

private:
    struct TranspositionEntry {
        std::uint64_t key = 0;
        int depth = -1;
        int score = 0;
        int flag = 0; // 0 = exact, 1 = lowerbound, 2 = upperbound
        std::string bestMove;
    };

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
                bool &timedOut);

    int negamax(OctiBoard &scratchBoard,
                int depth,
                int ply,
                int alpha,
                int beta,
                OctiPlayer currentPerspective,
                std::uint64_t &nodesVisited,
                long deadlineMs,
                bool &timedOut);

    BotDifficulty _difficulty;
    BotTuningProfile _profile;
    int _timeBudgetMs;
    bool _customTimeBudget;
    std::uint64_t _rngState;
    std::vector<TranspositionEntry> _tt;
    std::atomic<bool> _stopSearch{false};
    std::atomic<int> _liveDepthReached{0};
    std::atomic<std::uint64_t> _liveNodesVisited{0};
    std::atomic<int> _liveBestScore{0};
};

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
