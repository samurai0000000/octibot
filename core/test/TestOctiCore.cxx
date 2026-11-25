/*
 * TestOctiCore.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiCore.hxx"
#include "OctiDefs.hxx"
#include "OctiExceptions.hxx"
#include "OctiMove.hxx"
#include "OctiPod.hxx"
#include "OctiSquare.hxx"
#include "OctiBoard.hxx"
#include "OctiGame.hxx"
#include "OctiBotEngine.hxx"
#include "OctiOpeningBookData.hxx"
#include "OctiCrypto.hxx"
#include "OctiNetworkClient.hxx"
#include "OctiRulebook.hxx"
#include "OctiSoundCue.hxx"
#include "Version.hxx"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <httplib.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <rapidcheck.h>
#include <initializer_list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>
#include <CppUTest/CommandLineTestRunner.h>
#include <CppUTest/TestHarness.h>

TEST_GROUP(OctiCoreTest)
{
};

TEST(OctiCoreTest, VersionAndBuildBanner)
{
    octi::OctiCore core;
    (void)core;
    STRCMP_EQUAL(OCTI_VERSION_STRING, octi::OctiCore::versionString());

    std::string banner = octi::OctiCore::buildBanner("octi_core");
    CHECK_TRUE(banner.find("octi_core version ") != std::string::npos);
    CHECK_TRUE(banner.find(OCTI_VERSION_STRING) != std::string::npos);

    std::string fallbackBanner = octi::OctiCore::buildBanner(nullptr);
    CHECK_TRUE(fallbackBanner.find("octi version ") != std::string::npos);
}

TEST_GROUP(OctiPodTest)
{
};

TEST(OctiPodTest, DefaultConstructionAndClear)
{
    octi::OctiPod pod;
    CHECK_EQUAL(octi::POD_UNDEF, pod.getType());
    CHECK_EQUAL(0, pod.getNumProngs());
    CHECK_EQUAL(0, pod.getNumNormalProngs());
    CHECK_EQUAL(0, pod.getNumSuperProngs());
    CHECK_FALSE(pod.hasSuperProng());
    STRCMP_EQUAL("", pod.getConfigString().c_str());

    pod.setType(octi::A_POD);
    pod.addProng(octi::DIR_N, false);
    CHECK_EQUAL(1, pod.getNumProngs());

    pod.clear();
    CHECK_EQUAL(octi::POD_UNDEF, pod.getType());
    CHECK_EQUAL(0, pod.getNumProngs());
    STRCMP_EQUAL("", pod.getConfigString().c_str());
}

TEST(OctiPodTest, OwnerTypeManipulation)
{
    octi::OctiPod pod(octi::B_POD);
    CHECK_EQUAL(octi::B_POD, pod.getType());

    pod.setType(octi::C_POD);
    CHECK_EQUAL(octi::C_POD, pod.getType());

    pod.setType(octi::D_POD);
    CHECK_EQUAL(octi::D_POD, pod.getType());

    pod.setType(octi::A_POD);
    CHECK_EQUAL(octi::A_POD, pod.getType());
}

TEST(OctiPodTest, ProngAddRemoveAndInspection)
{
    octi::OctiPod pod(octi::A_POD);

    pod.addProng(octi::DIR_N, false);
    pod.addProng(octi::DIR_E, false);
    pod.addProng(octi::DIR_S, false);

    CHECK_EQUAL(octi::NORMAL_PRONG, pod.prongAt(octi::DIR_N));
    CHECK_EQUAL(octi::NO_PRONG, pod.prongAt(octi::DIR_NE));
    CHECK_EQUAL(octi::NORMAL_PRONG, pod.prongAt(octi::DIR_E));
    CHECK_EQUAL(octi::NORMAL_PRONG, pod.prongAt(octi::DIR_S));
    CHECK_EQUAL(3, pod.getNumProngs());
    CHECK_EQUAL(3, pod.getNumNormalProngs());
    CHECK_EQUAL(0, pod.getNumSuperProngs());
    STRCMP_EQUAL("ace", pod.getConfigString().c_str());

    pod.removeProng(octi::DIR_E);
    CHECK_EQUAL(octi::NO_PRONG, pod.prongAt(octi::DIR_E));
    CHECK_EQUAL(2, pod.getNumProngs());
    STRCMP_EQUAL("ae", pod.getConfigString().c_str());
}

TEST(OctiPodTest, SuperProngHandling)
{
    octi::OctiPod pod(octi::B_POD);

    pod.addProng(octi::DIR_N, false);
    pod.addProng(octi::DIR_E, true); // superprong at East
    CHECK_TRUE(pod.hasSuperProng());
    CHECK_EQUAL(octi::SUPER_PRONG, pod.prongAt(octi::DIR_E));
    CHECK_EQUAL(2, pod.getNumProngs());
    CHECK_EQUAL(1, pod.getNumNormalProngs());
    CHECK_EQUAL(1, pod.getNumSuperProngs());
    STRCMP_EQUAL("ac!", pod.getConfigString().c_str());

    pod.removeProng(octi::DIR_E);
    CHECK_FALSE(pod.hasSuperProng());
    CHECK_EQUAL(1, pod.getNumProngs());
    CHECK_EQUAL(1, pod.getNumNormalProngs());
    CHECK_EQUAL(0, pod.getNumSuperProngs());
    STRCMP_EQUAL("a", pod.getConfigString().c_str());
}

TEST(OctiPodTest, WeightComparisonAndStrictOrdering)
{
    octi::OctiPod p0(octi::A_POD); // 0 prongs
    octi::OctiPod p1(octi::A_POD); // 1 prong
    p1.addProng(octi::DIR_N, false);
    octi::OctiPod p1s(octi::A_POD); // 1 superprong
    p1s.addProng(octi::DIR_N, true);
    octi::OctiPod p2(octi::A_POD); // 2 normal prongs
    p2.addProng(octi::DIR_N, false);
    p2.addProng(octi::DIR_E, false);

    CHECK_TRUE(p0 < p1);
    CHECK_TRUE(p1 < p1s);
    CHECK_TRUE(p1s < p2);
    CHECK_TRUE(p0 <= p1);
    CHECK_TRUE(p2 > p1);
    CHECK_TRUE(p2 >= p1s);
    CHECK_TRUE(p1 == p1);
    CHECK_FALSE(p1 != p1);
    CHECK_TRUE(p1 != p2);
}

TEST(OctiPodTest, StreamOutputOperator)
{
    octi::OctiPod podA(octi::A_POD);
    podA.addProng(octi::DIR_N, false);
    podA.addProng(octi::DIR_NE, true);

    std::ostringstream ss;
    ss << podA;
    STRCMP_EQUAL("Aab!", ss.str().c_str());

    octi::OctiPod undefPod;
    std::ostringstream ssUndef;
    ssUndef << undefPod;
    STRCMP_EQUAL("?", ssUndef.str().c_str());
}

TEST(OctiPodTest, BoundaryAllProngsSuperAndNormal)
{
    octi::OctiPod allNormal(octi::A_POD);
    for (int d = octi::DIR_N; d <= octi::DIR_NW; ++d) {
        allNormal.addProng(static_cast<octi::OctiDir>(d), false);
    }
    CHECK_EQUAL(8, allNormal.getNumProngs());
    CHECK_EQUAL(8, allNormal.getNumNormalProngs());
    CHECK_EQUAL(0, allNormal.getNumSuperProngs());
    STRCMP_EQUAL("abcdefgh", allNormal.getConfigString().c_str());

    octi::OctiPod allSuper(octi::B_POD);
    for (int d = octi::DIR_N; d <= octi::DIR_NW; ++d) {
        allSuper.addProng(static_cast<octi::OctiDir>(d), true);
    }
    CHECK_EQUAL(8, allSuper.getNumProngs());
    CHECK_EQUAL(0, allSuper.getNumNormalProngs());
    CHECK_EQUAL(8, allSuper.getNumSuperProngs());
    STRCMP_EQUAL("a!b!c!d!e!f!g!h!", allSuper.getConfigString().c_str());
}

TEST(OctiPodTest, RapidCheckProngConfigurationInvariants)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    bool passed = rc::check("OctiPod random prong configurations", []() {
        auto ownerVal = *rc::gen::inRange(0, 5);
        octi::OctiPodType owner = static_cast<octi::OctiPodType>(ownerVal);
        octi::OctiPod pod(owner);

        int normalCount = 0;
        int superCount = 0;

        for (int d = octi::DIR_N; d <= octi::DIR_NW; ++d) {
            auto pchoice = *rc::gen::inRange(0, 3); // 0=none, 1=normal, 2=super
            octi::OctiDir dir = static_cast<octi::OctiDir>(d);
            if (pchoice == 1) {
                pod.addProng(dir, false);
                normalCount++;
            } else if (pchoice == 2) {
                pod.addProng(dir, true);
                superCount++;
            }
        }

        RC_ASSERT(pod.getType() == owner);
        RC_ASSERT(pod.getNumNormalProngs() == normalCount);
        RC_ASSERT(pod.getNumSuperProngs() == superCount);
        RC_ASSERT(pod.getNumProngs() == normalCount + superCount);
        RC_ASSERT(pod.hasSuperProng() == (superCount > 0));

        // Roundtrip via raw config
        octi::OctiPod pod2(pod.rawConfig());
        RC_ASSERT(pod == pod2);
        RC_ASSERT(pod.getConfigString() == pod2.getConfigString());
    });

    CHECK_TRUE(passed);
}

TEST_GROUP(OctiSquareTest)
{
};

TEST(OctiSquareTest, InitialConfigurationAndAccessors)
{
    octi::OctiSquare sq(octi::GAME_MODE_2, octi::A_HOME_SQUARE, 2, 2);
    CHECK_EQUAL(octi::GAME_MODE_2, sq.getGameMode());
    CHECK_EQUAL(octi::A_HOME_SQUARE, sq.getType());
    CHECK_EQUAL(2, sq.getLocX());
    CHECK_EQUAL(2, sq.getLocY());
    CHECK_EQUAL(33, sq.getLocId());
    int x = -1, y = -1;
    sq.getLocXY(x, y);
    CHECK_EQUAL(2, x);
    CHECK_EQUAL(2, y);
    CHECK_TRUE(sq.isEmpty());
    CHECK_EQUAL(0, sq.getNumPods());
}

TEST(OctiSquareTest, AddRemovePodsStacking)
{
    octi::OctiSquare sq(octi::GAME_MODE_2, octi::NEUTRAL_SQUARE, 4, 4);

    octi::OctiPod pod1(octi::A_POD);
    pod1.addProng(octi::DIR_N, false);
    octi::OctiPod pod2(octi::A_POD);
    pod2.addProng(octi::DIR_E, false);

    sq.addPod(pod1);
    sq.addPod(pod2);

    CHECK_FALSE(sq.isEmpty());
    CHECK_EQUAL(2, sq.getNumPods());
    CHECK_TRUE(sq.containsPod(pod1));
    CHECK_TRUE(sq.containsPod(pod2));
    CHECK_TRUE(sq.getPodAt(0) == pod1);
    CHECK_TRUE(sq.getPodAt(1) == pod2);

    CHECK_TRUE(sq.removePod(pod2));
    CHECK_EQUAL(1, sq.getNumPods());
    CHECK_FALSE(sq.containsPod(pod2));
    CHECK_TRUE(sq.containsPod(pod1));

    CHECK_FALSE(sq.removePod(pod2)); // already removed
}

TEST(OctiSquareTest, EmptyWithoutGhostPods)
{
    // Fixes Legacy Bug #1
    octi::OctiSquare sq(octi::GAME_MODE_2, octi::NEUTRAL_SQUARE, 3, 3);
    octi::OctiPod pA(octi::A_POD);
    octi::OctiPod pB(octi::A_POD);
    pB.addProng(octi::DIR_N, false);

    sq.addPod(pA);
    sq.addPod(pB);

    // Empty ghostPods -> not empty without them
    CHECK_FALSE(sq.isEmptyWithout({}));

    // Partial ghostPods (only pA) -> not empty without (pB still present)
    CHECK_FALSE(sq.isEmptyWithout({pA}));

    // Full ghostPods matching both -> empty without them
    CHECK_TRUE(sq.isEmptyWithout({pA, pB}));

    // Ghost pod that isn't on square -> not empty without
    octi::OctiPod pC(octi::B_POD);
    CHECK_FALSE(sq.isEmptyWithout({pA, pC}));
}

TEST(OctiSquareTest, IsCapturedMode2)
{
    // Fixes Legacy Bug #7
    octi::OctiSquare aHome(octi::GAME_MODE_2, octi::A_HOME_SQUARE, 2, 2); // 33
    CHECK_FALSE(aHome.isCaptured());

    // Occupied by friendly pod -> not captured
    octi::OctiPod podA(octi::A_POD);
    aHome.addPod(podA);
    CHECK_FALSE(aHome.isCaptured());
    CHECK_FALSE(aHome.isCapturedBy(octi::PLAYER_B));

    // Enemy pod enters -> captured!
    octi::OctiPod podB(octi::B_POD);
    aHome.addPod(podB);
    CHECK_TRUE(aHome.isCaptured());
    CHECK_TRUE(aHome.isCapturedBy(octi::PLAYER_B));
    CHECK_FALSE(aHome.isCapturedBy(octi::PLAYER_A));

    // Neutral square occupied by enemy pod is never "captured" as a home base
    octi::OctiSquare neutralSq(octi::GAME_MODE_2, octi::NEUTRAL_SQUARE, 4, 4);
    neutralSq.addPod(podB);
    CHECK_FALSE(neutralSq.isCaptured());
}

TEST(OctiSquareTest, IsCapturedMode4)
{
    // 4-Player Partnership: Team A & C vs Team B & D
    octi::OctiSquare aHome(octi::GAME_MODE_4, octi::A_HOME_SQUARE, 2, 2); // 33

    octi::OctiPod podC(octi::C_POD); // ally of A
    aHome.addPod(podC);
    CHECK_FALSE(aHome.isCaptured()); // Allied pod does NOT capture team's base

    octi::OctiPod podD(octi::D_POD); // enemy of A & C
    aHome.addPod(podD);
    CHECK_TRUE(aHome.isCaptured());
    CHECK_TRUE(aHome.isCapturedBy(octi::PLAYER_D));
    CHECK_TRUE(aHome.isCapturedByTeamOf(octi::PLAYER_B));
    CHECK_FALSE(aHome.isCapturedByTeamOf(octi::PLAYER_A));
}

TEST(OctiSquareTest, IsCapturedModeOFK)
{
    // OCTI for Kids: row 2 bases (22, 32, 42, 52) vs row 6 bases (26, 36, 46, 56)
    octi::OctiSquare ofkSq(octi::GAME_MODE_OFK, octi::B_HOME_SQUARE, 1, 5); // 26
    octi::OctiPod podA(octi::A_POD);
    ofkSq.addPod(podA);

    CHECK_TRUE(ofkSq.isCaptured());
    CHECK_TRUE(ofkSq.isCapturedBy(octi::PLAYER_A));
}

TEST(OctiSquareTest, HasEnemyOfAndIsEnemyOf)
{
    octi::OctiSquare sq(octi::GAME_MODE_2, octi::B_HOME_SQUARE, 2, 6); // 37
    CHECK_TRUE(sq.isEnemyOf(octi::PLAYER_A));
    CHECK_FALSE(sq.isEnemyOf(octi::PLAYER_B));

    octi::OctiPod podB(octi::B_POD);
    sq.addPod(podB);
    CHECK_TRUE(sq.hasEnemyOf(octi::PLAYER_A));
    CHECK_FALSE(sq.hasEnemyOf(octi::PLAYER_B));
}

TEST(OctiSquareTest, HasSameMaterialsAs)
{
    octi::OctiSquare s1(octi::GAME_MODE_2, octi::NEUTRAL_SQUARE, 0, 0);
    octi::OctiSquare s2(octi::GAME_MODE_2, octi::NEUTRAL_SQUARE, 1, 1);

    octi::OctiPod p1(octi::A_POD);
    octi::OctiPod p2(octi::B_POD);

    s1.addPod(p1);
    s1.addPod(p2);

    s2.addPod(p2);
    s2.addPod(p1);

    CHECK_TRUE(s1.hasSameMaterialsAs(s2));

    s2.addPod(p1);
    CHECK_FALSE(s1.hasSameMaterialsAs(s2));
}

TEST(OctiSquareTest, BoundaryFullStackAndEmpty)
{
    octi::OctiSquare sq(octi::GAME_MODE_4, octi::NEUTRAL_SQUARE, 4, 4);
    for (int i = 0; i < 8; ++i) {
        octi::OctiPod p(octi::A_POD);
        p.addProng(static_cast<octi::OctiDir>(i), false);
        sq.addPod(p);
    }
    CHECK_EQUAL(8, sq.getNumPods());

    sq.clear();
    CHECK_EQUAL(0, sq.getNumPods());
    CHECK_TRUE(sq.isEmpty());
}

TEST(OctiSquareTest, FaultOutOfBoundsPodAccess)
{
    octi::OctiSquare sq(octi::GAME_MODE_2, octi::NEUTRAL_SQUARE, 0, 0);
    CHECK_THROWS(std::out_of_range, sq.getPodAt(0));
    CHECK_THROWS(std::out_of_range, sq.podAt(-1));
}

TEST_GROUP(OctiMoveTest)
{
};

TEST(OctiMoveTest, ParsePodPlacementAndExplicitLiberation)
{
    // Reserve placement
    octi::OctiMove m1("33");
    CHECK_EQUAL(1, m1.size());
    const auto &ops1 = m1.getOps();
    CHECK_EQUAL(2, ops1[0].x0);
    CHECK_EQUAL(2, ops1[0].y0);
    CHECK_TRUE(ops1[0].addPod);
    CHECK_FALSE(ops1[0].liberate);
    STRCMP_EQUAL("33", m1.asString().c_str());

    // Explicit liberation
    octi::OctiMove m2("37*");
    CHECK_EQUAL(1, m2.size());
    const auto &ops2 = m2.getOps();
    CHECK_EQUAL(2, ops2[0].x0);
    CHECK_EQUAL(6, ops2[0].y0);
    CHECK_TRUE(ops2[0].addPod);
    CHECK_TRUE(ops2[0].liberate);
    STRCMP_EQUAL("37*", m2.asString().c_str());
}

TEST(OctiMoveTest, ParseProngAddNormalAndSuper)
{
    octi::OctiMove m1("33+b");
    CHECK_EQUAL(1, m1.size());
    const auto &ops1 = m1.getOps();
    CHECK_EQUAL(octi::DIR_NE, ops1[0].addProngDir);
    CHECK_FALSE(ops1[0].superProng);
    CHECK_EQUAL(0, ops1[0].pod.getNumProngs());
    STRCMP_EQUAL("33+b", m1.asString().c_str());

    octi::OctiMove m2("33b+a!");
    CHECK_EQUAL(1, m2.size());
    const auto &ops2 = m2.getOps();
    CHECK_EQUAL(octi::DIR_N, ops2[0].addProngDir);
    CHECK_TRUE(ops2[0].superProng);
    CHECK_EQUAL(1, ops2[0].pod.getNumProngs());
    CHECK_EQUAL(octi::NORMAL_PRONG, ops2[0].pod.prongAt(octi::DIR_NE));
    STRCMP_EQUAL("33b+a!", m2.asString().c_str());
}

TEST(OctiMoveTest, ParseProngRepositionCanonicalAndReversed)
{
    // R1: Prong repositioning (+add-rm and -rm+add) is prohibited across all variants
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a+b-a"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a-a+b"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("55acg+b-a"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("55acg-c+h"));
}

TEST(OctiMoveTest, ParseSingleStepMoveAndMultiHopJump)
{
    // Single step
    octi::OctiMove m1("33a-34");
    CHECK_EQUAL(1, m1.size());
    const auto &ops1 = m1.getOps();
    CHECK_EQUAL(1, ops1[0].hops.size());
    CHECK_EQUAL(2, ops1[0].hops[0].x);
    CHECK_EQUAL(3, ops1[0].hops[0].y);
    CHECK_FALSE(ops1[0].hops[0].capture);
    STRCMP_EQUAL("33a-34", m1.asString().c_str());

    // Single jump with capture
    octi::OctiMove m2("33a-35x");
    CHECK_EQUAL(1, m2.size());
    const auto &ops2 = m2.getOps();
    CHECK_EQUAL(1, ops2[0].hops.size());
    CHECK_EQUAL(2, ops2[0].hops[0].x);
    CHECK_EQUAL(4, ops2[0].hops[0].y);
    CHECK_TRUE(ops2[0].hops[0].capture);
    STRCMP_EQUAL("33a-35x", m2.asString().c_str());

    // Multi-hop jump
    octi::OctiMove m3("33ac-35x-55");
    CHECK_EQUAL(1, m3.size());
    const auto &ops3 = m3.getOps();
    CHECK_EQUAL(2, ops3[0].hops.size());
    CHECK_EQUAL(2, ops3[0].hops[0].x);
    CHECK_EQUAL(4, ops3[0].hops[0].y);
    CHECK_TRUE(ops3[0].hops[0].capture);
    CHECK_EQUAL(4, ops3[0].hops[1].x);
    CHECK_EQUAL(4, ops3[0].hops[1].y);
    CHECK_FALSE(ops3[0].hops[1].capture);
    STRCMP_EQUAL("33ac-35x-55", m3.asString().c_str());
}

TEST(OctiMoveTest, ParseCompositeStackMoves)
{
    octi::OctiMove m("37df-46,37e-36");
    CHECK_EQUAL(2, m.size());
    const auto &ops = m.getOps();
    CHECK_EQUAL(2, ops[0].x0);
    CHECK_EQUAL(6, ops[0].y0);
    CHECK_EQUAL(2, ops[1].x0);
    CHECK_EQUAL(6, ops[1].y0);
    STRCMP_EQUAL("37df-46,37e-36", m.asString().c_str());
}

TEST(OctiMoveTest, ParseSuperprongBonusMovePrelude)
{
    octi::OctiMove m("33a!-34;34a!+b");
    CHECK_EQUAL(2, m.size());
    const auto &ops = m.getOps();
    CHECK_TRUE(ops[0].isSuperProngBonusMove);
    CHECK_FALSE(ops[1].isSuperProngBonusMove);
    STRCMP_EQUAL("33a!-34;34a!+b", m.asString().c_str());

    octi::OctiMove m2("33a!-34;34a!-35;35a+c");
    CHECK_EQUAL(3, m2.size());
    CHECK_TRUE(m2.getOps()[0].isSuperProngBonusMove);
    CHECK_TRUE(m2.getOps()[1].isSuperProngBonusMove);
    CHECK_FALSE(m2.getOps()[2].isSuperProngBonusMove);
    STRCMP_EQUAL("33a!-34;34a!-35;35a+c", m2.asString().c_str());

    // R5: Up to 4 superprong bonus preludes (3 and 4 preludes in 4P_SP)
    octi::OctiMove m3("33a!c!e!-34;34a!c!e!-44;44a!c!e!-43;43a!c!e!+b");
    CHECK_EQUAL(4, m3.size());
    CHECK_TRUE(m3.getOps()[0].isSuperProngBonusMove);
    CHECK_TRUE(m3.getOps()[1].isSuperProngBonusMove);
    CHECK_TRUE(m3.getOps()[2].isSuperProngBonusMove);
    CHECK_FALSE(m3.getOps()[3].isSuperProngBonusMove);
    STRCMP_EQUAL("33a!c!e!-34;34a!c!e!-44;44a!c!e!-43;43a!c!e!+b", m3.asString().c_str());

    octi::OctiMove m4("33a!c!e!g!-34;34a!c!e!g!-44;44a!c!e!g!-43;43a!c!e!g!-33;33a!c!e!g!+b");
    CHECK_EQUAL(5, m4.size());
    CHECK_TRUE(m4.getOps()[0].isSuperProngBonusMove);
    CHECK_TRUE(m4.getOps()[1].isSuperProngBonusMove);
    CHECK_TRUE(m4.getOps()[2].isSuperProngBonusMove);
    CHECK_TRUE(m4.getOps()[3].isSuperProngBonusMove);
    CHECK_FALSE(m4.getOps()[4].isSuperProngBonusMove);
    STRCMP_EQUAL("33a!c!e!g!-34;34a!c!e!g!-44;44a!c!e!g!-43;43a!c!e!g!-33;33a!c!e!g!+b",
                 m4.asString().c_str());
}

TEST(OctiMoveTest, ConstructFromOpsVector)
{
    octi::OctiMoveOp op;
    op.x0 = 2;
    op.y0 = 2;
    op.pod.addProng(octi::DIR_N, false);
    octi::OctiHop hop;
    hop.x = 2;
    hop.y = 3;
    hop.capture = false;
    op.hops.push_back(hop);

    octi::OctiMove move({op});
    CHECK_EQUAL(1, move.size());
    STRCMP_EQUAL("33a-34", move.asString().c_str());
}

TEST(OctiMoveTest, FaultNullAndEmptyString)
{
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove(nullptr));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove(""));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("   \t  "));
}

TEST(OctiMoveTest, FaultInvalidCoordinates)
{
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("00"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("90"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("3a"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("3"));
}

TEST(OctiMoveTest, FaultDuplicateProngsInConfig)
{
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33aa+b"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a!a+b"));
}

TEST(OctiMoveTest, FaultProngAddAlreadyExisting)
{
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a+a"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a!+a"));
}

TEST(OctiMoveTest, FaultProngRepositionIllegalOperations)
{
    // R1: All prong reposition forms (+add-rm, -rm+add, programmatic rmProngDir) throw
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a+b-a"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a-a+b"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a+a-a"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a+b-c"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a!+b-a"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a+b!-a"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a!-a+b"));

    octi::OctiMoveOp repoOp;
    repoOp.x0 = 2;
    repoOp.y0 = 2;
    repoOp.pod.addProng(octi::DIR_N, false);
    repoOp.addProngDir = octi::DIR_NE;
    repoOp.rmProngDir = octi::DIR_N;
    CHECK_THROWS(octi::IllegalMoveSyntaxException,
                 octi::OctiMove(std::vector<octi::OctiMoveOp>{repoOp}));
}

TEST(OctiMoveTest, FaultCompositeMismatchSourceCoordinates)
{
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a-34,34a-35"));
}

TEST(OctiMoveTest, FaultCompositeMixingProngOrPodOps)
{
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a-34,33+b"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33,33a-34"));
}

TEST(OctiMoveTest, FaultSuperprongPreludeSyntaxErrors)
{
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove(";33+b"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a!-34;"));
    // > 4 bonus preludes (6 semicolon-separated segments) must be rejected
    CHECK_THROWS(octi::IllegalMoveSyntaxException,
                 octi::OctiMove("33a!-34;34a!-35;35a!-36;36a!-37;37a!-38;38a+b"));

    std::vector<octi::OctiMoveOp> fiveBonusOps;
    for (int i = 0; i < 5; ++i) {
        octi::OctiMoveOp bop;
        bop.x0 = 2;
        bop.y0 = 2 + i;
        bop.pod.addProng(octi::DIR_N, true);
        bop.isSuperProngBonusMove = true;
        bop.hops.push_back({2, 3 + i, false});
        fiveBonusOps.push_back(bop);
    }
    octi::OctiMoveOp regOp;
    regOp.x0 = 2;
    regOp.y0 = 7;
    regOp.pod.addProng(octi::DIR_N, true);
    regOp.addProngDir = octi::DIR_NE;
    fiveBonusOps.push_back(regOp);
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove(fiveBonusOps));
}

TEST(OctiMoveTest, RapidCheckMoveCanonicalRoundTrip)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    bool passed = rc::check("OctiMove canonical round trip across 0..4 SP preludes", []() {
        const int numPreludes = *rc::gen::inRange(0, 5); // 0..4 preludes
        std::vector<octi::OctiMoveOp> ops;
        ops.reserve(static_cast<std::size_t>(numPreludes + 2));

        for (int p = 0; p < numPreludes; ++p) {
            octi::OctiMoveOp bop;
            bop.x0 = *rc::gen::inRange(0, 9);
            bop.y0 = *rc::gen::inRange(0, 9);
            const int spDir = *rc::gen::inRange(0, 8);
            bop.pod.addProng(static_cast<octi::OctiDir>(spDir), true);
            bop.isSuperProngBonusMove = true;
            octi::OctiHop bh;
            bh.x = *rc::gen::inRange(0, 9);
            bh.y = *rc::gen::inRange(0, 9);
            bh.capture = *rc::gen::arbitrary<bool>();
            bop.hops.push_back(bh);
            ops.push_back(bop);
        }

        // Regular part: 0=place, 1=addProng, 2=hop, 3=composite hops
        auto kind = *rc::gen::inRange(0, 4);
        int x0 = *rc::gen::inRange(0, 9);
        int y0 = *rc::gen::inRange(0, 9);

        if (kind == 0) {
            octi::OctiMoveOp op;
            op.x0 = x0;
            op.y0 = y0;
            op.addPod = true;
            op.liberate = *rc::gen::arbitrary<bool>();
            ops.push_back(op);
        } else if (kind == 1) {
            octi::OctiMoveOp op;
            op.x0 = x0;
            op.y0 = y0;
            op.addProngDir = static_cast<octi::OctiDir>(*rc::gen::inRange(0, 8));
            op.superProng = *rc::gen::arbitrary<bool>();
            ops.push_back(op);
        } else if (kind == 2) {
            octi::OctiMoveOp op;
            op.x0 = x0;
            op.y0 = y0;
            op.pod.addProng(octi::DIR_N, false);
            octi::OctiHop h;
            h.x = *rc::gen::inRange(0, 9);
            h.y = *rc::gen::inRange(0, 9);
            h.capture = *rc::gen::arbitrary<bool>();
            op.hops.push_back(h);
            ops.push_back(op);
        } else {
            for (int c = 0; c < 2; ++c) {
                octi::OctiMoveOp op;
                op.x0 = x0;
                op.y0 = y0;
                op.pod.addProng(static_cast<octi::OctiDir>((c * 2) % 8), false);
                octi::OctiHop h;
                h.x = *rc::gen::inRange(0, 9);
                h.y = *rc::gen::inRange(0, 9);
                h.capture = *rc::gen::arbitrary<bool>();
                op.hops.push_back(h);
                ops.push_back(op);
            }
        }

        octi::OctiMove m(ops);
        std::string s = m.asString();
        octi::OctiMove mFromStr(s);

        RC_ASSERT(m == mFromStr);
        RC_ASSERT(m.asString() == mFromStr.asString());
        RC_ASSERT(m.structuralKey() == mFromStr.structuralKey());
    });

    CHECK_TRUE(passed);
}

TEST(OctiMoveTest, RapidCheckFuzzParserRobustness)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    bool passed = rc::check("OctiMove fuzz parser robustness", []() {
        std::string fuzz = *rc::gen::arbitrary<std::string>();
        try {
            octi::OctiMove m(fuzz);
            // If parsed, serialization must not throw or crash
            std::string s = m.asString();
            octi::OctiMove m2(s);
            RC_ASSERT(m == m2);
        } catch (const octi::IllegalMoveSyntaxException &) {
            // Expected rejection of malformed input
        }
    });

    CHECK_TRUE(passed);
}

// ---------------------------------------------------------------------------
// OctiBoardTest
// ---------------------------------------------------------------------------

class MockBoardListener : public octi::OctiBoardListener {
public:
    int updatedCount = 0;
    int gameChangedCount = 0;
    void boardUpdated(octi::OctiBoard *board, const octi::OctiMove *move) override {
        (void)board; (void)move;
        updatedCount++;
    }
    void gameChanged(octi::OctiBoard *board, octi::OctiGame *oldGame, octi::OctiGame *newGame) override {
        (void)board; (void)oldGame; (void)newGame;
        gameChangedCount++;
    }
};

TEST_GROUP(OctiBoardTest)
{
};

TEST(OctiBoardTest, BoardDimensionsAndModes)
{
    octi::OctiBoard b2(octi::GAME_MODE_2);
    CHECK_EQUAL(9, b2.getBoardWidth());
    CHECK_EQUAL(9, b2.getBoardHeight());
    CHECK_EQUAL(octi::GAME_MODE_2, b2.getGameMode());

    octi::OctiBoard b4(octi::GAME_MODE_4);
    CHECK_EQUAL(9, b4.getBoardWidth());
    CHECK_EQUAL(9, b4.getBoardHeight());
    CHECK_EQUAL(octi::GAME_MODE_4, b4.getGameMode());

    octi::OctiBoard bOfk(octi::GAME_MODE_OFK);
    CHECK_EQUAL(6, bOfk.getBoardWidth());
    CHECK_EQUAL(7, bOfk.getBoardHeight());
    CHECK_EQUAL(octi::GAME_MODE_OFK, bOfk.getGameMode());
}

TEST(OctiBoardTest, SquareTypesAndCoordinates)
{
    octi::OctiBoard b2(octi::GAME_MODE_2);
    CHECK_EQUAL(octi::A_HOME_SQUARE, b2.squareAt(2, 2).getType());
    CHECK_EQUAL(octi::A_HOME_SQUARE, b2.squareAt(4, 2).getType());
    CHECK_EQUAL(octi::A_HOME_SQUARE, b2.squareAt(6, 2).getType());
    CHECK_EQUAL(octi::B_HOME_SQUARE, b2.squareAt(2, 6).getType());
    CHECK_EQUAL(octi::B_HOME_SQUARE, b2.squareAt(4, 6).getType());
    CHECK_EQUAL(octi::B_HOME_SQUARE, b2.squareAt(6, 6).getType());
    CHECK_EQUAL(octi::NEUTRAL_SQUARE, b2.squareAt(0, 0).getType());

    CHECK_THROWS(octi::IllegalMoveException, b2.squareAt(-1, 0));
    CHECK_THROWS(octi::IllegalMoveException, b2.squareAt(9, 9));

    octi::OctiBoard b4(octi::GAME_MODE_4);
    CHECK_EQUAL(octi::A_HOME_SQUARE, b4.squareAt(2, 2).getType());
    CHECK_EQUAL(octi::B_HOME_SQUARE, b4.squareAt(2, 6).getType());
    CHECK_EQUAL(octi::C_HOME_SQUARE, b4.squareAt(6, 6).getType());
    CHECK_EQUAL(octi::D_HOME_SQUARE, b4.squareAt(6, 2).getType());
    CHECK_EQUAL(octi::NEUTRAL_OCTI_SQUARE, b4.squareAt(4, 2).getType());
    CHECK_EQUAL(octi::NEUTRAL_OCTI_SQUARE, b4.squareAt(4, 6).getType());

    octi::OctiBoard bOfk(octi::GAME_MODE_OFK);
    CHECK_EQUAL(octi::A_HOME_SQUARE, bOfk.squareAt(1, 1).getType());
    CHECK_EQUAL(octi::A_HOME_SQUARE, bOfk.squareAt(4, 1).getType());
    CHECK_EQUAL(octi::B_HOME_SQUARE, bOfk.squareAt(1, 5).getType());
    CHECK_EQUAL(octi::B_HOME_SQUARE, bOfk.squareAt(4, 5).getType());
    CHECK_THROWS(octi::IllegalMoveException, bOfk.squareAt(6, 0));
    CHECK_THROWS(octi::IllegalMoveException, bOfk.squareAt(0, 7));
}

TEST(OctiBoardTest, CanLiberateLogic)
{
    octi::OctiBoard b2(octi::GAME_MODE_2);
    CHECK_FALSE(b2.canLiberate(octi::PLAYER_A));
    CHECK_FALSE(b2.canLiberate(octi::PLAYER_B));

    b2.squareAt(2, 6).addPod(octi::OctiPod(octi::A_POD));
    CHECK_TRUE(b2.canLiberate(octi::PLAYER_A));
    CHECK_FALSE(b2.canLiberate(octi::PLAYER_B));

    octi::OctiBoard bOfk(octi::GAME_MODE_OFK);
    bOfk.squareAt(1, 5).addPod(octi::OctiPod(octi::A_POD));
    CHECK_FALSE(bOfk.canLiberate(octi::PLAYER_A));

    octi::OctiBoard b4(octi::GAME_MODE_4);
    CHECK_FALSE(b4.canLiberate(octi::PLAYER_A));
    b4.squareAt(4, 2).addPod(octi::OctiPod(octi::A_POD));
    CHECK_TRUE(b4.canLiberate(octi::PLAYER_A));
}

TEST(OctiBoardTest, BoardListenerNotifications)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    MockBoardListener listener;
    board.addBoardListener(&listener);

    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    game->move("33+b");
    CHECK_EQUAL(1, listener.updatedCount);

    board.setGameMode(octi::GAME_MODE_OFK);
    CHECK_EQUAL(1, listener.gameChangedCount);

    board.removeBoardListener(&listener);
}

TEST(OctiBoardTest, AsciiStreamOutput)
{
    octi::OctiBoard b2(octi::GAME_MODE_2);
    std::ostringstream ssOccupied;
    ssOccupied << b2;
    std::string sOcc = ssOccupied.str();
    CHECK_TRUE(sOcc.find("A:") != std::string::npos);

    b2.clear();
    std::ostringstream ss2;
    ss2 << b2;
    std::string s2 = ss2.str();
    CHECK_TRUE(s2.find("(A)") != std::string::npos);
    CHECK_TRUE(s2.find("(B)") != std::string::npos);

    octi::OctiBoard bOfk(octi::GAME_MODE_OFK);
    std::ostringstream ssOfk;
    ssOfk << bOfk;
    std::string sOfk = ssOfk.str();
    CHECK_TRUE(sOfk.find("6") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Octi2PlayerGameTest
// ---------------------------------------------------------------------------

TEST_GROUP(Octi2PlayerGameTest)
{
};

TEST(Octi2PlayerGameTest, ResetAndInitialReserves)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->reset();
    game->start(octi::PLAYER_A);

    CHECK_TRUE(game->isGameStarted());
    CHECK_FALSE(game->isGameOver());
    CHECK_EQUAL(octi::PLAYER_A, game->getTurn());
    CHECK_EQUAL(4, game->getFreePods(octi::PLAYER_A));
    CHECK_EQUAL(4, game->getFreePods(octi::PLAYER_B));
    CHECK_EQUAL(0, game->getCapturedPods(octi::PLAYER_A));
    CHECK_EQUAL(0, game->getCapturedPods(octi::PLAYER_B));
    CHECK_EQUAL(25, game->getFreeProngs(octi::PLAYER_A));
    CHECK_EQUAL(25, game->getFreeProngs(octi::PLAYER_B));
    CHECK_EQUAL(0, game->getFreeSuperProngs(octi::PLAYER_A));
    CHECK_EQUAL(0, game->getFreeSuperProngs(octi::PLAYER_B));
}

TEST(Octi2PlayerGameTest, TurnOptionI_AddNormalAndSuperProng)
{
    octi::OctiBoard board(octi::GAME_MODE_2, true /* useSuperProngs */);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    CHECK_EQUAL(24, game->getFreeProngs(octi::PLAYER_A));
    CHECK_EQUAL(1, game->getFreeSuperProngs(octi::PLAYER_A));

    game->move("33+b");
    CHECK_EQUAL(23, game->getFreeProngs(octi::PLAYER_A));
    CHECK_EQUAL(octi::PLAYER_B, game->getTurn());
    CHECK_EQUAL(octi::NORMAL_PRONG, board.squareAt(2, 2).getPodAt(0).prongAt(octi::DIR_NE));

    game->move("37+e!");
    CHECK_EQUAL(0, game->getFreeSuperProngs(octi::PLAYER_B));
    CHECK_EQUAL(octi::PLAYER_A, game->getTurn());
    CHECK_EQUAL(octi::SUPER_PRONG, board.squareAt(2, 6).getPodAt(0).prongAt(octi::DIR_S));
}

TEST(Octi2PlayerGameTest, TurnOptionII_RepositionNormalProng)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    game->move("33+b");
    game->move("37+e");
    // R1: Prong repositioning is rejected across all variants
    CHECK_THROWS(octi::IllegalMoveSyntaxException, game->move("33b+c-b"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, game->move("33b-b+c"));
    for (const auto &lm : game->generateLegalMoves()) {
        for (const auto &op : lm.getOps()) {
            CHECK_EQUAL(octi::DIR_UNDEF, op.rmProngDir);
        }
    }
}

TEST(Octi2PlayerGameTest, TurnOptionII_RejectSuperProngReposition)
{
    octi::OctiBoard board(octi::GAME_MODE_2, true);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    game->move("33+b!");
    game->move("37+e");

    CHECK_THROWS(octi::IllegalMoveSyntaxException, game->move("33b!+c-b"));
}

TEST(Octi2PlayerGameTest, TurnOptionIII_Distance1Move)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    game->move("33+a");
    game->move("37+e");
    game->move("33a-34");
    CHECK_EQUAL(0, board.squareAt(2, 2).getNumPods());
    CHECK_EQUAL(1, board.squareAt(2, 3).getNumPods());

    game->move("57+d");
    CHECK_THROWS(octi::IllegalMoveException, game->move("34a-35x"));
}

TEST(Octi2PlayerGameTest, TurnOptionIV_Distance2JumpAndCapture)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    game->move("33+a");
    game->move("37+e");
    game->move("53+c");
    game->move("37e-36");
    game->move("53c+b");
    game->move("36e-35");
    game->move("53bc+d");
    game->move("35e-34");

    CHECK_EQUAL(1, board.squareAt(2, 3).getNumPods());
    CHECK_EQUAL(0, game->getCapturedPods(octi::PLAYER_B));
    int prongsBefore = game->getFreeProngs(octi::PLAYER_A);

    game->move("33a-35x");
    CHECK_EQUAL(0, board.squareAt(2, 3).getNumPods());
    CHECK_EQUAL(1, board.squareAt(2, 4).getNumPods());
    CHECK_EQUAL(1, game->getCapturedPods(octi::PLAYER_B));
    CHECK_EQUAL(prongsBefore + 1, game->getFreeProngs(octi::PLAYER_A));
}

TEST(Octi2PlayerGameTest, TurnOptionV_ReservePodAndLiberation)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    CHECK_EQUAL(4, game->getFreePods(octi::PLAYER_A));
    game->move("33");
    CHECK_EQUAL(3, game->getFreePods(octi::PLAYER_A));
    CHECK_EQUAL(2, board.squareAt(2, 2).getNumPods());

    // Occupy enemy base 37 and liberate
    board.squareAt(2, 6).clear();
    board.squareAt(2, 6).addPod(octi::OctiPod(octi::A_POD));
    octi::OctiGameSnapshot snap = game->captureSnapshot();
    snap.players[octi::PLAYER_A].capturedPods = 1;
    game->restoreSnapshot(snap);

    game->move("57+e"); // B moves on 57
    game->move("37*");  // A explicitly liberates onto 37
    CHECK_EQUAL(0, game->getCapturedPods(octi::PLAYER_A));
}

TEST(Octi2PlayerGameTest, UserReported72MoveTranscriptRejectsEmptyBaseLiberationAtMove60)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    const std::vector<std::string> moves = {
        "33+b", "37+d", "53+h", "37d+e", "33b-44", "37de+f", "44b+h", "37def-46",
        "44bh+a", "57+d", "53h+b", "57d+e", "33", "57de+f", "33+h", "77+d",
        "33h+b", "46def-55", "44abh-66x-88x", "37", "88abh+f", "37", "88abfh-77", "37",
        "77abfh+g", "37", "77abfgh-67", "57def-66", "67abfgh+e", "66def-75",
        "67abefgh-57", "37+d", "57abefgh-47", "37d+c", "47abefgh-27x", "75def+a",
        "27abefgh+c", "75adef+b", "27abcefgh-37", "75abdef+c", "37", "75abcdef+g",
        "37abcefgh-47", "75abcdefg+h", "47abcefgh-57", "75abcdefgh-66", "57abcefgh-47",
        "66abcdefgh-56", "47abcefgh-38", "56abcdefgh-65", "38abcefgh+d", "65abcdefgh-74",
        "38abcdefgh-47", "74abcdefgh-72x", "47abcdefgh-57", "72abcdefgh-73", "57",
        "73", "57abcdefgh-67"
    };

    LONGS_EQUAL(59, moves.size());
    for (const auto &m : moves) {
        game->move(m);
    }

    CHECK_EQUAL(octi::PLAYER_B, game->getTurn());
    LONGS_EQUAL(0, game->getFreePods(octi::PLAYER_B));
    LONGS_EQUAL(5, game->getCapturedPods(octi::PLAYER_B));
    CHECK_TRUE(board.squareAt(6, 2).isOccupiedBy(octi::PLAYER_B));
    CHECK_TRUE(board.squareAt(6, 6).isEmpty());

    std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
    bool has73Star = false;
    bool has77 = false;
    bool has77Star = false;
    bool has37 = false;
    bool has37Star = false;
    bool has57 = false;
    bool has57Star = false;

    for (const auto &lm : legalMoves) {
        std::string s = lm.asString();
        if (s == "73*") has73Star = true;
        if (s == "77") has77 = true;
        if (s == "77*") has77Star = true;
        if (s == "37") has37 = true;
        if (s == "37*") has37Star = true;
        if (s == "57") has57 = true;
        if (s == "57*") has57Star = true;
    }

    CHECK_TRUE(has73Star);
    CHECK_FALSE(has77);
    CHECK_FALSE(has77Star);
    CHECK_FALSE(has37);
    CHECK_FALSE(has37Star);
    CHECK_FALSE(has57);
    CHECK_FALSE(has57Star);

    CHECK_THROWS(octi::IllegalMoveException, game->move("77"));
    CHECK_THROWS(octi::IllegalMoveException, game->move("77*"));
}

TEST(Octi2PlayerGameTest, EnemySquarePodPlacementPrioritizesReserveOverLiberationWhenUnadorned)
{
    // 1. 2P mode: placing an unadorned pod ("57") on an occupied enemy OCTI square
    // while holding both freePods > 0 and capturedPods > 0 must consume freePods first.
    octi::OctiBoard board2p(octi::GAME_MODE_2);
    octi::OctiGame *game2p = board2p.getGame();
    game2p->start(octi::PLAYER_A);

    board2p.squareAt(4, 6).clear(); // (5, 7) is a B_OCTI_SQUARE (enemy base for Player A)
    board2p.squareAt(4, 6).addPod(octi::OctiPod(octi::A_POD));
    octi::OctiGameSnapshot snap2p = game2p->captureSnapshot();
    snap2p.players[octi::PLAYER_A].freePods = 1;
    snap2p.players[octi::PLAYER_A].capturedPods = 2;
    game2p->restoreSnapshot(snap2p);

    // Unadorned "57" consumes freePods first (1 -> 0), capturedPods stays 2
    game2p->move("57");
    LONGS_EQUAL(0, game2p->getFreePods(octi::PLAYER_A));
    LONGS_EQUAL(2, game2p->getCapturedPods(octi::PLAYER_A));
    LONGS_EQUAL(2, board2p.squareAt(4, 6).getNumPods());

    // Pass B's turn
    game2p->move("37+d");

    // With freePods == 0, unadorned "57" falls back to consuming capturedPods (2 -> 1)
    game2p->move("57");
    LONGS_EQUAL(0, game2p->getFreePods(octi::PLAYER_A));
    LONGS_EQUAL(1, game2p->getCapturedPods(octi::PLAYER_A));
    LONGS_EQUAL(3, board2p.squareAt(4, 6).getNumPods());

    // Pass B's turn
    game2p->move("77+d");

    // Explicit liberation "57*" consumes the last captured pod (1 -> 0)
    game2p->move("57*");
    LONGS_EQUAL(0, game2p->getFreePods(octi::PLAYER_A));
    LONGS_EQUAL(0, game2p->getCapturedPods(octi::PLAYER_A));
    LONGS_EQUAL(4, board2p.squareAt(4, 6).getNumPods());

    // 2. 4P mode: (5, 7) is NEUTRAL_OCTI_SQUARE; reserve pods cannot be placed on neutral squares
    octi::OctiBoard board4p(octi::GAME_MODE_4);
    octi::OctiGame *game4p = board4p.getGame();
    game4p->start(octi::PLAYER_A);

    board4p.squareAt(4, 6).clear();
    board4p.squareAt(4, 6).addPod(octi::OctiPod(octi::A_POD));
    octi::OctiGameSnapshot snap4p = game4p->captureSnapshot();
    snap4p.players[octi::PLAYER_A].freePods = 2;
    snap4p.players[octi::PLAYER_A].capturedPods = 1;
    game4p->restoreSnapshot(snap4p);

    // Unadorned "57" is rejected because freePods cannot be placed on neutral squares
    CHECK_THROWS(octi::IllegalMoveException, game4p->move("57"));

    // "57*" liberates captured pod onto neutral square
    game4p->move("57*");
    LONGS_EQUAL(2, game4p->getFreePods(octi::PLAYER_A));
    LONGS_EQUAL(0, game4p->getCapturedPods(octi::PLAYER_A));
    LONGS_EQUAL(2, board4p.squareAt(4, 6).getNumPods());
}

TEST(Octi2PlayerGameTest, UndoRedoStack)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    std::string fen0 = board.toPositionString();
    game->move("33+b");
    std::string fen1 = board.toPositionString();
    game->move("37+e");
    std::string fen2 = board.toPositionString();

    CHECK_TRUE(game->canUndo());
    game->undo();
    CHECK_EQUAL(fen1, board.toPositionString());

    game->undo();
    CHECK_EQUAL(fen0, board.toPositionString());
    CHECK_FALSE(game->canUndo());

    CHECK_TRUE(game->canRedo());
    game->redo();
    CHECK_EQUAL(fen1, board.toPositionString());

    game->redo();
    CHECK_EQUAL(fen2, board.toPositionString());
    CHECK_FALSE(game->canRedo());
}

TEST(Octi2PlayerGameTest, VictoryConditions)
{
    octi::OctiBoard b1(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
    octi::OctiGame *g1 = b1.getGame();
    g1->start(octi::PLAYER_A);

    b1.squareAt(2, 6).addPod(octi::OctiPod(octi::A_POD));
    g1->move("33+b");
    CHECK_TRUE(g1->isGameOver());
    CHECK_EQUAL(octi::PLAYER_A, g1->getWinner());

    octi::OctiBoard bResign(octi::GAME_MODE_2);
    octi::OctiGame *gResign = bResign.getGame();
    gResign->start(octi::PLAYER_A);
    gResign->resign(octi::PLAYER_A);
    CHECK_TRUE(gResign->isGameOver());
    CHECK_TRUE(gResign->isGameResigned());
    CHECK_EQUAL(octi::PLAYER_B, gResign->getWinner());
}

TEST(Octi2PlayerGameTest, PreparatoryWorkflow)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    CHECK_FALSE(game->hasPreparedMove());
    game->prepareAddProng(2, 2, octi::OctiPod(octi::A_POD), octi::DIR_NE, false);
    CHECK_TRUE(game->hasPreparedMove());
    STRCMP_EQUAL("33+b", game->getPreparedMoveString().c_str());

    game->rollbackPreparedMove();
    CHECK_FALSE(game->hasPreparedMove());
    STRCMP_EQUAL("", game->getPreparedMoveString().c_str());

    game->prepareAddProng(2, 2, octi::OctiPod(octi::A_POD), octi::DIR_NE, false);
    game->commitPreparedMove();
    CHECK_FALSE(game->hasPreparedMove());
    CHECK_EQUAL(octi::PLAYER_B, game->getTurn());
    CHECK_EQUAL(1, game->getNumMoves());
}

// ---------------------------------------------------------------------------
// Octi4PlayerGameTest
// ---------------------------------------------------------------------------

TEST_GROUP(Octi4PlayerGameTest)
{
};

TEST(Octi4PlayerGameTest, SetupAndCyclicTurns)
{
    octi::OctiBoard board(octi::GAME_MODE_4);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    CHECK_EQUAL(octi::PLAYER_A, game->getTurn());
    game->move("33+a");
    CHECK_EQUAL(octi::PLAYER_B, game->getTurn());
    game->move("37+e");
    CHECK_EQUAL(octi::PLAYER_C, game->getTurn());
    game->move("77+e");
    CHECK_EQUAL(octi::PLAYER_D, game->getTurn());
    game->move("73+a");
    CHECK_EQUAL(octi::PLAYER_A, game->getTurn());
}

TEST(Octi4PlayerGameTest, AlliedStacking)
{
    octi::OctiBoard board(octi::GAME_MODE_4);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    // Players A and C remain diagonal partners for base safety & hasEnemyOf(),
    // while same-owner pod stacking ([A, A]) is legal and partner stacking ([A, C]) is forbidden.
    CHECK_TRUE(octi::areAlliedPlayers(octi::GAME_MODE_4, octi::PLAYER_A, octi::PLAYER_C));
    CHECK_FALSE(board.squareAt(6, 6).hasEnemyOf(octi::PLAYER_A));
    CHECK_FALSE(board.squareAt(2, 2).hasEnemyOf(octi::PLAYER_C));
    CHECK_TRUE(board.squareAt(6, 6).hasOtherOwnerThan(octi::PLAYER_A));
    CHECK_FALSE(board.squareAt(6, 6).canEnterByStep(octi::PLAYER_A));
    CHECK_FALSE(board.squareAt(2, 2).hasOtherOwnerThan(octi::PLAYER_A));
    CHECK_TRUE(board.squareAt(2, 2).canEnterByStep(octi::PLAYER_A));
    CHECK_TRUE(board.squareAt(4, 4).canEnterByStep(octi::PLAYER_A));

    octi::OctiBoard ofkBoard(octi::GAME_MODE_OFK);
    ofkBoard.getGame()->start(octi::PLAYER_A);
    CHECK_FALSE(ofkBoard.squareAt(1, 1).hasOtherOwnerThan(octi::PLAYER_A));
    CHECK_FALSE(ofkBoard.squareAt(1, 1).canEnterByStep(octi::PLAYER_A));
    CHECK_TRUE(ofkBoard.squareAt(1, 2).canEnterByStep(octi::PLAYER_A));

    // Same-owner reserve stacking on own home base (33) is legal
    game->move("33");
    CHECK_EQUAL(2, board.squareAt(2, 2).getNumPods());

    // Partner 1-step onto diagonal partner pod must be rejected
    octi::OctiBoard stepBoard(octi::GAME_MODE_4);
    stepBoard.fromPositionString(
        "4:3:0:0:P:A:-:0:3,0,11,0:3,0,12,0:3,0,12,0:3,0,12,0:A44c,C54,B37,D73");
    CHECK_THROWS(octi::IllegalMoveException, stepBoard.getGame()->move("44c-54"));
}

TEST(Octi4PlayerGameTest, PartnerOneStepAndSuperProngPreludeStepRejected)
{
    // 1. Regular 1-step onto partner pod rejected in both move() and generateLegalMoves()
    {
        octi::OctiBoard board(octi::GAME_MODE_4);
        board.fromPositionString(
            "4:3:0:0:P:A:-:0:3,0,11,0:3,0,12,0:3,0,12,0:3,0,12,0:A44c,C54,B37,D73");
        octi::OctiGame *game = board.getGame();
        CHECK_THROWS(octi::IllegalMoveException, game->move("44c-54"));

        std::vector<octi::OctiMove> legal = game->generateLegalMoves();
        bool foundPartnerStep = false;
        bool foundVaultOverPartner = false;
        for (const auto &m : legal) {
            if (m.asString() == "44c-54") {
                foundPartnerStep = true;
            }
            if (m.asString() == "44c-64") {
                foundVaultOverPartner = true;
            }
        }
        CHECK_FALSE(foundPartnerStep);
        CHECK_TRUE(foundVaultOverPartner);

        // Vaulting over partner C54 onto empty 64 succeeds without capturing C54
        game->move("44c-64");
        CHECK_EQUAL(1, board.squareAt(4, 3).getNumPods());
        CHECK_TRUE(board.squareAt(4, 3).isOccupiedBy(octi::PLAYER_C));
        CHECK_EQUAL(1, board.squareAt(5, 3).getNumPods());
        CHECK_TRUE(board.squareAt(5, 3).isOccupiedBy(octi::PLAYER_A));
    }

    // 2. Composite move where a second pod 1-steps onto a partner pod is rejected
    {
        octi::OctiBoard compBoard(octi::GAME_MODE_4);
        compBoard.fromPositionString(
            "4:3:0:0:P:A:-:0:2,0,10,0:3,0,12,0:3,0,12,0:3,0,12,0:A44a,A44c,C54,B37,D73");
        CHECK_THROWS(octi::IllegalMoveException, compBoard.getGame()->move("44a-45,44c-54"));
    }

    // 3. SuperProng bonus prelude 1-step onto partner pod rejected in move() & move-gen
    {
        octi::OctiBoard spBoard(octi::GAME_MODE_4, true, false, octi::GAME_BASES_3);
        spBoard.fromPositionString(
            "4:3:1:0:P:A:-:0:3,0,11,0:3,0,11,1:3,0,11,1:3,0,11,1:A44c!,C54a,B37,D73");
        octi::OctiGame *spGame = spBoard.getGame();
        CHECK_THROWS(octi::IllegalMoveException, spGame->move("44c!-54;33"));

        std::vector<octi::OctiMove> spLegal = spGame->generateLegalMoves();
        bool foundSpPartnerStep = false;
        bool foundSpVault = false;
        for (const auto &m : spLegal) {
            const std::string s = m.asString();
            if (s.rfind("44c!-54;", 0) == 0) {
                foundSpPartnerStep = true;
            }
            if (s.rfind("44c!-64;", 0) == 0) {
                foundSpVault = true;
            }
        }
        CHECK_FALSE(foundSpPartnerStep);
        CHECK_TRUE(foundSpVault);
    }

    // 4. hasAnyLegalMove() returns false when only pod is blocked from stepping onto partner
    //    and jump landing is also blocked, with 0 free pods and 0 free prongs
    {
        octi::OctiBoard blockedBoard(octi::GAME_MODE_4);
        blockedBoard.fromPositionString(
            "4:3:0:0:P:A:-:0:0,0,0,0:3,0,12,0:2,0,12,0:3,0,12,0:A11abcdefgh,C21,C12,C22,B31,B13,B33,D73");
        CHECK_FALSE(blockedBoard.getGame()->hasAnyLegalMove());
    }
}

TEST(Octi4PlayerGameTest, OwnHomeReserveDropRejectedWhenPartnerOccupiesBase)
{
    octi::OctiBoard board(octi::GAME_MODE_4);
    board.fromPositionString(
        "4:3:0:0:P:A:-:0:2,0,11,0:3,0,12,0:3,0,11,0:3,0,12,0:C33e,A44a,B37,D73");
    octi::OctiGame *game = board.getGame();

    // Partner C occupies A's home square 33 -> A cannot deploy reserve pod "33"
    CHECK_THROWS(octi::IllegalMoveException, game->move("33"));
    for (const auto &m : game->generateLegalMoves()) {
        CHECK_TRUE(m.asString() != "33");
    }

    // Same check in 4P_SP move generation
    octi::OctiBoard spBoard(octi::GAME_MODE_4, true, false, octi::GAME_BASES_3);
    spBoard.fromPositionString(
        "4:3:1:0:P:A:-:0:2,0,11,0:3,0,11,1:3,0,10,1:3,0,11,1:C33e,A44a!,B37,D73");
    for (const auto &m : spBoard.getGame()->generateLegalMoves()) {
        const std::string s = m.asString();
        CHECK_TRUE(s != "33");
    }
}

TEST(Octi4PlayerGameTest, ZeroBoardPodTurnSkippedWithoutEliminationWhilePartnerOnHomeAndResumesAfterVacate)
{
    // Player A has 0 pods on the board and 2 reserve freePods; C is at 34 with a/e prongs;
    // A's home square 33 is currently empty. It is C's turn.
    octi::OctiBoard board(octi::GAME_MODE_4);
    board.fromPositionString(
        "4:3:0:0:P:C:-:0:2,1,12,0:3,0,12,0:3,0,10,0:3,0,12,0:C34ae,B37,D73");
    octi::OctiGame *game = board.getGame();

    // 1. Partner C steps onto A's empty home base 33 -> zero elimination of A!
    game->move("34ae-33");
    CHECK_FALSE(game->isPlayerEliminated(octi::PLAYER_A));
    CHECK_FALSE(board.squareAt(2, 2).isCaptured());
    CHECK_EQUAL(octi::PLAYER_D, game->getTurn());

    // 2. D plays "73+a" -> A has 0 board pods and C occupies 33, so A has 0 legal moves:
    //    A's turn is temporarily skipped to B WITHOUT marking A eliminated!
    game->move("73+a");
    CHECK_FALSE(game->isPlayerEliminated(octi::PLAYER_A));
    CHECK_FALSE(game->isGameOver());
    CHECK_EQUAL(octi::PLAYER_B, game->getTurn());

    // 3. B plays "37+e", then C vacates 33 ("33ae-34"), then D plays "73a-74":
    //    Now 33 is empty again, so A's turn resumes normally and A deploys "33"!
    game->move("37+e");
    CHECK_EQUAL(octi::PLAYER_C, game->getTurn());
    game->move("33ae-34");
    CHECK_EQUAL(octi::PLAYER_D, game->getTurn());
    game->move("73a-74");
    CHECK_FALSE(game->isPlayerEliminated(octi::PLAYER_A));
    CHECK_EQUAL(octi::PLAYER_A, game->getTurn());

    game->move("33");
    CHECK_EQUAL(1, game->getFreePods(octi::PLAYER_A));
    CHECK_TRUE(board.squareAt(2, 2).isOccupiedBy(octi::PLAYER_A));
}

TEST(Octi4PlayerGameTest, EmptyAndSelfOccupiedPartnerBaseEntryCausesZeroElimination)
{
    // 1. A steps onto C's empty corner base 77 -> legal, C is NOT eliminated, elimMask == 0
    octi::OctiBoard board(octi::GAME_MODE_4);
    board.fromPositionString(
        "4:3:0:0:P:A:-:0:2,0,10,0:3,0,12,0:3,0,11,0:3,0,12,0:A57c,A67c,B37,C66a,D73");
    octi::OctiGame *game = board.getGame();

    game->move("67c-77");
    CHECK_FALSE(game->isPlayerEliminated(octi::PLAYER_C));
    CHECK_FALSE(board.squareAt(6, 6).isCaptured());
    CHECK_TRUE(board.squareAt(6, 6).isOccupiedBy(octi::PLAYER_A));

    game->move("37+e");
    game->move("66a-67");
    game->move("73+a");

    // 2. Partner C at 67 CANNOT 1-step onto A-occupied own base 77, while A at 57 CAN jump over C67?
    //    Wait: 77 is occupied by A, so jump cannot land on 77, while if C steps 67->68, a second A pod
    //    can step onto A-occupied 77 ([A, A] same-owner stack on partner's base) with zero elimination!
    octi::OctiBoard selfStackBoard(octi::GAME_MODE_4);
    selfStackBoard.fromPositionString(
        "4:3:0:0:P:A:-:0:2,0,10,0:3,0,12,0:3,0,11,0:3,0,12,0:A67c,A77,B37,C66a,D73");
    selfStackBoard.getGame()->move("67c-77");
    CHECK_EQUAL(2, selfStackBoard.squareAt(6, 6).getNumPods());
    CHECK_FALSE(selfStackBoard.getGame()->isPlayerEliminated(octi::PLAYER_C));
    CHECK_FALSE(selfStackBoard.squareAt(6, 6).isCaptured());

    // 3. Jumping onto empty partner base 77 causes zero elimination
    octi::OctiBoard jumpBoard(octi::GAME_MODE_4);
    jumpBoard.fromPositionString(
        "4:3:0:0:P:A:-:0:3,0,11,0:3,0,12,0:3,0,12,0:3,0,12,0:A57c,C67,B37,D73");
    jumpBoard.getGame()->move("57c-77");
    CHECK_EQUAL(1, jumpBoard.squareAt(6, 6).getNumPods());
    CHECK_FALSE(jumpBoard.getGame()->isPlayerEliminated(octi::PLAYER_C));
    CHECK_FALSE(jumpBoard.squareAt(6, 6).isCaptured());
}

TEST(Octi4PlayerGameTest, MixedOwnerFenRejectedIn2PAnd4P)
{
    octi::OctiBoard b4p(octi::GAME_MODE_4);
    // Partner mixed stack in 4P
    CHECK_THROWS(
        octi::IllegalMoveSyntaxException,
        b4p.fromPositionString(
            "4:3:0:0:P:A:-:0:2,0,12,0:3,0,12,0:2,0,12,0:3,0,12,0:A55,C55,B37,D73"));
    // Enemy mixed stack in 4P
    CHECK_THROWS(
        octi::IllegalMoveSyntaxException,
        b4p.fromPositionString(
            "4:3:0:0:P:A:-:0:2,0,12,0:2,0,12,0:3,0,12,0:3,0,12,0:A44,B44,C77,D73"));
    // Enemy mixed stack in 2P
    octi::OctiBoard b2p(octi::GAME_MODE_2);
    CHECK_THROWS(
        octi::IllegalMoveSyntaxException,
        b2p.fromPositionString(
            "2:3:0:0:P:A:-:0:3,0,25,0:3,0,25,0:A44,B44,A33,B37"));

    // Same-owner stacks remain valid in both 4P and 2P
    b4p.fromPositionString(
        "4:3:0:0:P:A:-:0:2,0,11,0:3,0,12,0:3,0,12,0:3,0,12,0:A55a,A55,B37,C77,D73");
    CHECK_EQUAL(2, b4p.squareAt(4, 4).getNumPods());
    b2p.fromPositionString(
        "2:3:0:0:P:A:-:0:3,0,24,0:4,0,25,0:A44a,A44,A33,B37");
    CHECK_EQUAL(2, b2p.squareAt(3, 3).getNumPods());
}

TEST(Octi4PlayerGameTest, BotEvalJumpLandingEmptyAndPartnerHomeCongestionPenalty)
{
    const octi::BotTuningProfile prof =
        octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::MASTER);

    // 1. Bucket (B) leapfrogBattery jump landing must require destSq.isEmpty()
    octi::OctiBoard leapEmpty(octi::GAME_MODE_4);
    leapEmpty.fromPositionString(
        "4:3:0:0:P:A:-:0:3,0,11,0:3,0,12,0:3,0,12,0:3,0,12,0:A33a,C34,B37,D73");
    octi::OctiBoard leapBlockedByPartner(octi::GAME_MODE_4);
    leapBlockedByPartner.fromPositionString(
        "4:3:0:0:P:A:-:0:3,0,11,0:3,0,12,0:2,0,12,0:3,0,12,0:A33a,C34,C35,B37,D73");

    const octi::OctiEvalBreakdown bdLeapEmpty =
        octi::OctiBotEngine::evaluatePositionBreakdown(leapEmpty, octi::PLAYER_A, prof);
    const octi::OctiEvalBreakdown bdLeapBlocked =
        octi::OctiBotEngine::evaluatePositionBreakdown(leapBlockedByPartner, octi::PLAYER_A, prof);
    CHECK_TRUE(bdLeapEmpty.leapfrogBattery > 0);
    CHECK_EQUAL(0, bdLeapBlocked.leapfrogBattery);

    // 2. 4P partner-home spawn-blocking congestion penalty:
    //    C occupies A's home square 33 while A is non-eliminated:
    //    - freePods(A) == 0 -> 0 congestion penalty
    //    - freePods(A) == 2 && boardPods(A) > 0 -> -900 penalty
    //    - freePods(A) == 2 && boardPods(A) == 0 -> -4500 penalty
    octi::OctiBoard cOnAHomeNoFree(octi::GAME_MODE_4);
    cOnAHomeNoFree.fromPositionString(
        "4:3:0:0:P:A:-:0:0,3,12,0:3,0,12,0:3,0,12,0:3,0,12,0:C33,A44,B37,D73");
    octi::OctiBoard cAdjNoBlockNoFree(octi::GAME_MODE_4);
    cAdjNoBlockNoFree.fromPositionString(
        "4:3:0:0:P:A:-:0:0,3,12,0:3,0,12,0:3,0,12,0:3,0,12,0:C34,A44,B37,D73");

    octi::OctiBoard cOnAHomeWithBoardPod(octi::GAME_MODE_4);
    cOnAHomeWithBoardPod.fromPositionString(
        "4:3:0:0:P:A:-:0:2,1,12,0:3,0,12,0:3,0,12,0:3,0,12,0:C33,A44,B37,D73");
    octi::OctiBoard cAdjNoBlockWithBoardPod(octi::GAME_MODE_4);
    cAdjNoBlockWithBoardPod.fromPositionString(
        "4:3:0:0:P:A:-:0:2,1,12,0:3,0,12,0:3,0,12,0:3,0,12,0:C34,A44,B37,D73");

    octi::OctiBoard cOnAHomeZeroBoardPods(octi::GAME_MODE_4);
    cOnAHomeZeroBoardPods.fromPositionString(
        "4:3:0:0:P:B:-:0:2,2,12,0:3,0,12,0:3,0,12,0:3,0,12,0:C33,B37,D73");
    octi::OctiBoard cAdjNoBlockZeroBoardPods(octi::GAME_MODE_4);
    cAdjNoBlockZeroBoardPods.fromPositionString(
        "4:3:0:0:P:B:-:0:2,2,12,0:3,0,12,0:3,0,12,0:3,0,12,0:C34,B37,D73");

    const octi::OctiEvalBreakdown bdBlock0 =
        octi::OctiBotEngine::evaluatePositionBreakdown(cOnAHomeNoFree, octi::PLAYER_A, prof);
    const octi::OctiEvalBreakdown bdFree0 =
        octi::OctiBotEngine::evaluatePositionBreakdown(cAdjNoBlockNoFree, octi::PLAYER_A, prof);
    CHECK_EQUAL(bdFree0.structuralPositional, bdBlock0.structuralPositional);

    const octi::OctiEvalBreakdown bdBlockHasPod =
        octi::OctiBotEngine::evaluatePositionBreakdown(cOnAHomeWithBoardPod, octi::PLAYER_A, prof);
    const octi::OctiEvalBreakdown bdFreeHasPod =
        octi::OctiBotEngine::evaluatePositionBreakdown(cAdjNoBlockWithBoardPod, octi::PLAYER_A, prof);
    CHECK_EQUAL(-900, bdBlockHasPod.structuralPositional - bdFreeHasPod.structuralPositional);
    CHECK_EQUAL(bdBlockHasPod.sumTerms(), bdBlockHasPod.totalScore);

    const octi::OctiEvalBreakdown bdBlockZeroPods =
        octi::OctiBotEngine::evaluatePositionBreakdown(cOnAHomeZeroBoardPods, octi::PLAYER_A, prof);
    const octi::OctiEvalBreakdown bdFreeZeroPods =
        octi::OctiBotEngine::evaluatePositionBreakdown(cAdjNoBlockZeroBoardPods, octi::PLAYER_A, prof);
    CHECK_EQUAL(-4500, bdBlockZeroPods.structuralPositional - bdFreeZeroPods.structuralPositional);
    CHECK_EQUAL(bdBlockZeroPods.sumTerms(), bdBlockZeroPods.totalScore);

    const octi::OctiEvalBreakdown bdBlockZeroPodsB =
        octi::OctiBotEngine::evaluatePositionBreakdown(cOnAHomeZeroBoardPods, octi::PLAYER_B, prof);
    CHECK_EQUAL(-bdBlockZeroPods.totalScore, bdBlockZeroPodsB.totalScore);
}

TEST(Octi4PlayerGameTest, HomeBaseEliminationAndTurnSkipping)
{
    octi::OctiBoard board(octi::GAME_MODE_4);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    // Enemy A occupies B's home base (37)
    board.squareAt(2, 6).addPod(octi::OctiPod(octi::A_POD));
    game->move("33+b"); // End of A's turn checks 4P elimination -> B eliminated!

    CHECK_TRUE(game->isPlayerEliminated(octi::PLAYER_B));
    CHECK_FALSE(game->isPlayerEliminated(octi::PLAYER_A));
    CHECK_FALSE(game->isPlayerEliminated(octi::PLAYER_C));
    CHECK_FALSE(game->isPlayerEliminated(octi::PLAYER_D));

    // B's turn is skipped, next is C!
    CHECK_EQUAL(octi::PLAYER_C, game->getTurn());
}

// ---------------------------------------------------------------------------
// OctiForKidsGameTest
// ---------------------------------------------------------------------------

TEST_GROUP(OctiForKidsGameTest)
{
};

TEST(OctiForKidsGameTest, DimensionsAndPodSetup)
{
    octi::OctiBoard board(octi::GAME_MODE_OFK);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    CHECK_EQUAL(6, board.getBoardWidth());
    CHECK_EQUAL(7, board.getBoardHeight());
    CHECK_EQUAL(0, game->getFreePods(octi::PLAYER_A));
    CHECK_EQUAL(12, game->getFreeProngs(octi::PLAYER_A));
    CHECK_EQUAL(0, game->getFreeSuperProngs(octi::PLAYER_A));

    // Pods on 22, 32, 42, 52 and 26, 36, 46, 56
    CHECK_EQUAL(1, board.squareAt(1, 1).getNumPods());
    CHECK_EQUAL(1, board.squareAt(2, 1).getNumPods());
    CHECK_EQUAL(1, board.squareAt(3, 1).getNumPods());
    CHECK_EQUAL(1, board.squareAt(4, 1).getNumPods());
    CHECK_EQUAL(1, board.squareAt(1, 5).getNumPods());
    CHECK_EQUAL(1, board.squareAt(2, 5).getNumPods());
    CHECK_EQUAL(1, board.squareAt(3, 5).getNumPods());
    CHECK_EQUAL(1, board.squareAt(4, 5).getNumPods());
}

TEST(OctiForKidsGameTest, DisallowPodPlacementAndReposition)
{
    octi::OctiBoard board(octi::GAME_MODE_OFK);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    CHECK_THROWS(octi::IllegalMoveException, game->move("22"));

    game->move("22+a");
    game->move("26+e");
    CHECK_THROWS(octi::IllegalMoveSyntaxException, game->move("22a+b-a"));
}

TEST(OctiForKidsGameTest, DisallowStacking)
{
    octi::OctiBoard board(octi::GAME_MODE_OFK);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    game->move("22+c");
    game->move("26+e");
    // Move East to 32 where friendly pod already sits -> forbidden in OFK!
    CHECK_THROWS(octi::IllegalMoveException, game->move("22c-32"));
}

TEST(OctiForKidsGameTest, SingleBaseVictory)
{
    octi::OctiBoard board(octi::GAME_MODE_OFK);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    board.squareAt(1, 5).addPod(octi::OctiPod(octi::A_POD)); // A occupies B's base 26
    game->move("22+a");
    CHECK_TRUE(game->isGameOver());
    CHECK_EQUAL(octi::PLAYER_A, game->getWinner());
}

// ---------------------------------------------------------------------------
// OctiRulebookExamplesTest
// ---------------------------------------------------------------------------

TEST_GROUP(OctiRulebookExamplesTest)
{
};

TEST(OctiRulebookExamplesTest, Example1_MovingStackedPods)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    // Setup 3 pods at 33:
    // Bottom: ↖→ (h, c)
    // Middle: ↑ (a)
    // Top: ↖ (h)
    board.squareAt(2, 2).clear();
    octi::OctiPod pBottom(octi::A_POD);
    pBottom.addProng(octi::DIR_NW, false);
    pBottom.addProng(octi::DIR_E, false);
    octi::OctiPod pMiddle(octi::A_POD);
    pMiddle.addProng(octi::DIR_N, false);
    octi::OctiPod pTop(octi::A_POD);
    pTop.addProng(octi::DIR_NW, false);

    board.squareAt(2, 2).addPod(pBottom);
    board.squareAt(2, 2).addPod(pMiddle);
    board.squareAt(2, 2).addPod(pTop);

    // Top moves to 24 (A), Middle moves to 34 (B), Bottom stays at 33
    // Composite move notation: "33h-24,33a-34"
    game->move("33h-24,33a-34");
    CHECK_EQUAL(1, board.squareAt(1, 3).getNumPods()); // 24
    CHECK_EQUAL(1, board.squareAt(2, 3).getNumPods()); // 34
    CHECK_EQUAL(1, board.squareAt(2, 2).getNumPods()); // 33 (bottom pod remained)
}

TEST(OctiRulebookExamplesTest, Example2_JumpingWithStackedPods)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    // Setup Illustrated Example 2 from doc/octi_game_instructions.md
    // Bottom pod jumps to A then B, top pod jumps to B then C
    board.clear();
    octi::OctiPod pBottom(octi::A_POD);
    pBottom.addProng(octi::DIR_NW, false); // h
    pBottom.addProng(octi::DIR_E, false);  // c
    octi::OctiPod pTop(octi::A_POD);
    pTop.addProng(octi::DIR_N, false);     // a

    board.squareAt(2, 0).addPod(pBottom); // 31
    board.squareAt(2, 0).addPod(pTop);

    // Stationary pods on jumped squares
    board.squareAt(1, 1).addPod(octi::OctiPod(octi::B_POD)); // 22
    board.squareAt(2, 1).addPod(octi::OctiPod(octi::A_POD)); // 32
    board.squareAt(1, 2).addPod(octi::OctiPod(octi::A_POD)); // 23
    board.squareAt(2, 3).addPod(octi::OctiPod(octi::B_POD)); // 34

    // Bottom pod jumps 31 -> 13 -> 33
    // Top pod jumps 31 -> 33 -> 35
    game->move("31ch-13-33,31a-33-35");
    CHECK_EQUAL(1, board.squareAt(2, 2).getNumPods()); // 33
    CHECK_EQUAL(1, board.squareAt(2, 4).getNumPods()); // 35
}

TEST(OctiRulebookExamplesTest, Example3_EdgelessMovesAndJumps)
{
    octi::OctiBoard board(octi::GAME_MODE_2, false, true /* edgeless */);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    // Pod at (0, 8) [19] with ↖ moves wrap-around to (8, 0) [91]
    board.clear();
    octi::OctiPod p1(octi::A_POD);
    p1.addProng(octi::DIR_NW, false);
    board.squareAt(0, 8).addPod(p1);

    game->move("19h-91");
    CHECK_EQUAL(0, board.squareAt(0, 8).getNumPods());
    CHECK_EQUAL(1, board.squareAt(8, 0).getNumPods());
}

TEST(OctiRulebookExamplesTest, ReservePodPlacementOnOccupiedAndEmptyFriendlyHomeSquares)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    // Initial 2P setup: Player A has pods at (2, 2) [33], (4, 2) [53], (6, 2) [73].
    // Player A has 4 free reserve pods.
    CHECK_EQUAL(4, game->getFreePods(octi::PLAYER_A));
    CHECK_EQUAL(1, board.squareAt(2, 2).getNumPods());

    // 1. Placing a reserve pod onto an already occupied friendly home square (33): "+c3" / "33"
    game->move("33");
    CHECK_EQUAL(2, board.squareAt(2, 2).getNumPods());
    CHECK_EQUAL(3, game->getFreePods(octi::PLAYER_A));
    CHECK_EQUAL(octi::PLAYER_B, game->getTurn());

    // Player B passes turn with a dummy prong addition on (2, 6) [37]:
    game->move("37+e");
    CHECK_EQUAL(octi::PLAYER_A, game->getTurn());

    // 2. Move pod from 53 (4, 2) to 54 (4, 3):
    // First give it a North prong so it can step North
    game->move("53+a");
    game->move("57+e");
    // Move from 53 to 54
    game->move("53a-54");
    // Now square (4, 2) [53] is an EMPTY friendly home square!
    CHECK_TRUE(board.squareAt(4, 2).isEmpty());
    CHECK_EQUAL(octi::PLAYER_B, game->getTurn());

    // Player B dummy move
    game->move("77+a");
    CHECK_EQUAL(octi::PLAYER_A, game->getTurn());

    // 3. Placing a reserve pod onto an EMPTY friendly home square (53): "53"
    game->move("53");
    CHECK_EQUAL(1, board.squareAt(4, 2).getNumPods());
    CHECK_EQUAL(2, game->getFreePods(octi::PLAYER_A));

    // 4. Verify illegal placements on non-friendly squares
    CHECK_EQUAL(octi::PLAYER_B, game->getTurn());
    // B cannot place pod on empty neutral square (0, 0) [11]
    CHECK_THROWS(octi::IllegalMoveException, game->move("11"));
    // B cannot place pod on Player A's home square (2, 2) [33]
    CHECK_THROWS(octi::IllegalMoveException, game->move("33"));
}

TEST(OctiRulebookExamplesTest, Randomize18PlySequence)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->randomize();

    CHECK_EQUAL(18, game->getNumMoves());
    CHECK_TRUE(game->isGameStarted());
    CHECK_FALSE(game->isGameOver());
    CHECK_EQUAL(octi::PLAYER_A, game->getTurn());
}

// ---------------------------------------------------------------------------
// OctiSerializationTest
// ---------------------------------------------------------------------------

TEST_GROUP(OctiSerializationTest)
{
};

TEST(OctiSerializationTest, OctiFenRoundTrip2Player)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    std::string fenStart = board.toPositionString();
    STRCMP_EQUAL("2:3:0:0:P:A:-:0:4,0,25,0:4,0,25,0:A33,A53,A73,B37,B57,B77", fenStart.c_str());

    octi::OctiBoard checkBoard;
    checkBoard.fromPositionString(fenStart);
    STRCMP_EQUAL(fenStart.c_str(), checkBoard.toPositionString().c_str());
}

TEST(OctiSerializationTest, OctiFenRoundTripOFK)
{
    octi::OctiBoard board(octi::GAME_MODE_OFK);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    std::string fenStart = board.toPositionString();
    STRCMP_EQUAL("OFK:1:0:0:P:A:-:0:0,0,12,0:0,0,12,0:A22,A32,A42,A52,B26,B36,B46,B56", fenStart.c_str());

    octi::OctiBoard checkBoard;
    checkBoard.fromPositionString(fenStart);
    STRCMP_EQUAL(fenStart.c_str(), checkBoard.toPositionString().c_str());
}

TEST(OctiSerializationTest, GameTranscriptMinimalAndAnnotated)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    game->move("33+b");
    game->move("37+e");
    game->move("33");
    game->move("37");

    std::string canonicalMoves = board.exportCanonicalMovesText();
    STRCMP_EQUAL("33+b\n37+e\n33\n37\n", canonicalMoves.c_str());

    std::string minimal = board.serializeGame();
    CHECK_TRUE(minimal.find("\"format\": \"octi.transcript.v3\"") != std::string::npos);
    CHECK_TRUE(minimal.find("\"plies\": [") != std::string::npos);

    octi::OctiBoard replayMinimal;
    replayMinimal.deserializeGame(minimal);
    CHECK_EQUAL(4, replayMinimal.getGame()->getNumMoves());
    CHECK_EQUAL(board.toPositionString(), replayMinimal.toPositionString());

    // Legacy v2.0 transcript must be rejected with IllegalMoveSyntaxException
    std::string legacy =
        "; OCTI Game Transcript v2.0\n"
        "GAME_MODE: 2\n"
        "GAME_BASES: 3\n"
        "USE_SUPER_PRONGS: No\n"
        "USE_EDGELESS_BOARD: No\n"
        "START_PLAYER: A\n"
        "STATUS: IN_PROGRESS\n"
        "WINNER: NONE\n"
        "MOVES: 4\n"
        "33+b\n37+e\n33\n37\n";

    octi::OctiBoard replayLegacy;
    CHECK_THROWS(octi::IllegalMoveSyntaxException, replayLegacy.deserializeGame(legacy));
}

// ---------------------------------------------------------------------------
// RapidCheckStateProperties
// ---------------------------------------------------------------------------

TEST_GROUP(RapidCheckStateProperties)
{
};

TEST(RapidCheckStateProperties, RandomLegalMoveWalk2P)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    bool passed = rc::check("OctiGame 2P random legal move walk", []() {
        octi::OctiBoard board(octi::GAME_MODE_2);
        octi::OctiGame *game = board.getGame();
        RC_ASSERT(game != nullptr);
        game->start(octi::PLAYER_A);

        std::string initialFen = board.toPositionString();
        int numSteps = *rc::gen::inRange(5, 16);

        for (int step = 0; step < numSteps; step++) {
            if (game->isGameOver()) {
                break;
            }
            std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
            if (legalMoves.empty()) {
                break;
            }
            int moveIdx = *rc::gen::inRange(0, static_cast<int>(legalMoves.size()));
            const octi::OctiMove &selected = legalMoves[moveIdx];

            game->move(selected);

            std::string fenAfter = board.toPositionString();
            octi::OctiBoard checkBoard;
            checkBoard.fromPositionString(fenAfter);
            RC_ASSERT(checkBoard.toPositionString() == fenAfter);
        }

        std::string transcript = board.serializeGame();
        octi::OctiBoard replayBoard;
        replayBoard.deserializeGame(transcript);
        RC_ASSERT(replayBoard.toPositionString() == board.toPositionString());
        RC_ASSERT(replayBoard.getGame()->getNumMoves() == game->getNumMoves());
        RC_ASSERT(replayBoard.getGame()->getTurn() == game->getTurn());

        int movesMade = game->getNumMoves();
        for (int u = 0; u < movesMade; u++) {
            game->undo();
        }
        RC_ASSERT(board.toPositionString() == initialFen);

        for (int r = 0; r < movesMade; r++) {
            game->redo();
        }
        RC_ASSERT(board.toPositionString() == replayBoard.toPositionString());
    });
    CHECK_TRUE(passed);
}

TEST(RapidCheckStateProperties, RandomLegalMoveWalk4P)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    bool passed = rc::check("OctiGame 4P/4P_SP/4P_ED/4P_SPED single-owner & generator/validator parity walk", []() {
        const bool useSp = *rc::gen::arbitrary<bool>();
        const bool useEd = *rc::gen::arbitrary<bool>();
        octi::OctiBoard board(octi::GAME_MODE_4, useSp, useEd, octi::GAME_BASES_3);
        octi::OctiGame *game = board.getGame();
        RC_ASSERT(game != nullptr);
        game->start(octi::PLAYER_A);

        auto assertSingleOwnerGrid = [](const octi::OctiBoard &b) {
            for (int x = 0; x < b.getBoardWidth(); ++x) {
                for (int y = 0; y < b.getBoardHeight(); ++y) {
                    const octi::OctiSquare &sq = b.squareAt(x, y);
                    octi::OctiPlayer firstOwner = octi::PLAYER_UNDEF;
                    for (int i = 0; i < sq.getNumPods(); ++i) {
                        octi::OctiPlayer owner = octi::podTypeToPlayer(sq.getPodAt(i).getType());
                        RC_ASSERT(owner != octi::PLAYER_UNDEF);
                        if (firstOwner == octi::PLAYER_UNDEF) {
                            firstOwner = owner;
                        } else {
                            RC_ASSERT(owner == firstOwner);
                        }
                    }
                }
            }
        };

        assertSingleOwnerGrid(board);
        std::string initialFen = board.toPositionString();
        int numSteps = *rc::gen::inRange(6, 16);

        for (int step = 0; step < numSteps; step++) {
            if (game->isGameOver()) {
                break;
            }
            std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
            RC_ASSERT(game->hasAnyLegalMove() == !legalMoves.empty());
            if (legalMoves.empty()) {
                break;
            }

            // Sample up to 6 generated moves on an isolated scratch board (preserving _history/_undoStack)
            const std::string stepFen = board.toPositionString();
            int sampleCount = std::min<int>(6, static_cast<int>(legalMoves.size()));
            for (int s = 0; s < sampleCount; ++s) {
                int sIdx = (s == 0)
                    ? *rc::gen::inRange(0, static_cast<int>(legalMoves.size()))
                    : ((s * 7) % static_cast<int>(legalMoves.size()));
                octi::OctiBoard sampleBoard;
                sampleBoard.suppressBoardUpdateNotification(true);
                sampleBoard.fromPositionString(stepFen);
                sampleBoard.getGame()->setSearchMode(true);
                sampleBoard.getGame()->move(legalMoves[sIdx]);
                assertSingleOwnerGrid(sampleBoard);
            }

            // Also verify validator rejects own-home reserve drop if another player occupies own home
            const octi::OctiPlayer curTurn = game->getTurn();
            static const int kHomeCoords[5][2] = {
                {-1, -1}, {2, 2}, {2, 6}, {6, 6}, {6, 2}
            };
            const int hx = kHomeCoords[static_cast<int>(curTurn)][0];
            const int hy = kHomeCoords[static_cast<int>(curTurn)][1];
            const octi::OctiSquare &ownHomeSq = board.squareAt(hx, hy);
            if (!ownHomeSq.isEmpty() && ownHomeSq.hasOtherOwnerThan(curTurn)) {
                std::string homeDrop = std::to_string(hx + 1) + std::to_string(hy + 1);
                for (const auto &lm : legalMoves) {
                    RC_ASSERT(lm.asString() != homeDrop);
                }
                octi::OctiBoard rejectBoard;
                rejectBoard.suppressBoardUpdateNotification(true);
                rejectBoard.fromPositionString(stepFen);
                bool threw = false;
                try {
                    rejectBoard.getGame()->move(homeDrop);
                } catch (const octi::IllegalMoveException &) {
                    threw = true;
                }
                RC_ASSERT(threw);
            }

            int moveIdx = *rc::gen::inRange(0, static_cast<int>(legalMoves.size()));
            const octi::OctiMove &selected = legalMoves[moveIdx];

            game->move(selected);
            assertSingleOwnerGrid(board);

            std::string fenAfter = board.toPositionString();
            octi::OctiBoard checkBoard;
            checkBoard.fromPositionString(fenAfter);
            RC_ASSERT(checkBoard.toPositionString() == fenAfter);
            assertSingleOwnerGrid(checkBoard);
        }

        std::string transcript = board.serializeGame();
        octi::OctiBoard replayBoard;
        replayBoard.deserializeGame(transcript);
        RC_ASSERT(replayBoard.toPositionString() == board.toPositionString());

        int movesMade = game->getNumMoves();
        for (int u = 0; u < movesMade; u++) {
            game->undo();
        }
        RC_ASSERT(board.toPositionString() == initialFen);

        for (int r = 0; r < movesMade; r++) {
            game->redo();
        }
        RC_ASSERT(board.toPositionString() == replayBoard.toPositionString());
    });
    CHECK_TRUE(passed);
}

TEST(RapidCheckStateProperties, RandomLegalMoveWalkOFK)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    bool passed = rc::check("OctiGame OFK random legal move walk", []() {
        octi::OctiBoard board(octi::GAME_MODE_OFK);
        octi::OctiGame *game = board.getGame();
        RC_ASSERT(game != nullptr);
        game->start(octi::PLAYER_A);

        std::string initialFen = board.toPositionString();
        int numSteps = *rc::gen::inRange(4, 12);

        for (int step = 0; step < numSteps; step++) {
            if (game->isGameOver()) {
                break;
            }
            std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
            if (legalMoves.empty()) {
                break;
            }
            int moveIdx = *rc::gen::inRange(0, static_cast<int>(legalMoves.size()));
            const octi::OctiMove &selected = legalMoves[moveIdx];

            game->move(selected);

            std::string fenAfter = board.toPositionString();
            octi::OctiBoard checkBoard;
            checkBoard.fromPositionString(fenAfter);
            RC_ASSERT(checkBoard.toPositionString() == fenAfter);
        }

        std::string transcript = board.serializeGame();
        octi::OctiBoard replayBoard;
        replayBoard.deserializeGame(transcript);
        RC_ASSERT(replayBoard.toPositionString() == board.toPositionString());

        int movesMade = game->getNumMoves();
        for (int u = 0; u < movesMade; u++) {
            game->undo();
        }
        RC_ASSERT(board.toPositionString() == initialFen);

        for (int r = 0; r < movesMade; r++) {
            game->redo();
        }
        RC_ASSERT(board.toPositionString() == replayBoard.toPositionString());
    });
    CHECK_TRUE(passed);
}

// ---------------------------------------------------------------------------
// OctiGhostPodTest
//
// Ghost (vacated) pods exist only on the moving stack's own starting square.
// A different square that merely holds an own pod with identical prongs must
// be treated as an ordinary occupied square for jump-over and landing checks.
// ---------------------------------------------------------------------------

namespace {

// Each scenario is evaluated twice (as separate tests) so that a failure in
// generateLegalMoves() cannot mask a failure in move() and vice versa.
enum GhostAspect {
    GHOST_ASPECT_GENERATOR,
    GHOST_ASPECT_MOVE
};

octi::OctiPod ghostTestPod(octi::OctiPodType type,
                           std::initializer_list<octi::OctiDir> prongs)
{
    octi::OctiPod pod(type);
    for (octi::OctiDir d : prongs) {
        pod.addProng(d, false);
    }
    return pod;
}

octi::OctiMove ghostTestJump(int x0, int y0, const octi::OctiPod &pod,
                             std::initializer_list<std::pair<int, int> > hops)
{
    octi::OctiMoveOp op;
    op.x0 = x0;
    op.y0 = y0;
    op.pod = pod;
    for (const auto &h : hops) {
        octi::OctiHop hop;
        hop.x = h.first;
        hop.y = h.second;
        hop.capture = false;
        op.hops.push_back(hop);
    }
    return octi::OctiMove(std::vector<octi::OctiMoveOp>{op});
}

bool ghostTestContains(const std::vector<octi::OctiMove> &moves,
                       const octi::OctiMove &m)
{
    for (const auto &candidate : moves) {
        if (candidate == m) {
            return true;
        }
    }
    return false;
}

// Applies m through move(); returns true when accepted, false when the engine
// raised IllegalMoveException.
bool ghostTestAccepts(octi::OctiGame *game, const octi::OctiMove &m)
{
    try {
        game->move(m);
    } catch (const octi::IllegalMoveException &) {
        return false;
    }
    return true;
}

// (i) Landing on a DIFFERENT square holding an identical own pod is illegal.
void ghostTestLandingOnIdenticalOwnPod(octi::OctiGameMode mode, GhostAspect aspect)
{
    octi::OctiBoard board(mode);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);
    board.clear();

    const octi::OctiPod mover = ghostTestPod(octi::A_POD, {octi::DIR_E});
    board.squareAt(0, 3).addPod(mover);                                // mover
    board.squareAt(1, 3).addPod(ghostTestPod(octi::A_POD, {}));        // jumped
    board.squareAt(2, 3).addPod(mover);                                // identical, NOT vacated

    const std::string before = board.toPositionString();
    const octi::OctiMove jump = ghostTestJump(0, 3, mover, {{2, 3}});

    const bool generated = ghostTestContains(game->generateLegalMoves(), jump);
    const bool accepted = ghostTestAccepts(game, jump);
    const std::string after = board.toPositionString();

    if (aspect == GHOST_ASPECT_GENERATOR) {
        CHECK_TEXT(!generated, "generateLegalMoves() emitted a jump landing on an identical own pod");
    } else {
        CHECK_TEXT(!accepted, "move() accepted a jump landing on an identical own pod");
        STRCMP_EQUAL(before.c_str(), after.c_str());
    }
}

// (ii) Jumping OVER a DIFFERENT square holding an identical own pod is legal
// when the landing square is empty.
void ghostTestJumpOverIdenticalOwnPod(octi::OctiGameMode mode, GhostAspect aspect)
{
    octi::OctiBoard board(mode);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);
    board.clear();

    const octi::OctiPod mover = ghostTestPod(octi::A_POD, {octi::DIR_E});
    board.squareAt(0, 3).addPod(mover);   // mover
    board.squareAt(1, 3).addPod(mover);   // jumped: identical own pod, stationary

    const octi::OctiMove jump = ghostTestJump(0, 3, mover, {{2, 3}});

    const bool generated = ghostTestContains(game->generateLegalMoves(), jump);
    const bool accepted = ghostTestAccepts(game, jump);

    if (aspect == GHOST_ASPECT_GENERATOR) {
        CHECK_TEXT(generated, "generateLegalMoves() omitted a jump over an identical own pod");
    } else {
        CHECK_TEXT(accepted, "move() rejected a jump over an identical own pod");
        CHECK_EQUAL(0, board.squareAt(0, 3).getNumPods());
        CHECK_EQUAL(1, board.squareAt(1, 3).getNumPods());
        CHECK_EQUAL(1, board.squareAt(2, 3).getNumPods());
    }
}

// (iii) Looping back to the vacated start square stays legal.
void ghostTestLoopBackToVacatedStart(octi::OctiGameMode mode, GhostAspect aspect)
{
    octi::OctiBoard board(mode);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);
    board.clear();

    const octi::OctiPod mover =
        ghostTestPod(octi::A_POD, {octi::DIR_E, octi::DIR_N, octi::DIR_SW});
    board.squareAt(1, 3).addPod(mover);                                // start
    board.squareAt(2, 3).addPod(ghostTestPod(octi::A_POD, {}));        // jumped by E hop
    board.squareAt(3, 4).addPod(ghostTestPod(octi::A_POD, {}));        // jumped by N hop
    board.squareAt(2, 4).addPod(ghostTestPod(octi::A_POD, {}));        // jumped by SW hop

    const octi::OctiMove loop = ghostTestJump(1, 3, mover, {{3, 3}, {3, 5}, {1, 3}});

    const bool generated = ghostTestContains(game->generateLegalMoves(), loop);
    const bool accepted = ghostTestAccepts(game, loop);

    if (aspect == GHOST_ASPECT_GENERATOR) {
        CHECK_TEXT(generated, "generateLegalMoves() omitted a loop back to the vacated start square");
    } else {
        CHECK_TEXT(accepted, "move() rejected a loop back to the vacated start square");
        CHECK_EQUAL(1, board.squareAt(1, 3).getNumPods());
        CHECK_EQUAL(0, board.squareAt(3, 3).getNumPods());
        CHECK_EQUAL(0, board.squareAt(3, 5).getNumPods());
    }
}

// Composite stack move: two identical pods on one start square both jump to
// the same destination over one stationary pod (rulebook: "A stack may jump
// to the same destination"). Stacks do not exist in OCTI for Kids.
void ghostTestStackJumpsToSameDestination(octi::OctiGameMode mode)
{
    octi::OctiBoard board(mode);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);
    board.clear();

    const octi::OctiPod mover = ghostTestPod(octi::A_POD, {octi::DIR_E});
    board.squareAt(0, 3).addPod(mover);
    board.squareAt(0, 3).addPod(mover);
    board.squareAt(1, 3).addPod(ghostTestPod(octi::A_POD, {}));

    octi::OctiMoveOp op;
    op.x0 = 0;
    op.y0 = 3;
    op.pod = mover;
    octi::OctiHop hop;
    hop.x = 2;
    hop.y = 3;
    hop.capture = false;
    op.hops.push_back(hop);
    const octi::OctiMove both(std::vector<octi::OctiMoveOp>{op, op});

    const bool accepted = ghostTestAccepts(game, both);
    CHECK_TEXT(accepted, "move() rejected a stack jumping to the same destination");
    if (accepted) {
        CHECK_EQUAL(0, board.squareAt(0, 3).getNumPods());
        CHECK_EQUAL(1, board.squareAt(1, 3).getNumPods());
        CHECK_EQUAL(2, board.squareAt(2, 3).getNumPods());
    }
}

// (iv) Every generated legal move must be accepted by move() and must be
// fully reversible (position string and transcript restored by undo()).
bool ghostTestGeneratedMovesAcceptedProperty(octi::OctiGameMode mode, const char *label)
{
    return rc::check(label, [mode]() {
        octi::OctiBoard board(mode);
        octi::OctiGame *game = board.getGame();
        RC_ASSERT(game != nullptr);
        game->start(octi::PLAYER_A);

        const int numSteps = *rc::gen::inRange(5, 16);
        for (int step = 0; step < numSteps; step++) {
            if (game->isGameOver()) {
                break;
            }
            std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
            if (legalMoves.empty()) {
                break;
            }

            const std::string fenBefore = board.toPositionString();
            const std::string transcriptBefore = board.serializeGame();
            const int movesBefore = game->getNumMoves();

            for (const auto &m : legalMoves) {
                bool accepted = true;
                std::string reason;
                try {
                    game->move(m);
                } catch (const octi::IllegalMoveException &e) {
                    accepted = false;
                    reason = e.what();
                }
                if (!accepted) {
                    RC_LOG() << "step " << step << " move " << m.asString()
                             << " rejected: " << reason << std::endl
                             << "position: " << fenBefore << std::endl;
                }
                RC_ASSERT(accepted);
                RC_ASSERT(game->getNumMoves() == movesBefore + 1);
                game->undo();
                RC_ASSERT(game->getNumMoves() == movesBefore);
                RC_ASSERT(board.toPositionString() == fenBefore);
                RC_ASSERT(board.serializeGame() == transcriptBefore);
            }

            const int moveIdx = *rc::gen::inRange(0, static_cast<int>(legalMoves.size()));
            game->move(legalMoves[static_cast<std::size_t>(moveIdx)]);
        }
    });
}

} // namespace

TEST_GROUP(OctiGhostPodTest)
{
};

#define GHOST_TEST_ALL_MODES(scenarioName, scenarioFn)                         \
    TEST(OctiGhostPodTest, scenarioName##Generator2P)                          \
    { scenarioFn(octi::GAME_MODE_2, GHOST_ASPECT_GENERATOR); }                 \
    TEST(OctiGhostPodTest, scenarioName##Generator4P)                          \
    { scenarioFn(octi::GAME_MODE_4, GHOST_ASPECT_GENERATOR); }                 \
    TEST(OctiGhostPodTest, scenarioName##GeneratorOFK)                         \
    { scenarioFn(octi::GAME_MODE_OFK, GHOST_ASPECT_GENERATOR); }               \
    TEST(OctiGhostPodTest, scenarioName##Move2P)                               \
    { scenarioFn(octi::GAME_MODE_2, GHOST_ASPECT_MOVE); }                      \
    TEST(OctiGhostPodTest, scenarioName##Move4P)                               \
    { scenarioFn(octi::GAME_MODE_4, GHOST_ASPECT_MOVE); }                      \
    TEST(OctiGhostPodTest, scenarioName##MoveOFK)                              \
    { scenarioFn(octi::GAME_MODE_OFK, GHOST_ASPECT_MOVE); }

GHOST_TEST_ALL_MODES(LandingOnIdenticalOwnPod, ghostTestLandingOnIdenticalOwnPod)
GHOST_TEST_ALL_MODES(JumpOverIdenticalOwnPod, ghostTestJumpOverIdenticalOwnPod)
GHOST_TEST_ALL_MODES(LoopBackToVacatedStart, ghostTestLoopBackToVacatedStart)

TEST(OctiGhostPodTest, StackJumpsToSameDestination2P)
{
    ghostTestStackJumpsToSameDestination(octi::GAME_MODE_2);
}

TEST(OctiGhostPodTest, StackJumpsToSameDestination4P)
{
    ghostTestStackJumpsToSameDestination(octi::GAME_MODE_4);
}

TEST(OctiGhostPodTest, GeneratedMovesAcceptedAndReversible2P)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    CHECK_TRUE(ghostTestGeneratedMovesAcceptedProperty(
                   octi::GAME_MODE_2, "2P every generated move is accepted and reversible"));
}

TEST(OctiGhostPodTest, GeneratedMovesAcceptedAndReversible4P)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    CHECK_TRUE(ghostTestGeneratedMovesAcceptedProperty(
                   octi::GAME_MODE_4, "4P every generated move is accepted and reversible"));
}

TEST(OctiGhostPodTest, GeneratedMovesAcceptedAndReversibleOFK)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    CHECK_TRUE(ghostTestGeneratedMovesAcceptedProperty(
                   octi::GAME_MODE_OFK, "OFK every generated move is accepted and reversible"));
}

// ---------------------------------------------------------------------------
// OctiBotEngineCoreTest
// ---------------------------------------------------------------------------

TEST_GROUP(OctiBotEngineCoreTest)
{
};

TEST(OctiBotEngineCoreTest, CanonicalServerHostAndPortConstants)
{
    STRCMP_EQUAL("octi.selfso.com", octi::OCTI_SERVER_HOST);
    LONGS_EQUAL(443, octi::OCTI_SERVER_PORT);
    STRCMP_EQUAL("trumpet.selfso.com", octi::OCTI_SERVER_FALLBACK_HOST);
    LONGS_EQUAL(6874, octi::OCTI_SERVER_FALLBACK_PORT);
    LONGS_EQUAL(6874, octi::OCTI_SERVER_DAEMON_PORT);
}

TEST(OctiBotEngineCoreTest, DefaultBudgetAndDepthPerDifficultyLevel)
{
    CHECK_EQUAL(5000, octi::OctiBotEngine::defaultBudgetForDifficulty(octi::BotDifficulty::CASUAL));
    CHECK_EQUAL(10000, octi::OctiBotEngine::defaultBudgetForDifficulty(octi::BotDifficulty::TACTICAL));
    CHECK_EQUAL(20000, octi::OctiBotEngine::defaultBudgetForDifficulty(octi::BotDifficulty::MASTER));
    CHECK_EQUAL(40000, octi::OctiBotEngine::defaultBudgetForDifficulty(octi::BotDifficulty::EXPERT));
    CHECK_EQUAL(60000, octi::OctiBotEngine::defaultBudgetForDifficulty(octi::BotDifficulty::GRANDMASTER));

    CHECK_EQUAL(3, octi::OctiBotEngine::maxDepthForDifficulty(octi::BotDifficulty::CASUAL));
    CHECK_EQUAL(64, octi::OctiBotEngine::maxDepthForDifficulty(octi::BotDifficulty::TACTICAL));
    CHECK_EQUAL(64, octi::OctiBotEngine::maxDepthForDifficulty(octi::BotDifficulty::MASTER));
    CHECK_EQUAL(64, octi::OctiBotEngine::maxDepthForDifficulty(octi::BotDifficulty::EXPERT));
    CHECK_EQUAL(64, octi::OctiBotEngine::maxDepthForDifficulty(octi::BotDifficulty::GRANDMASTER));

    CHECK_EQUAL(24, octi::OctiBotEngine::rootCapForDifficulty(octi::BotDifficulty::CASUAL));
    CHECK_EQUAL(40, octi::OctiBotEngine::rootCapForDifficulty(octi::BotDifficulty::TACTICAL));
    CHECK_EQUAL(56, octi::OctiBotEngine::rootCapForDifficulty(octi::BotDifficulty::MASTER));
    CHECK_EQUAL(64, octi::OctiBotEngine::rootCapForDifficulty(octi::BotDifficulty::EXPERT));
    CHECK_EQUAL(72, octi::OctiBotEngine::rootCapForDifficulty(octi::BotDifficulty::GRANDMASTER));

    CHECK_EQUAL(16, octi::OctiBotEngine::branchCapForDifficulty(octi::BotDifficulty::CASUAL));
    CHECK_EQUAL(24, octi::OctiBotEngine::branchCapForDifficulty(octi::BotDifficulty::TACTICAL));
    CHECK_EQUAL(32, octi::OctiBotEngine::branchCapForDifficulty(octi::BotDifficulty::MASTER));
    CHECK_EQUAL(36, octi::OctiBotEngine::branchCapForDifficulty(octi::BotDifficulty::EXPERT));
    CHECK_EQUAL(40, octi::OctiBotEngine::branchCapForDifficulty(octi::BotDifficulty::GRANDMASTER));
}

TEST(OctiBotEngineCoreTest, ImmediateOnePlyWinningBaseCaptureAllDifficulties)
{
    for (octi::BotDifficulty diff : {
             octi::BotDifficulty::CASUAL,
             octi::BotDifficulty::TACTICAL,
             octi::BotDifficulty::MASTER }) {
        octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
        octi::OctiGame *game = board.getGame();
        game->start(octi::PLAYER_A);
        board.clear();

        // Player A pod at 36 (0-based (2, 5)) with North prong 'a' can step onto B's base 37 (0-based (2, 6)) to win immediately
        octi::OctiPod podA(octi::A_POD);
        podA.addProng(octi::DIR_N, false);
        board.squareAt(2, 5).addPod(podA);

        octi::OctiPod podB(octi::B_POD);
        board.squareAt(6, 6).addPod(podB);

        std::string fenBefore = board.toPositionString();
        octi::OctiBotEngine bot(diff, 0x12345678ULL);
        bot.setTimeBudgetMs(25);
        octi::BotSearchStats stats;
        std::string moveStr = bot.chooseMove(board, &stats);

        // Input board must not be mutated
        STRCMP_EQUAL(fenBefore.c_str(), board.toPositionString().c_str());
        STRCMP_EQUAL("36a-37", moveStr.c_str());
        CHECK_TRUE(stats.nodesVisited > 0);

        game->move(moveStr);
        CHECK_TRUE(game->isGameOver());
        CHECK_EQUAL(octi::PLAYER_A, game->getWinner());
    }
}

TEST(OctiBotEngineCoreTest, OnePlyThreatDefenseBlocksImmediateLoss)
{
    for (octi::BotDifficulty diff : {
             octi::BotDifficulty::CASUAL,
             octi::BotDifficulty::TACTICAL,
             octi::BotDifficulty::MASTER }) {
        octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
        octi::OctiGame *game = board.getGame();
        game->start(octi::PLAYER_A);
        board.clear();

        // Player B has a pod at 34 (0-based (2, 3)) with South prong 'e' threatening A's home base 33 (0-based (2, 2)).
        // Player A has a pod at 35 (0-based (2, 4)) with South prong 'e' that can jump over 34 to 33 capturing B's threatening pod ("35e-33x").
        octi::OctiPod podA(octi::A_POD);
        podA.addProng(octi::DIR_S, false);
        board.squareAt(2, 4).addPod(podA);

        octi::OctiPod podB(octi::B_POD);
        podB.addProng(octi::DIR_S, false);
        board.squareAt(2, 3).addPod(podB);

        octi::OctiPod podB2(octi::B_POD);
        board.squareAt(6, 6).addPod(podB2);

        octi::OctiBotEngine bot(diff, 0xCAFEBABEULL);
        bot.setTimeBudgetMs(25);
        std::string moveStr = bot.chooseMove(board);
        STRCMP_EQUAL("35e-33x", moveStr.c_str());
    }
}

TEST(OctiBotEngineCoreTest, LegalMoveSelectionAcrossAllModesAndVariants)
{
    struct VariantCase {
        octi::OctiGameMode mode;
        bool superProngs;
        bool edgeless;
        octi::OctiGameBases bases;
    };
    const VariantCase cases[] = {
        { octi::GAME_MODE_2,   false, false, octi::GAME_BASES_3 },
        { octi::GAME_MODE_2,   true,  true,  octi::GAME_BASES_1 },
        { octi::GAME_MODE_4,   true,  false, octi::GAME_BASES_3 },
        { octi::GAME_MODE_OFK, false, true,  octi::GAME_BASES_1 },
    };

    for (const auto &vc : cases) {
        for (octi::BotDifficulty diff : {
                 octi::BotDifficulty::CASUAL,
                 octi::BotDifficulty::TACTICAL,
                 octi::BotDifficulty::MASTER }) {
            octi::OctiBoard board(vc.mode, vc.superProngs, vc.edgeless, vc.bases);
            octi::OctiGame *game = board.getGame();
            game->start(octi::PLAYER_A);

            octi::OctiBotEngine bot(diff, 0x99887766ULL);
            bot.setTimeBudgetMs(60);
            LONGS_EQUAL(static_cast<int>(diff), static_cast<int>(bot.difficulty()));
            CHECK_EQUAL(60, bot.timeBudgetMs());

            for (int ply = 0; ply < 4 && !game->isGameOver(); ++ply) {
                std::string beforeFen = board.toPositionString();
                std::vector<octi::OctiMove> legal = game->generateLegalMoves();
                octi::BotSearchStats stats;
                std::string chosen = bot.chooseMove(board, &stats);
                STRCMP_EQUAL(beforeFen.c_str(), board.toPositionString().c_str());
                CHECK_FALSE(chosen.empty());

                octi::OctiMove parsed(chosen);
                bool foundInLegal = false;
                for (const auto &m : legal) {
                    if (m.asString() == parsed.asString()) {
                        foundInLegal = true;
                        break;
                    }
                }
                CHECK_TRUE(foundInLegal);
                game->move(parsed);
            }
        }
    }
}

TEST(OctiBotEngineCoreTest, BoundaryFinishedOrUnstartedGameReturnsEmpty)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);

    // Not started yet
    STRCMP_EQUAL("", bot.chooseMove(board).c_str());

    // Resigned / finished game
    board.getGame()->start(octi::PLAYER_A);
    board.getGame()->resign(octi::PLAYER_A);
    STRCMP_EQUAL("", bot.chooseMove(board).c_str());
}

TEST(OctiBotEngineCoreTest, RapidCheckBotAlwaysSelectsLegalMoveWithoutMutatingBoard)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    bool passed = rc::check("OctiBotEngine always returns a legal move without mutating board", []() {
        int modePick = *rc::gen::inRange(0, 3);
        octi::OctiGameMode mode = (modePick == 0) ? octi::GAME_MODE_2 :
                                  (modePick == 1) ? octi::GAME_MODE_4 : octi::GAME_MODE_OFK;
        bool useSuper = (mode != octi::GAME_MODE_OFK) && *rc::gen::arbitrary<bool>();
        bool useEdgeless = *rc::gen::arbitrary<bool>();
        octi::OctiBoard board(mode, useSuper, useEdgeless);
        octi::OctiGame *game = board.getGame();
        game->start(octi::PLAYER_A);

        int preSteps = *rc::gen::inRange(0, 6);
        for (int i = 0; i < preSteps && !game->isGameOver(); ++i) {
            auto legal = game->generateLegalMoves();
            if (legal.empty()) {
                break;
            }
            int idx = *rc::gen::inRange(0, static_cast<int>(legal.size()));
            game->move(legal[static_cast<std::size_t>(idx)]);
        }

        int diffPick = *rc::gen::inRange(1, 4);
        octi::OctiBotEngine bot(static_cast<octi::BotDifficulty>(diffPick), 0xABCDEFULL);
        bot.setTimeBudgetMs(25);

        std::string fenBefore = board.toPositionString();
        std::string chosen = bot.chooseMove(board);
        RC_ASSERT(board.toPositionString() == fenBefore);

        if (game->isGameOver()) {
            RC_ASSERT(chosen.empty());
        } else {
            auto legal = game->generateLegalMoves();
            if (legal.empty()) {
                RC_ASSERT(chosen.empty());
            } else {
                RC_ASSERT(!chosen.empty());
                octi::OctiMove parsed(chosen);
                bool isLegal = false;
                for (const auto &m : legal) {
                    if (m.asString() == parsed.asString()) {
                        isLegal = true;
                        break;
                    }
                }
                RC_ASSERT(isLegal);
                game->move(parsed);
            }
        }
    });
    CHECK_TRUE(passed);
}

TEST(OctiBotEngineCoreTest, Level3MasterOpeningNeverDumpsUnarmedReservePodsOn37)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    octi::OctiBotEngine bot(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    bot.setTimeBudgetMs(350);

    const std::vector<std::string> aMoves = {"53+h", "33+b", "33b-44", "53h-35"};
    for (const auto &aMove : aMoves) {
        game->move(aMove);
        std::string bMove = bot.chooseMove(board);
        CHECK_FALSE(bMove.empty());
        CHECK_FALSE(bMove == "37" || bMove == "57" || bMove == "77");
        game->move(bMove);
    }

    LONGS_EQUAL(4, game->getFreePods(octi::PLAYER_B));
}

TEST(OctiBotEngineCoreTest, CanonicalOpeningHashCommutationAndMirrorSymmetry)
{
    // 1. Commutation invariance: A plays 33+c, B plays 37+g, A plays 53+b
    // vs A plays 53+b, B plays 37+g, A plays 33+c
    octi::OctiBoard bComm1(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    bComm1.getGame()->start(octi::PLAYER_A);
    bComm1.getGame()->move("33+c");
    bComm1.getGame()->move("37+g");
    bComm1.getGame()->move("53+b");

    octi::OctiBoard bComm2(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    bComm2.getGame()->start(octi::PLAYER_A);
    bComm2.getGame()->move("53+b");
    bComm2.getGame()->move("37+g");
    bComm2.getGame()->move("33+c");

    CHECK_EQUAL(octi::OctiBotEngine::computePositionHashStatic(bComm1),
                octi::OctiBotEngine::computePositionHashStatic(bComm2));
    CHECK_EQUAL(octi::OctiBotEngine::computeCanonicalOpeningHash(bComm1),
                octi::OctiBotEngine::computeCanonicalOpeningHash(bComm2));

    // 2. Horizontal mirror symmetry on 9x9 2P board: 33+b vs 73+h
    octi::OctiBoard bMirLeft(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    bMirLeft.getGame()->start(octi::PLAYER_A);
    bMirLeft.getGame()->move("33+b");

    octi::OctiBoard bMirRight(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    bMirRight.getGame()->start(octi::PLAYER_A);
    bMirRight.getGame()->move("73+h");

    bool mirLeftFlag = false;
    bool mirRightFlag = false;
    std::uint64_t canonLeft =
        octi::OctiBotEngine::computeCanonicalOpeningHash(bMirLeft, &mirLeftFlag);
    std::uint64_t canonRight =
        octi::OctiBotEngine::computeCanonicalOpeningHash(bMirRight, &mirRightFlag);
    CHECK_EQUAL(canonLeft, canonRight);
    CHECK_TRUE(mirLeftFlag != mirRightFlag);

    // 3. Horizontal mirror symmetry on OFK board: 22+b vs 52+h
    octi::OctiBoard ofkLeft(octi::GAME_MODE_OFK);
    ofkLeft.getGame()->start(octi::PLAYER_A);
    ofkLeft.getGame()->move("22+b");

    octi::OctiBoard ofkRight(octi::GAME_MODE_OFK);
    ofkRight.getGame()->start(octi::PLAYER_A);
    ofkRight.getGame()->move("52+h");

    bool ofkLeftMir = false;
    bool ofkRightMir = false;
    std::uint64_t ofkCanonLeft =
        octi::OctiBotEngine::computeCanonicalOpeningHash(ofkLeft, &ofkLeftMir);
    std::uint64_t ofkCanonRight =
        octi::OctiBotEngine::computeCanonicalOpeningHash(ofkRight, &ofkRightMir);
    CHECK_EQUAL(ofkCanonLeft, ofkCanonRight);
    CHECK_TRUE(ofkLeftMir != ofkRightMir);

    // 4. Ruleset disambiguation: 1-base vs 3-base and edgeless vs non-edgeless
    octi::OctiBoard b3Base(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    b3Base.getGame()->start(octi::PLAYER_A);

    octi::OctiBoard b1Base(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
    b1Base.getGame()->start(octi::PLAYER_A);

    octi::OctiBoard bEdgeless(octi::GAME_MODE_2, false, true, octi::GAME_BASES_3);
    bEdgeless.getGame()->start(octi::PLAYER_A);

    octi::OctiBoard bSuper(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    bSuper.getGame()->start(octi::PLAYER_A);

    CHECK_TRUE(octi::OctiBotEngine::computePositionHashStatic(b3Base) !=
               octi::OctiBotEngine::computePositionHashStatic(b1Base));
    CHECK_TRUE(octi::OctiBotEngine::computePositionHashStatic(b3Base) !=
               octi::OctiBotEngine::computePositionHashStatic(bEdgeless));
    CHECK_TRUE(octi::OctiBotEngine::computePositionHashStatic(b3Base) !=
               octi::OctiBotEngine::computePositionHashStatic(bSuper));
    CHECK_TRUE(octi::OctiBotEngine::computeCanonicalOpeningHash(b3Base) !=
               octi::OctiBotEngine::computeCanonicalOpeningHash(b1Base));
}

TEST(OctiBotEngineCoreTest, MirrorMoveNotationRoundTripAndVariants)
{
    STRCMP_EQUAL("73+h", octi::OctiBotEngine::mirrorMoveNotation("33+b", 9).c_str());
    STRCMP_EQUAL("33+b", octi::OctiBotEngine::mirrorMoveNotation("73+h", 9).c_str());
    STRCMP_EQUAL("53+a", octi::OctiBotEngine::mirrorMoveNotation("53+a", 9).c_str());
    STRCMP_EQUAL("73gh-55-35x",
                 octi::OctiBotEngine::mirrorMoveNotation("33bc-55-75x", 9).c_str());
    STRCMP_EQUAL("33bc-55-75x",
                 octi::OctiBotEngine::mirrorMoveNotation("73gh-55-35x", 9).c_str());
    STRCMP_EQUAL("53g-43,53h-44",
                 octi::OctiBotEngine::mirrorMoveNotation("53c-63,53b-64", 9).c_str());
    STRCMP_EQUAL("62+h", octi::OctiBotEngine::mirrorMoveNotation("22+b", 7).c_str());
    STRCMP_EQUAL("22+b", octi::OctiBotEngine::mirrorMoveNotation("62+h", 7).c_str());
    STRCMP_EQUAL("52+h", octi::OctiBotEngine::mirrorMoveNotation("22+b", 6).c_str());
    STRCMP_EQUAL("22+b", octi::OctiBotEngine::mirrorMoveNotation("52+h", 6).c_str());

    // Invalid syntax or out-of-bounds width returns empty string
    STRCMP_EQUAL("", octi::OctiBotEngine::mirrorMoveNotation("", 9).c_str());
    STRCMP_EQUAL("", octi::OctiBotEngine::mirrorMoveNotation("invalid", 9).c_str());
    STRCMP_EQUAL("", octi::OctiBotEngine::mirrorMoveNotation("33+b", 0).c_str());
    STRCMP_EQUAL("", octi::OctiBotEngine::mirrorMoveNotation("99+a", 6).c_str());
}

TEST(OctiBotEngineCoreTest, OpeningBookLookupNominalBoundaryAndFaultInjection)
{
    // 1. Nominal hit on initial 2P (3-base & 1-base) and OFK boards
    octi::OctiBoard b2p3(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    b2p3.getGame()->start(octi::PLAYER_A);

    octi::OctiBoard b2p1(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
    b2p1.getGame()->start(octi::PLAYER_A);

    octi::OctiBoard bOfk(octi::GAME_MODE_OFK);
    bOfk.getGame()->start(octi::PLAYER_A);

    octi::OctiBotEngine casualBot(octi::BotDifficulty::CASUAL);
    CHECK_FALSE(casualBot.tuningProfile().useOpeningBook);
    octi::OctiBotEngine tacticalBot(octi::BotDifficulty::TACTICAL);
    CHECK_TRUE(tacticalBot.tuningProfile().useOpeningBook);

    octi::OctiBotEngine bot(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    CHECK_TRUE(bot.tuningProfile().useOpeningBook);
    std::string move2p3;
    int score2p3 = 0;
    int depth2p3 = 0;
    CHECK_TRUE(bot.probeOpeningBook(b2p3, move2p3, &score2p3, &depth2p3));
    CHECK_FALSE(move2p3.empty());
    CHECK_TRUE(depth2p3 >= 6);

    std::string move2p1;
    CHECK_TRUE(bot.probeOpeningBook(b2p1, move2p1));
    CHECK_FALSE(move2p1.empty());

    std::string moveOfk;
    CHECK_TRUE(bot.probeOpeningBook(bOfk, moveOfk));
    CHECK_FALSE(moveOfk.empty());

    // 2. Boundary: useOpeningBook = false disables book lookup
    octi::BotTuningProfile disabledProf = bot.tuningProfile();
    disabledProf.useOpeningBook = false;
    bot.setTuningProfile(disabledProf);
    std::string disabledMove;
    CHECK_FALSE(bot.probeOpeningBook(b2p3, disabledMove));
    STRCMP_EQUAL("", disabledMove.c_str());

    // 3. Fault injection: corrupted/illegal candidate in book table is rejected,
    // selecting the valid second candidate; all-illegal entry returns false
    octi::BotTuningProfile enabledProf = bot.tuningProfile();
    enabledProf.useOpeningBook = true;
    enabledProf.rootScoreJitter = 0;
    enabledProf.bookSelectionWindow = 0;

    std::uint64_t key2p3 = octi::OctiBotEngine::computeCanonicalOpeningHash(b2p3);
    const octi::OpeningBookEntry mixedTable[] = {
        {
            key2p3,
            8,
            3,
            {
                { "99+a", 500 },
                { "invalid_syntax", 400 },
                { "53+a", 120 }
            }
        }
    };
    std::uint64_t rng = 0x12345678ULL;
    std::string fallbackMove;
    int fallbackScore = 0;
    int fallbackDepth = 0;
    CHECK_TRUE(octi::OctiBotEngine::probeOpeningBookTable(
        b2p3, mixedTable, 1, enabledProf, rng,
        fallbackMove, &fallbackScore, &fallbackDepth));
    STRCMP_EQUAL("53+a", fallbackMove.c_str());
    LONGS_EQUAL(120, fallbackScore);
    LONGS_EQUAL(8, fallbackDepth);

    const octi::OpeningBookEntry allIllegalTable[] = {
        {
            key2p3,
            8,
            2,
            {
                { "99+a", 500 },
                { "44a-45", 300 },
                { nullptr, 0 }
            }
        }
    };
    std::string noMove;
    CHECK_FALSE(octi::OctiBotEngine::probeOpeningBookTable(
        b2p3, allIllegalTable, 1, enabledProf, rng, noMove));
    STRCMP_EQUAL("", noMove.c_str());

    // 4. Verify full 3,714-entry generated playbook and ply-4 2P 3-Base entry (33+h 57+d 53+h 37+d -> 73+h mirrored / 33+b canonical)
    LONGS_EQUAL(3714, static_cast<long>(octi::g_openingBookEntryCount));
    octi::OctiBoard bPly4(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    bPly4.getGame()->start(octi::PLAYER_A);
    bPly4.getGame()->move("33+h");
    bPly4.getGame()->move("57+d");
    bPly4.getGame()->move("53+h");
    bPly4.getGame()->move("37+d");
    bot.setTuningProfile(enabledProf);
    std::string ply4Move;
    int ply4Score = 0;
    int ply4Depth = 0;
    CHECK_TRUE(bot.probeOpeningBook(bPly4, ply4Move, &ply4Score, &ply4Depth));
    STRCMP_EQUAL("73+h", ply4Move.c_str());
    LONGS_EQUAL(-438, ply4Score);
    LONGS_EQUAL(8, ply4Depth);

    octi::OctiBoard bPly4Canon(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    bPly4Canon.getGame()->start(octi::PLAYER_A);
    bPly4Canon.getGame()->move("73+b");
    bPly4Canon.getGame()->move("57+f");
    bPly4Canon.getGame()->move("53+b");
    bPly4Canon.getGame()->move("77+f");
    std::string ply4CanonMove;
    CHECK_TRUE(bot.probeOpeningBook(bPly4Canon, ply4CanonMove, &ply4Score, &ply4Depth));
    STRCMP_EQUAL("33+b", ply4CanonMove.c_str());
}

TEST(OctiBotEngineCoreTest, SearchTopMovesForBookAndCanonicalOrientation)
{
    // 1. Nominal: top-3 moves on initial 2P board are legal and sorted descending by score
    octi::OctiBoard b2p(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    b2p.getGame()->start(octi::PLAYER_A);

    octi::OctiBotEngine bot(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    octi::BotSearchStats stats;
    std::vector<octi::BookMoveScore> top3 =
        bot.searchTopMovesForBook(b2p, 2, 2000, 3, &stats);
    LONGS_EQUAL(3, top3.size());
    CHECK_EQUAL(2, stats.depthReached);
    CHECK_TRUE(stats.nodesVisited > 0);
    CHECK_TRUE(top3[0].score >= top3[1].score);
    CHECK_TRUE(top3[1].score >= top3[2].score);
    CHECK_FALSE(top3[0].move == top3[1].move);
    CHECK_FALSE(top3[1].move == top3[2].move);

    // 2. Canonical orientation round-trip across mirrored boards (33+b vs 73+h)
    octi::OctiBoard bLeft(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    bLeft.getGame()->start(octi::PLAYER_A);
    bLeft.getGame()->move("33+b");

    octi::OctiBoard bRight(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    bRight.getGame()->start(octi::PLAYER_A);
    bRight.getGame()->move("73+h");

    bool mirLeft = false;
    bool mirRight = false;
    std::uint64_t keyLeft = octi::OctiBotEngine::computeCanonicalOpeningHash(bLeft, &mirLeft);
    std::uint64_t keyRight = octi::OctiBotEngine::computeCanonicalOpeningHash(bRight, &mirRight);
    CHECK_EQUAL(keyLeft, keyRight);
    CHECK_TRUE(mirLeft != mirRight);

    const octi::OctiBoard &mirroredBoard = mirLeft ? bLeft : bRight;
    const octi::OctiBoard &directBoard = mirLeft ? bRight : bLeft;

    std::vector<octi::BookMoveScore> mirTop =
        bot.searchTopMovesForBook(mirroredBoard, 2, 2000, 2);
    LONGS_EQUAL(2, mirTop.size());

    std::string canonMove0 =
        octi::OctiBotEngine::mirrorMoveNotation(mirTop[0].move, mirroredBoard.getBoardWidth());
    std::string canonMove1 =
        octi::OctiBotEngine::mirrorMoveNotation(mirTop[1].move, mirroredBoard.getBoardWidth());

    const octi::OpeningBookEntry customEntry[] = {
        {
            keyLeft,
            2,
            2,
            {
                { canonMove0.c_str(), static_cast<std::int16_t>(mirTop[0].score) },
                { canonMove1.c_str(), static_cast<std::int16_t>(mirTop[1].score) },
                { nullptr, 0 }
            }
        }
    };

    octi::BotTuningProfile deterministicProf = bot.tuningProfile();
    deterministicProf.useOpeningBook = true;
    deterministicProf.rootScoreJitter = 0;
    deterministicProf.bookSelectionWindow = 0;
    std::uint64_t rng = 0x11223344ULL;

    std::string probedMir;
    std::string probedDir;
    CHECK_TRUE(octi::OctiBotEngine::probeOpeningBookTable(
        mirroredBoard, customEntry, 1, deterministicProf, rng, probedMir));
    CHECK_TRUE(octi::OctiBotEngine::probeOpeningBookTable(
        directBoard, customEntry, 1, deterministicProf, rng, probedDir));
    STRCMP_EQUAL(mirTop[0].move.c_str(), probedMir.c_str());
    STRCMP_EQUAL(canonMove0.c_str(), probedDir.c_str());

    // 3. Boundary & Fault: topK = 0 or unstarted game returns empty vector
    CHECK_EQUAL(0, bot.searchTopMovesForBook(b2p, 2, 500, 0).size());
    octi::OctiBoard unstarted(octi::GAME_MODE_2);
    CHECK_EQUAL(0, bot.searchTopMovesForBook(unstarted, 2, 500, 3).size());
}

TEST(OctiBotEngineCoreTest, UnboundedTimeBookSearchAndBookGenMultiLangSync)
{
    octi::OctiBoard bOfk(octi::GAME_MODE_OFK, false, false, octi::GAME_BASES_1);
    bOfk.getGame()->start(octi::PLAYER_A);

    octi::OctiBotEngine bot(octi::BotDifficulty::MASTER, 0x5EED2026ULL);

    // 1. Nominal: timeBudgetMs = 0 (unbounded time) searches full depth 2
    octi::BotSearchStats statsZero;
    std::vector<octi::BookMoveScore> topZero =
        bot.searchTopMovesForBook(bOfk, 2, 0, 3, &statsZero);
    LONGS_EQUAL(3, topZero.size());
    CHECK_EQUAL(2, statsZero.depthReached);
    CHECK_TRUE(statsZero.nodesVisited > 0);

    // 2. Boundary: negative timeBudgetMs <= 0 also runs unbounded to full depth 2
    octi::BotSearchStats statsNeg;
    std::vector<octi::BookMoveScore> topNeg =
        bot.searchTopMovesForBook(bOfk, 2, -100, 2, &statsNeg);
    LONGS_EQUAL(2, topNeg.size());
    CHECK_EQUAL(2, statsNeg.depthReached);

    // 3. End-to-end octi_book_gen CLI: MAKEFLAGS -j4, Java class, --from-json, --out-js
    std::string bookGenBin;
    std::error_code selfEc;
    std::filesystem::path selfDir =
        std::filesystem::canonical("/proc/self/exe", selfEc).parent_path();
    if (!selfEc && std::filesystem::exists(selfDir / "octi_book_gen")) {
        bookGenBin = (selfDir / "octi_book_gen").string();
    } else if (std::filesystem::exists("build/octi_book_gen")) {
        bookGenBin = "./build/octi_book_gen";
    } else if (std::filesystem::exists("build/asan/octi_book_gen")) {
        bookGenBin = "./build/asan/octi_book_gen";
    } else if (std::filesystem::exists("./octi_book_gen")) {
        bookGenBin = "./octi_book_gen";
    }
    CHECK_FALSE(bookGenBin.empty());

    std::filesystem::path tmpDir =
        std::filesystem::temp_directory_path() / "octi_book_gen_unit_test";
    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
    std::filesystem::create_directories(tmpDir, ec);

    std::string jsonPath = (tmpDir / "book.json").string();
    std::string hxxPath = (tmpDir / "OctiOpeningBookData.hxx").string();
    std::string javaPath = (tmpDir / "OctiOpeningBookData.java").string();
    std::string logPath = (tmpDir / "gen.log").string();

    std::string cmdGen =
        "MAKEFLAGS=\"-j4\" " + bookGenBin +
        " --mode OFK --max-ply 1 --search-depth 2"
        " --out-json \"" + jsonPath + "\""
        " --out-header \"" + hxxPath + "\""
        " --out-java \"" + javaPath + "\" > \"" + logPath + "\" 2>&1";
    int rcGen = std::system(cmdGen.c_str());
    CHECK_EQUAL(0, rcGen);

    auto readFileStr = [](const std::string &p) -> std::string {
        std::ifstream in(p);
        std::ostringstream oss;
        oss << in.rdbuf();
        return oss.str();
    };

    std::string logContent = readFileStr(logPath);
    CHECK_TRUE(logContent.find("CPU Threads    : 4") != std::string::npos);
    CHECK_TRUE(logContent.find("unlimited (depth-bound)") != std::string::npos);

    std::string hxxContent = readFileStr(hxxPath);
    CHECK_TRUE(hxxContent.find("0xBF20205531084330ULL") != std::string::npos);

    std::string javaContent = readFileStr(javaPath);
    CHECK_TRUE(javaContent.find("public final class OctiOpeningBookData") != std::string::npos);
    CHECK_TRUE(javaContent.find("0xBF20205531084330L") != std::string::npos);

    // Test --from-json + --out-js marker splicing
    std::string mockJsPath = (tmpDir / "mock_webui.cxx").string();
    {
        std::ofstream out(mockJsPath);
        out << "// Header before book\n"
            << "// BEGIN_OPENING_BOOK_DATA\n"
            << "var OPENING_BOOK = {};\n"
            << "// END_OPENING_BOOK_DATA\n"
            << "// Footer after book\n";
    }
    std::string optionalWwwPath = (tmpDir / "www" / "missing_index.html").string();
    std::string cmdSync =
        bookGenBin + " --from-json \"" + jsonPath + "\""
        " --out-js \"" + mockJsPath + "\""
        " --out-js \"" + optionalWwwPath + "\" > /dev/null 2>&1";
    int rcSync = std::system(cmdSync.c_str());
    CHECK_EQUAL(0, rcSync);

    std::string syncedJs = readFileStr(mockJsPath);
    CHECK_TRUE(syncedJs.find("// Header before book") != std::string::npos);
    CHECK_TRUE(syncedJs.find("\"0xBF20205531084330\": {") != std::string::npos);
    CHECK_TRUE(syncedJs.find("// Footer after book") != std::string::npos);

    // 4. Resume from existing checkpoint JSON (skips Ply 0 and expands Ply 1)
    std::string resumeLogPath = (tmpDir / "resume.log").string();
    std::string cmdResume =
        bookGenBin +
        " --threads 1 --max-ply 2"
        " --bot-branch 2 --opp-branch 2"
        " --checkpoint-interval 1"
        " --out-json \"" + jsonPath + "\""
        " --out-header \"" + hxxPath + "\""
        " --out-java \"" + javaPath + "\" > \"" + resumeLogPath + "\" 2>&1";
    int rcResume = std::system(cmdResume.c_str());
    CHECK_EQUAL(0, rcResume);

    std::string resumeLog = readFileStr(resumeLogPath);
    CHECK_TRUE(resumeLog.find("resumed 1 entries") != std::string::npos);
    CHECK_TRUE(resumeLog.find("Mode           : OFK") != std::string::npos);
    CHECK_TRUE(resumeLog.find("Search Depth   : 2") != std::string::npos);
    CHECK_TRUE(resumeLog.find("[Ply 0/2] Resumed 1/1 cached position(s); 0 remaining to search.") != std::string::npos);
    CHECK_TRUE(resumeLog.find("[Ply 1/2] Evaluating") != std::string::npos);

    // 5. Boundary: --no-resume resets and starts fresh from ply 0
    std::string freshLogPath = (tmpDir / "fresh.log").string();
    std::string cmdFresh =
        bookGenBin +
        " --threads 1 --no-resume --mode OFK --max-ply 1 --search-depth 1"
        " --out-json \"" + jsonPath + "\""
        " --out-header \"" + hxxPath + "\""
        " --out-java \"" + javaPath + "\" > \"" + freshLogPath + "\" 2>&1";
    int rcFresh = std::system(cmdFresh.c_str());
    CHECK_EQUAL(0, rcFresh);
    std::string freshLog = readFileStr(freshLogPath);
    CHECK_TRUE(freshLog.find("Resume State   : fresh start") != std::string::npos);
    CHECK_TRUE(freshLog.find("Total Unique Canonical Entries : 1") != std::string::npos);

    // 6. Fault: --out-js on a file missing markers fails with non-zero exit code
    std::string badJsPath = (tmpDir / "no_markers.cxx").string();
    {
        std::ofstream out(badJsPath);
        out << "var OPENING_BOOK = {};\n";
    }
    std::string cmdFault =
        bookGenBin + " --from-json \"" + jsonPath + "\""
        " --out-js \"" + badJsPath + "\" > /dev/null 2>&1";
    int rcFault = std::system(cmdFault.c_str());
    CHECK_TRUE(rcFault != 0);

    std::filesystem::remove_all(tmpDir, ec);
}

TEST(OctiBotEngineCoreTest, BookMoveScorePreservesUnclamped32BitMateAndLeadScores)
{
    octi::OctiBotEngine bot(octi::BotDifficulty::GRANDMASTER, 0x5EED2026ULL);
    octi::BotTuningProfile prof = bot.tuningProfile();
    prof.lazySmpThreads = 1;
    prof.useOpeningBook = false;
    prof.rootScoreJitter = 0;
    bot.setTuningProfile(prof);

    // 1. Position with immediate 1-ply base-capture win (56aeh-57) AND non-mate large-lead moves (> 32000)
    octi::OctiBoard mateBoard(octi::GAME_MODE_2);
    mateBoard.fromPositionString(
        "2:3:0:0:P:A:-:0:3,1,23,0:0,5,10,0:B53defgh,B53e,A56aeh,A37abeg,A77abdh");
    octi::BotSearchStats mateStats;
    std::vector<octi::BookMoveScore> topMate =
        bot.searchTopMovesForBook(mateBoard, 2, 0, 5, &mateStats);
    CHECK_TRUE(topMate.size() >= 2);
    STRCMP_EQUAL("56aeh-57", topMate[0].move.c_str());

    std::int32_t mateScore = static_cast<std::int32_t>(topMate[0].score);
    CHECK_TRUE(mateScore >= 900000);

    // 2. Non-mate large-lead position (> 32000 but < 900000) must not collide with mateScore or clamp to 32000
    octi::OctiBoard leadBoard(octi::GAME_MODE_2);
    leadBoard.fromPositionString(
        "2:3:0:0:P:A:-:0:3,1,23,0:0,5,10,0:B53defgh,B53e,A33aeh,A37,A77");
    octi::BotSearchStats leadStats;
    std::vector<octi::BookMoveScore> topLead =
        bot.searchTopMovesForBook(leadBoard, 2, 0, 3, &leadStats);
    CHECK_TRUE(topLead.size() >= 1);

    std::int32_t leadScore = static_cast<std::int32_t>(topLead[0].score);
    CHECK_TRUE(leadScore > 32000);
    CHECK_TRUE(leadScore < 900000);
    CHECK_TRUE(mateScore > leadScore);

    // 3. octi_book_gen preserves int16 [-32000, 32000] clamping when writing OpeningBookCandidate::score
    std::string bookGenBin;
    std::error_code selfEc;
    std::filesystem::path selfDir =
        std::filesystem::canonical("/proc/self/exe", selfEc).parent_path();
    std::filesystem::path coreRoot =
        std::filesystem::path(__FILE__).parent_path().parent_path();
    if (!selfEc && std::filesystem::exists(selfDir / "octi_book_gen")) {
        bookGenBin = (selfDir / "octi_book_gen").string();
    } else if (std::filesystem::exists("build/octi_book_gen")) {
        bookGenBin = "./build/octi_book_gen";
    } else if (std::filesystem::exists("build/asan/octi_book_gen")) {
        bookGenBin = "./build/asan/octi_book_gen";
    } else if (std::filesystem::exists("./octi_book_gen")) {
        bookGenBin = "./octi_book_gen";
    } else if (std::filesystem::exists(coreRoot / "build" / "octi_book_gen")) {
        bookGenBin = (coreRoot / "build" / "octi_book_gen").string();
    } else if (std::filesystem::exists(coreRoot / "build" / "asan" / "octi_book_gen")) {
        bookGenBin = (coreRoot / "build" / "asan" / "octi_book_gen").string();
    }
    CHECK_FALSE(bookGenBin.empty());

    std::filesystem::path tmpDir =
        std::filesystem::temp_directory_path() / "octi_book_score32_clamp_test";
    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
    std::filesystem::create_directories(tmpDir, ec);

    std::string jsonPath = (tmpDir / "clamp_book.json").string();
    std::string hxxPath = (tmpDir / "OctiOpeningBookData.hxx").string();
    {
        std::ofstream out(jsonPath);
        out << "{\n"
            << "  \"version\": 1,\n"
            << "  \"entries\": [\n"
            << "    {\n"
            << "      \"canonicalKey\": \"0xBF20205531084330\",\n"
            << "      \"depth\": 2,\n"
            << "      \"ply\": 0,\n"
            << "      \"candidates\": [\n"
            << "        { \"move\": \"32+a\", \"score\": 1000000 },\n"
            << "        { \"move\": \"42+a\", \"score\": -999999 }\n"
            << "      ]\n"
            << "    }\n"
            << "  ]\n"
            << "}\n";
    }
    std::string cmdClamp =
        bookGenBin + " --from-json \"" + jsonPath + "\""
        " --out-header \"" + hxxPath + "\" > /dev/null 2>&1";
    int rcClamp = std::system(cmdClamp.c_str());
    CHECK_EQUAL(0, rcClamp);

    std::ifstream hxxIn(hxxPath);
    std::ostringstream hxxOss;
    hxxOss << hxxIn.rdbuf();
    std::string hxxStr = hxxOss.str();
    CHECK_TRUE(hxxStr.find("{ \"32+a\", 32000 }") != std::string::npos);
    CHECK_TRUE(hxxStr.find("{ \"42+a\", -32000 }") != std::string::npos);

    std::filesystem::remove_all(tmpDir, ec);
}

TEST(OctiBotEngineCoreTest, LazySmpAggregatesAllHelperThreadNodes)
{
    octi::OctiBoard b2p(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    b2p.getGame()->start(octi::PLAYER_A);

    // 1. searchTopMovesForBook: 4 threads aggregates main + helper thread nodes
    octi::OctiBotEngine bot1(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    octi::BotTuningProfile prof1 = bot1.tuningProfile();
    prof1.lazySmpThreads = 1;
    prof1.useOpeningBook = false;
    prof1.rootScoreJitter = 0;
    bot1.setTuningProfile(prof1);

    octi::BotSearchStats bookStats1;
    std::vector<octi::BookMoveScore> top1 =
        bot1.searchTopMovesForBook(b2p, 3, 0, 3, &bookStats1);
    CHECK_EQUAL(3, top1.size());
    CHECK_TRUE(bookStats1.nodesVisited > 0);
    CHECK_EQUAL(bookStats1.nodesVisited, bot1.liveNodesVisited());

    octi::OctiBotEngine bot4(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    octi::BotTuningProfile prof4 = bot4.tuningProfile();
    prof4.lazySmpThreads = 4;
    prof4.useOpeningBook = false;
    prof4.rootScoreJitter = 0;
    bot4.setTuningProfile(prof4);

    octi::BotSearchStats bookStats4;
    std::vector<octi::BookMoveScore> top4 =
        bot4.searchTopMovesForBook(b2p, 3, 0, 3, &bookStats4);
    CHECK_EQUAL(3, top4.size());
    CHECK_EQUAL(bookStats4.nodesVisited, bot4.liveNodesVisited());
    if (std::thread::hardware_concurrency() > 1) {
        CHECK_TRUE(bookStats4.nodesVisited > bookStats1.nodesVisited);
    }

    // 2. chooseMove: 4 threads aggregates main + helper thread nodes and matches liveNodesVisited()
    octi::OctiBotEngine chooseBot1(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    prof1.maxSearchDepth = 3;
    prof1.timeBudgetMs = 10000;
    chooseBot1.setTuningProfile(prof1);
    octi::BotSearchStats chooseStats1;
    std::string mv1 = chooseBot1.chooseMove(b2p, &chooseStats1);
    CHECK_FALSE(mv1.empty());
    CHECK_TRUE(chooseStats1.nodesVisited > 0);
    CHECK_EQUAL(chooseStats1.nodesVisited, chooseBot1.liveNodesVisited());

    octi::OctiBotEngine chooseBot4(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    prof4.maxSearchDepth = 3;
    prof4.timeBudgetMs = 10000;
    chooseBot4.setTuningProfile(prof4);
    octi::BotSearchStats chooseStats4;
    std::string mv4 = chooseBot4.chooseMove(b2p, &chooseStats4);
    CHECK_FALSE(mv4.empty());
    CHECK_EQUAL(chooseStats4.nodesVisited, chooseBot4.liveNodesVisited());
    if (std::thread::hardware_concurrency() > 1) {
        CHECK_TRUE(chooseStats4.nodesVisited > chooseStats1.nodesVisited);
    }
}

TEST(OctiBotEngineCoreTest, AllOpeningBookEntriesAndCandidatesExecuteLegally)
{
    CHECK_EQUAL(3714u, octi::g_openingBookEntryCount);

    std::filesystem::path coreRoot =
        std::filesystem::path(__FILE__).parent_path().parent_path();
    std::filesystem::path hxxPath =
        coreRoot / "include" / "OctiOpeningBookData.hxx";
    CHECK_TRUE(std::filesystem::exists(hxxPath));

    std::ifstream in(hxxPath);
    CHECK_TRUE(in.good());

    struct ParsedBookLine {
        std::string variantLabel;
        int ply = 0;
        std::vector<std::string> prefixMoves;
    };
    std::vector<ParsedBookLine> parsedLines;
    parsedLines.reserve(octi::g_openingBookEntryCount);

    std::string line;
    while (std::getline(in, line)) {
        std::size_t slashPos = line.find("// ");
        if (slashPos == std::string::npos) {
            continue;
        }
        std::string comment = line.substr(slashPos + 3);
        while (!comment.empty() &&
               (comment.back() == '\r' || comment.back() == ' ')) {
            comment.pop_back();
        }
        std::size_t plyPos = comment.find(" ply ");
        std::size_t parenOpen = comment.find(" (");
        if (plyPos == std::string::npos || parenOpen == std::string::npos ||
            parenOpen <= plyPos + 5 || comment.back() != ')') {
            continue;
        }
        ParsedBookLine pl;
        pl.variantLabel = comment.substr(0, plyPos);
        pl.ply = std::stoi(
            comment.substr(plyPos + 5, parenOpen - (plyPos + 5)));
        std::string inner = comment.substr(
            parenOpen + 2, comment.size() - (parenOpen + 3));
        if (inner != "initial") {
            CHECK_EQUAL(0u, inner.find("after "));
            std::istringstream iss(inner.substr(6));
            std::string mv;
            while (iss >> mv) {
                pl.prefixMoves.push_back(mv);
            }
        }
        CHECK_EQUAL(static_cast<std::size_t>(pl.ply), pl.prefixMoves.size());
        parsedLines.push_back(std::move(pl));
    }

    CHECK_EQUAL(octi::g_openingBookEntryCount, parsedLines.size());

    auto makeVariantBoard = [](const std::string &v) -> octi::OctiBoard {
        if (v == "2P 3-Base") {
            return octi::OctiBoard(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
        }
        if (v == "2P 1-Base") {
            return octi::OctiBoard(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
        }
        if (v == "2P_3B_SP") {
            return octi::OctiBoard(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        }
        if (v == "2P_3B_ED") {
            return octi::OctiBoard(octi::GAME_MODE_2, false, true, octi::GAME_BASES_3);
        }
        if (v == "2P_3B_SPED") {
            return octi::OctiBoard(octi::GAME_MODE_2, true, true, octi::GAME_BASES_3);
        }
        if (v == "2P_1B_SP") {
            return octi::OctiBoard(octi::GAME_MODE_2, true, false, octi::GAME_BASES_1);
        }
        if (v == "2P_1B_ED") {
            return octi::OctiBoard(octi::GAME_MODE_2, false, true, octi::GAME_BASES_1);
        }
        if (v == "2P_1B_SPED") {
            return octi::OctiBoard(octi::GAME_MODE_2, true, true, octi::GAME_BASES_1);
        }
        if (v == "4P") {
            return octi::OctiBoard(octi::GAME_MODE_4, false, false, octi::GAME_BASES_1);
        }
        if (v == "4P_SP") {
            return octi::OctiBoard(octi::GAME_MODE_4, true, false, octi::GAME_BASES_1);
        }
        if (v == "4P_ED") {
            return octi::OctiBoard(octi::GAME_MODE_4, false, true, octi::GAME_BASES_1);
        }
        if (v == "4P_SPED") {
            return octi::OctiBoard(octi::GAME_MODE_4, true, true, octi::GAME_BASES_1);
        }
        if (v == "OFK") {
            return octi::OctiBoard(octi::GAME_MODE_OFK, false, false, octi::GAME_BASES_1);
        }
        FAIL(("Unknown opening book variant label: " + v).c_str());
        return octi::OctiBoard(octi::GAME_MODE_2);
    };

    std::size_t prefixMovesExecuted = 0;
    std::size_t candidateMovesExecuted = 0;

    for (std::size_t i = 0; i < octi::g_openingBookEntryCount; ++i) {
        const octi::OpeningBookEntry &entry = octi::g_openingBookEntries[i];
        const ParsedBookLine &pl = parsedLines[i];

        octi::OctiBoard board = makeVariantBoard(pl.variantLabel);
        board.suppressBoardUpdateNotification(true);
        octi::OctiGame *game = board.getGame();
        CHECK_TRUE(game != nullptr);
        game->setSearchMode(true);
        game->start(octi::PLAYER_A);

        for (const std::string &pm : pl.prefixMoves) {
            game->move(pm);
            ++prefixMovesExecuted;
        }

        bool mirrored = false;
        std::uint64_t canonHash =
            octi::OctiBotEngine::computeCanonicalOpeningHash(board, &mirrored);
        CHECK_EQUAL(entry.canonicalKey, canonHash);
        CHECK_TRUE(entry.numCandidates >= 1 && entry.numCandidates <= 3);

        std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
        CHECK_FALSE(legalMoves.empty());
        std::unordered_set<std::string> legalStrs;
        legalStrs.reserve(legalMoves.size());
        for (const octi::OctiMove &lm : legalMoves) {
            legalStrs.insert(lm.asString());
        }

        const std::string posStr = board.toPositionString();
        const int w = board.getBoardWidth();

        for (std::uint8_t c = 0; c < entry.numCandidates; ++c) {
            const char *rawMove = entry.candidates[c].move;
            CHECK_TRUE(rawMove != nullptr && rawMove[0] != '\0');
            std::string actualMove = mirrored
                ? octi::OctiBotEngine::mirrorMoveNotation(rawMove, w)
                : std::string(rawMove);
            CHECK_FALSE(actualMove.empty());

            octi::OctiMove parsed(actualMove);
            std::string canonicalStr = parsed.asString();
            if (parsed.getOps().size() == 1 ||
                board.getGameMode() == octi::GAME_MODE_OFK) {
                CHECK_TRUE_TEXT(
                    legalStrs.count(canonicalStr) > 0,
                    ("Candidate not in generateLegalMoves at entry " +
                     std::to_string(i) + ": " + canonicalStr).c_str());
            }

            octi::OctiBoard testBoard;
            testBoard.suppressBoardUpdateNotification(true);
            testBoard.fromPositionString(posStr);
            CHECK_TRUE(testBoard.getGame() != nullptr);
            testBoard.getGame()->setSearchMode(true);
            testBoard.getGame()->move(parsed);
            ++candidateMovesExecuted;
        }
    }

    CHECK_EQUAL(13337u, prefixMovesExecuted);
    CHECK_EQUAL(11142u, candidateMovesExecuted);
}

TEST(OctiBotEngineCoreTest, AllOpeningBookJsonPositionsAndCandidatesExecuteLegally)
{
    std::filesystem::path repoRoot =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    std::filesystem::path jsonPath = repoRoot / ".book" / "opening_book.json";
    CHECK_TRUE(std::filesystem::exists(jsonPath));

    std::ifstream in(jsonPath);
    CHECK_TRUE(in.is_open());
    nlohmann::json root = nlohmann::json::parse(in);
    CHECK_TRUE(root.contains("entries") && root["entries"].is_array());

    const auto &entries = root["entries"];
    CHECK_EQUAL(octi::g_openingBookEntryCount, entries.size());

    std::size_t totalEntries = 0;
    std::size_t fourPlayerEntries = 0;
    std::size_t candidatesExecuted = 0;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto &e = entries[i];
        const std::string posStr = e.at("position").get<std::string>();
        const std::string variant = e.value("variant", std::string());
        const std::string keyHex = e.at("canonicalKey").get<std::string>();
        const std::uint64_t expectedKey = std::stoull(keyHex, nullptr, 16);

        octi::OctiBoard board;
        board.suppressBoardUpdateNotification(true);
        board.fromPositionString(posStr);
        octi::OctiGame *game = board.getGame();
        CHECK_TRUE(game != nullptr);
        game->setSearchMode(true);

        if (board.getGameMode() == octi::GAME_MODE_4 || variant.rfind("4P", 0) == 0) {
            ++fourPlayerEntries;
        }

        bool mirrored = false;
        std::uint64_t canonHash =
            octi::OctiBotEngine::computeCanonicalOpeningHash(board, &mirrored);
        CHECK_EQUAL(expectedKey, canonHash);

        std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
        CHECK_FALSE(legalMoves.empty());
        std::unordered_set<std::string> legalStrs;
        legalStrs.reserve(legalMoves.size());
        for (const octi::OctiMove &lm : legalMoves) {
            legalStrs.insert(lm.asString());
        }

        const int w = board.getBoardWidth();
        const auto &cands = e.at("candidates");
        CHECK_TRUE(cands.is_array() && !cands.empty());
        for (const auto &cand : cands) {
            const std::string rawMove = cand.at("move").get<std::string>();
            CHECK_FALSE(rawMove.empty());
            const std::string actualMove = mirrored
                ? octi::OctiBotEngine::mirrorMoveNotation(rawMove, w)
                : rawMove;
            octi::OctiMove parsed(actualMove);
            if (parsed.getOps().size() == 1 ||
                board.getGameMode() == octi::GAME_MODE_OFK) {
                CHECK_TRUE_TEXT(
                    legalStrs.count(parsed.asString()) > 0,
                    ("JSON book candidate not in generateLegalMoves at entry " +
                     std::to_string(i) + ": " + parsed.asString()).c_str());
            }

            octi::OctiBoard stepBoard;
            stepBoard.suppressBoardUpdateNotification(true);
            stepBoard.fromPositionString(posStr);
            stepBoard.getGame()->setSearchMode(true);
            stepBoard.getGame()->move(parsed);
            ++candidatesExecuted;
        }
        ++totalEntries;
    }

    CHECK_EQUAL(3714u, totalEntries);
    CHECK_TRUE(fourPlayerEntries > 0u);
    CHECK_EQUAL(11142u, candidatesExecuted);

    if (const char *printEnv = std::getenv("OCTI_PRINT_BOOK_AUDIT");
        printEnv != nullptr && std::strcmp(printEnv, "1") == 0) {
        std::printf(
            "OPENING_BOOK_JSON_AUDIT: entries=%zu four_player_entries=%zu candidates_executed=%zu failures=0\n",
            totalEntries, fourPlayerEntries, candidatesExecuted);
        std::fflush(stdout);
    }
}

TEST(OctiBotEngineCoreTest, DepthOneDeadlineFallbackAndPartialRootGuard)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.fromPositionString(
        "2:3:0:0:P:A:-:0:2,0,12,0:2,0,12,0:"
        "A32abcdefg,A42abcdefg,A52abcdefg,A33abcdefg,"
        "A53abcdefg,A34abcdefg,A44abcdefg,"
        "B58abcdefg,B68abcdefg,B78abcdefg,B57abcdefg,"
        "B77abcdefg,B66abcdefg,B76abcdefg");
    octi::OctiGame *game = board.getGame();
    CHECK_TRUE(game != nullptr);

    std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
    CHECK_TRUE(legalMoves.size() > 40);

    auto isLegalMoveStr = [&legalMoves](const std::string &mvStr) -> bool {
        if (mvStr.empty()) {
            return false;
        }
        for (const auto &m : legalMoves) {
            if (m.asString() == mvStr) {
                return true;
            }
        }
        return false;
    };

    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL, 0x5EED2026ULL);
    octi::BotTuningProfile prof = bot.tuningProfile();
    prof.useOpeningBook = false;
    prof.rootScoreJitter = 0;
    prof.lazySmpThreads = 1;
    prof.rootBranchCap = 2048;
    prof.treeBranchCap = 2048;
    prof.compositeMoveMaxPly = 3;
    bot.setTuningProfile(prof);
    bot.setTimeBudgetMs(1);

    octi::BotSearchStats stats;
    std::string chosen = bot.chooseMove(board, &stats);
    CHECK_FALSE(chosen.empty());
    CHECK_TRUE(isLegalMoveStr(chosen));
    CHECK_EQUAL(1, stats.depthReached);

    octi::BotSearchStats bookStats;
    std::vector<octi::BookMoveScore> topMoves =
        bot.searchTopMovesForBook(board, 4, 1, 3, &bookStats);
    LONGS_EQUAL(3, topMoves.size());
    CHECK_EQUAL(1, bookStats.depthReached);
    for (const auto &cand : topMoves) {
        CHECK_FALSE(cand.move.empty());
        CHECK_TRUE(isLegalMoveStr(cand.move));
    }
}

// ---------------------------------------------------------------------------
// OctiCryptoAndTranscriptV3Test
// ---------------------------------------------------------------------------

TEST_GROUP(OctiCryptoAndTranscriptV3Test)
{
};

TEST(OctiCryptoAndTranscriptV3Test, NistSha256StandardVectors)
{
    // NIST Vector 1: Empty string
    STRCMP_EQUAL("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                 octi::OctiCrypto::sha256Hex("").c_str());

    // NIST Vector 2: "abc"
    STRCMP_EQUAL("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                 octi::OctiCrypto::sha256Hex("abc").c_str());

    // NIST Vector 3: 56-byte multi-block boundary
    const std::string v3 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    STRCMP_EQUAL("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                 octi::OctiCrypto::sha256Hex(v3).c_str());

    // sha256Bytes matches sha256Hex
    std::array<std::uint8_t, 32> bytes = octi::OctiCrypto::sha256Bytes("abc", 3);
    CHECK_EQUAL(0xba, bytes[0]);
    CHECK_EQUAL(0x78, bytes[1]);
    CHECK_EQUAL(0x16, bytes[2]);
    CHECK_EQUAL(0xad, bytes[31]);
}

TEST(OctiCryptoAndTranscriptV3Test, OctiBotEngineLiveTelemetryAccessors)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    octi::OctiBotEngine bot(octi::BotDifficulty::CASUAL, 0x12345ULL);
    bot.setTimeBudgetMs(100);

    // Initial state before search
    CHECK_EQUAL(0, bot.liveDepthReached());
    CHECK_EQUAL(0, bot.liveNodesVisited());
    CHECK_EQUAL(0, bot.liveBestScore());

    octi::BotSearchStats stats;
    std::string move = bot.chooseMove(board, &stats);
    CHECK_FALSE(move.empty());

    // Final live state matches returned stats
    CHECK_TRUE(bot.liveDepthReached() >= 1);
    CHECK_TRUE(bot.liveNodesVisited() > 0);
    CHECK_EQUAL(stats.depthReached, bot.liveDepthReached());
    CHECK_EQUAL(stats.nodesVisited, bot.liveNodesVisited());
    CHECK_EQUAL(stats.bestScore, bot.liveBestScore());

    // Concurrent thread polling check
    std::atomic<bool> sawLiveNodes{false};
    std::atomic<bool> threadRunning{true};
    octi::OctiBotEngine pollBot(octi::BotDifficulty::TACTICAL, 0x9988ULL);
    octi::BotTuningProfile pollProf = pollBot.tuningProfile();
    pollProf.useOpeningBook = false;
    pollBot.setTuningProfile(pollProf);
    pollBot.setTimeBudgetMs(150);

    std::thread poller([&]() {
        while (threadRunning.load()) {
            if (pollBot.liveNodesVisited() > 0) {
                sawLiveNodes.store(true);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    pollBot.chooseMove(board);
    threadRunning.store(false);
    poller.join();

    CHECK_TRUE(sawLiveNodes.load());
}

TEST(OctiCryptoAndTranscriptV3Test, JsonV3MetadataAndPerPlyAiEffortRoundTrip)
{
    octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    octi::OctiGameMetadata meta;
    meta.gameId = "custom-test-game-12345";
    meta.source = "TEST_SUITE";
    meta.license = "CC-BY-4.0";
    meta.attribution = octi::OCTI_DATA_ATTRIBUTION;
    meta.appVersion = "2.1.1";
    meta.buildType = "debug";
    meta.platform = "linux-x86_64";
    meta.extended.present = true;
    meta.extended.startedAt = 1761724000LL;
    meta.extended.endedAt = 1761724320LL;
    meta.extended.utcOffsetMin = 480;
    meta.extended.locale = "en-TW";
    meta.extended.osVersion = "Ubuntu 24.04";
    meta.extended.installId = "f47ac10b-58cc-4372-a567-0e02b2c3d479";
    meta.timeControl = "600+5";

    octi::OctiPlayerMetadata p1;
    p1.seat = 1;
    p1.player = octi::PLAYER_A;
    p1.type = "HUMAN";
    p1.handle = "Charles";
    meta.players.push_back(p1);

    octi::OctiPlayerMetadata p2;
    p2.seat = 2;
    p2.player = octi::PLAYER_B;
    p2.type = "AI";
    p2.handle = "AI_L3";
    p2.aiDifficulty = 3;
    p2.aiBudgetMs = 8000;
    meta.players.push_back(p2);

    game->setGameMetadata(meta);

    // Human move
    game->move("33+b");
    octi::OctiPlyMetadata ply1;
    ply1.elapsedMs = 1200;
    ply1.clockAfterMs = 598800;
    game->setLastPlyMetadata(ply1);

    // AI move
    game->move("37+e");
    octi::OctiPlyMetadata ply2;
    ply2.elapsedMs = 3400;
    ply2.clockAfterMs = 596600;
    ply2.isAiMove = true;
    ply2.aiDifficulty = 3;
    ply2.aiBudgetMs = 8000;
    ply2.aiDepthReached = 5;
    ply2.aiNodesVisited = 84200;
    ply2.aiBestScore = 42;
    game->setLastPlyMetadata(ply2);

    std::string jsonStr = board.serializeJson(meta);
    CHECK_TRUE(jsonStr.find("\"format\": \"octi.transcript.v3\"") != std::string::npos);
    CHECK_TRUE(jsonStr.find("\"gameId\": \"custom-test-game-12345\"") != std::string::npos);
    CHECK_TRUE(jsonStr.find("\"license\": \"CC-BY-4.0\"") != std::string::npos);
    CHECK_TRUE(jsonStr.find("\"attribution\": \"octi.selfso.com\"") != std::string::npos);
    CHECK_TRUE(jsonStr.find("\"installId\": \"f47ac10b-58cc-4372-a567-0e02b2c3d479\"") != std::string::npos);
    CHECK_TRUE(jsonStr.find("\"nodesVisited\":84200") != std::string::npos ||
               jsonStr.find("\"nodesVisited\": 84200") != std::string::npos);

    octi::OctiBoard replayBoard;
    replayBoard.deserializeGame(jsonStr);

    CHECK_EQUAL(2, replayBoard.getGame()->getNumMoves());
    CHECK_EQUAL(board.toPositionString(), replayBoard.toPositionString());
    CHECK_EQUAL(octi::GAME_MODE_2, replayBoard.getGameMode());
    CHECK_TRUE(replayBoard.getGame()->useSuperProngs());

    const octi::OctiGameMetadata &repMeta = replayBoard.getGame()->getGameMetadata();
    STRCMP_EQUAL("custom-test-game-12345", repMeta.gameId.c_str());
    STRCMP_EQUAL("CC-BY-4.0", repMeta.license.c_str());
    STRCMP_EQUAL("octi.selfso.com", repMeta.attribution.c_str());
    CHECK_TRUE(repMeta.extended.present);
    STRCMP_EQUAL("f47ac10b-58cc-4372-a567-0e02b2c3d479", repMeta.extended.installId.c_str());
    CHECK_EQUAL(2, repMeta.players.size());
    STRCMP_EQUAL("Charles", repMeta.players[0].handle.c_str());
    STRCMP_EQUAL("AI_L3", repMeta.players[1].handle.c_str());
    CHECK_EQUAL(3, repMeta.players[1].aiDifficulty);

    octi::OctiPlyMetadata repPly1 = replayBoard.getGame()->getPlyMetadata(0);
    CHECK_EQUAL(1200, repPly1.elapsedMs);
    CHECK_EQUAL(598800, repPly1.clockAfterMs);
    CHECK_FALSE(repPly1.isAiMove);

    octi::OctiPlyMetadata repPly2 = replayBoard.getGame()->getPlyMetadata(1);
    CHECK_EQUAL(3400, repPly2.elapsedMs);
    CHECK_EQUAL(596600, repPly2.clockAfterMs);
    CHECK_TRUE(repPly2.isAiMove);
    CHECK_EQUAL(3, repPly2.aiDifficulty);
    CHECK_EQUAL(8000, repPly2.aiBudgetMs);
    CHECK_EQUAL(5, repPly2.aiDepthReached);
    CHECK_EQUAL(84200ULL, repPly2.aiNodesVisited);
    CHECK_EQUAL(42, repPly2.aiBestScore);
}

TEST(OctiCryptoAndTranscriptV3Test, JsonV3ResignedStatusRoundTrip)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    game->move("33+b");
    game->move("37+e");
    game->resign(octi::PLAYER_B);

    CHECK_TRUE(game->isGameOver());
    CHECK_TRUE(game->isGameResigned());
    CHECK_EQUAL(octi::PLAYER_A, game->getWinner());

    std::string jsonStr = board.serializeGame();
    CHECK_TRUE(jsonStr.find("\"status\": \"RESIGNED\"") != std::string::npos);
    CHECK_TRUE(jsonStr.find("\"winner\": \"A\"") != std::string::npos);
    CHECK_TRUE(jsonStr.find("\"termination\": \"RESIGNATION\"") != std::string::npos);

    octi::OctiBoard replayBoard;
    replayBoard.deserializeGame(jsonStr);

    CHECK_TRUE(replayBoard.getGame()->isGameOver());
    CHECK_TRUE(replayBoard.getGame()->isGameResigned());
    CHECK_EQUAL(octi::PLAYER_A, replayBoard.getGame()->getWinner());
    CHECK_EQUAL(2, replayBoard.getGame()->getNumMoves());
    CHECK_EQUAL(board.toPositionString(), replayBoard.toPositionString());
}

TEST(OctiCryptoAndTranscriptV3Test, UndoRedoPreservesPlyMetadata)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    game->move("33+b");
    octi::OctiPlyMetadata p1;
    p1.elapsedMs = 1111;
    game->setLastPlyMetadata(p1);

    game->move("37+e");
    octi::OctiPlyMetadata p2;
    p2.elapsedMs = 2222;
    p2.isAiMove = true;
    p2.aiNodesVisited = 999;
    game->setLastPlyMetadata(p2);

    CHECK_EQUAL(2, game->getAllPlyMetadata().size());
    CHECK_EQUAL(1111, game->getPlyMetadata(0).elapsedMs);
    CHECK_EQUAL(2222, game->getPlyMetadata(1).elapsedMs);

    game->undo();
    CHECK_EQUAL(1, game->getNumMoves());
    CHECK_EQUAL(1, game->getAllPlyMetadata().size());
    CHECK_EQUAL(1111, game->getPlyMetadata(0).elapsedMs);

    game->redo();
    CHECK_EQUAL(2, game->getNumMoves());
    CHECK_EQUAL(2, game->getAllPlyMetadata().size());
    CHECK_EQUAL(1111, game->getPlyMetadata(0).elapsedMs);
    CHECK_EQUAL(2222, game->getPlyMetadata(1).elapsedMs);
    CHECK_TRUE(game->getPlyMetadata(1).isAiMove);
    CHECK_EQUAL(999ULL, game->getPlyMetadata(1).aiNodesVisited);
}

TEST(OctiCryptoAndTranscriptV3Test, CanonicalGameIdIsDeterministicAndDifferentiatesVariants)
{
    octi::OctiBoard b1(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    b1.getGame()->start(octi::PLAYER_A);
    b1.getGame()->move("33+b");
    b1.getGame()->move("37+e");

    octi::OctiBoard b2(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    b2.getGame()->start(octi::PLAYER_A);
    b2.getGame()->move("33+b");
    b2.getGame()->move("37+e");

    std::string id1 = b1.computeCanonicalGameId();
    std::string id2 = b2.computeCanonicalGameId();
    CHECK_EQUAL(64, id1.size());
    STRCMP_EQUAL(id1.c_str(), id2.c_str());

    // Different variant parameter (superProngs = true) gives different hash
    octi::OctiBoard b3(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    b3.getGame()->start(octi::PLAYER_A);
    b3.getGame()->move("33+b");
    b3.getGame()->move("37+e");
    std::string id3 = b3.computeCanonicalGameId();
    CHECK_FALSE(id1 == id3);

    // Another move gives different hash
    b1.getGame()->move("33");
    std::string id1After = b1.computeCanonicalGameId();
    CHECK_FALSE(id1 == id1After);
}

TEST_GROUP(OctiForKidsHardeningAndBotTest)
{
};

TEST(OctiForKidsHardeningAndBotTest, NormalizesSuperProngsEdgelessAndBases)
{
    octi::OctiBoard board(octi::GAME_MODE_OFK, true, true, octi::GAME_BASES_3);
    octi::OctiGame *game = board.getGame();
    CHECK_TRUE(game != nullptr);
    CHECK_EQUAL(octi::GAME_MODE_OFK, board.getGameMode());
    CHECK_EQUAL(octi::GAME_MODE_OFK, game->getMode());
    CHECK_FALSE(game->useSuperProngs());
    CHECK_FALSE(game->useEdgelessBoard());
    CHECK_EQUAL(octi::GAME_BASES_1, game->getBases());

    game->reset();
    CHECK_FALSE(game->useSuperProngs());
    CHECK_FALSE(game->useEdgelessBoard());
    CHECK_EQUAL(octi::GAME_BASES_1, game->getBases());
}

TEST(OctiForKidsHardeningAndBotTest, FromPositionStringInvariants)
{
    octi::OctiBoard board;

    // Accepts valid OFK position with capPods in 0..4
    board.fromPositionString("OFK:1:0:0:P:A:-:0:0,1,12,0:0,2,11,0:A22,A32,A42,A52,B26,B36");
    CHECK_EQUAL(octi::GAME_MODE_OFK, board.getGameMode());
    CHECK_EQUAL(1, board.getGame()->getCapturedPods(octi::PLAYER_A));
    CHECK_EQUAL(2, board.getGame()->getCapturedPods(octi::PLAYER_B));

    // Rejects freePods > 0
    CHECK_THROWS(octi::IllegalMoveSyntaxException,
                 board.fromPositionString("OFK:1:0:0:P:A:-:0:1,0,12,0:0,0,12,0:A22,A32,A42,A52,B26,B36,B46,B56"));

    // Rejects freeSuperProngs > 0
    CHECK_THROWS(octi::IllegalMoveSyntaxException,
                 board.fromPositionString("OFK:1:0:0:P:A:-:0:0,0,11,1:0,0,12,0:A22,A32,A42,A52,B26,B36,B46,B56"));

    // Rejects superProngs enabled in header
    CHECK_THROWS(octi::IllegalMoveSyntaxException,
                 board.fromPositionString("OFK:1:1:0:P:A:-:0:0,0,12,0:0,0,12,0:A22,A32,A42,A52,B26,B36,B46,B56"));

    // Rejects edgeless enabled in header
    CHECK_THROWS(octi::IllegalMoveSyntaxException,
                 board.fromPositionString("OFK:1:0:1:P:A:-:0:0,0,12,0:0,0,12,0:A22,A32,A42,A52,B26,B36,B46,B56"));

    // Rejects bases != 1
    CHECK_THROWS(octi::IllegalMoveSyntaxException,
                 board.fromPositionString("OFK:3:0:0:P:A:-:0:0,0,12,0:0,0,12,0:A22,A32,A42,A52,B26,B36,B46,B56"));

    // Rejects pod with superprong '!'
    CHECK_THROWS(octi::IllegalMoveSyntaxException,
                 board.fromPositionString("OFK:1:0:0:P:A:-:0:0,0,12,0:0,0,12,0:A22a!,A32,A42,A52,B26,B36,B46,B56"));

    // Rejects stacked pods on same square
    CHECK_THROWS(octi::IllegalMoveSyntaxException,
                 board.fromPositionString("OFK:1:0:0:P:A:-:0:0,0,12,0:0,0,12,0:A22a,A22b,A32,A42,A52,B26,B36,B46,B56"));

    // Rejects composite moves in OFK
    octi::OctiBoard ofkBoard(octi::GAME_MODE_OFK);
    ofkBoard.getGame()->start(octi::PLAYER_A);
    ofkBoard.getGame()->move("22+a");
    ofkBoard.getGame()->move("26+e");
    CHECK_THROWS(octi::IllegalMoveException,
                 ofkBoard.getGame()->move("22a-23,22a-24"));
}

TEST(OctiForKidsHardeningAndBotTest, SerializeJsonAccurateTerminationStrings)
{
    // 1. BASE_OCCUPATION in OFK
    {
        octi::OctiBoard board(octi::GAME_MODE_OFK);
        // Pod A at 25 with prong 'a' (dir N). B base is at 26 (y=5).
        // Moving from 25 to 26 captures base 26!
        board.fromPositionString("OFK:1:0:0:P:A:-:0:0,0,11,0:0,0,12,0:A25a,A32,A42,A52,B36,B46,B56");
        octi::OctiGame *game = board.getGame();
        game->move("25a-26");
        CHECK_TRUE(game->isGameOver());
        CHECK_EQUAL(octi::PLAYER_A, game->getWinner());

        std::string jsonStr = board.serializeJson();
        auto root = nlohmann::json::parse(jsonStr);
        STRCMP_EQUAL("BASE_OCCUPATION", root["termination"].get<std::string>().c_str());
    }

    // 2. TOTAL_CAPTURE in OFK
    {
        octi::OctiBoard board(octi::GAME_MODE_OFK);
        // Player B has only 1 pod left at 24. A pod at 23 with prong 'a' (N) jumps over B at 24 to 25.
        // Jump captures B's last pod! (x=1, y=3 is 24; jump lands at 25, x=1, y=4).
        board.fromPositionString("OFK:1:0:0:P:A:-:0:0,3,11,0:0,0,12,0:A23a,A32,A42,A52,B24");
        octi::OctiGame *game = board.getGame();
        game->move("23a-25x");
        CHECK_TRUE(game->isGameOver());
        CHECK_EQUAL(octi::PLAYER_A, game->getWinner());

        std::string jsonStr = board.serializeJson();
        auto root = nlohmann::json::parse(jsonStr);
        STRCMP_EQUAL("TOTAL_CAPTURE", root["termination"].get<std::string>().c_str());
    }

    // 3. STALEMATE in OFK
    {
        octi::OctiBoard board(octi::GAME_MODE_OFK);
        // Player B has 1 pod at 34 with NO prongs and freeProngs == 0.
        // Player A moves 22+a. Then it becomes B's turn, but B has no prongs, 0 free prongs, cannot place or reposition!
        board.fromPositionString("OFK:1:0:0:P:A:-:0:0,0,11,0:0,0,0,0:A22,A32,A42,A52,B34");
        octi::OctiGame *game = board.getGame();
        game->move("22+a");
        CHECK_TRUE(game->isGameOver());
        CHECK_EQUAL(octi::PLAYER_A, game->getWinner());

        std::string jsonStr = board.serializeJson();
        auto root = nlohmann::json::parse(jsonStr);
        STRCMP_EQUAL("STALEMATE", root["termination"].get<std::string>().c_str());
    }
}

TEST(OctiForKidsHardeningAndBotTest, BotEngineNoPrematurePenaltyCliffAndFindsLegalMove)
{
    octi::OctiBoard board6(octi::GAME_MODE_OFK);
    octi::OctiBoard board5(octi::GAME_MODE_OFK);
    board6.fromPositionString("OFK:1:0:0:P:A:-:0:0,0,6,0:0,0,12,0:A22,A32,A42,A52,B26,B36,B46,B56");
    board5.fromPositionString("OFK:1:0:0:P:A:-:0:0,0,5,0:0,0,12,0:A22,A32,A42,A52,B26,B36,B46,B56");

    int score6 = octi::OctiBotEngine::evaluatePosition(board6, octi::PLAYER_A);
    int score5 = octi::OctiBotEngine::evaluatePosition(board5, octi::PLAYER_A);

    // Difference must be approximately 1 prong worth (50 points), NOT the old 6000-point cliff!
    int diff = std::abs(score6 - score5);
    CHECK_TRUE(diff < 200);

    // Verify chooseMove finds a legal move without throwing
    octi::OctiBotEngine bot(octi::BotDifficulty::CASUAL);
    bot.setTimeBudgetMs(100);
    std::string moveStr = bot.chooseMove(board6);
    CHECK_FALSE(moveStr.empty());

    // Applying the chosen move must succeed
    board6.getGame()->move(moveStr);
}

// ---------------------------------------------------------------------------
// 4-Player Rulebook Compliance Tests
// ---------------------------------------------------------------------------

TEST_GROUP(Octi4PlayerRulebookComplianceTest) {
    void setup() {}
    void teardown() {}
};

TEST(Octi4PlayerRulebookComplianceTest, RejectFourIllegalPliesFromTranscript84628218)
{
    octi::OctiBoard board(octi::GAME_MODE_4);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    // 1. Ply 26: Player B cannot place reserve pod on neutral square (5, 3) / (4, 2)
    game->move("33+b"); // A
    board.squareAt(4, 2).clear();
    board.squareAt(4, 2).addPod(octi::OctiPod(octi::B_POD));
    std::vector<octi::OctiMove> bMoves = game->generateLegalMoves();
    for (const auto &m : bMoves) {
        CHECK_FALSE(m.asString() == "53");
    }
    CHECK_THROWS(octi::IllegalMoveException, game->move("53"));

    // 2. Ply 52: Player D cannot place reserve pod on partner B's home square (3, 7) / (2, 6)
    board.squareAt(2, 6).clear();
    board.squareAt(2, 6).addPod(octi::OctiPod(octi::D_POD));
    octi::OctiGameSnapshot snapD = game->captureSnapshot();
    snapD.turn = octi::PLAYER_D;
    snapD.players[octi::PLAYER_D].freePods = 2;
    game->restoreSnapshot(snapD);

    std::vector<octi::OctiMove> dMoves = game->generateLegalMoves();
    for (const auto &m : dMoves) {
        CHECK_FALSE(m.asString() == "37");
    }
    CHECK_THROWS(octi::IllegalMoveException, game->move("37"));

    // 3. Ply 82: Player C cannot liberate on enemy base 37 occupied ONLY by partner A
    board.squareAt(2, 6).clear();
    board.squareAt(2, 6).addPod(octi::OctiPod(octi::A_POD));
    octi::OctiGameSnapshot snapC = game->captureSnapshot();
    snapC.turn = octi::PLAYER_C;
    snapC.players[octi::PLAYER_C].capturedPods = 1;
    game->restoreSnapshot(snapC);

    std::vector<octi::OctiMove> cMoves = game->generateLegalMoves();
    for (const auto &m : cMoves) {
        CHECK_FALSE(m.asString() == "37*");
    }
    CHECK_THROWS(octi::IllegalMoveException, game->move("37*"));

    // 4. Ply 100: Player C cannot place reserve pod on neutral square 53 occupied only by partner A
    board.squareAt(4, 2).clear();
    board.squareAt(4, 2).addPod(octi::OctiPod(octi::A_POD));
    snapC = game->captureSnapshot();
    snapC.turn = octi::PLAYER_C;
    snapC.players[octi::PLAYER_C].freePods = 2;
    game->restoreSnapshot(snapC);

    cMoves = game->generateLegalMoves();
    for (const auto &m : cMoves) {
        CHECK_FALSE(m.asString() == "53");
    }
    CHECK_THROWS(octi::IllegalMoveException, game->move("53"));
}

TEST(Octi4PlayerRulebookComplianceTest, PartnerRevivalOnPartnerHome)
{
    octi::OctiBoard board(octi::GAME_MODE_4);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    // R6: Partner revival is abolished.
    // Case 1: Surviving D occupies eliminated partner B's home (37) with D capturedPods == 0
    // (even though eliminated B has capturedPods == 2) -> "37*" is forbidden and never generated.
    board.squareAt(2, 6).clear();
    board.squareAt(2, 6).addPod(octi::OctiPod(octi::D_POD));

    octi::OctiGameSnapshot snap = game->captureSnapshot();
    snap.turn = octi::PLAYER_D;
    snap.players[octi::PLAYER_B].eliminated = true;
    snap.players[octi::PLAYER_B].capturedPods = 2;
    snap.players[octi::PLAYER_D].capturedPods = 0;
    game->restoreSnapshot(snap);

    std::vector<octi::OctiMove> dMoves = game->generateLegalMoves();
    for (const auto &m : dMoves) {
        CHECK_FALSE(m.asString() == "37*");
    }
    CHECK_THROWS(octi::IllegalMoveException, game->move("37*"));
    CHECK_TRUE(game->isPlayerEliminated(octi::PLAYER_B));

    // Case 2: Eliminated partner B's home (37) is empty while D has liberation clearance & own capturedPods ->
    // "37*" is still forbidden because 37 is unoccupied by D.
    board.squareAt(2, 6).clear();
    board.squareAt(4, 2).clear();
    board.squareAt(4, 2).addPod(octi::OctiPod(octi::D_POD)); // D occupies neutral OCTI square 53 -> canLiberate(D)
    snap = game->captureSnapshot();
    snap.turn = octi::PLAYER_D;
    snap.players[octi::PLAYER_B].eliminated = true;
    snap.players[octi::PLAYER_B].capturedPods = 2;
    snap.players[octi::PLAYER_D].capturedPods = 1;
    game->restoreSnapshot(snap);

    dMoves = game->generateLegalMoves();
    for (const auto &m : dMoves) {
        CHECK_FALSE(m.asString() == "37*");
    }
    CHECK_THROWS(octi::IllegalMoveException, game->move("37*"));

    // Case 3: Surviving D personally occupies partner B's home (37), has liberation clearance (via 53),
    // and holds D's own capturedPods == 1 -> "37*" liberates D's OWN pod while B stays eliminated!
    board.squareAt(2, 6).addPod(octi::OctiPod(octi::D_POD));
    snap = game->captureSnapshot();
    snap.turn = octi::PLAYER_D;
    snap.players[octi::PLAYER_B].eliminated = true;
    snap.players[octi::PLAYER_B].capturedPods = 2;
    snap.players[octi::PLAYER_D].capturedPods = 1;
    game->restoreSnapshot(snap);

    dMoves = game->generateLegalMoves();
    bool has37Star = false;
    for (const auto &m : dMoves) {
        if (m.asString() == "37*") {
            has37Star = true;
            break;
        }
    }
    CHECK_TRUE(has37Star);

    game->move("37*");
    CHECK_TRUE(game->isPlayerEliminated(octi::PLAYER_B));
    LONGS_EQUAL(2, game->getCapturedPods(octi::PLAYER_B));
    LONGS_EQUAL(0, game->getCapturedPods(octi::PLAYER_D));
    LONGS_EQUAL(2, board.squareAt(2, 6).getNumPods());
    CHECK_EQUAL(octi::D_POD, board.squareAt(2, 6).getPodAt(0).getType());
    CHECK_EQUAL(octi::D_POD, board.squareAt(2, 6).getPodAt(1).getType());
}

TEST(Octi4PlayerRulebookComplianceTest, CanLiberatePersonalOccupancyOnly)
{
    octi::OctiBoard board(octi::GAME_MODE_4);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    // Enemy base for A/C is (2, 6) or (6, 2)
    board.squareAt(2, 6).clear();
    board.squareAt(2, 6).addPod(octi::OctiPod(octi::A_POD));

    CHECK_TRUE(board.canLiberate(octi::PLAYER_A));
    CHECK_FALSE(board.canLiberate(octi::PLAYER_C));

    board.squareAt(4, 2).clear();
    board.squareAt(4, 2).addPod(octi::OctiPod(octi::C_POD));
    CHECK_TRUE(board.canLiberate(octi::PLAYER_C));
}

TEST(Octi4PlayerRulebookComplianceTest, StalemateAndTerminationSerialization)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    for (int y = 0; y < board.getBoardHeight(); ++y) {
        for (int x = 0; x < board.getBoardWidth(); ++x) {
            if (board.squareAt(x, y).isOccupiedBy(octi::PLAYER_B)) {
                board.squareAt(x, y).clear();
            }
        }
    }
    octi::OctiGameSnapshot snap = game->captureSnapshot();
    snap.players[octi::PLAYER_B].freePods = 0;
    game->restoreSnapshot(snap);

    game->move("33+b");
    CHECK_TRUE(game->isGameOver());
    CHECK_EQUAL(octi::PLAYER_A, game->getWinner());

    std::string jsonStr = game->serializeJson();
    nlohmann::json j = nlohmann::json::parse(jsonStr);
    CHECK_EQUAL("TOTAL_CAPTURE", j["termination"].get<std::string>());
}

// ---------------------------------------------------------------------------
// Tactical Upgrade & Blunder Refutation Tests
// ---------------------------------------------------------------------------

TEST_GROUP(OctiBotTacticalUpgradeTest) {
    void setup() {}
    void teardown() {}
};

TEST(OctiBotTacticalUpgradeTest, TuningProfilesDefaultAndCustom)
{
    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
    const octi::BotTuningProfile &prof = bot.tuningProfile();
    LONGS_EQUAL(10000, prof.timeBudgetMs);
    LONGS_EQUAL(64, prof.maxSearchDepth);
    LONGS_EQUAL(2, prof.maxQuiesceDepth);
    LONGS_EQUAL(6, prof.quiesceBranchCap);
    LONGS_EQUAL(40, prof.rootBranchCap);
    LONGS_EQUAL(24, prof.treeBranchCap);
    LONGS_EQUAL(2, prof.lazySmpThreads);

    octi::BotTuningProfile custom = prof;
    custom.maxSearchDepth = 12;
    custom.maxQuiesceDepth = 5;
    bot.setTuningProfile(custom);
    CHECK_EQUAL(12, bot.tuningProfile().maxSearchDepth);
    CHECK_EQUAL(5, bot.tuningProfile().maxQuiesceDepth);
}

TEST(OctiBotTacticalUpgradeTest, MultiPodDevelopmentBonusAndMonopolyPenalty)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    int score0 = octi::OctiBotEngine::evaluatePosition(board, octi::PLAYER_A);

    board.squareAt(2, 2).podAt(0).addProng(octi::DIR_N);
    int score1 = octi::OctiBotEngine::evaluatePosition(board, octi::PLAYER_A);
    CHECK_TRUE(score1 > score0);

    board.squareAt(4, 2).podAt(0).addProng(octi::DIR_N);
    int score2 = octi::OctiBotEngine::evaluatePosition(board, octi::PLAYER_A);
    CHECK_TRUE(score2 > score1);

    board.squareAt(4, 2).podAt(0).removeProng(octi::DIR_N);
    board.squareAt(2, 2).podAt(0).addProng(octi::DIR_E);
    board.squareAt(2, 2).podAt(0).addProng(octi::DIR_NE);
    board.squareAt(2, 2).podAt(0).addProng(octi::DIR_NW);
    octi::BotTuningProfile prof = octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::TACTICAL);
    int scoreMonopoly = octi::OctiBotEngine::evaluatePosition(board, octi::PLAYER_A, prof);
    CHECK_TRUE(scoreMonopoly < score2 + 200);
}

TEST(OctiBotTacticalUpgradeTest, LeapfrogBatteryEvaluation)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    board.squareAt(2, 2).clear();
    board.squareAt(4, 2).clear();
    board.squareAt(6, 2).clear();

    board.squareAt(3, 3).addPod(octi::OctiPod(octi::A_POD));
    board.squareAt(3, 3).podAt(0).addProng(octi::DIR_N);

    octi::BotTuningProfile prof = octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::TACTICAL);
    int scoreNoPivot = octi::OctiBotEngine::evaluatePosition(board, octi::PLAYER_A, prof);

    board.squareAt(3, 4).addPod(octi::OctiPod(octi::A_POD));
    int scoreWithPivot = octi::OctiBotEngine::evaluatePosition(board, octi::PLAYER_A, prof);
    CHECK_TRUE(scoreWithPivot > scoreNoPivot);
}

TEST(OctiBotTacticalUpgradeTest, RefuteBlunderGameC205F2c1Ply14)
{
    octi::OctiBoard board(octi::GAME_MODE_OFK);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    game->move("22+b");
    game->move("26+e");
    game->move("22b-33");
    game->move("26e-25");
    game->move("33b+h");
    game->move("25e+d");
    game->move("32+a");
    game->move("25de+b");
    game->move("32a+h");
    game->move("25bde-34");
    game->move("32ah+b");
    game->move("34bde+f");
    game->move("33bh+a");

    CHECK_EQUAL(octi::PLAYER_B, game->getTurn());

    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
    bot.setTimeBudgetMs(2500);
    std::string chosen = bot.chooseMove(board);
    CHECK_FALSE(chosen == "34bdef-43");
}

TEST(OctiBotTacticalUpgradeTest, ReservePodsPreventPrematureTotalCaptureAndRefutePly34Blunder)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.fromPositionString("2:3:0:0:P:B:-:0:2,0,21,0:4,2,14,0:A33,A53a,A73h,A44ah,B55adefg,A78abcfgh");
    octi::OctiGame *game = board.getGame();

    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
    bot.setTimeBudgetMs(2500);
    std::string chosen = bot.chooseMove(board);
    CHECK_FALSE(chosen == "55adefg-54");

    // If Player B plays the blunder 55adefg-54 and Player A captures B's last board pod via 53a-55x,
    // the game must NOT end because Player B still has 4 reserve pods and empty home squares 37, 57, 77.
    game->move("55adefg-54");
    game->move("53a-55x");
    CHECK_FALSE(game->isGameOver());
    CHECK_EQUAL(octi::PLAYER_B, game->getTurn());
    CHECK_EQUAL(4, game->getFreePods(octi::PLAYER_B));

    // Player B can legally place a reserve pod on 57
    game->move("57");
    CHECK_FALSE(game->isGameOver());
    CHECK_EQUAL(octi::PLAYER_A, game->getTurn());
    CHECK_EQUAL(3, game->getFreePods(octi::PLAYER_B));
}

TEST(OctiBotTacticalUpgradeTest, BotEngineRealGame3BaseTacticalDecisions)
{
    // 1. Ply 74 of game 3c8ded87926d3900684b14c5a9a31c993c466e9c0f0d85dd43a2bf83a561302b:
    // Jump capture 77defh-55-33x (capturing A44b and landing on A's base 33) exceeds passive 66abcdefg-65
    {
        octi::OctiBoard bCap(octi::GAME_MODE_2);
        bCap.fromPositionString("2:3:0:0:P:B:-:0:2,3,23,0:0,5,19,0:A44b,A64bh,A35abefgh,B66abcdefg,B77defh");
        bCap.getGame()->move("77defh-55-33x");

        octi::OctiBoard bPass(octi::GAME_MODE_2);
        bPass.fromPositionString("2:3:0:0:P:B:-:0:2,3,23,0:0,5,19,0:A44b,A64bh,A35abefgh,B66abcdefg,B77defh");
        bPass.getGame()->move("66abcdefg-65");

        int evalCap = octi::OctiBotEngine::evaluatePosition(bCap, octi::PLAYER_B);
        int evalPass = octi::OctiBotEngine::evaluatePosition(bPass, octi::PLAYER_B);
        CHECK_TRUE(evalCap > evalPass);
    }

    // 2. Ply 92: B must capture A76h via 66abcdefg-86x rather than stepping away to 66abcdefg-65
    {
        octi::OctiBoard board(octi::GAME_MODE_2);
        board.fromPositionString("2:3:0:0:P:B:-:0:1,5,21,0:0,5,19,0:A35abefgh,A76h,B66abcdefg,B77cdef");
        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
        bot.setTimeBudgetMs(1500);
        std::string chosen = bot.chooseMove(board);
        CHECK_TRUE(chosen.find("x") != std::string::npos);
        CHECK_FALSE(chosen == "66abcdefg-65");
    }

    // 3. Ply 98: B has 1 pod on board, 0 freePods, 3-base game (base win mathematically impossible).
    // B must capture A55h via 65acdefg-45x rather than chasing empty base distance via 65acdefg-54.
    {
        octi::OctiBoard board(octi::GAME_MODE_2);
        board.fromPositionString("2:3:0:0:P:B:-:0:0,5,21,0:0,6,24,0:B65acdefg,A55h,A35abefgh");
        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
        bot.setTimeBudgetMs(1500);
        std::string chosen = bot.chooseMove(board);
        CHECK_TRUE(chosen.find("65acdefg-45x") == 0);
        CHECK_FALSE(chosen == "65acdefg-54");
    }
}

TEST(OctiBotTacticalUpgradeTest, BotEngineThreeVariants2P4POfkEvaluationAndTactics)
{
    // 1. 128-bit jump mask on row 9 (y=8, sqBit >= 72): pod at (6, 6) jumping over enemy at (7, 7) to (8, 8)
    {
        octi::OctiBoard board(octi::GAME_MODE_2);
        board.fromPositionString("2:3:0:0:P:A:-:0:3,0,20,0:3,0,20,0:A67b,B78a");
        int scoreWithTarget = octi::OctiBotEngine::evaluatePosition(board, octi::PLAYER_A);

        octi::OctiBoard boardNoProng(octi::GAME_MODE_2);
        boardNoProng.fromPositionString("2:3:0:0:P:A:-:0:3,0,21,0:3,0,20,0:A67,B78a");
        int scoreWithoutProng = octi::OctiBotEngine::evaluatePosition(boardNoProng, octi::PLAYER_A);
        CHECK_TRUE(scoreWithTarget > scoreWithoutProng + 3000);
    }

    // 2. Per-pod base-threat deduplication & 3-base vs 1-base scaling
    {
        octi::OctiBoard board1Base(octi::GAME_MODE_2);
        board1Base.fromPositionString("2:1:0:0:P:A:-:0:3,0,20,0:3,0,20,0:A66bh,B37");
        octi::OctiBoard board3Base(octi::GAME_MODE_2);
        board3Base.fromPositionString("2:3:0:0:P:A:-:0:3,0,20,0:3,0,20,0:A66bh,B37");

        int eval1 = octi::OctiBotEngine::evaluatePosition(board1Base, octi::PLAYER_A);
        int eval3 = octi::OctiBotEngine::evaluatePosition(board3Base, octi::PLAYER_A);
        // In 1-base mode, threatening a base at (6,6)->(5,7)/(7,7) is an immediate game-winning threat (100% scale),
        // whereas in 3-base mode with 0 bases captured it is scaled down to 20%.
        CHECK_TRUE(eval1 > eval3 + 4000);
    }

    // 3. 4P (2v2 coalition) single-seat elimination defense penalty
    {
        octi::OctiBoard board4P(octi::GAME_MODE_4);
        // Player C has 1 pod on board at (4,4), 0 freePods, threatened by B at (4,3) with North prong 'a'
        board4P.fromPositionString("4:2:0:0:P:B:-:0:3,0,10,0:3,0,10,0:0,3,10,0:3,0,10,0:A22,C44,B43a,D68");
        int scoreThreatened = octi::OctiBotEngine::evaluatePosition(board4P, octi::PLAYER_A);

        octi::OctiBoard board4PSafe(octi::GAME_MODE_4);
        board4PSafe.fromPositionString("4:2:0:0:P:B:-:0:3,0,10,0:3,0,10,0:1,2,10,0:3,0,10,0:A22,C44,B43a,D68");
        int scoreWithReserve = octi::OctiBotEngine::evaluatePosition(board4PSafe, octi::PLAYER_A);
        // Having 0 freePods when C's sole board pod is hanging incurs single-seat elimination danger penalty
        CHECK_TRUE(scoreWithReserve > scoreThreatened + 5000);
    }

    // 4. OFK (7x7 OCTI for Kids) capture prioritization with zero reserves
    {
        octi::OctiBoard boardOfk(octi::GAME_MODE_OFK);
        boardOfk.fromPositionString("OFK:1:0:0:P:A:-:0:0,0,8,0:0,0,8,0:A33a,A22,B34e,B56");
        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
        bot.setTimeBudgetMs(1000);
        std::string chosen = bot.chooseMove(boardOfk);
        CHECK_EQUAL(std::string("33a-35x"), chosen);
    }
}

TEST(OctiBotTacticalUpgradeTest, RefuteRedundantLiberationSpamAndBackRankProngHoarding)
{
    // Nominal 1: Anti-Redundant-Liberation Spam from #g-20261106-0116 Ply 63
    // In #g-20261106-0116, Player A held enemy base 57 with A57abch and had liberated A57.
    // Playing a second liberation 57* stacks a second 0-prong pod on 57, wasting a captured pod and
    // incurring capBasePen. evaluatePosition after 57* must be strictly lower, and chooseMove must not choose 57*.
    {
        octi::OctiBoard board(octi::GAME_MODE_2);
        board.fromPositionString("2:3:0:0:P:A:-:0:0,3,6,0:0,2,14,0:A57abch,A57,A44abeh,A73abh,B63cdefg,B67fg");
        int scoreBeforeLib = octi::OctiBotEngine::evaluatePosition(board, octi::PLAYER_A);

        octi::OctiBoard boardAfterLib(octi::GAME_MODE_2);
        boardAfterLib.fromPositionString("2:3:0:0:P:B:-:0:0,2,6,0:0,2,14,0:A57abch,A57,A57,A44abeh,A73abh,B63cdefg,B67fg");
        int scoreAfterLib = octi::OctiBotEngine::evaluatePosition(boardAfterLib, octi::PLAYER_A);

        CHECK_TRUE(scoreBeforeLib > scoreAfterLib);

        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
        bot.setTimeBudgetMs(1000);
        std::string chosen = bot.chooseMove(board);
        CHECK_FALSE(chosen == "57*");
    }

    // Boundary: Solo 0-Prong Base Holder Retention
    // When an armed pod vacates a captured enemy base, leaving a single 0-prong pod (A57) to hold the base,
    // the position retains base capture value and must evaluate > 5000 points higher than leaving the base empty.
    {
        octi::OctiBoard boardHeld(octi::GAME_MODE_2);
        boardHeld.fromPositionString("2:3:0:0:P:B:-:0:0,3,6,0:0,2,14,0:A57,A47abch,B67fg");
        int scoreHeld = octi::OctiBotEngine::evaluatePosition(boardHeld, octi::PLAYER_A);

        octi::OctiBoard boardEmpty(octi::GAME_MODE_2);
        boardEmpty.fromPositionString("2:3:0:0:P:B:-:0:0,4,6,0:0,2,14,0:A47abch,B67fg");
        int scoreEmpty = octi::OctiBotEngine::evaluatePosition(boardEmpty, octi::PLAYER_A);

        CHECK_TRUE(scoreHeld > scoreEmpty + 5000);
    }

    // Dominance / Midgame Advancement Over Back-Rank 4th+ Prong Hoarding
    // With 3 armed pods on row 3 (dist >= 4), adding a 4th prong to a back-rank pod incurs the hoarding penalty,
    // whereas advancing across the midline (e.g. 53abc-54) yields forward distance and reach bonuses.
    {
        octi::OctiBoard bBack3(octi::GAME_MODE_2);
        bBack3.fromPositionString("2:3:0:0:P:A:-:0:1,0,16,0:1,0,25,0:A33abc,A53abc,A73abc,B37,B57,B77");
        int score3Prongs = octi::OctiBotEngine::evaluatePosition(bBack3, octi::PLAYER_A);

        // Position with 4th prong added to back-rank pod A33: freeProngs drops by 1 (16 -> 15)
        octi::OctiBoard bBack4(octi::GAME_MODE_2);
        bBack4.fromPositionString("2:3:0:0:P:B:-:0:1,0,15,0:1,0,25,0:A33abcd,A53abc,A73abc,B37,B57,B77");
        int score4Prongs = octi::OctiBotEngine::evaluatePosition(bBack4, octi::PLAYER_A);

        // Position with 53abc advancing to 54 (dist reduced from 4 to 3 toward targets)
        octi::OctiBoard bAdv(octi::GAME_MODE_2);
        bAdv.fromPositionString("2:3:0:0:P:B:-:0:1,0,16,0:1,0,25,0:A33abc,A54abc,A73abc,B37,B57,B77");
        int scoreAdv = octi::OctiBotEngine::evaluatePosition(bAdv, octi::PLAYER_A);

        // Advancing forward scores higher than staying back and hoarding a 4th prong
        CHECK_TRUE(scoreAdv > score4Prongs);
        // And the hoarding penalty is active on bBack4 compared to pod prong base growth
        CHECK_TRUE(score4Prongs - score3Prongs < 500);
    }
}

TEST(OctiBotTacticalUpgradeTest, RefuteSelfVacatingStepIntoCaptureLanePly16)
{
    // #g-20261106-0147 Ply 16: B66de must NOT step to 65, which vacates 66 and allows A64ah to jump-capture via 64ah-66x
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.fromPositionString("2:3:0:0:P:B:-:0:4,0,18,0:4,0,20,0:A33ab,A53abh,A64ah,B46de,B66de,B77f");
    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
    bot.setTimeBudgetMs(1000);
    std::string chosen = bot.chooseMove(board);
    CHECK_FALSE(chosen == "66de-65");
}

TEST(OctiBotTacticalUpgradeTest, RefuteLonePodProngMonopolyAndUnsafeHomeBasePodPlacement)
{
    // #g-20261106-0147 Plies 28-34:
    // Player B has lone pod on board B46abcdef (6 prongs), 4 freePods in reserve, and A66abfgh is threatening home square 57 via jump-lane 66abfgh-48x.
    // Bot must NOT add 7th prong (46abcdef+g, 46abcdef+h), must NOT rearrange (46abcdef+g-a),
    // and must NOT place a new pod on jump-threatened square 57 (safe home square is 37 or 77).
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.fromPositionString("2:3:0:0:P:B:-:0:3,1,18,0:4,2,17,0:A33a,A34abh,B46abcdef,A66abfgh");
    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
    octi::BotTuningProfile prof =
        octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::TACTICAL);
    prof.maxSearchDepth = 4;
    prof.lazySmpThreads = 1;
    prof.rootScoreJitter = 0;
    prof.timeBudgetMs = 60000;
    bot.setTuningProfile(prof);
    std::string chosen = bot.chooseMove(board);
    CHECK_FALSE(chosen == "46abcdef+g");
    CHECK_FALSE(chosen == "46abcdef+h");
    CHECK_FALSE(chosen == "46abcdef+g-a");
    CHECK_FALSE(chosen == "46abcdef+h-a");
    CHECK_FALSE(chosen == "57");

    // evaluatePosition checks:
    // Monopoly/overarming: adding a 7th prong to lone pod vs placing a new pod on 37
    octi::OctiBoard bProng(octi::GAME_MODE_2);
    bProng.fromPositionString("2:3:0:0:P:A:-:0:3,1,18,0:4,2,16,0:A33a,A34abh,B46abcdefg,A66abfgh");
    int evalProng = octi::OctiBotEngine::evaluatePosition(bProng, octi::PLAYER_B);

    octi::OctiBoard bPlaceSafe(octi::GAME_MODE_2);
    bPlaceSafe.fromPositionString("2:3:0:0:P:A:-:0:3,1,18,0:3,2,17,0:A33a,A34abh,B46abcdef,A66abfgh,B37");
    int evalPlaceSafe = octi::OctiBotEngine::evaluatePosition(bPlaceSafe, octi::PLAYER_B);

    CHECK_TRUE(evalPlaceSafe > evalProng);
}

TEST(OctiBotTacticalUpgradeTest, RefuteProngRearrangementOscillationPly97)
{
    // #g-20261106-0142 Ply 97:
    // Pod A35abg should NOT oscillate with +h-b / +b-h prong rearrangements
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.fromPositionString("2:3:0:0:P:A:-:0:0,1,10,0:0,0,15,0:A35abg,A53adefh,B64abcdefh");
    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
    bot.setTimeBudgetMs(1000);
    std::string chosen = bot.chooseMove(board);
    CHECK_FALSE(chosen == "35abg+h-b");
    CHECK_FALSE(chosen == "35abg+h-a");
    CHECK_FALSE(chosen == "35abg+h-g");
    CHECK_FALSE(chosen == "35abg+c-b");
}

TEST(OctiBotTacticalUpgradeTest, ImmobilizedArmedPodAndCanLiberateUnfreezing)
{
    // #g-20261106-0147 Ply 19:
    // B55f is pinned/immobilized: prong f points SW to 44 and 33, but 44 has A44abh and 33 has A33ab!
    // B55f cannot step (44 has enemy) and cannot jump (33 is occupied), so legalMoveDirs == 0.
    // An immobilized armed pod under enemy threat receives heavy penalty.
    octi::OctiBoard bPinned(octi::GAME_MODE_2);
    bPinned.fromPositionString("2:3:0:0:P:B:-:0:3,0,18,0:4,1,20,0:A33ab,A44abh,B55f,B46de");
    int scorePinned = octi::OctiBotEngine::evaluatePosition(bPinned, octi::PLAYER_B);

    octi::OctiBoard bFree(octi::GAME_MODE_2);
    bFree.fromPositionString("2:3:0:0:P:B:-:0:3,0,18,0:4,1,20,0:A33ab,A44abh,B55fh,B46de");
    int scoreFree = octi::OctiBotEngine::evaluatePosition(bFree, octi::PLAYER_B);
    CHECK_TRUE(scoreFree > scorePinned + 1000);

    // #g-20261106-0142: Reduced canLiberate bonus (450 vs 2500)
    // A pod with 5 prongs on a friendly base (A53adefh) has incentive to advance forward rather than
    // sit frozen for 80 plies due to passive +2500 canLiberate inflation.
    octi::OctiBoard bOnBase(octi::GAME_MODE_2);
    bOnBase.fromPositionString("2:3:0:0:P:A:-:0:0,1,10,0:0,0,15,0:A53adefh,B64abcdefh");
    int scoreBase = octi::OctiBotEngine::evaluatePosition(bOnBase, octi::PLAYER_A);

    octi::OctiBoard bAdvBase(octi::GAME_MODE_2);
    bAdvBase.fromPositionString("2:3:0:0:P:A:-:0:0,1,10,0:0,0,15,0:A54adefh,B64abcdefh");
    int scoreAdv = octi::OctiBotEngine::evaluatePosition(bAdvBase, octi::PLAYER_A);
    // Advancing toward enemy bases now competes strongly against sitting passively on friendly home base
    CHECK_TRUE(scoreAdv > scoreBase - 1500);
}

TEST(OctiBotTacticalUpgradeTest, SearchConvergenceAndAntiFarmingMatches0165_0167_0171)
{
    // C2 (#g-20261106-0165 Ply 55): Hard dominance pruning of single-op +x-y when freeProngs > 0 && numProngs < 4
    {
        octi::OctiBoard b1(octi::GAME_MODE_2);
        b1.fromPositionString("2:3:0:0:P:A:-:0:1,0,10,0:0,0,11,0:A25abch,A44abfh,A53abc,A62abgh,B35defg,B37def,B55bcdef,B75cdfg");
        CHECK_EQUAL(10, b1.getGame()->getFreeProngs(octi::PLAYER_A));
        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
        bot.setTimeBudgetMs(500);
        std::string move1 = bot.chooseMove(b1);
        // Pod A53abc has 3 prongs (< 4). Verify chooseMove never selects a +x-y move on it.
        CHECK_FALSE(move1 == "53abc+d-a");
        CHECK_FALSE(move1 == "53abc+d-b");
        CHECK_FALSE(move1 == "53abc+d-c");
        CHECK_FALSE(move1 == "53abc+e-a");
        CHECK_FALSE(move1 == "53abc+e-b");
        CHECK_FALSE(move1 == "53abc+e-c");
        CHECK_FALSE(move1 == "53abc+f-a");
        CHECK_FALSE(move1 == "53abc+f-b");
        CHECK_FALSE(move1 == "53abc+f-c");
        CHECK_FALSE(move1 == "53abc+g-a");
        CHECK_FALSE(move1 == "53abc+g-b");
        CHECK_FALSE(move1 == "53abc+g-c");
        CHECK_FALSE(move1 == "53abc+h-a");
        CHECK_FALSE(move1 == "53abc+h-b");
        CHECK_FALSE(move1 == "53abc+h-c");

        octi::OctiBoard b2(octi::GAME_MODE_2);
        b2.fromPositionString("2:3:0:0:P:A:-:0:1,0,10,0:0,0,11,0:A25abc,A44abfh,A53abc,B35defg,B37def");
        std::string move2 = bot.chooseMove(b2);
        // Verify chooseMove does not return any +...- rearrangement move on pods with < 4 prongs
        bool isRearrange = (move2.find('+') != std::string::npos &&
                            move2.find('-', move2.find('+')) != std::string::npos);
        if (isRearrange) {
            // If it is a rearrange, it can only be on A44abfh (4 prongs), never on A25abc or A53abc
            CHECK_FALSE(move2.rfind("25", 0) == 0);
            CHECK_FALSE(move2.rfind("53", 0) == 0);
        }
    }

    // C3 (#g-20261106-0165 Plies 33 & 35): Forward-only leapfrog battery, behind-home and zero-forward-prong penalties
    {
        octi::OctiBoard bStay(octi::GAME_MODE_2);
        bStay.fromPositionString("2:3:0:0:P:A:-:0:3,0,16,0:2,0,17,0:A33c,A44abfh,A53ab,A73ab,B37de,B46def,B55def,B66df");
        int scoreStay = octi::OctiBotEngine::evaluatePosition(bStay, octi::PLAYER_A);

        octi::OctiBoard bBack(octi::GAME_MODE_2);
        bBack.fromPositionString("2:3:0:0:P:B:-:0:3,0,16,0:2,0,17,0:A33c,A22abfh,A53ab,A73ab,B37de,B46def,B55def,B66df");
        int scoreBack = octi::OctiBotEngine::evaluatePosition(bBack, octi::PLAYER_A);
        CHECK_TRUE(scoreStay > scoreBack + 1500);

        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
        bot.setTimeBudgetMs(500);
        std::string chosenStay = bot.chooseMove(bStay);
        CHECK_FALSE(chosenStay == "44abfh-22");

        octi::OctiBoard bA2(octi::GAME_MODE_2);
        bA2.fromPositionString("2:3:0:0:P:A:-:0:3,0,16,0:2,0,16,0:A33c,A22abfh,A53ab,A73ab,B37de,B46def,B55def,B66dfg");
        std::string chosenA2 = bot.chooseMove(bA2);
        CHECK_FALSE(chosenA2 == "33c+f");

        octi::OctiBoard bFwdProng(octi::GAME_MODE_2);
        bFwdProng.fromPositionString("2:3:0:0:P:A:-:0:3,0,15,0:2,0,16,0:A33ac,A22abfh,A53ab,A73ab,B37de,B46def,B55def,B66dfg");
        int scoreFwdProng = octi::OctiBotEngine::evaluatePosition(bFwdProng, octi::PLAYER_A);

        octi::OctiBoard bBackProng(octi::GAME_MODE_2);
        bBackProng.fromPositionString("2:3:0:0:P:A:-:0:3,0,15,0:2,0,16,0:A33cf,A22abfh,A53ab,A73ab,B37de,B46def,B55def,B66dfg");
        int scoreBackProng = octi::OctiBotEngine::evaluatePosition(bBackProng, octi::PLAYER_A);
        CHECK_TRUE(scoreFwdProng > scoreBackProng + 1200);
    }

    // C4 (#g-20261106-0167 Ply 38): Non-base friendly pod stacking penalty
    {
        octi::OctiBoard bOrig(octi::GAME_MODE_2);
        bOrig.fromPositionString("2:3:0:0:P:B:-:0:2,0,12,0:1,0,15,0:A34abgh,A53abc,A65abgh,B37ef,B46defh,B57f,B66ef");
        int scoreOrig = octi::OctiBotEngine::evaluatePosition(bOrig, octi::PLAYER_B);

        octi::OctiBoard bStacked(octi::GAME_MODE_2);
        bStacked.fromPositionString("2:3:0:0:P:A:-:0:2,0,12,0:1,0,15,0:A34abgh,A53abc,A65abgh,B37ef,B46defh,B46f,B66ef");
        int scoreStacked = octi::OctiBotEngine::evaluatePosition(bStacked, octi::PLAYER_B);
        CHECK_TRUE(scoreOrig > scoreStacked + 2000);

        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
        bot.setTimeBudgetMs(500);
        std::string chosen = bot.chooseMove(bOrig);
        CHECK_FALSE(chosen == "57f-46");
    }

    // C8 (#g-20261106-0171 Plies 20 & 42): Allied home-base defense and enemy base capture valuation
    {
        octi::OctiBoard bPly20(octi::GAME_MODE_2);
        bPly20.fromPositionString("2:3:0:0:P:B:-:0:3,0,19,0:3,0,19,0:A33ab,A55ab,A73a,B37e,B57df,B77ef");
        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
        bot.setTimeBudgetMs(500);
        std::string chosenPly20 = bot.chooseMove(bPly20);
        CHECK_FALSE(chosenPly20 == "77ef-55");

        octi::OctiBoard bPly42(octi::GAME_MODE_2);
        bPly42.fromPositionString("2:3:0:0:P:B:-:0:1,2,14,0:2,0,13,0:A43abc,A65abgh,B32ae,B46def,B55defg");

        octi::OctiBoard bCap(octi::GAME_MODE_2);
        bCap.fromPositionString("2:3:0:0:P:A:-:0:1,2,14,0:1,0,13,0:A43abc,A65abgh,B33ae,B46def,B55defg");
        int scoreCap = octi::OctiBotEngine::evaluatePosition(bCap, octi::PLAYER_B);

        octi::OctiBoard bProng(octi::GAME_MODE_2);
        bProng.fromPositionString("2:3:0:0:P:A:-:0:1,2,14,0:2,0,12,0:A43abc,A65abgh,B32ade,B46def,B55defg");
        int scoreProng = octi::OctiBotEngine::evaluatePosition(bProng, octi::PLAYER_B);
        CHECK_TRUE(scoreCap > scoreProng + 7000);

        std::string chosenPly42 = bot.chooseMove(bPly42);
        CHECK_FALSE(chosenPly42 == "32ae+d");

        // Verify that when capturing base 33 is the winning move, chooseMove decisively selects 32ae-33
        octi::OctiBoard bWin(octi::GAME_MODE_2);
        bWin.fromPositionString("2:3:0:0:P:B:-:0:1,2,14,0:2,0,13,0:A43abc,B32ae,B53,B73");
        std::string chosenWin = bot.chooseMove(bWin);
        CHECK_TRUE(chosenWin == "32ae-33" || chosenWin.find("-33") != std::string::npos);
    }
}

TEST(OctiBotTacticalUpgradeTest, HomeBaseEvacuationUnderJumpThreatAndCenterSquareOccupationD1)
{
    // D1 (#g-20261106-0212 Ply 20 / #g-20261106-0208 Ply 22):
    // Player A has A44ab and A55 threatening 44ab-66-88x over Player B's pod 77def on base 77.
    // Player B also has armed pod B57de.
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.fromPositionString("2:3:0:0:P:B:-:0:3,0,18,0:4,0,18,0:A33a,A53,A44ab,A55,B57de,B77def");

    // 1. Verify chooseMove does NOT leave 77def sitting defenseless on 77 to be captured next turn.
    // Player B's only ways to save B77def are evacuating 77def (77def-...) or blocking 66 with 57de-66.
    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
    octi::BotTuningProfile prof =
        octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::TACTICAL);
    prof.maxSearchDepth = 5;
    prof.lazySmpThreads = 1;
    prof.rootScoreJitter = 0;
    prof.timeBudgetMs = 60000;
    bot.setTuningProfile(prof);
    std::string chosen = bot.chooseMove(board);

    CHECK_TRUE(chosen.rfind("77def-", 0) == 0 || chosen == "57de-66");

    // 2. Verify that moving 37cd off home base 37 (e.g. 37cd-46 or 37cd-47) is not hit with
    // the -14,000 home base abandonment penalty merely because an enemy pod sits on 55
    // at chebyshevDist == 2 with prong pointing North away from 37.
    octi::OctiBoard bIso37(octi::GAME_MODE_2);
    bIso37.fromPositionString("2:3:0:0:P:B:-:0:4,0,22,0:6,0,23,0:A33,A55a,B37cd");
    std::string chosenIso = bot.chooseMove(bIso37);
    CHECK_TRUE(chosenIso == "37cd-46" || chosenIso == "37cd-47");
}

TEST(OctiBotTacticalUpgradeTest, CapturedBaseRejectsThreePodStackingAndLiberationCampingD3)
{
    // D3 (#g-20261106-0200 Ply 102):
    // Player B has captured Player A's base 33 with 2 pods already on 33 (B33cdfg and B33)
    // and has captured pods available to liberate.
    octi::OctiBoard b2Stacked(octi::GAME_MODE_2);
    b2Stacked.fromPositionString("2:3:0:0:P:B:-:0:0,0,10,0:0,2,10,0:A53ab,A73ab,B33cdfg,B33,B57def");

    // Board with 3 pods stacked on captured base 33 (e.g. after liberating 33*)
    octi::OctiBoard b3Stacked(octi::GAME_MODE_2);
    b3Stacked.fromPositionString("2:3:0:0:P:A:-:0:0,0,10,0:0,1,10,0:A53ab,A73ab,B33cdfg,B33,B33,B57def");
    int score3Stacked = octi::OctiBotEngine::evaluatePosition(b3Stacked, octi::PLAYER_B);

    // Board where 1 pod holds the base (B33) and the 4-prong pod advances toward base 53 (B43cdfg)
    octi::OctiBoard b1Adv(octi::GAME_MODE_2);
    b1Adv.fromPositionString("2:3:0:0:P:A:-:0:0,0,10,0:0,2,10,0:A53ab,A73ab,B43cdfg,B33,B57def");
    int score1Adv = octi::OctiBotEngine::evaluatePosition(b1Adv, octi::PLAYER_B);

    // Stacking 3 pods on captured base must be heavily penalized relative to advancing
    CHECK_TRUE(score1Adv > score3Stacked + 4000);

    // chooseMove on b2Stacked must advance/arm active pods rather than liberating a 3rd pod (33*)
    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL);
    bot.setTimeBudgetMs(500);
    std::string chosen = bot.chooseMove(b2Stacked);
    CHECK_FALSE(chosen == "33*");
}

TEST(OctiBotTacticalUpgradeTest, FriendlyOccupiedStepMobility2PVsOfkE13)
{
    // E13 fix verification:
    // B55f has a single prong 'f' (SW) pointing to adjacent square 44 and jump square 33
    // (occupied by enemy A33, blocking the jump). Enemy A54a threatens 55 from adjacent
    // square 54 (while 56 has B56, blocking A54a's jump).
    // Compare placing a friendly 0-prong pod at 44 (B44) vs at 24 (B24, leaving 44 empty):
    // - In GAME_MODE_2, stepping onto friendly B44 is legal (canStep == true, legalMoveDirs == 1),
    //   so B55f is NOT immobilized when B44 occupies 44.
    // - In GAME_MODE_OFK, stepping onto friendly B44 is illegal (canStep == false, legalMoveDirs == 0),
    //   so B55f IS penalized as immobilized (-900 plus adjacent-threat immobilized vulnerability)
    //   when B44 occupies 44.
    {
        octi::OctiBoard b2pFriendlyAdj(octi::GAME_MODE_2);
        b2pFriendlyAdj.fromPositionString(
            "2:3:0:0:P:B:-:0:4,0,20,0:3,0,20,0:A33,B44,A54a,B55f,B56");
        int score2pFriendlyAdj =
            octi::OctiBotEngine::evaluatePosition(b2pFriendlyAdj, octi::PLAYER_B);

        octi::OctiBoard b2pEmptyAdj(octi::GAME_MODE_2);
        b2pEmptyAdj.fromPositionString(
            "2:3:0:0:P:B:-:0:4,0,20,0:3,0,20,0:A33,B24,A54a,B55f,B56");
        int score2pEmptyAdj =
            octi::OctiBotEngine::evaluatePosition(b2pEmptyAdj, octi::PLAYER_B);

        // In 2P, B55f remains mobile (canStep == true) even when friendly B44 occupies 44
        CHECK_TRUE(score2pEmptyAdj - score2pFriendlyAdj < 500);
    }

    {
        octi::OctiBoard bOfkFriendlyAdj(octi::GAME_MODE_OFK);
        bOfkFriendlyAdj.fromPositionString(
            "OFK:1:0:0:P:B:-:0:0,1,8,0:0,1,8,0:A22,A33,B44,A54a,B55f,B56");
        int scoreOfkFriendlyAdj =
            octi::OctiBotEngine::evaluatePosition(bOfkFriendlyAdj, octi::PLAYER_B);

        octi::OctiBoard bOfkEmptyAdj(octi::GAME_MODE_OFK);
        bOfkEmptyAdj.fromPositionString(
            "OFK:1:0:0:P:B:-:0:0,1,8,0:0,1,8,0:A22,A33,B24,A54a,B55f,B56");
        int scoreOfkEmptyAdj =
            octi::OctiBotEngine::evaluatePosition(bOfkEmptyAdj, octi::PLAYER_B);

        // In OFK, friendly B44 blocks B55f's step (canStep == false, legalMoveDirs == 0),
        // triggering the immobilized + adjacent-threat penalty (> 3000 pts)
        CHECK_TRUE(scoreOfkEmptyAdj - scoreOfkFriendlyAdj > 3000);
    }
}

TEST_GROUP(OctiCore_NetworkClient)
{
};

TEST(OctiCore_NetworkClient, NominalTokenAndLeaseMetadata)
{
    octi::OctiNetworkClient client;
    CHECK_EQUAL(std::string("desktop"), client.clientType());
    CHECK_FALSE(client.isRobotToken());
    CHECK_FALSE(client.isAuthenticated());

    client.setBotToken("octi_bot_test123");
    CHECK_TRUE(client.isRobotToken());
    CHECK_EQUAL(std::string("robot"), client.clientType());
    CHECK_TRUE(client.isAuthenticated());
    CHECK_EQUAL(std::string("octi_bot_test123"), client.sessionToken());

    client.setSessionToken("human_session_xyz");
    CHECK_FALSE(client.isRobotToken());
    CHECK_EQUAL(std::string("desktop"), client.clientType());
    CHECK_TRUE(client.isAuthenticated());
    CHECK_EQUAL(std::string("human_session_xyz"), client.sessionToken());

    client.setRobotLeaseId("lease_abc");
    CHECK_EQUAL(std::string("lease_abc"), client.robotLeaseId());
    client.clearRobotLeaseId();
    CHECK_EQUAL(std::string(""), client.robotLeaseId());

    client.setClientType("custom_client");
    CHECK_EQUAL(std::string("custom_client"), client.clientType());
}

TEST(OctiCore_NetworkClient, BoundarySseParsing)
{
    // Empty buffer
    std::string emptyBuf;
    std::vector<octi::SseEvent> events =
        octi::OctiNetworkClient::parseSseBuffer(emptyBuf);
    CHECK_EQUAL(0, events.size());
    CHECK_EQUAL(0, emptyBuf.length());

    // Partial frame across chunks
    std::string partialBuf = "id: 10\nevent: update\ndata: {\"key\":\"val";
    events = octi::OctiNetworkClient::parseSseBuffer(partialBuf);
    CHECK_EQUAL(0, events.size());
    CHECK_TRUE(partialBuf.length() > 0);

    partialBuf += "\"}\n\n";
    events = octi::OctiNetworkClient::parseSseBuffer(partialBuf);
    CHECK_EQUAL(1, events.size());
    CHECK_EQUAL(10, events[0].id);
    CHECK_EQUAL(std::string("update"), events[0].event);
    CHECK_EQUAL(std::string("{\"key\":\"val\"}"), events[0].dataJson);
    CHECK_EQUAL(0, partialBuf.length());

    // Multi-line data frame
    std::string multiLineBuf =
        "id: 20\nevent: msg\ndata: line 1\ndata: line 2\n\n";
    events = octi::OctiNetworkClient::parseSseBuffer(multiLineBuf);
    CHECK_EQUAL(1, events.size());
    CHECK_EQUAL(20, events[0].id);
    CHECK_EQUAL(std::string("msg"), events[0].event);
    CHECK_EQUAL(std::string("line 1\nline 2"), events[0].dataJson);
    CHECK_EQUAL(0, multiLineBuf.length());

    // \r\n vs \n line endings, and comment lines (: heartbeat)
    std::string mixedBuf =
        ": heartbeat\r\n: ping\r\nid: 30\r\nevent: chat\r\ndata: {\"msg\":\"hi\"}\r\n\r\n";
    events = octi::OctiNetworkClient::parseSseBuffer(mixedBuf);
    CHECK_EQUAL(1, events.size());
    CHECK_EQUAL(30, events[0].id);
    CHECK_EQUAL(std::string("chat"), events[0].event);
    CHECK_EQUAL(std::string("{\"msg\":\"hi\"}"), events[0].dataJson);
    CHECK_EQUAL(0, mixedBuf.length());
}

TEST(OctiCore_NetworkClient, FaultClosedLoopbackConnection)
{
    std::unique_ptr<octi::OctiNetworkClient> client =
        octi::OctiNetworkClient::createForLoopbackTest(1);
    CHECK_TRUE(client != nullptr);
    octi::ApiResult<octi::HealthInfo> res = client->fetchHealth();
    CHECK_FALSE(res.ok);
}

TEST(OctiCore_NetworkClient, SseWorkerSelfStopAndDestroyInCallbackN4)
{
    octi::CreateTableRequest createReq;
    CHECK_EQUAL(1, createReq.seat);
    createReq.seat = 2;
    CHECK_EQUAL(2, createReq.seat);

    httplib::Server svr;
    svr.Get("/api/v1/games/room-self-stop/events",
            [](const httplib::Request &, httplib::Response &res) {
                res.set_header("Cache-Control", "no-cache");
                res.set_content(
                    "id: 7\nevent: move\ndata: {\"seq\":7,\"move\":\"33+a\"}\n\n"
                    "id: 8\nevent: move\ndata: {\"seq\":8,\"move\":\"57+e\"}\n\n",
                    "text/event-stream");
            });

    int port = svr.bind_to_any_port("127.0.0.1");
    CHECK_TRUE(port > 0);
    std::thread serverThread([&svr]() { svr.listen_after_bind(); });

    std::mutex cbMutex;
    std::condition_variable cbCv;
    bool callbackCompleted = false;
    int callbackCount = 0;
    std::uint64_t seenId = 0;
    std::string seenEvent;

    std::unique_ptr<octi::OctiNetworkClient> client =
        octi::OctiNetworkClient::createForLoopbackTest(port);
    client->setBotToken("octi_bot_self_stop_test");
    client->setRobotLeaseId("lease_self_stop");

    std::shared_ptr<int> callbackLifetimeGuard = std::make_shared<int>(1);

    client->startGameEventStream(
        "room-self-stop", 0,
        [&client, &cbMutex, &cbCv, &callbackCompleted, &callbackCount,
         &seenId, &seenEvent, callbackLifetimeGuard](const octi::SseEvent &ev) {
            (void)callbackLifetimeGuard;
            int callIndex = ++callbackCount;
            if (callIndex == 1) {
                seenId = ev.id;
                seenEvent = ev.event;
                // Stop streams AND destroy the owning OctiNetworkClient
                // directly from inside onEvent on the SSE worker thread.
                client->stopEventStreams();
                client.reset();
                {
                    std::lock_guard<std::mutex> lock(cbMutex);
                    callbackCompleted = true;
                }
                cbCv.notify_all();
            }
        });

    {
        std::unique_lock<std::mutex> lock(cbMutex);
        bool ok = cbCv.wait_for(
            lock, std::chrono::seconds(5),
            [&callbackCompleted]() { return callbackCompleted; });
        CHECK_TRUE(ok);
    }

    // Wait for the detached worker thread to release the callback closure
    for (int i = 0; i < 200 && callbackLifetimeGuard.use_count() > 1; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    svr.stop();
    if (serverThread.joinable()) {
        serverThread.join();
    }

    CHECK_TRUE(client == nullptr);
    CHECK_EQUAL(1, callbackCount);
    CHECK_EQUAL(7, static_cast<int>(seenId));
    STRCMP_EQUAL("move", seenEvent.c_str());
}

TEST(OctiCore_NetworkClient, ArchiveRobotPlayerFieldsAndFilterOverload)
{
    std::mutex reqMutex;
    std::string lastMode;
    std::string lastPlayer;
    std::string lastRobot;
    bool hadRobotParam = false;

    httplib::Server svr;
    svr.Get("/api/v1/archive/games",
            [&](const httplib::Request &req, httplib::Response &res) {
                std::string modeVal = req.has_param("mode") ? req.get_param_value("mode") : "";
                std::string playerVal = req.has_param("player") ? req.get_param_value("player") : "";
                bool hasRobot = req.has_param("robot");
                std::string robotVal = hasRobot ? req.get_param_value("robot") : "";
                {
                    std::lock_guard<std::mutex> lock(reqMutex);
                    lastMode = modeVal;
                    lastPlayer = playerVal;
                    lastRobot = robotVal;
                    hadRobotParam = hasRobot;
                }

                if (robotVal == "bot-uuid-123" &&
                    (playerVal.empty() || playerVal == "human-uuid-456")) {
                    res.status = 200;
                    res.set_content(
                        "{"
                        "\"total\":1,"
                        "\"limit\":20,"
                        "\"offset\":0,"
                        "\"games\":[{"
                        "\"game_id\":\"g-robot-1\","
                        "\"mode\":\"2P\","
                        "\"bases\":3,"
                        "\"super_prongs\":false,"
                        "\"edgeless\":false,"
                        "\"time_control\":\"untimed\","
                        "\"players\":["
                        "{\"seat\":1,\"handle\":\"alice\",\"player_uuid\":\"human-uuid-456\"},"
                        "{\"seat\":2,\"handle\":\"stock\",\"is_robot\":true,\"robot_uuid\":\"bot-uuid-123\"}"
                        "],"
                        "\"result\":\"P2_WIN\","
                        "\"winner_seat\":2,"
                        "\"termination\":\"RESIGN\","
                        "\"ply_count\":12,"
                        "\"started_at\":1700000000,"
                        "\"ended_at\":1700000100"
                        "}]"
                        "}",
                        "application/json");
                    return;
                }

                res.status = 200;
                res.set_content(
                    "{\"total\":0,\"limit\":20,\"offset\":0,\"games\":[]}",
                    "application/json");
            });

    int port = svr.bind_to_any_port("127.0.0.1");
    CHECK_TRUE(port > 0);
    std::thread serverThread([&svr]() { svr.listen_after_bind(); });

    std::unique_ptr<octi::OctiNetworkClient> client =
        octi::OctiNetworkClient::createForLoopbackTest(port);
    CHECK_TRUE(client != nullptr);

    /* 1. Robot filter overload sends robot=<robot_uuid> and parses isRobot + robotUuid */
    octi::ApiResult<octi::ArchivedGamesPage> pageRes =
        client->listArchivedGames("2P", "human-uuid-456", "bot-uuid-123", 20, 0);

    /* 2. Existing 4-arg overload leaves robot parameter absent */
    octi::ApiResult<octi::ArchivedGamesPage> legacyRes =
        client->listArchivedGames("2P", "human-uuid-456", 10, 5);
    bool legacyHadRobot = true;
    {
        std::lock_guard<std::mutex> lock(reqMutex);
        legacyHadRobot = hadRobotParam;
    }

    svr.stop();
    if (serverThread.joinable()) {
        serverThread.join();
    }

    CHECK_TRUE(pageRes.ok);
    CHECK_EQUAL(1, static_cast<int>(pageRes.data.total));
    CHECK_EQUAL(1, static_cast<int>(pageRes.data.games.size()));
    STRCMP_EQUAL("g-robot-1", pageRes.data.games[0].gameId.c_str());
    CHECK_EQUAL(2, static_cast<int>(pageRes.data.games[0].players.size()));

    const octi::ArchivedGamePlayer &p1 = pageRes.data.games[0].players[0];
    CHECK_EQUAL(1, p1.seat);
    STRCMP_EQUAL("alice", p1.handle.c_str());
    STRCMP_EQUAL("human-uuid-456", p1.playerUuid.c_str());
    CHECK_FALSE(p1.isRobot);
    STRCMP_EQUAL("", p1.robotUuid.c_str());

    const octi::ArchivedGamePlayer &p2 = pageRes.data.games[0].players[1];
    CHECK_EQUAL(2, p2.seat);
    STRCMP_EQUAL("stock", p2.handle.c_str());
    STRCMP_EQUAL("", p2.playerUuid.c_str());
    CHECK_TRUE(p2.isRobot);
    STRCMP_EQUAL("bot-uuid-123", p2.robotUuid.c_str());

    CHECK_TRUE(legacyRes.ok);
    CHECK_FALSE(legacyHadRobot);
}

TEST(OctiCore_NetworkClient, KeepAliveClientPoolReusesSingleSocketAcrossSequentialRequests)
{
    struct LoopbackKeepAliveServer {
        int listenFd = -1;
        int boundPort = 0;
        std::atomic<bool> stopRequested{false};
        std::atomic<bool> closeAfterFirstResponse{false};
        std::atomic<int> acceptCount{0};
        std::atomic<int> requestCount{0};
        std::thread worker;

        explicit LoopbackKeepAliveServer(bool closeAfterFirst = false)
        {
            closeAfterFirstResponse.store(closeAfterFirst);
            listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
            if (listenFd < 0) {
                return;
            }
            int opt = 1;
            ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR,
                         reinterpret_cast<const char *>(&opt), sizeof(opt));

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = 0;
            if (::bind(listenFd, reinterpret_cast<sockaddr *>(&addr),
                       sizeof(addr)) < 0) {
                ::close(listenFd);
                listenFd = -1;
                return;
            }
            socklen_t addrLen = sizeof(addr);
            if (::getsockname(listenFd, reinterpret_cast<sockaddr *>(&addr),
                              &addrLen) < 0) {
                ::close(listenFd);
                listenFd = -1;
                return;
            }
            boundPort = ntohs(addr.sin_port);
            if (::listen(listenFd, 16) < 0) {
                ::close(listenFd);
                listenFd = -1;
                return;
            }

            worker = std::thread([this]() { runLoop(); });
        }

        ~LoopbackKeepAliveServer()
        {
            stop();
        }

        void stop()
        {
            stopRequested.store(true);
            if (listenFd >= 0) {
                ::shutdown(listenFd, SHUT_RDWR);
            }
            if (worker.joinable()) {
                worker.join();
            }
            if (listenFd >= 0) {
                ::close(listenFd);
                listenFd = -1;
            }
        }

        void runLoop()
        {
            static const char kResponse[] =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 15\r\n"
                "Connection: keep-alive\r\n"
                "\r\n"
                "{\"status\":\"ok\"}";
            const std::size_t kResponseLen = sizeof(kResponse) - 1;

            while (!stopRequested.load()) {
                pollfd pfd{};
                pfd.fd = listenFd;
                pfd.events = POLLIN;
                int pr = ::poll(&pfd, 1, 25);
                if (pr <= 0 || !(pfd.revents & POLLIN)) {
                    continue;
                }
                sockaddr_in cliAddr{};
                socklen_t cliLen = sizeof(cliAddr);
                int clientFd = ::accept(
                    listenFd, reinterpret_cast<sockaddr *>(&cliAddr), &cliLen);
                if (clientFd < 0) {
                    continue;
                }
                acceptCount.fetch_add(1);

                std::string reqBuf;
                while (!stopRequested.load()) {
                    pollfd cpfd{};
                    cpfd.fd = clientFd;
                    cpfd.events = POLLIN;
                    int cpr = ::poll(&cpfd, 1, 25);
                    if (cpr < 0) {
                        break;
                    }
                    if (cpr == 0) {
                        continue;
                    }
                    char buf[1024];
                    ssize_t n = ::recv(clientFd, buf, sizeof(buf), 0);
                    if (n <= 0) {
                        break;
                    }
                    reqBuf.append(buf, static_cast<std::size_t>(n));
                    std::size_t headerEnd = reqBuf.find("\r\n\r\n");
                    if (headerEnd == std::string::npos) {
                        continue;
                    }
                    reqBuf.erase(0, headerEnd + 4);
                    requestCount.fetch_add(1);

                    std::size_t sentTotal = 0;
                    bool sendOk = true;
                    while (sentTotal < kResponseLen) {
                        ssize_t s = ::send(
                            clientFd, kResponse + sentTotal,
                            kResponseLen - sentTotal, 0);
                        if (s <= 0) {
                            sendOk = false;
                            break;
                        }
                        sentTotal += static_cast<std::size_t>(s);
                    }
                    if (!sendOk || closeAfterFirstResponse.load()) {
                        ::shutdown(clientFd, SHUT_WR);
                        break;
                    }
                }
                ::close(clientFd);
            }
        }
    };

    /* 1. Nominal socket reuse: 10 sequential fetchHealth() calls reuse 1 socket */
    LoopbackKeepAliveServer server1(false);
    CHECK_TRUE(server1.boundPort > 0);

    std::unique_ptr<octi::OctiNetworkClient> client =
        octi::OctiNetworkClient::createForLoopbackTest(server1.boundPort);
    CHECK_TRUE(client != nullptr);

    for (int i = 0; i < 10; ++i) {
        octi::ApiResult<octi::HealthInfo> res = client->fetchHealth();
        CHECK_TRUE(res.ok);
        CHECK_EQUAL(200, res.httpStatus);
        STRCMP_EQUAL("ok", res.data.status.c_str());
    }
    CHECK_EQUAL(1, server1.acceptCount.load());
    CHECK_EQUAL(10, server1.requestCount.load());

    /* 2. Boundary: setEndpoint() invalidates pooled client and connects to new port */
    LoopbackKeepAliveServer server2(false);
    CHECK_TRUE(server2.boundPort > 0);
    CHECK_TRUE(server2.boundPort != server1.boundPort);

    client->setEndpoint("127.0.0.1", server2.boundPort);
    octi::ApiResult<octi::HealthInfo> switchRes = client->fetchHealth();
    CHECK_TRUE(switchRes.ok);
    CHECK_EQUAL(200, switchRes.httpStatus);
    STRCMP_EQUAL("ok", switchRes.data.status.c_str());
    CHECK_EQUAL(1, server2.acceptCount.load());
    CHECK_EQUAL(1, server2.requestCount.load());
    CHECK_EQUAL(1, server1.acceptCount.load());

    /* 3. Fault injection: server closes keep-alive socket mid-idle after 1st response */
    LoopbackKeepAliveServer server3(true);
    CHECK_TRUE(server3.boundPort > 0);

    client->setEndpoint("127.0.0.1", server3.boundPort);
    octi::ApiResult<octi::HealthInfo> faultFirst = client->fetchHealth();
    CHECK_TRUE(faultFirst.ok);
    CHECK_EQUAL(200, faultFirst.httpStatus);
    STRCMP_EQUAL("ok", faultFirst.data.status.c_str());
    CHECK_EQUAL(1, server3.acceptCount.load());

    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    octi::ApiResult<octi::HealthInfo> faultSecond = client->fetchHealth();
    CHECK_TRUE(faultSecond.ok);
    CHECK_EQUAL(200, faultSecond.httpStatus);
    STRCMP_EQUAL("ok", faultSecond.data.status.c_str());
    CHECK_EQUAL(2, server3.acceptCount.load());
    CHECK_EQUAL(2, server3.requestCount.load());
}

TEST(OctiCore_NetworkClient, ObserverHandlesWireFormatInGameViewAndTables)
{
    httplib::Server svr;
    svr.Get("/api/v1/games/g-obs-wire-1",
            [](const httplib::Request &, httplib::Response &res) {
                const nlohmann::json body = {
                    {"ok", true},
                    {"live", true},
                    {"room_id", "g-obs-wire-1"},
                    {"mode", "2P"},
                    {"bases", 3},
                    {"phase", "WAITING_FOR_PLAYERS"},
                    {"observers", {"f03d0863-1111-2222-3333-444444444444", ""}},
                    {"observer_handles", {"player_one", "anonymous-127.0.0.1"}},
                    {"observer_count", 2}
                };
                res.set_content(body.dump(), "application/json");
            });
    svr.Get("/api/v1/tables",
            [](const httplib::Request &, httplib::Response &res) {
                const nlohmann::json body = {
                    {"ok", true},
                    {"tables", {{
                        {"room_id", "g-obs-wire-1"},
                        {"mode", "2P"},
                        {"bases", 3},
                        {"phase", "WAITING_FOR_PLAYERS"},
                        {"observer_handles", {"player_one", "anonymous-127.0.0.1"}},
                        {"observer_count", 2}
                    }}}
                };
                res.set_content(body.dump(), "application/json");
            });

    int port = svr.bind_to_any_port("127.0.0.1");
    CHECK_TRUE(port > 0);
    std::thread serverThread([&svr]() { svr.listen_after_bind(); });

    std::unique_ptr<octi::OctiNetworkClient> client =
        octi::OctiNetworkClient::createForLoopbackTest(port);
    CHECK_TRUE(client != nullptr);

    octi::ApiResult<octi::GameViewData> gv = client->fetchGameView("g-obs-wire-1");
    CHECK_TRUE(gv.ok);
    CHECK_EQUAL(200, gv.httpStatus);
    CHECK_EQUAL(2, gv.data.observerCount);
    CHECK_EQUAL(static_cast<std::size_t>(2), gv.data.observers.size());
    CHECK_EQUAL(static_cast<std::size_t>(2), gv.data.observerHandles.size());
    STRCMP_EQUAL("f03d0863-1111-2222-3333-444444444444", gv.data.observers[0].c_str());
    STRCMP_EQUAL("", gv.data.observers[1].c_str());
    STRCMP_EQUAL("player_one", gv.data.observerHandles[0].c_str());
    STRCMP_EQUAL("anonymous-127.0.0.1", gv.data.observerHandles[1].c_str());

    octi::ApiResult<std::vector<octi::TableSummaryInfo>> tb = client->listTables();
    CHECK_TRUE(tb.ok);
    CHECK_EQUAL(static_cast<std::size_t>(1), tb.data.size());
    CHECK_EQUAL(2, tb.data[0].observerCount);
    CHECK_EQUAL(static_cast<std::size_t>(2), tb.data[0].observerHandles.size());
    STRCMP_EQUAL("player_one", tb.data[0].observerHandles[0].c_str());
    STRCMP_EQUAL("anonymous-127.0.0.1", tb.data[0].observerHandles[1].c_str());

    svr.stop();
    if (serverThread.joinable()) {
        serverThread.join();
    }
}

// ---------------------------------------------------------------------------
// Self-Capture Rules: OFK Double Jump, 2P Superprong & AI Leapfrog Preservation
// ---------------------------------------------------------------------------

TEST_GROUP(OctiSelfCaptureRules)
{
};

TEST(OctiSelfCaptureRules, OFKDoubleJumpSelfAndEnemyCapture)
{
    octi::OctiBoard board(octi::GAME_MODE_OFK);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);
    board.clear();

    // Player A mover at 33 (x=2, y=2) with N (a) and E (c) prongs
    octi::OctiPod mover(octi::A_POD);
    mover.addProng(octi::DIR_N);
    mover.addProng(octi::DIR_E);
    board.squareAt(2, 2).addPod(mover);

    // Friendly Player A pod at 34 (x=2, y=3) with 2 prongs (a and e)
    octi::OctiPod friendlyPod(octi::A_POD);
    friendlyPod.addProng(octi::DIR_N);
    friendlyPod.addProng(octi::DIR_S);
    board.squareAt(2, 3).addPod(friendlyPod);

    // Enemy Player B pod at 45 (x=3, y=4) with 2 prongs (c and g)
    octi::OctiPod enemyPod(octi::B_POD);
    enemyPod.addProng(octi::DIR_E);
    enemyPod.addProng(octi::DIR_W);
    board.squareAt(3, 4).addPod(enemyPod);

    int freeProngsBefore = game->getFreeProngs(octi::PLAYER_A);
    int capturedPodsABefore = game->getCapturedPods(octi::PLAYER_A);
    int capturedPodsBBefore = game->getCapturedPods(octi::PLAYER_B);

    // Verify 33ac-35x-55x is in generateLegalMoves()
    std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
    bool foundDoubleCapture = false;
    for (const auto &m : legalMoves) {
        if (m.asString() == "33ac-35x-55x") {
            foundDoubleCapture = true;
            break;
        }
    }
    CHECK_TRUE_TEXT(foundDoubleCapture, "generateLegalMoves() should include 33ac-35x-55x");

    // Execute move
    game->move("33ac-35x-55x");

    // Verify both jumped pods were captured and removed
    CHECK_EQUAL(0, board.squareAt(2, 2).getNumPods()); // vacated origin
    CHECK_EQUAL(0, board.squareAt(2, 3).getNumPods()); // friendly captured
    CHECK_EQUAL(0, board.squareAt(3, 4).getNumPods()); // enemy captured
    CHECK_EQUAL(1, board.squareAt(4, 4).getNumPods()); // final landing 55
    CHECK_EQUAL(capturedPodsABefore + 1, game->getCapturedPods(octi::PLAYER_A));
    CHECK_EQUAL(capturedPodsBBefore + 1, game->getCapturedPods(octi::PLAYER_B));
    CHECK_EQUAL(freeProngsBefore + 4, game->getFreeProngs(octi::PLAYER_A)); // 2 friendly + 2 enemy

    // Verify undo restores exact state
    game->undo();
    CHECK_EQUAL(1, board.squareAt(2, 2).getNumPods());
    CHECK_EQUAL(1, board.squareAt(2, 3).getNumPods());
    CHECK_EQUAL(1, board.squareAt(3, 4).getNumPods());
    CHECK_EQUAL(0, board.squareAt(4, 4).getNumPods());
    CHECK_EQUAL(capturedPodsABefore, game->getCapturedPods(octi::PLAYER_A));
    CHECK_EQUAL(capturedPodsBBefore, game->getCapturedPods(octi::PLAYER_B));
    CHECK_EQUAL(freeProngsBefore, game->getFreeProngs(octi::PLAYER_A));
}

TEST(OctiSelfCaptureRules, Standard2PSelfCaptureWithSuperprong)
{
    octi::OctiBoard board(octi::GAME_MODE_2, true); // useSuperProngs = true
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);
    board.clear();

    // Player A mover at 33 (x=2, y=2) with N (a) prong
    octi::OctiPod mover(octi::A_POD);
    mover.addProng(octi::DIR_N);
    board.squareAt(2, 2).addPod(mover);

    // Friendly Player A pod at 34 (x=2, y=3) with 1 normal prong and 1 superprong
    octi::OctiPod friendlyPod(octi::A_POD);
    friendlyPod.addProng(octi::DIR_E);
    friendlyPod.addProng(octi::DIR_N, true); // superprong!
    board.squareAt(2, 3).addPod(friendlyPod);

    int freeProngsBefore = game->getFreeProngs(octi::PLAYER_A);
    int freeSuperBefore = game->getFreeSuperProngs(octi::PLAYER_A);
    int capturedBefore = game->getCapturedPods(octi::PLAYER_A);

    // Verify move is legal
    game->move("33a-35x");

    CHECK_EQUAL(0, board.squareAt(2, 3).getNumPods());
    CHECK_EQUAL(1, board.squareAt(2, 4).getNumPods());
    CHECK_EQUAL(capturedBefore + 1, game->getCapturedPods(octi::PLAYER_A));
    CHECK_EQUAL(freeProngsBefore + 1, game->getFreeProngs(octi::PLAYER_A));
    CHECK_EQUAL(freeSuperBefore + 1, game->getFreeSuperProngs(octi::PLAYER_A));

    // Verify undo
    game->undo();
    CHECK_EQUAL(1, board.squareAt(2, 2).getNumPods());
    CHECK_EQUAL(1, board.squareAt(2, 3).getNumPods());
    CHECK_TRUE(board.squareAt(2, 3).getPodAt(0).hasSuperProng());
    CHECK_EQUAL(capturedBefore, game->getCapturedPods(octi::PLAYER_A));
    CHECK_EQUAL(freeProngsBefore, game->getFreeProngs(octi::PLAYER_A));
    CHECK_EQUAL(freeSuperBefore, game->getFreeSuperProngs(octi::PLAYER_A));
}

TEST(OctiSelfCaptureRules, StepCaptureFaultInjection)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);
    board.clear();

    octi::OctiPod mover(octi::A_POD);
    mover.addProng(octi::DIR_N);
    board.squareAt(2, 2).addPod(mover);

    // Distance-1 step with capture flag must be rejected
    CHECK_THROWS(octi::IllegalMoveException, game->move("33a-34x"));
}

TEST(OctiSelfCaptureRules, BotPreservesFriendlyLeapfrog)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);
    board.clear();

    // Friendly mover at 33 with N prong
    octi::OctiPod mover(octi::A_POD);
    mover.addProng(octi::DIR_N);
    board.squareAt(2, 2).addPod(mover);

    // Friendly 0-prong pod at 34
    octi::OctiPod friendlyStationary(octi::A_POD);
    board.squareAt(2, 3).addPod(friendlyStationary);

    // Player A has free prongs in reserve
    CHECK_TRUE(game->getFreeProngs(octi::PLAYER_A) > 0);

    // filterDominatedMoves should keep 33a-35 (leapfrog) and prune 33a-35x (pointless self-capture of 0-prong pod)
    std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
    std::vector<octi::OctiMove> filtered = octi::filterDominatedMoves(board, octi::PLAYER_A, legalMoves);

    bool hasNonCap = false;
    bool hasCap = false;
    for (const auto &m : filtered) {
        if (m.asString() == "33a-35") hasNonCap = true;
        if (m.asString() == "33a-35x") hasCap = true;
    }
    CHECK_TRUE_TEXT(hasNonCap, "filterDominatedMoves must retain non-capturing leapfrog 33a-35");
    CHECK_FALSE_TEXT(hasCap, "filterDominatedMoves must prune pointless self-capture 33a-35x of 0-prong pod when freeProngs > 0");

    // Also verify chooseMove does not choose self-capture
    octi::OctiBotEngine bot(octi::BotDifficulty::CASUAL);
    std::string chosen = bot.chooseMove(board);
    CHECK_TRUE_TEXT(chosen.find('x') == std::string::npos, "Bot should not choose self-capture when leapfrog is available");
}

// ============================================================================
// Group 32: 4-Player Per-Seat Prong-Direction Symmetry & 2P/OFK Invariance
// ============================================================================

namespace {

using namespace octi;

OctiDir rotateDir180ForTest(OctiDir d)
{
    return static_cast<OctiDir>((static_cast<int>(d) + 4) % 8);
}

OctiDir reflectDirDiagForTest(OctiDir d)
{
    switch (d) {
    case DIR_N:  return DIR_E;
    case DIR_NE: return DIR_NE;
    case DIR_E:  return DIR_N;
    case DIR_SE: return DIR_NW;
    case DIR_S:  return DIR_W;
    case DIR_SW: return DIR_SW;
    case DIR_W:  return DIR_S;
    case DIR_NW: return DIR_SE;
    default:     return DIR_UNDEF;
    }
}

OctiPlayer rotatePlayer180ForTest(OctiPlayer p)
{
    switch (p) {
    case PLAYER_A: return PLAYER_C;
    case PLAYER_B: return PLAYER_D;
    case PLAYER_C: return PLAYER_A;
    case PLAYER_D: return PLAYER_B;
    default:       return PLAYER_UNDEF;
    }
}

OctiPlayer reflectPlayerDiagForTest(OctiPlayer p)
{
    switch (p) {
    case PLAYER_A: return PLAYER_A;
    case PLAYER_B: return PLAYER_D;
    case PLAYER_C: return PLAYER_C;
    case PLAYER_D: return PLAYER_B;
    default:       return PLAYER_UNDEF;
    }
}

OctiPodType playerToPodForTest(OctiPlayer p)
{
    switch (p) {
    case PLAYER_A: return A_POD;
    case PLAYER_B: return B_POD;
    case PLAYER_C: return C_POD;
    case PLAYER_D: return D_POD;
    default:       return POD_UNDEF;
    }
}

OctiPlayer podToPlayerForTest(OctiPodType t)
{
    switch (t) {
    case A_POD: return PLAYER_A;
    case B_POD: return PLAYER_B;
    case C_POD: return PLAYER_C;
    case D_POD: return PLAYER_D;
    default:    return PLAYER_UNDEF;
    }
}

void cloneTransformed4PForTest(const OctiBoard &src,
                               OctiBoard &dstBoard,
                               bool diagReflect)
{
    dstBoard.setGameMode(GAME_MODE_4, src.getGame()->useSuperProngs(),
                         src.getGame()->useEdgelessBoard(), src.getGame()->getBases());
    OctiGame *dstGame = dstBoard.getGame();
    dstGame->reset();
    dstGame->start(PLAYER_A);

    OctiGameSnapshot snap = dstGame->captureSnapshot();
    const int w = src.getBoardWidth();
    const int h = src.getBoardHeight();
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            snap.squares[x][y].clear();
        }
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const OctiSquare &sq = src.squareAt(x, y);
            const int tx = diagReflect ? y : (w - 1 - x);
            const int ty = diagReflect ? x : (h - 1 - y);
            for (int i = 0; i < sq.getNumPods(); i++) {
                const OctiPod &pod = sq.getPodAt(i);
                OctiPlayer origOwner = podToPlayerForTest(pod.getType());
                OctiPlayer newOwner = diagReflect ? reflectPlayerDiagForTest(origOwner)
                                                  : rotatePlayer180ForTest(origOwner);
                OctiPod newPod(playerToPodForTest(newOwner));
                for (int d = 0; d < DIR_MAX; d++) {
                    OctiDir origDir = static_cast<OctiDir>(d);
                    OctiProngType pt = pod.prongAt(origDir);
                    if (pt != NO_PRONG) {
                        OctiDir newDir = diagReflect ? reflectDirDiagForTest(origDir)
                                                     : rotateDir180ForTest(origDir);
                        newPod.addProng(newDir, pt == SUPER_PRONG);
                    }
                }
                snap.squares[tx][ty].push_back(newPod);
            }
        }
    }

    const OctiGame *srcGame = src.getGame();
    const OctiPlayer seats[4] = { PLAYER_A, PLAYER_B, PLAYER_C, PLAYER_D };
    for (int i = 0; i < 4; i++) {
        OctiPlayer s = seats[i];
        OctiPlayer ts = diagReflect ? reflectPlayerDiagForTest(s) : rotatePlayer180ForTest(s);
        snap.players[ts].freePods = srcGame->getFreePods(s);
        snap.players[ts].capturedPods = srcGame->getCapturedPods(s);
        snap.players[ts].freeProngs = srcGame->getFreeProngs(s);
        snap.players[ts].freeSuperProngs = srcGame->getFreeSuperProngs(s);
        snap.players[ts].eliminated = srcGame->isPlayerEliminated(s);
    }

    snap.turn = diagReflect ? reflectPlayerDiagForTest(srcGame->getTurn())
                            : rotatePlayer180ForTest(srcGame->getTurn());
    snap.gameStarted = true;
    snap.gameOver = false;
    snap.gameResigned = false;
    dstGame->restoreSnapshot(snap);
}

BotTuningProfile deterministicDepth2ProfileForTest()
{
    BotTuningProfile p = OctiBotEngine::defaultProfileForDifficulty(BotDifficulty::TACTICAL);
    p.useOpeningBook = false;
    p.lazySmpThreads = 1;
    p.rootScoreJitter = 0;
    p.maxSearchDepth = 2;
    p.timeBudgetMs = 60000;
    return p;
}

void makeSinglePod4PForTest(OctiBoard &b, int x, int y, OctiPlayer owner, OctiDir dir)
{
    b.setGameMode(GAME_MODE_4, false, false, GAME_BASES_3);
    OctiGame *g = b.getGame();
    g->reset();
    g->start(PLAYER_A);
    OctiGameSnapshot snap = g->captureSnapshot();
    for (int yy = 0; yy < b.getBoardHeight(); yy++) {
        for (int xx = 0; xx < b.getBoardWidth(); xx++) {
            snap.squares[xx][yy].clear();
        }
    }
    OctiPod pod(playerToPodForTest(owner));
    pod.addProng(dir, false);
    snap.squares[x][y].push_back(pod);
    snap.turn = owner;
    snap.gameStarted = true;
    snap.gameOver = false;
    g->restoreSnapshot(snap);
}

} // namespace

TEST_GROUP(Octi4PlayerSeatSymmetry)
{
};

TEST(Octi4PlayerSeatSymmetry, TwoPlayerAndOfkInvarianceGuard)
{
    using namespace octi;

    struct PinnedFixture {
        const char *pos;
        int evalA;
        int evalB;
        const char *bestMove;
    };

    const PinnedFixture fixtures[] = {
        {
            "2:3:0:0:P:A:-:0:4,0,18,0:4,0,18,0:A33abh,A53ab,A64ah,B46def,B37de,B77ef",
            -3414, 3414, "73"
        },
        {
            "2:3:0:0:P:B:-:0:3,0,23,0:3,2,13,0:A44abh,A64ah,A55ah,B66efgh,B57,A38abg",
            29509, -29509, "57+d"
        },
        {
            "2:3:0:0:P:A:-:0:4,0,17,0:4,0,16,0:A53abh,A73af,B55defg,A56abh,B66def,B37de",
            -3701, 3701, "56abh+e"
        },
        {
            "OFK:1:0:0:P:A:-:0:0,0,9,0:0,0,7,0:A22,A32,A42,A65abh,B26d,B46cf,B56,B47be",
            -9532, 9532, "22+a"
        },
        {
            "OFK:1:0:0:P:B:-:0:0,0,8,0:0,0,6,0:A22,A32,A42,A64abeh,B26d,B46cef,B56,B47be",
            -14827, 14827, "47be+d"
        }
    };

    BotTuningProfile prof = deterministicDepth2ProfileForTest();
    for (std::size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
        OctiBoard board;
        board.fromPositionString(fixtures[i].pos);
        LONGS_EQUAL(fixtures[i].evalA, OctiBotEngine::evaluatePosition(board, PLAYER_A, prof));
        LONGS_EQUAL(fixtures[i].evalB, OctiBotEngine::evaluatePosition(board, PLAYER_B, prof));

        OctiBotEngine bot(BotDifficulty::TACTICAL, 0x5EED2026ULL);
        bot.setTuningProfile(prof);
        STRCMP_EQUAL(fixtures[i].bestMove, bot.chooseMove(board).c_str());
    }
}

TEST(Octi4PlayerSeatSymmetry, FourPlayerDirectionBonusBySeat)
{
    using namespace octi;

    BotTuningProfile prof = deterministicDepth2ProfileForTest();

    OctiBoard bCS, bCN, bCW, bCE, bCSW, bCNE;
    makeSinglePod4PForTest(bCS, 6, 6, PLAYER_C, DIR_S);
    makeSinglePod4PForTest(bCN, 6, 6, PLAYER_C, DIR_N);
    makeSinglePod4PForTest(bCW, 6, 6, PLAYER_C, DIR_W);
    makeSinglePod4PForTest(bCE, 6, 6, PLAYER_C, DIR_E);
    makeSinglePod4PForTest(bCSW, 6, 6, PLAYER_C, DIR_SW);
    makeSinglePod4PForTest(bCNE, 6, 6, PLAYER_C, DIR_NE);

    CHECK_TRUE(OctiBotEngine::evaluatePosition(bCS, PLAYER_C, prof) >
               OctiBotEngine::evaluatePosition(bCN, PLAYER_C, prof));
    CHECK_TRUE(OctiBotEngine::evaluatePosition(bCW, PLAYER_C, prof) >
               OctiBotEngine::evaluatePosition(bCE, PLAYER_C, prof));
    CHECK_TRUE(OctiBotEngine::evaluatePosition(bCSW, PLAYER_C, prof) >
               OctiBotEngine::evaluatePosition(bCNE, PLAYER_C, prof));

    OctiBoard bDN, bDS, bDW, bDE, bDNW, bDSE;
    makeSinglePod4PForTest(bDN, 6, 2, PLAYER_D, DIR_N);
    makeSinglePod4PForTest(bDS, 6, 2, PLAYER_D, DIR_S);
    makeSinglePod4PForTest(bDW, 6, 2, PLAYER_D, DIR_W);
    makeSinglePod4PForTest(bDE, 6, 2, PLAYER_D, DIR_E);
    makeSinglePod4PForTest(bDNW, 6, 2, PLAYER_D, DIR_NW);
    makeSinglePod4PForTest(bDSE, 6, 2, PLAYER_D, DIR_SE);

    CHECK_TRUE(OctiBotEngine::evaluatePosition(bDN, PLAYER_D, prof) >
               OctiBotEngine::evaluatePosition(bDS, PLAYER_D, prof));
    CHECK_TRUE(OctiBotEngine::evaluatePosition(bDW, PLAYER_D, prof) >
               OctiBotEngine::evaluatePosition(bDE, PLAYER_D, prof));
    CHECK_TRUE(OctiBotEngine::evaluatePosition(bDNW, PLAYER_D, prof) >
               OctiBotEngine::evaluatePosition(bDSE, PLAYER_D, prof));
}

TEST(Octi4PlayerSeatSymmetry, FourPlayerSeatEvaluationSymmetry)
{
    using namespace octi;

    BotTuningProfile prof = deterministicDepth2ProfileForTest();

    const char *seq0[] = { "33+a", "37+e", "77+e", "73+c", "33a+b" };
    const char *seq1[] = { "33+c", "37+d", "77+f", "73+a", "33c-43", "37d+e", "77f+g" };
    const char *seq2[] = { "33+a", "37+c", "77+e", "73+g", "33a-34", "37c-47", "77e-76", "73g+a", "34a+b" };
    const std::vector<std::vector<const char *>> seqs = {
        std::vector<const char *>(seq0, seq0 + 5),
        std::vector<const char *>(seq1, seq1 + 7),
        std::vector<const char *>(seq2, seq2 + 9)
    };

    const OctiPlayer seats[4] = { PLAYER_A, PLAYER_B, PLAYER_C, PLAYER_D };
    for (std::size_t idx = 0; idx < seqs.size(); idx++) {
        OctiBoard bOrig(GAME_MODE_4, false, false, GAME_BASES_3);
        OctiBoard bRot(GAME_MODE_4, false, false, GAME_BASES_3);
        OctiBoard bRef(GAME_MODE_4, false, false, GAME_BASES_3);
        bOrig.getGame()->start(PLAYER_A);
        for (const char *m : seqs[idx]) {
            bOrig.getGame()->move(m);
        }
        cloneTransformed4PForTest(bOrig, bRot, false);
        cloneTransformed4PForTest(bOrig, bRef, true);

        for (int i = 0; i < 4; i++) {
            OctiPlayer p = seats[i];
            OctiPlayer pRot = rotatePlayer180ForTest(p);
            OctiPlayer pRef = reflectPlayerDiagForTest(p);
            int evalOrig = OctiBotEngine::evaluatePosition(bOrig, p, prof);
            int evalRot = OctiBotEngine::evaluatePosition(bRot, pRot, prof);
            int evalRef = OctiBotEngine::evaluatePosition(bRef, pRef, prof);
            LONGS_EQUAL(evalOrig, evalRot);
            LONGS_EQUAL(evalOrig, evalRef);
        }
    }
}

TEST(Octi4PlayerSeatSymmetry, FourPlayerSearchSymmetry)
{
    using namespace octi;

    BotTuningProfile prof = deterministicDepth2ProfileForTest();

    const char *seq0[] = { "33+a", "37+e", "77+e", "73+c", "33a+b" };
    const char *seq1[] = { "33+c", "37+d", "77+f", "73+a", "33c-43", "37d+e", "77f+g" };
    const char *seq2[] = { "33+a", "37+c", "77+e", "73+g", "33a-34", "37c-47", "77e-76", "73g+a", "34a+b" };
    const std::vector<std::vector<const char *>> seqs = {
        std::vector<const char *>(seq0, seq0 + 5),
        std::vector<const char *>(seq1, seq1 + 7),
        std::vector<const char *>(seq2, seq2 + 9)
    };

    for (std::size_t idx = 0; idx < seqs.size(); idx++) {
        OctiBoard bOrig(GAME_MODE_4, false, false, GAME_BASES_3);
        OctiBoard bRot(GAME_MODE_4, false, false, GAME_BASES_3);
        OctiBoard bOrigAfterRot(GAME_MODE_4, false, false, GAME_BASES_3);
        bOrig.getGame()->start(PLAYER_A);
        for (const char *m : seqs[idx]) {
            bOrig.getGame()->move(m);
        }
        cloneTransformed4PForTest(bOrig, bRot, false);

        OctiBotEngine botOrig(BotDifficulty::TACTICAL, 0x5EED2026ULL);
        OctiBotEngine botRot(BotDifficulty::TACTICAL, 0x5EED2026ULL);
        botOrig.setTuningProfile(prof);
        botRot.setTuningProfile(prof);

        std::string mvOrig = botOrig.chooseMove(bOrig);
        std::string mvRot = botRot.chooseMove(bRot);
        CHECK_FALSE(mvOrig.empty());
        CHECK_FALSE(mvRot.empty());

        bOrig.getGame()->move(mvOrig);
        bRot.getGame()->move(mvRot);
        cloneTransformed4PForTest(bOrig, bOrigAfterRot, false);
        STRCMP_EQUAL(bOrigAfterRot.toPositionString().c_str(),
                     bRot.toPositionString().c_str());
    }
}

// ---------------------------------------------------------------------------
// Envelope 1 (all-variants plan): Superprong Bonus-Move Generation
// ---------------------------------------------------------------------------

namespace {

bool moveHasPrelude(const octi::OctiMove &m)
{
    for (const auto &op : m.getOps()) {
        if (op.isSuperProngBonusMove) {
            return true;
        }
    }
    return false;
}

int countPreludeOps(const octi::OctiMove &m)
{
    int count = 0;
    for (const auto &op : m.getOps()) {
        if (op.isSuperProngBonusMove) {
            count++;
        }
    }
    return count;
}

bool containsMoveWithPrefix(const std::vector<octi::OctiMove> &moves,
                            const std::string &prefix)
{
    for (const auto &m : moves) {
        const std::string s = m.asString();
        if (s.compare(0, prefix.size(), prefix) == 0) {
            return true;
        }
    }
    return false;
}

bool containsExactMove(const std::vector<octi::OctiMove> &moves,
                       const std::string &target)
{
    for (const auto &m : moves) {
        if (m.asString() == target) {
            return true;
        }
    }
    return false;
}

std::string canonicalBoardStateForTest(const octi::OctiBoard &board)
{
    const octi::OctiGame *game = board.getGame();
    std::ostringstream ss;
    ss << static_cast<int>(board.getGameMode()) << ":"
       << game->getBasesToWin() << ":"
       << (game->useSuperProngs() ? 1 : 0) << ":"
       << (game->useEdgelessBoard() ? 1 : 0) << ":"
       << static_cast<int>(game->getTurn()) << ":"
       << (game->isGameOver() ? 1 : 0) << ":"
       << static_cast<int>(game->getWinner()) << ":";
    int numPlayers = board.getNumPlayers();
    for (int p = 1; p <= numPlayers; p++) {
        octi::OctiPlayer pl = static_cast<octi::OctiPlayer>(p);
        ss << game->getFreePods(pl) << ","
           << game->getCapturedPods(pl) << ","
           << game->getFreeProngs(pl) << ","
           << game->getFreeSuperProngs(pl) << ","
           << (game->isPlayerEliminated(pl) ? 1 : 0) << ":";
    }
    for (int y = 0; y < board.getBoardHeight(); y++) {
        for (int x = 0; x < board.getBoardWidth(); x++) {
            const octi::OctiSquare &sq = board.squareAt(x, y);
            if (sq.isEmpty()) {
                continue;
            }
            std::vector<std::string> podDescs;
            podDescs.reserve(sq.getNumPods());
            for (int i = 0; i < sq.getNumPods(); i++) {
                const octi::OctiPod &pod = sq.getPodAt(i);
                podDescs.push_back(std::to_string(static_cast<int>(pod.getType())) +
                                   ":" + pod.getConfigString());
            }
            std::sort(podDescs.begin(), podDescs.end());
            ss << "(" << x << "," << y << ";";
            for (const auto &d : podDescs) {
                ss << d << ",";
            }
            ss << ")";
        }
    }
    return ss.str();
}

} // namespace

TEST_GROUP(OctiSuperProngBonusGeneration)
{
};

TEST(OctiSuperProngBonusGeneration, StepRulesAndSuperProngsOffGuard)
{
    // A33 has superprongs N(a!), E(c!), S(e!) and normal prong W(g).
    // 34 (N) is empty -> bonus step allowed.
    // 43 (E) has friendly pod A43 -> bonus step onto own pod allowed.
    // 32 (S) has enemy pod B32 -> bonus step onto enemy pod refused.
    // 23 (W) is empty, but g is a normal prong -> bonus step along normal prong refused.
    octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    board.fromPositionString(
        "2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:B32,A33a!c!e!g,A43");

    std::vector<octi::OctiMove> moves;
    board.getGame()->generateLegalMoves(moves);

    // Step onto empty square 34 allowed:
    CHECK_TRUE(containsMoveWithPrefix(moves, "33a!c!e!g-34;"));
    // Step onto friendly square 43 allowed:
    CHECK_TRUE(containsMoveWithPrefix(moves, "33a!c!e!g-43;"));
    // Step onto enemy square 32 refused:
    CHECK_FALSE(containsMoveWithPrefix(moves, "33a!c!e!g-32;"));
    // Step along normal prong g to 23 refused as bonus prelude:
    CHECK_FALSE(containsMoveWithPrefix(moves, "33a!c!e!g-23;"));

    // Superprongs off guard: when useSuperProngs is false, zero prelude moves are generated.
    octi::OctiBoard boardOff(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    boardOff.fromPositionString(
        "2:3:0:0:P:A:-:0:0,0,1,0:0,0,0,0:B32,A33aceg,A43");
    std::vector<octi::OctiMove> movesOff;
    boardOff.getGame()->generateLegalMoves(movesOff);
    CHECK_FALSE(movesOff.empty());
    for (const auto &m : movesOff) {
        CHECK_FALSE(moveHasPrelude(m));
        CHECK_EQUAL(std::string::npos, m.asString().find(';'));
    }
}

TEST(OctiSuperProngBonusGeneration, StraightJumpChainAndRefusalRules)
{
    // A31 has superprongs N(a!), NE(b!), E(c!).
    // North (a!): B32 at (2,1), 33 empty, B34 at (2,3), 35 empty, 36 empty.
    //   -> 1-hop prefix 31a!b!c!-33; and 2-hop prefix 31a!b!c!-33-35; allowed;
    //   -> 3-hop to 37 over empty 36 refused.
    // NE (b!): B42 at (3,1), B53 at (4,2) (landing occupied)
    //   -> jump onto occupied 53 refused.
    // East (c!): 41 at (3,0) is empty
    //   -> jump over empty 41 to 51 refused.
    octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    board.fromPositionString(
        "2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A31a!b!c!,B32,B42,B53,B34");

    std::vector<octi::OctiMove> moves;
    board.getGame()->generateLegalMoves(moves);

    CHECK_TRUE(containsMoveWithPrefix(moves, "31a!b!c!-33;"));
    CHECK_TRUE(containsMoveWithPrefix(moves, "31a!b!c!-33-35;"));
    CHECK_FALSE(containsMoveWithPrefix(moves, "31a!b!c!-33-35-37"));
    CHECK_FALSE(containsMoveWithPrefix(moves, "31a!b!c!-53;"));
    CHECK_FALSE(containsMoveWithPrefix(moves, "31a!b!c!-51;"));

    // Also verify on edgeless board that jumping the same square twice in a
    // straight jump chain is refused.
    octi::OctiBoard edBoard(octi::GAME_MODE_2, true, true, octi::GAME_BASES_3);
    edBoard.fromPositionString(
        "2:3:1:1:P:A:-:0:0,0,1,0:0,0,0,0:A11a!,B12,B14,B16,B18,B19,B13,B15,B17");
    std::vector<octi::OctiMove> edMoves;
    edBoard.getGame()->generateLegalMoves(edMoves);
    for (const auto &m : edMoves) {
        octi::OctiBoard copy(octi::GAME_MODE_2, true, true, octi::GAME_BASES_3);
        copy.fromPositionString(edBoard.toPositionString());
        copy.getGame()->move(m);
    }
}

TEST(OctiSuperProngBonusGeneration, OptionalCaptureAndPostPreludeRegularActions)
{
    // Player A has 0 freePods, 0 capturedPods, 0 freeProngs, 0 freeSuperProngs.
    // A33a! can jump over B34e to 35.
    // Without capture (33a!-35), A still has 0 freeProngs -> cannot add a prong at 35.
    // With capture (33a!-35x), B34e is captured before the regular action, giving A
    // 1 freeProng -> regular action 35a!+b becomes legal in the same turn!
    octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    board.fromPositionString(
        "2:3:1:0:P:A:-:0:0,0,0,0:0,0,0,0:A33a!,B34e");

    std::vector<octi::OctiMove> moves;
    board.getGame()->generateLegalMoves(moves);

    // Non-capturing jump prelude + regular step 35a!-36:
    CHECK_TRUE(containsExactMove(moves, "33a!-35;35a!-36"));
    // Capturing jump prelude + regular step 35a!-36:
    CHECK_TRUE(containsExactMove(moves, "33a!-35x;35a!-36"));
    // Non-capturing jump prelude CANNOT add a prong (0 freeProngs):
    CHECK_FALSE(containsExactMove(moves, "33a!-35;35a!+b"));
    // Capturing jump prelude unlocks prong addition 35a!+b from the captured e prong:
    CHECK_TRUE(containsExactMove(moves, "33a!-35x;35a!+b"));

    // Verify notation round-trip and execution of "33a!-35x;35a!+b":
    octi::OctiMove parsed("33a!-35x;35a!+b");
    STRCMP_EQUAL("33a!-35x;35a!+b", parsed.asString().c_str());
    board.getGame()->move(parsed);
    CHECK_TRUE(board.squareAt(2, 3).isEmpty());
    CHECK_EQUAL(1, board.squareAt(2, 4).getNumPods());
}

TEST(OctiSuperProngBonusGeneration, EdgelessWrapAndTwoPreludesCanonicalDeduplication)
{
    // Part A: Edgeless wrap step and jump preludes.
    // A15g! at (0, 4) has West superprong g!.
    // Stepping West wraps to 95 (8, 4).
    // If B95 is at (8, 4) and 85 (7, 4) is empty, jumping West wraps to 85.
    octi::OctiBoard edStepBoard(octi::GAME_MODE_2, true, true, octi::GAME_BASES_3);
    edStepBoard.fromPositionString(
        "2:3:1:1:P:A:-:0:0,0,1,0:0,0,0,0:A15g!,B57");
    std::vector<octi::OctiMove> edStepMoves;
    edStepBoard.getGame()->generateLegalMoves(edStepMoves);
    CHECK_TRUE(containsExactMove(edStepMoves, "15g!-95;95g!+a"));

    octi::OctiBoard edJumpBoard(octi::GAME_MODE_2, true, true, octi::GAME_BASES_3);
    edJumpBoard.fromPositionString(
        "2:3:1:1:P:A:-:0:0,0,1,0:0,0,0,0:A15g!,B95,B57");
    std::vector<octi::OctiMove> edJumpMoves;
    edJumpBoard.getGame()->generateLegalMoves(edJumpMoves);
    CHECK_TRUE(containsExactMove(edJumpMoves, "15g!-85;85g!+a"));
    CHECK_TRUE(containsExactMove(edJumpMoves, "15g!-85x;85g!+a"));

    // Part B: Two preludes (single-socket rejection, 2-superprong pod, pre-existing
    // destination pod, and commuting different pods).
    octi::OctiBoard twoPrelBoard(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    twoPrelBoard.fromPositionString(
        "2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a!,A53a!,B57");
    std::vector<octi::OctiMove> twoPrelMoves;
    twoPrelBoard.getGame()->generateLegalMoves(twoPrelMoves);

    // Single superprong A33a! CANNOT expend its 'a!' socket twice across two preludes:
    CHECK_FALSE(containsExactMove(twoPrelMoves, "33a!-34;34a!-35;35a!+b"));

    // Two different pods (A33a! and A53a!) commute: emitted in canonical order
    // (lower source square 33 before 53) and NOT in reversed order (53 before 33):
    CHECK_TRUE(containsExactMove(twoPrelMoves, "33a!-34;53a!-54;34a!+b"));
    CHECK_FALSE(containsExactMove(twoPrelMoves, "53a!-54;33a!-34;34a!+b"));

    // Same pod with TWO distinct superprongs (A33a!c!) CAN take two sequential preludes
    // using 'a!' then 'c!' (33 -> 34 -> 44), then +b at 44:
    octi::OctiBoard twoSpPodBoard(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    twoSpPodBoard.fromPositionString(
        "2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a!c!,B57");
    std::vector<octi::OctiMove> twoSpPodMoves;
    twoSpPodBoard.getGame()->generateLegalMoves(twoSpPodMoves);
    CHECK_TRUE(containsExactMove(twoSpPodMoves, "33a!c!-34;34a!c!-44;44a!c!+b"));
    CHECK_FALSE(containsExactMove(twoSpPodMoves, "33a!c!-34;34a!c!-35;35a!c!+b"));

    // Pre-existing destination pod (A33a! and A34a!) allows the second prelude from 34
    // because the pre-existing A34a! has not expended its 'a!' socket:
    // - With B35 blocking a 1-prelude leapfrog to 35, 33a!-34;34a!-36;36a!+b is uniquely
    //   reachable in 2 preludes and emitted by generateLegalMoves().
    // - With 35 empty, move("33a!-34;34a!-35;35a!+b") is accepted by move() while
    //   generateLegalMoves() canonically deduplicates it to 1-prelude 33a!-35;35a!+b.
    octi::OctiBoard preDestBoard(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    preDestBoard.fromPositionString(
        "2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a!,A34a!,B35,B57");
    std::vector<octi::OctiMove> preDestMoves;
    preDestBoard.getGame()->generateLegalMoves(preDestMoves);
    CHECK_TRUE(containsExactMove(preDestMoves, "33a!-34;34a!-36;36a!+b"));
    CHECK_TRUE(containsExactMove(preDestMoves, "33a!-34;34a!-36x;36a!+b"));

    octi::OctiBoard singleBlockedBoard(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    singleBlockedBoard.fromPositionString(
        "2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a!,B35,B57");
    std::vector<octi::OctiMove> singleBlockedMoves;
    singleBlockedBoard.getGame()->generateLegalMoves(singleBlockedMoves);
    CHECK_FALSE(containsExactMove(singleBlockedMoves, "33a!-34;34a!-36;36a!+b"));
}

TEST(OctiSuperProngBonusGeneration, SingleSocketAndStraightDirectionRulesMatrix)
{
    // 1. Single superprong pod A33a! (34 empty):
    //    - 1 prelude + 1 regular (33a!-34;34a!-35 and 33a!-34;34a!+b) is LEGAL.
    //    - 2 preludes reusing 'a!' (33a!-34;34a!-35;35a!+b or ...;35a!-36) is ILLEGAL.
    {
        octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        board.fromPositionString("2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a!,B57");
        const std::string initialFen = board.toPositionString();

        std::vector<octi::OctiMove> moves = board.getGame()->generateLegalMoves();
        CHECK_TRUE(containsExactMove(moves, "33a!-34;34a!-35"));
        CHECK_TRUE(containsExactMove(moves, "33a!-34;34a!+b"));
        CHECK_FALSE(containsExactMove(moves, "33a!-34;34a!-35;35a!+b"));
        CHECK_FALSE(containsExactMove(moves, "33a!-34;34a!-35;35a!-36"));
        for (const auto &m : moves) {
            CHECK_TRUE(countPreludeOps(m) <= 1);
        }

        CHECK_THROWS(octi::IllegalMoveException,
                     board.getGame()->move("33a!-34;34a!-35;35a!+b"));
        CHECK_THROWS(octi::IllegalMoveException,
                     board.getGame()->move("33a!-34;34a!-35;35a!-36"));
        STRCMP_EQUAL(initialFen.c_str(), board.toPositionString().c_str());

        board.getGame()->move("33a!-34;34a!-35");
        CHECK_TRUE(board.squareAt(2, 2).isEmpty());
        CHECK_TRUE(board.squareAt(2, 3).isEmpty());
        CHECK_EQUAL(1, board.squareAt(2, 4).getNumPods());
    }

    // 2. Dual-superprong single pod A33a!c!:
    //    - 33a!c!-34;34a!c!-44;44a!c!+b (using a! then c!) is LEGAL.
    //    - 33a!c!-34;34a!c!-35;35a!c!+b (using a! twice) and
    //      33a!c!-43;43a!c!-53;53a!c!+b (using c! twice) are ILLEGAL.
    {
        octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        board.fromPositionString("2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a!c!,B57");
        std::vector<octi::OctiMove> moves = board.getGame()->generateLegalMoves();

        CHECK_TRUE(containsExactMove(moves, "33a!c!-34;34a!c!-44;44a!c!+b"));
        CHECK_FALSE(containsExactMove(moves, "33a!c!-34;34a!c!-35;35a!c!+b"));
        CHECK_FALSE(containsExactMove(moves, "33a!c!-43;43a!c!-53;53a!c!+b"));

        CHECK_THROWS(octi::IllegalMoveException,
                     board.getGame()->move("33a!c!-34;34a!c!-35;35a!c!+b"));
        CHECK_THROWS(octi::IllegalMoveException,
                     board.getGame()->move("33a!c!-43;43a!c!-53;53a!c!+b"));

        board.getGame()->move("33a!c!-34;34a!c!-44;44a!c!+b");
        CHECK_EQUAL(1, board.squareAt(3, 3).getNumPods());
        STRCMP_EQUAL("a!bc!", board.squareAt(3, 3).getPodAt(0).getConfigString().c_str());
    }

    // 3. Stacked identical pods A33a!,A33a!:
    //    - 33a!-34;33a!-34;34a!+b (each pod expends its own a! once) is LEGAL.
    //    - 33a!-34;34a!-35;35a!+b (moving the first pod twice as two preludes) is ILLEGAL.
    {
        octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        board.fromPositionString("2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a!,A33a!,B57");
        std::vector<octi::OctiMove> moves = board.getGame()->generateLegalMoves();

        CHECK_TRUE(containsExactMove(moves, "33a!-34;33a!-34;34a!+b"));
        CHECK_FALSE(containsExactMove(moves, "33a!-34;34a!-35;35a!+b"));

        CHECK_THROWS(octi::IllegalMoveException,
                     board.getGame()->move("33a!-34;34a!-35;35a!+b"));

        board.getGame()->move("33a!-34;33a!-34;34a!+b");
        CHECK_TRUE(board.squareAt(2, 2).isEmpty());
        CHECK_EQUAL(2, board.squareAt(2, 3).getNumPods());
    }

    // 4. Pre-existing destination pod A33a!,A34a!:
    //    - 33a!-34;34a!-35;35a!+b is LEGAL in move() (sameCfgCountAfterP1 == 2 at 34),
    //      while generateLegalMoves() emits its canonical 1-prelude equivalent 33a!-35;35a!+b;
    //      and when B35 occupies 35 (blocking 33a!-35), generateLegalMoves() directly emits
    //      33a!-34;34a!-36;36a!+b.
    //    - 33a!-35;35a!-36;36a!+b is ILLEGAL (p1 jumped over 34 to empty 35 -> sameCfgCountAfterP1 == 1).
    {
        octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        board.fromPositionString("2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a!,A34a!,B57");
        std::vector<octi::OctiMove> moves = board.getGame()->generateLegalMoves();

        CHECK_TRUE(containsExactMove(moves, "33a!-35;35a!+b"));
        CHECK_FALSE(containsExactMove(moves, "33a!-35;35a!-36;36a!+b"));

        CHECK_THROWS(octi::IllegalMoveException,
                     board.getGame()->move("33a!-35;35a!-36;36a!+b"));

        board.getGame()->move("33a!-34;34a!-35;35a!+b");
        CHECK_TRUE(board.squareAt(2, 2).isEmpty());
        CHECK_EQUAL(1, board.squareAt(2, 3).getNumPods());
        CHECK_EQUAL(1, board.squareAt(2, 4).getNumPods());

        octi::OctiBoard blockedBoard(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        blockedBoard.fromPositionString("2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a!,A34a!,B35,B57");
        std::vector<octi::OctiMove> blockedMoves = blockedBoard.getGame()->generateLegalMoves();
        CHECK_TRUE(containsExactMove(blockedMoves, "33a!-34;34a!-36;36a!+b"));
        blockedBoard.getGame()->move("33a!-34;34a!-36;36a!+b");
        CHECK_TRUE(blockedBoard.squareAt(2, 2).isEmpty());
        CHECK_EQUAL(1, blockedBoard.squareAt(2, 3).getNumPods());
        CHECK_EQUAL(1, blockedBoard.squareAt(2, 5).getNumPods());
    }

    // 5. Mid-prelude bend loophole (over enemy pods, over friendly pods, in p2, and across edgeless wrap):
    //    - Single bonus op 31a!c!-33-53;53a!c!+b bends from DIR_N to DIR_E -> ILLEGAL.
    //    - Two separate straight preludes 31a!c!-33;33a!c!-53;53a!c!+b -> LEGAL.
    {
        octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        board.fromPositionString("2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A31a!c!,B32,B43,B57");
        std::vector<octi::OctiMove> moves = board.getGame()->generateLegalMoves();

        CHECK_FALSE(containsExactMove(moves, "31a!c!-33-53;53a!c!+b"));
        CHECK_TRUE(containsExactMove(moves, "31a!c!-33;33a!c!-53;53a!c!+b"));

        bool caughtBendEnemy = false;
        try {
            board.getGame()->move("31a!c!-33-53;53a!c!+b");
        } catch (const octi::IllegalMoveException &ex) {
            caughtBendEnemy = true;
            STRCMP_CONTAINS("bonus jump must follow single superprong direction", ex.what());
        }
        CHECK_TRUE(caughtBendEnemy);

        // Friendly pods jumped in a bent single prelude must also fail:
        octi::OctiBoard friendlyBoard(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        friendlyBoard.fromPositionString("2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A31a!c!,A32,A43,B57");
        bool caughtBendFriendly = false;
        try {
            friendlyBoard.getGame()->move("31a!c!-33-53;53a!c!+b");
        } catch (const octi::IllegalMoveException &ex) {
            caughtBendFriendly = true;
            STRCMP_CONTAINS("bonus jump must follow single superprong direction", ex.what());
        }
        CHECK_TRUE(caughtBendFriendly);

        // Bend inside bonusOps[1] (2nd prelude) must also fail:
        octi::OctiBoard p2BendBoard(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        p2BendBoard.fromPositionString("2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A11a!,A31a!c!,B32,B43,B57");
        bool caughtBendP2 = false;
        try {
            p2BendBoard.getGame()->move("11a!-12;31a!c!-33-53;53a!c!+b");
        } catch (const octi::IllegalMoveException &ex) {
            caughtBendP2 = true;
            STRCMP_CONTAINS("bonus jump must follow single superprong direction", ex.what());
        }
        CHECK_TRUE(caughtBendP2);

        // Edgeless wrap mid-prelude bend (98 -> 91 North over 99, then -> 21 East over 11) must fail:
        octi::OctiBoard edBendBoard(octi::GAME_MODE_2, true, true, octi::GAME_BASES_3);
        edBendBoard.fromPositionString("2:3:1:1:P:A:-:0:0,0,1,0:0,0,0,0:B11,B57,A98a!c!,B99");
        bool caughtEdBend = false;
        try {
            edBendBoard.getGame()->move("98a!c!-91-21;21a!c!+b");
        } catch (const octi::IllegalMoveException &ex) {
            caughtEdBend = true;
            STRCMP_CONTAINS("bonus jump must follow single superprong direction", ex.what());
        }
        CHECK_TRUE(caughtEdBend);
        // Two separate edgeless straight preludes 98a!c!-91;91a!c!-21;21a!c!+b succeed:
        edBendBoard.getGame()->move("98a!c!-91;91a!c!-21;21a!c!+b");
        CHECK_EQUAL(1, edBendBoard.squareAt(1, 0).getNumPods());

        // Two separate straight preludes on standard board succeed:
        board.getGame()->move("31a!c!-33;33a!c!-53;53a!c!+b");
        CHECK_EQUAL(1, board.squareAt(4, 2).getNumPods());
    }
}

TEST(OctiSuperProngBonusGeneration, ApplyUncommittedTurnForPreviewNominalBoundaryAndFaults)
{
    // 1. Prelude-only 33a!-35x capturing B34ef! updates squares & reserves while keeping turn == PLAYER_A
    {
        octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        board.fromPositionString("2:3:1:0:P:A:-:0:0,0,0,0:0,0,0,0:A33a!,B34ef!,B57");
        octi::OctiGame *game = board.getGame();

        octi::OctiPod podA(octi::A_POD);
        podA.addProng(octi::DIR_N, true);

        octi::OctiMoveOp p1;
        p1.x0 = 2;
        p1.y0 = 2;
        p1.pod = podA;
        p1.isSuperProngBonusMove = true;
        p1.hops.push_back({2, 4, true});

        game->applyUncommittedTurnForPreview({p1});

        CHECK_TRUE(board.squareAt(2, 2).isEmpty());
        CHECK_TRUE(board.squareAt(2, 3).isEmpty());
        CHECK_EQUAL(1, board.squareAt(2, 4).getNumPods());
        STRCMP_EQUAL("a!", board.squareAt(2, 4).getPodAt(0).getConfigString().c_str());
        CHECK_EQUAL(1, game->getFreeProngs(octi::PLAYER_A));
        CHECK_EQUAL(1, game->getFreeSuperProngs(octi::PLAYER_A));
        CHECK_EQUAL(1, game->getCapturedPods(octi::PLAYER_B));
        CHECK_EQUAL(octi::PLAYER_A, game->getTurn());
        CHECK_EQUAL(0, game->getNumMoves());
        CHECK_FALSE(game->canUndo());
        CHECK_FALSE(game->isGameOver());
    }

    // 2. Two-prelude-only preview with R5 immediate capture removal across p1 and p2
    {
        octi::OctiBoard board2p(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        board2p.fromPositionString("2:3:1:0:P:A:-:0:0,0,0,0:0,0,0,0:A33a!,A24c!,B34e,B57");
        const std::string before2pFen = board2p.toPositionString();

        octi::OctiPod podN(octi::A_POD);
        podN.addProng(octi::DIR_N, true);
        octi::OctiPod podE(octi::A_POD);
        podE.addProng(octi::DIR_E, true);

        octi::OctiMoveOp p1;
        p1.x0 = 2; p1.y0 = 2; p1.pod = podN; p1.isSuperProngBonusMove = true;
        p1.hops.push_back({2, 4, true}); // 33a!-35x immediately removes B34e

        // Attempting to jump over already-captured 34 in p2 must fail and roll back cleanly:
        octi::OctiMoveOp p2JumpEmpty;
        p2JumpEmpty.x0 = 1; p2JumpEmpty.y0 = 3; p2JumpEmpty.pod = podE; p2JumpEmpty.isSuperProngBonusMove = true;
        p2JumpEmpty.hops.push_back({3, 3, false});
        CHECK_THROWS(octi::IllegalMoveException,
                     board2p.getGame()->applyUncommittedTurnForPreview({p1, p2JumpEmpty}));
        STRCMP_EQUAL(before2pFen.c_str(), board2p.toPositionString().c_str());

        // Stepping into newly vacated 34 in p2 succeeds because B34e was removed immediately after p1:
        octi::OctiMoveOp p2StepVacated;
        p2StepVacated.x0 = 1; p2StepVacated.y0 = 3; p2StepVacated.pod = podE; p2StepVacated.isSuperProngBonusMove = true;
        p2StepVacated.hops.push_back({2, 3, false}); // 24c!-34 steps onto newly vacated 34

        board2p.getGame()->applyUncommittedTurnForPreview({p1, p2StepVacated});
        CHECK_EQUAL(1, board2p.squareAt(2, 3).getNumPods());
        CHECK_EQUAL(octi::A_POD, board2p.squareAt(2, 3).getPodAt(0).getType());
        CHECK_EQUAL(1, board2p.squareAt(2, 4).getNumPods());
        CHECK_EQUAL(1, board2p.getGame()->getFreeProngs(octi::PLAYER_A));
        CHECK_EQUAL(1, board2p.getGame()->getCapturedPods(octi::PLAYER_B));
        CHECK_EQUAL(octi::PLAYER_A, board2p.getGame()->getTurn());
        CHECK_EQUAL(0, board2p.getGame()->getNumMoves());

        // Full p1 + regular prong-addition preview from 0 initial reserve prongs:
        octi::OctiBoard boardFull(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        boardFull.fromPositionString("2:3:1:0:P:A:-:0:0,0,0,0:0,0,0,0:A33a!,B34e,B57");
        octi::OctiMove fullMv("33a!-35x;35a!+b");
        boardFull.getGame()->applyUncommittedTurnForPreview(fullMv.getOps());
        CHECK_EQUAL(1, boardFull.squareAt(2, 4).getNumPods());
        STRCMP_EQUAL("a!b", boardFull.squareAt(2, 4).getPodAt(0).getConfigString().c_str());
        CHECK_EQUAL(0, boardFull.getGame()->getFreeProngs(octi::PLAYER_A));
        CHECK_EQUAL(octi::PLAYER_A, boardFull.getGame()->getTurn());
        CHECK_EQUAL(0, boardFull.getGame()->getNumMoves());
    }

    // 3. Fault injection & atomic rollback on invalid preview operations
    {
        octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        board.fromPositionString("2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a!,A31a!c!,B32,B43,B57");
        const std::string origFen = board.toPositionString();
        octi::OctiGame *game = board.getGame();

        // Empty ops
        CHECK_THROWS(octi::IllegalMoveException, game->applyUncommittedTurnForPreview({}));
        STRCMP_EQUAL(origFen.c_str(), board.toPositionString().c_str());

        // Reused single superprong socket in p1 + p2 prelude-only preview
        octi::OctiPod podA(octi::A_POD);
        podA.addProng(octi::DIR_N, true);
        octi::OctiMoveOp p1;
        p1.x0 = 2; p1.y0 = 2; p1.pod = podA; p1.isSuperProngBonusMove = true;
        p1.hops.push_back({2, 3, false});
        octi::OctiMoveOp p2Reuse;
        p2Reuse.x0 = 2; p2Reuse.y0 = 3; p2Reuse.pod = podA; p2Reuse.isSuperProngBonusMove = true;
        p2Reuse.hops.push_back({2, 4, false});
        CHECK_THROWS(octi::IllegalMoveException,
                     game->applyUncommittedTurnForPreview({p1, p2Reuse}));
        STRCMP_EQUAL(origFen.c_str(), board.toPositionString().c_str());

        // Bent single bonus prelude in preview
        octi::OctiPod podAC(octi::A_POD);
        podAC.addProng(octi::DIR_N, true);
        podAC.addProng(octi::DIR_E, true);
        octi::OctiMoveOp pBend;
        pBend.x0 = 2; pBend.y0 = 0; pBend.pod = podAC; pBend.isSuperProngBonusMove = true;
        pBend.hops.push_back({2, 2, false});
        pBend.hops.push_back({4, 2, false});
        CHECK_THROWS(octi::IllegalMoveException,
                     game->applyUncommittedTurnForPreview({pBend}));
        STRCMP_EQUAL(origFen.c_str(), board.toPositionString().c_str());

        // Bonus op placed after regular op
        octi::OctiMoveOp regStep = p1;
        regStep.isSuperProngBonusMove = false;
        CHECK_THROWS(octi::IllegalMoveException,
                     game->applyUncommittedTurnForPreview({regStep, p2Reuse}));
        STRCMP_EQUAL(origFen.c_str(), board.toPositionString().c_str());

        // Out-of-bounds hop coordinate
        octi::OctiMoveOp oobOp = p1;
        oobOp.hops[0] = {9, 3, false};
        CHECK_THROWS(octi::IllegalMoveException,
                     game->applyUncommittedTurnForPreview({oobOp}));
        STRCMP_EQUAL(origFen.c_str(), board.toPositionString().c_str());

        // Superprongs disabled
        octi::OctiBoard offBoard(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
        offBoard.fromPositionString("2:3:0:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a,B57");
        CHECK_THROWS(octi::IllegalMoveException,
                     offBoard.getGame()->applyUncommittedTurnForPreview({p1}));
    }
}

TEST(OctiSuperProngBonusGeneration, RapidCheckGeneratorAndValidatorBijectiveAgreement)
{
    bool passed = rc::check(
        "generateLegalMoves and move() bijectively agree on randomized 2P/4P/edgeless superprong boards",
        []() {
            const bool is4P = *rc::gen::arbitrary<bool>();
            const bool edgeless = *rc::gen::arbitrary<bool>();
            const octi::OctiGameMode mode = is4P ? octi::GAME_MODE_4 : octi::GAME_MODE_2;
            const octi::OctiGameBases bases =
                *rc::gen::arbitrary<bool>() ? octi::GAME_BASES_1 : octi::GAME_BASES_3;

            const int freePodsA = *rc::gen::inRange(0, 2);
            const int capPodsA = *rc::gen::inRange(0, 2);
            const int freeProngsA = *rc::gen::inRange(0, 3);
            const int freeSuperA = *rc::gen::inRange(0, 2);

            static const int kSafeCoords[][2] = {
                {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {7, 0}, {8, 0},
                {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1},
                {1, 2}, {3, 2}, {5, 2},
                {0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3},
                {0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {8, 4},
                {1, 5}, {2, 5}, {3, 5}, {4, 5}
            };
            constexpr int kNumSafe = static_cast<int>(sizeof(kSafeCoords) / sizeof(kSafeCoords[0]));

            struct Placement {
                int x;
                int y;
                char owner;
                std::string cfg;
            };
            std::vector<Placement> pls;

            // Anchor friendly pod 0 on a safe square with 1 or 2 superprongs + optional normal prong
            const int idx0 = *rc::gen::inRange(0, kNumSafe);
            const int x0 = kSafeCoords[idx0][0];
            const int y0 = kSafeCoords[idx0][1];
            const int sp1 = *rc::gen::inRange(0, 8);
            const int sp2 = *rc::gen::inRange(0, 8);
            const int np1 = *rc::gen::inRange(0, 8);
            std::string cfg0;
            for (int d = 0; d < 8; ++d) {
                char c = static_cast<char>('a' + d);
                if (d == sp1 || (*rc::gen::arbitrary<bool>() && d == sp2)) {
                    cfg0.push_back(c);
                    cfg0.push_back('!');
                } else if (d == np1) {
                    cfg0.push_back(c);
                }
            }
            pls.push_back({x0, y0, 'A', cfg0});

            // Second friendly pod: stacked identical, adjacent pre-existing, or elsewhere
            const int secondMode = *rc::gen::inRange(0, 3);
            if (secondMode == 0) {
                pls.push_back({x0, y0, 'A', cfg0});
            } else if (secondMode == 1) {
                int nx = edgeless ? ((x0 + octi::dirDeltaX(static_cast<octi::OctiDir>(sp1)) + 9) % 9)
                                  : (x0 + octi::dirDeltaX(static_cast<octi::OctiDir>(sp1)));
                int ny = edgeless ? ((y0 + octi::dirDeltaY(static_cast<octi::OctiDir>(sp1)) + 9) % 9)
                                  : (y0 + octi::dirDeltaY(static_cast<octi::OctiDir>(sp1)));
                if (nx >= 0 && nx < 9 && ny >= 0 && ny < 9 && ny != 2 && ny != 6) {
                    pls.push_back({nx, ny, 'A', cfg0});
                }
            } else {
                const int idx1 = *rc::gen::inRange(0, kNumSafe);
                const int d1 = *rc::gen::inRange(0, 8);
                std::string cfg1(1, static_cast<char>('a' + d1));
                cfg1.push_back('!');
                pls.push_back({kSafeCoords[idx1][0], kSafeCoords[idx1][1], 'A', cfg1});
            }

            // Place 1 or 2 enemy pods (often adjacent to x0,y0 for jump/capture/bend coverage)
            const int numEnemy = *rc::gen::inRange(1, 3);
            for (int e = 0; e < numEnemy; ++e) {
                int ex = kSafeCoords[*rc::gen::inRange(0, kNumSafe)][0];
                int ey = kSafeCoords[*rc::gen::inRange(0, kNumSafe)][1];
                if (e == 0 && *rc::gen::arbitrary<bool>()) {
                    int d = *rc::gen::inRange(0, 8);
                    int tx = edgeless
                        ? ((x0 + octi::dirDeltaX(static_cast<octi::OctiDir>(d)) + 9) % 9)
                        : (x0 + octi::dirDeltaX(static_cast<octi::OctiDir>(d)));
                    int ty = edgeless
                        ? ((y0 + octi::dirDeltaY(static_cast<octi::OctiDir>(d)) + 9) % 9)
                        : (y0 + octi::dirDeltaY(static_cast<octi::OctiDir>(d)));
                    if (tx >= 0 && tx < 9 && ty >= 0 && ty < 9 && ty != 2 && ty != 6) {
                        ex = tx;
                        ey = ty;
                    }
                }
                bool occupiedByFriendly = false;
                for (const auto &p : pls) {
                    if (p.owner == 'A' && p.x == ex && p.y == ey) {
                        occupiedByFriendly = true;
                        break;
                    }
                }
                if (!occupiedByFriendly) {
                    int ed = *rc::gen::inRange(0, 8);
                    std::string ecfg(1, static_cast<char>('a' + ed));
                    if (*rc::gen::arbitrary<bool>()) {
                        ecfg.push_back('!');
                    }
                    pls.push_back({ex, ey, 'B', ecfg});
                }
            }
            // Guarantee B has at least one pod so the game is active
            bool hasB = false;
            for (const auto &p : pls) {
                if (p.owner == 'B') {
                    hasB = true;
                    break;
                }
            }
            if (!hasB) {
                pls.push_back({8, 8, 'B', "e"});
            }

            std::ostringstream fen;
            fen << (is4P ? "4:" : "2:")
                << (bases == octi::GAME_BASES_1 ? "1:" : "3:")
                << "1:" << (edgeless ? "1:" : "0:")
                << "P:A:-:0:"
                << freePodsA << "," << capPodsA << "," << freeProngsA << "," << freeSuperA << ":"
                << "1,0,1,0:";
            if (is4P) {
                fen << "1,0,1,0:1,0,1,0:";
            }
            for (std::size_t i = 0; i < pls.size(); ++i) {
                if (i > 0) fen << ",";
                fen << pls[i].owner << (pls[i].x + 1) << (pls[i].y + 1) << pls[i].cfg;
            }

            const std::string fenStr = fen.str();
            octi::OctiBoard board(mode, true, edgeless, bases);
            board.suppressBoardUpdateNotification(true);
            board.fromPositionString(fenStr);

            std::vector<octi::OctiMove> legalMoves;
            board.getGame()->generateLegalMoves(legalMoves);
            if (legalMoves.empty()) {
                return;
            }

            octi::OctiBoard sim(mode, true, edgeless, bases);
            sim.suppressBoardUpdateNotification(true);
            sim.fromPositionString(fenStr);
            sim.getGame()->setSearchMode(true);
            const octi::OctiGameSnapshot snap = sim.getGame()->captureSnapshot();

            std::unordered_set<std::string> genCanonStates;
            std::unordered_set<std::string> genStrings;
            genCanonStates.reserve(legalMoves.size() * 2 + 1);
            genStrings.reserve(legalMoves.size() * 2 + 1);

            std::vector<octi::OctiMoveOp> singlePreludes;
            std::vector<octi::OctiMoveOp> sampleRegularOps;

            // Direction A: Every move in generateLegalMoves() must be accepted by move(),
            // produce a unique canonical resulting state, and satisfy straight/single-socket rules.
            for (const auto &m : legalMoves) {
                genStrings.insert(m.asString());
                const auto &ops = m.getOps();
                if (ops.size() == 1 && !ops[0].isSuperProngBonusMove && sampleRegularOps.size() < 6) {
                    sampleRegularOps.push_back(ops[0]);
                }
                if (ops.size() >= 2 && ops[0].isSuperProngBonusMove && !ops[1].isSuperProngBonusMove &&
                    singlePreludes.size() < 12) {
                    singlePreludes.push_back(ops[0]);
                }

                sim.getGame()->restoreSnapshot(snap);
                sim.getGame()->move(m);
                const std::string afterCanon = canonicalBoardStateForTest(sim);
                RC_ASSERT(genCanonStates.insert(afterCanon).second);
            }

            // Direction B: Probe adversarial & boundary candidate moves against move() and
            // verify move() accepts iff the candidate is legal AND lands in genCanonStates.
            // Probe 1: Reusing a single prelude's pod + direction from its landing square
            for (const auto &p1 : singlePreludes) {
                const int endX = p1.hops.back().x;
                const int endY = p1.hops.back().y;
                // Determine p1 direction
                int dx = 0, dy = 0;
                if (!edgeless) {
                    dx = p1.hops[0].x - p1.x0;
                    dy = p1.hops[0].y - p1.y0;
                } else {
                    for (int d = -2; d <= 2; ++d) {
                        if (((p1.x0 + d) % 9 + 9) % 9 == p1.hops[0].x) { dx = d; break; }
                    }
                    for (int d = -2; d <= 2; ++d) {
                        if (((p1.y0 + d) % 9 + 9) % 9 == p1.hops[0].y) { dy = d; break; }
                    }
                }
                const int stepDx = (std::abs(dx) == 2 || std::abs(dy) == 2) ? (dx / 2) : dx;
                const int stepDy = (std::abs(dx) == 2 || std::abs(dy) == 2) ? (dy / 2) : dy;
                const int nx = edgeless ? (((endX + stepDx) % 9 + 9) % 9) : (endX + stepDx);
                const int ny = edgeless ? (((endY + stepDy) % 9 + 9) % 9) : (endY + stepDy);
                if (nx < 0 || nx >= 9 || ny < 0 || ny >= 9) {
                    continue;
                }

                // Count pre-existing identical friendly pods at (endX, endY) before p1
                int preExistingSameCfg = 0;
                if (!(endX == p1.x0 && endY == p1.y0)) {
                    const octi::OctiSquare &destSq = board.squareAt(endX, endY);
                    for (int pi = 0; pi < destSq.getNumPods(); ++pi) {
                        const octi::OctiPod &dp = destSq.getPodAt(pi);
                        if (dp.getType() == octi::A_POD &&
                            (dp.rawConfig() & 0xFFFFu) == (p1.pod.rawConfig() & 0xFFFFu)) {
                            ++preExistingSameCfg;
                        }
                    }
                }

                octi::OctiMoveOp p2Reuse;
                p2Reuse.x0 = endX;
                p2Reuse.y0 = endY;
                p2Reuse.pod = p1.pod;
                p2Reuse.isSuperProngBonusMove = true;
                p2Reuse.hops.push_back({nx, ny, false});

                // Pair with a pod placement or prong addition or regular op
                for (const auto &reg : sampleRegularOps) {
                    std::vector<octi::OctiMoveOp> candOps = {p1, p2Reuse, reg};
                    octi::OctiMove candMove(candOps);
                    sim.getGame()->restoreSnapshot(snap);
                    bool accepted = false;
                    try {
                        sim.getGame()->move(candMove);
                        accepted = true;
                    } catch (const octi::IllegalMoveException &) {
                        accepted = false;
                    }
                    if (preExistingSameCfg == 0) {
                        RC_ASSERT(!accepted);
                        RC_ASSERT(genStrings.count(candMove.asString()) == 0u);
                    } else if (accepted) {
                        const std::string afterCanon = canonicalBoardStateForTest(sim);
                        RC_ASSERT(genCanonStates.count(afterCanon) == 1u);
                    }
                }
            }

            // Probe 2: Preview helper on every singlePrelude preserves turn == PLAYER_A and 0 moves
            for (const auto &p1 : singlePreludes) {
                sim.getGame()->restoreSnapshot(snap);
                sim.getGame()->setSearchMode(false);
                sim.getGame()->applyUncommittedTurnForPreview({p1});
                RC_ASSERT(sim.getGame()->getTurn() == octi::PLAYER_A);
                RC_ASSERT(sim.getGame()->getNumMoves() == 0);
                sim.getGame()->setSearchMode(true);
            }
        });
    CHECK_TRUE(passed);
}

TEST(OctiSuperProngBonusGeneration, Random1000SuperProngPositionsRoundTripAndUniqueResultingPositions)
{
    std::mt19937 rng(0x50504247u);
    int totalPositions = 0;
    std::size_t totalMoves = 0;
    std::size_t totalPreludeMoves = 0;
    std::size_t totalTwoPreludeMoves = 0;

    for (int iter = 0; iter < 1000; iter++) {
        bool is4P = (iter % 4 == 3);
        bool edgeless = (iter % 2 == 1);
        octi::OctiGameMode mode = is4P ? octi::GAME_MODE_4 : octi::GAME_MODE_2;
        octi::OctiGameBases bases = (iter % 3 == 0) ? octi::GAME_BASES_1 : octi::GAME_BASES_3;

        int freePodsA = static_cast<int>(rng() % 2);
        int capPodsA = static_cast<int>(rng() % 2);
        int freeProngsA = 1 + static_cast<int>(rng() % 2);
        int freeSuperA = static_cast<int>(rng() % 2);

        std::ostringstream pos;
        pos << (is4P ? "4" : "2") << ":"
            << (bases == octi::GAME_BASES_1 ? "1" : "3") << ":1:"
            << (edgeless ? "1" : "0") << ":P:A:-:0:"
            << freePodsA << "," << capPodsA << "," << freeProngsA << "," << freeSuperA << ":"
            << "0,0,1,0:";
        if (is4P) {
            pos << "0,0,1,0:0,0,1,0:";
        }

        // Generate 2 to 4 pods on safe non-base coordinates so the game is not already over
        static const int kSafeCoords[][2] = {
            {0, 0}, {1, 0}, {3, 0}, {4, 0}, {7, 0}, {8, 0},
            {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {7, 1},
            {1, 2}, {3, 2}, {5, 2}, {7, 2},
            {0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3}, {7, 3},
            {0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}, {6, 4}, {7, 4}, {8, 4},
            {1, 5}, {2, 5}, {3, 5}, {4, 5}, {5, 5}, {6, 5}, {7, 5}
        };
        constexpr int kNumSafe = static_cast<int>(sizeof(kSafeCoords) / sizeof(kSafeCoords[0]));

        int numFriendly = 1 + static_cast<int>(rng() % 2);
        int numEnemy = 1 + static_cast<int>(rng() % 2);
        std::vector<int> chosenIndices;
        while (static_cast<int>(chosenIndices.size()) < numFriendly + numEnemy) {
            int idx = static_cast<int>(rng() % kNumSafe);
            if (std::find(chosenIndices.begin(), chosenIndices.end(), idx) == chosenIndices.end()) {
                chosenIndices.push_back(idx);
            }
        }

        struct PodPlacement {
            int x;
            int y;
            char owner;
            std::string cfg;
        };
        std::vector<PodPlacement> placements;

        for (int f = 0; f < numFriendly; f++) {
            int cx = kSafeCoords[chosenIndices[f]][0];
            int cy = kSafeCoords[chosenIndices[f]][1];
            int spDir1 = static_cast<int>(rng() % 8);
            int spDir2 = static_cast<int>(rng() % 8);
            int normDir = static_cast<int>(rng() % 8);
            std::string cfg;
            for (int d = 0; d < 8; d++) {
                char c = static_cast<char>('a' + d);
                if (d == spDir1 || (f == 0 && d == spDir2)) {
                    cfg.push_back(c);
                    cfg.push_back('!');
                } else if (d == normDir) {
                    cfg.push_back(c);
                }
            }
            placements.push_back({cx, cy, 'A', cfg});
        }

        for (int e = 0; e < numEnemy; e++) {
            // Place at least the first enemy pod adjacent to the first friendly pod
            // half the time so jump/step interactions are heavily exercised.
            int cx = kSafeCoords[chosenIndices[numFriendly + e]][0];
            int cy = kSafeCoords[chosenIndices[numFriendly + e]][1];
            if (e == 0 && (iter % 2 == 0)) {
                int fx = placements[0].x;
                int fy = placements[0].y;
                int d = static_cast<int>(rng() % 8);
                static const int kDx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
                static const int kDy[8] = {1, 1, 0, -1, -1, -1, 0, 1};
                int nx = edgeless ? ((fx + kDx[d] + 9) % 9) : (fx + kDx[d]);
                int ny = edgeless ? ((fy + kDy[d] + 9) % 9) : (fy + kDy[d]);
                if (nx >= 0 && nx < 9 && ny >= 0 && ny < 9 && ny != 2 && ny != 6) {
                    bool clash = false;
                    for (const auto &pl : placements) {
                        if (pl.x == nx && pl.y == ny) {
                            clash = true;
                            break;
                        }
                    }
                    if (!clash) {
                        cx = nx;
                        cy = ny;
                    }
                }
            }
            int eDir = static_cast<int>(rng() % 8);
            std::string cfg(1, static_cast<char>('a' + eDir));
            if (e == 0 && (iter % 50 == 0)) {
                cfg.push_back('!');
            }
            placements.push_back({cx, cy, 'B', cfg});
        }

        std::sort(placements.begin(), placements.end(),
                  [](const PodPlacement &a, const PodPlacement &b) {
                      if (a.y != b.y) return a.y < b.y;
                      return a.x < b.x;
                  });

        for (std::size_t i = 0; i < placements.size(); i++) {
            if (i > 0) pos << ",";
            pos << placements[i].owner
                << (placements[i].x + 1)
                << (placements[i].y + 1)
                << placements[i].cfg;
        }

        const std::string posStr = pos.str();
        octi::OctiBoard board(mode, true, edgeless, bases);
        board.fromPositionString(posStr);

        std::vector<octi::OctiMove> moves;
        board.getGame()->generateLegalMoves(moves);
        CHECK_FALSE(moves.empty());

        std::set<std::string> seenResultPositions;
        std::set<std::string> seenCanonicalStates;

        octi::OctiBoard simBoard(mode, true, edgeless, bases);
        simBoard.fromPositionString(posStr);
        simBoard.getGame()->setSearchMode(true);
        octi::OctiGameSnapshot snap = simBoard.getGame()->captureSnapshot();
        for (const auto &m : moves) {
            int pCount = countPreludeOps(m);
            if (pCount > 0) totalPreludeMoves++;
            if (pCount == 2) totalTwoPreludeMoves++;

            // Notation round-trip check
            std::string moveStr = m.asString();
            octi::OctiMove roundTripped(moveStr);
            STRCMP_EQUAL(moveStr.c_str(), roundTripped.asString().c_str());
            CHECK_EQUAL(m.getOps().size(), roundTripped.getOps().size());
            for (std::size_t k = 0; k < m.getOps().size(); k++) {
                const auto &opA = m.getOps()[k];
                const auto &opB = roundTripped.getOps()[k];
                CHECK_EQUAL(opA.x0, opB.x0);
                CHECK_EQUAL(opA.y0, opB.y0);
                CHECK_EQUAL(opA.pod.rawConfig() & 0xFFFFu,
                            opB.pod.rawConfig() & 0xFFFFu);
                CHECK_EQUAL(opA.addPod, opB.addPod);
                CHECK_EQUAL(opA.liberate, opB.liberate);
                CHECK_EQUAL(opA.addProngDir, opB.addProngDir);
                CHECK_EQUAL(opA.rmProngDir, opB.rmProngDir);
                CHECK_EQUAL(opA.superProng, opB.superProng);
                CHECK_EQUAL(opA.isSuperProngBonusMove, opB.isSuperProngBonusMove);
                CHECK_TRUE(opA.hops == opB.hops);
            }

            // Execution check via applyMoveOps
            simBoard.getGame()->restoreSnapshot(snap);
            simBoard.getGame()->move(roundTripped);

            std::string afterPos = simBoard.toPositionString();
            std::string afterCanon = canonicalBoardStateForTest(simBoard);
            CHECK_TRUE(seenResultPositions.insert(afterPos).second);
            CHECK_TRUE(seenCanonicalStates.insert(afterCanon).second);
        }

        totalPositions++;
        totalMoves += moves.size();
    }

    CHECK_EQUAL(1000, totalPositions);
    CHECK_TRUE(totalPreludeMoves > 0);
    CHECK_TRUE(totalTwoPreludeMoves > 0);
}

TEST_GROUP(OctiEngineSuperProngVariants)
{
};

TEST(OctiEngineSuperProngVariants, ChooseMoveFindsWinningLineRequiringSuperProngBonusCapture)
{
    IGNORE_ALL_LEAKS_IN_TEST();

    // Constructed 2P_1B_SP position:
    // Player A's bases are 33 (2,2), 53 (4,2), 73 (6,2);
    // Player B's bases are 37 (2,6), 57 (4,6), 77 (6,6); basesToWin = 1.
    // - A35ac! at 35 (2,4) has N normal prong 'a' and E superprong 'c!'.
    // - B45b at 45 (3,4) has NE normal prong 'b' guarding 56 (4,5) via 45b-67x.
    // - B15d at 15 (0,4) has SE normal prong 'd' racing to A's base 33 (15d-24, 24d-33),
    //   and is West of A35ac! so it cannot be captured.
    // - A81g! and A91g! on row y=0 generate >60 2-prelude combinations
    //   (35ac!-55x; 81/91g!-...; +prong) and >20 1-prelude +prong combinations.
    // The unique winning move for Player A is "35ac!-55x;55ac!-56":
    //   1) Superprong bonus jump 35ac!-55x captures B45b and lands on 55 (4,4).
    //   2) Regular action 55ac!-56 steps North with normal prong 'a' to 56 (4,5),
    //      threatening 56ac!-57 on ply 3.
    // Every other move loses to B45b->67x (capturing A56) or B15d->24->33 (on ply 4).
    octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_1);
    board.fromPositionString(
        "2:1:1:0:P:A:-:0:0,0,2,0:0,0,0,0:A81g!,A91g!,B15d,A35ac!,B45b");
    octi::OctiGame *game = board.getGame();
    CHECK_TRUE(game != nullptr);

    std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
    std::vector<octi::OctiMove> filtered = octi::filterDominatedMoves(board, octi::PLAYER_A, legalMoves);

    bool foundCapturingWin = false;
    bool foundDominatedNonCapture = false;
    for (const auto &m : filtered) {
        std::string s = m.asString();
        if (s == "35ac!-55x;55ac!-56") {
            foundCapturingWin = true;
        } else if (s == "35ac!-55;55ac!-56") {
            foundDominatedNonCapture = true;
        }
    }
    CHECK_TRUE(foundCapturingWin);
    CHECK_FALSE(foundDominatedNonCapture);

    // 1. Depth-3 search finds the unique winning bonus-capture line
    {
        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL, 0xE2B0701ULL);
        octi::BotTuningProfile prof = bot.tuningProfile();
        prof.useOpeningBook = false;
        prof.maxSearchDepth = 3;
        prof.maxQuiesceDepth = 2;
        prof.quiesceBranchCap = 6;
        prof.rootBranchCap = 40;
        prof.treeBranchCap = 24;
        prof.lazySmpThreads = 1;
        prof.rootScoreJitter = 0;
        bot.setTuningProfile(prof);
        bot.setTimeBudgetMs(2000);

        octi::BotSearchStats stats;
        std::string chosen = bot.chooseMove(board, &stats);
        STRCMP_EQUAL("35ac!-55x;55ac!-56", chosen.c_str());
        CHECK_TRUE(stats.bestScore > 900000);
    }

    // 2. Shallow 6-cap / quiescence regime ranks the capturing prelude + base-threat
    //    continuation inside top 6 and solves the win
    {
        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL, 0xE2B0702ULL);
        octi::BotTuningProfile prof = bot.tuningProfile();
        prof.useOpeningBook = false;
        prof.maxSearchDepth = 3;
        prof.maxQuiesceDepth = 2;
        prof.quiesceBranchCap = 6;
        prof.rootBranchCap = 6;
        prof.treeBranchCap = 6;
        prof.lazySmpThreads = 1;
        prof.rootScoreJitter = 0;
        bot.setTuningProfile(prof);
        bot.setTimeBudgetMs(2000);

        octi::BotSearchStats stats;
        std::string chosen = bot.chooseMove(board, &stats);
        STRCMP_EQUAL("35ac!-55x;55ac!-56", chosen.c_str());
    }
}

TEST(OctiEngineSuperProngVariants, AllThirteenVariantsTenGamesEachCompleteAndPlayLegalMoves)
{
    IGNORE_ALL_LEAKS_IN_TEST();

    // Verify superprong bonus-capture selection before running the 130-game suite
    {
        octi::OctiBoard spBoard(octi::GAME_MODE_2, true, false, octi::GAME_BASES_1);
        spBoard.fromPositionString(
            "2:1:1:0:P:A:-:0:0,0,2,0:0,0,0,0:A81g!,A91g!,B15d,A35ac!,B45b");
        octi::OctiBotEngine spBot(octi::BotDifficulty::CASUAL, 0xE20000ULL);
        octi::BotTuningProfile spProf = spBot.tuningProfile();
        spProf.useOpeningBook = false;
        spProf.maxSearchDepth = 3;
        spProf.rootScoreJitter = 0;
        spBot.setTuningProfile(spProf);
        spBot.setTimeBudgetMs(2000);
        STRCMP_EQUAL("35ac!-55x;55ac!-56", spBot.chooseMove(spBoard).c_str());
    }

    struct VariantSpec {
        const char *id;
        octi::OctiGameMode mode;
        bool superProngs;
        bool edgeless;
        octi::OctiGameBases bases;
    };

    const VariantSpec kVariants[13] = {
        { "2P_3B",      octi::GAME_MODE_2,   false, false, octi::GAME_BASES_3 },
        { "2P_3B_SP",   octi::GAME_MODE_2,   true,  false, octi::GAME_BASES_3 },
        { "2P_3B_ED",   octi::GAME_MODE_2,   false, true,  octi::GAME_BASES_3 },
        { "2P_3B_SPED", octi::GAME_MODE_2,   true,  true,  octi::GAME_BASES_3 },
        { "2P_1B",      octi::GAME_MODE_2,   false, false, octi::GAME_BASES_1 },
        { "2P_1B_SP",   octi::GAME_MODE_2,   true,  false, octi::GAME_BASES_1 },
        { "2P_1B_ED",   octi::GAME_MODE_2,   false, true,  octi::GAME_BASES_1 },
        { "2P_1B_SPED", octi::GAME_MODE_2,   true,  true,  octi::GAME_BASES_1 },
        { "4P",         octi::GAME_MODE_4,   false, false, octi::GAME_BASES_3 },
        { "4P_SP",      octi::GAME_MODE_4,   true,  false, octi::GAME_BASES_3 },
        { "4P_ED",      octi::GAME_MODE_4,   false, true,  octi::GAME_BASES_3 },
        { "4P_SPED",    octi::GAME_MODE_4,   true,  true,  octi::GAME_BASES_3 },
        { "OFK",        octi::GAME_MODE_OFK, false, false, octi::GAME_BASES_3 }
    };

    struct VariantSummary {
        std::string id;
        int games = 0;
        int finished = 0;
        int totalPlies = 0;
        int preludeMoves = 0;
        int capturingPreludeMoves = 0;
    };

    std::vector<std::string> jsonlLines;
    jsonlLines.reserve(130);
    std::vector<VariantSummary> summaries(13);

    int totalGamesCompleted = 0;
    int totalSuperProngPreludeMovesPlayed = 0;

#if defined(__SANITIZE_ADDRESS__)
    const bool isAsanBuild = true;
#else
    const bool isAsanBuild = false;
#endif
    const bool forceFull130 = (std::getenv("OCTI_E2_GAMES_JSONL_OUT") != nullptr);
    const int gamesPerVariant = (isAsanBuild && !forceFull130) ? 1 : 10;
    const int maxPliesPerGame = (isAsanBuild && !forceFull130) ? 40 : 300;

    for (std::size_t vIdx = 0; vIdx < 13; ++vIdx) {
        const VariantSpec &vs = kVariants[vIdx];
        summaries[vIdx].id = vs.id;

        for (int g = 0; g < gamesPerVariant; ++g) {
            octi::OctiBoard board(vs.mode, vs.superProngs, vs.edgeless, vs.bases);
            octi::OctiGame *game = board.getGame();
            CHECK_TRUE(game != nullptr);
            game->start(octi::PLAYER_A);

            std::uint64_t seed = 0xE2202600ULL + static_cast<std::uint64_t>(vIdx * 31 + g * 3 + 1);
            octi::OctiBotEngine bot(octi::BotDifficulty::CASUAL, seed);
            octi::BotTuningProfile prof = bot.tuningProfile();
            prof.useOpeningBook = false;
            prof.maxSearchDepth = 3;
            prof.maxQuiesceDepth = 0;
            prof.quiesceBranchCap = 0;
            prof.rootBranchCap = 8;
            prof.treeBranchCap = 6;
            prof.lazySmpThreads = 1;
            prof.compositeMoveMaxPly = -1;
            prof.rootScoreJitter = 60;
            bot.setTuningProfile(prof);

            int pliesPlayed = 0;
            int gamePreludeMoves = 0;
            int gameCapturingPreludeMoves = 0;
            int minDepthReached = 3;

            while (pliesPlayed < maxPliesPerGame && !game->isGameOver()) {
                std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
                if (legalMoves.empty()) {
                    break;
                }

                octi::BotSearchStats stats;
                std::string chosenStr = bot.chooseMove(board, &stats);
                CHECK_FALSE(chosenStr.empty());
                if (stats.depthReached > 0 && stats.depthReached < minDepthReached) {
                    minDepthReached = stats.depthReached;
                }

                octi::OctiMove chosenMove(chosenStr);
                std::string canonicalChosen = chosenMove.asString();
                bool inLegalMoves = false;
                for (const auto &lm : legalMoves) {
                    if (lm == chosenMove || lm.asString() == canonicalChosen) {
                        inLegalMoves = true;
                        break;
                    }
                }
                CHECK_TEXT(inLegalMoves,
                           (std::string("Played move not in generateLegalMoves for ") +
                            vs.id + ": " + canonicalChosen).c_str());

                bool hasPrelude = false;
                bool hasCapturingPrelude = false;
                for (const auto &op : chosenMove.getOps()) {
                    if (op.isSuperProngBonusMove) {
                        hasPrelude = true;
                        for (const auto &h : op.hops) {
                            if (h.capture) {
                                hasCapturingPrelude = true;
                            }
                        }
                    }
                }
                if (hasPrelude) {
                    ++gamePreludeMoves;
                    ++totalSuperProngPreludeMovesPlayed;
                }
                if (hasCapturingPrelude) {
                    ++gameCapturingPreludeMoves;
                }

                game->move(chosenMove);
                ++pliesPlayed;
            }

            CHECK_TRUE(pliesPlayed > 0);
            CHECK_TRUE(pliesPlayed <= maxPliesPerGame);

            ++totalGamesCompleted;
            summaries[vIdx].games++;
            if (game->isGameOver()) {
                summaries[vIdx].finished++;
            }
            summaries[vIdx].totalPlies += pliesPlayed;
            summaries[vIdx].preludeMoves += gamePreludeMoves;
            summaries[vIdx].capturingPreludeMoves += gameCapturingPreludeMoves;

            nlohmann::json rec;
            rec["variant"] = vs.id;
            rec["game_index"] = g;
            rec["seed"] = seed;
            rec["search_depth"] = 3;
            rec["max_plies"] = maxPliesPerGame;
            rec["plies_played"] = pliesPlayed;
            rec["game_over"] = game->isGameOver();
            rec["winner"] = static_cast<int>(game->getWinner());
            rec["prelude_moves"] = gamePreludeMoves;
            rec["capturing_prelude_moves"] = gameCapturingPreludeMoves;
            jsonlLines.push_back(rec.dump());
        }
    }

    CHECK_EQUAL(13 * gamesPerVariant, totalGamesCompleted);
    CHECK_TRUE(totalSuperProngPreludeMovesPlayed > 0);

    const char *envJsonl = std::getenv("OCTI_E2_GAMES_JSONL_OUT");
    const char *envSummary = std::getenv("OCTI_E2_GAMES_SUMMARY_OUT");
    std::string jsonlPath = envJsonl ? envJsonl : "";
    std::string summaryPath = envSummary ? envSummary : "";
    if (!isAsanBuild && jsonlPath.empty() &&
        std::filesystem::exists("plan/plan_sdk_bot_all_variants.artifacts/e2/anchors.md")) {
        jsonlPath = "plan/plan_sdk_bot_all_variants.artifacts/e2/games_130.jsonl";
        summaryPath = "plan/plan_sdk_bot_all_variants.artifacts/e2/games_130_summary.md";
    }
    if (!jsonlPath.empty()) {
        std::ofstream jf(jsonlPath, std::ios::out | std::ios::trunc);
        for (const auto &line : jsonlLines) {
            jf << line << "\n";
        }
    }
    if (!summaryPath.empty()) {
        std::ofstream sf(summaryPath, std::ios::out | std::ios::trunc);
        sf << "# Envelope 2 — 130 Engine-vs-Engine Games Summary (13 Variants x 10 Games, Depth 3, Max 300 Plies)\n\n";
        sf << "| Variant | Games | Terminated <= 300 Plies | Total Plies | Prelude Moves | Capturing Prelude Moves |\n";
        sf << "|---|---:|---:|---:|---:|---:|\n";
        for (const auto &s : summaries) {
            sf << "| `" << s.id << "` | " << s.games << " | " << s.finished
               << " | " << s.totalPlies << " | " << s.preludeMoves
               << " | " << s.capturingPreludeMoves << " |\n";
        }
    }
}

// ===========================================================================
// Envelope 3: Opening Book All-Variants, 4P Key Fix & bookSelectionWindow
// ===========================================================================

TEST_GROUP(OctiOpeningBookAllVariants)
{
};

TEST(OctiOpeningBookAllVariants, BookSelectionWindowDeterministicAndRandomized)
{
    octi::OctiBoard b2p3(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    b2p3.getGame()->start(octi::PLAYER_A);

    // Built-in AI profiles have bookSelectionWindow > 0 even when rootScoreJitter == 0
    CHECK_TRUE(octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::CASUAL).bookSelectionWindow > 0);
    CHECK_TRUE(octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::TACTICAL).bookSelectionWindow > 0);
    CHECK_TRUE(octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::MASTER).bookSelectionWindow > 0);
    CHECK_TRUE(octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::EXPERT).bookSelectionWindow > 0);
    CHECK_TRUE(octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::GRANDMASTER).bookSelectionWindow > 0);
    LONGS_EQUAL(0, octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::EXPERT).rootScoreJitter);
    LONGS_EQUAL(0, octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::GRANDMASTER).rootScoreJitter);

    // Default-constructed BotTuningProfile (bench / teacher / deterministic tests) has bookSelectionWindow == 0
    octi::BotTuningProfile defaultProf;
    LONGS_EQUAL(0, defaultProf.bookSelectionWindow);

    // 1. bookSelectionWindow = 0 always returns the exact same move over 200 probes
    octi::OctiBotEngine detBot(octi::BotDifficulty::GRANDMASTER, 0x5EED2026ULL);
    octi::BotTuningProfile detProf = detBot.tuningProfile();
    detProf.useOpeningBook = true;
    detProf.rootScoreJitter = 50; // Even with non-zero rootScoreJitter, bookSelectionWindow = 0 is deterministic
    detProf.bookSelectionWindow = 0;
    detBot.setTuningProfile(detProf);

    std::string firstDetMove;
    CHECK_TRUE(detBot.probeOpeningBook(b2p3, firstDetMove));
    CHECK_FALSE(firstDetMove.empty());
    for (int i = 0; i < 200; ++i) {
        std::string mv;
        CHECK_TRUE(detBot.probeOpeningBook(b2p3, mv));
        STRCMP_EQUAL(firstDetMove.c_str(), mv.c_str());
    }

    // 2. bookSelectionWindow > 0 (with rootScoreJitter = 0, as in Expert/Master/Grandmaster)
    // returns more than one distinct move over 200 probes of a position with near-equal candidates
    octi::OctiBotEngine varBot(octi::BotDifficulty::GRANDMASTER, 0x5EED2026ULL);
    octi::BotTuningProfile varProf = varBot.tuningProfile();
    CHECK_EQUAL(0, varProf.rootScoreJitter);
    CHECK_TRUE(varProf.bookSelectionWindow > 0);

    std::set<std::string> distinctMoves;
    for (int i = 0; i < 200; ++i) {
        std::string mv;
        CHECK_TRUE(varBot.probeOpeningBook(b2p3, mv));
        CHECK_FALSE(mv.empty());
        distinctMoves.insert(mv);
    }
    CHECK_TRUE(distinctMoves.size() >= 2);
}

TEST(OctiOpeningBookAllVariants, FourPlayerBasesToWinKeyInvarianceAndStandardEntriesPreserved)
{
    // 1. 4P table with GAME_BASES_1 and with GAME_BASES_3 produce identical canonical opening hashes
    octi::OctiBoard b4p1(octi::GAME_MODE_4, false, false, octi::GAME_BASES_1);
    b4p1.getGame()->start(octi::PLAYER_A);

    octi::OctiBoard b4p3(octi::GAME_MODE_4, false, false, octi::GAME_BASES_3);
    b4p3.getGame()->start(octi::PLAYER_A);

    std::uint64_t key4p1 = octi::OctiBotEngine::computeCanonicalOpeningHash(b4p1);
    std::uint64_t key4p3 = octi::OctiBotEngine::computeCanonicalOpeningHash(b4p3);
    CHECK_EQUAL(key4p1, key4p3);

    // Transposition-table static hash remains unchanged
    CHECK_EQUAL(0x5C3D5244B85BC3A6ULL, octi::OctiBotEngine::computePositionHashStatic(b4p3));

    // Both 4P GAME_BASES_1 and GAME_BASES_3 hit the opening book from initial position
    octi::OctiBotEngine bot(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    octi::BotTuningProfile detProf = bot.tuningProfile();
    detProf.bookSelectionWindow = 0;
    bot.setTuningProfile(detProf);

    std::string move4p1;
    std::string move4p3;
    int score4p1 = 0, depth4p1 = 0;
    int score4p3 = 0, depth4p3 = 0;
    CHECK_TRUE(bot.probeOpeningBook(b4p1, move4p1, &score4p1, &depth4p1));
    CHECK_TRUE(bot.probeOpeningBook(b4p3, move4p3, &score4p3, &depth4p3));
    STRCMP_EQUAL("33+a", move4p1.c_str());
    STRCMP_EQUAL("33+a", move4p3.c_str());
    LONGS_EQUAL(score4p1, score4p3);
    LONGS_EQUAL(8, depth4p1);
    LONGS_EQUAL(8, depth4p3);

    // Also verify ply 1..4 4P hits with both GAME_BASES_1 and GAME_BASES_3
    b4p1.getGame()->move("33+a");
    b4p3.getGame()->move("33+a");
    CHECK_EQUAL(octi::OctiBotEngine::computeCanonicalOpeningHash(b4p1),
                octi::OctiBotEngine::computeCanonicalOpeningHash(b4p3));
    std::string reply4p1, reply4p3;
    CHECK_TRUE(bot.probeOpeningBook(b4p1, reply4p1));
    CHECK_TRUE(bot.probeOpeningBook(b4p3, reply4p3));
    STRCMP_EQUAL(reply4p1.c_str(), reply4p3.c_str());

    // 2. Existing standard-variant (2P_3B, 2P_1B, OFK) keys and entries unchanged
    octi::OctiBoard b2p3(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    b2p3.getGame()->start(octi::PLAYER_A);
    CHECK_EQUAL(0x3BCD8E7CEAE1563EULL, octi::OctiBotEngine::computePositionHashStatic(b2p3));
    CHECK_EQUAL(0x3BCD8E7CEAE1563EULL, octi::OctiBotEngine::computeCanonicalOpeningHash(b2p3));

    octi::OctiBoard b2p1(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
    b2p1.getGame()->start(octi::PLAYER_A);
    CHECK_EQUAL(0x26BE80163B6578BBULL, octi::OctiBotEngine::computePositionHashStatic(b2p1));
    CHECK_EQUAL(0x26BE80163B6578BBULL, octi::OctiBotEngine::computeCanonicalOpeningHash(b2p1));

    octi::OctiBoard bOfk(octi::GAME_MODE_OFK, false, false, octi::GAME_BASES_1);
    bOfk.getGame()->start(octi::PLAYER_A);
    CHECK_EQUAL(0xBF20205531084330ULL, octi::OctiBotEngine::computeCanonicalOpeningHash(bOfk));

    LONGS_EQUAL(3714, static_cast<long>(octi::g_openingBookEntryCount));
}

TEST(OctiOpeningBookAllVariants, CompiledOpeningBookHitsAll13VariantsAndHumanLossLines)
{
    octi::OctiBotEngine bot(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    octi::BotTuningProfile detProf = bot.tuningProfile();
    detProf.useOpeningBook = true;
    detProf.rootScoreJitter = 0;
    detProf.bookSelectionWindow = 0;
    bot.setTuningProfile(detProf);

    // 1. Compiled-in book hits at ply 0 across all 13 variants at depth 8,
    //    and deeper Task 3.3 plies hit across all 9 new variants (ply 1 for SP/SPED, plies 0..3 for ED)
    struct VariantInitCase {
        octi::OctiGameMode mode;
        octi::OctiGameBases bases;
        bool sp;
        bool ed;
    };
    const VariantInitCase all13[] = {
        { octi::GAME_MODE_2,   octi::GAME_BASES_3, false, false },
        { octi::GAME_MODE_2,   octi::GAME_BASES_3, true,  false },
        { octi::GAME_MODE_2,   octi::GAME_BASES_3, false, true  },
        { octi::GAME_MODE_2,   octi::GAME_BASES_3, true,  true  },
        { octi::GAME_MODE_2,   octi::GAME_BASES_1, false, false },
        { octi::GAME_MODE_2,   octi::GAME_BASES_1, true,  false },
        { octi::GAME_MODE_2,   octi::GAME_BASES_1, false, true  },
        { octi::GAME_MODE_2,   octi::GAME_BASES_1, true,  true  },
        { octi::GAME_MODE_4,   octi::GAME_BASES_3, false, false },
        { octi::GAME_MODE_4,   octi::GAME_BASES_3, true,  false },
        { octi::GAME_MODE_4,   octi::GAME_BASES_3, false, true  },
        { octi::GAME_MODE_4,   octi::GAME_BASES_3, true,  true  },
        { octi::GAME_MODE_OFK, octi::GAME_BASES_1, false, false }
    };
    for (const auto &vc : all13) {
        octi::OctiBoard b(vc.mode, vc.sp, vc.ed, vc.bases);
        b.getGame()->start(octi::PLAYER_A);
        const int targetPlies = (!vc.sp && !vc.ed) ? 5 : (vc.sp ? 2 : 4);
        for (int p = 0; p < targetPlies; ++p) {
            std::string mv;
            int sc = 0;
            int dp = 0;
            CHECK_TRUE(bot.probeOpeningBook(b, mv, &sc, &dp));
            CHECK_FALSE(mv.empty());
            LONGS_EQUAL(8, dp);
            b.getGame()->move(mv);
        }
    }

    // 2. All 4 4P variants hit identically with GAME_BASES_1 and GAME_BASES_3
    for (bool sp : {false, true}) {
        for (bool ed : {false, true}) {
            octi::OctiBoard b1(octi::GAME_MODE_4, sp, ed, octi::GAME_BASES_1);
            b1.getGame()->start(octi::PLAYER_A);
            octi::OctiBoard b3(octi::GAME_MODE_4, sp, ed, octi::GAME_BASES_3);
            b3.getGame()->start(octi::PLAYER_A);
            CHECK_EQUAL(octi::OctiBotEngine::computeCanonicalOpeningHash(b1),
                        octi::OctiBotEngine::computeCanonicalOpeningHash(b3));
            std::string m1, m3;
            int s1 = 0, s3 = 0, d1 = 0, d3 = 0;
            CHECK_TRUE(bot.probeOpeningBook(b1, m1, &s1, &d1));
            CHECK_TRUE(bot.probeOpeningBook(b3, m3, &s3, &d3));
            STRCMP_EQUAL(m1.c_str(), m3.c_str());
            LONGS_EQUAL(s1, s3);
            LONGS_EQUAL(8, d1);
            LONGS_EQUAL(8, d3);
        }
    }

    // 3. Re-searched human-loss book lines return the updated depth-8 top moves
    // (a) 2P 1-Base ply 1 after 33+b (Don Green game1 & game7): replaces 57+d (-3350) with 37+e (-3100)
    {
        octi::OctiBoard b(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
        b.getGame()->start(octi::PLAYER_A);
        b.getGame()->move("33+b");
        std::string mv;
        int sc = 0, dp = 0;
        CHECK_TRUE(bot.probeOpeningBook(b, mv, &sc, &dp));
        CHECK_TRUE(mv != "57+d");
        LONGS_EQUAL(-3100, sc);
        LONGS_EQUAL(8, dp);
    }
    // (b) OFK ply 1 after 32+b (Don Green game2): replaces 26+d (-4143) with 36+e/46+e (-3343)
    {
        octi::OctiBoard b(octi::GAME_MODE_OFK, false, false, octi::GAME_BASES_1);
        b.getGame()->start(octi::PLAYER_A);
        b.getGame()->move("32+b");
        std::string mv;
        int sc = 0, dp = 0;
        CHECK_TRUE(bot.probeOpeningBook(b, mv, &sc, &dp));
        CHECK_TRUE(mv != "26+d");
        LONGS_EQUAL(-3343, sc);
        LONGS_EQUAL(8, dp);
    }
    // (c) OFK ply 2 after 32+a 36+d (Don Green game4): replaces 22+b (-28) with 42+a (-640)
    {
        octi::OctiBoard b(octi::GAME_MODE_OFK, false, false, octi::GAME_BASES_1);
        b.getGame()->start(octi::PLAYER_A);
        b.getGame()->move("32+a");
        b.getGame()->move("36+d");
        std::string mv;
        int sc = 0, dp = 0;
        CHECK_TRUE(bot.probeOpeningBook(b, mv, &sc, &dp));
        CHECK_TRUE(mv != "22+b");
        LONGS_EQUAL(-640, sc);
        LONGS_EQUAL(8, dp);
    }
    // (d) OFK ply 2 after 32+a 46+f (Don Green game0 & game3): updated score -461 (with 42+h tied at -461)
    {
        octi::OctiBoard b(octi::GAME_MODE_OFK, false, false, octi::GAME_BASES_1);
        b.getGame()->start(octi::PLAYER_A);
        b.getGame()->move("32+a");
        b.getGame()->move("46+f");
        std::string mv;
        int sc = 0, dp = 0;
        CHECK_TRUE(bot.probeOpeningBook(b, mv, &sc, &dp));
        CHECK_TRUE(mv == "22+b" || mv == "42+h");
        LONGS_EQUAL(-461, sc);
        LONGS_EQUAL(8, dp);
    }

    // 4. Re-searched 4P Seat C/D entries reflect the Envelope 1b prong-direction fix
    // Negative control 0x22430FCFC057D77A (4P ply 3, Seat D to move after 33+b 37+g 77+e):
    // returns 73+a (-2097) instead of the pre-Envelope-1b biased move 73+f (-1396).
    {
        octi::OctiBoard b(octi::GAME_MODE_4, false, false, octi::GAME_BASES_3);
        b.getGame()->start(octi::PLAYER_A);
        b.getGame()->move("33+b");
        b.getGame()->move("37+g");
        b.getGame()->move("77+e");
        CHECK_EQUAL(0x22430FCFC057D77AULL, octi::OctiBotEngine::computeCanonicalOpeningHash(b));
        std::string mv;
        int sc = 0, dp = 0;
        CHECK_TRUE(bot.probeOpeningBook(b, mv, &sc, &dp));
        STRCMP_EQUAL("73+a", mv.c_str());
        LONGS_EQUAL(-2097, sc);
        LONGS_EQUAL(8, dp);
    }
    // Shared 4P ply 3, Seat D to move after 33+c 37+f 77+d (0xA24890BCA3FB4122):
    // returns 73+a (-1577) instead of the pre-Envelope-1b biased move 73+f (-976).
    {
        octi::OctiBoard b(octi::GAME_MODE_4, false, false, octi::GAME_BASES_3);
        b.getGame()->start(octi::PLAYER_A);
        b.getGame()->move("33+c");
        b.getGame()->move("37+f");
        b.getGame()->move("77+d");
        CHECK_EQUAL(0xA24890BCA3FB4122ULL, octi::OctiBotEngine::computeCanonicalOpeningHash(b));
        std::string mv;
        int sc = 0, dp = 0;
        CHECK_TRUE(bot.probeOpeningBook(b, mv, &sc, &dp));
        STRCMP_EQUAL("73+a", mv.c_str());
        LONGS_EQUAL(-1577, sc);
        LONGS_EQUAL(8, dp);
    }
    // 4P ply 2, Seat C to move after 33+c 37+c (0xAB2530E97A188B07):
    // returns 77+e (0) instead of the pre-Envelope-1b biased move 77+h (0).
    {
        octi::OctiBoard b(octi::GAME_MODE_4, false, false, octi::GAME_BASES_3);
        b.getGame()->start(octi::PLAYER_A);
        b.getGame()->move("33+c");
        b.getGame()->move("37+c");
        CHECK_EQUAL(0xAB2530E97A188B07ULL, octi::OctiBotEngine::computeCanonicalOpeningHash(b));
        std::string mv;
        int sc = 0, dp = 0;
        CHECK_TRUE(bot.probeOpeningBook(b, mv, &sc, &dp));
        STRCMP_EQUAL("77+e", mv.c_str());
        LONGS_EQUAL(0, sc);
        LONGS_EQUAL(8, dp);
    }
}

TEST(OctiOpeningBookAllVariants, BookGenNewVariantModesFlagsAndAll13Expansion)
{
    std::string bookGenBin;
    std::error_code selfEc;
    std::filesystem::path selfDir = std::filesystem::canonical("/proc/self/exe", selfEc).parent_path();
    if (!selfEc && std::filesystem::exists(selfDir / "octi_book_gen")) {
        bookGenBin = (selfDir / "octi_book_gen").string();
    } else if (std::filesystem::exists("build/octi_book_gen")) {
        bookGenBin = "./build/octi_book_gen";
    } else if (std::filesystem::exists("build/asan/octi_book_gen")) {
        bookGenBin = "./build/asan/octi_book_gen";
    } else if (std::filesystem::exists("./octi_book_gen")) {
        bookGenBin = "./octi_book_gen";
    } else if (std::filesystem::exists("core/build/octi_book_gen")) {
        bookGenBin = "./core/build/octi_book_gen";
    }
    CHECK_FALSE(bookGenBin.empty());

    std::filesystem::path tmpDir =
        std::filesystem::temp_directory_path() / "octi_book_gen_all13_test";
    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
    std::filesystem::create_directories(tmpDir, ec);

    std::string jsonSp = (tmpDir / "sp.json").string();
    std::string hxxSp = (tmpDir / "sp.hxx").string();
    std::string javaSp = (tmpDir / "sp.java").string();

    // 1. Test --mode 2P_3B --super-prongs
    std::string cmdSp =
        bookGenBin + " --threads 1 --no-resume --mode 2P_3B --super-prongs --max-ply 1 --search-depth 1"
        " --out-json \"" + jsonSp + "\" --out-header \"" + hxxSp + "\" --out-java \"" + javaSp + "\" > /dev/null 2>&1";
    CHECK_EQUAL(0, std::system(cmdSp.c_str()));

    octi::OctiBoard b2p3Sp(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    b2p3Sp.getGame()->start(octi::PLAYER_A);
    std::uint64_t key2p3Sp = octi::OctiBotEngine::computeCanonicalOpeningHash(b2p3Sp);

    {
        std::ifstream in(jsonSp);
        nlohmann::json j;
        in >> j;
        LONGS_EQUAL(1, j["entryCount"].get<int>());
        STRCMP_EQUAL("2P_3B_SP", j["entries"][0]["variant"].get<std::string>().c_str());
        std::uint64_t parsedKey = std::stoull(j["entries"][0]["canonicalKey"].get<std::string>(), nullptr, 16);
        CHECK_EQUAL(key2p3Sp, parsedKey);
    }

    // 2. Test --mode all13 --max-ply 1 --search-depth 1 expands all 13 variants
    std::string json13 = (tmpDir / "all13.json").string();
    std::string hxx13 = (tmpDir / "all13.hxx").string();
    std::string java13 = (tmpDir / "all13.java").string();
    std::string cmd13 =
        bookGenBin + " --threads 1 --no-resume --mode all13 --max-ply 1 --search-depth 1"
        " --out-json \"" + json13 + "\" --out-header \"" + hxx13 + "\" --out-java \"" + java13 + "\" > /dev/null 2>&1";
    CHECK_EQUAL(0, std::system(cmd13.c_str()));

    std::ifstream in13(json13);
    nlohmann::json j13;
    in13 >> j13;
    LONGS_EQUAL(13, j13["entryCount"].get<int>());

    // Build an in-memory OpeningBookEntry table from j13 and verify all 13 variants
    // (plus 4P with GAME_BASES_1 and GAME_BASES_3) hit!
    std::vector<std::string> moveStrings;
    moveStrings.reserve(39);
    std::vector<octi::OpeningBookEntry> entries;
    for (const auto &item : j13["entries"]) {
        octi::OpeningBookEntry e{};
        e.canonicalKey = std::stoull(item["canonicalKey"].get<std::string>(), nullptr, 16);
        e.depth = static_cast<std::uint8_t>(item["depth"].get<int>());
        const auto &cands = item["candidates"];
        e.numCandidates = static_cast<std::uint8_t>(std::min<std::size_t>(3, cands.size()));
        for (std::size_t c = 0; c < 3; ++c) {
            if (c < e.numCandidates) {
                moveStrings.push_back(cands[c]["move"].get<std::string>());
                e.candidates[c].move = moveStrings.back().c_str();
                e.candidates[c].score = static_cast<std::int16_t>(cands[c]["score"].get<int>());
            } else {
                e.candidates[c].move = nullptr;
                e.candidates[c].score = 0;
            }
        }
        entries.push_back(e);
    }
    std::sort(entries.begin(), entries.end(),
              [](const octi::OpeningBookEntry &a, const octi::OpeningBookEntry &b) {
                  return a.canonicalKey < b.canonicalKey;
              });

    octi::BotTuningProfile prof;
    prof.useOpeningBook = true;
    prof.bookSelectionWindow = 0;
    std::uint64_t rng = 0x5EED2026ULL;

    struct VariantCheck {
        octi::OctiGameMode mode;
        octi::OctiGameBases bases;
        bool sp;
        bool ed;
    };
    const VariantCheck checks[] = {
        { octi::GAME_MODE_2,   octi::GAME_BASES_3, false, false },
        { octi::GAME_MODE_2,   octi::GAME_BASES_3, true,  false },
        { octi::GAME_MODE_2,   octi::GAME_BASES_3, false, true  },
        { octi::GAME_MODE_2,   octi::GAME_BASES_3, true,  true  },
        { octi::GAME_MODE_2,   octi::GAME_BASES_1, false, false },
        { octi::GAME_MODE_2,   octi::GAME_BASES_1, true,  false },
        { octi::GAME_MODE_2,   octi::GAME_BASES_1, false, true  },
        { octi::GAME_MODE_2,   octi::GAME_BASES_1, true,  true  },
        { octi::GAME_MODE_4,   octi::GAME_BASES_3, false, false },
        { octi::GAME_MODE_4,   octi::GAME_BASES_1, false, false },
        { octi::GAME_MODE_4,   octi::GAME_BASES_3, true,  false },
        { octi::GAME_MODE_4,   octi::GAME_BASES_1, true,  false },
        { octi::GAME_MODE_4,   octi::GAME_BASES_3, false, true  },
        { octi::GAME_MODE_4,   octi::GAME_BASES_1, false, true  },
        { octi::GAME_MODE_4,   octi::GAME_BASES_3, true,  true  },
        { octi::GAME_MODE_4,   octi::GAME_BASES_1, true,  true  },
        { octi::GAME_MODE_OFK, octi::GAME_BASES_1, false, false }
    };
    for (const auto &vc : checks) {
        octi::OctiBoard b(vc.mode, vc.sp, vc.ed, vc.bases);
        b.getGame()->start(octi::PLAYER_A);
        std::string outMove;
        CHECK_TRUE(octi::OctiBotEngine::probeOpeningBookTable(
            b, entries.data(), entries.size(), prof, rng, outMove));
        CHECK_FALSE(outMove.empty());
    }

    std::filesystem::remove_all(tmpDir, ec);
}

TEST(OctiOpeningBookAllVariants, BookGenFreshEnginePerPositionOrderIndependence)
{
    std::string bookGenBin;
    std::error_code selfEc;
    std::filesystem::path selfDir = std::filesystem::canonical("/proc/self/exe", selfEc).parent_path();
    if (!selfEc && std::filesystem::exists(selfDir / "octi_book_gen")) {
        bookGenBin = (selfDir / "octi_book_gen").string();
    } else if (std::filesystem::exists("build/octi_book_gen")) {
        bookGenBin = "./build/octi_book_gen";
    } else if (std::filesystem::exists("build/asan/octi_book_gen")) {
        bookGenBin = "./build/asan/octi_book_gen";
    } else if (std::filesystem::exists("./octi_book_gen")) {
        bookGenBin = "./octi_book_gen";
    } else if (std::filesystem::exists("core/build/octi_book_gen")) {
        bookGenBin = "./core/build/octi_book_gen";
    }
    CHECK_FALSE(bookGenBin.empty());

    std::filesystem::path tmpDir =
        std::filesystem::temp_directory_path() / "octi_book_gen_order_indep_test";
    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
    std::filesystem::create_directories(tmpDir, ec);

    auto makeBoardStr = [](const std::vector<std::string> &moves, std::string &outKeyHex) -> std::string {
        octi::OctiBoard b(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
        b.getGame()->start(octi::PLAYER_A);
        for (const auto &m : moves) {
            b.getGame()->move(m);
        }
        std::uint64_t k = octi::OctiBotEngine::computeCanonicalOpeningHash(b);
        std::ostringstream oss;
        oss << "0x" << std::uppercase << std::hex
            << std::setw(16) << std::setfill('0') << k;
        outKeyHex = oss.str();
        return b.toPositionString();
    };

    std::string k0, k1, p1Key, p2Key, p3Key;
    std::string pos0 = makeBoardStr({}, k0);
    std::string pos1 = makeBoardStr({"33+h"}, k1);
    std::string posP1 = makeBoardStr({"33+h", "37+d"}, p1Key);
    std::string posP2 = makeBoardStr({"33+b", "37+d"}, p2Key);
    std::string posP3 = makeBoardStr({"53+a", "37+d"}, p3Key);

    STRCMP_EQUAL("0x101EDCB755B8D169", p1Key.c_str());
    STRCMP_EQUAL("0x16E47C32DE61104A", p2Key.c_str());
    STRCMP_EQUAL("0x3B1A0BE82177D5B1", p3Key.c_str());

    auto makeEntryJson = [](const std::string &key, int ply, const std::string &hist, const std::string &pos) {
        nlohmann::json e;
        e["canonicalKey"] = key;
        e["depth"] = 1;
        e["ply"] = ply;
        e["variant"] = "2P 3-Base";
        e["history"] = hist;
        e["position"] = pos;
        e["candidates"] = nlohmann::json::array({{{"move", "33+a"}, {"score", 0}}});
        return e;
    };

    nlohmann::json templateBook;
    templateBook["generator"] = "octi_book_gen";
    templateBook["mode"] = "2P_3B";
    templateBook["maxPly"] = 3;
    templateBook["searchDepth"] = 4;
    templateBook["timeBudgetMs"] = 0;
    templateBook["botBranch"] = 3;
    templateBook["oppBranch"] = 6;
    templateBook["threads"] = 1;
    templateBook["elapsedMs"] = 0;
    templateBook["entries"] = nlohmann::json::array({
        makeEntryJson(k0, 0, "initial", pos0),
        makeEntryJson(k1, 1, "after 33+h", pos1),
        makeEntryJson(p1Key, 2, "after 33+h 37+d", posP1),
        makeEntryJson(p2Key, 2, "after 33+b 37+d", posP2),
        makeEntryJson(p3Key, 2, "after 53+a 37+d", posP3)
    });
    templateBook["entryCount"] = 5;

    auto runKeysOrder = [&](const std::vector<std::string> &keys, const std::string &tag) -> nlohmann::json {
        std::string jsonPath = (tmpDir / (tag + ".json")).string();
        {
            std::ofstream out(jsonPath);
            out << templateBook.dump(2) << "\n";
        }
        std::string cmd = bookGenBin +
            " --re-search-only --mode 2P_3B --max-ply 3 --search-depth 4"
            " --bot-branch 3 --opp-branch 6 --threads 1 --threads-per-worker 1"
            " --out-json \"" + jsonPath + "\" --out-header \"\" --out-java \"\"";
        for (const auto &k : keys) {
            cmd += " --re-search-key " + k;
        }
        cmd += " > /dev/null 2>&1";
        CHECK_EQUAL(0, std::system(cmd.c_str()));

        std::ifstream in(jsonPath);
        nlohmann::json res;
        in >> res;
        nlohmann::json byKey = nlohmann::json::object();
        for (const auto &item : res["entries"]) {
            byKey[item["canonicalKey"].get<std::string>()] = item["candidates"];
        }
        return byKey;
    };

    nlohmann::json freshP1 = runKeysOrder({p1Key}, "fresh_p1");
    nlohmann::json freshP2 = runKeysOrder({p2Key}, "fresh_p2");
    nlohmann::json freshP3 = runKeysOrder({p3Key}, "fresh_p3");
    nlohmann::json batchFwd = runKeysOrder({p1Key, p2Key, p3Key}, "batch_fwd");
    nlohmann::json batchRev = runKeysOrder({p3Key, p2Key, p1Key}, "batch_rev");

    STRCMP_EQUAL(freshP1[p1Key].dump().c_str(), batchFwd[p1Key].dump().c_str());
    STRCMP_EQUAL(freshP1[p1Key].dump().c_str(), batchRev[p1Key].dump().c_str());
    STRCMP_EQUAL(freshP2[p2Key].dump().c_str(), batchFwd[p2Key].dump().c_str());
    STRCMP_EQUAL(freshP2[p2Key].dump().c_str(), batchRev[p2Key].dump().c_str());
    STRCMP_EQUAL(freshP3[p3Key].dump().c_str(), batchFwd[p3Key].dump().c_str());
    STRCMP_EQUAL(freshP3[p3Key].dump().c_str(), batchRev[p3Key].dump().c_str());

    std::filesystem::remove_all(tmpDir, ec);
}

namespace {

struct VariantSpecE3 {
    octi::OctiGameMode mode;
    octi::OctiGameBases bases;
    bool superProngs;
    bool edgeless;
    const char *name;
};

static const VariantSpecE3 kAll13VariantsE3[13] = {
    {octi::GAME_MODE_2,   octi::GAME_BASES_1, false, false, "2P_1B"},
    {octi::GAME_MODE_2,   octi::GAME_BASES_1, false, true,  "2P_1B_E"},
    {octi::GAME_MODE_2,   octi::GAME_BASES_1, true,  false, "2P_1B_S"},
    {octi::GAME_MODE_2,   octi::GAME_BASES_1, true,  true,  "2P_1B_SE"},
    {octi::GAME_MODE_2,   octi::GAME_BASES_3, false, false, "2P_3B"},
    {octi::GAME_MODE_2,   octi::GAME_BASES_3, false, true,  "2P_3B_E"},
    {octi::GAME_MODE_2,   octi::GAME_BASES_3, true,  false, "2P_3B_S"},
    {octi::GAME_MODE_2,   octi::GAME_BASES_3, true,  true,  "2P_3B_SE"},
    {octi::GAME_MODE_4,   octi::GAME_BASES_3, false, false, "4P_3B"},
    {octi::GAME_MODE_4,   octi::GAME_BASES_3, false, true,  "4P_3B_E"},
    {octi::GAME_MODE_4,   octi::GAME_BASES_3, true,  false, "4P_3B_S"},
    {octi::GAME_MODE_4,   octi::GAME_BASES_3, true,  true,  "4P_3B_SE"},
    {octi::GAME_MODE_OFK, octi::GAME_BASES_1, false, false, "OFK"}
};

static inline std::uint64_t splitMix64E3(std::uint64_t &state)
{
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static inline octi::OctiPlayer nextSeatE3(octi::OctiGameMode mode, octi::OctiPlayer p)
{
    if (mode == octi::GAME_MODE_2 || mode == octi::GAME_MODE_OFK) {
        return (p == octi::PLAYER_A) ? octi::PLAYER_B : octi::PLAYER_A;
    }
    switch (p) {
    case octi::PLAYER_A: return octi::PLAYER_B;
    case octi::PLAYER_B: return octi::PLAYER_C;
    case octi::PLAYER_C: return octi::PLAYER_D;
    case octi::PLAYER_D: return octi::PLAYER_A;
    default: return octi::PLAYER_A;
    }
}

static bool isEnemyHomeForE3Test(octi::OctiSquareType sqType,
                                 octi::OctiGameMode mode,
                                 octi::OctiPlayer p)
{
    if (mode == octi::GAME_MODE_2 || mode == octi::GAME_MODE_OFK) {
        if (p == octi::PLAYER_A) return sqType == octi::B_HOME_SQUARE;
        if (p == octi::PLAYER_B) return sqType == octi::A_HOME_SQUARE;
        return false;
    }
    if (mode == octi::GAME_MODE_4) {
        if (p == octi::PLAYER_A || p == octi::PLAYER_C) {
            return sqType == octi::B_HOME_SQUARE || sqType == octi::D_HOME_SQUARE;
        }
        if (p == octi::PLAYER_B || p == octi::PLAYER_D) {
            return sqType == octi::A_HOME_SQUARE || sqType == octi::C_HOME_SQUARE;
        }
    }
    return false;
}

static bool referenceIsTacticalMoveE3(const octi::OctiBoard &board,
                                      octi::OctiGameMode mode,
                                      bool edgeless,
                                      octi::OctiPlayer turn,
                                      const octi::OctiMove &m)
{
    int w = board.getBoardWidth();
    int h = board.getBoardHeight();
    const auto &mOps = m.getOps();
    for (std::size_t opIdx = 0; opIdx < mOps.size(); ++opIdx) {
        const auto &op = mOps[opIdx];
        int cx = op.x0;
        int cy = op.y0;
        for (const auto &hop : op.hops) {
            int dx = hop.x - cx;
            int dy = hop.y - cy;
            if (edgeless) {
                for (int d = -2; d <= 2; d++) {
                    if (((cx + d) % w + w) % w == hop.x) { dx = d; break; }
                }
                for (int d = -2; d <= 2; d++) {
                    if (((cy + d) % h + h) % h == hop.y) { dy = d; break; }
                }
            }
            if (hop.capture && (std::abs(dx) == 2 || std::abs(dy) == 2)) {
                int mx = edgeless ? (((cx + dx / 2) % w + w) % w) : (cx + dx / 2);
                int my = edgeless ? (((cy + dy / 2) % h + h) % h) : (cy + dy / 2);
                if (board.isValidCoord(mx, my)) {
                    const octi::OctiSquare &midSq = board.squareAt(mx, my);
                    if (midSq.hasEnemyOf(turn)) {
                        return true;
                    }
                }
            }
            cx = hop.x;
            cy = hop.y;
        }
        if (!op.hops.empty()) {
            const octi::OctiHop &lastHop = op.hops.back();
            bool continuedLater = false;
            for (std::size_t j = opIdx + 1; j < mOps.size(); ++j) {
                if (!mOps[j].hops.empty() &&
                    mOps[j].x0 == lastHop.x &&
                    mOps[j].y0 == lastHop.y) {
                    continuedLater = true;
                    break;
                }
            }
            if (!continuedLater) {
                const octi::OctiSquare &destSq = board.squareAt(lastHop.x, lastHop.y);
                if (isEnemyHomeForE3Test(destSq.getType(), mode, turn) &&
                    !destSq.isCapturedByTeamOf(turn)) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

TEST_GROUP(OctiEnvelope3SpeedupTest)
{
};

TEST(OctiEnvelope3SpeedupTest, HasAnyLegalMoveMatchesGenerateLegalMovesAndPlayoutEquivalence)
{
    // 1. Explicit edge cases: stalemate, OFK blocked pods, edgeless wraparound,
    //    superprong bonus prelude, and 4P seat skipping / all-seats-blocked stalemate.
    {
        // 2P stalemate: A has a single 0-prong pod at 55, 0 free pods, 0 free prongs, 0 captured pods
        octi::OctiBoard bStale2P(octi::GAME_MODE_2);
        bStale2P.fromPositionString("2:3:0:0:P:A:-:0:0,0,0,0:2,0,10,0:A55,B37e");
        CHECK_FALSE(bStale2P.getGame()->hasAnyLegalMove());
        CHECK_TRUE(bStale2P.getGame()->generateLegalMoves().empty());

        // B moves 37e-36, handing turn to A who has 0 legal moves -> immediate stalemate win for B
        octi::OctiBoard bTrig2P(octi::GAME_MODE_2);
        bTrig2P.fromPositionString("2:3:0:0:P:B:-:0:0,0,0,0:2,0,10,0:A55,B37e");
        bTrig2P.getGame()->move("37e-36");
        CHECK_TRUE(bTrig2P.getGame()->isGameOver());
        CHECK_EQUAL(octi::PLAYER_B, bTrig2P.getGame()->getWinner());
    }
    {
        // OFK blocked pod vs jumpable pod: in OFK, friendly adjacent blocks step and enemy landing blocks jump
        octi::OctiBoard bOfkStale(octi::GAME_MODE_OFK);
        bOfkStale.fromPositionString("OFK:1:0:0:P:A:-:0:0,0,0,0:0,0,5,0:A22a,A23,B24,B56e");
        // Wait: 22a has 1 prong (N -> 23 occupied by A23, jump landing 24 occupied by B24), 0 freeProngs -> 0 moves!
        CHECK_FALSE(bOfkStale.getGame()->hasAnyLegalMove());
        CHECK_TRUE(bOfkStale.getGame()->generateLegalMoves().empty());

        // Vacate 24 so 22a can jump over A23 to 24
        octi::OctiBoard bOfkJump(octi::GAME_MODE_OFK);
        bOfkJump.fromPositionString("OFK:1:0:0:P:A:-:0:0,0,0,0:0,0,5,0:A22a,A23,B56e");
        CHECK_TRUE(bOfkJump.getGame()->hasAnyLegalMove());
        CHECK_FALSE(bOfkJump.getGame()->generateLegalMoves().empty());
    }
    {
        // Edgeless wraparound step/jump from edge (1,5) West ('g') to (9,5) when 8 normal prongs prevent reposition
        octi::OctiBoard bEdgeBlocked(octi::GAME_MODE_2, false, true, octi::GAME_BASES_3);
        bEdgeBlocked.fromPositionString(
            "2:3:0:1:P:A:-:0:0,0,0,0:0,0,0,0:"
            "A15abcdefgh,B16,B26,B25,B24,B14,B94,B95,B96,"
            "B17,B37,B35,B33,B13,B83,B85,B87");
        CHECK_FALSE(bEdgeBlocked.getGame()->hasAnyLegalMove());
        CHECK_TRUE(bEdgeBlocked.getGame()->generateLegalMoves().empty());

        // Remove B85 so A15abcdefgh can jump West over B95 to empty 85 via edgeless wraparound
        octi::OctiBoard bEdgeJump(octi::GAME_MODE_2, false, true, octi::GAME_BASES_3);
        bEdgeJump.fromPositionString(
            "2:3:0:1:P:A:-:0:0,0,0,0:0,0,0,0:"
            "A15abcdefgh,B16,B26,B25,B24,B14,B94,B95,B96,"
            "B17,B37,B35,B33,B13,B83,B87");
        CHECK_TRUE(bEdgeJump.getGame()->hasAnyLegalMove());
        CHECK_FALSE(bEdgeJump.getGame()->generateLegalMoves().empty());
    }
    {
        // Superprong-only pod (0 normal prongs, so no reposition; 0 reserve pods/prongs)
        octi::OctiBoard bSuper(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        bSuper.fromPositionString("2:3:1:0:P:A:-:0:0,0,0,0:0,0,0,0:A11g!,B57e");
        // Superprong g! points West off-board from (1,1), 0 normal prongs -> 0 legal moves!
        CHECK_FALSE(bSuper.getGame()->hasAnyLegalMove());
        CHECK_TRUE(bSuper.getGame()->generateLegalMoves().empty());

        octi::OctiBoard bSuperStep(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        bSuperStep.fromPositionString("2:3:1:0:P:A:-:0:0,0,0,0:0,0,0,0:A11a!,B57e");
        CHECK_TRUE(bSuperStep.getGame()->hasAnyLegalMove());
        CHECK_FALSE(bSuperStep.getGame()->generateLegalMoves().empty());
    }
    {
        // 4P seat skipping: Player A moves, Player B has 0 legal moves (non-eliminated pod B55 with 0 prongs
        // and 0 reserve pods/prongs), so turn skips B and lands on C!
        octi::OctiBoard b4Skip(octi::GAME_MODE_4, false, false, octi::GAME_BASES_3);
        b4Skip.fromPositionString(
            "4:3:0:0:P:A:-:0:0,0,2,0:0,0,0,0:1,0,2,0:1,0,2,0:A33a,B55,C77e,D73g");
        b4Skip.getGame()->setSearchMode(true);
        b4Skip.getGame()->move("33a-34");
        CHECK_FALSE(b4Skip.getGame()->isGameOver());
        CHECK_EQUAL(octi::PLAYER_C, b4Skip.getGame()->getTurn());
        CHECK_TRUE(b4Skip.getGame()->hasAnyLegalMove());
    }

    // 2. 10,000 random playouts per variant across ALL 13 variants (130,000 playouts total).
    //    For every variant we build a deterministic seed bank of diverse starting positions
    //    (fresh game start, midgame positions, and depleted-reserve near-stalemate states),
    //    then execute 10,000 random playouts verifying at EVERY ply:
    //    - hasAnyLegalMove() == !generateLegalMoves().empty()
    //    - exact equivalence of isGameOver(), getWinner(), and getTurn() sequence
    //      (both searchMode=true and searchMode=false).
    constexpr int kPlayoutsPerVariant = 10000;
    constexpr int kNumVariants = 13;

    struct VariantResult {
        int playoutsCompleted = 0;
        std::uint64_t pliesVerified = 0;
        int gameOversObserved = 0;
        int mismatches = 0;
    };
    std::vector<VariantResult> results(kNumVariants);

    // Pre-generate a seed bank of 16 diverse positions per variant (including fresh start,
    // midgame played states, and low-reserve states) so playouts hit every branch.
    std::vector<std::vector<std::string>> seedBanks(kNumVariants);
    for (int v = 0; v < kNumVariants; ++v) {
        const VariantSpecE3 &vs = kAll13VariantsE3[v];
        std::uint64_t bankRng = 0xA5A50000ULL + static_cast<std::uint64_t>(v) * 1337ULL;

        octi::OctiBoard fresh(vs.mode, vs.superProngs, vs.edgeless, vs.bases);
        fresh.suppressBoardUpdateNotification(true);
        fresh.getGame()->start(octi::PLAYER_A);
        seedBanks[v].push_back(fresh.toPositionString());

        for (int s = 1; s < 12; ++s) {
            octi::OctiBoard b(vs.mode, vs.superProngs, vs.edgeless, vs.bases);
            b.suppressBoardUpdateNotification(true);
            b.getGame()->setSearchMode(true);
            b.getGame()->start(octi::PLAYER_A);
            int targetPlies = 4 + s * 2;
            std::vector<octi::OctiMove> mvs;
            for (int p = 0; p < targetPlies && !b.getGame()->isGameOver(); ++p) {
                b.getGame()->generateLegalMoves(mvs);
                if (mvs.empty()) break;
                const octi::OctiMove &mv = mvs[splitMix64E3(bankRng) % mvs.size()];
                b.getGame()->move(mv);
            }
            if (!b.getGame()->isGameOver()) {
                seedBanks[v].push_back(b.toPositionString());
            }
        }

        // Also add low-reserve / near-stalemate positions so 0-legal-move transitions occur frequently
        if (vs.mode == octi::GAME_MODE_2) {
            std::string hdr = std::string("2:") +
                (vs.bases == octi::GAME_BASES_1 ? "1:" : "3:") +
                (vs.superProngs ? "1:" : "0:") +
                (vs.edgeless ? "1:" : "0:");
            seedBanks[v].push_back(hdr + "P:A:-:0:0,0,1,0:0,0,1,0:A44a,B56e");
            seedBanks[v].push_back(hdr + "P:B:-:0:0,0,0,0:0,0,1,0:A55,B46ef");
            seedBanks[v].push_back(hdr + "P:A:-:0:0,0,0,0:0,0,0,0:A37,A57,A76a,B55");
        } else if (vs.mode == octi::GAME_MODE_4) {
            std::string hdr = std::string("4:3:") +
                (vs.superProngs ? "1:" : "0:") +
                (vs.edgeless ? "1:" : "0:");
            seedBanks[v].push_back(hdr + "P:A:-:0:0,0,1,0:0,0,0,0:0,0,1,0:0,0,0,0:A34a,B55,C66e,D53");
            seedBanks[v].push_back(hdr + "P:B:-:0:0,0,0,0:0,0,1,0:0,0,0,0:0,0,0,0:A34,B46c,C66,D53");
            seedBanks[v].push_back(hdr + "P:A:-:0:0,0,0,0:0,0,0,0:0,0,0,0:0,0,0,0:A36a,B55,C63c,D53");
        } else {
            seedBanks[v].push_back("OFK:1:0:0:P:A:-:0:0,0,1,0:0,0,1,0:A22a,A23,B45e,B56");
            seedBanks[v].push_back("OFK:1:0:0:P:B:-:0:0,0,0,0:0,0,1,0:A22a,A23,B24,B55e");
        }
    }

    struct WorkItem {
        int variantIdx;
        int startIdx;
        int endIdx;
        VariantResult partial;
    };

    std::vector<WorkItem> items;
    constexpr int kChunkSize = 500;
    for (int v = 0; v < kNumVariants; ++v) {
        for (int start = 0; start < kPlayoutsPerVariant; start += kChunkSize) {
            int end = std::min(kPlayoutsPerVariant, start + kChunkSize);
            items.push_back({v, start, end, {}});
        }
    }

    std::atomic<std::size_t> nextItem{0};
    unsigned int hw = std::thread::hardware_concurrency();
    unsigned int numWorkers = std::min<unsigned int>(36u, std::max<unsigned int>(4u, hw));

    std::vector<std::thread> workers;
    workers.reserve(numWorkers);
    for (unsigned int wIdx = 0; wIdx < numWorkers; ++wIdx) {
        workers.emplace_back([&]() {
            std::vector<octi::OctiMove> moves;
            std::vector<octi::OctiMove> checkMoves;
            while (true) {
                std::size_t idx = nextItem.fetch_add(1, std::memory_order_relaxed);
                if (idx >= items.size()) {
                    break;
                }
                WorkItem &item = items[idx];
                const int v = item.variantIdx;
                const VariantSpecE3 &vs = kAll13VariantsE3[v];
                const auto &bank = seedBanks[v];

                octi::OctiBoard board(vs.mode, vs.superProngs, vs.edgeless, vs.bases);
                board.suppressBoardUpdateNotification(true);

                for (int pIdx = item.startIdx; pIdx < item.endIdx; ++pIdx) {
                    std::uint64_t rng = (static_cast<std::uint64_t>(v) << 32) ^
                                        (static_cast<std::uint64_t>(pIdx) * 0x9E3779B97F4A7C15ULL + 1ULL);
                    const std::string &startFen = bank[static_cast<std::size_t>(pIdx) % bank.size()];
                    board.fromPositionString(startFen);
                    octi::OctiGame *g = board.getGame();
                    g->setSearchMode((pIdx & 1) == 0);

                    const int maxPlies = (vs.superProngs ? 6 : 10);
                    for (int ply = 0; ply < maxPlies; ++ply) {
                        bool fastAny = g->hasAnyLegalMove();
                        g->generateLegalMoves(moves);
                        bool refAny = !moves.empty();
                        if (fastAny != refAny) {
                            ++item.partial.mismatches;
                            break;
                        }
                        if (g->isGameOver() || moves.empty()) {
                            break;
                        }

                        octi::OctiPlayer prevTurn = g->getTurn();
                        std::string preMoveFen;
                        if (vs.mode == octi::GAME_MODE_4 || moves.size() <= 4) {
                            preMoveFen = board.toPositionString();
                        }

                        const octi::OctiMove &chosen =
                            moves[splitMix64E3(rng) % moves.size()];
                        g->move(chosen);
                        ++item.partial.pliesVerified;

                        if (g->isGameOver()) {
                            ++item.partial.gameOversObserved;
                            // Verify terminal state: hasAnyLegalMove() and generateLegalMoves() must both be false
                            if (g->hasAnyLegalMove() || !g->generateLegalMoves().empty()) {
                                ++item.partial.mismatches;
                                break;
                            }
                            // If neither base win nor pod wipeout occurred, verify the next player genuinely had 0 moves
                            if (!preMoveFen.empty()) {
                                octi::OctiBoard refB(vs.mode, vs.superProngs, vs.edgeless, vs.bases);
                                refB.suppressBoardUpdateNotification(true);
                                refB.fromPositionString(preMoveFen);
                                refB.getGame()->setSearchMode(true);
                                refB.getGame()->move(chosen);
                                if (!refB.getGame()->isGameOver() ||
                                    refB.getGame()->getWinner() != g->getWinner() ||
                                    refB.getGame()->getTurn() != g->getTurn()) {
                                    ++item.partial.mismatches;
                                    break;
                                }
                            }
                            break;
                        } else if (vs.mode == octi::GAME_MODE_4) {
                            // If 4P skipped any non-eliminated seat(s) between prevTurn and g->getTurn(),
                            // verify each skipped non-eliminated seat genuinely had generateLegalMoves().empty()!
                            octi::OctiPlayer expectedNext = nextSeatE3(vs.mode, prevTurn);
                            while (expectedNext != g->getTurn() &&
                                   g->isPlayerEliminated(expectedNext)) {
                                expectedNext = nextSeatE3(vs.mode, expectedNext);
                            }
                            if (expectedNext != g->getTurn()) {
                                // Seat(s) were skipped due to 0 legal moves: verify on board copy with turn set to skipped seat
                                octi::OctiPlayer cur = expectedNext;
                                int guard = 0;
                                while (cur != g->getTurn() && guard < 4) {
                                    if (!g->isPlayerEliminated(cur)) {
                                        std::string curFen = board.toPositionString();
                                        // Replace ":P:<turn>:" with ":P:<cur>:"
                                        char curChar = static_cast<char>('A' + (static_cast<int>(cur) - 1));
                                        std::size_t pPos = curFen.find(":P:");
                                        if (pPos != std::string::npos && pPos + 3 < curFen.size()) {
                                            curFen[pPos + 3] = curChar;
                                            octi::OctiBoard seatB(vs.mode, vs.superProngs, vs.edgeless, vs.bases);
                                            seatB.suppressBoardUpdateNotification(true);
                                            seatB.fromPositionString(curFen);
                                            seatB.getGame()->generateLegalMoves(checkMoves);
                                            if (!checkMoves.empty() || seatB.getGame()->hasAnyLegalMove()) {
                                                ++item.partial.mismatches;
                                            }
                                        }
                                    }
                                    cur = nextSeatE3(vs.mode, cur);
                                    ++guard;
                                }
                            }
                        }
                    }

                    // Final post-loop check on reached position
                    bool finalFast = g->hasAnyLegalMove();
                    g->generateLegalMoves(moves);
                    if (finalFast != !moves.empty()) {
                        ++item.partial.mismatches;
                    }
                    if (!g->isGameOver() && moves.empty()) {
                        ++item.partial.mismatches;
                    }

                    ++item.partial.playoutsCompleted;
                }
            }
        });
    }

    for (auto &th : workers) {
        th.join();
    }

    int totalPlayouts = 0;
    std::uint64_t totalPlies = 0;
    int totalGameOvers = 0;
    for (const auto &item : items) {
        VariantResult &vr = results[item.variantIdx];
        vr.playoutsCompleted += item.partial.playoutsCompleted;
        vr.pliesVerified += item.partial.pliesVerified;
        vr.gameOversObserved += item.partial.gameOversObserved;
        vr.mismatches += item.partial.mismatches;
    }

    for (int v = 0; v < kNumVariants; ++v) {
        CHECK_EQUAL(kPlayoutsPerVariant, results[v].playoutsCompleted);
        CHECK_EQUAL(0, results[v].mismatches);
        CHECK_TRUE(results[v].pliesVerified > 0);
        CHECK_TRUE(results[v].gameOversObserved > 0);
        totalPlayouts += results[v].playoutsCompleted;
        totalPlies += results[v].pliesVerified;
        totalGameOvers += results[v].gameOversObserved;
    }
    CHECK_EQUAL(kNumVariants * kPlayoutsPerVariant, totalPlayouts);
    CHECK_TRUE(totalPlies >= 500000ULL);
    CHECK_TRUE(totalGameOvers >= 1000);
}

TEST(OctiEnvelope3SpeedupTest, LazyMoveAsStringAndStructuralKeyMatchEagerAcross1000Positions)
{
    // Collect 1,000 distinct positions across all 13 variants, including explicit
    // superprong bonus-prelude positions and multi-pod composite move checks.
    std::vector<std::string> fens;
    fens.reserve(1020);
    std::vector<VariantSpecE3> fenSpecs;
    fenSpecs.reserve(1020);

    // Explicit superprong prelude & tactical capture positions
    {
        VariantSpecE3 sp2 = {octi::GAME_MODE_2, octi::GAME_BASES_3, true, false, "2P_3B_S"};
        fens.push_back("2:3:1:0:P:A:-:0:2,0,12,2:2,0,12,2:A33a!b!h,A44ab,B45def,B57ef");
        fenSpecs.push_back(sp2);
        fens.push_back("2:3:1:0:P:A:-:0:1,1,8,1:1,0,10,1:A34a!b!,A54b!c!,B35e,B45f,B66de");
        fenSpecs.push_back(sp2);

        VariantSpecE3 sp2e = {octi::GAME_MODE_2, octi::GAME_BASES_3, true, true, "2P_3B_SE"};
        fens.push_back("2:3:1:1:P:A:-:0:2,0,10,2:2,0,10,2:A15a!g!h,B95de,A44ab,B56ef");
        fenSpecs.push_back(sp2e);

        VariantSpecE3 sp4 = {octi::GAME_MODE_4, octi::GAME_BASES_3, true, false, "4P_3B_S"};
        fens.push_back("4:3:1:0:P:A:-:0:2,0,8,1:2,0,8,1:2,0,8,1:2,0,8,1:A33a!b!,B36de,C77e!f!,D63gh");
        fenSpecs.push_back(sp4);
    }

    std::uint64_t rng = 0xC0FFEE123456789ULL;
    int vCursor = 0;
    while (fens.size() < 1000) {
        const VariantSpecE3 &vs = kAll13VariantsE3[vCursor % 13];
        ++vCursor;
        octi::OctiBoard b(vs.mode, vs.superProngs, vs.edgeless, vs.bases);
        b.suppressBoardUpdateNotification(true);
        b.getGame()->setSearchMode(true);
        b.getGame()->start(octi::PLAYER_A);

        std::vector<octi::OctiMove> mvs;
        int steps = 1 + static_cast<int>(splitMix64E3(rng) % 24);
        for (int s = 0; s < steps && !b.getGame()->isGameOver(); ++s) {
            b.getGame()->generateLegalMoves(mvs);
            if (mvs.empty()) break;
            b.getGame()->move(mvs[splitMix64E3(rng) % mvs.size()]);
            if (fens.size() < 1000 && !b.getGame()->isGameOver()) {
                fens.push_back(b.toPositionString());
                fenSpecs.push_back(vs);
            }
        }
    }
    fens.resize(1000);
    fenSpecs.resize(1000);

    std::size_t totalMovesChecked = 0;
    std::size_t totalTacticalMovesChecked = 0;
    std::size_t superProngBonusMovesSeen = 0;

    for (std::size_t idx = 0; idx < fens.size(); ++idx) {
        const VariantSpecE3 &vs = fenSpecs[idx];
        octi::OctiBoard b(vs.mode, vs.superProngs, vs.edgeless, vs.bases);
        b.suppressBoardUpdateNotification(true);
        b.fromPositionString(fens[idx]);
        const octi::OctiGame *g = b.getGame();

        std::vector<octi::OctiMove> moves = g->generateLegalMoves();
        CHECK_EQUAL(!moves.empty(), g->hasAnyLegalMove());

        // Verify generateTacticalMoves() matches reference filtering of generateLegalMoves()
        std::vector<octi::OctiMove> tacMoves = g->generateTacticalMoves();
        std::vector<octi::OctiMove> refTacMoves;
        for (const auto &m : moves) {
            if (referenceIsTacticalMoveE3(b, vs.mode, vs.edgeless, g->getTurn(), m)) {
                refTacMoves.push_back(m);
            }
        }
        CHECK_EQUAL(refTacMoves.size(), tacMoves.size());
        for (std::size_t ti = 0; ti < tacMoves.size(); ++ti) {
            CHECK_EQUAL(refTacMoves[ti].structuralKey(), tacMoves[ti].structuralKey());
            STRCMP_EQUAL(refTacMoves[ti].asString().c_str(), tacMoves[ti].asString().c_str());
        }
        totalTacticalMovesChecked += tacMoves.size();

        std::unordered_set<std::uint64_t> keySet;
        std::unordered_set<std::string> strSet;
        keySet.reserve(moves.size() * 2 + 1);
        strSet.reserve(moves.size() * 2 + 1);

        for (const auto &m : moves) {
            std::string lazyStr = m.asString();
            std::string eagerStr = octi::OctiMove::formatOps(m.getOps());
            STRCMP_EQUAL(eagerStr.c_str(), lazyStr.c_str());

            std::uint64_t key = m.structuralKey();
            CHECK_TRUE(key != 0ULL);

            octi::OctiMove reparsed(lazyStr);
            CHECK_EQUAL(key, reparsed.structuralKey());
            STRCMP_EQUAL(eagerStr.c_str(), reparsed.asString().c_str());

            bool newStr = strSet.insert(eagerStr).second;
            bool newKey = keySet.insert(key).second;
            CHECK_EQUAL(newStr, newKey);

            for (const auto &op : m.getOps()) {
                if (op.isSuperProngBonusMove) {
                    ++superProngBonusMovesSeen;
                }
            }
            ++totalMovesChecked;
        }
    }

    CHECK_TRUE(totalMovesChecked > 10000);
    CHECK_TRUE(totalTacticalMovesChecked > 50);
    CHECK_TRUE(superProngBonusMovesSeen > 0);

    // Explicit multi-pod composite move & long jump chain (>= 5 hops -> SplitMix64 bit-63 path)
    {
        octi::OctiMove comp("33ab-44,33a-34");
        STRCMP_EQUAL("33ab-44,33a-34", comp.asString().c_str());
        STRCMP_EQUAL(octi::OctiMove::formatOps(comp.getOps()).c_str(), comp.asString().c_str());
        CHECK_TRUE((comp.structuralKey() & (1ULL << 63)) != 0ULL);

        octi::OctiMove longChain("11abcdefgh-33x-55-77x-75-53x");
        STRCMP_EQUAL(octi::OctiMove::formatOps(longChain.getOps()).c_str(),
                     longChain.asString().c_str());
        CHECK_TRUE((longChain.structuralKey() & (1ULL << 63)) != 0ULL);
        CHECK_TRUE(comp.structuralKey() != longChain.structuralKey());

        // Concurrent const asString() reads across 8 threads (verifies zero mutable race)
        const std::vector<octi::OctiMove> sharedMoves = {comp, longChain};
        std::atomic<int> mismatchCount{0};
        std::vector<std::thread> readers;
        for (int t = 0; t < 8; ++t) {
            readers.emplace_back([&]() {
                for (int iter = 0; iter < 500; ++iter) {
                    if (sharedMoves[0].asString() != "33ab-44,33a-34" ||
                        sharedMoves[1].asString() != "11abcdefgh-33x-55-77x-75-53x") {
                        mismatchCount.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        for (auto &th : readers) {
            th.join();
        }
        CHECK_EQUAL(0, mismatchCount.load());
    }
}

TEST(OctiEnvelope3SpeedupTest, CompactTtMoveKeyRoundTripsAndLazySmp8ThreadsClean)
{
    // 1. Verify compact trivially copyable TranspositionEntry size (24 bytes, down from 56 bytes)
    CHECK_EQUAL(24u, octi::OctiBotEngine::transpositionEntrySize());

    // 2. Verify compact 64-bit bestMoveKey stores into TT and round-trips back to the exact OctiMove
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.suppressBoardUpdateNotification(true);
    board.fromPositionString(
        "2:3:0:0:P:A:-:0:3,0,16,0:3,0,16,0:A44abh,A53ab,A73a,B46def,B57ef,B77f");

    octi::OctiBotEngine singleEngine(octi::BotDifficulty::TACTICAL);
    octi::BotTuningProfile prof = singleEngine.tuningProfile();
    prof.useOpeningBook = false;
    prof.rootScoreJitter = 0;
    prof.lazySmpThreads = 1;
    prof.maxSearchDepth = 3;
    prof.timeBudgetMs = 30000;
    singleEngine.setTuningProfile(prof);

    octi::BotSearchStats stats1;
    std::string bestMove1 = singleEngine.chooseMove(board, &stats1);
    CHECK_FALSE(bestMove1.empty());
    CHECK_TRUE(stats1.nodesVisited > 20);
    CHECK_EQUAL(3, stats1.depthReached);

    // Inspect child position after bestMove1: interior negamax stored bestMoveKey in _tt!
    octi::OctiBoard childBoard(octi::GAME_MODE_2);
    childBoard.suppressBoardUpdateNotification(true);
    childBoard.fromPositionString(board.toPositionString());
    childBoard.getGame()->move(bestMove1);

    std::uint64_t childTtMoveKey = singleEngine.lookupTranspositionBestMoveKey(childBoard);
    CHECK_TRUE(childTtMoveKey != 0ULL);

    std::vector<octi::OctiMove> childLegal = childBoard.getGame()->generateLegalMoves();
    int keyMatches = 0;
    for (const auto &cm : childLegal) {
        if (cm.structuralKey() == childTtMoveKey) {
            ++keyMatches;
            CHECK_FALSE(cm.asString().empty());
        }
    }
    CHECK_EQUAL(1, keyMatches);

    // Search childBoard at depth 4 on the warm TT so negamax probes stored TT entries & bestMoveKey
    prof.maxSearchDepth = 4;
    singleEngine.setTuningProfile(prof);
    octi::BotSearchStats statsWarm;
    std::string childBest = singleEngine.chooseMove(childBoard, &statsWarm);
    CHECK_FALSE(childBest.empty());
    CHECK_TRUE(statsWarm.nodesVisited > 0);

    // 3. Concurrent 8-thread Lazy SMP search exercising TT_MUTEX_COUNT = 16384 and shared compact TT
    octi::OctiBotEngine smpEngine(octi::BotDifficulty::MASTER);
    octi::BotTuningProfile smpProf = smpEngine.tuningProfile();
    smpProf.useOpeningBook = false;
    smpProf.rootScoreJitter = 0;
    smpProf.lazySmpThreads = 8;
    smpProf.maxSearchDepth = 6;
    smpProf.timeBudgetMs = 550; // >= 500ms triggers Lazy SMP helper threads in chooseMove
    smpEngine.setTuningProfile(smpProf);

    octi::BotSearchStats smpStats;
    std::string smpBest = smpEngine.chooseMove(board, &smpStats);
    CHECK_FALSE(smpBest.empty());
    CHECK_TRUE(smpStats.depthReached >= 1);
    CHECK_TRUE(smpStats.nodesVisited > 50);

    // Verify smpBest is a legal move on board
    std::vector<octi::OctiMove> rootLegal = board.getGame()->generateLegalMoves();
    bool foundLegal = false;
    for (const auto &rm : rootLegal) {
        if (rm.asString() == smpBest) {
            foundLegal = true;
            break;
        }
    }
    CHECK_TRUE(foundLegal);
}

// ---------------------------------------------------------------------------
// Envelope 4 Search Improvements Unit Tests
// ---------------------------------------------------------------------------

TEST_GROUP(OctiEnvelope4SearchTest)
{
};

TEST(OctiEnvelope4SearchTest, DefaultOffProfilePreservesExactNodeCountsBestMovesAndUncappedBranches)
{
    octi::BotTuningProfile defProf;
    CHECK_FALSE(defProf.usePvs);
    CHECK_EQUAL(0, defProf.aspirationWindowCp);
    CHECK_FALSE(defProf.useKillers);
    CHECK_FALSE(defProf.useHistory);
    CHECK_EQUAL(0, defProf.lmrMinDepth);
    CHECK_EQUAL(0, defProf.lmrMinMoveIndex);
    CHECK_EQUAL(0, defProf.nullMoveMinDepth);
    CHECK_EQUAL(2, defProf.nullMoveReduction);
    CHECK_FALSE(defProf.qsearchUseTt);
    CHECK_EQUAL(18, defProf.ttSizeLog2);
    CHECK_FALSE(defProf.lazySmpDiversifyHelpers);

    octi::OctiBotEngine checkEngine(octi::BotDifficulty::GRANDMASTER, 0x5EED2026ULL);
    CHECK_EQUAL(24u, octi::OctiBotEngine::transpositionEntrySize());
    CHECK_EQUAL(std::size_t{1} << 18, checkEngine.transpositionTableEntryCount());

    // Exact depth-5 single-thread pin against Envelope 3 bench fixtures [5] and [23]
    struct PinnedFixture {
        const char *fen;
        std::uint64_t expectedNodes;
        const char *expectedBestMove;
        int expectedScore;
    };
    const PinnedFixture kPinned[2] = {
        {"2:3:0:0:P:A:-:0:4,0,22,0:4,0,22,0:A33,A53ab,A73a,B37e,B57e,B77f",
         41577ULL, "73a+h", 2752},
        {"OFK:1:0:0:P:A:-:0:0,0,8,0:0,0,6,0:A22,A32,A42,A64abdh,B26de,B36de,B46,B56ef",
         45124ULL, "64abdh-65", 14350}
    };

    for (const auto &fix : kPinned) {
        octi::OctiBoard board;
        board.suppressBoardUpdateNotification(true);
        board.fromPositionString(fix.fen);

        octi::OctiBotEngine engine(octi::BotDifficulty::GRANDMASTER, 0x5EED2026ULL);
        octi::BotTuningProfile prof =
            octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::GRANDMASTER);
        prof.lazySmpThreads = 1;
        prof.useOpeningBook = false;
        prof.rootScoreJitter = 0;
        prof.bookSelectionWindow = 0;
        prof.maxSearchDepth = 5;
        prof.timeBudgetMs = 3600000;
        engine.setTuningProfile(prof);
        engine.setTimeBudgetMs(3600000);

        octi::BotSearchStats stats;
        std::string best = engine.chooseMove(board, &stats);
        CHECK_EQUAL(5, stats.depthReached);
        CHECK_EQUAL(fix.expectedNodes, stats.nodesVisited);
        STRCMP_EQUAL(fix.expectedBestMove, best.c_str());
        CHECK_EQUAL(fix.expectedScore, stats.bestScore);

        octi::BotSearchDiagnostics diag = engine.lastSearchDiagnostics();
        CHECK_EQUAL(0ULL, diag.pvsReSearches);
        CHECK_EQUAL(0ULL, diag.aspirationReSearches);
        CHECK_EQUAL(0ULL, diag.killerUpdates);
        CHECK_EQUAL(0ULL, diag.historyUpdates);
        CHECK_EQUAL(0ULL, diag.lmrReductions);
        CHECK_EQUAL(0ULL, diag.nullMoveTries);
        CHECK_EQUAL(0ULL, diag.qsearchTtProbes);
    }

    // Verify rootBranchCap <= 0 and treeBranchCap <= 0 mean "all legal moves" (uncapped)
    octi::OctiBoard wideBoard(octi::GAME_MODE_2);
    wideBoard.suppressBoardUpdateNotification(true);
    wideBoard.fromPositionString(
        "2:3:0:0:P:A:-:0:2,0,12,0:2,0,12,0:"
        "A33abch,A53abgh,A73abf,A44abcdefgh,A64abch,B37def,B57def,B77def");
    std::vector<octi::OctiMove> rawLegal = wideBoard.getGame()->generateLegalMoves();
    CHECK_TRUE(rawLegal.size() > 48u);

    octi::OctiBotEngine uncappedEngine(octi::BotDifficulty::TACTICAL);
    octi::BotTuningProfile uncappedProf = uncappedEngine.tuningProfile();
    uncappedProf.useOpeningBook = false;
    uncappedProf.rootScoreJitter = 0;
    uncappedProf.lazySmpThreads = 1;
    uncappedProf.compositeMoveMaxPly = -1;
    uncappedProf.rootBranchCap = 0;
    uncappedProf.treeBranchCap = 0;
    uncappedEngine.setTuningProfile(uncappedProf);

    std::vector<octi::BookMoveScore> allScored =
        uncappedEngine.searchTopMovesForBook(wideBoard, 1, 0, 512);
    CHECK_TRUE(allScored.size() > 48u);
}

TEST(OctiEnvelope4SearchTest, MakeAndUnmakeNullMoveZobristInvariantsAndPruningGuards)
{
    // 1. Rejected outside search mode and before game start
    {
        octi::OctiBoard b2(octi::GAME_MODE_2);
        octi::OctiGame *g2 = b2.getGame();
        g2->setSearchMode(true);
        CHECK_FALSE(g2->makeNullMove()); // not started yet

        g2->setSearchMode(false);
        g2->start(octi::PLAYER_A);
        std::uint64_t hBefore = octi::OctiBotEngine::computePositionHashStatic(b2);
        std::uint64_t hTrack = hBefore;
        CHECK_FALSE(g2->makeNullMove(&hTrack));
        CHECK_FALSE(g2->isInNullMove());
        CHECK_EQUAL(octi::PLAYER_A, g2->getTurn());
        CHECK_EQUAL(hBefore, hTrack);

        // Now enable search mode: makeNullMove & unmakeNullMove must round-trip cleanly
        g2->setSearchMode(true);
        int movesBefore = g2->getNumMoves();
        CHECK_TRUE(g2->makeNullMove(&hTrack));
        CHECK_TRUE(g2->isInNullMove());
        CHECK_EQUAL(octi::PLAYER_B, g2->getTurn());
        CHECK_EQUAL(movesBefore + 1, g2->getNumMoves());
        CHECK_TRUE(hTrack != hBefore);
        CHECK_EQUAL(octi::OctiBotEngine::computePositionHashStatic(b2), hTrack);

        // Snapshot round-trip inside active null move preserves isInNullMove()
        octi::OctiGameSnapshot snap = g2->captureSnapshot();
        g2->move("57+e");
        g2->restoreSnapshot(snap);
        CHECK_TRUE(g2->isInNullMove());
        CHECK_EQUAL(octi::PLAYER_B, g2->getTurn());

        CHECK_TRUE(g2->unmakeNullMove(&hTrack));
        CHECK_FALSE(g2->isInNullMove());
        CHECK_EQUAL(octi::PLAYER_A, g2->getTurn());
        CHECK_EQUAL(movesBefore, g2->getNumMoves());
        CHECK_EQUAL(hBefore, hTrack);
        CHECK_EQUAL(hBefore, octi::OctiBotEngine::computePositionHashStatic(b2));
        CHECK_FALSE(g2->unmakeNullMove(&hTrack)); // extra unmake rejected
    }

    // 2. Works identically in GAME_MODE_OFK search mode
    {
        octi::OctiBoard bOfk(octi::GAME_MODE_OFK);
        octi::OctiGame *gOfk = bOfk.getGame();
        gOfk->start(octi::PLAYER_A);
        gOfk->setSearchMode(true);
        std::uint64_t h0 = octi::OctiBotEngine::computePositionHashStatic(bOfk);
        std::uint64_t hInc = h0;
        CHECK_TRUE(gOfk->makeNullMove(&hInc));
        CHECK_EQUAL(octi::PLAYER_B, gOfk->getTurn());
        CHECK_EQUAL(octi::OctiBotEngine::computePositionHashStatic(bOfk), hInc);
        CHECK_TRUE(gOfk->unmakeNullMove(&hInc));
        CHECK_EQUAL(octi::PLAYER_A, gOfk->getTurn());
        CHECK_EQUAL(h0, hInc);
    }

    // 3. Strictly rejected in GAME_MODE_4 even when searchMode is true
    {
        octi::OctiBoard b4(octi::GAME_MODE_4);
        octi::OctiGame *g4 = b4.getGame();
        g4->start(octi::PLAYER_A);
        g4->setSearchMode(true);
        std::uint64_t h0 = octi::OctiBotEngine::computePositionHashStatic(b4);
        std::uint64_t hInc = h0;
        CHECK_FALSE(g4->makeNullMove(&hInc));
        CHECK_FALSE(g4->isInNullMove());
        CHECK_EQUAL(octi::PLAYER_A, g4->getTurn());
        CHECK_EQUAL(h0, hInc);

        octi::OctiBotEngine bot4(octi::BotDifficulty::TACTICAL);
        octi::BotTuningProfile p4 = bot4.tuningProfile();
        p4.useOpeningBook = false;
        p4.rootScoreJitter = 0;
        p4.lazySmpThreads = 1;
        p4.maxSearchDepth = 3;
        p4.rootBranchCap = 10;
        p4.treeBranchCap = 10;
        p4.nullMoveMinDepth = 2;
        p4.nullMoveReduction = 2;
        p4.timeBudgetMs = 30000;
        bot4.setTuningProfile(p4);
        bot4.chooseMove(b4);
        CHECK_EQUAL(0ULL, bot4.lastSearchDiagnostics().nullMoveTries);
    }

    // 4. Disabled when side to move has freePods == 0 AND < 2 armed pods on board
    {
        octi::OctiBoard bLowMat(octi::GAME_MODE_2);
        bLowMat.suppressBoardUpdateNotification(true);
        bLowMat.fromPositionString("2:3:0:0:P:A:-:0:0,0,6,0:0,0,6,0:A44ab,A33,B56ef,B77");

        octi::OctiBotEngine botLow(octi::BotDifficulty::TACTICAL);
        octi::BotTuningProfile pLow = botLow.tuningProfile();
        pLow.useOpeningBook = false;
        pLow.rootScoreJitter = 0;
        pLow.lazySmpThreads = 1;
        pLow.maxSearchDepth = 4;
        pLow.rootBranchCap = 12;
        pLow.treeBranchCap = 12;
        pLow.nullMoveMinDepth = 2;
        pLow.nullMoveReduction = 2;
        pLow.timeBudgetMs = 30000;
        botLow.setTuningProfile(pLow);
        botLow.chooseMove(bLowMat);
        CHECK_EQUAL(0ULL, botLow.lastSearchDiagnostics().nullMoveTries);
    }

    // 5. Active with verification search in normal 2P midgame
    {
        octi::OctiBoard bMid(octi::GAME_MODE_2);
        bMid.suppressBoardUpdateNotification(true);
        bMid.fromPositionString(
            "2:3:0:0:P:A:-:0:3,0,18,0:3,0,18,0:A44abh,A54ab,A73a,B37de,B57ef,B77f");

        octi::BotTuningProfile baseP =
            octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::TACTICAL);
        baseP.useOpeningBook = false;
        baseP.rootScoreJitter = 0;
        baseP.lazySmpThreads = 1;
        baseP.maxSearchDepth = 4;
        baseP.rootBranchCap = 16;
        baseP.treeBranchCap = 14;
        baseP.timeBudgetMs = 30000;

        octi::OctiBotEngine botOff(octi::BotDifficulty::TACTICAL);
        botOff.setTuningProfile(baseP);
        octi::BotSearchStats statsOff;
        botOff.chooseMove(bMid, &statsOff);
        CHECK_EQUAL(0ULL, botOff.lastSearchDiagnostics().nullMoveTries);

        octi::OctiBotEngine botOn(octi::BotDifficulty::TACTICAL);
        octi::BotTuningProfile onP = baseP;
        onP.nullMoveMinDepth = 2;
        onP.nullMoveReduction = 2;
        botOn.setTuningProfile(onP);
        octi::BotSearchStats statsOn;
        std::string mvOn = botOn.chooseMove(bMid, &statsOn);
        CHECK_FALSE(mvOn.empty());

        octi::BotSearchDiagnostics diagOn = botOn.lastSearchDiagnostics();
        CHECK_TRUE(diagOn.nullMoveTries > 0ULL);
        CHECK_TRUE(diagOn.nullMoveVerifications > 0ULL);
        CHECK_TRUE(diagOn.nullMoveCutoffs > 0ULL);
        CHECK_TRUE(statsOn.nodesVisited < statsOff.nodesVisited);
    }
}

TEST(OctiEnvelope4SearchTest, PvsReSearchesOnFailHighAndMatchesFullWindowAlphaBeta)
{
    // Position where shallow d=1 choice gets tactically refuted at d=2/d=3, so a move at
    // mvIdx >= 1 beats move 0's alpha under the PVS null window and triggers full re-search.
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.suppressBoardUpdateNotification(true);
    board.fromPositionString(
        "2:3:0:0:P:B:-:0:3,0,17,0:3,0,16,0:"
        "B51def,B33,A53,A54abegh,A64abfh,A55afh,B46de,B37");

    octi::BotTuningProfile prof =
        octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::TACTICAL);
    prof.useOpeningBook = false;
    prof.rootScoreJitter = 0;
    prof.lazySmpThreads = 1;
    prof.compositeMoveMaxPly = -1;
    prof.maxQuiesceDepth = 2;
    prof.quiesceBranchCap = 0;
    prof.rootBranchCap = 0;
    prof.treeBranchCap = 0;
    prof.maxSearchDepth = 3;
    prof.timeBudgetMs = 60000;
    prof.moveOrderCaptureBonus = -16000;
    prof.moveOrderLeapfrogBonus = -1200;

    octi::OctiBotEngine abEngine(octi::BotDifficulty::TACTICAL);
    abEngine.setTuningProfile(prof);
    octi::BotSearchStats abStats;
    std::string abBest = abEngine.chooseMove(board, &abStats);
    CHECK_EQUAL(0ULL, abEngine.lastSearchDiagnostics().pvsReSearches);

    octi::OctiBotEngine pvsEngine(octi::BotDifficulty::TACTICAL);
    octi::BotTuningProfile pvsProf = prof;
    pvsProf.usePvs = true;
    pvsEngine.setTuningProfile(pvsProf);
    octi::BotSearchStats pvsStats;
    std::string pvsBest = pvsEngine.chooseMove(board, &pvsStats);

    CHECK_TRUE(pvsEngine.lastSearchDiagnostics().pvsReSearches > 0ULL);
    CHECK_TRUE(pvsStats.nodesVisited < abStats.nodesVisited);
    CHECK_EQUAL(abStats.bestScore, pvsStats.bestScore);
    STRCMP_EQUAL(abBest.c_str(), pvsBest.c_str());
}

TEST(OctiEnvelope4SearchTest, AspirationWindowWidensOnFailLowAndFailHighAndMatchesFullWindow)
{
    const char *kFens[3] = {
        "2:3:0:0:P:A:-:0:3,0,18,0:3,0,18,0:A44abh,A55ab,A73a,B46def,B57ef,B77f",
        "2:3:0:0:P:B:-:0:3,0,17,0:3,0,16,0:B51def,B33,A53,A54abegh,A64abfh,A55afh,B46de,B37",
        "OFK:1:0:0:P:A:-:0:0,0,8,0:0,0,6,0:A22,A32,A42,A64abdh,B26de,B36de,B46,B56ef"
    };

    std::uint64_t totalFailLows = 0;
    std::uint64_t totalFailHighs = 0;
    std::uint64_t totalReSearches = 0;

    for (const char *fen : kFens) {
        octi::OctiBoard board;
        board.suppressBoardUpdateNotification(true);
        board.fromPositionString(fen);

        octi::BotTuningProfile fullProf =
            octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::TACTICAL);
        fullProf.useOpeningBook = false;
        fullProf.rootScoreJitter = 0;
        fullProf.lazySmpThreads = 1;
        fullProf.compositeMoveMaxPly = -1;
        fullProf.rootBranchCap = 18;
        fullProf.treeBranchCap = 14;
        fullProf.maxSearchDepth = 3;
        fullProf.timeBudgetMs = 60000;

        octi::OctiBotEngine fullEngine(octi::BotDifficulty::TACTICAL);
        fullEngine.setTuningProfile(fullProf);
        octi::BotSearchStats fullStats;
        std::string fullMove = fullEngine.chooseMove(board, &fullStats);
        CHECK_EQUAL(0ULL, fullEngine.lastSearchDiagnostics().aspirationReSearches);

        octi::OctiBotEngine aspEngine(octi::BotDifficulty::TACTICAL);
        octi::BotTuningProfile aspProf = fullProf;
        aspProf.aspirationWindowCp = 5; // Narrow 5-cp window forces fail-low & fail-high
        aspEngine.setTuningProfile(aspProf);
        octi::BotSearchStats aspStats;
        std::string aspMove = aspEngine.chooseMove(board, &aspStats);

        CHECK_EQUAL(fullStats.bestScore, aspStats.bestScore);
        STRCMP_EQUAL(fullMove.c_str(), aspMove.c_str());

        octi::BotSearchDiagnostics d = aspEngine.lastSearchDiagnostics();
        totalFailLows += d.aspirationFailLows;
        totalFailHighs += d.aspirationFailHighs;
        totalReSearches += d.aspirationReSearches;
    }

    CHECK_TRUE(totalFailLows > 0ULL);
    CHECK_TRUE(totalFailHighs > 0ULL);
    CHECK_TRUE(totalReSearches >= totalFailLows + totalFailHighs);
}

TEST(OctiEnvelope4SearchTest, KillersAndHistoryUpdateOnQuietCutoffsAndLazySmp8ThreadsRaceFree)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.suppressBoardUpdateNotification(true);
    board.fromPositionString(
        "2:3:0:0:P:A:-:0:3,0,16,0:3,0,16,0:A44abh,A53ab,A73a,B46def,B57ef,B77f");

    // 1. Single-thread deterministic check of killer updates, killer hits, and history updates
    {
        octi::OctiBotEngine engine(octi::BotDifficulty::TACTICAL);
        octi::BotTuningProfile prof = engine.tuningProfile();
        prof.useOpeningBook = false;
        prof.rootScoreJitter = 0;
        prof.lazySmpThreads = 1;
        prof.maxSearchDepth = 4;
        prof.rootBranchCap = 16;
        prof.treeBranchCap = 14;
        prof.useKillers = true;
        prof.useHistory = true;
        prof.timeBudgetMs = 60000;
        engine.setTuningProfile(prof);

        octi::BotSearchStats stats;
        std::string best = engine.chooseMove(board, &stats);
        CHECK_FALSE(best.empty());
        CHECK_EQUAL(4, stats.depthReached);

        octi::BotSearchDiagnostics diag = engine.lastSearchDiagnostics();
        CHECK_TRUE(diag.killerUpdates > 0ULL);
        CHECK_TRUE(diag.killerHits > 0ULL);
        CHECK_TRUE(diag.historyUpdates > 0ULL);
    }

    // 2. 8-thread Lazy SMP with per-thread killer/history tables and diversified helpers
    {
        octi::OctiBotEngine smpEngine(octi::BotDifficulty::MASTER);
        octi::BotTuningProfile smpProf = smpEngine.tuningProfile();
        smpProf.useOpeningBook = false;
        smpProf.rootScoreJitter = 0;
        smpProf.lazySmpThreads = 8;
        smpProf.lazySmpDiversifyHelpers = true;
        smpProf.usePvs = true;
        smpProf.aspirationWindowCp = 250;
        smpProf.useKillers = true;
        smpProf.useHistory = true;
        smpProf.lmrMinDepth = 3;
        smpProf.lmrMinMoveIndex = 3;
        smpProf.nullMoveMinDepth = 3;
        smpProf.nullMoveReduction = 2;
        smpProf.qsearchUseTt = true;
        smpProf.maxSearchDepth = 6;
        smpProf.timeBudgetMs = 550; // >= 500ms spawns all 7 helper threads
        smpEngine.setTuningProfile(smpProf);

        octi::BotSearchStats smpStats;
        std::string smpBest = smpEngine.chooseMove(board, &smpStats);
        CHECK_FALSE(smpBest.empty());
        CHECK_TRUE(smpStats.depthReached >= 1);
        CHECK_TRUE(smpStats.nodesVisited > 50ULL);

        octi::BotSearchDiagnostics smpDiag = smpEngine.lastSearchDiagnostics();
        CHECK_TRUE(smpDiag.killerUpdates > 0ULL);
        CHECK_TRUE(smpDiag.historyUpdates > 0ULL);

        std::vector<octi::OctiMove> legal = board.getGame()->generateLegalMoves();
        bool isLegal = false;
        for (const auto &m : legal) {
            if (m.asString() == smpBest) {
                isLegal = true;
                break;
            }
        }
        CHECK_TRUE(isLegal);
    }
}

TEST(OctiEnvelope4SearchTest, LmrReducesLateQuietMovesAndReSearchesWhenBeatingAlpha)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.suppressBoardUpdateNotification(true);
    board.fromPositionString(
        "2:3:0:0:P:B:-:0:1,0,20,0:4,0,17,0:"
        "A33c,A53,A73,A34ab,A54a,A75b,B56cdef,B57ef,B77ef");

    octi::OctiBotEngine engine(octi::BotDifficulty::TACTICAL);
    octi::BotTuningProfile prof = engine.tuningProfile();
    prof.useOpeningBook = false;
    prof.rootScoreJitter = 0;
    prof.lazySmpThreads = 1;
    prof.compositeMoveMaxPly = -1;
    prof.rootBranchCap = 12;
    prof.treeBranchCap = 12;
    prof.maxSearchDepth = 4; // Root d=4 enters interior negamax at depth=3 >= lmrMinDepth
    prof.lmrMinDepth = 3;
    prof.lmrMinMoveIndex = 2; // Reduce quiet moves from index 2 at interior depth >= 3
    prof.timeBudgetMs = 60000;
    engine.setTuningProfile(prof);

    octi::BotSearchStats stats;
    std::string best = engine.chooseMove(board, &stats);
    CHECK_FALSE(best.empty());
    CHECK_EQUAL(4, stats.depthReached);

    octi::BotSearchDiagnostics diag = engine.lastSearchDiagnostics();
    CHECK_TRUE(diag.lmrReductions > 0ULL);
    CHECK_TRUE(diag.lmrReSearches > 0ULL);
    CHECK_TRUE(diag.lmrReductions > diag.lmrReSearches);
}

TEST(OctiEnvelope4SearchTest, QuiescenceTtReducesNodesOnTranspositionsAndTtSizeLog2ResizesCleanly)
{
    // Two independent jump-capture lanes (A34ab->36x and A64ab->66x, plus B counter-captures)
    // transpose in quiescence search depending on capture order.
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.suppressBoardUpdateNotification(true);
    board.fromPositionString(
        "2:3:0:0:P:A:-:0:2,0,12,0:2,0,12,0:"
        "A34ab,A64ab,A53a,B35ef,B65ef,B47def,B77def");

    octi::BotTuningProfile baseProf =
        octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::TACTICAL);
    baseProf.useOpeningBook = false;
    baseProf.rootScoreJitter = 0;
    baseProf.lazySmpThreads = 1;
    baseProf.compositeMoveMaxPly = -1;
    baseProf.maxSearchDepth = 2;
    baseProf.maxQuiesceDepth = 5;
    baseProf.quiesceBranchCap = 0;
    baseProf.rootBranchCap = 24;
    baseProf.treeBranchCap = 24;
    baseProf.timeBudgetMs = 60000;

    octi::OctiBotEngine qOffEngine(octi::BotDifficulty::TACTICAL);
    qOffEngine.setTuningProfile(baseProf);
    octi::BotSearchStats statsOff;
    std::string moveOff = qOffEngine.chooseMove(board, &statsOff);
    CHECK_EQUAL(0ULL, qOffEngine.lastSearchDiagnostics().qsearchTtProbes);
    CHECK_EQUAL(0ULL, qOffEngine.lastSearchDiagnostics().qsearchTtHits);

    octi::OctiBotEngine qOnEngine(octi::BotDifficulty::TACTICAL);
    octi::BotTuningProfile onProf = baseProf;
    onProf.qsearchUseTt = true;
    qOnEngine.setTuningProfile(onProf);
    octi::BotSearchStats statsOn;
    std::string moveOn = qOnEngine.chooseMove(board, &statsOn);

    octi::BotSearchDiagnostics diagOn = qOnEngine.lastSearchDiagnostics();
    CHECK_TRUE(diagOn.qsearchTtProbes > 0ULL);
    CHECK_TRUE(diagOn.qsearchTtHits > 0ULL);
    CHECK_TRUE(statsOn.nodesVisited < statsOff.nodesVisited);
    CHECK_EQUAL(statsOff.bestScore, statsOn.bestScore);
    STRCMP_EQUAL(moveOff.c_str(), moveOn.c_str());

    // Verify dynamic TT resizing via ttSizeLog2 and bounds clamping
    CHECK_EQUAL(std::size_t{1} << 18, qOnEngine.transpositionTableEntryCount());
    onProf.ttSizeLog2 = 20;
    qOnEngine.setTuningProfile(onProf);
    CHECK_EQUAL(std::size_t{1} << 20, qOnEngine.transpositionTableEntryCount());

    onProf.ttSizeLog2 = 8; // Clamped to MIN_TT_SIZE_LOG2 = 12
    qOnEngine.setTuningProfile(onProf);
    CHECK_EQUAL(std::size_t{1} << 12, qOnEngine.transpositionTableEntryCount());
}

namespace {

static const char *const kE3bRef24Fens[24] = {
    "2:3:1:0:P:B:-:0:4,0,17,0:4,0,18,0:A33abh,B53ade!,B53e,B37def,A77a!be,A77ab",
    "2:3:1:0:P:B:-:0:3,0,16,0:4,0,12,0:A42abfh,B72abde!,A53h,B73e,B76defg,A37a!beh,A57abch",
    "2:3:1:0:P:B:-:0:4,0,20,0:4,1,19,0:B52ade!,A33ab,B37def,A77a!eh,A77",
    "2:3:1:0:P:A:-:0:4,0,17,0:4,0,17,0:B72ae!f,A53abh,B73ef,A37ab,B57def,A38a!be",
    "2:3:1:0:P:A:-:0:4,0,23,0:4,1,23,0:A33,A53a,B34e!,B57e,A78a!",
    "2:3:1:0:P:B:-:0:4,0,18,0:4,1,18,0:B33ade!h,B73efg,A37abc,A77a!bde,A77",
    "2:1:1:0:P:B:-:0:4,1,21,0:4,0,24,0:B32e!,A53ab,A73a,B57,B77e!",
    "2:1:1:0:P:B:-:0:4,0,22,0:4,0,24,0:A33b!,A53,A73ah,B34e!,B57,B77",
    "2:1:1:0:P:B:-:0:4,1,19,0:4,0,22,0:B32e!,A53adh,A73ah,B66d!ef,B77",
    "2:1:1:0:P:B:-:0:4,2,19,1:4,1,23,0:A71abdh,B32e!,B77de",
    "2:1:1:0:P:B:-:0:4,1,20,0:4,0,23,0:B32e!,A53abh,A73a,B57e!f,B77",
    "2:1:1:0:P:B:-:0:4,1,20,0:4,0,23,0:B32e!,A53ah,A73ah,B57e!f,B77",
    "4:3:1:0:P:A:-:0:3,0,11,0:2,2,10,0:2,1,10,0:3,0,9,0:C27fg!,A38a!be!,D78a!eh",
    "4:3:1:0:P:D:-:3:3,1,11,0:0,4,10,0:2,0,9,0:2,0,10,1:D33ab!g!h,D73,C37cfg!,C77",
    "4:3:1:0:P:D:-:2:0,3,10,0:2,2,9,0:3,0,11,0:3,0,10,0:D23acg!h!,A33,C37ce!fg!",
    "4:3:1:0:P:D:-:1:3,1,10,0:2,2,7,0:3,0,12,1:3,0,9,0:D33abfg!h!,C36efg!h",
    "4:3:1:0:P:C:-:3:3,1,10,1:2,2,11,1:2,0,8,0:2,1,7,0:D33agh,C66efg,C37ce!fg!",
    "4:3:1:0:P:C:-:2:3,0,11,0:3,1,10,0:3,0,9,0:3,0,9,0:D73a!gh,A37a!bc!,C77e!fg",
    "2:3:1:1:P:A:-:0:4,0,15,0:4,0,14,0:B33cde!f,A73abh,A44abh,B65defg,A37a!bch,B77def",
    "2:3:1:1:P:A:-:0:4,0,17,0:4,0,17,0:B53ade!,B53ef,A73abh,A37ab,B77def,A38a!be",
    "2:3:1:1:P:B:-:0:4,0,25,0:2,4,14,0:B86bdef!,A37ab,A57abcf!h,A57",
    "2:3:1:1:P:B:-:0:2,0,6,0:3,0,7,0:B53cde!fg,A73abch,A24abh,A34abch,A35abch,B95cdef,B36cdef,B37cdefg,A57a!bch",
    "2:3:1:1:P:B:-:0:4,0,18,0:4,0,19,0:B33de,B33,A74a!bde,B36ae!fh,A77abh,A77",
    "2:3:1:1:P:B:-:0:4,1,21,0:4,1,21,0:B32ade!,A73a,A57a!eh,B77d"
};

struct E3bSpVariantSpec {
    octi::OctiGameMode mode;
    bool edgeless;
    octi::OctiGameBases bases;
};

static const E3bSpVariantSpec kE3bSpVariants8[8] = {
    { octi::GAME_MODE_2, false, octi::GAME_BASES_3 },
    { octi::GAME_MODE_2, true,  octi::GAME_BASES_3 },
    { octi::GAME_MODE_2, false, octi::GAME_BASES_1 },
    { octi::GAME_MODE_2, true,  octi::GAME_BASES_1 },
    { octi::GAME_MODE_4, false, octi::GAME_BASES_3 },
    { octi::GAME_MODE_4, true,  octi::GAME_BASES_3 },
    { octi::GAME_MODE_4, false, octi::GAME_BASES_1 },
    { octi::GAME_MODE_4, true,  octi::GAME_BASES_1 }
};

std::uint64_t e3bFnvMix64(std::uint64_t h, std::uint64_t v)
{
    for (int b = 0; b < 8; ++b) {
        h ^= ((v >> (b * 8)) & 0xFFULL);
        h *= 1099511628211ULL;
    }
    return h;
}

std::uint64_t e3bHashMoveListOpsAndKeys(const std::vector<octi::OctiMove> &moves)
{
    std::uint64_t h = 14695981039346656037ULL;
    h = e3bFnvMix64(h, static_cast<std::uint64_t>(moves.size()));
    for (const auto &m : moves) {
        h = e3bFnvMix64(h, m.structuralKey());
        const auto &ops = m.getOps();
        h = e3bFnvMix64(h, static_cast<std::uint64_t>(ops.size()));
        for (const auto &op : ops) {
            std::uint64_t packed =
                static_cast<std::uint64_t>(op.x0 & 0xFF) |
                (static_cast<std::uint64_t>(op.y0 & 0xFF) << 8) |
                (static_cast<std::uint64_t>(static_cast<unsigned char>(op.addProngDir)) << 16) |
                (static_cast<std::uint64_t>(static_cast<unsigned char>(op.rmProngDir)) << 24) |
                ((op.addPod ? 1ULL : 0ULL) << 32) |
                ((op.liberate ? 1ULL : 0ULL) << 33) |
                ((op.superProng ? 1ULL : 0ULL) << 34) |
                ((op.isSuperProngBonusMove ? 1ULL : 0ULL) << 35) |
                (static_cast<std::uint64_t>(op.hops.size() & 0xFFF) << 36);
            h = e3bFnvMix64(h, packed);
            h = e3bFnvMix64(h, static_cast<std::uint64_t>(op.pod.rawConfig()));
            for (const auto &hop : op.hops) {
                std::uint64_t hp =
                    static_cast<std::uint64_t>(hop.x & 0xFF) |
                    (static_cast<std::uint64_t>(hop.y & 0xFF) << 8) |
                    ((hop.capture ? 1ULL : 0ULL) << 16);
                h = e3bFnvMix64(h, hp);
            }
        }
    }
    return h;
}

std::string e3bJoinMoveStrings(const std::vector<octi::OctiMove> &moves)
{
    std::string out;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        if (i > 0) {
            out.push_back(' ');
        }
        out += moves[i].asString();
    }
    return out;
}

std::filesystem::path e3bResolveGoldenFixturePath()
{
    std::error_code ec;
    std::filesystem::path fromSource =
        std::filesystem::path(__FILE__).parent_path() / "fixtures" / "sp_movegen_golden.tsv";
    if (std::filesystem::exists(fromSource, ec)) {
        return fromSource;
    }
    const char *const kRelCandidates[] = {
        "test/fixtures/sp_movegen_golden.tsv",
        "../test/fixtures/sp_movegen_golden.tsv",
        "core/test/fixtures/sp_movegen_golden.tsv"
    };
    for (const char *rel : kRelCandidates) {
        std::filesystem::path p(rel);
        if (std::filesystem::exists(p, ec)) {
            return p;
        }
    }
    return fromSource;
}

struct E3bGoldenRecord {
    int index = -1;
    std::string fen;
    std::string legalOpsHashHex;
    std::string legalJoined;
    std::string tacticalOpsHashHex;
    std::string tacticalJoined;
};

} // namespace

TEST_GROUP(OctiEnvelope3bSpMoveGenTest)
{
};

TEST(OctiEnvelope3bSpMoveGenTest, GoldenMoveListOrderOpsAndStringsAcrossRef24And1000RandomPlayouts)
{
    std::filesystem::path goldenPath = e3bResolveGoldenFixturePath();
    std::ifstream ifs(goldenPath);
    CHECK_TEXT(ifs.is_open(), "Failed to open sp_movegen_golden.tsv fixture");

    std::vector<E3bGoldenRecord> golden;
    golden.reserve(1024);
    std::string rawLine;
    while (std::getline(ifs, rawLine)) {
        if (rawLine.empty() || rawLine[0] == '#') {
            continue;
        }
        std::vector<std::string> cols;
        cols.reserve(6);
        std::size_t start = 0;
        while (true) {
            std::size_t tab = rawLine.find('\t', start);
            if (tab == std::string::npos) {
                cols.push_back(rawLine.substr(start));
                break;
            }
            cols.push_back(rawLine.substr(start, tab - start));
            start = tab + 1;
        }
        CHECK_EQUAL(6u, cols.size());
        E3bGoldenRecord rec;
        rec.index = std::stoi(cols[0]);
        rec.fen = std::move(cols[1]);
        rec.legalOpsHashHex = std::move(cols[2]);
        rec.legalJoined = std::move(cols[3]);
        rec.tacticalOpsHashHex = std::move(cols[4]);
        rec.tacticalJoined = std::move(cols[5]);
        golden.push_back(std::move(rec));
    }
    CHECK_EQUAL(1024u, golden.size());

    std::size_t cursor = 0;
    auto verifyPositionAgainstGolden = [&](const octi::OctiBoard &board, bool parseEveryMoveString) {
        CHECK_TRUE(cursor < golden.size());
        const E3bGoldenRecord &exp = golden[cursor];
        CHECK_EQUAL(static_cast<int>(cursor), exp.index);

        std::string actualFen = board.toPositionString();
        STRCMP_EQUAL(exp.fen.c_str(), actualFen.c_str());

        const octi::OctiGame *game = board.getGame();
        CHECK_TRUE(game != nullptr);

        std::vector<octi::OctiMove> legal;
        game->generateLegalMoves(legal);
        std::string actualLegalJoined = e3bJoinMoveStrings(legal);
        CHECK_TRUE(actualLegalJoined == exp.legalJoined);

        std::ostringstream lHashHex;
        lHashHex << std::hex << e3bHashMoveListOpsAndKeys(legal);
        STRCMP_EQUAL(exp.legalOpsHashHex.c_str(), lHashHex.str().c_str());

        if (parseEveryMoveString) {
            std::istringstream ms(exp.legalJoined);
            std::string tok;
            std::size_t mIdx = 0;
            while (ms >> tok) {
                CHECK_TRUE(mIdx < legal.size());
                STRCMP_EQUAL(tok.c_str(), legal[mIdx].asString().c_str());
                octi::OctiMove parsed(tok);
                CHECK_EQUAL(parsed.structuralKey(), legal[mIdx].structuralKey());
                CHECK_EQUAL(legal[mIdx].getOps().size(), parsed.getOps().size());
                for (std::size_t opIdx = 0; opIdx < parsed.getOps().size(); ++opIdx) {
                    const octi::OctiMoveOp &opA = legal[mIdx].getOps()[opIdx];
                    const octi::OctiMoveOp &opB = parsed.getOps()[opIdx];
                    CHECK_EQUAL(opA.x0, opB.x0);
                    CHECK_EQUAL(opA.y0, opB.y0);
                    CHECK_EQUAL(opA.pod.rawConfig() & 0xFFFFu,
                                opB.pod.rawConfig() & 0xFFFFu);
                    CHECK_EQUAL(opA.addPod, opB.addPod);
                    CHECK_EQUAL(opA.liberate, opB.liberate);
                    CHECK_EQUAL(opA.addProngDir, opB.addProngDir);
                    CHECK_EQUAL(opA.rmProngDir, opB.rmProngDir);
                    CHECK_EQUAL(opA.superProng, opB.superProng);
                    CHECK_EQUAL(opA.isSuperProngBonusMove, opB.isSuperProngBonusMove);
                    CHECK_TRUE(opA.hops == opB.hops);
                }
                ++mIdx;
            }
            CHECK_EQUAL(legal.size(), mIdx);
        }

        std::vector<octi::OctiMove> tactical;
        game->generateTacticalMoves(tactical);
        std::string actualTacticalJoined = e3bJoinMoveStrings(tactical);
        CHECK_TRUE(actualTacticalJoined == exp.tacticalJoined);

        std::ostringstream tHashHex;
        tHashHex << std::hex << e3bHashMoveListOpsAndKeys(tactical);
        STRCMP_EQUAL(exp.tacticalOpsHashHex.c_str(), tHashHex.str().c_str());

        ++cursor;
    };

    // 1. All 24 reference superprong bench positions (with full OctiMove string round-trip equality)
    for (int i = 0; i < 24; ++i) {
        octi::OctiBoard board;
        board.suppressBoardUpdateNotification(true);
        board.fromPositionString(kE3bRef24Fens[i]);
        verifyPositionAgainstGolden(board, true);
    }

    // 2. 1,000 positions from seeded random superprong playouts across the 8 superprong variants (125 each):
    //    - 2P variants (v = 0..3, 500 positions) verify bit-for-bit against sp_movegen_golden.tsv[24..523];
    //    - 4P variants (v = 4..7, 500 positions) run under single-owner 4P stacking rules (where pre-O6c
    //      golden[524..1023] allowed partner stacking) and verify single-owner invariants, tactical subset
    //      inclusion, and OctiMove string/ops/validator parity.
    std::size_t fourPlayerPlayoutVerified = 0;
    for (int v = 0; v < 8; ++v) {
        const E3bSpVariantSpec &cfg = kE3bSpVariants8[v];
        std::mt19937_64 rng(0xE3B502026ULL + static_cast<std::uint64_t>(v) * 0x9E3779B97F4A7C15ULL);
        int collected = 0;
        while (collected < 125) {
            octi::OctiBoard board(cfg.mode, true, cfg.edgeless, cfg.bases);
            board.suppressBoardUpdateNotification(true);
            octi::OctiGame *game = board.getGame();
            game->setSearchMode(true);
            game->start(octi::PLAYER_A);

            int ply = 0;
            while (ply < 160 && !game->isGameOver() && collected < 125) {
                std::vector<octi::OctiMove> legal;
                game->generateLegalMoves(legal);
                if (legal.empty()) {
                    break;
                }
                if (ply >= 4 && ((ply - 4) % 3 == 0)) {
                    if (cfg.mode == octi::GAME_MODE_2) {
                        verifyPositionAgainstGolden(board, (collected % 25 == 0));
                    } else {
                        for (int bx = 0; bx < board.getBoardWidth(); ++bx) {
                            for (int by = 0; by < board.getBoardHeight(); ++by) {
                                const octi::OctiSquare &sq = board.squareAt(bx, by);
                                if (!sq.isEmpty()) {
                                    octi::OctiPlayer sqOwn =
                                        octi::podTypeToPlayer(sq.getPodAt(0).getType());
                                    CHECK_FALSE(sq.hasOtherOwnerThan(sqOwn));
                                }
                            }
                        }
                        std::unordered_set<std::string> legalSet;
                        legalSet.reserve(legal.size());
                        for (const auto &lm : legal) {
                            legalSet.insert(lm.asString());
                        }
                        std::vector<octi::OctiMove> tactical;
                        game->generateTacticalMoves(tactical);
                        for (const auto &tm : tactical) {
                            CHECK_TRUE(legalSet.count(tm.asString()) > 0);
                        }
                        if (collected % 25 == 0) {
                            const std::string curFen = board.toPositionString();
                            for (std::size_t mi = 0; mi < legal.size(); ++mi) {
                                octi::OctiMove parsed(legal[mi].asString());
                                CHECK_EQUAL(legal[mi].structuralKey(), parsed.structuralKey());
                                CHECK_EQUAL(legal[mi].getOps().size(), parsed.getOps().size());
                                if (mi < 8) {
                                    octi::OctiBoard stepB;
                                    stepB.suppressBoardUpdateNotification(true);
                                    stepB.fromPositionString(curFen);
                                    stepB.getGame()->setSearchMode(true);
                                    stepB.getGame()->move(parsed);
                                }
                            }
                        }
                        ++fourPlayerPlayoutVerified;
                    }
                    ++collected;
                    if (collected >= 125) {
                        break;
                    }
                }
                std::size_t pickIdx = 0;
                if (game->getFreeSuperProngs(game->getTurn()) > 0 && (rng() & 1ULL) != 0ULL) {
                    std::vector<std::size_t> spArmIdx;
                    for (std::size_t mi = 0; mi < legal.size(); ++mi) {
                        const auto &ops = legal[mi].getOps();
                        if (ops.size() == 1 && ops[0].superProng && ops[0].addProngDir != octi::DIR_UNDEF) {
                            spArmIdx.push_back(mi);
                        }
                    }
                    if (!spArmIdx.empty()) {
                        pickIdx = spArmIdx[static_cast<std::size_t>(rng() % spArmIdx.size())];
                    } else {
                        pickIdx = static_cast<std::size_t>(rng() % legal.size());
                    }
                } else {
                    pickIdx = static_cast<std::size_t>(rng() % legal.size());
                }
                game->move(legal[pickIdx]);
                ++ply;
            }
        }
    }

    CHECK_EQUAL(524u, cursor);
    CHECK_EQUAL(500u, fourPlayerPlayoutVerified);
}

// ---------------------------------------------------------------------------
// Envelope 5 Evaluation Breakdown & Candidate Tuning Profile Fields
// ---------------------------------------------------------------------------

TEST_GROUP(OctiEnvelope5EvalTest)
{
};

TEST(OctiEnvelope5EvalTest, BreakdownSumEqualsEvaluatePositionAcrossVariantsAndTerminalStates)
{
    const char *const kTestFens[] = {
        "2:3:0:0:P:A:-:0:4,0,22,0:4,0,22,0:A33,A53ab,A73a,B37e,B57e,B77f",
        "2:3:0:0:P:A:-:0:4,1,14,0:4,1,17,0:B45acdefg,B75bdefgh,A57abh,A28abdh",
        "2:3:0:0:P:A:-:0:3,1,12,0:3,1,20,0:B56acdgh,A37abdefh,A37,A37,B67abcdefg,B77",
        "2:1:0:0:P:B:-:0:2,0,22,0:4,0,22,0:A33b,A33ac,A33,A53,A73,B66df,B37,B77f",
        "OFK:1:0:0:P:A:-:0:0,0,7,0:0,0,7,0:A32ab,A42h,A52,A33ab,B55cdefh,B26,B36,B46",
        "OFK:1:0:0:P:A:-:0:0,0,9,0:0,0,10,0:A22b,A32a,A52,A33h,B34df,B26,B46,B56",
        "4:3:0:0:P:A:-:0:3,0,11,0:3,0,11,0:3,0,11,0:3,0,11,0:A33ab,B37ef,C77ef,D73cd",
        "2:3:1:1:P:A:-:0:4,0,15,0:4,0,14,0:B33cde!f,A73abh,A44abh,B65defg,A37a!bch,B77def"
    };

    octi::BotTuningProfile masterProf =
        octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::MASTER);
    octi::BotTuningProfile gmProf =
        octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::GRANDMASTER);

    for (const char *fen : kTestFens) {
        octi::OctiBoard board;
        board.suppressBoardUpdateNotification(true);
        board.fromPositionString(fen);
        const octi::OctiGame *game = board.getGame();
        CHECK_TRUE(game != nullptr);

        for (octi::OctiPlayer p : {octi::PLAYER_A, octi::PLAYER_B}) {
            octi::OctiEvalBreakdown bdDefault =
                octi::OctiBotEngine::evaluatePositionBreakdown(board, p);
            octi::OctiEvalBreakdown bdGameDefault =
                octi::OctiBotEngine::evaluatePositionBreakdown(*game, p, nullptr);
            octi::OctiEvalBreakdown bdMaster =
                octi::OctiBotEngine::evaluatePositionBreakdown(board, p, masterProf);
            octi::OctiEvalBreakdown bdGm =
                octi::OctiBotEngine::evaluatePositionBreakdown(*game, p, &gmProf);

            int evalDefault = octi::OctiBotEngine::evaluatePosition(board, p);
            int evalGameDefault = octi::OctiBotEngine::evaluatePosition(*game, p);
            int evalGm = octi::OctiBotEngine::evaluatePosition(*game, p, gmProf);

            CHECK_EQUAL(bdDefault.sumTerms(), bdDefault.totalScore);
            CHECK_EQUAL(evalDefault, bdDefault.totalScore);
            CHECK_EQUAL(evalDefault, evalGameDefault);
            CHECK_EQUAL(bdDefault.totalScore, bdGameDefault.totalScore);
            CHECK_EQUAL(bdDefault.totalScore, bdMaster.totalScore);
            CHECK_EQUAL(bdGm.sumTerms(), bdGm.totalScore);
            CHECK_EQUAL(evalGm, bdGm.totalScore);
        }
    }

    // Terminal position breakdown check
    octi::OctiBoard winBoard(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
    winBoard.suppressBoardUpdateNotification(true);
    winBoard.fromPositionString("2:1:0:0:P:A:-:0:4,0,22,0:4,0,22,0:A56a,B37");
    winBoard.getGame()->move("56a-57");
    CHECK_TRUE(winBoard.getGame()->isGameOver());
    octi::OctiEvalBreakdown winBdA =
        octi::OctiBotEngine::evaluatePositionBreakdown(winBoard, octi::PLAYER_A);
    octi::OctiEvalBreakdown winBdB =
        octi::OctiBotEngine::evaluatePositionBreakdown(winBoard, octi::PLAYER_B);
    CHECK_EQUAL(winBdA.sumTerms(), winBdA.totalScore);
    CHECK_EQUAL(winBdB.sumTerms(), winBdB.totalScore);
    CHECK_EQUAL(1000000, winBdA.terminalScore);
    CHECK_EQUAL(-1000000, winBdB.terminalScore);
    CHECK_EQUAL(octi::OctiBotEngine::evaluatePosition(winBoard, octi::PLAYER_A),
                winBdA.totalScore);
}

TEST(OctiEnvelope5EvalTest, CandidateTuningProfileFieldsModifyTargetTermsAndPreserveDefaults)
{
    octi::BotTuningProfile defProf;
    CHECK_EQUAL(100, defProf.contestedEnemyBaseThreatScalePct);
    CHECK_EQUAL(-1, defProf.ofkSinglePodMonopolyPenalty);
    CHECK_EQUAL(0, defProf.enemyHeavyPodThreatPenalty);
    CHECK_EQUAL(900, defProf.unarmedHomePodPenalty);
    CHECK_EQUAL(450, defProf.canLiberateBonus);
    CHECK_EQUAL(450, defProf.reserveFreePodVal);
    CHECK_EQUAL(3800, defProf.capturedPodPenalty);
    CHECK_EQUAL(140, defProf.reserveFreeProngVal);

    // 1. OFK single-pod monopoly override + enemy heavy-pod threat penalty on G5 (OFK)
    {
        octi::OctiBoard bOfk;
        bOfk.suppressBoardUpdateNotification(true);
        bOfk.fromPositionString(
            "OFK:1:0:0:P:A:-:0:0,0,7,0:0,0,7,0:A32ab,A42h,A52,A33ab,B55cdefh,B26,B36,B46");
        octi::BotTuningProfile baseProf =
            octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::MASTER);
        octi::OctiEvalBreakdown bdBase =
            octi::OctiBotEngine::evaluatePositionBreakdown(bOfk, octi::PLAYER_A, baseProf);
        // Under default (-1), B55cdefh (5 prongs, 3 unarmed home pods) gives A +(5-2)*1400 = +4200 monopoly
        CHECK_EQUAL(4200, bdBase.singlePodMonopoly);
        CHECK_EQUAL(0, bdBase.heavyPodThreat);

        octi::BotTuningProfile tunedOfk = baseProf;
        tunedOfk.ofkSinglePodMonopolyPenalty = 200;
        tunedOfk.enemyHeavyPodThreatPenalty = 500;
        octi::OctiEvalBreakdown bdTuned =
            octi::OctiBotEngine::evaluatePositionBreakdown(bOfk, octi::PLAYER_A, tunedOfk);
        CHECK_EQUAL(600, bdTuned.singlePodMonopoly);
        CHECK_EQUAL(-1000, bdTuned.heavyPodThreat);
        CHECK_EQUAL(bdBase.totalScore - 3600 - 1000, bdTuned.totalScore);
    }

    // 2. Contested enemy base threat scaling + E16 unarmedHomePodPenalty / canLiberateBonus
    {
        octi::OctiBoard b2p;
        b2p.suppressBoardUpdateNotification(true);
        b2p.fromPositionString(
            "2:3:0:0:P:A:-:0:2,1,16,0:3,1,16,0:A33,A53,A46ab,B47def,B37,B77");
        octi::BotTuningProfile baseProf =
            octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::MASTER);
        octi::OctiEvalBreakdown bd100 =
            octi::OctiBotEngine::evaluatePositionBreakdown(b2p, octi::PLAYER_A, baseProf);
        CHECK_TRUE(bd100.oneStepBaseThreat > 0);
        CHECK_TRUE(bd100.twoPlyBaseThreat > 0);

        octi::BotTuningProfile contestedProf = baseProf;
        contestedProf.contestedEnemyBaseThreatScalePct = 50;
        octi::OctiEvalBreakdown bd50 =
            octi::OctiBotEngine::evaluatePositionBreakdown(b2p, octi::PLAYER_A, contestedProf);
        CHECK_TRUE(bd50.oneStepBaseThreat < bd100.oneStepBaseThreat);
        CHECK_TRUE(bd50.twoPlyBaseThreat < bd100.twoPlyBaseThreat);
        CHECK_TRUE(bd50.totalScore < bd100.totalScore);

        octi::BotTuningProfile e16OffProf = baseProf;
        e16OffProf.unarmedHomePodPenalty = 0;
        e16OffProf.canLiberateBonus = 2500;
        octi::OctiEvalBreakdown bdE16Off =
            octi::OctiBotEngine::evaluatePositionBreakdown(b2p, octi::PLAYER_A, e16OffProf);
        CHECK_EQUAL(bdE16Off.sumTerms(), bdE16Off.totalScore);
    }
}

namespace {

class ConstantOffsetLeafEvaluator : public octi::OctiLeafEvaluator
{
public:
    explicit ConstantOffsetLeafEvaluator(int offset)
        : _offset(offset)
    {
    }

    int evaluateLeaf(const octi::OctiBoard &board,
                     octi::OctiPlayer perspective,
                     int heuristicScore) const override
    {
        (void) board;
        (void) perspective;
        _calls.fetch_add(1, std::memory_order_relaxed);
        return heuristicScore + _offset;
    }

    int calls() const noexcept
    {
        return _calls.load(std::memory_order_relaxed);
    }

private:
    int _offset;
    mutable std::atomic<int> _calls{0};
};

class ThreadRecordingLeafEvaluator : public octi::OctiLeafEvaluator
{
public:
    int evaluateLeaf(const octi::OctiBoard &board,
                     octi::OctiPlayer perspective,
                     int heuristicScore) const override
    {
        (void) board;
        (void) perspective;
        _calls.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(_mtx);
        _threads.insert(std::this_thread::get_id());
        return heuristicScore;
    }

    int calls() const noexcept
    {
        return _calls.load(std::memory_order_relaxed);
    }

    std::size_t uniqueThreadCount() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _threads.size();
    }

private:
    mutable std::atomic<int> _calls{0};
    mutable std::mutex _mtx;
    mutable std::set<std::thread::id> _threads;
};

} // namespace

TEST_GROUP(OctiLeafEvaluatorHookTest)
{
};

TEST(OctiLeafEvaluatorHookTest, NullLeafEvaluatorPreservesSearchMovesScoresAndNodes)
{
    const char *fens[] = {
        "2:3:0:0:P:B:-:0:3,0,17,0:3,0,16,0:B51def,B33,A53,A54abegh,A64abfh,A55afh,B46de,B37",
        "OFK:1:0:0:P:A:-:0:0,1,8,0:0,0,9,0:A22,A32,A42,B44cef,B26d,B56,B47bde",
        "2:3:1:0:P:A:-:0:3,0,16,1:3,0,16,1:A33ab,A53a,B37ef,B57e"
    };

    for (const char *fen : fens) {
        octi::OctiBoard board;
        board.suppressBoardUpdateNotification(true);
        board.fromPositionString(fen);

        octi::BotTuningProfile prof =
            octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::MASTER);
        prof.useOpeningBook = false;
        prof.rootScoreJitter = 0;
        prof.lazySmpThreads = 1;
        prof.maxSearchDepth = 4;
        prof.timeBudgetMs = 300000;

        octi::OctiBotEngine engDefault(octi::BotDifficulty::MASTER, 0x12345678ULL);
        CHECK_TRUE(engDefault.leafEvaluator() == nullptr);
        engDefault.setTuningProfile(prof);
        octi::BotSearchStats statsDefault;
        std::string mvDefault = engDefault.chooseMove(board, &statsDefault);

        ConstantOffsetLeafEvaluator zeroEval(0);
        octi::OctiBotEngine engZero(octi::BotDifficulty::MASTER, 0x12345678ULL);
        engZero.setTuningProfile(prof);
        engZero.setLeafEvaluator(&zeroEval);
        CHECK_TRUE(engZero.leafEvaluator() == &zeroEval);

        octi::OctiBotEngine engCopied(engZero);
        CHECK_TRUE(engCopied.leafEvaluator() == &zeroEval);
        engCopied.setLeafEvaluator(nullptr);
        CHECK_TRUE(engCopied.leafEvaluator() == nullptr);

        octi::BotSearchStats statsZero;
        std::string mvZero = engZero.chooseMove(board, &statsZero);
        CHECK_TRUE(zeroEval.calls() > 0);
        STRCMP_EQUAL(mvDefault.c_str(), mvZero.c_str());
        CHECK_EQUAL(4, statsDefault.depthReached);
        CHECK_EQUAL(statsDefault.bestScore, statsZero.bestScore);
        CHECK_EQUAL(statsDefault.depthReached, statsZero.depthReached);
        CHECK_EQUAL(statsDefault.nodesVisited, statsZero.nodesVisited);
    }
}

TEST(OctiLeafEvaluatorHookTest, ConstantOffsetLeafEvaluatorShiftsLeafAndRootScores)
{
    octi::OctiBoard board;
    board.suppressBoardUpdateNotification(true);
    board.fromPositionString(
        "2:3:0:0:P:B:-:0:3,0,17,0:3,0,16,0:B51def,B33,A53,A54abegh,A64abfh,A55afh,B46de,B37");

    octi::BotTuningProfile prof =
        octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::MASTER);
    prof.useOpeningBook = false;
    prof.rootScoreJitter = 0;
    prof.lazySmpThreads = 1;
    prof.maxSearchDepth = 1;
    prof.maxQuiesceDepth = 0;
    prof.timeBudgetMs = 60000;

    octi::OctiBotEngine engBase(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    engBase.setTuningProfile(prof);
    octi::BotSearchStats statsBase;
    std::string mvBase = engBase.chooseMove(board, &statsBase);

    ConstantOffsetLeafEvaluator plus321(321);
    octi::OctiBotEngine engShift(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    engShift.setTuningProfile(prof);
    engShift.setLeafEvaluator(&plus321);
    octi::BotSearchStats statsShift;
    std::string mvShift = engShift.chooseMove(board, &statsShift);

    CHECK_TRUE(plus321.calls() > 0);
    STRCMP_EQUAL(mvBase.c_str(), mvShift.c_str());
    // At depth 1 with maxQuiesceDepth = 0 in 2P, root negates opponent's leaf score (+321 -> -321)
    CHECK_EQUAL(statsBase.bestScore - 321, statsShift.bestScore);
}

TEST(OctiLeafEvaluatorHookTest, LazySmpEightThreadsInvokeLeafEvaluatorConcurrentlyWithoutRaces)
{
    octi::OctiBoard board;
    board.suppressBoardUpdateNotification(true);
    board.fromPositionString(
        "2:3:0:0:P:B:-:0:3,0,17,0:3,0,16,0:B51def,B33,A53,A54abegh,A64abfh,A55afh,B46de,B37");

    octi::BotTuningProfile prof =
        octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::MASTER);
    prof.useOpeningBook = false;
    prof.rootScoreJitter = 0;
    prof.lazySmpThreads = 8;
    prof.maxSearchDepth = 20;
    prof.timeBudgetMs = 600;

    ThreadRecordingLeafEvaluator recorder;
    octi::OctiBotEngine eng(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
    eng.setTuningProfile(prof);
    eng.setLeafEvaluator(&recorder);

    octi::BotSearchStats stats;
    std::string bestMove = eng.chooseMove(board, &stats);
    CHECK_FALSE(bestMove.empty());
    CHECK_TRUE(recorder.calls() > 100);
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw >= 2) {
        CHECK_TRUE(recorder.uniqueThreadCount() >= 2);
    }
}

namespace {

class EvalBoardCountingEngine : public octi::OctiBotEngine
{
public:
    using octi::OctiBotEngine::OctiBotEngine;
    using octi::OctiBotEngine::evaluateBoard;

    int evaluateBoard(const octi::OctiBoard &board,
                      octi::OctiPlayer perspective) const override
    {
        _evalBoardCalls.fetch_add(1, std::memory_order_relaxed);
        return octi::OctiBotEngine::evaluateBoard(
            board, perspective, tuningProfile());
    }

    int evaluateBoard(const octi::OctiBoard &board,
                      octi::OctiPlayer perspective,
                      const octi::BotTuningProfile &profile) const override
    {
        _evalBoardCalls.fetch_add(1, std::memory_order_relaxed);
        return octi::OctiBotEngine::evaluateBoard(board, perspective, profile);
    }

    void resetEvalBoardCalls() noexcept
    {
        _evalBoardCalls.store(0, std::memory_order_relaxed);
        _injectedIllegalCount.store(0, std::memory_order_relaxed);
    }

    std::uint64_t evalBoardCalls() const noexcept
    {
        return _evalBoardCalls.load(std::memory_order_relaxed);
    }

    void setInjectIllegalMove(bool enable) noexcept
    {
        _injectIllegalMove = enable;
    }

    std::uint64_t injectedIllegalCount() const noexcept
    {
        return _injectedIllegalCount.load(std::memory_order_relaxed);
    }

protected:
    void generateSearchMoves(const octi::OctiGame &game,
                             octi::OctiPlayer turn,
                             int ply,
                             std::vector<octi::OctiMove> &outMoves) const override
    {
        octi::OctiBotEngine::generateSearchMoves(game, turn, ply, outMoves);
        const octi::OctiBoard *b = game.getBoard();
        if (_injectIllegalMove && b != nullptr && b->squareAt(0, 0).isEmpty()) {
            outMoves.emplace_back("11+a");
            _injectedIllegalCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void appendExtraQuiesceMoves(const octi::OctiBoard &board,
                                 const octi::OctiGame &game,
                                 octi::OctiGameMode mode,
                                 octi::OctiPlayer turn,
                                 int qDepth,
                                 std::vector<octi::OctiMove> &tacticalMoves) const override
    {
        octi::OctiBotEngine::appendExtraQuiesceMoves(
            board, game, mode, turn, qDepth, tacticalMoves);
        if (_injectIllegalMove && qDepth == 0 && board.squareAt(0, 0).isEmpty()) {
            tacticalMoves.emplace_back("11+a");
            _injectedIllegalCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    bool hasCustomMoveOrdering() const noexcept override
    {
        return _injectIllegalMove;
    }

    void adjustMoveOrderScores(
        const octi::OctiBoard &board,
        const octi::OctiGame &game,
        octi::OctiPlayer turn,
        int ply,
        bool inQuiesce,
        const std::vector<octi::OctiMove> &moves,
        std::vector<std::pair<int, std::size_t>> &scored) const override
    {
        (void)board;
        (void)game;
        (void)turn;
        (void)ply;
        (void)inQuiesce;
        if (!_injectIllegalMove) {
            return;
        }
        for (auto &entry : scored) {
            if (entry.second < moves.size() &&
                moves[entry.second].asString() == "11+a") {
                entry.first += 500000;
            }
        }
    }

private:
    mutable std::atomic<std::uint64_t> _evalBoardCalls{0};
    mutable std::atomic<std::uint64_t> _injectedIllegalCount{0};
    bool _injectIllegalMove = false;
};

class PlyStackAuditingLeafEvaluator : public octi::OctiLeafEvaluator
{
public:
    struct Summary {
        std::size_t threadCount = 0;
        std::uint64_t rootCalls = 0;
        std::uint64_t beforeMoveCalls = 0;
        std::uint64_t nullMovePushCalls = 0;
        std::uint64_t popCalls = 0;
        std::uint64_t leafAtPlyCalls = 0;
        std::uint64_t legacyEvaluateLeafCalls = 0;
        std::uint64_t violations = 0;
        int maxPlySeen = 0;
        bool allThreadsBalancedAtRoot = true;
    };

    PlyStackAuditingLeafEvaluator(bool requireHeuristic,
                                  const octi::BotTuningProfile &evalProfile,
                                  int scoreOffset = 0)
        : _requireHeuristic(requireHeuristic)
        , _evalProfile(evalProfile)
        , _scoreOffset(scoreOffset)
    {
    }

    bool requiresHeuristicScore() const noexcept override
    {
        return _requireHeuristic;
    }

    void onSearchRoot(const octi::OctiBoard &rootBoard) const override
    {
        std::lock_guard<std::mutex> lock(_mtx);
        ThreadState &st = _threads[std::this_thread::get_id()];
        const octi::OctiGame *g = rootBoard.getGame();
        if (g == nullptr) {
            ++_violations;
            return;
        }
        if (st.initialized && st.curPly != 0) {
            ++_violations;
        }
        st.initialized = true;
        st.curPly = 0;
        ++st.rootCalls;
        PlyFrame rootFrame;
        rootFrame.boardHash = octi::OctiBotEngine::computePositionHashStatic(rootBoard);
        rootFrame.noProgressPlies = g->getNoProgressPlies();
        rootFrame.hasBoard = true;
        st.frames.assign(1, rootFrame);
    }

    void onBeforeMove(const octi::OctiBoard &boardBefore,
                      const octi::OctiMove &move,
                      int parentPly) const override
    {
        std::lock_guard<std::mutex> lock(_mtx);
        ThreadState &st = _threads[std::this_thread::get_id()];
        const octi::OctiGame *g = boardBefore.getGame();
        if (!st.initialized || g == nullptr || move.isEmpty() ||
            parentPly != st.curPly || parentPly < 0 ||
            static_cast<std::size_t>(parentPly) >= st.frames.size()) {
            ++_violations;
            return;
        }
        verifyOrRecordBoardLocked(st.frames[static_cast<std::size_t>(parentPly)],
                                  boardBefore);
        st.frames.push_back(PlyFrame{});
        st.curPly = parentPly + 1;
        if (st.curPly > st.maxPlySeen) {
            st.maxPlySeen = st.curPly;
        }
        ++st.beforeMoveCalls;
    }

    void onPushNullMove(const octi::OctiBoard &boardAfter,
                        int childPly) const override
    {
        std::lock_guard<std::mutex> lock(_mtx);
        ThreadState &st = _threads[std::this_thread::get_id()];
        const octi::OctiGame *g = boardAfter.getGame();
        if (!st.initialized || g == nullptr || childPly != st.curPly + 1 ||
            static_cast<std::size_t>(childPly) != st.frames.size()) {
            ++_violations;
            return;
        }
        PlyFrame nullFrame;
        nullFrame.boardHash = octi::OctiBotEngine::computePositionHashStatic(boardAfter);
        nullFrame.noProgressPlies = g->getNoProgressPlies();
        nullFrame.hasBoard = true;
        st.frames.push_back(nullFrame);
        st.curPly = childPly;
        if (st.curPly > st.maxPlySeen) {
            st.maxPlySeen = st.curPly;
        }
        ++st.nullMovePushCalls;
    }

    void onPopPly(int restoredPly) const override
    {
        std::lock_guard<std::mutex> lock(_mtx);
        ThreadState &st = _threads[std::this_thread::get_id()];
        if (!st.initialized || restoredPly < 0 ||
            restoredPly != st.curPly - 1 ||
            st.frames.size() != static_cast<std::size_t>(st.curPly + 1)) {
            ++_violations;
            return;
        }
        st.frames.resize(static_cast<std::size_t>(restoredPly + 1));
        st.curPly = restoredPly;
        ++st.popCalls;
    }

    int evaluateLeaf(const octi::OctiBoard &board,
                     octi::OctiPlayer perspective,
                     int heuristicScore) const override
    {
        std::lock_guard<std::mutex> lock(_mtx);
        ++_legacyEvaluateLeafCalls;
        int base = _requireHeuristic
            ? heuristicScore
            : octi::OctiBotEngine::evaluatePosition(board, perspective, _evalProfile);
        return base + _scoreOffset;
    }

    int evaluateLeafAtPly(const octi::OctiBoard &board,
                          octi::OctiPlayer perspective,
                          int heuristicScore,
                          int ply) const override
    {
        const int expectedScore =
            octi::OctiBotEngine::evaluatePosition(board, perspective, _evalProfile);
        {
            std::lock_guard<std::mutex> lock(_mtx);
            ThreadState &st = _threads[std::this_thread::get_id()];
            const octi::OctiGame *g = board.getGame();
            if (!st.initialized || g == nullptr || ply != st.curPly || ply < 0 ||
                static_cast<std::size_t>(ply) >= st.frames.size()) {
                ++_violations;
            } else {
                verifyOrRecordBoardLocked(st.frames[static_cast<std::size_t>(ply)],
                                          board);
            }
            if (_requireHeuristic) {
                if (heuristicScore != expectedScore) {
                    ++_violations;
                }
            } else if (heuristicScore != 0) {
                ++_violations;
            }
            ++st.leafAtPlyCalls;
        }
        return (_requireHeuristic ? heuristicScore : expectedScore) + _scoreOffset;
    }

    Summary summary() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        Summary out;
        out.threadCount = _threads.size();
        out.legacyEvaluateLeafCalls = _legacyEvaluateLeafCalls;
        out.violations = _violations;
        for (const auto &kv : _threads) {
            const ThreadState &st = kv.second;
            out.rootCalls += st.rootCalls;
            out.beforeMoveCalls += st.beforeMoveCalls;
            out.nullMovePushCalls += st.nullMovePushCalls;
            out.popCalls += st.popCalls;
            out.leafAtPlyCalls += st.leafAtPlyCalls;
            if (st.maxPlySeen > out.maxPlySeen) {
                out.maxPlySeen = st.maxPlySeen;
            }
            if (!st.initialized || st.curPly != 0 || st.frames.size() != 1u ||
                (st.beforeMoveCalls + st.nullMovePushCalls) != st.popCalls) {
                out.allThreadsBalancedAtRoot = false;
            }
        }
        return out;
    }

private:
    struct PlyFrame {
        std::uint64_t boardHash = 0ULL;
        int noProgressPlies = 0;
        bool hasBoard = false;
    };

    struct ThreadState {
        bool initialized = false;
        int curPly = 0;
        int maxPlySeen = 0;
        std::uint64_t rootCalls = 0;
        std::uint64_t beforeMoveCalls = 0;
        std::uint64_t nullMovePushCalls = 0;
        std::uint64_t popCalls = 0;
        std::uint64_t leafAtPlyCalls = 0;
        std::vector<PlyFrame> frames;
    };

    void verifyOrRecordBoardLocked(PlyFrame &frame,
                                   const octi::OctiBoard &board) const
    {
        const octi::OctiGame *g = board.getGame();
        if (g == nullptr) {
            ++_violations;
            return;
        }
        const std::uint64_t h = octi::OctiBotEngine::computePositionHashStatic(board);
        const int npp = g->getNoProgressPlies();
        if (!frame.hasBoard) {
            frame.boardHash = h;
            frame.noProgressPlies = npp;
            frame.hasBoard = true;
        } else if (frame.boardHash != h || frame.noProgressPlies != npp) {
            ++_violations;
        }
    }

    bool _requireHeuristic;
    octi::BotTuningProfile _evalProfile;
    int _scoreOffset;
    mutable std::mutex _mtx;
    mutable std::map<std::thread::id, ThreadState> _threads;
    mutable std::uint64_t _legacyEvaluateLeafCalls = 0;
    mutable std::uint64_t _violations = 0;
};

} // namespace

TEST_GROUP(OctiLeafEvaluatorVirtualHooksTest)
{
};

TEST(OctiLeafEvaluatorVirtualHooksTest,
     RequiresHeuristicFalseSkipsEvaluateBoardAndPreservesExactSearchParity)
{
    const char *fens[] = {
        "2:3:0:0:P:B:-:0:2,0,14,0:2,0,14,0:"
        "B51def,B33,A53,A54abegh,A64abfh,A55afh,B46de,B37",
        "OFK:1:0:0:P:A:-:0:0,1,8,0:0,0,9,0:A22,A32,A42,B44cef,B26d,B56,B47bde",
        "2:3:1:0:P:A:-:0:2,0,12,1:2,0,12,1:A33ab,A53a,B37ef,B57e",
        "2:3:0:0:P:A:-:0:0,0,6,0:0,0,6,0:A35abce,A54ac,B46defg,B66ef:24"
    };

    octi::BotTuningProfile prof =
        octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::MASTER);
    prof.useOpeningBook = false;
    prof.rootScoreJitter = 0;
    prof.lazySmpThreads = 1;
    prof.maxSearchDepth = 4;
    prof.maxQuiesceDepth = 2;
    prof.quiesceBranchCap = 6;
    prof.rootBranchCap = 16;
    prof.treeBranchCap = 12;
    prof.usePvs = true;
    prof.aspirationWindowCp = 250;
    prof.useKillers = true;
    prof.useHistory = true;
    prof.lmrMinDepth = 3;
    prof.lmrMinMoveIndex = 2;
    prof.nullMoveMinDepth = 2;
    prof.nullMoveReduction = 2;
    prof.qsearchUseTt = true;
    prof.timeBudgetMs = 300000;

    bool sawNullMovePush = false;
    bool sawQuiesceExtensionPly = false;

    for (const char *fen : fens) {
        octi::OctiBoard board;
        board.suppressBoardUpdateNotification(true);
        board.fromPositionString(fen);

        // Verify polymorphic dispatch of virtual OctiBotEngine::evaluateBoard
        EvalBoardCountingEngine polyCheck(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
        polyCheck.setTuningProfile(prof);
        const octi::OctiBotEngine &baseRef = polyCheck;
        const int directVal = baseRef.evaluateBoard(board, octi::PLAYER_A);
        CHECK_EQUAL(1u, polyCheck.evalBoardCalls());
        CHECK_EQUAL(octi::OctiBotEngine::evaluatePosition(board, octi::PLAYER_A, prof),
                    directVal);

        // 1. Baseline: _leafEvaluator == nullptr
        EvalBoardCountingEngine engNull(octi::BotDifficulty::MASTER, 0x20261118ULL);
        engNull.setTuningProfile(prof);
        octi::BotSearchStats statsNull;
        const std::string mvNull = engNull.chooseMove(board, &statsNull);
        CHECK_FALSE(mvNull.empty());
        CHECK_EQUAL(4, statsNull.depthReached);
        CHECK_TRUE(engNull.evalBoardCalls() > 0);

        // 2. Legacy evaluateLeaf-only subclass (default requiresHeuristicScore == true)
        ConstantOffsetLeafEvaluator legacyZero(0);
        CHECK_TRUE(legacyZero.requiresHeuristicScore());
        EvalBoardCountingEngine engLegacy(octi::BotDifficulty::MASTER, 0x20261118ULL);
        engLegacy.setTuningProfile(prof);
        engLegacy.setLeafEvaluator(&legacyZero);
        octi::BotSearchStats statsLegacy;
        const std::string mvLegacy = engLegacy.chooseMove(board, &statsLegacy);
        STRCMP_EQUAL(mvNull.c_str(), mvLegacy.c_str());
        CHECK_EQUAL(statsNull.depthReached, statsLegacy.depthReached);
        CHECK_EQUAL(statsNull.bestScore, statsLegacy.bestScore);
        CHECK_EQUAL(statsNull.nodesVisited, statsLegacy.nodesVisited);
        CHECK_EQUAL(engNull.evalBoardCalls(), engLegacy.evalBoardCalls());
        CHECK_EQUAL(engNull.evalBoardCalls(),
                    static_cast<std::uint64_t>(legacyZero.calls()));

        // 3. Virtual ply-hook evaluator with requiresHeuristicScore() == true
        PlyStackAuditingLeafEvaluator evalWithHeur(true, prof);
        EvalBoardCountingEngine engWithHeur(octi::BotDifficulty::MASTER, 0x20261118ULL);
        engWithHeur.setTuningProfile(prof);
        engWithHeur.setLeafEvaluator(&evalWithHeur);
        octi::BotSearchStats statsWithHeur;
        const std::string mvWithHeur = engWithHeur.chooseMove(board, &statsWithHeur);
        const auto sumWithHeur = evalWithHeur.summary();
        STRCMP_EQUAL(mvNull.c_str(), mvWithHeur.c_str());
        CHECK_EQUAL(statsNull.depthReached, statsWithHeur.depthReached);
        CHECK_EQUAL(statsNull.bestScore, statsWithHeur.bestScore);
        CHECK_EQUAL(statsNull.nodesVisited, statsWithHeur.nodesVisited);
        CHECK_EQUAL(engNull.evalBoardCalls(), engWithHeur.evalBoardCalls());
        CHECK_EQUAL(0u, sumWithHeur.violations);
        CHECK_EQUAL(0u, sumWithHeur.legacyEvaluateLeafCalls);
        CHECK_TRUE(sumWithHeur.allThreadsBalancedAtRoot);
        CHECK_EQUAL(1u, sumWithHeur.rootCalls);
        CHECK_EQUAL(sumWithHeur.beforeMoveCalls + sumWithHeur.nullMovePushCalls,
                    sumWithHeur.popCalls);

        // 4. Pure leaf evaluator with requiresHeuristicScore() == false -> 0 evaluateBoard calls
        PlyStackAuditingLeafEvaluator evalPure(false, prof);
        EvalBoardCountingEngine engPure(octi::BotDifficulty::MASTER, 0x20261118ULL);
        engPure.setTuningProfile(prof);
        engPure.setLeafEvaluator(&evalPure);
        octi::BotSearchStats statsPure;
        const std::string mvPure = engPure.chooseMove(board, &statsPure);
        const auto sumPure = evalPure.summary();
        CHECK_EQUAL(0u, engPure.evalBoardCalls());
        STRCMP_EQUAL(mvNull.c_str(), mvPure.c_str());
        CHECK_EQUAL(statsNull.depthReached, statsPure.depthReached);
        CHECK_EQUAL(statsNull.bestScore, statsPure.bestScore);
        CHECK_EQUAL(statsNull.nodesVisited, statsPure.nodesVisited);
        CHECK_EQUAL(0u, sumPure.violations);
        CHECK_TRUE(sumPure.allThreadsBalancedAtRoot);
        CHECK_EQUAL(1u, sumPure.rootCalls);
        CHECK_EQUAL(engNull.evalBoardCalls(), sumPure.leafAtPlyCalls);
        CHECK_EQUAL(sumPure.beforeMoveCalls + sumPure.nullMovePushCalls,
                    sumPure.popCalls);
        if (sumPure.nullMovePushCalls > 0) {
            sawNullMovePush = true;
        }
        if (sumPure.maxPlySeen > prof.maxSearchDepth) {
            sawQuiesceExtensionPly = true;
        }
    }

    CHECK_TRUE(sawNullMovePush);
    CHECK_TRUE(sawQuiesceExtensionPly);
}

TEST(OctiLeafEvaluatorVirtualHooksTest,
     LazySmpOneAndFourThreadsAndBookSearchMaintainBalancedPlyStacks)
{
    octi::OctiBoard board;
    board.suppressBoardUpdateNotification(true);
    board.fromPositionString(
        "2:3:0:0:P:B:-:0:2,0,14,0:2,0,14,0:"
        "B51def,B33,A53,A54abegh,A64abfh,A55afh,B46de,B37");

    for (int threads : {1, 4}) {
        for (bool diversify : {false, true}) {
            octi::BotTuningProfile prof =
                octi::OctiBotEngine::defaultProfileForDifficulty(
                    octi::BotDifficulty::MASTER);
            prof.useOpeningBook = false;
            prof.rootScoreJitter = 0;
            prof.lazySmpThreads = threads;
            prof.lazySmpDiversifyHelpers = diversify;
            prof.maxSearchDepth = 5;
            prof.maxQuiesceDepth = 2;
            prof.rootBranchCap = 14;
            prof.treeBranchCap = 12;
            prof.usePvs = true;
            prof.useKillers = true;
            prof.useHistory = true;
            prof.lmrMinDepth = 3;
            prof.lmrMinMoveIndex = 2;
            prof.nullMoveMinDepth = 2;
            prof.qsearchUseTt = true;
            prof.timeBudgetMs = 650;

            PlyStackAuditingLeafEvaluator evalPure(false, prof);
            EvalBoardCountingEngine eng(octi::BotDifficulty::MASTER, 0x5EED2026ULL);
            eng.setTuningProfile(prof);
            eng.setLeafEvaluator(&evalPure);

            octi::BotSearchStats stats;
            const std::string mv = eng.chooseMove(board, &stats);
            CHECK_FALSE(mv.empty());
            CHECK_EQUAL(0u, eng.evalBoardCalls());

            const auto sum = evalPure.summary();
            CHECK_EQUAL(0u, sum.violations);
            CHECK_TRUE(sum.allThreadsBalancedAtRoot);
            CHECK_TRUE(sum.leafAtPlyCalls > 0);
            CHECK_EQUAL(sum.beforeMoveCalls + sum.nullMovePushCalls, sum.popCalls);
            if (threads == 4 && std::thread::hardware_concurrency() >= 2) {
                CHECK_TRUE(sum.threadCount >= 2u);
            }

            // Re-search on the same engine + evaluator to verify root re-entry at ply 0
            const auto topBook = eng.searchTopMovesForBook(board, 3, 400, 3);
            CHECK_FALSE(topBook.empty());
            CHECK_EQUAL(0u, eng.evalBoardCalls());
            const auto sumAfterBook = evalPure.summary();
            CHECK_EQUAL(0u, sumAfterBook.violations);
            CHECK_TRUE(sumAfterBook.allThreadsBalancedAtRoot);
            CHECK_EQUAL(sumAfterBook.beforeMoveCalls + sumAfterBook.nullMovePushCalls,
                        sumAfterBook.popCalls);
        }
    }
}

TEST(OctiLeafEvaluatorVirtualHooksTest,
     ThrowingMoveAndMidSearchTimeoutUnwindPlyStacksCleanlyViaRaii)
{
    octi::OctiBoard board;
    board.suppressBoardUpdateNotification(true);
    board.fromPositionString(
        "2:3:0:0:P:A:-:0:2,0,14,0:2,0,14,0:"
        "A33abc,A54bc,B45efg,B57ef,B77g");
    CHECK_TRUE(board.squareAt(0, 0).isEmpty());

    // 1. Throwing move() edge across 1T, 4T diversified/non-diversified, and book search
    for (int threads : {1, 4}) {
        for (bool diversify : {false, true}) {
            octi::BotTuningProfile prof =
                octi::OctiBotEngine::defaultProfileForDifficulty(
                    octi::BotDifficulty::TACTICAL);
            prof.useOpeningBook = false;
            prof.rootScoreJitter = 0;
            prof.lazySmpThreads = threads;
            prof.lazySmpDiversifyHelpers = diversify;
            prof.maxSearchDepth = 3;
            prof.maxQuiesceDepth = 2;
            prof.rootBranchCap = 10;
            prof.treeBranchCap = 8;
            prof.nullMoveMinDepth = 2;
            prof.timeBudgetMs = 800;

            PlyStackAuditingLeafEvaluator evalPure(false, prof);
            EvalBoardCountingEngine eng(octi::BotDifficulty::TACTICAL, 0x99887766ULL);
            eng.setTuningProfile(prof);
            eng.setInjectIllegalMove(true);
            eng.setLeafEvaluator(&evalPure);

            octi::BotSearchStats stats;
            const std::string chosen = eng.chooseMove(board, &stats);
            CHECK_FALSE(chosen.empty());
            CHECK_TRUE(chosen != "11+a");
            CHECK_TRUE(eng.injectedIllegalCount() > 0);
            CHECK_EQUAL(0u, eng.evalBoardCalls());

            const auto topBook = eng.searchTopMovesForBook(board, 2, 350, 2);
            CHECK_FALSE(topBook.empty());
            CHECK_TRUE(topBook[0].move != "11+a");
            CHECK_EQUAL(0u, eng.evalBoardCalls());

            const auto sum = evalPure.summary();
            CHECK_EQUAL(0u, sum.violations);
            CHECK_TRUE(sum.allThreadsBalancedAtRoot);
            CHECK_EQUAL(sum.beforeMoveCalls + sum.nullMovePushCalls, sum.popCalls);
        }
    }

    // 2. Mid-search wall-clock timeout edge (1T tight 15ms & 4T 520ms deep horizon)
    for (int threads : {1, 4}) {
        octi::BotTuningProfile prof =
            octi::OctiBotEngine::defaultProfileForDifficulty(
                octi::BotDifficulty::GRANDMASTER);
        prof.useOpeningBook = false;
        prof.rootScoreJitter = 0;
        prof.lazySmpThreads = threads;
        prof.lazySmpDiversifyHelpers = true;
        prof.maxSearchDepth = 32;
        prof.maxQuiesceDepth = 4;
        prof.nullMoveMinDepth = 2;
        prof.usePvs = true;
        prof.timeBudgetMs = (threads == 1) ? 15 : 520;

        PlyStackAuditingLeafEvaluator evalPure(false, prof);
        EvalBoardCountingEngine eng(octi::BotDifficulty::GRANDMASTER, 0xABCD2026ULL);
        eng.setTuningProfile(prof);
        eng.setLeafEvaluator(&evalPure);

        octi::BotSearchStats stats;
        const std::string chosen = eng.chooseMove(board, &stats);
        CHECK_FALSE(chosen.empty());
        CHECK_TRUE(stats.depthReached < 32);
        CHECK_EQUAL(0u, eng.evalBoardCalls());

        const auto sum = evalPure.summary();
        CHECK_EQUAL(0u, sum.violations);
        CHECK_TRUE(sum.allThreadsBalancedAtRoot);
        CHECK_TRUE(sum.popCalls > 0);
        CHECK_EQUAL(sum.beforeMoveCalls + sum.nullMovePushCalls, sum.popCalls);
    }
}

namespace {

std::string resolveSharedSoundAssetPath(const std::string &fileName)
{
    const char *prefixes[] = {
        "assets/sounds/",
        "../assets/sounds/",
        "../../assets/sounds/",
        "../../../assets/sounds/"
    };
    for (const char *prefix : prefixes) {
        std::string candidate = std::string(prefix) + fileName;
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate;
        }
    }
    return std::string("../assets/sounds/") + fileName;
}

bool readBinaryFileBytes(const std::string &path, std::vector<std::uint8_t> &out)
{
    out.clear();
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    in.seekg(0, std::ios::end);
    std::streamoff len = in.tellg();
    if (len < 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(len));
    if (len > 0) {
        in.read(reinterpret_cast<char *>(out.data()), len);
        if (!in) {
            out.clear();
            return false;
        }
    }
    return true;
}

void writeLe16(std::vector<std::uint8_t> &buf, std::size_t offset, std::uint16_t v)
{
    buf[offset] = static_cast<std::uint8_t>(v & 0xFFu);
    buf[offset + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

void writeLe32(std::vector<std::uint8_t> &buf, std::size_t offset, std::uint32_t v)
{
    buf[offset] = static_cast<std::uint8_t>(v & 0xFFu);
    buf[offset + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
    buf[offset + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
    buf[offset + 3] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
}

std::vector<std::uint8_t> buildSyntheticWavPcm16Mono(
    const std::vector<std::int16_t> &samples,
    std::uint32_t sampleRate)
{
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * 2u);
    const std::uint32_t riffBytes = 36u + dataBytes;
    std::vector<std::uint8_t> wav(44u + static_cast<std::size_t>(dataBytes), 0);
    std::memcpy(&wav[0], "RIFF", 4);
    writeLe32(wav, 4, riffBytes);
    std::memcpy(&wav[8], "WAVE", 4);
    std::memcpy(&wav[12], "fmt ", 4);
    writeLe32(wav, 16, 16u);
    writeLe16(wav, 20, 1u);
    writeLe16(wav, 22, 1u);
    writeLe32(wav, 24, sampleRate);
    writeLe32(wav, 28, sampleRate * 2u);
    writeLe16(wav, 32, 2u);
    writeLe16(wav, 34, 16u);
    std::memcpy(&wav[36], "data", 4);
    writeLe32(wav, 40, dataBytes);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        std::uint16_t u = static_cast<std::uint16_t>(samples[i]);
        writeLe16(wav, 44u + i * 2u, u);
    }
    return wav;
}

} // namespace

TEST_GROUP(OctiSoundCueTest)
{
};

TEST(OctiSoundCueTest, CueNamesAssetFilenamesAndWhitelistValidation)
{
    struct ExpectedCueMeta {
        octi::SoundCue cue;
        const char *name;
        const char *fileName;
    };
    const ExpectedCueMeta expected[octi::OCTI_SOUND_CUE_COUNT] = {
        { octi::SoundCue::POD_PLACE,      "POD_PLACE",      "pod_place.wav"      },
        { octi::SoundCue::PRONG_SNAP,     "PRONG_SNAP",     "prong_snap.wav"     },
        { octi::SoundCue::POD_STEP,       "POD_STEP",       "pod_step.wav"       },
        { octi::SoundCue::POD_JUMP,       "POD_JUMP",       "pod_jump.wav"       },
        { octi::SoundCue::POD_CAPTURE,    "POD_CAPTURE",    "pod_capture.wav"    },
        { octi::SoundCue::CLOCK_LOW_TICK, "CLOCK_LOW_TICK", "clock_low_tick.wav" },
        { octi::SoundCue::GAME_VICTORY,   "GAME_VICTORY",   "game_victory.wav"   },
    };

    for (std::size_t i = 0; i < octi::OCTI_SOUND_CUE_COUNT; ++i) {
        STRCMP_EQUAL(expected[i].name, octi::soundCueName(expected[i].cue));
        STRCMP_EQUAL(expected[i].fileName, octi::soundCueAssetFileName(expected[i].cue));

        octi::SoundCue parsedByName = octi::SoundCue::GAME_VICTORY;
        CHECK_TRUE(octi::soundCueFromName(expected[i].name, parsedByName));
        CHECK_EQUAL(static_cast<int>(expected[i].cue), static_cast<int>(parsedByName));

        octi::SoundCue parsedByFile = octi::SoundCue::GAME_VICTORY;
        CHECK_TRUE(octi::soundCueFromAssetFileName(expected[i].fileName, parsedByFile));
        CHECK_EQUAL(static_cast<int>(expected[i].cue), static_cast<int>(parsedByFile));
    }

    octi::SoundCue dropAlias = octi::SoundCue::GAME_VICTORY;
    CHECK_TRUE(octi::soundCueFromName("POD_DROP", dropAlias));
    CHECK_EQUAL(static_cast<int>(octi::SoundCue::POD_PLACE), static_cast<int>(dropAlias));

    octi::SoundCue outCue = octi::SoundCue::PRONG_SNAP;
    CHECK_FALSE(octi::soundCueFromName("", outCue));
    CHECK_FALSE(octi::soundCueFromName("pod_place", outCue));
    CHECK_FALSE(octi::soundCueFromName("UNKNOWN_CUE", outCue));
    CHECK_FALSE(octi::soundCueFromName(std::string("POD_PLACE\0EXTRA", 15), outCue));

    CHECK_FALSE(octi::soundCueFromAssetFileName("", outCue));
    CHECK_FALSE(octi::soundCueFromAssetFileName("../etc/passwd", outCue));
    CHECK_FALSE(octi::soundCueFromAssetFileName("..%2fetc%2fpasswd", outCue));
    CHECK_FALSE(octi::soundCueFromAssetFileName("sounds/pod_place.wav", outCue));
    CHECK_FALSE(octi::soundCueFromAssetFileName("/pod_place.wav", outCue));
    CHECK_FALSE(octi::soundCueFromAssetFileName("pod_place.wav/", outCue));
    CHECK_FALSE(octi::soundCueFromAssetFileName("unknown.wav", outCue));
    CHECK_FALSE(octi::soundCueFromAssetFileName("prong_snap.mp3", outCue));
    CHECK_FALSE(octi::soundCueFromAssetFileName("POD_PLACE.WAV", outCue));
    CHECK_FALSE(octi::soundCueFromAssetFileName(std::string("pod_place.wav\0x", 15), outCue));

    std::size_t badSize = 999;
    CHECK_TRUE(octi::soundCueWavBytes(static_cast<octi::SoundCue>(99), badSize) == nullptr);
    CHECK_EQUAL(0u, badSize);
    STRCMP_EQUAL("", octi::soundCueName(static_cast<octi::SoundCue>(-1)));
    STRCMP_EQUAL("", octi::soundCueAssetFileName(static_cast<octi::SoundCue>(99)));
}

TEST(OctiSoundCueTest, CanonicalWavBytesMatchDiskAssetsAndDecodePcm16)
{
    struct ExpectedAsset {
        octi::SoundCue cue;
        const char *fileName;
        std::size_t wavSize;
        std::size_t sampleCount;
    };
    const ExpectedAsset assets[octi::OCTI_SOUND_CUE_COUNT] = {
        { octi::SoundCue::POD_PLACE,      "pod_place.wav",      3884u,  1920u },
        { octi::SoundCue::PRONG_SNAP,     "prong_snap.wav",     2444u,  1200u },
        { octi::SoundCue::POD_STEP,       "pod_step.wav",       3084u,  1520u },
        { octi::SoundCue::POD_JUMP,       "pod_jump.wav",       4844u,  2400u },
        { octi::SoundCue::POD_CAPTURE,    "pod_capture.wav",    6764u,  3360u },
        { octi::SoundCue::CLOCK_LOW_TICK, "clock_low_tick.wav", 1484u,   720u },
        { octi::SoundCue::GAME_VICTORY,   "game_victory.wav",  11564u,  5760u },
    };

    std::vector<std::vector<std::int16_t>> decodedWaveforms;
    for (std::size_t i = 0; i < octi::OCTI_SOUND_CUE_COUNT; ++i) {
        std::size_t embeddedSize = 0;
        const std::uint8_t *embeddedPtr =
            octi::soundCueWavBytes(assets[i].cue, embeddedSize);
        CHECK_TRUE(embeddedPtr != nullptr);
        CHECK_EQUAL(assets[i].wavSize, embeddedSize);

        std::string diskPath = resolveSharedSoundAssetPath(assets[i].fileName);
        std::vector<std::uint8_t> diskBytes;
        CHECK_TRUE(readBinaryFileBytes(diskPath, diskBytes));
        CHECK_EQUAL(assets[i].wavSize, diskBytes.size());
        CHECK_EQUAL(0, std::memcmp(embeddedPtr, diskBytes.data(), assets[i].wavSize));

        std::vector<std::int16_t> pcm;
        int sampleRate = 0;
        CHECK_TRUE(octi::decodeWavPcm16Mono(embeddedPtr, embeddedSize, pcm, sampleRate));
        CHECK_EQUAL(octi::OCTI_SOUND_SAMPLE_RATE, sampleRate);
        CHECK_EQUAL(assets[i].sampleCount, pcm.size());
        CHECK_EQUAL(0, static_cast<int>(pcm[0]));

        int peakAbs = 0;
        for (std::int16_t s : pcm) {
            int a = s < 0 ? -static_cast<int>(s) : static_cast<int>(s);
            if (a > peakAbs) {
                peakAbs = a;
            }
        }
        CHECK_TRUE(peakAbs > 5000);
        CHECK_TRUE(peakAbs <= 30000);
        decodedWaveforms.push_back(std::move(pcm));
    }

    for (std::size_t i = 0; i < decodedWaveforms.size(); ++i) {
        for (std::size_t j = i + 1; j < decodedWaveforms.size(); ++j) {
            CHECK_FALSE(decodedWaveforms[i] == decodedWaveforms[j]);
        }
    }
}

TEST(OctiSoundCueTest, DecodeWavBoundaryAndFaultInjection)
{
    std::vector<std::int16_t> pcm = { 123, -456 };
    int sr = 44100;

    // 0-byte and nullptr
    CHECK_FALSE(octi::decodeWavPcm16Mono(nullptr, 0, pcm, sr));
    CHECK_TRUE(pcm.empty());
    CHECK_EQUAL(0, sr);

    std::uint8_t dummy[64] = { 0 };
    CHECK_FALSE(octi::decodeWavPcm16Mono(nullptr, 44, pcm, sr));
    CHECK_FALSE(octi::decodeWavPcm16Mono(dummy, 0, pcm, sr));

    // 0-sample valid 44-byte WAV
    std::vector<std::uint8_t> emptyValid =
        buildSyntheticWavPcm16Mono({}, octi::OCTI_SOUND_SAMPLE_RATE);
    CHECK_EQUAL(44u, emptyValid.size());
    CHECK_TRUE(octi::decodeWavPcm16Mono(emptyValid.data(), emptyValid.size(), pcm, sr));
    CHECK_TRUE(pcm.empty());
    CHECK_EQUAL(octi::OCTI_SOUND_SAMPLE_RATE, sr);

    // 43-byte truncated header
    CHECK_FALSE(octi::decodeWavPcm16Mono(emptyValid.data(), 43, pcm, sr));

    // Base valid 4-sample WAV for fault injection
    std::vector<std::int16_t> origSamples = { 0, 12000, -12000, 30000 };
    const std::vector<std::uint8_t> baseValid =
        buildSyntheticWavPcm16Mono(origSamples, octi::OCTI_SOUND_SAMPLE_RATE);
    CHECK_TRUE(octi::decodeWavPcm16Mono(baseValid.data(), baseValid.size(), pcm, sr));
    CHECK_TRUE(pcm == origSamples);

    // Corrupted RIFF magic
    {
        auto bad = baseValid;
        bad[3] = 'X'; // "RIFX"
        CHECK_FALSE(octi::decodeWavPcm16Mono(bad.data(), bad.size(), pcm, sr));
    }

    // Corrupted WAVE magic
    {
        auto bad = baseValid;
        std::memcpy(&bad[8], "AVI ", 4);
        CHECK_FALSE(octi::decodeWavPcm16Mono(bad.data(), bad.size(), pcm, sr));
    }

    // audioFormat != 1 (e.g. IEEE float = 3)
    {
        auto bad = baseValid;
        writeLe16(bad, 20, 3u);
        CHECK_FALSE(octi::decodeWavPcm16Mono(bad.data(), bad.size(), pcm, sr));
    }

    // channels = 2 (stereo)
    {
        auto bad = baseValid;
        writeLe16(bad, 22, 2u);
        writeLe32(bad, 28, octi::OCTI_SOUND_SAMPLE_RATE * 4u);
        writeLe16(bad, 32, 4u);
        CHECK_FALSE(octi::decodeWavPcm16Mono(bad.data(), bad.size(), pcm, sr));
    }

    // bitsPerSample = 8
    {
        auto bad = baseValid;
        writeLe16(bad, 34, 8u);
        writeLe32(bad, 28, octi::OCTI_SOUND_SAMPLE_RATE);
        writeLe16(bad, 32, 1u);
        CHECK_FALSE(octi::decodeWavPcm16Mono(bad.data(), bad.size(), pcm, sr));
    }

    // sampleRate = 0
    {
        auto bad = baseValid;
        writeLe32(bad, 24, 0u);
        writeLe32(bad, 28, 0u);
        CHECK_FALSE(octi::decodeWavPcm16Mono(bad.data(), bad.size(), pcm, sr));
    }

    // data chunk length 0xFFFFFFFF
    {
        auto bad = baseValid;
        writeLe32(bad, 40, 0xFFFFFFFFu);
        CHECK_FALSE(octi::decodeWavPcm16Mono(bad.data(), bad.size(), pcm, sr));
    }

    // data chunk length exceeding buffer by 2 bytes
    {
        auto bad = baseValid;
        writeLe32(bad, 4, static_cast<std::uint32_t>(bad.size() - 8u + 2u));
        writeLe32(bad, 40, static_cast<std::uint32_t>(origSamples.size() * 2u + 2u));
        CHECK_FALSE(octi::decodeWavPcm16Mono(bad.data(), bad.size(), pcm, sr));
    }

    // Odd data byte length
    {
        std::vector<std::uint8_t> oddWav = emptyValid;
        oddWav.insert(oddWav.end(), { 0x11, 0x22, 0x33, 0x00 });
        writeLe32(oddWav, 4, 40u);
        writeLe32(oddWav, 40, 3u);
        CHECK_FALSE(octi::decodeWavPcm16Mono(oddWav.data(), oddWav.size(), pcm, sr));
    }

    // Valid WAV containing an extra JUNK subchunk before data
    {
        std::vector<std::uint8_t> withJunk(baseValid.begin(), baseValid.begin() + 36);
        const std::uint8_t junkChunk[12] = {
            'J', 'U', 'N', 'K', 4, 0, 0, 0, 0, 0, 0, 0
        };
        withJunk.insert(withJunk.end(), junkChunk, junkChunk + 12);
        withJunk.insert(withJunk.end(), baseValid.begin() + 36, baseValid.end());
        writeLe32(withJunk, 4, static_cast<std::uint32_t>(withJunk.size() - 8u));
        CHECK_TRUE(octi::decodeWavPcm16Mono(withJunk.data(), withJunk.size(), pcm, sr));
        CHECK_TRUE(pcm == origSamples);
    }
}

TEST(OctiSoundCueTest, ClassifyMoveAndNotationSoundCuePriorityAndEdgelessWrap)
{
    auto checkMoveCue = [](const char *notation,
                           octi::SoundCue expected,
                           bool edgeless = false,
                           int bw = 9,
                           int bh = 9) {
        CHECK_EQUAL(
            static_cast<int>(expected),
            static_cast<int>(octi::classifyMoveNotationSoundCue(notation, edgeless, bw, bh)));
        octi::OctiMove parsed(notation);
        CHECK_EQUAL(
            static_cast<int>(expected),
            static_cast<int>(octi::classifyMoveSoundCue(parsed, edgeless, bw, bh)));
    };

    // Pod drop & liberate
    checkMoveCue("33", octi::SoundCue::POD_PLACE);
    checkMoveCue("33*", octi::SoundCue::POD_PLACE);

    // Prong add & superprong add (prong reposition is illegal syntax and falls back safely)
    checkMoveCue("33+a", octi::SoundCue::PRONG_SNAP);
    checkMoveCue("33+a!", octi::SoundCue::PRONG_SNAP);
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a+b-a"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a-a+b"));
    CHECK_EQUAL(static_cast<int>(octi::SoundCue::POD_PLACE),
                static_cast<int>(octi::classifyMoveNotationSoundCue("33a+b-a")));
    CHECK_EQUAL(static_cast<int>(octi::SoundCue::POD_PLACE),
                static_cast<int>(octi::classifyMoveNotationSoundCue("33a-a+b")));

    // Step, jump, multi-hop, capture, composite
    checkMoveCue("33a-34", octi::SoundCue::POD_STEP);
    checkMoveCue("33a-35", octi::SoundCue::POD_JUMP);
    checkMoveCue("33ac-35-55", octi::SoundCue::POD_JUMP);
    checkMoveCue("33a-35x", octi::SoundCue::POD_CAPTURE);
    checkMoveCue("33ac-35-55x", octi::SoundCue::POD_CAPTURE);
    checkMoveCue("33ab-34,33ab-44", octi::SoundCue::POD_STEP);
    checkMoveCue("33ab-34,33ab-55", octi::SoundCue::POD_JUMP);
    checkMoveCue("33ab-35,33ab-55x", octi::SoundCue::POD_CAPTURE);

    // Edgeless toroidal wrap boundaries (0 <-> 8)
    checkMoveCue("15g-85", octi::SoundCue::POD_JUMP, true, 9, 9);
    checkMoveCue("15g-95", octi::SoundCue::POD_STEP, true, 9, 9);
    checkMoveCue("95c-15", octi::SoundCue::POD_STEP, true, 9, 9);
    checkMoveCue("95c-25", octi::SoundCue::POD_JUMP, true, 9, 9);
    checkMoveCue("85c-15", octi::SoundCue::POD_JUMP, true, 9, 9);
    checkMoveCue("51e-59", octi::SoundCue::POD_STEP, true, 9, 9);
    checkMoveCue("51e-58", octi::SoundCue::POD_JUMP, true, 9, 9);
    checkMoveCue("59a-51", octi::SoundCue::POD_STEP, true, 9, 9);
    checkMoveCue("59a-52", octi::SoundCue::POD_JUMP, true, 9, 9);
    checkMoveCue("11f-88", octi::SoundCue::POD_JUMP, true, 9, 9);
    checkMoveCue("11f-99", octi::SoundCue::POD_STEP, true, 9, 9);

    // Superprong prelude + regular priority rules
    checkMoveCue("33a!-34;34a!+b", octi::SoundCue::PRONG_SNAP);
    checkMoveCue("33A-34;34+b", octi::SoundCue::PRONG_SNAP);
    checkMoveCue("33a!-35x;35a!+b", octi::SoundCue::POD_CAPTURE);
    checkMoveCue("33A-35x;35+b", octi::SoundCue::POD_CAPTURE);
    checkMoveCue("33a!-35;35a!+b", octi::SoundCue::POD_JUMP);
    checkMoveCue("33a!-34;53", octi::SoundCue::POD_PLACE);
    checkMoveCue("33a!-34;34a!-35", octi::SoundCue::POD_STEP);

    // Bonus-only synthetic OctiMove fallback
    {
        octi::OctiMoveOp bonusStep;
        bonusStep.x0 = 2;
        bonusStep.y0 = 2;
        bonusStep.pod.addProng(octi::DIR_N, true);
        bonusStep.isSuperProngBonusMove = true;
        bonusStep.hops.push_back({ 2, 3, false });
        octi::OctiMove bonusOnly =
            octi::OctiMove::fromValidatedSingleOp(std::move(bonusStep));
        CHECK_EQUAL(
            static_cast<int>(octi::SoundCue::POD_STEP),
            static_cast<int>(octi::classifyMoveSoundCue(bonusOnly)));
    }

    // Empty and malformed move notations default cleanly to POD_PLACE
    CHECK_EQUAL(
        static_cast<int>(octi::SoundCue::POD_PLACE),
        static_cast<int>(octi::classifyMoveNotationSoundCue("")));
    CHECK_EQUAL(
        static_cast<int>(octi::SoundCue::POD_PLACE),
        static_cast<int>(octi::classifyMoveNotationSoundCue("   ")));
    CHECK_EQUAL(
        static_cast<int>(octi::SoundCue::POD_PLACE),
        static_cast<int>(octi::classifyMoveNotationSoundCue("not-a-move")));
    CHECK_EQUAL(
        static_cast<int>(octi::SoundCue::POD_PLACE),
        static_cast<int>(octi::classifyMoveNotationSoundCue("99+z")));
    CHECK_EQUAL(
        static_cast<int>(octi::SoundCue::POD_PLACE),
        static_cast<int>(octi::classifyMoveNotationSoundCue(std::string("33\0bad", 6))));
    CHECK_EQUAL(
        static_cast<int>(octi::SoundCue::POD_PLACE),
        static_cast<int>(octi::classifyMoveSoundCue(octi::OctiMove())));
}

TEST(OctiSoundCueTest, RapidCheckWavDecoderAndClassifierFuzzing)
{
    IGNORE_ALL_LEAKS_IN_TEST();

    bool wavRoundtripAndFuzz = rc::check(
        "decodeWavPcm16Mono lossless roundtrip and arbitrary byte fuzzing",
        []() {
            auto samples = *rc::gen::container<std::vector<std::int16_t>>(
                rc::gen::arbitrary<std::int16_t>());
            auto sr = static_cast<std::uint32_t>(*rc::gen::inRange(8000, 96001));
            std::vector<std::uint8_t> validWav = buildSyntheticWavPcm16Mono(samples, sr);

            std::vector<std::int16_t> outPcm;
            int outSr = 0;
            RC_ASSERT(octi::decodeWavPcm16Mono(
                validWav.data(), validWav.size(), outPcm, outSr));
            RC_ASSERT(outSr == static_cast<int>(sr));
            RC_ASSERT(outPcm == samples);

            auto rawBytes = *rc::gen::container<std::vector<std::uint8_t>>(
                rc::gen::arbitrary<std::uint8_t>());
            std::vector<std::int16_t> fuzzPcm;
            int fuzzSr = -1;
            bool ok = octi::decodeWavPcm16Mono(
                rawBytes.data(), rawBytes.size(), fuzzPcm, fuzzSr);
            if (ok) {
                RC_ASSERT(fuzzSr > 0);
            } else {
                RC_ASSERT(fuzzPcm.empty());
                RC_ASSERT(fuzzSr == 0);
            }
        });
    CHECK_TRUE(wavRoundtripAndFuzz);

    bool moveClassifierFuzz = rc::check(
        "classifyMoveNotationSoundCue consistency and fuzz safety",
        []() {
            auto fuzzStr = *rc::gen::arbitrary<std::string>();
            bool edgeless = *rc::gen::arbitrary<bool>();
            int bw = *rc::gen::inRange(-2, 12);
            int bh = *rc::gen::inRange(-2, 12);
            octi::SoundCue c = octi::classifyMoveNotationSoundCue(fuzzStr, edgeless, bw, bh);
            int ci = static_cast<int>(c);
            RC_ASSERT(ci >= 0 && ci <= static_cast<int>(octi::SoundCue::POD_CAPTURE));

            int x0 = *rc::gen::inRange(0, 9);
            int y0 = *rc::gen::inRange(0, 9);
            int kind = *rc::gen::inRange(0, 5);
            octi::OctiMoveOp op;
            op.x0 = x0;
            op.y0 = y0;
            if (kind == 0) {
                op.addPod = true;
                op.liberate = *rc::gen::arbitrary<bool>();
                octi::OctiMove mv(std::vector<octi::OctiMoveOp>{ op });
                RC_ASSERT(octi::classifyMoveSoundCue(mv, edgeless, 9, 9) ==
                          octi::SoundCue::POD_PLACE);
                RC_ASSERT(octi::classifyMoveNotationSoundCue(mv.asString(), edgeless, 9, 9) ==
                          octi::SoundCue::POD_PLACE);
            } else if (kind == 1) {
                int d = *rc::gen::inRange(0, 8);
                op.addProngDir = static_cast<octi::OctiDir>(d);
                op.superProng = *rc::gen::arbitrary<bool>();
                octi::OctiMove mv(std::vector<octi::OctiMoveOp>{ op });
                RC_ASSERT(octi::classifyMoveSoundCue(mv, edgeless, 9, 9) ==
                          octi::SoundCue::PRONG_SNAP);
                RC_ASSERT(octi::classifyMoveNotationSoundCue(mv.asString(), edgeless, 9, 9) ==
                          octi::SoundCue::PRONG_SNAP);
            } else {
                int d = *rc::gen::inRange(0, 8);
                auto dir = static_cast<octi::OctiDir>(d);
                op.pod.addProng(dir, false);
                int dist = (kind == 2) ? 1 : 2;
                bool cap = (kind == 4);
                int nx = x0 + octi::dirDeltaX(dir) * dist;
                int ny = y0 + octi::dirDeltaY(dir) * dist;
                if (edgeless) {
                    nx = (nx % 9 + 9) % 9;
                    ny = (ny % 9 + 9) % 9;
                }
                if (nx >= 0 && nx < 9 && ny >= 0 && ny < 9 && (nx != x0 || ny != y0)) {
                    op.hops.push_back({ nx, ny, cap });
                    octi::OctiMove mv(std::vector<octi::OctiMoveOp>{ op });
                    octi::SoundCue expected = cap
                        ? octi::SoundCue::POD_CAPTURE
                        : (dist == 2 ? octi::SoundCue::POD_JUMP : octi::SoundCue::POD_STEP);
                    RC_ASSERT(octi::classifyMoveSoundCue(mv, edgeless, 9, 9) == expected);
                    RC_ASSERT(octi::classifyMoveNotationSoundCue(mv.asString(), edgeless, 9, 9) ==
                              expected);
                }
            }
        });
    CHECK_TRUE(moveClassifierFuzz);
}

TEST(OctiSoundCueTest, OctiSoundGenCliVerifyAndJsSync)
{
    std::string soundGenBin;
    std::error_code selfEc;
    std::filesystem::path selfDir =
        std::filesystem::canonical("/proc/self/exe", selfEc).parent_path();
    if (!selfEc && std::filesystem::exists(selfDir / "octi_sound_gen")) {
        soundGenBin = (selfDir / "octi_sound_gen").string();
    }
    const char *binCandidates[] = {
        "./build/octi_sound_gen",
        "./build/asan/octi_sound_gen",
        "./core/build/octi_sound_gen",
        "./core/build/asan/octi_sound_gen",
        "./octi_sound_gen",
        "../octi_sound_gen"
    };
    if (soundGenBin.empty()) {
        for (const char *cand : binCandidates) {
            std::error_code ec;
            if (std::filesystem::exists(cand, ec) && !ec) {
                soundGenBin = cand;
                break;
            }
        }
    }
    CHECK_FALSE(soundGenBin.empty());

    std::filesystem::path sampleAssetPath(resolveSharedSoundAssetPath("pod_place.wav"));
    std::filesystem::path assetsDir = sampleAssetPath.parent_path();

    std::string verifyCmd =
        "\"" + soundGenBin + "\" --verify-dir \"" + assetsDir.string() + "\"";
    CHECK_EQUAL(0, std::system(verifyCmd.c_str()));

    std::filesystem::path scratchDir =
        std::filesystem::path(soundGenBin).parent_path() / "sound_gen_test_scratch";
    std::error_code rmEc;
    std::filesystem::remove_all(scratchDir, rmEc);
    std::filesystem::create_directories(scratchDir);

    std::filesystem::path outAssetsDir = scratchDir / "sounds";
    std::string genCmd =
        "\"" + soundGenBin + "\" --out-dir \"" + outAssetsDir.string() +
        "\" --verify-dir \"" + outAssetsDir.string() + "\"";
    CHECK_EQUAL(0, std::system(genCmd.c_str()));

    // Corrupt 1 byte in prong_snap.wav and confirm --verify-dir fails
    {
        std::filesystem::path snapPath = outAssetsDir / "prong_snap.wav";
        std::vector<std::uint8_t> snapBytes;
        CHECK_TRUE(readBinaryFileBytes(snapPath.string(), snapBytes));
        CHECK_EQUAL(2444u, snapBytes.size());
        snapBytes[100] ^= 0xFFu;
        std::ofstream out(snapPath, std::ios::out | std::ios::trunc | std::ios::binary);
        out.write(reinterpret_cast<const char *>(snapBytes.data()),
                  static_cast<std::streamsize>(snapBytes.size()));
        out.close();

        std::string failVerifyCmd =
            "\"" + soundGenBin + "\" --verify-dir \"" + outAssetsDir.string() +
            "\" > /dev/null 2>&1";
        CHECK_TRUE(std::system(failVerifyCmd.c_str()) != 0);
    }

    // Verify --from-dir --out-js marker replacement and idempotency
    {
        std::filesystem::path stubJs = scratchDir / "WebUiStub.cxx";
        {
            std::ofstream out(stubJs, std::ios::out | std::ios::trunc | std::ios::binary);
            out << "const char kHtml[] = R\"OCTIUI(\n"
                << "/* BEGIN_OCTI_SHARED_SOUND_ASSETS */\n"
                << "/* END_OCTI_SHARED_SOUND_ASSETS */\n"
                << ")OCTIUI\";\n";
        }
        std::string syncCmd =
            "\"" + soundGenBin + "\" --from-dir \"" + assetsDir.string() +
            "\" --out-js \"" + stubJs.string() + "\"";
        CHECK_EQUAL(0, std::system(syncCmd.c_str()));

        std::vector<std::uint8_t> syncedPass1;
        CHECK_TRUE(readBinaryFileBytes(stubJs.string(), syncedPass1));
        CHECK_EQUAL(0, std::system(syncCmd.c_str()));
        std::vector<std::uint8_t> syncedPass2;
        CHECK_TRUE(readBinaryFileBytes(stubJs.string(), syncedPass2));
        CHECK_TRUE(syncedPass1 == syncedPass2);

        std::string syncedText(syncedPass1.begin(), syncedPass1.end());
        CHECK_TRUE(syncedText.find("var OCTI_SHARED_WAV_BASE64 = {") != std::string::npos);
        for (std::size_t i = 0; i < octi::OCTI_SOUND_CUE_COUNT; ++i) {
            auto cue = static_cast<octi::SoundCue>(i);
            std::string keyToken = std::string("  ") + octi::soundCueName(cue) + ": \"UklGR";
            CHECK_TRUE(syncedText.find(keyToken) != std::string::npos);
        }

        // Missing markers must fail cleanly
        std::filesystem::path noMarkerJs = scratchDir / "NoMarkers.cxx";
        {
            std::ofstream out(noMarkerJs, std::ios::out | std::ios::trunc | std::ios::binary);
            out << "var x = 1;\n";
        }
        std::string badSyncCmd =
            "\"" + soundGenBin + "\" --from-dir \"" + assetsDir.string() +
            "\" --out-js \"" + noMarkerJs.string() + "\" > /dev/null 2>&1";
        CHECK_TRUE(std::system(badSyncCmd.c_str()) != 0);
    }

    std::filesystem::remove_all(scratchDir, rmEc);
}

TEST_GROUP(OctiPostMortemEngineBugFixes)
{
    static int findScoredMove(const std::vector<octi::BookMoveScore> &scores,
                              const std::string &targetMove,
                              bool *found = nullptr)
    {
        for (const auto &entry : scores) {
            if (entry.move == targetMove) {
                if (found) {
                    *found = true;
                }
                return entry.score;
            }
        }
        if (found) {
            *found = false;
        }
        return 0;
    }
};

TEST(OctiPostMortemEngineBugFixes, CompositeTacticalFilterAtPly3FindsSynchronizedTwoPodStrike)
{
    // Ply 43 FEN: Player A to move; "33abg-23" allows 44. 47g+d -> 46. 47efg-45,47dg-56
    const char *kPly43Fen =
        "2:1:0:0:P:A:-:0:1,1,15,0:0,1,14,0:"
        "A33abg,A33a,A73h,A34abgh,A64bh,B46bcdefg,B37,B47efg,B47g,B57,B77";

    octi::OctiBoard b43;
    b43.fromPositionString(kPly43Fen);

    octi::BotTuningProfile baseProf;
    baseProf.useOpeningBook = false;
    baseProf.rootScoreJitter = 0;
    baseProf.lazySmpThreads = 1;
    baseProf.maxSearchDepth = 4;
    baseProf.timeBudgetMs = 0;
    baseProf.compositeMoveMaxPly = 1;
    baseProf.compositeTacticalOnlyMinPly = 0;

    octi::OctiBotEngine baseBot(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    baseBot.setTuningProfile(baseProf);
    octi::BotSearchStats baseStats;
    std::vector<octi::BookMoveScore> baseRootScores =
        baseBot.searchTopMovesForBook(b43, 4, 0, 36, &baseStats);
    bool foundBaseBlunder = false;
    int baseBlunderScore = findScoredMove(baseRootScores, "33abg-23", &foundBaseBlunder);
    CHECK_TRUE(foundBaseBlunder);
    STRCMP_EQUAL("33abg-23", baseRootScores.front().move.c_str());

    // With fixed eval + compositeMoveMaxPly = 4 and compositeTacticalOnlyMinPly = 2 at depth 5
    // (where ply 3 = B's 46. 47efg-45,47dg-56 has depth = 2 >= 2), 33abg-23 is refuted
    octi::BotTuningProfile fixedPly1Prof = baseProf;
    fixedPly1Prof.maxSearchDepth = 5;
    fixedPly1Prof.fixStackAndHomeEvalMirage = true;
    fixedPly1Prof.emptyAlliedBasePenalty1B = 3800;
    fixedPly1Prof.qsearchIncludeBaseThreatProngs = true;

    octi::BotTuningProfile compProf = fixedPly1Prof;
    compProf.compositeMoveMaxPly = 4;
    compProf.compositeTacticalOnlyMinPly = 2;

    octi::OctiBotEngine fixedPly1Bot(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    fixedPly1Bot.setTuningProfile(fixedPly1Prof);
    std::vector<octi::BookMoveScore> fixedPly1Scores =
        fixedPly1Bot.searchTopMovesForBook(b43, 5, 0, 36, nullptr);
    bool foundFixedPly1 = false;
    int fixedPly1BlunderScore = findScoredMove(fixedPly1Scores, "33abg-23", &foundFixedPly1);
    CHECK_TRUE(foundFixedPly1);

    octi::OctiBotEngine compBot(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    compBot.setTuningProfile(compProf);
    octi::BotSearchStats compStats;
    std::vector<octi::BookMoveScore> compRootScores =
        compBot.searchTopMovesForBook(b43, 5, 0, 36, &compStats);
    bool foundCompBlunder = false;
    int compBlunderScore = findScoredMove(compRootScores, "33abg-23", &foundCompBlunder);
    CHECK_TRUE(foundCompBlunder);
    CHECK_TRUE(compBlunderScore < baseBlunderScore);
    CHECK_TRUE(compBlunderScore < fixedPly1BlunderScore);
    CHECK_TRUE(compRootScores.front().move != "33abg-23");

    // Direct interior-ply composite check from Ply 44 after 43. 33abg-23 at depth 4
    // (ply 0 = B's 44. 47g+d, ply 1 = A's reply, ply 2 = B's 46. 47efg-45,47dg-56 with depth = 2):
    // compositeTacticalOnlyMinPly = 2 must retain 47efg-45,47dg-56, match minPly = 0 score on 47g+d,
    // and prune quiet non-tactical interior composites (fewer nodes than minPly = 0)
    octi::OctiBoard b44;
    b44.fromPositionString(kPly43Fen);
    b44.getGame()->move("33abg-23");

    octi::BotTuningProfile pureTacProf = baseProf;
    pureTacProf.compositeMoveMaxPly = 4;
    pureTacProf.compositeTacticalOnlyMinPly = 2;

    octi::BotTuningProfile fullCompProf = pureTacProf;
    fullCompProf.compositeTacticalOnlyMinPly = 0;

    octi::OctiBotEngine baseBot44(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    baseBot44.setTuningProfile(baseProf);
    std::vector<octi::BookMoveScore> s44Base =
        baseBot44.searchTopMovesForBook(b44, 4, 0, 36, nullptr);

    octi::OctiBotEngine tacBot44(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    tacBot44.setTuningProfile(pureTacProf);
    octi::BotSearchStats st44Tac;
    std::vector<octi::BookMoveScore> s44Tac =
        tacBot44.searchTopMovesForBook(b44, 4, 0, 36, &st44Tac);

    octi::OctiBotEngine fullBot44(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    fullBot44.setTuningProfile(fullCompProf);
    octi::BotSearchStats st44Full;
    std::vector<octi::BookMoveScore> s44Full =
        fullBot44.searchTopMovesForBook(b44, 4, 0, 36, &st44Full);

    bool fBase = false;
    bool fTac = false;
    bool fFull = false;
    int score47gdBase = findScoredMove(s44Base, "47g+d", &fBase);
    int score47gdTac = findScoredMove(s44Tac, "47g+d", &fTac);
    int score47gdFull = findScoredMove(s44Full, "47g+d", &fFull);
    CHECK_TRUE(fBase && fTac && fFull);
    CHECK_TRUE(score47gdTac > score47gdBase);
    CHECK_EQUAL(score47gdFull, score47gdTac);
    CHECK_TRUE(st44Tac.nodesVisited < st44Full.nodesVisited);

    // Boundary check across compositeTacticalOnlyMinPly = 0..5 on a symmetric position where
    // both sides have purely lateral quiet 2-pod step pairs (A53cg,A53cg and B57cg,B57cg).
    // Use maxSearchDepth = 6 so ply = 0..4 all have depth >= 2, while ply = 5 has depth = 1 < 2.
    const char *kLateralStackFen =
        "2:1:0:0:P:B:-:0:2,0,20,0:2,0,20,0:A53cg,A53cg,B57cg,B57cg";
    octi::OctiBoard bLat;
    bLat.fromPositionString(kLateralStackFen);

    std::uint64_t nodesByMinPly[6] = {0};
    for (int minPly = 0; minPly <= 5; ++minPly) {
        octi::BotTuningProfile p = baseProf;
        p.timeBudgetMs = 300000;
        p.maxSearchDepth = 6;
        p.rootBranchCap = 36;
        p.treeBranchCap = 36;
        p.compositeMoveMaxPly = 4;
        p.compositeTacticalOnlyMinPly = minPly;
        octi::OctiBotEngine eng(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
        eng.setTuningProfile(p);
        octi::BotSearchStats st;
        (void)eng.chooseMove(bLat, &st);
        nodesByMinPly[minPly] = st.nodesVisited;
    }

    // At maxSearchDepth = 6, composites are generated for ply = 0..4, so minPly = 5 matches minPly = 0
    CHECK_EQUAL(nodesByMinPly[0], nodesByMinPly[5]);
    // Pruning quiet lateral 2-pod step pairs at ply >= 1..4 strictly cuts interior nodes
    CHECK_TRUE(nodesByMinPly[1] < nodesByMinPly[2]);
    CHECK_TRUE(nodesByMinPly[2] < nodesByMinPly[3]);
    CHECK_TRUE(nodesByMinPly[3] < nodesByMinPly[4]);
    CHECK_TRUE(nodesByMinPly[4] < nodesByMinPly[5]);
}

TEST(OctiPostMortemEngineBugFixes, StackAndHomeEvalMirageFixEliminatesPhantomCushion)
{
    // Ply 41 FEN: B has 3 armed pods (B46bcdefg, B57efg, B57g), 3 unarmed home pods (B37, B57, B77),
    // and freePods(B) == 0 (all pods deployed on board).
    const char *kPly41Fen =
        "2:1:0:0:P:A:-:0:1,1,16,0:0,1,14,0:"
        "A33abg,A33,A73h,A34abgh,A64bh,B46bcdefg,B37,B57efg,B57g,B57,B77";

    octi::OctiBoard b41;
    b41.fromPositionString(kPly41Fen);

    octi::BotTuningProfile baseProf;
    baseProf.fixStackAndHomeEvalMirage = false;
    octi::OctiEvalBreakdown bdBase =
        octi::OctiBotEngine::evaluatePositionBreakdown(b41, octi::PLAYER_A, baseProf);

    CHECK_EQUAL(2800, bdBase.stackAndCapturedBaseHolding);
    CHECK_EQUAL(7800, bdBase.undeployedAndUnarmedHomePenalty);
    CHECK_EQUAL(5000, bdBase.singlePodMonopoly);
    CHECK_EQUAL(23860, bdBase.totalScore);

    octi::BotTuningProfile fixProf;
    fixProf.fixStackAndHomeEvalMirage = true;
    octi::OctiEvalBreakdown bdFix =
        octi::OctiBotEngine::evaluatePositionBreakdown(b41, octi::PLAYER_A, fixProf);

    // 1. 2-pod armed battery exemption eliminates +2,800 distortion (net -3750 + 3750 = 0)
    CHECK_EQUAL(0, bdFix.stackAndCapturedBaseHolding);
    // 2. freePods(B) == 0 no longer pays (4 - 0) * 1500 = 6,000 undeployed-reserve penalty
    CHECK_EQUAL(1800, bdFix.undeployedAndUnarmedHomePenalty);
    // 3. armedPods(B) == 3 >= 2 no longer triggers singlePodMonopoly (+5,000 removed)
    CHECK_EQUAL(0, bdFix.singlePodMonopoly);
    CHECK_EQUAL(bdFix.sumTerms(), bdFix.totalScore);
    CHECK_EQUAL(10060, bdFix.totalScore);

    // Verify clean 2-pod armed battery on 47 (Ply 44 FEN) has 0 stack penalty, while 3 armed pods pay 2800
    octi::OctiBoard b44;
    b44.fromPositionString(
        "2:1:0:0:P:B:-:0:1,1,15,0:0,1,14,0:"
        "A23abg,A33a,A73h,A34abgh,A64bh,B46bcdefg,B37,B47efg,B47g,B57,B77");
    CHECK_EQUAL(2800, octi::OctiBotEngine::evaluatePositionBreakdown(
        b44, octi::PLAYER_A, baseProf).stackAndCapturedBaseHolding);
    CHECK_EQUAL(0, octi::OctiBotEngine::evaluatePositionBreakdown(
        b44, octi::PLAYER_A, fixProf).stackAndCapturedBaseHolding);

    octi::OctiBoard b3Armed;
    b3Armed.fromPositionString(
        "2:1:0:0:P:B:-:0:1,1,15,0:0,1,13,0:"
        "A23abg,A33a,A73h,A34abgh,A64bh,B37,B47efg,B47dg,B47e,B57,B77");
    CHECK_EQUAL(2800, octi::OctiBotEngine::evaluatePositionBreakdown(
        b3Armed, octi::PLAYER_A, fixProf).stackAndCapturedBaseHolding);

    // Verify genuine single-pod monopoly (armedPods == 1) and genuine undeployed reserve
    // (freePods >= 2 && unarmedHome >= 2 && freeProngs >= 8) still fire under fixProf
    octi::OctiBoard bGenuine;
    bGenuine.fromPositionString(
        "2:1:0:0:P:A:-:0:2,0,12,0:0,0,12,0:A33,A53,A44abcef,B37e,B57f,B77g");
    octi::OctiEvalBreakdown bdGen =
        octi::OctiBotEngine::evaluatePositionBreakdown(bGenuine, octi::PLAYER_A, fixProf);
    CHECK_EQUAL(-3000, bdGen.singlePodMonopoly);
    // 2 freePods * 900 + 2 unarmedHome * 900 = -3600
    CHECK_EQUAL(-3600, bdGen.undeployedAndUnarmedHomePenalty);
}

TEST(OctiPostMortemEngineBugFixes, EmptyAlliedBasePenalty1BPrefersRegarrisoningOverStacking)
{
    // Ply 37 FEN in 2P_1B: A's home square 53 is empty; B46cdefg is at Chebyshev dist 3 from 53
    const char *kPly37Fen =
        "2:1:0:0:P:A:-:0:2,1,17,0:1,1,15,0:"
        "A33abg,A73h,A34abgh,A64h,B46cdefg,B37,B57efg,B57g,B77";

    octi::OctiBoard b37;
    b37.fromPositionString(kPly37Fen);

    octi::BotTuningProfile baseProf;
    CHECK_EQUAL(0, octi::OctiBotEngine::evaluatePositionBreakdown(
        b37, octi::PLAYER_A, baseProf).lostAlliedBases);

    octi::BotTuningProfile penProf;
    penProf.emptyAlliedBasePenalty1B = 3800;

    // dEnemy == 3 -> urgencyPct = 130 -> -(3800 * 130) / 100 = -4940
    octi::OctiEvalBreakdown bdBBase =
        octi::OctiBotEngine::evaluatePositionBreakdown(b37, octi::PLAYER_B, baseProf);
    octi::OctiEvalBreakdown bdA =
        octi::OctiBotEngine::evaluatePositionBreakdown(b37, octi::PLAYER_A, penProf);
    octi::OctiEvalBreakdown bdB =
        octi::OctiBotEngine::evaluatePositionBreakdown(b37, octi::PLAYER_B, penProf);
    CHECK_EQUAL(-4940, bdA.lostAlliedBases);
    CHECK_EQUAL(0, bdB.lostAlliedBases);
    CHECK_EQUAL(bdBBase.twoPlyBaseThreat + 4940, bdB.twoPlyBaseThreat);
    CHECK_EQUAL(-bdA.totalScore, bdB.totalScore);

    // Check urgency tiers: dEnemy <= 2 (175%) and dEnemy >= 4 (100%)
    octi::OctiBoard bDist2;
    bDist2.fromPositionString(
        "2:1:0:0:P:A:-:0:2,1,17,0:1,1,15,0:"
        "A33abg,A73h,A34abgh,A64h,B45cdefg,B37,B57efg,B57g,B77");
    CHECK_EQUAL(-6650, octi::OctiBotEngine::evaluatePositionBreakdown(
        bDist2, octi::PLAYER_A, penProf).lostAlliedBases);

    octi::OctiBoard bDist4;
    bDist4.fromPositionString(
        "2:1:0:0:P:A:-:0:2,1,17,0:1,1,15,0:"
        "A33abg,A73h,A34abgh,A64h,B37,B57efg,B57g,B77");
    CHECK_EQUAL(-3800, octi::OctiBotEngine::evaluatePositionBreakdown(
        bDist4, octi::PLAYER_A, penProf).lostAlliedBases);

    // Garrisoning empty 53 eliminates the -4940 penalty and scores strictly higher than stacking on 33
    octi::OctiBoard bGarrison53;
    bGarrison53.fromPositionString(kPly37Fen);
    bGarrison53.getGame()->move("53");
    octi::OctiEvalBreakdown bd53 =
        octi::OctiBotEngine::evaluatePositionBreakdown(bGarrison53, octi::PLAYER_A, penProf);
    CHECK_EQUAL(0, bd53.lostAlliedBases);

    octi::OctiBoard bStack33;
    bStack33.fromPositionString(kPly37Fen);
    bStack33.getGame()->move("33");
    octi::OctiEvalBreakdown bd33 =
        octi::OctiBotEngine::evaluatePositionBreakdown(bStack33, octi::PLAYER_A, penProf);
    CHECK_EQUAL(-4940, bd33.lostAlliedBases);
    CHECK_TRUE(bd53.totalScore > bd33.totalScore);

    penProf.useOpeningBook = false;
    penProf.rootScoreJitter = 0;
    penProf.lazySmpThreads = 1;
    penProf.timeBudgetMs = 0;
    octi::OctiBotEngine bot37(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    bot37.setTuningProfile(penProf);
    std::vector<octi::BookMoveScore> s37 =
        bot37.searchTopMovesForBook(b37, 2, 0, 36, nullptr);
    bool f53 = false;
    bool f33 = false;
    int searchScore53 = findScoredMove(s37, "53", &f53);
    int searchScore33 = findScoredMove(s37, "33", &f33);
    CHECK_TRUE(f53 && f33);
    CHECK_TRUE(searchScore53 > searchScore33);

    // Zero effect in 2P_3B (basesNeeded == 3 > 1) and in OFK
    octi::OctiBoard b3B;
    b3B.fromPositionString(
        "2:3:0:0:P:A:-:0:2,1,17,0:1,1,15,0:"
        "A33abg,A73h,A34abgh,A64h,B46cdefg,B37,B57efg,B57g,B77");
    CHECK_EQUAL(0, octi::OctiBotEngine::evaluatePositionBreakdown(
        b3B, octi::PLAYER_A, penProf).lostAlliedBases);

    octi::OctiBoard bOfk;
    bOfk.fromPositionString("OFK:1:0:0:P:A:-:0:0,0,10,0:0,0,10,0:A22a,A32,B26e,B36");
    CHECK_EQUAL(0, octi::OctiBotEngine::evaluatePositionBreakdown(
        bOfk, octi::PLAYER_A, penProf).lostAlliedBases);
}

TEST(OctiPostMortemEngineBugFixes, QuiesceIncludesDistOneBaseThreatProngInsertion)
{
    // Ply 49 FEN: B22efg sits at Chebyshev dist 1 from A's base 33 (occupied by A33a with empty 44).
    const char *kPly49Fen =
        "2:1:0:0:P:A:-:0:1,2,14,0:0,1,17,0:"
        "B22efg,A33a,A73h,A24abeg,A64bh,B46bcdefg,B37,B47dg,B57,B77";

    octi::OctiBoard b49;
    b49.fromPositionString(kPly49Fen);

    octi::BotTuningProfile baseProf;
    baseProf.useOpeningBook = false;
    baseProf.rootScoreJitter = 0;
    baseProf.lazySmpThreads = 1;
    baseProf.timeBudgetMs = 0;
    baseProf.maxSearchDepth = 1;
    baseProf.maxQuiesceDepth = 2;
    baseProf.quiesceBranchCap = 6;
    baseProf.qsearchIncludeBaseThreatProngs = false;

    octi::BotTuningProfile qProngProf = baseProf;
    qProngProf.qsearchIncludeBaseThreatProngs = true;

    octi::OctiBotEngine baseBot(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    baseBot.setTuningProfile(baseProf);
    octi::BotSearchStats stBase;
    std::vector<octi::BookMoveScore> sBase =
        baseBot.searchTopMovesForBook(b49, 1, 0, 36, &stBase);

    octi::OctiBotEngine qBot(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    qBot.setTuningProfile(qProngProf);
    octi::BotSearchStats stQ;
    std::vector<octi::BookMoveScore> sQ =
        qBot.searchTopMovesForBook(b49, 1, 0, 36, &stQ);

    // At depth 1, each A root move enters quiesce(qDepth == 0) for B (Ply 50):
    // B searches 22efg+b instead of standing pat, exposing the threat after passive 73h+a or vacating 33a-34
    bool fPassBase = false;
    bool fPassQ = false;
    bool fVacBase = false;
    bool fVacQ = false;
    int passScoreBase = findScoredMove(sBase, "73h+a", &fPassBase);
    int passScoreQ = findScoredMove(sQ, "73h+a", &fPassQ);
    int vacScoreBase = findScoredMove(sBase, "33a-34", &fVacBase);
    int vacScoreQ = findScoredMove(sQ, "33a-34", &fVacQ);
    CHECK_TRUE(fPassBase && fPassQ && fVacBase && fVacQ);
    CHECK_TRUE(stQ.nodesVisited > stBase.nodesVisited);
    CHECK_TRUE(passScoreQ < passScoreBase);
    CHECK_TRUE(vacScoreQ <= vacScoreBase - 7500);

    // Defensive 33a+f (enabling 33af-11x counter-capture at qDepth == 1) beats passive 73h+a
    bool fDefQ = false;
    int defScoreQ = findScoredMove(sQ, "33a+f", &fDefQ);
    CHECK_TRUE(fDefQ);
    CHECK_TRUE(defScoreQ > passScoreQ);

    // Ordering priority (+14000):
    // (a) When no capture/win exists (A22 + A33 block 44befg's 'f' prong, while 53 is empty),
    //     quiesceBranchCap = 1 retains 44befg+d (+14000) and detects the -8500 oneStepBaseThreat.
    // (b) On Ply 57 FEN (where A33 is jumpable via 44befg-22x at +16000), quiesceBranchCap = 1
    //     orders the capture ahead of 44befg+d (+14000 < +16000), matching baseBot's capture score.
    octi::OctiBoard b57NoCap;
    b57NoCap.fromPositionString(
        "2:1:0:0:P:A:-:0:0,2,11,0:0,1,19,0:"
        "A22,A33,A73fh,A24abeg,B44befg,A64bh,B46bcdefg,B37,B47dg,B57,B77");
    octi::BotTuningProfile cap1Prof = qProngProf;
    cap1Prof.quiesceBranchCap = 1;
    octi::OctiBotEngine cap1Bot(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    cap1Bot.setTuningProfile(cap1Prof);
    std::vector<octi::BookMoveScore> sNoCapQ =
        cap1Bot.searchTopMovesForBook(b57NoCap, 1, 0, 36, nullptr);
    std::vector<octi::BookMoveScore> sNoCapBase =
        baseBot.searchTopMovesForBook(b57NoCap, 1, 0, 36, nullptr);
    bool fNoCapBase = false;
    bool fNoCapQ = false;
    int scNoCapBase = findScoredMove(sNoCapBase, "24abeg-23", &fNoCapBase);
    int scNoCapQ = findScoredMove(sNoCapQ, "24abeg-23", &fNoCapQ);
    CHECK_TRUE(fNoCapBase && fNoCapQ);
    CHECK_TRUE(scNoCapQ <= scNoCapBase - 6000);

    octi::OctiBoard b57;
    b57.fromPositionString(
        "2:1:0:0:P:A:-:0:0,3,11,0:0,1,19,0:"
        "A33,A73fh,A24abeg,B44befg,A64bh,B46bcdefg,B37,B47dg,B57,B77");
    std::vector<octi::BookMoveScore> s57Q =
        cap1Bot.searchTopMovesForBook(b57, 1, 0, 36, nullptr);
    std::vector<octi::BookMoveScore> s57Base =
        baseBot.searchTopMovesForBook(b57, 1, 0, 36, nullptr);
    bool f57Base = false;
    bool f57Q = false;
    CHECK_EQUAL(findScoredMove(s57Base, "24abeg-23", &f57Base),
                findScoredMove(s57Q, "24abeg-23", &f57Q));
    CHECK_TRUE(f57Base && f57Q);

    // qDepth > 0 boundary: when A makes a tactical capture at qDepth == 0, B at qDepth == 1
    // must NOT expand quiet +prong insertions
    octi::OctiBoard bQDepth1;
    bQDepth1.fromPositionString(
        "2:1:0:0:P:B:-:0:1,1,14,0:1,1,17,0:"
        "B22efg,A33a,A73h,B64e,A65e,B57");
    // B plays 57+e at depth 1 -> qDepth 0 is A (captures B64e via 65e-63x) -> qDepth 1 is B (has B22efg):
    // if B generated 22efg+b at qDepth == 1, maxQuiesceDepth = 2 would visit more nodes than maxQuiesceDepth = 1
    octi::BotTuningProfile qd1Base = baseProf;
    qd1Base.maxQuiesceDepth = 2;
    octi::BotTuningProfile qd1Prong = qProngProf;
    qd1Prong.maxQuiesceDepth = 2;
    octi::OctiBotEngine botQdBase(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    botQdBase.setTuningProfile(qd1Base);
    octi::OctiBotEngine botQdProng(octi::BotDifficulty::EXPERT, 0x5EED2026ULL);
    botQdProng.setTuningProfile(qd1Prong);
    // From B's turn, B has 22efg+b as a normal root move; for non-22 root moves like 57+e,
    // qDepth == 0 is A (no dist-1 enemy base pod) and qDepth == 1 is B (must NOT generate 22efg+b!)
    std::vector<octi::BookMoveScore> sqdBase =
        botQdBase.searchTopMovesForBook(bQDepth1, 1, 0, 36, nullptr);
    std::vector<octi::BookMoveScore> sqdProng =
        botQdProng.searchTopMovesForBook(bQDepth1, 1, 0, 36, nullptr);
    bool fQdB = false;
    bool fQdP = false;
    CHECK_EQUAL(findScoredMove(sqdBase, "57+e", &fQdB),
                findScoredMove(sqdProng, "57+e", &fQdP));
    CHECK_TRUE(fQdB && fQdP);

    // Blocked capture-hop check: if A33 is on 33 AND A44 is on 44 (neither can step away),
    // B22efg+b has neither a step onto 33 nor a jump over 33, so quiesce(qDepth == 0) ignores 22efg+b
    octi::OctiBoard bBlocked;
    bBlocked.fromPositionString(
        "2:1:0:0:P:A:-:0:0,2,14,0:0,1,17,0:"
        "B22efg,A33,A73h,A44,B57");
    octi::BotSearchStats stBlkBase;
    octi::BotSearchStats stBlkQ;
    (void)baseBot.searchTopMovesForBook(bBlocked, 1, 0, 36, &stBlkBase);
    (void)qBot.searchTopMovesForBook(bBlocked, 1, 0, 36, &stBlkQ);
    CHECK_EQUAL(stBlkBase.nodesVisited, stBlkQ.nodesVisited);
}

TEST(OctiPostMortemEngineBugFixes, DefaultOffProfilePreservesBitIdenticalEvalAndSearch)
{
    octi::BotTuningProfile def;
    CHECK_EQUAL(1, def.compositeMoveMaxPly);
    CHECK_EQUAL(0, def.compositeTacticalOnlyMinPly);
    CHECK_FALSE(def.qsearchIncludeBaseThreatProngs);
    CHECK_FALSE(def.fixStackAndHomeEvalMirage);
    CHECK_EQUAL(0, def.emptyAlliedBasePenalty1B);

    const char *fens[] = {
        "2:1:0:0:P:A:-:0:2,1,17,0:1,1,15,0:A33abg,A73h,A34abgh,A64h,B46cdefg,B37,B57efg,B57g,B77",
        "2:1:0:0:P:A:-:0:1,1,16,0:0,1,14,0:A33abg,A33,A73h,A34abgh,A64bh,B46bcdefg,B37,B57efg,B57g,B57,B77",
        "2:1:0:0:P:A:-:0:1,1,15,0:0,1,14,0:A33abg,A33a,A73h,A34abgh,A64bh,B46bcdefg,B37,B47efg,B47g,B57,B77",
        "2:1:0:0:P:A:-:0:1,2,14,0:0,1,17,0:B22efg,A33a,A73h,A24abeg,A64bh,B46bcdefg,B37,B47dg,B57,B77"
    };
    const int expectedEvals[] = { 18230, 23860, 22740, 6560 };

    for (std::size_t i = 0; i < 4; ++i) {
        octi::OctiBoard b;
        b.fromPositionString(fens[i]);
        octi::OctiEvalBreakdown bd =
            octi::OctiBotEngine::evaluatePositionBreakdown(b, octi::PLAYER_A, def);
        CHECK_EQUAL(expectedEvals[i], bd.totalScore);
        CHECK_EQUAL(bd.sumTerms(), bd.totalScore);
    }

    octi::OctiBoard b43;
    b43.fromPositionString(fens[2]);
    def.useOpeningBook = false;
    def.rootScoreJitter = 0;
    def.lazySmpThreads = 1;
    def.maxSearchDepth = 4;
    def.timeBudgetMs = 300000;
    octi::OctiBotEngine eng(octi::BotDifficulty::EXPERT, 42);
    eng.setTuningProfile(def);
    octi::BotSearchStats st;
    std::string bm = eng.chooseMove(b43, &st);
    STRCMP_EQUAL("33abg-23", bm.c_str());
    CHECK_EQUAL(45301ULL, st.nodesVisited);
    CHECK_EQUAL(10630, st.bestScore);
}

TEST_GROUP(OctiVariantTuningParams)
{
};

TEST(OctiVariantTuningParams, DefaultStructValuesAndSixVariantEvalParity)
{
    const octi::BotTuningProfile def{};
    CHECK_EQUAL(135, def.ofkPodValScalePct);
    CHECK_EQUAL(4800, def.ofkCapturedPodPenalty);
    CHECK_EQUAL(180, def.ofkReserveFreeProngVal);
    CHECK_EQUAL(20000, def.fourPlayerEliminatedPenalty);
    CHECK_EQUAL(50, def.fourPlayerWipeoutDangerPct);
    CHECK_EQUAL(150, def.reserveFreeSuperProngVal);
    CHECK_EQUAL(850, def.nonEdgelessBackRankPenalty2P);
    CHECK_EQUAL(3500, def.moveOrderBackRankRetreatPenalty2P);
    CHECK_EQUAL(35, def.multiBaseTwoNeededThreatScalePct);
    CHECK_EQUAL(20, def.multiBaseThreeNeededThreatScalePct);
    CHECK_EQUAL(60, def.multiBaseTwoNeededScaleFloorPct);
    CHECK_EQUAL(35, def.multiBaseThreeNeededScaleFloorPct);
    CHECK_EQUAL(175, def.emptyAlliedBase1BCloseUrgencyPct);
    CHECK_EQUAL(130, def.emptyAlliedBase1BMidUrgencyPct);

    const char *variantFens[6] = {
        // 2P_3B
        "2:3:0:0:P:B:-:0:3,0,17,0:3,0,16,0:B51def,B33,A53,A54abegh,A64abfh,A55afh,B46de,B37",
        // 2P_1B
        "2:1:0:0:P:A:-:0:2,1,17,0:1,1,15,0:A33abg,A73h,A34abgh,A64h,B46cdefg,B37,B57efg,B57g,B77",
        // OFK
        "OFK:1:0:0:P:A:-:0:0,1,8,0:0,0,9,0:A22,A32,A42,B44cef,B26d,B56,B47bde",
        // 4P
        "4:3:1:0:P:D:-:3:3,1,11,0:0,4,10,0:2,0,9,0:2,0,10,1:D33ab!g!h,D73,C37cfg!,C77",
        // 2P_3B_ED
        "2:3:1:1:P:A:-:0:4,0,15,0:4,0,14,0:B33cde!f,A73abh,A44abh,B65defg,A37a!bch,B77def",
        // 2P_3B_SP
        "2:3:1:0:P:B:-:0:4,0,17,0:4,0,18,0:A33abh,B53ade!,B53e,B37def,A77a!be,A77ab"
    };

    for (std::size_t i = 0; i < 6; ++i) {
        octi::OctiBoard b;
        b.fromPositionString(variantFens[i]);
        const octi::OctiPlayer turn = b.getGame()->getTurn();
        const octi::OctiEvalBreakdown bd =
            octi::OctiBotEngine::evaluatePositionBreakdown(b, turn, def);
        CHECK_EQUAL(bd.sumTerms(), bd.totalScore);
        CHECK_EQUAL(bd.totalScore,
                    octi::OctiBotEngine::evaluatePosition(b, turn, def));
    }
}

TEST(OctiVariantTuningParams, OfkParametersAffectOfkAndIsolateNonOfkVariants)
{
    octi::OctiBoard ofkBoard;
    ofkBoard.fromPositionString(
        "OFK:1:0:0:P:A:-:0:0,1,8,0:0,0,5,0:A22,A32a,A42ab,A52abc,B26de,B36,B46,B56");

    octi::OctiBoard twoP3B;
    twoP3B.fromPositionString(
        "2:3:0:0:P:A:-:0:2,1,16,0:3,0,12,0:A33a,A53ab,A73abc,B37de,B57");
    octi::OctiBoard twoP1B;
    twoP1B.fromPositionString(
        "2:1:0:0:P:A:-:0:2,1,16,0:3,0,12,0:A33a,A53ab,A73abc,B37de,B57");
    octi::OctiBoard fourP;
    fourP.fromPositionString(
        "4:3:0:0:P:A:-:0:2,1,10,0:2,0,8,0:2,0,10,0:2,0,9,0:A33a,B37e,C77ef,D73c");

    const octi::BotTuningProfile base{};
    const auto ofkBase = octi::OctiBotEngine::evaluatePositionBreakdown(
        ofkBoard, octi::PLAYER_A, base);
    const int base2P3B = octi::OctiBotEngine::evaluatePosition(twoP3B, octi::PLAYER_A, base);
    const int base2P1B = octi::OctiBotEngine::evaluatePosition(twoP1B, octi::PLAYER_A, base);
    const int base4P = octi::OctiBotEngine::evaluatePosition(fourP, octi::PLAYER_A, base);

    {
        octi::BotTuningProfile p = base;
        p.ofkPodValScalePct = 200;
        const auto ofkMod = octi::OctiBotEngine::evaluatePositionBreakdown(
            ofkBoard, octi::PLAYER_A, p);
        CHECK_EQUAL(2698, ofkMod.podMaterial - ofkBase.podMaterial);
        CHECK_EQUAL(2698, ofkMod.totalScore - ofkBase.totalScore);
        CHECK_EQUAL(base2P3B, octi::OctiBotEngine::evaluatePosition(twoP3B, octi::PLAYER_A, p));
        CHECK_EQUAL(base2P1B, octi::OctiBotEngine::evaluatePosition(twoP1B, octi::PLAYER_A, p));
        CHECK_EQUAL(base4P, octi::OctiBotEngine::evaluatePosition(fourP, octi::PLAYER_A, p));
    }

    {
        octi::BotTuningProfile p = base;
        p.ofkCapturedPodPenalty = 7000;
        const auto ofkMod = octi::OctiBotEngine::evaluatePositionBreakdown(
            ofkBoard, octi::PLAYER_A, p);
        CHECK_EQUAL(-2200,
                    ofkMod.reserveAndCapturedMaterial - ofkBase.reserveAndCapturedMaterial);
        CHECK_EQUAL(-2200, ofkMod.totalScore - ofkBase.totalScore);
        CHECK_EQUAL(base2P3B, octi::OctiBotEngine::evaluatePosition(twoP3B, octi::PLAYER_A, p));
        CHECK_EQUAL(base2P1B, octi::OctiBotEngine::evaluatePosition(twoP1B, octi::PLAYER_A, p));
        CHECK_EQUAL(base4P, octi::OctiBotEngine::evaluatePosition(fourP, octi::PLAYER_A, p));
    }

    {
        octi::BotTuningProfile p = base;
        p.ofkReserveFreeProngVal = 300;
        const auto ofkMod = octi::OctiBotEngine::evaluatePositionBreakdown(
            ofkBoard, octi::PLAYER_A, p);
        CHECK_EQUAL(360,
                    ofkMod.reserveAndCapturedMaterial - ofkBase.reserveAndCapturedMaterial);
        CHECK_EQUAL(360, ofkMod.totalScore - ofkBase.totalScore);
        CHECK_EQUAL(base2P3B, octi::OctiBotEngine::evaluatePosition(twoP3B, octi::PLAYER_A, p));
        CHECK_EQUAL(base2P1B, octi::OctiBotEngine::evaluatePosition(twoP1B, octi::PLAYER_A, p));
        CHECK_EQUAL(base4P, octi::OctiBotEngine::evaluatePosition(fourP, octi::PLAYER_A, p));
    }
}

TEST(OctiVariantTuningParams, FourPlayerAndSuperProngParametersIsolateCleanly)
{
    const octi::BotTuningProfile base{};

    // 4P with PLAYER_B eliminated (elimMask = 2)
    octi::OctiBoard fourPElim;
    fourPElim.fromPositionString(
        "4:3:0:0:P:A:-:2:2,0,10,0:0,4,10,0:2,0,10,0:2,0,10,0:A33a,C77e,D73g");

    // 4P where PLAYER_A has 1 board pod at (4,4) [55] with 0 free pods while partner C
    // still has a pod at (6,6) [77], and enemy B at (4,5) [56] threatens jump-capture South to (4,3) [54]
    octi::OctiBoard fourPWipeout;
    fourPWipeout.fromPositionString(
        "4:3:0:0:P:A:-:0:0,3,10,0:2,0,10,0:2,0,10,0:2,0,10,0:A55a,B56ef,C77e,D73g");

    octi::OctiBoard twoP;
    twoP.fromPositionString(
        "2:3:0:0:P:A:-:0:0,3,15,0:3,0,15,0:A55a,B56ef");
    octi::OctiBoard ofk;
    ofk.fromPositionString(
        "OFK:1:0:0:P:A:-:0:0,1,8,0:0,0,8,0:A33a,B34ef,A22,B26");

    const auto elimBase = octi::OctiBotEngine::evaluatePositionBreakdown(
        fourPElim, octi::PLAYER_A, base);
    const auto wipeBase = octi::OctiBotEngine::evaluatePositionBreakdown(
        fourPWipeout, octi::PLAYER_A, base);
    const int base2P = octi::OctiBotEngine::evaluatePosition(twoP, octi::PLAYER_A, base);
    const int baseOfk = octi::OctiBotEngine::evaluatePosition(ofk, octi::PLAYER_A, base);

    {
        octi::BotTuningProfile p = base;
        p.fourPlayerEliminatedPenalty = 35000;
        const auto elimMod = octi::OctiBotEngine::evaluatePositionBreakdown(
            fourPElim, octi::PLAYER_A, p);
        CHECK_EQUAL(15000,
                    elimMod.reserveAndCapturedMaterial - elimBase.reserveAndCapturedMaterial);
        CHECK_EQUAL(base2P, octi::OctiBotEngine::evaluatePosition(twoP, octi::PLAYER_A, p));
        CHECK_EQUAL(baseOfk, octi::OctiBotEngine::evaluatePosition(ofk, octi::PLAYER_A, p));
    }

    {
        octi::BotTuningProfile p = base;
        p.fourPlayerWipeoutDangerPct = 80;
        const auto wipeMod = octi::OctiBotEngine::evaluatePositionBreakdown(
            fourPWipeout, octi::PLAYER_A, p);
        CHECK_EQUAL(-4500, wipeMod.hangingVulnerability - wipeBase.hangingVulnerability);
        CHECK_EQUAL(base2P, octi::OctiBotEngine::evaluatePosition(twoP, octi::PLAYER_A, p));
        CHECK_EQUAL(baseOfk, octi::OctiBotEngine::evaluatePosition(ofk, octi::PLAYER_A, p));
    }

    // Overflow safety: 50,000,000 * 60 = 3,000,000,000 > INT32_MAX must not overflow
    {
        octi::BotTuningProfile pZero = base;
        pZero.wipeoutDangerPenalty = 50000000;
        pZero.fourPlayerWipeoutDangerPct = 0;
        octi::BotTuningProfile pSixty = pZero;
        pSixty.fourPlayerWipeoutDangerPct = 60;
        const auto bdZero = octi::OctiBotEngine::evaluatePositionBreakdown(
            fourPWipeout, octi::PLAYER_A, pZero);
        const auto bdSixty = octi::OctiBotEngine::evaluatePositionBreakdown(
            fourPWipeout, octi::PLAYER_A, pSixty);
        CHECK_EQUAL(-30000000, bdSixty.hangingVulnerability - bdZero.hangingVulnerability);
    }

    // Superprong reserve valuation
    {
        octi::OctiBoard spBoard;
        spBoard.fromPositionString(
            "2:3:1:0:P:A:-:0:4,0,20,2:4,0,20,1:A33a!,B57e");
        const auto spBase = octi::OctiBotEngine::evaluatePositionBreakdown(
            spBoard, octi::PLAYER_A, base);
        octi::BotTuningProfile p = base;
        p.reserveFreeSuperProngVal = 400;
        const auto spMod = octi::OctiBotEngine::evaluatePositionBreakdown(
            spBoard, octi::PLAYER_A, p);
        CHECK_EQUAL(250,
                    spMod.reserveAndCapturedMaterial - spBase.reserveAndCapturedMaterial);
        CHECK_EQUAL(base2P, octi::OctiBotEngine::evaluatePosition(twoP, octi::PLAYER_A, p));
        CHECK_EQUAL(baseOfk, octi::OctiBotEngine::evaluatePosition(ofk, octi::PLAYER_A, p));
    }
}

TEST(OctiVariantTuningParams, EdgelessAndBackRankParametersIsolateByGeometryAndMode)
{
    const octi::BotTuningProfile base{};

    // A52a sits at y=1 (< 2) behind Player A's home rank
    octi::OctiBoard nonEdgeless2P;
    nonEdgeless2P.fromPositionString(
        "2:3:0:0:P:A:-:0:3,0,20,0:4,0,20,0:A52a,B57e");
    octi::OctiBoard edgeless2P;
    edgeless2P.fromPositionString(
        "2:3:0:1:P:A:-:0:3,0,20,0:4,0,20,0:A52a,B57e");
    octi::OctiBoard nonEdgeless4P;
    nonEdgeless4P.fromPositionString(
        "4:3:0:0:P:A:-:0:2,0,10,0:2,0,10,0:2,0,10,0:2,0,10,0:A52a,B37e,C77e,D73g");

    const auto base2P = octi::OctiBotEngine::evaluatePositionBreakdown(
        nonEdgeless2P, octi::PLAYER_A, base);
    const int baseEd = octi::OctiBotEngine::evaluatePosition(edgeless2P, octi::PLAYER_A, base);
    const int base4P = octi::OctiBotEngine::evaluatePosition(nonEdgeless4P, octi::PLAYER_A, base);

    {
        octi::BotTuningProfile p = base;
        p.nonEdgelessBackRankPenalty2P = 2000;
        const auto mod2P = octi::OctiBotEngine::evaluatePositionBreakdown(
            nonEdgeless2P, octi::PLAYER_A, p);
        CHECK_EQUAL(-1150, mod2P.structuralPositional - base2P.structuralPositional);
        CHECK_EQUAL(baseEd, octi::OctiBotEngine::evaluatePosition(edgeless2P, octi::PLAYER_A, p));
        CHECK_EQUAL(base4P, octi::OctiBotEngine::evaluatePosition(nonEdgeless4P, octi::PLAYER_A, p));
    }

    // Move ordering behind-home-rank retreat penalty in 2P non-edgeless only
    {
        octi::OctiBoard retreat2P;
        retreat2P.fromPositionString(
            "2:3:0:0:P:A:-:0:0,0,0,0:0,0,0,0:A53ce,B57e");
        octi::OctiBoard retreatEd;
        retreatEd.fromPositionString(
            "2:3:0:1:P:A:-:0:0,0,0,0:0,0,0,0:A53ce,B57e");
        octi::OctiBoard retreat4P;
        retreat4P.fromPositionString(
            "4:3:0:0:P:A:-:0:0,0,0,0:0,0,0,0:0,0,0,0:0,0,0,0:A53ce,B37e,C77e,D73g");

        octi::BotTuningProfile searchBase = base;
        searchBase.useOpeningBook = false;
        searchBase.rootScoreJitter = 0;
        searchBase.lazySmpThreads = 1;
        searchBase.maxSearchDepth = 1;
        searchBase.maxQuiesceDepth = 0;
        searchBase.rootBranchCap = 1;
        searchBase.timeBudgetMs = 5000;

        octi::OctiBotEngine eng(octi::BotDifficulty::TACTICAL, 12345ULL);
        eng.setTuningProfile(searchBase);
        const std::string defMove2P = eng.chooseMove(retreat2P);
        const std::string defMoveEd = eng.chooseMove(retreatEd);
        const std::string defMove4P = eng.chooseMove(retreat4P);
        CHECK_TRUE(defMove2P != "53ce-52");

        octi::BotTuningProfile rewardRetreat = searchBase;
        rewardRetreat.moveOrderBackRankRetreatPenalty2P = -50000;
        eng.setTuningProfile(rewardRetreat);
        STRCMP_EQUAL("53ce-52", eng.chooseMove(retreat2P).c_str());
        STRCMP_EQUAL(defMoveEd.c_str(), eng.chooseMove(retreatEd).c_str());
        STRCMP_EQUAL(defMove4P.c_str(), eng.chooseMove(retreat4P).c_str());
    }
}

TEST(OctiVariantTuningParams, MultiBaseAndSingleBaseParametersIsolateByBaseCount)
{
    const octi::BotTuningProfile base{};

    // 2P_3B with 0 captured enemy bases -> basesNeeded == 3
    // A36a at (2,5) threatens step into B home (2,6) [37]
    octi::OctiBoard threeNeeded2P;
    threeNeeded2P.fromPositionString(
        "2:3:0:0:P:A:-:0:3,0,20,0:3,0,20,0:A36a,B77e");

    // 2P_3B with 1 captured enemy base (A37a on B home (2,6)) -> basesNeeded == 2
    // A56a at (4,5) threatens step into B home (4,6) [57]
    octi::OctiBoard twoNeeded2P;
    twoNeeded2P.fromPositionString(
        "2:3:0:0:P:A:-:0:2,0,20,0:3,0,20,0:A37a,A56a,B77e");

    // 2P_1B with emptyAlliedBasePenalty1B active
    // B54e at (4,3) is dist <= 2 from empty allied bases (4,2) and (6,2), while A33a holds (2,2)
    octi::OctiBoard oneBaseClose;
    oneBaseClose.fromPositionString(
        "2:1:0:0:P:A:-:0:3,0,20,0:3,0,20,0:A33a,B54e");

    // B76e at (6,5) is dist == 3 from the sole empty allied base (6,2), while A33a/A53a hold (2,2)/(4,2)
    octi::OctiBoard oneBaseMid;
    oneBaseMid.fromPositionString(
        "2:1:0:0:P:A:-:0:2,0,20,0:3,0,20,0:A33a,A53a,B76e");

    octi::OctiBoard ofkBoard;
    ofkBoard.fromPositionString(
        "OFK:1:0:0:P:A:-:0:0,0,10,0:0,0,10,0:A22a,B24e,B36,B46");

    const auto bdThreeBase = octi::OctiBotEngine::evaluatePositionBreakdown(
        threeNeeded2P, octi::PLAYER_A, base);
    const auto bdTwoBase = octi::OctiBotEngine::evaluatePositionBreakdown(
        twoNeeded2P, octi::PLAYER_A, base);
    const int base1BClose = octi::OctiBotEngine::evaluatePosition(oneBaseClose, octi::PLAYER_A, base);
    const int baseOfk = octi::OctiBotEngine::evaluatePosition(ofkBoard, octi::PLAYER_A, base);

    // multiBaseThreeNeededThreatScalePct affects 3-needed, not 1B or OFK
    {
        octi::BotTuningProfile p = base;
        p.multiBaseThreeNeededThreatScalePct = 80;
        const auto bdMod = octi::OctiBotEngine::evaluatePositionBreakdown(
            threeNeeded2P, octi::PLAYER_A, p);
        CHECK_TRUE(bdMod.oneStepBaseThreat > bdThreeBase.oneStepBaseThreat);
        CHECK_EQUAL(base1BClose,
                    octi::OctiBotEngine::evaluatePosition(oneBaseClose, octi::PLAYER_A, p));
        CHECK_EQUAL(baseOfk,
                    octi::OctiBotEngine::evaluatePosition(ofkBoard, octi::PLAYER_A, p));
    }

    // multiBaseTwoNeededThreatScalePct & multiBaseTwoNeededScaleFloorPct affect 2-needed, not 1B or OFK
    {
        octi::BotTuningProfile p = base;
        p.multiBaseTwoNeededThreatScalePct = 50;
        const auto bdMod = octi::OctiBotEngine::evaluatePositionBreakdown(
            twoNeeded2P, octi::PLAYER_A, p);
        CHECK_TRUE(bdMod.oneStepBaseThreat > bdTwoBase.oneStepBaseThreat);
        CHECK_EQUAL(bdTwoBase.capturedEnemyBases, bdMod.capturedEnemyBases);
        CHECK_EQUAL(base1BClose,
                    octi::OctiBotEngine::evaluatePosition(oneBaseClose, octi::PLAYER_A, p));
        CHECK_EQUAL(baseOfk,
                    octi::OctiBotEngine::evaluatePosition(ofkBoard, octi::PLAYER_A, p));
    }

    {
        octi::BotTuningProfile p = base;
        p.multiBaseTwoNeededScaleFloorPct = 90;
        p.multiBaseThreeNeededScaleFloorPct = 70;
        const auto bdMod = octi::OctiBotEngine::evaluatePositionBreakdown(
            twoNeeded2P, octi::PLAYER_A, p);
        CHECK_EQUAL((base.capturedEnemyBaseBonus * 90) / 100 -
                    (base.capturedEnemyBaseBonus * 60) / 100,
                    bdMod.capturedEnemyBases - bdTwoBase.capturedEnemyBases);
        CHECK_EQUAL(base1BClose,
                    octi::OctiBotEngine::evaluatePosition(oneBaseClose, octi::PLAYER_A, p));
        CHECK_EQUAL(baseOfk,
                    octi::OctiBotEngine::evaluatePosition(ofkBoard, octi::PLAYER_A, p));
    }

    // emptyAlliedBase1B{Close,Mid}UrgencyPct affect 1-base needed when penalty > 0, not 3B (0 captured) or OFK
    {
        octi::BotTuningProfile penBase = base;
        penBase.emptyAlliedBasePenalty1B = 1000;
        const auto closeBaseBd = octi::OctiBotEngine::evaluatePositionBreakdown(
            oneBaseClose, octi::PLAYER_A, penBase);
        const auto midBaseBd = octi::OctiBotEngine::evaluatePositionBreakdown(
            oneBaseMid, octi::PLAYER_A, penBase);
        const int pen3B = octi::OctiBotEngine::evaluatePosition(
            threeNeeded2P, octi::PLAYER_A, penBase);
        const int penOfk = octi::OctiBotEngine::evaluatePosition(
            ofkBoard, octi::PLAYER_A, penBase);

        octi::BotTuningProfile pClose = penBase;
        pClose.emptyAlliedBase1BCloseUrgencyPct = 250;
        const auto closeModBd = octi::OctiBotEngine::evaluatePositionBreakdown(
            oneBaseClose, octi::PLAYER_A, pClose);
        CHECK_EQUAL(-1500, closeModBd.lostAlliedBases - closeBaseBd.lostAlliedBases);
        CHECK_EQUAL(pen3B,
                    octi::OctiBotEngine::evaluatePosition(threeNeeded2P, octi::PLAYER_A, pClose));
        CHECK_EQUAL(penOfk,
                    octi::OctiBotEngine::evaluatePosition(ofkBoard, octi::PLAYER_A, pClose));

        octi::BotTuningProfile pMid = penBase;
        pMid.emptyAlliedBase1BMidUrgencyPct = 210;
        const auto midModBd = octi::OctiBotEngine::evaluatePositionBreakdown(
            oneBaseMid, octi::PLAYER_A, pMid);
        CHECK_EQUAL(-800, midModBd.lostAlliedBases - midBaseBd.lostAlliedBases);
        CHECK_EQUAL(pen3B,
                    octi::OctiBotEngine::evaluatePosition(threeNeeded2P, octi::PLAYER_A, pMid));
        CHECK_EQUAL(penOfk,
                    octi::OctiBotEngine::evaluatePosition(ofkBoard, octi::PLAYER_A, pMid));
    }
}

TEST_GROUP(OctiRulebookTest)
{
};

TEST(OctiRulebookTest, EmbeddedPngAssetsSectionsAndDesktopHtml)
{
    const std::vector<std::string> files = octi::rulebookImageFileNames();
    LONGS_EQUAL(static_cast<long>(octi::OCTI_RULEBOOK_IMAGE_COUNT),
                static_cast<long>(files.size()));

    static const std::uint8_t kPngMagic[8] = {
        0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au
    };

    std::size_t totalBytes = 0u;
    for (const std::string &fn : files) {
        std::size_t sz = 0u;
        const std::uint8_t *bytes = octi::rulebookImageBytes(fn, sz);
        CHECK_TRUE(bytes != nullptr);
        CHECK_TRUE(sz >= 1024u);
        CHECK_TRUE(sz <= 80u * 1024u);
        CHECK_EQUAL(0, std::memcmp(bytes, kPngMagic, sizeof(kPngMagic)));
        totalBytes += sz;
    }
    CHECK_TRUE(totalBytes < 1024u * 1024u);

    std::size_t badSz = 42u;
    CHECK_TRUE(octi::rulebookImageBytes("../etc/passwd", badSz) == nullptr);
    LONGS_EQUAL(0, static_cast<long>(badSz));
    CHECK_TRUE(octi::rulebookImageBytes("unknown.png", badSz) == nullptr);

    const std::vector<octi::RulebookSection> &sections = octi::rulebookSections();
    LONGS_EQUAL(static_cast<long>(octi::OCTI_RULEBOOK_CHAPTER_COUNT),
                static_cast<long>(sections.size()));
    for (const auto &sec : sections) {
        CHECK_TRUE(std::string(sec.title).find("Revival") == std::string::npos);
        CHECK_TRUE(std::string(sec.summary).find("revival") == std::string::npos);
        CHECK_TRUE(std::string(sec.summary).find("reposition") == std::string::npos);
    }

    const std::string darkHtml = octi::buildDesktopRulebookHtml(false);
    const std::string lightHtml = octi::buildDesktopRulebookHtml(true);
    CHECK_TRUE(darkHtml != lightHtml);
    CHECK_TRUE(darkHtml.find("<table") != std::string::npos);
    CHECK_TRUE(darkHtml.find("display:grid") == std::string::npos);
    CHECK_TRUE(darkHtml.find("display:flex") == std::string::npos);
    for (const std::string &fn : files) {
        CHECK_TRUE(darkHtml.find("octi://rulebook/" + fn) != std::string::npos);
        CHECK_TRUE(lightHtml.find("octi://rulebook/" + fn) != std::string::npos);
    }
    for (const std::string &vCode : octi::allOctiVariantCodes()) {
        CHECK_TRUE(darkHtml.find(vCode) != std::string::npos);
    }

    // Verify R1-R6 revisions in embedded Desktop rulebook HTML (dark & light)
    for (const std::string *htmlPtr : { &darkHtml, &lightHtml }) {
        const std::string &html = *htmlPtr;
        CHECK_TRUE(html.find("4. Victory &amp; 4P Elimination") != std::string::npos);
        CHECK_TRUE(html.find("44a-45,44beg-66x-64x-44") != std::string::npos);
        CHECK_TRUE(html.find("44bcd-66-86-16-34-54") != std::string::npos);
        CHECK_TRUE(html.find("whole-stack capture") != std::string::npos);
        CHECK_TRUE(html.find("2P_3B*") != std::string::npos);
        CHECK_TRUE(html.find("1 bonus prelude move per SuperProng") != std::string::npos);
        CHECK_TRUE(html.find("permanently eliminated") != std::string::npos);
        CHECK_TRUE(html.find("capturing enemy player") != std::string::npos);

        CHECK_TRUE(html.find("33b+a-b") == std::string::npos);
        CHECK_TRUE(html.find("Reposition a Normal Prong") == std::string::npos);
        CHECK_TRUE(html.find("Teammate Revival") == std::string::npos);
        CHECK_TRUE(html.find("revive eliminated 4P partner") == std::string::npos);
        CHECK_TRUE(html.find("4. Victory &amp; 4P Revival") == std::string::npos);
    }
}

TEST(OctiRulebookTest, SupportedLanguagesNormalizationFallbackAndLocalizedHtml)
{
    const std::vector<std::string> &langs = octi::rulebookSupportedLanguages();
    LONGS_EQUAL(16, static_cast<long>(langs.size()));
    STRCMP_EQUAL("en", langs[0].c_str());
    STRCMP_EQUAL("zh-TW", langs[1].c_str());
    STRCMP_EQUAL("zh-CN", langs[2].c_str());
    STRCMP_EQUAL("fr", langs[3].c_str());
    STRCMP_EQUAL("ro", langs[4].c_str());
    STRCMP_EQUAL("ru", langs[5].c_str());
    STRCMP_EQUAL("es", langs[6].c_str());
    STRCMP_EQUAL("de", langs[7].c_str());
    STRCMP_EQUAL("ja", langs[8].c_str());
    STRCMP_EQUAL("tr", langs[9].c_str());
    STRCMP_EQUAL("pt-BR", langs[10].c_str());
    STRCMP_EQUAL("ko", langs[11].c_str());
    STRCMP_EQUAL("pl", langs[12].c_str());
    STRCMP_EQUAL("it", langs[13].c_str());
    STRCMP_EQUAL("id", langs[14].c_str());
    STRCMP_EQUAL("vi", langs[15].c_str());

    /* Normalization of case/underscore variants and legacy Indonesian 'in' */
    STRCMP_EQUAL("zh-TW", octi::normalizeRulebookLanguage("zh_tw").c_str());
    STRCMP_EQUAL("zh-TW", octi::normalizeRulebookLanguage("ZH-TW").c_str());
    STRCMP_EQUAL("zh-CN", octi::normalizeRulebookLanguage("zh_CN").c_str());
    STRCMP_EQUAL("pt-BR", octi::normalizeRulebookLanguage("pt").c_str());
    STRCMP_EQUAL("pt-BR", octi::normalizeRulebookLanguage("pt_br").c_str());
    STRCMP_EQUAL("pt-BR", octi::normalizeRulebookLanguage("PT_BR").c_str());
    STRCMP_EQUAL("pt-BR", octi::normalizeRulebookLanguage("pt-PT").c_str());
    STRCMP_EQUAL("pt-BR", octi::normalizeRulebookLanguage("pt_PT").c_str());
    STRCMP_EQUAL("pt-BR", octi::normalizeRulebookLanguage("pt-AO").c_str());
    STRCMP_EQUAL("id", octi::normalizeRulebookLanguage("in").c_str());
    STRCMP_EQUAL("id", octi::normalizeRulebookLanguage("IN").c_str());
    STRCMP_EQUAL("id", octi::normalizeRulebookLanguage("id").c_str());

    /* Fallback to 'en' on empty, unknown, or path-traversal inputs */
    for (const char *bad : { "", "   ", "xx", "ar", "zh", "pt-12", "pt-pt-extra", "pt-../", "../en", "en/../../etc", "fr-CA" }) {
        STRCMP_EQUAL("en", octi::normalizeRulebookLanguage(bad).c_str());
        CHECK_TRUE(&octi::rulebookSections(bad) == &octi::rulebookSections());
        CHECK_TRUE(octi::buildDesktopRulebookHtml(false, bad) ==
                   octi::buildDesktopRulebookHtml(false));
        CHECK_TRUE(octi::buildDesktopRulebookHtml(true, bad) ==
                   octi::buildDesktopRulebookHtml(true));
    }

    const std::vector<std::string> files = octi::rulebookImageFileNames();
    const std::string enDark = octi::buildDesktopRulebookHtml(false, "en");
    const std::string enLight = octi::buildDesktopRulebookHtml(true, "en");
    CHECK_TRUE(enDark == octi::buildDesktopRulebookHtml(false));
    CHECK_TRUE(enLight == octi::buildDesktopRulebookHtml(true));

    for (const std::string &code : langs) {
        STRCMP_EQUAL(code.c_str(), octi::normalizeRulebookLanguage(code).c_str());
        const auto &secs = octi::rulebookSections(code);
        LONGS_EQUAL(static_cast<long>(octi::OCTI_RULEBOOK_CHAPTER_COUNT),
                    static_cast<long>(secs.size()));
        for (std::size_t i = 0; i < secs.size(); ++i) {
            std::string expectedId = "ch" + std::to_string(i + 1);
            STRCMP_EQUAL(expectedId.c_str(), secs[i].id);
            CHECK_TRUE(secs[i].number != nullptr && std::strlen(secs[i].number) > 0u);
            CHECK_TRUE(secs[i].title != nullptr && std::strlen(secs[i].title) > 0u);
            CHECK_TRUE(secs[i].summary != nullptr && std::strlen(secs[i].summary) > 0u);
        }

        const std::string darkHtml = octi::buildDesktopRulebookHtml(false, code);
        const std::string lightHtml = octi::buildDesktopRulebookHtml(true, code);
        CHECK_TRUE(darkHtml != lightHtml);
        CHECK_TRUE(darkHtml.find("<table") != std::string::npos);
        CHECK_TRUE(darkHtml.find("display:grid") == std::string::npos);
        CHECK_TRUE(darkHtml.find("display:flex") == std::string::npos);
        CHECK_TRUE(darkHtml.find("Pod") != std::string::npos);
        CHECK_TRUE(darkHtml.find("Prong") != std::string::npos);
        CHECK_TRUE(darkHtml.find("SuperProng") != std::string::npos);
        CHECK_TRUE(darkHtml.find("Elo") != std::string::npos);
        CHECK_TRUE(darkHtml.find("44a-45,44beg-66x-64x-44") != std::string::npos);
        CHECK_TRUE(darkHtml.find("44bcd-66-86-16-34-54") != std::string::npos);

        for (int ch = 1; ch <= 7; ++ch) {
            CHECK_TRUE(darkHtml.find("<a name=\"ch" + std::to_string(ch) + "\"></a>") !=
                       std::string::npos);
        }
        for (const std::string &fn : files) {
            CHECK_TRUE(darkHtml.find("octi://rulebook/" + fn) != std::string::npos);
            CHECK_TRUE(lightHtml.find("octi://rulebook/" + fn) != std::string::npos);
        }
        for (const std::string &vCode : octi::allOctiVariantCodes()) {
            CHECK_TRUE(darkHtml.find(vCode) != std::string::npos);
        }
        if (code != "en") {
            CHECK_TRUE(darkHtml != enDark);
            CHECK_TRUE(darkHtml.find("<html lang=\"" + code + "\">") !=
                       std::string::npos);
            CHECK_TRUE(darkHtml.find("toPositionString()") != std::string::npos);
            CHECK_TRUE(darkHtml.find("elimMask") != std::string::npos);
            CHECK_TRUE(darkHtml.find("<td>1-step</td>") == std::string::npos);
            CHECK_TRUE(darkHtml.find("<td>Composite</td>") == std::string::npos);
            CHECK_TRUE(darkHtml.find("(4. Victory &amp; 4P Elimination)") ==
                       std::string::npos);
            CHECK_TRUE(std::string(secs[0].title) !=
                       std::string(octi::rulebookSections("en")[0].title));
        }
    }

    /* Regional Chinese style separation: zh-TW vs zh-CN */
    const std::string twHtml = octi::buildDesktopRulebookHtml(false, "zh-TW");
    const std::string cnHtml = octi::buildDesktopRulebookHtml(false, "zh-CN");
    CHECK_TRUE(twHtml != cnHtml);
    CHECK_TRUE(twHtml.find("棋盤") != std::string::npos);
    CHECK_TRUE(twHtml.find("對局") != std::string::npos);
    CHECK_TRUE(twHtml.find("設定") != std::string::npos);
    for (const char *badTw : { "设置", "服务器", "连接", "默认", "保存", "加载", "对局", "棋盘" }) {
        CHECK_TRUE_TEXT(twHtml.find(badTw) == std::string::npos, badTw);
    }
    CHECK_TRUE(cnHtml.find("棋盘") != std::string::npos);
    CHECK_TRUE(cnHtml.find("对局") != std::string::npos);
    CHECK_TRUE(cnHtml.find("设置") != std::string::npos);
    for (const char *badCn : { "設定", "帳號", "伺服器", "連線", "預設", "儲存", "載入", "對局", "棋盤" }) {
        CHECK_TRUE_TEXT(cnHtml.find(badCn) == std::string::npos, badCn);
    }
}

namespace {

bool isSingleOpRepositionMove(const octi::OctiMove &m)
{
    const auto &ops = m.getOps();
    return ops.size() == 1 &&
           ops[0].hops.empty() &&
           !ops[0].addPod &&
           ops[0].addProngDir != octi::DIR_UNDEF &&
           ops[0].rmProngDir != octi::DIR_UNDEF;
}

bool moveHasAnyRepositionOp(const octi::OctiMove &m)
{
    for (const auto &op : m.getOps()) {
        if (op.addProngDir != octi::DIR_UNDEF && op.rmProngDir != octi::DIR_UNDEF) {
            return true;
        }
    }
    return false;
}

class TestSubclassHookEngine : public octi::OctiBotEngine
{
public:
    using octi::OctiBotEngine::OctiBotEngine;

    mutable std::atomic<int> searchMovesCalls{0};
    mutable std::atomic<int> extraQuiesceCalls{0};
    mutable std::atomic<int> moveOrderCalls{0};
    mutable std::atomic<int> moveOrderInQuiesceCalls{0};
    mutable std::atomic<int> evalBreakdownCalls{0};
    mutable std::atomic<int> lmrCalls{0};
    int customMarker = 777;

protected:
    void generateSearchMoves(const octi::OctiGame &game,
                             octi::OctiPlayer turn,
                             int ply,
                             std::vector<octi::OctiMove> &outMoves) const override
    {
        searchMovesCalls.fetch_add(1, std::memory_order_relaxed);
        octi::OctiBotEngine::generateSearchMoves(game, turn, ply, outMoves);
    }

    void appendExtraQuiesceMoves(const octi::OctiBoard &board,
                                 const octi::OctiGame &game,
                                 octi::OctiGameMode mode,
                                 octi::OctiPlayer turn,
                                 int qDepth,
                                 std::vector<octi::OctiMove> &tacticalMoves) const override
    {
        extraQuiesceCalls.fetch_add(1, std::memory_order_relaxed);
        octi::OctiBotEngine::appendExtraQuiesceMoves(
            board, game, mode, turn, qDepth, tacticalMoves);
    }

    bool hasCustomMoveOrdering() const noexcept override
    {
        return true;
    }

    void adjustMoveOrderScores(
        const octi::OctiBoard &board,
        const octi::OctiGame &game,
        octi::OctiPlayer turn,
        int ply,
        bool inQuiesce,
        const std::vector<octi::OctiMove> &moves,
        std::vector<std::pair<int, std::size_t>> &scored) const override
    {
        (void)board;
        (void)game;
        (void)turn;
        (void)ply;
        (void)moves;
        moveOrderCalls.fetch_add(1, std::memory_order_relaxed);
        if (inQuiesce) {
            moveOrderInQuiesceCalls.fetch_add(1, std::memory_order_relaxed);
        }
        if (!scored.empty()) {
            scored[0].first += 1;
        }
    }

    void adjustEvalBreakdown(const octi::OctiBoard &board,
                             octi::OctiPlayer perspective,
                             const EvalPhaseSummary &summary,
                             octi::OctiEvalBreakdown &bd) const override
    {
        (void)board;
        (void)perspective;
        evalBreakdownCalls.fetch_add(1, std::memory_order_relaxed);
        if (summary.allyPhase.basesNeeded >= 1) {
            bd.structuralPositional += customMarker - 777;
        }
    }

    int adjustLmrReduction(const octi::OctiBoard &board,
                           octi::OctiGameMode mode,
                           octi::OctiPlayer turn,
                           int depth,
                           int ply,
                           std::size_t mvIdx,
                           const octi::OctiMove &mv,
                           bool mvIsTactical,
                           bool mvIsKiller,
                           int defaultReduction) const override
    {
        (void)board;
        (void)mode;
        (void)turn;
        (void)depth;
        (void)ply;
        (void)mvIdx;
        (void)mv;
        (void)mvIsTactical;
        (void)mvIsKiller;
        lmrCalls.fetch_add(1, std::memory_order_relaxed);
        return defaultReduction;
    }
};

} // namespace

TEST_GROUP(OctiSearchMoveGenAndSubclassHooksTest)
{
};

TEST(OctiSearchMoveGenAndSubclassHooksTest, SearchMoveGenSkipsDominatedRepositionsWhenFreeProngsAvail)
{
    octi::OctiBoard board;
    board.fromPositionString(
        "2:3:0:1:P:A:-:0:1,0,16,0:1,0,15,0:"
        "A33abc,A53de,A73fg,B37efg,B57cd,B77ab");
    const octi::OctiGame *game = board.getGame();
    CHECK_TRUE(game != nullptr);

    // R1: generateLegalMoves() emits zero reposition moves; generateLegalMovesForSearch()
    // keeps defaulted no-op boolean flags for OctiCcBotEngine signature compatibility.
    std::vector<octi::OctiMove> fullMoves = game->generateLegalMoves();
    std::vector<octi::OctiMove> searchNoSingleRepo;
    game->generateLegalMovesForSearch(searchNoSingleRepo, false, true);
    std::vector<octi::OctiMove> searchWithSingleRepo;
    game->generateLegalMovesForSearch(searchWithSingleRepo, true, true);

    CHECK_EQUAL(fullMoves.size(), searchNoSingleRepo.size());
    CHECK_EQUAL(fullMoves.size(), searchWithSingleRepo.size());
    for (std::size_t i = 0; i < fullMoves.size(); ++i) {
        CHECK_FALSE(isSingleOpRepositionMove(fullMoves[i]));
        CHECK_FALSE(moveHasAnyRepositionOp(fullMoves[i]));
        CHECK_EQUAL(fullMoves[i].asString(), searchNoSingleRepo[i].asString());
        CHECK_EQUAL(fullMoves[i].asString(), searchWithSingleRepo[i].asString());
    }

    octi::OctiBoard spBoard;
    spBoard.fromPositionString(
        "2:3:1:0:P:A:-:0:2,0,12,1:2,0,12,1:"
        "A33a!b,A43a,A53,B37e,B57f,B77g");
    const octi::OctiGame *spGame = spBoard.getGame();
    CHECK_TRUE(spGame != nullptr);

    std::vector<octi::OctiMove> spFull = spGame->generateLegalMoves();
    std::vector<octi::OctiMove> spNoSingle;
    spGame->generateLegalMovesForSearch(spNoSingle, false, true);
    std::vector<octi::OctiMove> spNoRepoAtAll;
    spGame->generateLegalMovesForSearch(spNoRepoAtAll, false, false);

    CHECK_EQUAL(spFull.size(), spNoSingle.size());
    CHECK_EQUAL(spFull.size(), spNoRepoAtAll.size());
    for (std::size_t i = 0; i < spFull.size(); ++i) {
        CHECK_FALSE(moveHasAnyRepositionOp(spFull[i]));
        CHECK_EQUAL(spFull[i].asString(), spNoSingle[i].asString());
        CHECK_EQUAL(spFull[i].asString(), spNoRepoAtAll[i].asString());
    }
}

TEST(OctiSearchMoveGenAndSubclassHooksTest, LostAlliedBasesDoesNotReceivePositive1BOffenseCredit)
{
    // 1593 ply 58 geometry in 2P_3B_ED:
    // B has captured 2 of A's bases (53, 73), leaving A's 33 empty with B45a at dist 2 (allyPhase.basesNeeded == 1).
    // A has captured B's 37, while B's 57 and 77 are empty (enemyPhase.basesNeeded == 2).
    // With emptyAlliedBasePenalty1B = 4500 and lostAlliedBasePenalty = 24000 (80% floor -> -19200),
    // bdB.lostAlliedBases must be -19200 (<= 0), NEVER -19200 + 7875 = -11325!
    const char *kPly58MirageFen =
        "2:3:0:1:P:B:-:0:0,1,4,0:0,0,5,0:"
        "A37ab,A56cde,B53acde,B73a,B45a,B67abcdfg";
    octi::OctiBoard board;
    board.fromPositionString(kPly58MirageFen);

    octi::BotTuningProfile baseProf;
    baseProf.lostAlliedBasePenalty = 24000;
    baseProf.multiBaseTwoNeededScaleFloorPct = 80;
    baseProf.emptyAlliedBasePenalty1B = 0;

    octi::BotTuningProfile penProf = baseProf;
    penProf.emptyAlliedBasePenalty1B = 4500;
    penProf.emptyAlliedBase1BCloseUrgencyPct = 175;

    octi::OctiEvalBreakdown bdBaseA =
        octi::OctiBotEngine::evaluatePositionBreakdown(board, octi::PLAYER_A, baseProf);
    octi::OctiEvalBreakdown bdBaseB =
        octi::OctiBotEngine::evaluatePositionBreakdown(board, octi::PLAYER_B, baseProf);
    octi::OctiEvalBreakdown bdPenB =
        octi::OctiBotEngine::evaluatePositionBreakdown(board, octi::PLAYER_B, penProf);
    octi::OctiEvalBreakdown bdPenA =
        octi::OctiBotEngine::evaluatePositionBreakdown(board, octi::PLAYER_A, penProf);

    CHECK_EQUAL(-19200, bdBaseB.lostAlliedBases);
    CHECK_EQUAL(-19200, bdPenB.lostAlliedBases);
    CHECK_TRUE(bdPenB.lostAlliedBases <= 0);
    CHECK_EQUAL(bdBaseB.twoPlyBaseThreat + 7875, bdPenB.twoPlyBaseThreat);
    CHECK_EQUAL(bdBaseA.lostAlliedBases - 7875, bdPenA.lostAlliedBases);
    CHECK_EQUAL(bdBaseB.totalScore + 7875, bdPenB.totalScore);
    CHECK_EQUAL(bdBaseA.totalScore - 7875, bdPenA.totalScore);
}

TEST(OctiSearchMoveGenAndSubclassHooksTest, OctiBotEngineSubclassHooksAreNonSlicingAndPreservedAcrossSearch)
{
    TestSubclassHookEngine subEngine(octi::BotDifficulty::TACTICAL, 0x5EED2026ULL);
    subEngine.customMarker = 900;

    octi::BotTuningProfile prof =
        octi::OctiBotEngine::defaultProfileForDifficulty(octi::BotDifficulty::TACTICAL);
    prof.useOpeningBook = false;
    prof.lazySmpThreads = 1;
    prof.timeBudgetMs = 5000;
    prof.maxSearchDepth = 4;
    prof.rootBranchCap = 6;
    prof.treeBranchCap = 8;
    prof.maxQuiesceDepth = 2;
    prof.lmrMinDepth = 3;
    prof.lmrMinMoveIndex = 1;
    prof.qsearchIncludeBaseThreatProngs = true;
    prof.rootScoreJitter = 0;
    subEngine.setTuningProfile(prof);

    // Call resetSearchState through a base OctiBotEngine& reference (as polymorphic callers do)
    // and verify dynamic type, customMarker, and tuning profile are preserved without slicing.
    octi::OctiBotEngine &baseRef = subEngine;
    baseRef.resetSearchState(0x12345678ULL);
    CHECK_TRUE(dynamic_cast<TestSubclassHookEngine *>(&baseRef) != nullptr);
    CHECK_EQUAL(900, subEngine.customMarker);
    CHECK_EQUAL(4, baseRef.tuningProfile().maxSearchDepth);

    octi::OctiBoard board;
    board.fromPositionString(
        "2:1:0:0:P:A:-:0:2,0,14,0:2,0,14,0:"
        "A33abc,A54bc,B45efg,B57ef,B77g");

    octi::OctiEvalBreakdown bdInst = baseRef.evaluateBoardBreakdown(board, octi::PLAYER_A);
    octi::OctiEvalBreakdown bdStat =
        octi::OctiBotEngine::evaluatePositionBreakdown(board, octi::PLAYER_A, prof);
    CHECK_EQUAL(bdStat.structuralPositional + 123, bdInst.structuralPositional);
    CHECK_EQUAL(bdInst.sumTerms(), bdInst.totalScore);
    CHECK_EQUAL(bdInst.totalScore, baseRef.evaluateBoard(board, octi::PLAYER_A));

    subEngine.evalBreakdownCalls.store(0, std::memory_order_relaxed);
    octi::BotSearchStats stats;
    std::string bestMove = baseRef.chooseMove(board, &stats);
    CHECK_FALSE(bestMove.empty());
    CHECK_TRUE(subEngine.searchMovesCalls.load(std::memory_order_relaxed) > 0);
    CHECK_TRUE(subEngine.extraQuiesceCalls.load(std::memory_order_relaxed) > 0);
    CHECK_TRUE(subEngine.moveOrderCalls.load(std::memory_order_relaxed) > 0);
    CHECK_TRUE(subEngine.moveOrderInQuiesceCalls.load(std::memory_order_relaxed) > 0);
    CHECK_TRUE(subEngine.evalBreakdownCalls.load(std::memory_order_relaxed) > 0);
    CHECK_TRUE(subEngine.lmrCalls.load(std::memory_order_relaxed) > 0);

    // Verify resetSearchState clears TT entries and live search state
    octi::OctiBoard childBoard;
    childBoard.fromPositionString(board.toPositionString());
    childBoard.getGame()->move(octi::OctiMove(bestMove));
    CHECK_TRUE(baseRef.lookupTranspositionBestMoveKey(childBoard) != 0ULL);
    CHECK_TRUE(baseRef.liveDepthReached() > 0);
    baseRef.resetSearchState(0x5EED2026ULL);
    CHECK_EQUAL(0ULL, baseRef.lookupTranspositionBestMoveKey(childBoard));
    CHECK_EQUAL(0, baseRef.liveDepthReached());
}

// ===========================================================================
// Don Green Rulebook & Engine Revisions Qualification Suite (R1, R4, R5, R6)
// ===========================================================================

TEST_GROUP(OctiDonGreenRuleRevisionsTest)
{
};

TEST(OctiDonGreenRuleRevisionsTest, R1_NoRepositionGeneratedAcrossAllVariantsAndBoxedInArmedPodStalemate)
{
    // 1. Across all 13 variants (including 0-reserve-prong states where bots formerly used +add-rm),
    //    generateLegalMoves() and generateLegalMovesForSearch() emit zero reposition moves.
    const char *const kTestFens[] = {
        "2:3:0:0:P:A:-:0:0,0,0,0:0,0,0,0:A33abc,B57efg",
        "2:1:0:0:P:A:-:0:0,0,0,0:0,0,0,0:A33abc,B57efg",
        "2:3:1:0:P:A:-:0:0,0,0,0:0,0,0,0:A33a!bc,B57efg",
        "2:3:0:1:P:A:-:0:0,0,0,0:0,0,0,0:A33abc,B57efg",
        "2:3:1:1:P:A:-:0:0,0,0,0:0,0,0,0:A33a!bc,B57efg",
        "4:3:0:0:P:A:-:0:0,0,0,0:0,0,0,0:0,0,0,0:0,0,0,0:A33abc,B37ef,C77ef,D73ab",
        "4:3:1:1:P:A:-:0:0,0,0,0:0,0,0,0:0,0,0,0:0,0,0,0:A33a!bc,B37ef,C77ef,D73ab",
        "OFK:1:0:0:P:A:-:0:0,0,0,0:0,0,0,0:A22abc,B56efg"
    };

    for (const char *fen : kTestFens) {
        octi::OctiBoard b;
        b.fromPositionString(fen);
        const octi::OctiGame *g = b.getGame();
        std::vector<octi::OctiMove> legal = g->generateLegalMoves();
        std::vector<octi::OctiMove> searchMoves;
        g->generateLegalMovesForSearch(searchMoves, true, true);
        CHECK_EQUAL(legal.size(), searchMoves.size());
        for (const auto &m : legal) {
            CHECK_FALSE(moveHasAnyRepositionOp(m));
            const std::string s = m.asString();
            if (s.find('+') != std::string::npos) {
                CHECK_EQUAL(std::string::npos, s.find('-', s.find('+')));
            }
        }
    }

    // 2. Boxed-in armed pod with < 8 normal prongs and 0 reserve pods/prongs:
    //    A11c at (0,0) has 1 normal prong 'c' (East), blocked from stepping to 21 by B21
    //    and blocked from jumping to 31 by B31. Must have hasAnyLegalMove() == false -> STALEMATE!
    {
        octi::OctiBoard bStale(octi::GAME_MODE_2);
        bStale.fromPositionString("2:3:0:0:P:A:-:0:0,0,0,0:0,0,2,0:A11c,B21,B31,B57e");
        CHECK_FALSE(bStale.getGame()->hasAnyLegalMove());
        CHECK_TRUE(bStale.getGame()->generateLegalMoves().empty());

        // B moves 57e-56 -> A is stalemated immediately and B wins with STALEMATE termination
        octi::OctiBoard bTrig(octi::GAME_MODE_2);
        bTrig.fromPositionString("2:3:0:0:P:B:-:0:0,0,0,0:0,0,2,0:A11c,B21,B31,B57e");
        bTrig.getGame()->move("57e-56");
        CHECK_TRUE(bTrig.getGame()->isGameOver());
        CHECK_EQUAL(octi::PLAYER_B, bTrig.getGame()->getWinner());
        auto root = nlohmann::json::parse(bTrig.serializeJson());
        STRCMP_EQUAL("STALEMATE", root["termination"].get<std::string>().c_str());
    }

    // 3. 4P boxed-in armed pod seat-skip: B11c (1 normal prong, 0 reserves, blocked by A21 & A31)
    //    is NOT eliminated, has 0 legal moves, and is skipped cleanly from A -> C!
    {
        octi::OctiBoard b4Skip(octi::GAME_MODE_4);
        b4Skip.fromPositionString(
            "4:3:0:0:P:A:-:0:0,0,2,0:0,0,0,0:0,0,2,0:0,0,2,0:A21,A31,A33a,B11c,C77e,D73g");
        octi::OctiBoard b4CheckB(octi::GAME_MODE_4);
        b4CheckB.fromPositionString(
            "4:3:0:0:P:B:-:0:0,0,2,0:0,0,0,0:0,0,2,0:0,0,2,0:A21,A31,A34a,B11c,C77e,D73g");
        CHECK_FALSE(b4CheckB.getGame()->hasAnyLegalMove());
        CHECK_TRUE(b4CheckB.getGame()->generateLegalMoves().empty());

        b4Skip.getGame()->move("33a-34");
        CHECK_FALSE(b4Skip.getGame()->isGameOver());
        CHECK_FALSE(b4Skip.getGame()->isPlayerEliminated(octi::PLAYER_B));
        CHECK_EQUAL(octi::PLAYER_C, b4Skip.getGame()->getTurn());
    }
}

TEST(OctiDonGreenRuleRevisionsTest, R4_StartingSquareVacancyLoopBackAndEdgelessStackedPodHopOver)
{
    // 1. Stationary A44a on 44 blocks single-pod loop-back 44beg-66x-64x-44,
    //    whereas composite clear-out 44a-45,44beg-66x-64x-44 succeeds!
    {
        octi::OctiBoard board(octi::GAME_MODE_2);
        board.fromPositionString(
            "2:3:0:0:P:A:-:0:0,0,2,0:0,0,2,0:A44a,A44beg,B54,B55,B65,B37e");
        const std::string initialFen = board.toPositionString();
        octi::OctiGame *game = board.getGame();

        const std::vector<octi::OctiMove> legal = game->generateLegalMoves();
        const std::vector<octi::OctiMove> tactical = game->generateTacticalMoves();
        CHECK_FALSE(containsExactMove(legal, "44beg-66x-64x-44"));
        CHECK_FALSE(containsExactMove(legal, "44beg-66-64-44"));
        CHECK_FALSE(containsExactMove(tactical, "44beg-66x-64x-44"));
        CHECK_TRUE(containsExactMove(legal, "44beg-66x-64x"));
        CHECK_TRUE(containsExactMove(tactical, "44beg-66x-64x"));

        CHECK_THROWS(octi::IllegalMoveException, game->move("44beg-66x-64x-44"));
        CHECK_THROWS(octi::IllegalMoveException, game->move("44beg-66-64-44"));
        STRCMP_EQUAL(initialFen.c_str(), board.toPositionString().c_str());

        // Composite clear-out: A44a steps to 45 so 44 is physically vacated for 44beg-66x-64x-44
        game->move("44a-45,44beg-66x-64x-44");
        CHECK_EQUAL(1, board.squareAt(3, 3).getNumPods()); // 44 holds A44beg
        STRCMP_EQUAL("beg", board.squareAt(3, 3).getPodAt(0).getConfigString().c_str());
        CHECK_EQUAL(1, board.squareAt(3, 4).getNumPods()); // 45 holds A45a
        STRCMP_EQUAL("a", board.squareAt(3, 4).getPodAt(0).getConfigString().c_str());
        CHECK_TRUE(board.squareAt(4, 4).isEmpty()); // B55 captured
        CHECK_TRUE(board.squareAt(5, 4).isEmpty()); // B65 captured
        CHECK_EQUAL(1, board.squareAt(4, 3).getNumPods()); // B54 jumped without x
        CHECK_EQUAL(2, game->getCapturedPods(octi::PLAYER_B));

        game->undo();
        STRCMP_EQUAL(initialFen.c_str(), board.toPositionString().c_str());
    }

    // 2. Partial clear-out of a 3-pod stack (A44c remains stationary on 44) still rejects loop-back
    {
        octi::OctiBoard board3(octi::GAME_MODE_2);
        board3.fromPositionString(
            "2:3:0:0:P:A:-:0:0,0,2,0:0,0,2,0:A44a,A44c,A44beg,B54,B55,B65,B37e");
        const std::string fenBefore = board3.toPositionString();
        CHECK_THROWS(octi::IllegalMoveException,
                     board3.getGame()->move("44a-45,44beg-66x-64x-44"));
        STRCMP_EQUAL(fenBefore.c_str(), board3.toPositionString().c_str());
    }

    // 3. Lone pod on 44 (no stationary stack-mate) CAN loop back 44beg-66x-64x-44
    {
        octi::OctiBoard boardLone(octi::GAME_MODE_2);
        boardLone.fromPositionString(
            "2:3:0:0:P:A:-:0:0,0,2,0:0,0,2,0:A44beg,B54,B55,B65,B37e");
        const std::vector<octi::OctiMove> legal = boardLone.getGame()->generateLegalMoves();
        const std::vector<octi::OctiMove> tactical = boardLone.getGame()->generateTacticalMoves();
        CHECK_TRUE(containsExactMove(legal, "44beg-66x-64x-44"));
        CHECK_TRUE(containsExactMove(tactical, "44beg-66x-64x-44"));
        boardLone.getGame()->move("44beg-66x-64x-44");
        CHECK_EQUAL(1, boardLone.squareAt(3, 3).getNumPods());
    }

    // 4. Edgeless (_ED) wraparound 44bcd-66-86-16-34-54 over stationary A44a on 44 is ILLEGAL
    //    (Don Green uid:119195: cannot jump pods with which you started stacked)
    {
        octi::OctiBoard edBoard(octi::GAME_MODE_2, false, true, octi::GAME_BASES_3);
        edBoard.fromPositionString(
            "2:3:0:1:P:A:-:0:0,0,2,0:0,0,2,0:A44a,A44bcd,B25,B55,B76,B96,B37e");
        const std::string edFen = edBoard.toPositionString();
        octi::OctiGame *edGame = edBoard.getGame();

        const std::vector<octi::OctiMove> legal = edGame->generateLegalMoves();
        const std::vector<octi::OctiMove> tactical = edGame->generateTacticalMoves();
        // 4-hop prefix stopping at 34 is legal; 5th hop 34->54 over stationary A44a on 44 is illegal!
        CHECK_TRUE(containsExactMove(legal, "44bcd-66-86-16-34"));
        CHECK_FALSE(containsExactMove(legal, "44bcd-66-86-16-34-54"));
        CHECK_FALSE(containsExactMove(legal, "44bcd-66-86-16-34-54x"));
        CHECK_FALSE(containsExactMove(tactical, "44bcd-66-86-16-34-54x"));

        CHECK_THROWS(octi::IllegalMoveException, edGame->move("44bcd-66-86-16-34-54"));
        CHECK_THROWS(octi::IllegalMoveException, edGame->move("44bcd-66-86-16-34-54x"));
        STRCMP_EQUAL(edFen.c_str(), edBoard.toPositionString().c_str());
    }

    // 5. Edgeless SuperProng (_SPED) straight-line 5-hop wraparound 15c!-35-55-75-95-25
    //    over stationary A15a on 15 is ILLEGAL in both SP bonus prelude and regular move
    {
        octi::OctiBoard spedBoard(octi::GAME_MODE_2, true, true, octi::GAME_BASES_3);
        spedBoard.fromPositionString(
            "2:3:1:1:P:A:-:0:0,0,1,0:0,0,1,0:A15a,A15c!,B25,B45,B65,B85,B57e");
        const std::string spedFen = spedBoard.toPositionString();
        octi::OctiGame *spedGame = spedBoard.getGame();

        const std::vector<octi::OctiMove> legal = spedGame->generateLegalMoves();
        const std::vector<octi::OctiMove> tactical = spedGame->generateTacticalMoves();
        CHECK_TRUE(containsExactMove(legal, "15c!-35-55-75-95;95c!+b"));
        CHECK_FALSE(containsMoveWithPrefix(legal, "15c!-35-55-75-95-25"));
        CHECK_FALSE(containsMoveWithPrefix(tactical, "15c!-35-55-75-95-25"));

        CHECK_THROWS(octi::IllegalMoveException,
                     spedGame->move("15c!-35-55-75-95-25;25c!+b"));
        CHECK_THROWS(octi::IllegalMoveException,
                     spedGame->move("15c!-35-55-75-95-25"));
        STRCMP_EQUAL(spedFen.c_str(), spedBoard.toPositionString().c_str());
    }
}

TEST(OctiDonGreenRuleRevisionsTest, R5_OneToFourSuperProngPreludesImmediateCapturesRollbackAndWinTiming)
{
    // 1. 1 controlled SP on board (even with 1 freeSuperProng in reserve) -> max 1 prelude;
    //    2 controlled SPs in 2P_SP -> 0..2 preludes;
    //    3 and 4 controlled SPs in 4P_SP -> 0..3 and 0..4 preludes!
    {
        octi::OctiBoard b1Sp(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        b1Sp.fromPositionString("2:3:1:0:P:A:-:0:0,0,1,1:0,0,1,0:A33a!,A53c,B57e");
        for (const auto &m : b1Sp.getGame()->generateLegalMoves()) {
            CHECK_TRUE(countPreludeOps(m) <= 1);
        }
        CHECK_THROWS(octi::IllegalMoveException,
                     b1Sp.getGame()->move("33a!-34;34a!-35;35a!+b"));

        // 4P_SP with 3 controlled SPs on board -> 0..3 preludes generated & accepted; 4th throws
        octi::OctiBoard b3Sp(octi::GAME_MODE_4, true, false, octi::GAME_BASES_3);
        b3Sp.fromPositionString(
            "4:3:1:0:P:A:-:0:0,0,1,0:0,0,1,0:0,0,1,0:0,0,1,0:"
            "A11a!,A21a!,A41a!,B37e,C77e,D73a");
        std::vector<octi::OctiMove> moves3 = b3Sp.getGame()->generateLegalMoves();
        int maxPrel3 = 0;
        for (const auto &m : moves3) {
            maxPrel3 = std::max(maxPrel3, countPreludeOps(m));
        }
        CHECK_EQUAL(3, maxPrel3);
        CHECK_TRUE(containsExactMove(moves3, "11a!-12;21a!-22;41a!-42;12a!+b"));
        CHECK_THROWS(octi::IllegalMoveException,
                     b3Sp.getGame()->move("11a!-12;21a!-22;41a!-42;42a!-43;12a!+b"));
        b3Sp.getGame()->move("11a!-12;21a!-22;41a!-42;12a!+b");
        CHECK_EQUAL(1, b3Sp.squareAt(0, 1).getNumPods());
        CHECK_EQUAL(1, b3Sp.squareAt(1, 1).getNumPods());
        CHECK_EQUAL(1, b3Sp.squareAt(3, 1).getNumPods());

        // 4P_SP with 4 controlled SPs (both 4 separate pods AND 1 pod with 4 SP sockets) -> 0..4 preludes!
        octi::OctiBoard b4Pods(octi::GAME_MODE_4, true, false, octi::GAME_BASES_3);
        b4Pods.fromPositionString(
            "4:3:1:0:P:A:-:0:0,0,1,0:0,0,1,0:0,0,1,0:0,0,1,0:"
            "A11a!,A21a!,A41a!,A51a!,B37e,C77e,D73a");
        std::vector<octi::OctiMove> moves4Pods = b4Pods.getGame()->generateLegalMoves();
        int maxPrel4Pods = 0;
        for (const auto &m : moves4Pods) {
            maxPrel4Pods = std::max(maxPrel4Pods, countPreludeOps(m));
        }
        CHECK_EQUAL(4, maxPrel4Pods);
        CHECK_TRUE(containsExactMove(moves4Pods, "11a!-12;21a!-22;41a!-42;51a!-52;12a!+b"));
        b4Pods.getGame()->move("11a!-12;21a!-22;41a!-42;51a!-52;12a!+b");
        CHECK_EQUAL(octi::PLAYER_B, b4Pods.getGame()->getTurn());
        b4Pods.getGame()->undo();
        CHECK_EQUAL(octi::PLAYER_A, b4Pods.getGame()->getTurn());

        // Single pod A11a!b!c!d! with 4 SP sockets firing all 4 in sequence, then regular move using a! again!
        octi::OctiBoard b4SinglePod(octi::GAME_MODE_4, true, false, octi::GAME_BASES_3);
        b4SinglePod.fromPositionString(
            "4:3:1:0:P:A:-:0:0,0,1,0:0,0,1,0:0,0,1,0:0,0,1,0:"
            "A11a!b!c!d!,B37e,C77e,D73a");
        std::vector<octi::OctiMove> moves4Single = b4SinglePod.getGame()->generateLegalMoves();
        CHECK_TRUE(containsExactMove(
            moves4Single,
            "11a!b!c!d!-12;12a!b!c!d!-23;23a!b!c!d!-33;33a!b!c!d!-42;42a!b!c!d!+e"));
        // Reusing a! in p1 and p3 on the same pod is rejected:
        CHECK_THROWS(
            octi::IllegalMoveException,
            b4SinglePod.getGame()->move(
                "11a!b!c!d!-12;12a!b!c!d!-23;23a!b!c!d!-24;24a!b!c!d!-34;34a!b!c!d!+e"));
        // Using already-fired SP socket a! in regularOps after 4 SP preludes IS legal:
        b4SinglePod.getGame()->move(
            "11a!b!c!d!-12;12a!b!c!d!-23;23a!b!c!d!-33;33a!b!c!d!-42;42a!b!c!d!-43");
        CHECK_EQUAL(1, b4SinglePod.squareAt(3, 2).getNumPods()); // landed at 43

        // Also exercise prepareHops(..., isSuperProngBonusMove = true) + commitPreparedMove() across multiple SP preludes:
        octi::OctiBoard bPrep(octi::GAME_MODE_4, true, false, octi::GAME_BASES_3);
        bPrep.fromPositionString(
            "4:3:1:0:P:A:-:0:0,0,1,0:0,0,1,0:0,0,1,0:0,0,1,0:"
            "A11a!b!c!d!,B37e,C77e,D73a");
        octi::OctiGame *gPrep = bPrep.getGame();
        const octi::OctiPod pod4 = bPrep.squareAt(0, 0).getPodAt(0);
        gPrep->prepareHops(0, 0, pod4, {octi::OctiHop{0, 1, false}}, true);
        gPrep->prepareHops(0, 1, pod4, {octi::OctiHop{1, 2, false}}, true);
        gPrep->prepareHops(1, 2, pod4, {octi::OctiHop{2, 2, false}}, true);
        gPrep->prepareHops(2, 2, pod4, {octi::OctiHop{3, 1, false}}, true);
        gPrep->prepareHops(3, 1, pod4, {octi::OctiHop{3, 2, false}}, false);
        STRCMP_EQUAL(
            "11a!b!c!d!-12;12a!b!c!d!-23;23a!b!c!d!-33;33a!b!c!d!-42;42a!b!c!d!-43",
            gPrep->getPreparedMoveString().c_str());
        gPrep->commitPreparedMove();
        CHECK_EQUAL(1, bPrep.squareAt(3, 2).getNumPods());
    }

    // 2. Stacked identical multi-SP pods disambiguation (Move X vs Move Y on A32a!c! + A33a!c!)
    {
        const char *kStackedMultiSpFen =
            "4:3:1:0:P:A:-:0:0,0,1,0:0,0,1,0:0,0,1,0:0,0,1,0:"
            "A32a!c!,A33a!c!,B37e,C77e,D73a";
        octi::OctiBoard bx(octi::GAME_MODE_4, true, false, octi::GAME_BASES_3);
        bx.fromPositionString(kStackedMultiSpFen);
        // Move X: p1 moves 32->33 (a!), p2 moves that pod 33->43 (c!), p3 moves the fresh 33->34 (a!)
        bx.getGame()->move("32a!c!-33;33a!c!-43;33a!c!-34;43a!c!+b");
        CHECK_EQUAL(1, bx.squareAt(3, 2).getNumPods()); // 43
        CHECK_EQUAL(1, bx.squareAt(2, 3).getNumPods()); // 34

        octi::OctiBoard by(octi::GAME_MODE_4, true, false, octi::GAME_BASES_3);
        by.fromPositionString(kStackedMultiSpFen);
        // Move Y: p1 moves 32->33 (a!), p2 moves the fresh 33->43 (c!), p3 moves it 43->44 (a!)
        by.getGame()->move("32a!c!-33;33a!c!-43;43a!c!-44;44a!c!+b");
        CHECK_EQUAL(1, by.squareAt(2, 2).getNumPods()); // 33
        CHECK_EQUAL(1, by.squareAt(3, 3).getNumPods()); // 44

        octi::OctiBoard bz(octi::GAME_MODE_4, true, false, octi::GAME_BASES_3);
        bz.fromPositionString(kStackedMultiSpFen);
        // Move Z: p1 32->33 (a!), p2 33->34 (a!), p3 34->35 (a! reused!) -> ILLEGAL
        CHECK_THROWS(octi::IllegalMoveException,
                     bz.getGame()->move("32a!c!-33;33a!c!-34;34a!c!-35;35a!c!+b"));
    }

    // 3. Immediate capture removal between SP preludes & atomic rollback on mid-prelude failure:
    //    (a) SP1 33a!-35x captures B34e immediately so SP2 14c!-34x can jump over B24 INTO vacated 34!
    {
        octi::OctiBoard bImm(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        bImm.fromPositionString("2:3:1:0:P:A:-:0:0,0,0,0:0,0,0,0:A33a!,A14c!,B24,B34e,B57e");
        const std::string origImmFen = bImm.toPositionString();
        const std::vector<octi::OctiMove> legal = bImm.getGame()->generateLegalMoves();
        const std::vector<octi::OctiMove> tactical = bImm.getGame()->generateTacticalMoves();

        // Without x on SP1 (33a!-35), B34e still blocks 34 so 14c!-34x is impossible:
        CHECK_FALSE(containsExactMove(legal, "33a!-35;14c!-34x;35a!+b"));
        // With x on SP1 (33a!-35x), 34 is vacated immediately so 14c!-34x succeeds AND captured prong 'e' funds +b!
        CHECK_TRUE(containsExactMove(legal, "33a!-35x;14c!-34x;35a!+b"));
        CHECK_TRUE(containsExactMove(tactical, "33a!-35x;14c!-34x;35a!+b"));

        // Failed SP2 after SP1 capture rolls back SP1 captures & reserve increments atomically:
        CHECK_THROWS(octi::IllegalMoveException,
                     bImm.getGame()->move("33a!-35x;14c!-36;35a!+b"));
        STRCMP_EQUAL(origImmFen.c_str(), bImm.toPositionString().c_str());
        CHECK_EQUAL(0, bImm.getGame()->getCapturedPods(octi::PLAYER_B));
        CHECK_EQUAL(0, bImm.getGame()->getFreeProngs(octi::PLAYER_A));
        CHECK_FALSE(bImm.getGame()->canUndo());

        bImm.getGame()->move("33a!-35x;14c!-34x;35a!+b");
        CHECK_EQUAL(1, bImm.squareAt(2, 3).getNumPods()); // A14c! landed on 34
        CHECK_EQUAL(1, bImm.squareAt(2, 4).getNumPods()); // A33a!+b at 35
        CHECK_EQUAL(2, bImm.getGame()->getCapturedPods(octi::PLAYER_B));
        CHECK_EQUAL(0, bImm.getGame()->getFreeProngs(octi::PLAYER_A));
    }

    //    (b) Friendly SP pod captured by SP1 (33a!-35x capturing A34c!) is removed immediately
    //        and CANNOT fire in SP2 (nor can the newly freed reserve SP fire a prelude)
    {
        octi::OctiBoard bSelfSp(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        bSelfSp.fromPositionString("2:3:1:0:P:A:-:0:0,0,0,0:0,0,0,0:A33a!,A34c!,B57e");
        const std::string origSelfFen = bSelfSp.toPositionString();
        const std::vector<octi::OctiMove> legal = bSelfSp.getGame()->generateLegalMoves();

        CHECK_FALSE(containsMoveWithPrefix(legal, "33a!-35x;34c!-44;"));
        CHECK_TRUE(containsExactMove(legal, "33a!-35;34c!-44;35a!-36"));
        // Capturing A34c! puts c! into reserve so regularOps CAN re-attach +c! on 35:
        CHECK_TRUE(containsExactMove(legal, "33a!-35x;35a!+c!"));

        CHECK_THROWS(octi::IllegalMoveException,
                     bSelfSp.getGame()->move("33a!-35x;34c!-44;35a!-36"));
        STRCMP_EQUAL(origSelfFen.c_str(), bSelfSp.toPositionString().c_str());
    }

    // 4. Victory & 4P base elimination evaluated ONLY at completion of full turn:
    //    SP1 landing on final enemy base 37 does NOT win if regularOps vacates 37,
    //    and DOES win if 37 remains occupied after regularOps!
    {
        octi::OctiBoard bWinVacate(octi::GAME_MODE_2, true, false, octi::GAME_BASES_1);
        bWinVacate.fromPositionString("2:1:1:0:P:A:-:0:0,0,1,0:0,0,1,0:A36a!e,B77e");
        bWinVacate.getGame()->move("36a!e-37;37a!e-36");
        CHECK_FALSE(bWinVacate.getGame()->isGameOver());
        CHECK_EQUAL(octi::PLAYER_UNDEF, bWinVacate.getGame()->getWinner());
        CHECK_EQUAL(octi::PLAYER_B, bWinVacate.getGame()->getTurn());

        octi::OctiBoard bWinStay(octi::GAME_MODE_2, true, false, octi::GAME_BASES_1);
        bWinStay.fromPositionString("2:1:1:0:P:A:-:0:0,0,1,0:0,0,1,0:A36a!e,B77e");
        bWinStay.getGame()->move("36a!e-37;37a!e+b");
        CHECK_TRUE(bWinStay.getGame()->isGameOver());
        CHECK_EQUAL(octi::PLAYER_A, bWinStay.getGame()->getWinner());

        // 4P_SP transient base touch in SP1 followed by regular step off 37 does NOT eliminate B
        octi::OctiBoard b4Touch(octi::GAME_MODE_4, true, false, octi::GAME_BASES_3);
        b4Touch.fromPositionString(
            "4:3:1:0:P:A:-:0:0,0,1,0:0,0,1,0:0,0,1,0:0,0,1,0:A36a!e,B46e!,C77e,D73a");
        b4Touch.getGame()->move("36a!e-37;37a!e-36");
        CHECK_FALSE(b4Touch.getGame()->isPlayerEliminated(octi::PLAYER_B));
        CHECK_EQUAL(1, b4Touch.squareAt(3, 5).getNumPods()); // B46e! not swept
        CHECK_EQUAL(octi::PLAYER_B, b4Touch.getGame()->getTurn());
    }
}

TEST(OctiDonGreenRuleRevisionsTest, R6_Permanent4PEliminationSweepsProngsToCapturerAndNoPartnerRevival)
{
    // Verify permanent 4P elimination sweeps all board pods of eliminated player B,
    // transfers all normal prongs and SuperProngs from B's swept pods to capturing enemy A,
    // skips B permanently, and forbids partner D from reviving B via 37*!
    octi::OctiBoard board(octi::GAME_MODE_4, true, false, octi::GAME_BASES_3);
    board.fromPositionString(
        "4:3:1:0:P:A:-:0:1,0,2,0:1,0,3,0:1,0,2,0:1,1,2,0:"
        "A36a,B44bc!,B55d,C77e,D53a,D73a");
    octi::OctiGame *game = board.getGame();

    // Before A captures B's home base 37: A has 2 freeProngs, 0 freeSuperProngs; B has 2 board pods (2 normal + 1 SP)
    CHECK_EQUAL(2, game->getFreeProngs(octi::PLAYER_A));
    CHECK_EQUAL(0, game->getFreeSuperProngs(octi::PLAYER_A));
    CHECK_EQUAL(0, game->getCapturedPods(octi::PLAYER_B));

    // A steps onto B's home base 37 -> B is permanently eliminated!
    game->move("36a-37");
    CHECK_TRUE(game->isPlayerEliminated(octi::PLAYER_B));
    CHECK_TRUE(board.squareAt(3, 3).isEmpty()); // B44bc! swept
    CHECK_TRUE(board.squareAt(4, 4).isEmpty()); // B55d swept
    CHECK_EQUAL(2, game->getCapturedPods(octi::PLAYER_B));
    // Capturing enemy A receives B's 2 normal prongs (b, d) and 1 SuperProng (c!)
    CHECK_EQUAL(4, game->getFreeProngs(octi::PLAYER_A));
    CHECK_EQUAL(1, game->getFreeSuperProngs(octi::PLAYER_A));
    // B's turn is skipped -> C's turn!
    CHECK_EQUAL(octi::PLAYER_C, game->getTurn());

    // C plays 77, then D's turn: even if A vacates 37 later, D can NEVER revive B on empty 37 with 37*
    game->move("77");
    CHECK_EQUAL(octi::PLAYER_D, game->getTurn());
    for (const auto &m : game->generateLegalMoves()) {
        CHECK_FALSE(m.asString() == "37*");
    }
    CHECK_THROWS(octi::IllegalMoveException, game->move("37*"));

    // Undo restores pre-elimination state, Zobrist hash, and swept pods cleanly
    game->undo(); // undo C's 77
    game->undo(); // undo A's 36a-37
    CHECK_FALSE(game->isPlayerEliminated(octi::PLAYER_B));
    CHECK_EQUAL(1, board.squareAt(3, 3).getNumPods());
    CHECK_EQUAL(1, board.squareAt(4, 4).getNumPods());
    CHECK_EQUAL(2, game->getFreeProngs(octi::PLAYER_A));
    CHECK_EQUAL(0, game->getFreeSuperProngs(octi::PLAYER_A));
}

TEST(OctiDonGreenRuleRevisionsTest, RapidCheckFourSuperProngAndEliminationStateInvariants)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    bool passed = rc::check(
        "4P_SP with up to 4 controlled SPs, multi-pod stacks & eliminated seats preserves generator-validator & undo invariants",
        []() {
            const bool edgeless = *rc::gen::arbitrary<bool>();
            const bool elimB = *rc::gen::arbitrary<bool>();
            octi::OctiBoard board(octi::GAME_MODE_4, true, edgeless, octi::GAME_BASES_3);
            board.suppressBoardUpdateNotification(true);

            const int freePodsA = *rc::gen::inRange(0, 2);
            const int capPodsA = *rc::gen::inRange(0, 2);
            const int freeProngsA = *rc::gen::inRange(0, 2);
            const int freeSuperA = *rc::gen::inRange(0, 2);

            static const int kCoords[][2] = {
                {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0},
                {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1},
                {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3},
                {1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}
            };
            constexpr int kNumCoords = static_cast<int>(sizeof(kCoords) / sizeof(kCoords[0]));

            // Place 1..3 A pods holding up to 4 SP sockets total + optional stacked stationary A pod
            int spRemaining = *rc::gen::inRange(1, 5); // 1..4 SP sockets
            const int numAPods = *rc::gen::inRange(1, 4);
            std::ostringstream sqPart;
            bool first = true;
            auto appendTok = [&](const std::string &tok) {
                if (!first) sqPart << ",";
                first = false;
                sqPart << tok;
            };

            bool usedByA[9][9] = {};
            int anchorX = kCoords[*rc::gen::inRange(0, kNumCoords)][0];
            int anchorY = kCoords[*rc::gen::inRange(0, kNumCoords)][1];
            usedByA[anchorX][anchorY] = true;
            if (*rc::gen::arbitrary<bool>()) {
                // Stationary stack-mate on anchor square to stress R4
                appendTok(std::string("A") + static_cast<char>('1' + anchorX) +
                          static_cast<char>('1' + anchorY) + "a");
            }

            for (int i = 0; i < numAPods; ++i) {
                int px = (i == 0) ? anchorX : kCoords[*rc::gen::inRange(0, kNumCoords)][0];
                int py = (i == 0) ? anchorY : kCoords[*rc::gen::inRange(0, kNumCoords)][1];
                usedByA[px][py] = true;
                int wantSp = (i + 1 == numAPods) ? spRemaining : *rc::gen::inRange(0, spRemaining + 1);
                spRemaining -= wantSp;
                int normDir = *rc::gen::inRange(0, 8);
                std::string cfg;
                int assignedSp = 0;
                int startD = *rc::gen::inRange(0, 8);
                bool isSp[8] = {false};
                for (int k = 0; k < 8 && assignedSp < wantSp; ++k) {
                    isSp[(startD + k * 3) % 8] = true;
                    ++assignedSp;
                }
                for (int d = 0; d < 8; ++d) {
                    char c = static_cast<char>('a' + d);
                    if (isSp[d]) {
                        cfg.push_back(c);
                        cfg.push_back('!');
                    } else if (d == normDir) {
                        cfg.push_back(c);
                    }
                }
                appendTok(std::string("A") + static_cast<char>('1' + px) +
                          static_cast<char>('1' + py) + cfg);
            }

            // Place a pivot/enemy pod on an unoccupied square and D73a so game is active
            int pivotSeed = *rc::gen::inRange(0, kNumCoords);
            int pivotIdx = pivotSeed;
            for (int step = 0; step < kNumCoords; ++step) {
                int cand = (pivotSeed + step) % kNumCoords;
                if (!usedByA[kCoords[cand][0]][kCoords[cand][1]]) {
                    pivotIdx = cand;
                    break;
                }
            }
            appendTok(std::string("D") + static_cast<char>('1' + kCoords[pivotIdx][0]) +
                      static_cast<char>('1' + kCoords[pivotIdx][1]) + "e");
            if (!elimB) {
                appendTok("B37e");
            }
            appendTok("C77e");
            appendTok("D73a");

            std::ostringstream fen;
            fen << "4:3:1:" << (edgeless ? "1" : "0") << ":P:A:-:"
                << (elimB ? "2" : "0") << ":"
                << freePodsA << "," << capPodsA << "," << freeProngsA << "," << freeSuperA << ":"
                << "0,2,0,0:1,0,2,0:1,1,2,0:" << sqPart.str();

            board.fromPositionString(fen.str());
            octi::OctiGame *game = board.getGame();
            const std::string fenBefore = board.toPositionString();
            const std::uint64_t hashBefore = octi::OctiBotEngine::computePositionHashStatic(board);

            std::vector<octi::OctiMove> legal = game->generateLegalMoves();
            RC_ASSERT(game->hasAnyLegalMove() == !legal.empty());
            if (legal.empty()) {
                return;
            }

            for (const auto &m : legal) {
                RC_ASSERT(!moveHasAnyRepositionOp(m));
                RC_ASSERT(countPreludeOps(m) <= 4);
            }

            // Sample up to 24 legal moves (including max-prelude moves) for full move() + undo() + Zobrist check
            std::vector<std::size_t> indices;
            for (std::size_t i = 0; i < legal.size(); ++i) {
                if (i < 12 || countPreludeOps(legal[i]) >= 3 || i + 12 >= legal.size()) {
                    indices.push_back(i);
                }
            }
            for (std::size_t idx : indices) {
                const octi::OctiMove &m = legal[idx];
                octi::OctiMove reparsed(m.asString());
                RC_ASSERT(reparsed.asString() == m.asString());
                RC_ASSERT(reparsed.structuralKey() == m.structuralKey());
                game->move(reparsed);
                game->undo();
                RC_ASSERT(board.toPositionString() == fenBefore);
                RC_ASSERT(octi::OctiBotEngine::computePositionHashStatic(board) == hashBefore);
            }
        });
    CHECK_TRUE(passed);
}

// ---------------------------------------------------------------------------
// Rule R7 — Universal No-Progress Draw Tests
// ---------------------------------------------------------------------------

TEST_GROUP(R7NoProgressDrawTests)
{
};

TEST(R7NoProgressDrawTests, ThirtyPlyDrawTriggerIn2P3B_2P1B_And_OFK)
{
    // 1. 2P_3B: 30 consecutive non-progress plies from a full game start trigger NO_PROGRESS_DRAW
    {
        octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
        octi::OctiGame *game = board.getGame();
        game->start(octi::PLAYER_A);
        CHECK_EQUAL(30, game->getNoProgressDrawLimit());
        CHECK_EQUAL(0, game->getNoProgressPlies());
        CHECK_FALSE(game->isNoProgressDraw());

        // Arm A33ae and B37ae via normal prong progress moves (counter stays 0)
        game->move("33+a");
        game->move("37+a");
        game->move("33a+e");
        game->move("37a+e");
        CHECK_EQUAL(0, game->getNoProgressPlies());

        for (int round = 0; round < 15; ++round) {
            const bool even = (round % 2 == 0);
            game->move(even ? "33ae-34" : "34ae-33");
            CHECK_EQUAL(round * 2 + 1, game->getNoProgressPlies());
            CHECK_FALSE(game->isGameOver());
            CHECK_FALSE(game->isNoProgressDraw());

            game->move(even ? "37ae-36" : "36ae-37");
            CHECK_EQUAL(round * 2 + 2, game->getNoProgressPlies());
            if (round < 14) {
                CHECK_FALSE(game->isGameOver());
                CHECK_FALSE(game->isNoProgressDraw());
            }
        }

        CHECK_EQUAL(30, game->getNoProgressPlies());
        CHECK_TRUE(game->isGameOver());
        CHECK_FALSE(game->isGameResigned());
        CHECK_EQUAL(octi::PLAYER_UNDEF, game->getWinner());
        CHECK_EQUAL(octi::PLAYER_UNDEF, game->getTurn());
        CHECK_TRUE(game->isNoProgressDraw());

        auto root = nlohmann::json::parse(board.serializeJson());
        STRCMP_EQUAL("GAME_OVER", root["status"].get<std::string>().c_str());
        STRCMP_EQUAL("NONE", root["winner"].get<std::string>().c_str());
        STRCMP_EQUAL("NO_PROGRESS_DRAW", root["termination"].get<std::string>().c_str());
        CHECK_EQUAL(30, root["noProgressPlies"].get<int>());

        octi::OctiBoard replay;
        replay.deserializeGame(board.serializeGame());
        CHECK_TRUE(replay.getGame()->isGameOver());
        CHECK_TRUE(replay.getGame()->isNoProgressDraw());
        CHECK_EQUAL(octi::PLAYER_UNDEF, replay.getGame()->getWinner());
        CHECK_EQUAL(30, replay.getGame()->getNoProgressPlies());
    }

    // 2. 2P_1B: 30 consecutive non-progress plies trigger NO_PROGRESS_DRAW
    {
        octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
        board.fromPositionString("2:1:0:0:P:A:-:0:4,0,23,0:4,0,23,0:A33ae,B37ae");
        octi::OctiGame *game = board.getGame();
        CHECK_EQUAL(30, game->getNoProgressDrawLimit());

        for (int round = 0; round < 15; ++round) {
            const bool even = (round % 2 == 0);
            game->move(even ? "33ae-34" : "34ae-33");
            game->move(even ? "37ae-36" : "36ae-37");
        }
        CHECK_TRUE(game->isGameOver());
        CHECK_TRUE(game->isNoProgressDraw());
        CHECK_EQUAL(octi::PLAYER_UNDEF, game->getWinner());
    }

    // 3. OFK: 30 consecutive non-progress plies trigger NO_PROGRESS_DRAW
    {
        octi::OctiBoard board(octi::GAME_MODE_OFK);
        board.fromPositionString(
            "OFK:1:0:0:P:A:-:0:0,0,10,0:0,0,10,0:A22ae,A32,A42,A52,B26ae,B36,B46,B56");
        octi::OctiGame *game = board.getGame();
        CHECK_EQUAL(30, game->getNoProgressDrawLimit());

        for (int round = 0; round < 15; ++round) {
            const bool even = (round % 2 == 0);
            game->move(even ? "22ae-23" : "23ae-22");
            CHECK_FALSE(game->isGameOver());
            game->move(even ? "26ae-25" : "25ae-26");
            if (round < 14) {
                CHECK_FALSE(game->isGameOver());
            }
        }
        CHECK_EQUAL(30, game->getNoProgressPlies());
        CHECK_TRUE(game->isGameOver());
        CHECK_TRUE(game->isNoProgressDraw());
        CHECK_EQUAL(octi::PLAYER_UNDEF, game->getWinner());
    }
}

TEST(R7NoProgressDrawTests, SixtyPlyDrawTriggerIn4PIncludingEliminatedSeats)
{
    // Full 4-seat 4P game: does NOT draw at ply 30; draws at ply 60
    {
        octi::OctiBoard board(octi::GAME_MODE_4);
        board.fromPositionString(
            "4:3:0:0:P:A:-:0:3,0,10,0:3,0,10,0:3,0,10,0:3,0,10,0:A33ae,B37ae,C77ae,D73ae");
        octi::OctiGame *game = board.getGame();
        CHECK_EQUAL(60, game->getNoProgressDrawLimit());

        for (int round = 0; round < 15; ++round) {
            const bool even = (round % 2 == 0);
            game->move(even ? "33ae-34" : "34ae-33");
            game->move(even ? "37ae-36" : "36ae-37");
            if (round == 7) {
                // Ply 30 reached: must NOT draw in 4P
                CHECK_EQUAL(30, game->getNoProgressPlies());
                CHECK_FALSE(game->isGameOver());
                CHECK_FALSE(game->isNoProgressDraw());
            }
            game->move(even ? "77ae-76" : "76ae-77");
            if (round == 14) {
                CHECK_EQUAL(59, game->getNoProgressPlies());
                CHECK_FALSE(game->isGameOver());
            }
            game->move(even ? "73ae-74" : "74ae-73");
        }

        CHECK_EQUAL(60, game->getNoProgressPlies());
        CHECK_TRUE(game->isGameOver());
        CHECK_TRUE(game->isNoProgressDraw());
        CHECK_EQUAL(octi::PLAYER_UNDEF, game->getWinner());
        CHECK_EQUAL(octi::PLAYER_UNDEF, game->getTurn());
    }

    // 4P with seat B already eliminated (3 active seats): threshold remains 60 plies
    {
        octi::OctiBoard board(octi::GAME_MODE_4);
        board.fromPositionString(
            "4:3:0:0:P:A:-:2:3,0,10,0:0,1,0,0:3,0,10,0:3,0,10,0:A33ae,C77ae,D73ae");
        octi::OctiGame *game = board.getGame();
        CHECK_EQUAL(60, game->getNoProgressDrawLimit());

        for (int round = 0; round < 20; ++round) {
            const bool even = (round % 2 == 0);
            game->move(even ? "33ae-34" : "34ae-33");
            game->move(even ? "77ae-76" : "76ae-77");
            game->move(even ? "73ae-74" : "74ae-73");
            if (round < 19) {
                CHECK_FALSE(game->isGameOver());
            }
        }
        CHECK_EQUAL(60, game->getNoProgressPlies());
        CHECK_TRUE(game->isGameOver());
        CHECK_TRUE(game->isNoProgressDraw());
    }
}

TEST(R7NoProgressDrawTests, NonCapturingCircularJumpsRemainLegalAndCountAsNoProgress)
{
    // Disjoint 4-hop diamond loop-back chains for A and B:
    // A: 44 -> 66 (over 55) -> 48 (over 57) -> 26 (over 37) -> 44 (over 35)
    // B: 54 -> 76 (over 65) -> 58 (over 67) -> 36 (over 47) -> 54 (over 45)
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.fromPositionString(
        "2:3:0:0:P:A:-:0:0,0,5,0:0,0,5,0:"
        "A44bdfh,A45,A47,A65,A67,B35,B37,B54bdfh,B55,B57");
    octi::OctiGame *game = board.getGame();

    bool foundCircular = false;
    for (const auto &m : game->generateLegalMoves()) {
        if (m.asString() == "44bdfh-66-48-26-44") {
            foundCircular = true;
            break;
        }
    }
    CHECK_TRUE(foundCircular);

    for (int ply = 1; ply <= 30; ++ply) {
        if (ply % 2 == 1) {
            game->move("44bdfh-66-48-26-44");
        } else {
            game->move("54bdfh-76-58-36-54");
        }
        CHECK_EQUAL(ply, game->getNoProgressPlies());
        if (ply < 30) {
            CHECK_FALSE(game->isGameOver());
        }
    }
    CHECK_TRUE(game->isGameOver());
    CHECK_TRUE(game->isNoProgressDraw());
    CHECK_EQUAL(octi::PLAYER_UNDEF, game->getWinner());

    // Composite clear-out + circular jump S -> ... -> S is also legal and advances counter
    octi::OctiBoard compBoard(octi::GAME_MODE_2);
    compBoard.fromPositionString(
        "2:3:0:0:P:A:-:0:0,0,5,0:0,0,5,0:"
        "A44a,A44bdfh,B35,B37,B55,B57:10");
    compBoard.getGame()->move("44a-45,44bdfh-66-48-26-44");
    CHECK_EQUAL(11, compBoard.getGame()->getNoProgressPlies());
    CHECK_EQUAL(1, compBoard.squareAt(3, 3).getNumPods());
}

TEST(R7NoProgressDrawTests, AllFourProgressEventsResetCounterToZero)
{
    // 1a. Regular jump capture (x) resets counter
    {
        octi::OctiBoard board(octi::GAME_MODE_2);
        board.fromPositionString("2:3:0:0:P:A:-:0:0,0,5,0:0,0,5,0:A33a,B34,B77e:29");
        CHECK_EQUAL(29, board.getGame()->getNoProgressPlies());
        board.getGame()->move("33a-35x");
        CHECK_FALSE(board.getGame()->isGameOver());
        CHECK_EQUAL(0, board.getGame()->getNoProgressPlies());
    }

    // 1b. SuperProng bonus prelude capture (x) followed by regular step resets counter
    {
        octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        board.fromPositionString("2:3:1:0:P:A:-:0:0,0,5,0:0,0,5,0:A44a!c,B45,B77e:29");
        CHECK_EQUAL(29, board.getGame()->getNoProgressPlies());
        board.getGame()->move("44a!c-46x;46a!c-56");
        CHECK_FALSE(board.getGame()->isGameOver());
        CHECK_EQUAL(0, board.getGame()->getNoProgressPlies());
    }

    // 2a. Reserve pod deployment (xy) resets counter
    {
        octi::OctiBoard board(octi::GAME_MODE_2);
        board.fromPositionString("2:3:0:0:P:A:-:0:2,0,5,0:2,0,5,0:A33a,B37e:29");
        board.getGame()->move("53");
        CHECK_FALSE(board.getGame()->isGameOver());
        CHECK_EQUAL(0, board.getGame()->getNoProgressPlies());
    }

    // 2b. Captured pod liberation (xy*) resets counter
    {
        octi::OctiBoard board(octi::GAME_MODE_2);
        board.fromPositionString("2:3:0:0:P:A:-:0:0,1,5,0:2,0,5,0:A33a,A37a,B77e:29");
        board.getGame()->move("33*");
        CHECK_FALSE(board.getGame()->isGameOver());
        CHECK_EQUAL(0, board.getGame()->getNoProgressPlies());
    }

    // 3a & 3b. Normal prong (+d) and SuperProng (+d!) insertions reset counter
    {
        octi::OctiBoard board(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
        board.fromPositionString("2:3:1:0:P:A:-:0:0,0,5,1:0,0,5,1:A33a,B37e:29");
        board.getGame()->move("33a+c");
        CHECK_EQUAL(0, board.getGame()->getNoProgressPlies());

        board.getGame()->setNoProgressPlies(29);
        board.getGame()->move("37e+g!");
        CHECK_FALSE(board.getGame()->isGameOver());
        CHECK_EQUAL(0, board.getGame()->getNoProgressPlies());
    }

    // 4. 4P permanent base elimination via non-capturing step resets counter,
    //    whereas re-entering an already-eliminated player's base does NOT reset
    {
        octi::OctiBoard board(octi::GAME_MODE_4);
        board.fromPositionString(
            "4:3:0:0:P:A:-:0:1,0,5,0:1,0,5,0:1,0,5,0:1,0,5,0:A36ae,B44e,C77ae,D73ae:59");
        octi::OctiGame *game = board.getGame();
        CHECK_EQUAL(59, game->getNoProgressPlies());

        // Non-capturing step onto B's home base 37 permanently eliminates B -> progress!
        game->move("36ae-37");
        CHECK_TRUE(game->isPlayerEliminated(octi::PLAYER_B));
        CHECK_FALSE(game->isGameOver());
        CHECK_EQUAL(0, game->getNoProgressPlies());

        // Advance C, D, then A vacates 37, C, D, then A re-enters 37 (B already eliminated)
        game->move("77ae-76"); // ply 1
        game->move("73ae-74"); // ply 2
        game->move("37ae-36"); // ply 3
        game->move("76ae-77"); // ply 4
        game->move("74ae-73"); // ply 5
        game->move("36ae-37"); // ply 6: B was already eliminated -> NO new elimination -> 6!
        CHECK_EQUAL(6, game->getNoProgressPlies());
    }
}

TEST(R7NoProgressDrawTests, SteppingOnOrOffBasesWithoutCaptureDoesNotResetCounter)
{
    // 2P_3B: stepping onto empty enemy base (2nd of 3), stepping off it,
    // and stepping onto friendly-occupied enemy base all increment noProgressPlies
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.fromPositionString(
        "2:3:0:0:P:A:-:0:0,0,5,0:0,0,5,0:A36ae,A56a,A57a,B77ae:10");
    octi::OctiGame *game = board.getGame();

    game->move("36ae-37"); // step onto empty enemy base 37 (2nd base, not game over)
    CHECK_EQUAL(11, game->getNoProgressPlies());

    game->move("77ae-76");
    CHECK_EQUAL(12, game->getNoProgressPlies());

    game->move("37ae-36"); // step off enemy base 37
    CHECK_EQUAL(13, game->getNoProgressPlies());

    game->move("76ae-77");
    CHECK_EQUAL(14, game->getNoProgressPlies());

    game->move("56a-57");  // step onto friendly-occupied enemy base 57 (stacking)
    CHECK_EQUAL(15, game->getNoProgressPlies());

    // 4P: stepping onto neutral OCTI square 53 without capture increments counter
    octi::OctiBoard b4(octi::GAME_MODE_4);
    b4.fromPositionString(
        "4:3:0:0:P:A:-:0:0,0,5,0:0,0,5,0:0,0,5,0:0,0,5,0:A52a,B37e,C77e,D73a:20");
    b4.getGame()->move("52a-53");
    CHECK_EQUAL(21, b4.getGame()->getNoProgressPlies());
}

TEST(R7NoProgressDrawTests, VictoryAndStalemateTakePrecedenceOverR7DrawOnBoundaryPly)
{
    // 1. 2P_3B base-occupation victory on ply 30 wins instead of drawing
    {
        octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
        board.fromPositionString(
            "2:3:0:0:P:A:-:0:0,0,5,0:0,0,5,0:A37a,A57a,A76a,B33e:29");
        octi::OctiGame *game = board.getGame();
        game->move("76a-77");
        CHECK_EQUAL(30, game->getNoProgressPlies());
        CHECK_TRUE(game->isGameOver());
        CHECK_EQUAL(octi::PLAYER_A, game->getWinner());
        CHECK_FALSE(game->isNoProgressDraw());
        auto root = nlohmann::json::parse(board.serializeJson());
        STRCMP_EQUAL("BASE_OCCUPATION", root["termination"].get<std::string>().c_str());
    }

    // 2. 2P_1B base-occupation victory on ply 30 wins instead of drawing
    {
        octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
        board.fromPositionString("2:1:0:0:P:A:-:0:0,0,5,0:0,0,5,0:A36a,B33e:29");
        octi::OctiGame *game = board.getGame();
        game->move("36a-37");
        CHECK_TRUE(game->isGameOver());
        CHECK_EQUAL(octi::PLAYER_A, game->getWinner());
        CHECK_FALSE(game->isNoProgressDraw());
    }

    // 3. 4P base-occupation victory on ply 60 wins instead of drawing
    {
        octi::OctiBoard board(octi::GAME_MODE_4);
        board.fromPositionString(
            "4:3:0:0:P:C:-:2:0,0,5,0:0,1,0,0:0,0,5,0:0,0,5,0:A37a,C72a,D55e:59");
        octi::OctiGame *game = board.getGame();
        game->move("72a-73");
        CHECK_TRUE(game->isGameOver());
        CHECK_EQUAL(octi::PLAYER_A, game->getWinner());
        CHECK_FALSE(game->isNoProgressDraw());
    }

    // 4. Stalemate win on ply 30: A steps 28a-29 boxing in B19c (with A39a blocking jump)
    {
        octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
        board.fromPositionString(
            "2:3:0:0:P:A:-:0:0,0,5,0:0,0,0,0:A28a,A39a,B19c:29");
        octi::OctiGame *game = board.getGame();
        game->move("28a-29");
        CHECK_EQUAL(30, game->getNoProgressPlies());
        CHECK_TRUE(game->isGameOver());
        CHECK_EQUAL(octi::PLAYER_A, game->getWinner());
        CHECK_FALSE(game->isNoProgressDraw());
        auto root = nlohmann::json::parse(board.serializeJson());
        STRCMP_EQUAL("STALEMATE", root["termination"].get<std::string>().c_str());
    }
}

TEST(R7NoProgressDrawTests, UndoAndRedoRestoreNoProgressPliesAndDrawState)
{
    octi::OctiBoard board(octi::GAME_MODE_2);
    board.fromPositionString("2:3:0:0:P:A:-:0:0,0,5,0:0,0,5,0:A33ae,B37ae:28");
    octi::OctiGame *game = board.getGame();

    game->move("33ae-34"); // ply 29
    CHECK_EQUAL(29, game->getNoProgressPlies());
    CHECK_FALSE(game->isGameOver());

    game->move("37ae-36"); // ply 30 -> R7 draw
    CHECK_EQUAL(30, game->getNoProgressPlies());
    CHECK_TRUE(game->isGameOver());
    CHECK_TRUE(game->isNoProgressDraw());
    CHECK_EQUAL(octi::PLAYER_UNDEF, game->getWinner());

    game->undo();
    CHECK_EQUAL(29, game->getNoProgressPlies());
    CHECK_FALSE(game->isGameOver());
    CHECK_FALSE(game->isNoProgressDraw());
    CHECK_EQUAL(octi::PLAYER_B, game->getTurn());

    game->redo();
    CHECK_EQUAL(30, game->getNoProgressPlies());
    CHECK_TRUE(game->isGameOver());
    CHECK_TRUE(game->isNoProgressDraw());
    CHECK_EQUAL(octi::PLAYER_UNDEF, game->getWinner());

    // Undo again and play a progress move instead -> counter resets to 0
    game->undo();
    game->move("37ae+c");
    CHECK_EQUAL(0, game->getNoProgressPlies());
    CHECK_FALSE(game->isGameOver());
}

TEST(R7NoProgressDrawTests, OctiFenOptionalTokenAndCanonicalToPositionStringInvariance)
{
    auto countColons = [](const std::string &s) {
        return static_cast<int>(std::count(s.begin(), s.end(), ':'));
    };

    // 2P 12-token FEN sets noProgressPlies while toPositionString stays 11 tokens (10 colons)
    octi::OctiBoard b2;
    const std::string fen2Canon = "2:3:0:0:P:A:-:0:4,0,23,0:4,0,23,0:A33ae,B37ae";
    b2.fromPositionString(fen2Canon + ":27");
    CHECK_EQUAL(27, b2.getGame()->getNoProgressPlies());
    STRCMP_EQUAL(fen2Canon.c_str(), b2.toPositionString().c_str());
    CHECK_EQUAL(10, countColons(b2.toPositionString()));

    // OFK 12-token FEN
    octi::OctiBoard bOfk(octi::GAME_MODE_OFK);
    const std::string fenOfkCanon =
        "OFK:1:0:0:P:A:-:0:0,0,10,0:0,0,10,0:A22ae,A32,A42,A52,B26ae,B36,B46,B56";
    bOfk.fromPositionString(fenOfkCanon + ":19");
    CHECK_EQUAL(19, bOfk.getGame()->getNoProgressPlies());
    STRCMP_EQUAL(fenOfkCanon.c_str(), bOfk.toPositionString().c_str());

    // 4P 14-token FEN sets noProgressPlies while toPositionString stays 13 tokens (12 colons)
    octi::OctiBoard b4(octi::GAME_MODE_4);
    const std::string fen4Canon =
        "4:3:0:0:P:A:-:0:3,0,10,0:3,0,10,0:3,0,10,0:3,0,10,0:A33ae,D73ae,B37ae,C77ae";
    b4.fromPositionString(fen4Canon + ":58");
    CHECK_EQUAL(58, b4.getGame()->getNoProgressPlies());
    STRCMP_EQUAL(fen4Canon.c_str(), b4.toPositionString().c_str());
    CHECK_EQUAL(12, countColons(b4.toPositionString()));

    // Terminal draw FEN without optional token defaults noProgressPlies to 30 (2P) / 60 (4P)
    b2.fromPositionString("2:3:0:0:O:-:-:0:4,0,23,0:4,0,23,0:A34ae,B36ae");
    CHECK_EQUAL(30, b2.getGame()->getNoProgressPlies());
    CHECK_TRUE(b2.getGame()->isNoProgressDraw());

    b4.fromPositionString(
        "4:3:0:0:O:-:-:0:3,0,10,0:3,0,10,0:3,0,10,0:3,0,10,0:A34ae,B36ae,C76ae,D74ae");
    CHECK_EQUAL(60, b4.getGame()->getNoProgressPlies());
    CHECK_TRUE(b4.getGame()->isNoProgressDraw());

    // Reject malformed optional tokens or wrong token counts
    CHECK_THROWS(octi::IllegalMoveSyntaxException, b2.fromPositionString(fen2Canon + ":-1"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, b2.fromPositionString(fen2Canon + ":abc"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, b2.fromPositionString(fen2Canon + ":1001"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, b2.fromPositionString(fen2Canon + ":"));
    CHECK_THROWS(octi::IllegalMoveSyntaxException, b2.fromPositionString(fen2Canon + ":5:0"));
}

TEST(R7NoProgressDrawTests, OctiBotEngineSeeksDrawWhenLosingAndAvoidsDrawWhenWinningAtPly29)
{
    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL, 0x20261117ULL);
    octi::BotTuningProfile prof = bot.tuningProfile();
    prof.useOpeningBook = false;
    prof.rootScoreJitter = 0;
    prof.maxSearchDepth = 4;
    prof.lazySmpThreads = 1;
    prof.timeBudgetMs = 2000;
    bot.setTuningProfile(prof);

    // 1. Losing side at noProgressPlies = 29 forces an immediate R7 draw (score == 0)
    //    over progress moves that lose next ply to B34e-33 / B54e-53 (-WIN_SCORE + 2).
    //    First search at noProgressPlies = 0 on the SAME bot instance to verify TT horizon salting!
    {
        octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
        const std::string losingFen =
            "2:1:0:0:P:A:-:0:0,0,1,0:0,0,5,0:A19ce,B29,B34e,B54e";
        board.fromPositionString(losingFen);
        octi::BotSearchStats statsLow;
        (void)bot.chooseMove(board, &statsLow);
        CHECK_TRUE(statsLow.bestScore < -900000);

        board.getGame()->setNoProgressPlies(29);
        octi::BotSearchStats statsDraw;
        std::string chosen = bot.chooseMove(board, &statsDraw);
        CHECK_FALSE(chosen.empty());
        CHECK_EQUAL(0, statsDraw.bestScore);

        board.getGame()->move(chosen);
        CHECK_TRUE(board.getGame()->isGameOver());
        CHECK_TRUE(board.getGame()->isNoProgressDraw());
        CHECK_EQUAL(octi::PLAYER_UNDEF, board.getGame()->getWinner());
    }

    // 2. Winning side at noProgressPlies = 29 avoids R7 draw and plays progress move to win!
    //    A has unarmed A36 (1 prong insertion +a away from 36a-37 win) plus A19ce (which could
    //    step 19ce-18 for an instant 0-score R7 draw), while B only has harmless B79g.
    {
        octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
        const std::string winningFen =
            "2:1:0:0:P:A:-:0:0,0,1,0:0,0,0,0:A36,A19ce,B79g";
        board.fromPositionString(winningFen);
        octi::BotSearchStats statsLow;
        (void)bot.chooseMove(board, &statsLow);

        board.getGame()->setNoProgressPlies(29);
        octi::BotSearchStats statsWin;
        std::string chosen = bot.chooseMove(board, &statsWin);
        STRCMP_EQUAL("36+a", chosen.c_str());
        CHECK_TRUE(statsWin.bestScore > 900000);

        board.getGame()->move(chosen);
        CHECK_FALSE(board.getGame()->isGameOver());
        CHECK_FALSE(board.getGame()->isNoProgressDraw());
        CHECK_EQUAL(0, board.getGame()->getNoProgressPlies());
    }

    // 3. searchTopMovesForBook & Lazy SMP threads propagate noProgressPlies and score draws as 0
    {
        octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
        board.fromPositionString(
            "2:1:0:0:P:A:-:0:0,0,1,0:0,0,5,0:A19ce,B29,B34e,B54e:29");

        auto top = bot.searchTopMovesForBook(board, 3, 1000, 3);
        CHECK_FALSE(top.empty());
        CHECK_EQUAL(0, top[0].score);

        for (bool diversify : {false, true}) {
            octi::BotTuningProfile smpProf = prof;
            smpProf.lazySmpThreads = 2;
            smpProf.lazySmpDiversifyHelpers = diversify;
            smpProf.qsearchUseTt = true;
            smpProf.timeBudgetMs = 600;
            smpProf.maxSearchDepth = 3;
            bot.setTuningProfile(smpProf);

            octi::BotSearchStats smpStats;
            std::string smpMove = bot.chooseMove(board, &smpStats);
            CHECK_FALSE(smpMove.empty());
            CHECK_EQUAL(0, smpStats.bestScore);
        }
    }
}

// ---------------------------------------------------------------------------
// Envelope 2: OctiNetworkClient Ply-0 Abort & leaveTable(abortIfPly0) Tests
// ---------------------------------------------------------------------------

TEST_GROUP(OctiNetworkClientPly0AbortTest)
{
};

TEST(OctiNetworkClientPly0AbortTest,
     AbortGameParsesStoodUpPly0DetailAndHandles409And400FailClosed)
{
    std::mutex reqMutex;
    std::vector<std::string> seenActions;

    httplib::Server svr;
    auto recordAction = [&](const httplib::Request &req) -> std::string {
        nlohmann::json parsed = nlohmann::json::parse(req.body, nullptr, false);
        std::string act = parsed.is_object() ? parsed.value("action", "") : "";
        std::lock_guard<std::mutex> lock(reqMutex);
        seenActions.push_back(act);
        return act;
    };

    svr.Post("/api/v1/games/g-ply0-ok/action",
             [&](const httplib::Request &req, httplib::Response &res) {
                 std::string act = recordAction(req);
                 const nlohmann::json body = {
                     {"ok", true},
                     {"action", act},
                     {"room_id", "g-ply0-ok"},
                     {"game_over", false},
                     {"stood_up_ply0", true},
                     {"detail", "ply0_standup"},
                     {"seq", 42}
                 };
                 res.status = 200;
                 res.set_content(body.dump(), "application/json");
             });

    svr.Post("/api/v1/games/g-ply1-conflict/action",
             [&](const httplib::Request &req, httplib::Response &res) {
                 (void)recordAction(req);
                 const nlohmann::json body = {
                     {"error", "INVALID_STATE"},
                     {"message", "cannot stand up or abort without resigning after moves have been played"}
                 };
                 res.status = 409;
                 res.set_content(body.dump(), "application/json");
             });

    svr.Post("/api/v1/games/g-old-server/action",
             [&](const httplib::Request &req, httplib::Response &res) {
                 (void)recordAction(req);
                 const nlohmann::json body = {
                     {"error", "INVALID_ARGUMENT"},
                     {"message", "unknown action"}
                 };
                 res.status = 400;
                 res.set_content(body.dump(), "application/json");
             });

    int port = svr.bind_to_any_port("127.0.0.1");
    CHECK_TRUE(port > 0);
    std::thread serverThread([&svr]() { svr.listen_after_bind(); });

    std::unique_ptr<octi::OctiNetworkClient> client =
        octi::OctiNetworkClient::createForLoopbackTest(port);
    CHECK_TRUE(client != nullptr);

    /* 1. Nominal Ply-0 abort succeeds with stoodUpPly0=true and detail="ply0_standup" */
    octi::ApiResult<octi::GameActionData> okRes = client->abortGame("g-ply0-ok");
    CHECK_TRUE(okRes.ok);
    CHECK_EQUAL(200, okRes.httpStatus);
    STRCMP_EQUAL("abort", okRes.data.action.c_str());
    STRCMP_EQUAL("g-ply0-ok", okRes.data.roomId.c_str());
    CHECK_FALSE(okRes.data.gameOver);
    CHECK_TRUE(okRes.data.stoodUpPly0);
    STRCMP_EQUAL("ply0_standup", okRes.data.detail.c_str());
    CHECK_EQUAL(42, static_cast<int>(okRes.data.seq));

    /* 2. Ply > 0 rejection returns 409 INVALID_STATE with stoodUpPly0=false */
    octi::ApiResult<octi::GameActionData> conflictRes =
        client->abortGame("g-ply1-conflict");
    CHECK_FALSE(conflictRes.ok);
    CHECK_EQUAL(409, conflictRes.httpStatus);
    STRCMP_EQUAL("INVALID_STATE", conflictRes.errorCode.c_str());
    CHECK_FALSE(conflictRes.data.stoodUpPly0);

    /* 3. Un-upgraded server rejection returns 400 INVALID_ARGUMENT (fail-closed) */
    octi::ApiResult<octi::GameActionData> oldSrvRes =
        client->abortGame("g-old-server");
    CHECK_FALSE(oldSrvRes.ok);
    CHECK_EQUAL(400, oldSrvRes.httpStatus);
    STRCMP_EQUAL("INVALID_ARGUMENT", oldSrvRes.errorCode.c_str());
    STRCMP_EQUAL("unknown action", oldSrvRes.errorMessage.c_str());
    CHECK_FALSE(oldSrvRes.data.stoodUpPly0);

    svr.stop();
    if (serverThread.joinable()) {
        serverThread.join();
    }

    std::lock_guard<std::mutex> lock(reqMutex);
    CHECK_EQUAL(static_cast<std::size_t>(3), seenActions.size());
    STRCMP_EQUAL("abort", seenActions[0].c_str());
    STRCMP_EQUAL("abort", seenActions[1].c_str());
    STRCMP_EQUAL("abort", seenActions[2].c_str());
}

TEST(OctiNetworkClientPly0AbortTest,
     LeaveTableDefaultEmptyPayloadVsAbortIfPly0TrueAnd409Rejection)
{
    std::mutex reqMutex;
    std::vector<std::string> rawBodies;

    httplib::Server svr;
    svr.Post("/api/v1/tables/g-leave-payload/leave",
             [&](const httplib::Request &req, httplib::Response &res) {
                 {
                     std::lock_guard<std::mutex> lock(reqMutex);
                     rawBodies.push_back(req.body);
                 }
                 nlohmann::json parsed =
                     nlohmann::json::parse(req.body, nullptr, false);
                 bool abortPly0 = parsed.is_object() &&
                                  parsed.value("abort_if_ply0", false);
                 const nlohmann::json resp = {
                     {"ok", true},
                     {"stood_up_ply0", abortPly0},
                     {"detail", abortPly0 ? "ply0_standup" : "in_progress_disconnected"},
                     {"seq", 19}
                 };
                 res.status = 200;
                 res.set_content(resp.dump(), "application/json");
             });

    svr.Post("/api/v1/tables/g-leave-ply1/leave",
             [&](const httplib::Request &req, httplib::Response &res) {
                 {
                     std::lock_guard<std::mutex> lock(reqMutex);
                     rawBodies.push_back(req.body);
                 }
                 const nlohmann::json resp = {
                     {"error", "INVALID_STATE"},
                     {"message", "cannot stand up or abort without resigning after moves have been played"}
                 };
                 res.status = 409;
                 res.set_content(resp.dump(), "application/json");
             });

    int port = svr.bind_to_any_port("127.0.0.1");
    CHECK_TRUE(port > 0);
    std::thread serverThread([&svr]() { svr.listen_after_bind(); });

    std::unique_ptr<octi::OctiNetworkClient> client =
        octi::OctiNetworkClient::createForLoopbackTest(port);
    CHECK_TRUE(client != nullptr);

    /* 1. Default leaveTable(roomId) sends wire-identical empty JSON object {} */
    octi::ApiResult<bool> defRes = client->leaveTable("g-leave-payload");
    CHECK_TRUE(defRes.ok);
    CHECK_EQUAL(200, defRes.httpStatus);
    CHECK_TRUE(defRes.data);

    /* 2. Explicit leaveTable(roomId, false) also sends empty JSON object {} */
    octi::ApiResult<bool> falseRes = client->leaveTable("g-leave-payload", false);
    CHECK_TRUE(falseRes.ok);
    CHECK_EQUAL(200, falseRes.httpStatus);
    CHECK_TRUE(falseRes.data);

    /* 3. Explicit leaveTable(roomId, true) sends {"abort_if_ply0":true} */
    octi::ApiResult<bool> trueRes = client->leaveTable("g-leave-payload", true);
    CHECK_TRUE(trueRes.ok);
    CHECK_EQUAL(200, trueRes.httpStatus);
    CHECK_TRUE(trueRes.data);

    /* 4. leaveTable(roomId, true) at Ply > 0 surfaces 409 INVALID_STATE */
    octi::ApiResult<bool> conflictRes = client->leaveTable("g-leave-ply1", true);
    CHECK_FALSE(conflictRes.ok);
    CHECK_EQUAL(409, conflictRes.httpStatus);
    STRCMP_EQUAL("INVALID_STATE", conflictRes.errorCode.c_str());

    svr.stop();
    if (serverThread.joinable()) {
        serverThread.join();
    }

    std::lock_guard<std::mutex> lock(reqMutex);
    CHECK_EQUAL(static_cast<std::size_t>(4), rawBodies.size());

    nlohmann::json j0 = nlohmann::json::parse(rawBodies[0], nullptr, false);
    CHECK_TRUE(j0.is_object());
    CHECK_TRUE(j0.empty());
    CHECK_FALSE(j0.contains("abort_if_ply0"));

    nlohmann::json j1 = nlohmann::json::parse(rawBodies[1], nullptr, false);
    CHECK_TRUE(j1.is_object());
    CHECK_TRUE(j1.empty());
    CHECK_FALSE(j1.contains("abort_if_ply0"));

    nlohmann::json j2 = nlohmann::json::parse(rawBodies[2], nullptr, false);
    CHECK_TRUE(j2.is_object());
    CHECK_TRUE(j2.contains("abort_if_ply0"));
    CHECK_TRUE(j2["abort_if_ply0"].is_boolean());
    CHECK_TRUE(j2["abort_if_ply0"].get<bool>());

    nlohmann::json j3 = nlohmann::json::parse(rawBodies[3], nullptr, false);
    CHECK_TRUE(j3.is_object());
    CHECK_TRUE(j3.value("abort_if_ply0", false));
}

// ---------------------------------------------------------------------------
// Envelope 1: Core Virtual Dispatch Hooks & Zobrist freeProngs[5][51] Fix
// ---------------------------------------------------------------------------

namespace {

class MockPolymorphicOctiBotEngine : public octi::OctiBotEngine {
public:
    explicit MockPolymorphicOctiBotEngine(bool *destroyedFlag = nullptr)
        : octi::OctiBotEngine(octi::BotDifficulty::TACTICAL, 0x5EED2026ULL)
        , _destroyedFlag(destroyedFlag)
    {
    }

    ~MockPolymorphicOctiBotEngine() override
    {
        if (_destroyedFlag != nullptr) {
            *_destroyedFlag = true;
        }
    }

    int liveDepthReached() const override
    {
        return 17;
    }

    std::uint64_t liveNodesVisited() const override
    {
        return 987654ULL;
    }

    int liveBestScore() const override
    {
        return 4321;
    }

    std::string chooseMove(const octi::OctiBoard &board,
                           octi::BotSearchStats *outStats = nullptr) override
    {
        (void)board;
        if (outStats != nullptr) {
            outStats->depthReached = 17;
            outStats->nodesVisited = 987654ULL;
            outStats->bestScore = 4321;
            outStats->elapsedMs = 12;
        }
        return "33+a";
    }

    std::vector<octi::BookMoveScore> searchTopMovesForBook(
        const octi::OctiBoard &board,
        int searchDepth,
        int timeBudgetMs = 0,
        std::size_t topK = 3,
        octi::BotSearchStats *outStats = nullptr) override
    {
        (void)board;
        if (outStats != nullptr) {
            outStats->depthReached = searchDepth;
            outStats->nodesVisited = 654321ULL;
            outStats->bestScore = 4321;
            outStats->elapsedMs = timeBudgetMs;
        }
        std::vector<octi::BookMoveScore> res = {
            { "33+a", 4321 },
            { "53+a", 4100 },
            { "73+a", 3900 }
        };
        if (res.size() > topK) {
            res.resize(topK);
        }
        return res;
    }

private:
    bool *_destroyedFlag = nullptr;
};

} // namespace

TEST_GROUP(OctiEnvelope1CoreHooksAndZobristTest)
{
};

TEST(OctiEnvelope1CoreHooksAndZobristTest,
     ZobristDistinguishesFreeProngsAbove32UpTo50)
{
    octi::OctiBoard b32(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    octi::OctiBoard b40(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    octi::OctiBoard b50(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);

    b32.fromPositionString("2:3:0:0:P:A:-:0:0,0,32,0:0,0,25,0:A33ae,B37ae");
    b40.fromPositionString("2:3:0:0:P:A:-:0:0,0,40,0:0,0,25,0:A33ae,B37ae");
    b50.fromPositionString("2:3:0:0:P:A:-:0:0,0,50,0:0,0,25,0:A33ae,B37ae");

    CHECK_EQUAL(32, b32.getGame()->getFreeProngs(octi::PLAYER_A));
    CHECK_EQUAL(40, b40.getGame()->getFreeProngs(octi::PLAYER_A));
    CHECK_EQUAL(50, b50.getGame()->getFreeProngs(octi::PLAYER_A));

    const std::uint64_t h32 = octi::OctiBotEngine::computePositionHashStatic(b32);
    const std::uint64_t h40 = octi::OctiBotEngine::computePositionHashStatic(b40);
    const std::uint64_t h50 = octi::OctiBotEngine::computePositionHashStatic(b50);

    CHECK_TRUE(h32 != h40);
    CHECK_TRUE(h40 != h50);
    CHECK_TRUE(h32 != h50);

    const std::uint64_t c32 = octi::OctiBotEngine::computeCanonicalOpeningHash(b32);
    const std::uint64_t c40 = octi::OctiBotEngine::computeCanonicalOpeningHash(b40);
    const std::uint64_t c50 = octi::OctiBotEngine::computeCanonicalOpeningHash(b50);

    CHECK_TRUE(c32 != c40);
    CHECK_TRUE(c40 != c50);
    CHECK_TRUE(c32 != c50);

    std::set<std::uint64_t> distinctHashesA;
    std::set<std::uint64_t> distinctHashesB;
    for (int fpr = 0; fpr <= 50; ++fpr) {
        octi::OctiBoard ba(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
        ba.fromPositionString(
            "2:3:0:0:P:A:-:0:0,0," + std::to_string(fpr) + ",0:0,0,10,0:A33ae,B37ae");
        distinctHashesA.insert(octi::OctiBotEngine::computePositionHashStatic(ba));

        octi::OctiBoard bb(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
        bb.fromPositionString(
            "2:3:0:0:P:A:-:0:0,0,10,0:0,0," + std::to_string(fpr) + ",0:A33ae,B37ae");
        distinctHashesB.insert(octi::OctiBotEngine::computePositionHashStatic(bb));
    }
    CHECK_EQUAL(static_cast<std::size_t>(51), distinctHashesA.size());
    CHECK_EQUAL(static_cast<std::size_t>(51), distinctHashesB.size());

    // Values above 50 clamp safely to 50 without out-of-bounds access
    octi::OctiBoard b55(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    b55.fromPositionString("2:3:0:0:P:A:-:0:0,0,55,0:0,0,25,0:A33ae,B37ae");
    CHECK_EQUAL(h50, octi::OctiBotEngine::computePositionHashStatic(b55));
}

TEST(OctiEnvelope1CoreHooksAndZobristTest,
     PolymorphicOverrideOfChooseMoveBookSearchAndLiveTelemetry)
{
    octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
    board.getGame()->start(octi::PLAYER_A);

    bool destroyed = false;
    std::unique_ptr<octi::OctiBotEngine> ownedPtr =
        std::make_unique<MockPolymorphicOctiBotEngine>(&destroyed);
    octi::OctiBotEngine *basePtr = ownedPtr.get();
    octi::OctiBotEngine &baseRef = *ownedPtr;

    CHECK_EQUAL(17, basePtr->liveDepthReached());
    CHECK_EQUAL(static_cast<std::uint64_t>(987654ULL), basePtr->liveNodesVisited());
    CHECK_EQUAL(4321, basePtr->liveBestScore());

    CHECK_EQUAL(17, baseRef.liveDepthReached());
    CHECK_EQUAL(static_cast<std::uint64_t>(987654ULL), baseRef.liveNodesVisited());
    CHECK_EQUAL(4321, baseRef.liveBestScore());

    octi::BotSearchStats chooseStats{};
    std::string chosen = basePtr->chooseMove(board, &chooseStats);
    STRCMP_EQUAL("33+a", chosen.c_str());
    CHECK_EQUAL(17, chooseStats.depthReached);
    CHECK_EQUAL(static_cast<std::uint64_t>(987654ULL), chooseStats.nodesVisited);
    CHECK_EQUAL(4321, chooseStats.bestScore);
    LONGS_EQUAL(12, chooseStats.elapsedMs);

    octi::BotSearchStats bookStats{};
    std::vector<octi::BookMoveScore> top =
        baseRef.searchTopMovesForBook(board, 9, 250, 2, &bookStats);
    CHECK_EQUAL(static_cast<std::size_t>(2), top.size());
    STRCMP_EQUAL("33+a", top[0].move.c_str());
    CHECK_EQUAL(4321, top[0].score);
    STRCMP_EQUAL("53+a", top[1].move.c_str());
    CHECK_EQUAL(4100, top[1].score);
    CHECK_EQUAL(9, bookStats.depthReached);
    CHECK_EQUAL(static_cast<std::uint64_t>(654321ULL), bookStats.nodesVisited);
    LONGS_EQUAL(250, bookStats.elapsedMs);

    ownedPtr.reset();
    CHECK_TRUE(destroyed);
}

// ---------------------------------------------------------------------------
// Single Legal Move Fast Path (4P 0-Pod Forced Reserve Placement & 1-Ply Win)
// ---------------------------------------------------------------------------

TEST_GROUP(OctiSingleLegalMoveFastPathTest)
{
};

TEST(OctiSingleLegalMoveFastPathTest,
     NominalFourPlayerZeroPodSingleReservePlacementReturnsImmediately)
{
    // Reproduces Don Green's 4P match 951d15cc... (plies 60 & 68):
    // Player D has 0 pods on the board, freePods >= 1, and home square 73 empty,
    // leaving strictly 1 legal move ("73").
    octi::OctiBoard board(octi::GAME_MODE_4);
    board.fromPositionString(
        "4:2:0:0:P:D:-:0:3,0,10,0:3,0,10,0:3,0,10,0:1,0,10,0:A33a,B37e,C77e");

    octi::OctiGame *game = board.getGame();
    CHECK_TRUE(game != nullptr);
    std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
    CHECK_EQUAL(static_cast<std::size_t>(1), legalMoves.size());
    STRCMP_EQUAL("73", legalMoves[0].asString().c_str());

    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL, 0x5EED2026ULL);
    octi::BotTuningProfile prof = bot.tuningProfile();
    prof.timeBudgetMs = 5000;
    prof.lazySmpThreads = 4;
    prof.useOpeningBook = false;
    bot.setTuningProfile(prof);
    bot.setTimeBudgetMs(5000);

    octi::BotSearchStats stats{};
    std::string chosen = bot.chooseMove(board, &stats);
    STRCMP_EQUAL("73", chosen.c_str());
    CHECK_EQUAL(1, stats.depthReached);
    CHECK_EQUAL(static_cast<std::uint64_t>(1ULL), stats.nodesVisited);
    CHECK_EQUAL(0, stats.bestScore);
    CHECK_TRUE(stats.elapsedMs < 500);
    CHECK_EQUAL(1, bot.liveDepthReached());
    CHECK_EQUAL(static_cast<std::uint64_t>(1ULL), bot.liveNodesVisited());
    CHECK_EQUAL(0, bot.liveBestScore());
}

TEST(OctiSingleLegalMoveFastPathTest,
     BoundarySingleLegalMoveOnePlyVictoryPreservesWinScore)
{
    // Boundary 1: Strictly 1 legal move ("36a-37") which immediately captures
    // enemy base 37 in a 1-base game (1-ply victory). Must preserve WIN_SCORE (1000000).
    octi::OctiBoard board(octi::GAME_MODE_2, false, false, octi::GAME_BASES_1);
    board.fromPositionString(
        "2:1:0:0:P:A:-:0:0,0,0,0:0,0,0,0:A36a,B57");

    octi::OctiGame *game = board.getGame();
    CHECK_TRUE(game != nullptr);
    std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
    CHECK_EQUAL(static_cast<std::size_t>(1), legalMoves.size());
    STRCMP_EQUAL("36a-37", legalMoves[0].asString().c_str());

    octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL, 0x5EED2026ULL);
    octi::BotTuningProfile prof = bot.tuningProfile();
    prof.timeBudgetMs = 5000;
    prof.lazySmpThreads = 4;
    prof.useOpeningBook = false;
    bot.setTuningProfile(prof);
    bot.setTimeBudgetMs(5000);

    octi::BotSearchStats stats{};
    std::string chosen = bot.chooseMove(board, &stats);
    STRCMP_EQUAL("36a-37", chosen.c_str());
    CHECK_EQUAL(1, stats.depthReached);
    CHECK_EQUAL(static_cast<std::uint64_t>(1ULL), stats.nodesVisited);
    CHECK_EQUAL(1000000, stats.bestScore);
    CHECK_TRUE(stats.elapsedMs < 500);
    CHECK_EQUAL(1, bot.liveDepthReached());
    CHECK_EQUAL(static_cast<std::uint64_t>(1ULL), bot.liveNodesVisited());
    CHECK_EQUAL(1000000, bot.liveBestScore());
}

TEST(OctiSingleLegalMoveFastPathTest,
     BoundaryMultiMovePositionAndDegenerateZeroMovePosition)
{
    // Boundary 2: Position with >= 2 legal moves still runs full iterative deepening
    {
        octi::OctiBoard multiBoard(octi::GAME_MODE_4);
        multiBoard.fromPositionString(
            "4:2:0:0:P:D:-:0:3,0,10,0:3,0,10,0:3,0,10,0:1,0,10,0:A33a,B37e,C77e,D73");

        octi::OctiGame *game = multiBoard.getGame();
        CHECK_TRUE(game != nullptr);
        std::vector<octi::OctiMove> legalMoves = game->generateLegalMoves();
        CHECK_TRUE(legalMoves.size() >= 2);

        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL, 0x5EED2026ULL);
        octi::BotTuningProfile prof = bot.tuningProfile();
        prof.timeBudgetMs = 150;
        prof.lazySmpThreads = 1;
        prof.useOpeningBook = false;
        bot.setTuningProfile(prof);
        bot.setTimeBudgetMs(150);

        octi::BotSearchStats stats{};
        std::string chosen = bot.chooseMove(multiBoard, &stats);
        CHECK_FALSE(chosen.empty());
        CHECK_TRUE(stats.nodesVisited > 1ULL);
    }

    // Fault / Degenerate: 0 legal moves (unarmed pod with 0 reserve pods/prongs) and finished board
    {
        octi::OctiBoard zeroMoveBoard(octi::GAME_MODE_2, false, false, octi::GAME_BASES_3);
        zeroMoveBoard.fromPositionString(
            "2:3:0:0:P:A:-:0:0,0,0,0:0,0,0,0:A33,B37");
        CHECK_EQUAL(static_cast<std::size_t>(0),
                    zeroMoveBoard.getGame()->generateLegalMoves().size());

        octi::OctiBotEngine bot(octi::BotDifficulty::TACTICAL, 0x5EED2026ULL);
        octi::BotSearchStats stats{};
        std::string chosen = bot.chooseMove(zeroMoveBoard, &stats);
        STRCMP_EQUAL("", chosen.c_str());
        CHECK_EQUAL(0, stats.depthReached);
        CHECK_EQUAL(static_cast<std::uint64_t>(0ULL), stats.nodesVisited);

        octi::OctiBoard unstartedBoard(octi::GAME_MODE_2);
        octi::BotSearchStats unstartedStats{};
        STRCMP_EQUAL("", bot.chooseMove(unstartedBoard, &unstartedStats).c_str());
    }
}

// ---------------------------------------------------------------------------
// OctiNetworkClient Presence Heartbeat, Leave & Additive Parsing Tests
// ---------------------------------------------------------------------------

TEST_GROUP(OctiNetworkClientPresenceTest)
{
};

TEST(OctiNetworkClientPresenceTest,
     AdditiveAndLegacyPresenceParsingAndHeartbeatLeaveTransactions)
{
    using Json = nlohmann::json;

    /* 1. Verify static parsePresenceSseEvent with additive and legacy payloads */
    {
        const std::string richJson = R"({
            "player_uuid": "u-local-4p",
            "handle": "AliceLocal",
            "status": "IN_LOBBY",
            "room_id": "",
            "activity": "PLAYING_LOCAL",
            "activity_detail": "lvl3",
            "local_mode": "4P",
            "local_opponent": "vs_ai",
            "tab_visible": false,
            "active_sessions": 2,
            "clients": ["desktop", "web"],
            "online_sec": 120,
            "idle_sec": 3
        })";
        octi::LobbyPlayerInfo pRich =
            octi::OctiNetworkClient::parsePresenceSseEvent(richJson);
        STRCMP_EQUAL("u-local-4p", pRich.playerUuid.c_str());
        STRCMP_EQUAL("AliceLocal", pRich.handle.c_str());
        STRCMP_EQUAL("IN_LOBBY", pRich.status.c_str());
        STRCMP_EQUAL("PLAYING_LOCAL", pRich.activity.c_str());
        STRCMP_EQUAL("lvl3", pRich.activityDetail.c_str());
        STRCMP_EQUAL("4P", pRich.localMode.c_str());
        STRCMP_EQUAL("vs_ai", pRich.localOpponent.c_str());
        CHECK_FALSE(pRich.tabVisible);
        LONGS_EQUAL(2, pRich.activeSessions);
        LONGS_EQUAL(2, static_cast<long>(pRich.clients.size()));

        const std::string legacyPlayingJson = R"({
            "player_uuid": "u-legacy-play",
            "handle": "BobLegacy",
            "status": "PLAYING",
            "room_id": "room42"
        })";
        octi::LobbyPlayerInfo pLegacy =
            octi::OctiNetworkClient::parsePresenceSseEvent(legacyPlayingJson);
        STRCMP_EQUAL("u-legacy-play", pLegacy.playerUuid.c_str());
        STRCMP_EQUAL("PLAYING", pLegacy.status.c_str());
        STRCMP_EQUAL("room42", pLegacy.roomId.c_str());
        STRCMP_EQUAL("PLAYING_ONLINE", pLegacy.activity.c_str());
        STRCMP_EQUAL("room42", pLegacy.activityDetail.c_str());
        CHECK_TRUE(pLegacy.tabVisible);
        LONGS_EQUAL(1, pLegacy.activeSessions);

        octi::LobbyPlayerInfo pMalformed =
            octi::OctiNetworkClient::parsePresenceSseEvent("{invalid json");
        CHECK_TRUE(pMalformed.playerUuid.empty());
    }

    /* 2. Verify X-Octi-Session-Id header, /api/v1/lobby/state, heartbeat & leave */
    httplib::Server mockSrv;
    std::mutex capMutex;
    std::string lastLobbySessionHdr;
    std::string lastHbSessionHdr;
    Json lastHbBody;
    std::string lastLeaveSessionHdr;
    Json lastLeaveBody;

    mockSrv.Get("/api/v1/lobby/state",
                [&](const httplib::Request &req, httplib::Response &res) {
                    {
                        std::lock_guard<std::mutex> lock(capMutex);
                        lastLobbySessionHdr =
                            req.get_header_value("X-Octi-Session-Id");
                    }
                    Json p1 = {
                        {"player_uuid", "u-1"},
                        {"handle", "DesktopHero"},
                        {"status", "IN_LOBBY"},
                        {"room_id", ""},
                        {"activity", "PLAYING_LOCAL"},
                        {"activity_detail", "lvl2"},
                        {"local_mode", "2P_1B!"},
                        {"local_opponent", "vs_ai"},
                        {"tab_visible", true},
                        {"active_sessions", 1},
                        {"clients", Json::array({"desktop"})},
                        {"online_sec", 45},
                        {"idle_sec", 0}
                    };
                    Json p2 = {
                        {"player_uuid", "u-2"},
                        {"handle", "LegacyObserver"},
                        {"status", "OBSERVING"},
                        {"room_id", "tbl777"}
                    };
                    Json body = {
                        {"players", Json::array({p1, p2})},
                        {"tables", Json::array()},
                        {"recent_chat", Json::array()},
                        {"latest_seq", 9}
                    };
                    res.status = 200;
                    res.set_content(body.dump(), "application/json");
                });

    mockSrv.Post("/api/v1/lobby/presence/heartbeat",
                 [&](const httplib::Request &req, httplib::Response &res) {
                     Json parsed = Json::parse(req.body, nullptr, false);
                     {
                         std::lock_guard<std::mutex> lock(capMutex);
                         lastHbSessionHdr =
                             req.get_header_value("X-Octi-Session-Id");
                         lastHbBody = parsed;
                     }
                     Json resp = {
                         {"ok", true},
                         {"status", "IN_LOBBY"},
                         {"activity", parsed.value("activity", "IN_LOBBY")},
                         {"activity_detail", parsed.value("activity_detail", "")},
                         {"local_mode", parsed.value("local_mode", "")},
                         {"local_opponent", parsed.value("local_opponent", "")},
                         {"tab_visible", parsed.value("visible", true)},
                         {"active_sessions", 1}
                     };
                     res.status = 200;
                     res.set_content(resp.dump(), "application/json");
                 });

    mockSrv.Post("/api/v1/lobby/presence/leave",
                 [&](const httplib::Request &req, httplib::Response &res) {
                     Json parsed = Json::parse(req.body, nullptr, false);
                     {
                         std::lock_guard<std::mutex> lock(capMutex);
                         lastLeaveSessionHdr =
                             req.get_header_value("X-Octi-Session-Id");
                         lastLeaveBody = parsed;
                     }
                     Json resp = {
                         {"ok", true},
                         {"online", false},
                         {"remaining_sessions", 0}
                     };
                     res.status = 200;
                     res.set_content(resp.dump(), "application/json");
                 });

    int port = mockSrv.bind_to_any_port("127.0.0.1");
    CHECK_TRUE(port > 0);
    std::thread srvThread([&mockSrv]() { mockSrv.listen_after_bind(); });
    for (int i = 0; i < 50 && !mockSrv.is_running(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    auto cli = octi::OctiNetworkClient::createForLoopbackTest(port);
    cli->setSessionToken("tok-core-presence");
    cli->setPresenceSessionId("desktop-0123456789abcdef");
    STRCMP_EQUAL("desktop-0123456789abcdef", cli->presenceSessionId().c_str());

    auto stateRes = cli->fetchLobbyState();
    CHECK_TRUE_TEXT(stateRes.ok, stateRes.errorMessage.c_str());
    LONGS_EQUAL(2, static_cast<long>(stateRes.data.players.size()));
    STRCMP_EQUAL("PLAYING_LOCAL", stateRes.data.players[0].activity.c_str());
    STRCMP_EQUAL("2P_1B!", stateRes.data.players[0].localMode.c_str());
    STRCMP_EQUAL("vs_ai", stateRes.data.players[0].localOpponent.c_str());
    STRCMP_EQUAL("OBSERVING", stateRes.data.players[1].activity.c_str());
    STRCMP_EQUAL("tbl777", stateRes.data.players[1].activityDetail.c_str());

    octi::PresenceHeartbeatRequest hbReq;
    hbReq.activity = "PLAYING_LOCAL";
    hbReq.activityDetail = "lvl3";
    hbReq.localMode = "4P";
    hbReq.localOpponent = "vs_ai";
    hbReq.visible = true;
    hbReq.interactive = true;
    auto hbRes = cli->sendPresenceHeartbeat(hbReq);
    CHECK_TRUE_TEXT(hbRes.ok, hbRes.errorMessage.c_str());
    STRCMP_EQUAL("PLAYING_LOCAL", hbRes.data.activity.c_str());
    STRCMP_EQUAL("4P", hbRes.data.localMode.c_str());
    STRCMP_EQUAL("vs_ai", hbRes.data.localOpponent.c_str());

    auto leaveRes = cli->sendPresenceLeave("", "app_close");
    CHECK_TRUE_TEXT(leaveRes.ok, leaveRes.errorMessage.c_str());
    CHECK_FALSE(leaveRes.data.online);
    LONGS_EQUAL(0, leaveRes.data.remainingSessions);

    {
        std::lock_guard<std::mutex> lock(capMutex);
        STRCMP_EQUAL("desktop-0123456789abcdef", lastLobbySessionHdr.c_str());
        STRCMP_EQUAL("desktop-0123456789abcdef", lastHbSessionHdr.c_str());
        STRCMP_EQUAL("desktop-0123456789abcdef",
                     lastHbBody.value("session_id", "").c_str());
        STRCMP_EQUAL("PLAYING_LOCAL",
                     lastHbBody.value("activity", "").c_str());
        CHECK_TRUE(lastHbBody.value("interactive", false));
        STRCMP_EQUAL("desktop-0123456789abcdef", lastLeaveSessionHdr.c_str());
        STRCMP_EQUAL("app_close", lastLeaveBody.value("reason", "").c_str());
    }

    mockSrv.stop();
    if (srvThread.joinable()) {
        srvThread.join();
    }
}

int main(int argc, char **argv)
{
    MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
    return CommandLineTestRunner::RunAllTests(argc, argv);
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
