/*
 * OctiBotClock.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_CLOCK_HXX
#define OCTIBOT_CLOCK_HXX

#include <cstdint>
#include <map>
#include <string>
#include "OctiBotConfig.hxx"
#include "OctiNetworkClient.hxx"

namespace octi {

struct TimedClockBudgetPolicy {
    bool timedAutoPace = true;
    int timedTimeBudgetMinMs = 250;
    int timedTimeBudgetMaxMs = 60000;
    int clockSafetyMarginMs = 1500;
    int timedOpeningTaperMaxPly = 6;
    int timedOpeningTaperPct = 75;
    int timedMidgameHorizonMoves = 24;
    int timedEndgameHorizonMoves = 16;
    int timedEndgameMinPly = 40;
    int timedIncrementWeightPct = 75;
    int timedMaxBankFractionPct = 40;
    int timedEmergencyFloorMs = 200;
};

struct TimedMoveBudgetBreakdown {
    int allocatedBudgetMs = 1500;
    int hardClockCeilingMs = 1500;
    std::int64_t usableClockMs = 0;
    int horizonMoves = 24;
    bool usedDynamicClock = false;
    bool clampedByMin = false;
    bool clampedByMax = false;
    bool clampedBySafetyCeiling = false;
    bool inEmergencyTimeTrouble = false;
};

void applyConfigToTimedClockPolicy(const OctiBotConfigStore &config,
                                   TimedClockBudgetPolicy &policy);

void serializeTimedClockPolicyToMap(const TimedClockBudgetPolicy &policy,
                                    std::map<std::string, std::string> &out);

TimedMoveBudgetBreakdown computeClockManagedMoveBudget(
    const TimedClockBudgetPolicy &policy,
    int configuredUntimedBudgetMs,
    const GameViewData &game,
    std::int64_t remainingClockMs,
    int currentPly = 0,
    int extraEngineStopOverheadMs = 0);

inline int computeClockManagedMoveBudgetMs(
    const TimedClockBudgetPolicy &policy,
    int configuredUntimedBudgetMs,
    const GameViewData &game,
    std::int64_t remainingClockMs,
    int currentPly = 0,
    int extraEngineStopOverheadMs = 0)
{
    return computeClockManagedMoveBudget(
        policy, configuredUntimedBudgetMs, game, remainingClockMs,
        currentPly, extraEngineStopOverheadMs).allocatedBudgetMs;
}

std::int64_t computeEffectiveRemainingClockMs(
    const GameViewData &game,
    int mySeat,
    std::int64_t snapshotFetchedAtMs,
    std::int64_t nowAtTurnStartMs);

} // namespace octi

#endif /* OCTIBOT_CLOCK_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
