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
#include "Version.hxx"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <httplib.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <rapidcheck.h>
#include <initializer_list>
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
    // Canonical input: +b-a
    octi::OctiMove m1("33a+b-a");
    CHECK_EQUAL(1, m1.size());
    const auto &ops1 = m1.getOps();
    CHECK_EQUAL(octi::DIR_NE, ops1[0].addProngDir);
    CHECK_EQUAL(octi::DIR_N, ops1[0].rmProngDir);
    STRCMP_EQUAL("33a+b-a", m1.asString().c_str());

    // Reversed input: -a+b -> canonicalizes to +b-a
    octi::OctiMove m2("33a-a+b");
    CHECK_EQUAL(1, m2.size());
    const auto &ops2 = m2.getOps();
    CHECK_EQUAL(octi::DIR_NE, ops2[0].addProngDir);
    CHECK_EQUAL(octi::DIR_N, ops2[0].rmProngDir);
    STRCMP_EQUAL("33a+b-a", m2.asString().c_str());
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
    // Identical add and remove directions
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a+a-a"));

    // Removing non-existent prong
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a+b-c"));

    // Repositioning superprong is forbidden
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a!+b-a"));

    // Adding superprong in reposition is forbidden
    CHECK_THROWS(octi::IllegalMoveSyntaxException, octi::OctiMove("33a+b!-a"));
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
    CHECK_THROWS(octi::IllegalMoveSyntaxException,
                 octi::OctiMove("33a!-34;34a!-35;35a!-36;36a+b")); // >2 bonus moves
}

TEST(OctiMoveTest, RapidCheckMoveCanonicalRoundTrip)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    bool passed = rc::check("OctiMove canonical round trip", []() {
        // Generate random valid moves across different kinds:
        auto kind = *rc::gen::inRange(0, 4); // 0=place, 1=addProng, 2=reposition, 3=hop
        int x0 = *rc::gen::inRange(0, 9);
        int y0 = *rc::gen::inRange(0, 9);

        octi::OctiMoveOp op;
        op.x0 = x0;
        op.y0 = y0;

        if (kind == 0) {
            // Pod place
            op.addPod = true;
            op.liberate = *rc::gen::arbitrary<bool>();
        } else if (kind == 1) {
            // Prong add
            op.addProngDir = static_cast<octi::OctiDir>(*rc::gen::inRange(0, 8));
            op.superProng = *rc::gen::arbitrary<bool>();
        } else if (kind == 2) {
            // Reposition
            int add = *rc::gen::inRange(0, 8);
            int rm = *rc::gen::inRange(0, 7);
            if (rm >= add) rm++;
            op.addProngDir = static_cast<octi::OctiDir>(add);
            op.rmProngDir = static_cast<octi::OctiDir>(rm);
            op.pod.addProng(op.rmProngDir, false); // pod must have rmDir
        } else {
            // Hop
            op.pod.addProng(octi::DIR_N, false);
            octi::OctiHop h;
            h.x = *rc::gen::inRange(0, 9);
            h.y = *rc::gen::inRange(0, 9);
            h.capture = *rc::gen::arbitrary<bool>();
            op.hops.push_back(h);
        }

        octi::OctiMove m({op});
        std::string s = m.asString();
        octi::OctiMove mFromStr(s);

        RC_ASSERT(m == mFromStr);
        RC_ASSERT(m.asString() == mFromStr.asString());
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
    game->move("33b+c-b");
    CHECK_EQUAL(octi::NO_PRONG, board.squareAt(2, 2).getPodAt(0).prongAt(octi::DIR_NE));
    CHECK_EQUAL(octi::NORMAL_PRONG, board.squareAt(2, 2).getPodAt(0).prongAt(octi::DIR_E));
}

TEST(Octi2PlayerGameTest, TurnOptionII_RejectSuperProngReposition)
{
    octi::OctiBoard board(octi::GAME_MODE_2, true);
    octi::OctiGame *game = board.getGame();
    game->start(octi::PLAYER_A);

    game->move("33+b!");
    game->move("37+e");

    CHECK_THROWS(std::runtime_error, game->move("33b!+c-b"));
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

    // Allied pod stacking: Player A and Player C are partners
    board.squareAt(2, 2).addPod(octi::OctiPod(octi::C_POD));
    CHECK_EQUAL(2, board.squareAt(2, 2).getNumPods());
    CHECK_FALSE(board.squareAt(2, 2).hasEnemyOf(octi::PLAYER_A));
    CHECK_FALSE(board.squareAt(2, 2).hasEnemyOf(octi::PLAYER_C));
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
    CHECK_THROWS(octi::IllegalMoveException, game->move("22a+b-a"));
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
    bool passed = rc::check("OctiGame 4P random legal move walk", []() {
        octi::OctiBoard board(octi::GAME_MODE_4);
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
    if (std::filesystem::exists("build/octi_book_gen")) {
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
        " --max-ply 2"
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
        " --no-resume --mode OFK --max-ply 1 --search-depth 1"
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
        "2:3:0:0:P:A:-:0:3,1,23,0:0,5,10,0:B53defgh,B53e,A53aeh,A37,A77");
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
    std::filesystem::path coreRoot =
        std::filesystem::path(__FILE__).parent_path().parent_path();
    if (std::filesystem::exists("build/octi_book_gen")) {
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
    meta.attribution = "Octi game data, Charles Chiou (selfso.com)";
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

    board.squareAt(2, 6).clear();
    board.squareAt(2, 6).addPod(octi::OctiPod(octi::D_POD));

    octi::OctiGameSnapshot snap = game->captureSnapshot();
    snap.turn = octi::PLAYER_D;
    snap.players[octi::PLAYER_B].eliminated = true;
    snap.players[octi::PLAYER_B].capturedPods = 2;
    snap.players[octi::PLAYER_D].capturedPods = 0;
    game->restoreSnapshot(snap);

    std::vector<octi::OctiMove> dMoves = game->generateLegalMoves();
    bool has37Star = false;
    for (const auto &m : dMoves) {
        if (m.asString() == "37*") {
            has37Star = true;
            break;
        }
    }
    CHECK_TRUE(has37Star);

    game->move("37*");
    CHECK_FALSE(game->isPlayerEliminated(octi::PLAYER_B));
    LONGS_EQUAL(1, game->getCapturedPods(octi::PLAYER_B));
    LONGS_EQUAL(2, board.squareAt(2, 6).getNumPods());
    CHECK_EQUAL(octi::B_POD, board.squareAt(2, 6).getPodAt(1).getType());
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
                        "{\"seat\":1,\"handle\":\"samurai\",\"player_uuid\":\"human-uuid-456\"},"
                        "{\"seat\":2,\"handle\":\"octibot-sdk-cpu\",\"is_robot\":true,\"robot_uuid\":\"bot-uuid-123\"}"
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
    STRCMP_EQUAL("samurai", p1.handle.c_str());
    STRCMP_EQUAL("human-uuid-456", p1.playerUuid.c_str());
    CHECK_FALSE(p1.isRobot);
    STRCMP_EQUAL("", p1.robotUuid.c_str());

    const octi::ArchivedGamePlayer &p2 = pageRes.data.games[0].players[1];
    CHECK_EQUAL(2, p2.seat);
    STRCMP_EQUAL("octibot-sdk-cpu", p2.handle.c_str());
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

    // Part B: Two preludes (sequential by same pod, and commuting by different pods).
    octi::OctiBoard twoPrelBoard(octi::GAME_MODE_2, true, false, octi::GAME_BASES_3);
    twoPrelBoard.fromPositionString(
        "2:3:1:0:P:A:-:0:0,0,1,0:0,0,0,0:A33a!,A53a!,B57");
    std::vector<octi::OctiMove> twoPrelMoves;
    twoPrelBoard.getGame()->generateLegalMoves(twoPrelMoves);

    // Same pod takes two sequential step preludes 33 -> 34 -> 35, then +b at 35:
    CHECK_TRUE(containsExactMove(twoPrelMoves, "33a!-34;34a!-35;35a!+b"));

    // Two different pods (A33a! and A53a!) commute: emitted in canonical order
    // (lower source square 33 before 53) and NOT in reversed order (53 before 33):
    CHECK_TRUE(containsExactMove(twoPrelMoves, "33a!-34;53a!-54;34a!+b"));
    CHECK_FALSE(containsExactMove(twoPrelMoves, "53a!-54;33a!-34;34a!+b"));
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
    if (std::filesystem::exists("build/octi_book_gen")) {
        bookGenBin = "./build/octi_book_gen";
    } else if (std::filesystem::exists("build/asan/octi_book_gen")) {
        bookGenBin = "./build/asan/octi_book_gen";
    } else if (std::filesystem::exists("./octi_book_gen")) {
        bookGenBin = "./octi_book_gen";
    } else if (std::filesystem::exists("core/build/octi_book_gen")) {
        bookGenBin = "./core/build/octi_book_gen";
    } else if (!selfEc && std::filesystem::exists(selfDir / "octi_book_gen")) {
        bookGenBin = (selfDir / "octi_book_gen").string();
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
        bookGenBin + " --no-resume --mode 2P_3B --super-prongs --max-ply 1 --search-depth 1"
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
        bookGenBin + " --no-resume --mode all13 --max-ply 1 --search-depth 1"
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
    if (std::filesystem::exists("build/octi_book_gen")) {
        bookGenBin = "./build/octi_book_gen";
    } else if (std::filesystem::exists("build/asan/octi_book_gen")) {
        bookGenBin = "./build/asan/octi_book_gen";
    } else if (std::filesystem::exists("./octi_book_gen")) {
        bookGenBin = "./octi_book_gen";
    } else if (std::filesystem::exists("core/build/octi_book_gen")) {
        bookGenBin = "./core/build/octi_book_gen";
    } else if (!selfEc && std::filesystem::exists(selfDir / "octi_book_gen")) {
        bookGenBin = (selfDir / "octi_book_gen").string();
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
         41596ULL, "73a+h", 2752},
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

    // 2. 1,000 positions from seeded random superprong playouts across the 8 superprong variants (125 each)
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
                    verifyPositionAgainstGolden(board, (collected % 25 == 0));
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

    CHECK_EQUAL(1024u, cursor);
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
        prof.timeBudgetMs = 60000;

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
