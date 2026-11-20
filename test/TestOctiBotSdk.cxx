/*
 * Copyright (C) 2026, Charles Chiou
 */

#include <rapidcheck.h>
#include <nlohmann/json.hpp>
#include <httplib.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <CppUTest/CommandLineTestRunner.h>
#include <CppUTest/TestHarness.h>

#include "AccountStore.hxx"
#include "GameRoomManager.hxx"
#include "OctiBoard.hxx"
#include "OctiBotAgent.hxx"
#include "OctiBotConfig.hxx"
#include "OctiBotEngine.hxx"
#include "OctiBotPolicy.hxx"
#include "OctiBotRunner.hxx"
#include "OctiDefs.hxx"
#include "OctiHttpServer.hxx"
#include "OidcVerifier.hxx"

using namespace octi;

namespace {

struct TestServerEnv {
    std::string dir;
    std::string dbPath;
    AccountStore store;
    OidcVerifier verifier;
    std::unique_ptr<GameRoomManager> rooms;
    std::unique_ptr<OctiHttpServer> server;
    int port = 0;

    TestServerEnv() {
        char tmpl[] = "/tmp/octibot_test_XXXXXX";
        if (::mkdtemp(tmpl) == nullptr) {
            return;
        }
        dir = tmpl;
        dbPath = dir + "/test.db";

        std::string err;
        if (!store.openOrCreate(dbPath, err)) {
            return;
        }

        rooms = std::make_unique<GameRoomManager>(store);

        HttpServerConfig cfg;
        cfg.bindAddress = "127.0.0.1";
        cfg.port = 0;
        cfg.tickIntervalMs = 25;
        cfg.ssePollMs = 15;
        cfg.sseHeartbeatMs = 200;
        cfg.sseSessionRecheckMs = 100;

        server = std::make_unique<OctiHttpServer>(store, verifier, *rooms, cfg);
        if (server->start(err)) {
            port = server->port();
        }
    }

    ~TestServerEnv() {
        if (server) server->stop();
        server.reset();
        rooms.reset();
        store.close();
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
};

} // namespace

// ============================================================================
// 1. OctiBotConfigStore Tests
// ============================================================================

TEST_GROUP(OctiBotConfigStoreTest) {
};

TEST(OctiBotConfigStoreTest, NominalParseAndInheritance) {
    const std::string conf = R"(
# Global defaults
server_host = 127.0.0.1
server_port = 8080
server_use_ssl = false
table_behavior = auto
publish_move_telemetry = false
custom_float = 42.5

[stock]
algorithm = alpha_beta
time_budget_ms = 1500
max_search_depth = 6

[bot_fast]
robot_name = tactician
algorithm = alpha_beta
time_budget_ms = 500
max_search_depth = 4

[bot_deep]
algorithm = alpha_beta
time_budget_ms = 4000
max_search_depth = 8
captured_enemy_base_bonus = 32000
)";

    OctiBotConfigStore store;
    std::string err;
    CHECK_TRUE(store.parseString(conf, err));

    CHECK_EQUAL("127.0.0.1", store.getString("server_host"));
    CHECK_EQUAL(8080, store.getInt("server_port"));
    CHECK_FALSE(store.getBool("server_use_ssl"));
    CHECK_EQUAL(42.5, store.getDouble("custom_float"));

    auto profiles = store.profileNames();
    CHECK_EQUAL(3, profiles.size());

    // Resolve stock profile
    auto stockProf = store.resolveProfile("stock");
    CHECK_EQUAL("stock", stockProf.getString("robot_name"));
    CHECK_EQUAL(1500, stockProf.getInt("time_budget_ms"));
    CHECK_EQUAL(6, stockProf.getInt("max_search_depth"));
    CHECK_EQUAL("127.0.0.1", stockProf.getString("server_host")); // Inherited

    // Resolve bot_fast profile
    auto fastProf = store.resolveProfile("bot_fast");
    CHECK_EQUAL("tactician", fastProf.getString("robot_name"));
    CHECK_EQUAL(500, fastProf.getInt("time_budget_ms"));
    CHECK_EQUAL(4, fastProf.getInt("max_search_depth"));

    // Resolve bot_deep profile
    auto deepProf = store.resolveProfile("bot_deep");
    CHECK_EQUAL("bot_deep", deepProf.getString("robot_name"));
    CHECK_EQUAL(4000, deepProf.getInt("time_budget_ms"));
    CHECK_EQUAL(32000, deepProf.getInt("captured_enemy_base_bonus"));
}

TEST(OctiBotConfigStoreTest, VerbosityLevelParsingAndDefault) {
    OctiBotConfigStore store;
    CHECK_EQUAL(3, store.getVerbosityLevel());
    CHECK_EQUAL(3, store.getVerbosityLevel(3));

    CHECK_EQUAL(3, OctiBotConfigStore::parseVerbosityLevel("verbose"));
    CHECK_EQUAL(3, OctiBotConfigStore::parseVerbosityLevel("debug"));
    CHECK_EQUAL(3, OctiBotConfigStore::parseVerbosityLevel("trace"));
    CHECK_EQUAL(3, OctiBotConfigStore::parseVerbosityLevel("max"));
    CHECK_EQUAL(3, OctiBotConfigStore::parseVerbosityLevel("3"));
    CHECK_EQUAL(3, OctiBotConfigStore::parseVerbosityLevel("VERBOSE"));

    CHECK_EQUAL(2, OctiBotConfigStore::parseVerbosityLevel("info"));
    CHECK_EQUAL(2, OctiBotConfigStore::parseVerbosityLevel("normal"));
    CHECK_EQUAL(2, OctiBotConfigStore::parseVerbosityLevel("2"));

    CHECK_EQUAL(1, OctiBotConfigStore::parseVerbosityLevel("error"));
    CHECK_EQUAL(1, OctiBotConfigStore::parseVerbosityLevel("warn"));
    CHECK_EQUAL(1, OctiBotConfigStore::parseVerbosityLevel("1"));

    CHECK_EQUAL(0, OctiBotConfigStore::parseVerbosityLevel("quiet"));
    CHECK_EQUAL(0, OctiBotConfigStore::parseVerbosityLevel("silent"));
    CHECK_EQUAL(0, OctiBotConfigStore::parseVerbosityLevel("none"));
    CHECK_EQUAL(0, OctiBotConfigStore::parseVerbosityLevel("0"));
    CHECK_EQUAL(0, OctiBotConfigStore::parseVerbosityLevel("false"));

    store.set("verbosity", "quiet");
    CHECK_EQUAL(0, store.getVerbosityLevel());
    store.set("verbosity", "info");
    CHECK_EQUAL(2, store.getVerbosityLevel());
    store.set("verbosity", "error");
    CHECK_EQUAL(1, store.getVerbosityLevel());
    store.set("verbosity", "verbose");
    CHECK_EQUAL(3, store.getVerbosityLevel());
}

TEST(OctiBotConfigStoreTest, RunnerVerbosityAndLogging) {
    OctiBotConfigStore config;
    config.set("verbosity", "verbose");
    auto agent = std::make_unique<StockAlphaBetaAgent>(config);
    OctiBotRunner runner(std::move(agent), config);
    CHECK_EQUAL(3, runner.verbosity());
    runner.setVerbosity(0);
    CHECK_EQUAL(0, runner.verbosity());
}

TEST(OctiBotConfigStoreTest, ApplyTuningProfileMapping) {
    const std::string conf = R"(
time_budget_ms = 2500
max_search_depth = 7
max_quiesce_depth = 3
quiesce_branch_cap = 5
root_branch_cap = 40
tree_branch_cap = 30
lazy_smp_threads = 4
composite_move_max_ply = 2
composite_tactical_only_min_ply = 3
qsearch_include_base_threat_prongs = true
root_score_jitter = 10
book_selection_window = 35
use_opening_book = false
use_pvs = true
aspiration_window_cp = 150
use_killers = true
use_history = true
lmr_min_depth = 3
lmr_min_move_index = 4
null_move_min_depth = 3
null_move_reduction = 3
qsearch_use_tt = true
tt_size_log2 = 20
lazy_smp_diversify_helpers = true
captured_enemy_base_bonus = 30000
lost_allied_base_penalty = 35000
neutral_octi_square_bonus = 4500
closest_pod_dist_weight = 350
second_closest_pod_dist_weight = 160
one_step_base_threat_bonus = 9000
one_jump_base_threat_bonus = 13000
two_ply_base_threat_weight = 2600
contested_enemy_base_threat_scale_pct = 65
empty_allied_base_penalty_1b = 4100
pod_zero_prong_val = 330
pod_one_prong_val = 1400
pod_two_prong_val = 2200
pod_three_plus_prong_val = 2900
normal_prong_val = 400
super_prong_val = 600
orth_diag_versatility_bonus = 580
diag_only_parity_penalty = 360
first_armed_pod_bonus = 650
second_armed_pod_bonus = 1150
third_armed_pod_bonus = 750
single_pod_monopoly_penalty = 1100
ofk_single_pod_monopoly_penalty = 250
lone_one_prong_rush_penalty = 600
enemy_heavy_pod_threat_penalty = 520
unarmed_home_pod_penalty = 850
can_liberate_bonus = 500
reserve_free_pod_val = 470
captured_pod_penalty = 3900
reserve_free_prong_val = 150
fix_stack_and_home_eval_mirage = true
leapfrog_battery_weight = 580
leapfrog_forward_bonus = 420
jump_reachable_square_weight = 140
jump_reach_step_weight = 480
jump_capturable_pod_weight = 650
jump_capturable_prong_weight = 130
hanging_pod_base_penalty = 1500
hanging_prong_weight = 240
ofk_prong_seizure_weight = 480
opponent_turn_hanging_pct = 130
sole_armed_pod_danger_penalty = 3600
wipeout_danger_penalty = 16000
move_order_capture_bonus = 17000
move_order_base_entry_bonus = 62000
move_order_leapfrog_bonus = 1300
move_order_blunder_penalty = 7500
)";

    OctiBotConfigStore store;
    std::string err;
    CHECK_TRUE(store.parseString(conf, err));

    BotTuningProfile profile;
    applyConfigToTuningProfile(store, profile);

    CHECK_EQUAL(2500, profile.timeBudgetMs);
    CHECK_EQUAL(7, profile.maxSearchDepth);
    CHECK_EQUAL(3, profile.maxQuiesceDepth);
    CHECK_EQUAL(5, profile.quiesceBranchCap);
    CHECK_EQUAL(40, profile.rootBranchCap);
    CHECK_EQUAL(30, profile.treeBranchCap);
    CHECK_EQUAL(4, profile.lazySmpThreads);
    CHECK_EQUAL(2, profile.compositeMoveMaxPly);
    CHECK_EQUAL(3, profile.compositeTacticalOnlyMinPly);
    CHECK_TRUE(profile.qsearchIncludeBaseThreatProngs);
    CHECK_EQUAL(10, profile.rootScoreJitter);
    CHECK_EQUAL(35, profile.bookSelectionWindow);
    CHECK_FALSE(profile.useOpeningBook);
    CHECK_TRUE(profile.usePvs);
    CHECK_EQUAL(150, profile.aspirationWindowCp);
    CHECK_TRUE(profile.useKillers);
    CHECK_TRUE(profile.useHistory);
    CHECK_EQUAL(3, profile.lmrMinDepth);
    CHECK_EQUAL(4, profile.lmrMinMoveIndex);
    CHECK_EQUAL(3, profile.nullMoveMinDepth);
    CHECK_EQUAL(3, profile.nullMoveReduction);
    CHECK_TRUE(profile.qsearchUseTt);
    CHECK_EQUAL(20, profile.ttSizeLog2);
    CHECK_TRUE(profile.lazySmpDiversifyHelpers);
    CHECK_EQUAL(30000, profile.capturedEnemyBaseBonus);
    CHECK_EQUAL(35000, profile.lostAlliedBasePenalty);
    CHECK_EQUAL(4500, profile.neutralOctiSquareBonus);
    CHECK_EQUAL(350, profile.closestPodDistWeight);
    CHECK_EQUAL(160, profile.secondClosestPodDistWeight);
    CHECK_EQUAL(9000, profile.oneStepBaseThreatBonus);
    CHECK_EQUAL(13000, profile.oneJumpBaseThreatBonus);
    CHECK_EQUAL(2600, profile.twoPlyBaseThreatWeight);
    CHECK_EQUAL(65, profile.contestedEnemyBaseThreatScalePct);
    CHECK_EQUAL(4100, profile.emptyAlliedBasePenalty1B);
    CHECK_EQUAL(330, profile.podZeroProngVal);
    CHECK_EQUAL(1400, profile.podOneProngVal);
    CHECK_EQUAL(2200, profile.podTwoProngVal);
    CHECK_EQUAL(2900, profile.podThreePlusProngVal);
    CHECK_EQUAL(400, profile.normalProngVal);
    CHECK_EQUAL(600, profile.superProngVal);
    CHECK_EQUAL(580, profile.orthDiagVersatilityBonus);
    CHECK_EQUAL(360, profile.diagOnlyParityPenalty);
    CHECK_EQUAL(650, profile.firstArmedPodBonus);
    CHECK_EQUAL(1150, profile.secondArmedPodBonus);
    CHECK_EQUAL(750, profile.thirdArmedPodBonus);
    CHECK_EQUAL(1100, profile.singlePodMonopolyPenalty);
    CHECK_EQUAL(250, profile.ofkSinglePodMonopolyPenalty);
    CHECK_EQUAL(600, profile.loneOneProngRushPenalty);
    CHECK_EQUAL(520, profile.enemyHeavyPodThreatPenalty);
    CHECK_EQUAL(850, profile.unarmedHomePodPenalty);
    CHECK_EQUAL(500, profile.canLiberateBonus);
    CHECK_EQUAL(470, profile.reserveFreePodVal);
    CHECK_EQUAL(3900, profile.capturedPodPenalty);
    CHECK_EQUAL(150, profile.reserveFreeProngVal);
    CHECK_TRUE(profile.fixStackAndHomeEvalMirage);
    CHECK_EQUAL(580, profile.leapfrogBatteryWeight);
    CHECK_EQUAL(420, profile.leapfrogForwardBonus);
    CHECK_EQUAL(140, profile.jumpReachableSquareWeight);
    CHECK_EQUAL(480, profile.jumpReachStepWeight);
    CHECK_EQUAL(650, profile.jumpCapturablePodWeight);
    CHECK_EQUAL(130, profile.jumpCapturableProngWeight);
    CHECK_EQUAL(1500, profile.hangingPodBasePenalty);
    CHECK_EQUAL(240, profile.hangingProngWeight);
    CHECK_EQUAL(480, profile.ofkProngSeizureWeight);
    CHECK_EQUAL(130, profile.opponentTurnHangingPct);
    CHECK_EQUAL(3600, profile.soleArmedPodDangerPenalty);
    CHECK_EQUAL(16000, profile.wipeoutDangerPenalty);
    CHECK_EQUAL(17000, profile.moveOrderCaptureBonus);
    CHECK_EQUAL(62000, profile.moveOrderBaseEntryBonus);
    CHECK_EQUAL(1300, profile.moveOrderLeapfrogBonus);
    CHECK_EQUAL(7500, profile.moveOrderBlunderPenalty);
}

TEST(OctiBotConfigStoreTest, TacticalAndEvalBugFixFlagsRoundTripAndDefaults) {
    // 1. Empty store preserves upstream default-off values
    OctiBotConfigStore emptyStore;
    BotTuningProfile defProf;
    applyConfigToTuningProfile(emptyStore, defProf);
    CHECK_EQUAL(1, defProf.compositeMoveMaxPly);
    CHECK_EQUAL(0, defProf.compositeTacticalOnlyMinPly);
    CHECK_FALSE(defProf.qsearchIncludeBaseThreatProngs);
    CHECK_FALSE(defProf.fixStackAndHomeEvalMirage);
    CHECK_EQUAL(0, defProf.emptyAlliedBasePenalty1B);

    auto defAgent = std::make_unique<StockAlphaBetaAgent>(emptyStore);
    OctiBotRunner defRunner(std::move(defAgent), emptyStore);
    const auto defEff = defRunner.buildEffectiveConfigMap();
    CHECK_EQUAL("0", defEff.at("composite_tactical_only_min_ply"));
    CHECK_EQUAL("false", defEff.at("qsearch_include_base_threat_prongs"));
    CHECK_EQUAL("false", defEff.at("fix_stack_and_home_eval_mirage"));
    CHECK_EQUAL("0", defEff.at("empty_allied_base_penalty_1b"));

    // 2. Round-trip profile resolution + effectiveDefaults export
    const std::string conf = R"(
composite_move_max_ply = 4
composite_tactical_only_min_ply = 2
qsearch_include_base_threat_prongs = true
fix_stack_and_home_eval_mirage = true
empty_allied_base_penalty_1b = 3800

[one_base_bot]
empty_allied_base_penalty_1b = 4500
)";
    OctiBotConfigStore store;
    std::string err;
    CHECK_TRUE(store.parseString(conf, err));

    auto resolved = store.resolveProfile("one_base_bot");
    BotTuningProfile prof;
    applyConfigToTuningProfile(resolved, prof);
    CHECK_EQUAL(4, prof.compositeMoveMaxPly);
    CHECK_EQUAL(2, prof.compositeTacticalOnlyMinPly);
    CHECK_TRUE(prof.qsearchIncludeBaseThreatProngs);
    CHECK_TRUE(prof.fixStackAndHomeEvalMirage);
    CHECK_EQUAL(4500, prof.emptyAlliedBasePenalty1B);

    auto agent = std::make_unique<StockAlphaBetaAgent>(resolved);
    OctiBotRunner runner(std::move(agent), resolved);
    const auto eff = runner.buildEffectiveConfigMap();
    CHECK_EQUAL("2", eff.at("composite_tactical_only_min_ply"));
    CHECK_EQUAL("true", eff.at("qsearch_include_base_threat_prongs"));
    CHECK_EQUAL("true", eff.at("fix_stack_and_home_eval_mirage"));
    CHECK_EQUAL("4500", eff.at("empty_allied_base_penalty_1b"));
}

TEST(OctiBotConfigStoreTest, TokenPoolAndResolution) {
    const std::string conf = R"(
bot_token = auto
bot_token_stock = octi_bot_1111111111111111111111111111111111111111111111111111111111111111
bot_token_fast = octi_bot_2222222222222222222222222222222222222222222222222222222222222222
)";

    OctiBotConfigStore store;
    std::string err;
    CHECK_TRUE(store.parseString(conf, err));

    auto pool = store.tokenPool();
    CHECK_EQUAL(2, pool.size());
    CHECK_EQUAL("fast", pool[0].first);
    CHECK_EQUAL("stock", pool[1].first);

    CHECK_EQUAL("octi_bot_1111111111111111111111111111111111111111111111111111111111111111",
                store.resolveTokenForRobot("stock"));
    CHECK_EQUAL("octi_bot_2222222222222222222222222222222222222222222222222222222222222222",
                store.resolveTokenForRobot("fast"));
    CHECK_EQUAL("", store.resolveTokenForRobot("unknown"));
}

TEST(OctiBotConfigStoreTest, FileSaveAndPermissions) {
    char tmpl[] = "/tmp/octibot_cfg_test_XXXXXX";
    char *dir = ::mkdtemp(tmpl);
    CHECK_TRUE(dir != nullptr);

    std::string cfgPath = std::string(dir) + "/test.conf";
    OctiBotConfigStore store;
    store.set("key1", "val1");
    store.set("key2", "val2");

    std::string err;
    CHECK_TRUE(store.saveFile(cfgPath, err));

    struct stat st;
    CHECK_EQUAL(0, ::stat(cfgPath.c_str(), &st));
    CHECK_EQUAL(0600, st.st_mode & 0777);

    OctiBotConfigStore loaded;
    CHECK_TRUE(loaded.loadFile(cfgPath, err));
    CHECK_EQUAL("val1", loaded.getString("key1"));
    CHECK_EQUAL("val2", loaded.getString("key2"));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(OctiBotConfigStoreTest, BoundaryAndMalformedSyntax) {
    OctiBotConfigStore store;
    std::string err;

    CHECK_TRUE(store.parseString("", err));
    CHECK_EQUAL(0, store.keys().size());

    CHECK_TRUE(store.parseString("# Comment 1\n; Comment 2\n", err));
    CHECK_EQUAL(0, store.keys().size());

    const std::string quoted = "greeting = \"Hello = World; # Test\"\npath = 'C:\\foo\\bar'";
    CHECK_TRUE(store.parseString(quoted, err));
    CHECK_EQUAL("Hello = World; # Test", store.getString("greeting"));
    CHECK_EQUAL("C:\\foo\\bar", store.getString("path"));

    CHECK_FALSE(store.parseString("invalid_line_without_equals", err));
    CHECK_TRUE(err.find("Missing '='") != std::string::npos);

    CHECK_FALSE(store.parseString("bad = \"unterminated", err));
    CHECK_TRUE(err.find("Unterminated quote") != std::string::npos);

    CHECK_FALSE(store.parseString("[unclosed_section", err));
    CHECK_TRUE(err.find("missing ']'") != std::string::npos);

    store.set("bad_int", "not_a_number");
    CHECK_EQUAL(42, store.getInt("bad_int", 42));
    store.set("bad_double", "invalid_double");
    CHECK_EQUAL(3.14, store.getDouble("bad_double", 3.14));
}

TEST(OctiBotConfigStoreTest, RapidCheckConfigRoundTrip) {
    bool passed = rc::check("OctiBotConfig key-value round-trip invariance", []() {
        int count = *rc::gen::inRange(1, 10);
        OctiBotConfigStore store;
        std::map<std::string, std::string> expected;

        for (int i = 0; i < count; ++i) {
            std::string k = "key_" + std::to_string(i);
            std::string v = "val_" + std::to_string(*rc::gen::inRange(0, 1000));
            store.set(k, v);
            expected[k] = v;
        }

        std::string err;
        char tmpl[] = "/tmp/octibot_rc_XXXXXX";
        char *dir = ::mkdtemp(tmpl);
        RC_ASSERT(dir != nullptr);
        std::string path = std::string(dir) + "/rc.conf";

        RC_ASSERT(store.saveFile(path, err));

        OctiBotConfigStore loaded;
        RC_ASSERT(loaded.loadFile(path, err));

        for (const auto &kv : expected) {
            RC_ASSERT(loaded.getString(kv.first) == kv.second);
        }

        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    });
    CHECK_TRUE(passed);
}

// ============================================================================
// 2. OctiBotPolicyEngine Tests
// ============================================================================

TEST_GROUP(OctiBotPolicyTest) {
};

TEST(OctiBotPolicyTest, TableCompatibilityFilters) {
    OctiBotConfigStore config;
    config.set("accept_modes", "2P, OFK");
    config.set("accept_bases", "3");
    config.set("accept_super_prongs", "false");
    config.set("accept_edgeless", "false");
    config.set("accept_time_control", "any");

    OctiBotPolicyEngine policy(config);

    TableSummaryInfo validTable;
    validTable.mode = "2P";
    validTable.bases = 3;
    validTable.superProngs = false;
    validTable.edgeless = false;
    validTable.allowRobots = true;
    validTable.phase = "WAITING_FOR_PLAYERS";
    validTable.seatsTotal = 2;
    validTable.seatsFilled = 1;

    CHECK_TRUE(policy.isTableCompatible(validTable, "test_owner", "stock"));

    TableSummaryInfo noRobots = validTable;
    noRobots.allowRobots = false;
    CHECK_FALSE(policy.isTableCompatible(noRobots, "test_owner", "stock"));

    TableSummaryInfo mode4p = validTable;
    mode4p.mode = "4P";
    CHECK_FALSE(policy.isTableCompatible(mode4p, "test_owner", "stock"));

    TableSummaryInfo bases1 = validTable;
    bases1.bases = 1;
    CHECK_FALSE(policy.isTableCompatible(bases1, "test_owner", "stock"));

    TableSummaryInfo superP = validTable;
    superP.superProngs = true;
    CHECK_FALSE(policy.isTableCompatible(superP, "test_owner", "stock"));
}

TEST(OctiBotPolicyTest, FriendAndBanListGovernance) {
    OctiBotConfigStore config;
    config.set("friend_list", "alice, bob");
    config.set("ban_list", "troll, griefer");

    OctiBotPolicyEngine policy(config);
    CHECK_TRUE(policy.isFriend("Alice"));
    CHECK_TRUE(policy.isFriend("bob"));
    CHECK_FALSE(policy.isFriend("charlie"));

    CHECK_TRUE(policy.isBanned("Troll"));
    CHECK_TRUE(policy.isBanned("griefer"));
    CHECK_FALSE(policy.isBanned("test_owner"));

    policy.addFriend("charlie");
    CHECK_TRUE(policy.isFriend("charlie"));
    policy.removeFriend("alice");
    CHECK_FALSE(policy.isFriend("alice"));

    policy.addBan("bad_actor");
    CHECK_TRUE(policy.isBanned("bad_actor"));
    policy.removeBan("troll");
    CHECK_FALSE(policy.isBanned("troll"));
}

TEST(OctiBotPolicyTest, TableRankingPriorities) {
    OctiBotConfigStore config;
    config.set("friend_list", "alice");
    OctiBotPolicyEngine policy(config);

    TableSummaryInfo t1;
    t1.roomId = "t1";
    t1.hostHandle = "stranger";
    t1.seatsTotal = 2;
    t1.seatsFilled = 0;
    t1.phase = "WAITING_FOR_PLAYERS";

    TableSummaryInfo t2;
    t2.roomId = "t2";
    t2.hostHandle = "alice";
    t2.seatsTotal = 2;
    t2.seatsFilled = 1;
    t2.phase = "WAITING_FOR_PLAYERS";

    TableSummaryInfo t3;
    t3.roomId = "t3";
    t3.hostHandle = "test_owner";
    t3.seatsTotal = 2;
    t3.seatsFilled = 1;
    t3.phase = "WAITING_FOR_PLAYERS";

    std::vector<TableSummaryInfo> tables = {t1, t2, t3};
    auto ranked = policy.rankCompatibleTables(tables, "test_owner", "stock");

    CHECK_EQUAL(3, ranked.size());
    CHECK_EQUAL("t3", ranked[0].roomId);
    CHECK_EQUAL("t2", ranked[1].roomId);
    CHECK_EQUAL("t1", ranked[2].roomId);
}

TEST(OctiBotPolicyTest, ProposalAcceptanceDecisions) {
    OctiBotConfigStore config;
    config.set("accept_draw_eval_threshold", "-1500");
    config.set("accept_undo", "friends_only");
    config.set("friend_list", "friend1");

    OctiBotPolicyEngine policy(config);

    GameProposalInfo drawProp;
    drawProp.type = "DRAW_OFFER";
    CHECK_FALSE(policy.shouldAcceptProposal(drawProp, "opp", 0, 0));
    CHECK_TRUE(policy.shouldAcceptProposal(drawProp, "opp", -2000, 0));

    GameProposalInfo undoProp;
    undoProp.type = "UNDO_REQUEST";
    CHECK_FALSE(policy.shouldAcceptProposal(undoProp, "stranger", 0, 0));
    CHECK_TRUE(policy.shouldAcceptProposal(undoProp, "friend1", 0, 0));
}

// ============================================================================
// 3. OctiBotConversationEngine Tests
// ============================================================================

TEST_GROUP(OctiBotIntegrationLoopbackTest) {
    std::unique_ptr<TestServerEnv> env;

    void setup() {
        env = std::make_unique<TestServerEnv>();
    }

    void teardown() {
        env.reset();
    }
};

TEST(OctiBotIntegrationLoopbackTest, FullGameAndHumanEloInvariant) {
    VerifiedClaims claims;
    claims.provider = "google";
    claims.sub = "sub-owner1";
    claims.name = "test_owner";
    claims.email = "test_owner@example.com";
    auto loginRes = env->store.loginWithSso(claims, "test-agent");
    int64_t ownerId = loginRes.profile.playerId;
    CHECK_TRUE(ownerId > 0);

    auto r1 = env->store.createRobot(ownerId, "stock", "Stock Bot");
    auto r2 = env->store.createRobot(ownerId, "bot_fast", "Fast Bot");
    CHECK_EQUAL(static_cast<int>(RobotStatus::OK), static_cast<int>(r1.status));
    CHECK_EQUAL(static_cast<int>(RobotStatus::OK), static_cast<int>(r2.status));

    OctiBotConfigStore config;
    config.set("server_host", "127.0.0.1");
    config.set("server_port", std::to_string(env->port));
    config.set("table_behavior", "auto");
    config.set("min_move_think_ms", "0");

    config.set("bot_token_stock", r1.rawToken);
    config.set("bot_token_bot_fast", r2.rawToken);

    OctiBotConfigStore stockConf = config.resolveProfile("stock");
    stockConf.set("table_behavior", "host_only");
    stockConf.set("bot_token", r1.rawToken);

    OctiBotConfigStore fastConf = config.resolveProfile("bot_fast");
    fastConf.set("table_behavior", "join_only");
    fastConf.set("bot_token", r2.rawToken);

    auto stockAgent = std::make_unique<StockAlphaBetaAgent>(stockConf);
    auto stockRunner = std::make_unique<OctiBotRunner>(std::move(stockAgent), stockConf);

    auto fastAgent = std::make_unique<StockAlphaBetaAgent>(fastConf);
    auto fastRunner = std::make_unique<OctiBotRunner>(std::move(fastAgent), fastConf);

    std::string err;
    CHECK_TRUE(stockRunner->connectAndAuthenticate(err));
    CHECK_TRUE(fastRunner->connectAndAuthenticate(err));

    stockRunner->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    fastRunner->start();

    int timeoutLoops = 50;
    while (timeoutLoops-- > 0) {
        if (!stockRunner->currentRoomId().empty() &&
            stockRunner->currentPhase() == "IN_PROGRESS") {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    CHECK_EQUAL("IN_PROGRESS", stockRunner->currentPhase());

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    stockRunner->stop();
    fastRunner->stop();

    PlayerProfile ownerProf;
    CHECK_TRUE(env->store.getProfile(ownerId, ownerProf));
    CHECK_EQUAL(1200, ownerProf.elo2p);
}

TEST(OctiBotIntegrationLoopbackTest, SupersedingTokenTakeoverAndActiveResume) {
    VerifiedClaims claims;
    claims.provider = "google";
    claims.sub = "sub-owner2";
    claims.name = "test_owner2";
    claims.email = "test_owner2@example.com";
    auto loginRes = env->store.loginWithSso(claims, "test-agent");
    int64_t ownerId = loginRes.profile.playerId;

    auto r1 = env->store.createRobot(ownerId, "takeover_bot", "Takeover Bot");
    CHECK_EQUAL(static_cast<int>(RobotStatus::OK), static_cast<int>(r1.status));

    OctiBotConfigStore conf;
    conf.set("server_host", "127.0.0.1");
    conf.set("server_port", std::to_string(env->port));
    conf.set("bot_token", r1.rawToken);
    conf.set("table_behavior", "host_only");

    auto agent1 = std::make_unique<StockAlphaBetaAgent>(conf);
    auto runner1 = std::make_unique<OctiBotRunner>(std::move(agent1), conf);

    std::string err;
    CHECK_TRUE(runner1->connectAndAuthenticate(err));
    runner1->start();

    for (int i = 0; i < 30; ++i) {
        if (!runner1->currentRoomId().empty()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::string roomId = runner1->currentRoomId();
    CHECK_FALSE(roomId.empty());

    // By default (allow_supersede = false), a second runner is blocked from kicking runner1
    auto blockedAgent = std::make_unique<StockAlphaBetaAgent>(conf);
    auto blockedRunner = std::make_unique<OctiBotRunner>(std::move(blockedAgent), conf);
    CHECK_FALSE(blockedRunner->connectAndAuthenticate(err));
    CHECK_FALSE(runner1->isSuperseded());

    // Explicit allow_supersede = true permits superseding takeover and active room resume
    conf.set("allow_supersede", "true");
    auto agent2 = std::make_unique<StockAlphaBetaAgent>(conf);
    auto runner2 = std::make_unique<OctiBotRunner>(std::move(agent2), conf);
    CHECK_TRUE(runner2->connectAndAuthenticate(err));

    CHECK_EQUAL(roomId, runner2->identity().activeRoomId);

    for (int i = 0; i < 30; ++i) {
        if (runner1->isSuperseded()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    CHECK_TRUE(runner1->isSuperseded());

    runner1->stop();
    runner2->stop();
}

TEST(OctiBotIntegrationLoopbackTest, PoliteHostSeatingWaitForHumanAndRandom) {
    // 1. Unit checks on OctiBotPolicyEngine::buildHostTableRequest() and isTableCompatible()
    {
        OctiBotConfigStore cfgDefault;
        OctiBotPolicyEngine polDefault(cfgDefault);
        CHECK_EQUAL(0, polDefault.buildHostTableRequest().seat);

        OctiBotConfigStore cfgSeat1;
        cfgSeat1.set("host_seat", "A");
        OctiBotPolicyEngine polSeat1(cfgSeat1);
        CHECK_EQUAL(1, polSeat1.buildHostTableRequest().seat);

        OctiBotConfigStore cfgSeat2;
        cfgSeat2.set("host_seat", "B");
        OctiBotPolicyEngine polSeat2(cfgSeat2);
        CHECK_EQUAL(2, polSeat2.buildHostTableRequest().seat);

        OctiBotConfigStore cfgRand;
        cfgRand.set("host_seat", "random");
        OctiBotPolicyEngine polRand(cfgRand);
        bool saw1 = false, saw2 = false;
        for (int i = 0; i < 40; ++i) {
            int s = polRand.buildHostTableRequest().seat;
            CHECK_TRUE(s == 1 || s == 2);
            if (s == 1) saw1 = true;
            if (s == 2) saw2 = true;
        }
        CHECK_TRUE(saw1 && saw2);

        TableSummaryInfo ownUnseatedTable;
        ownUnseatedTable.roomId = "room-own";
        ownUnseatedTable.phase = "WAITING_FOR_PLAYERS";
        ownUnseatedTable.mode = "2P";
        ownUnseatedTable.bases = 3;
        ownUnseatedTable.seatsTotal = 2;
        ownUnseatedTable.seatsFilled = 0;
        ownUnseatedTable.allowRobots = true;
        ownUnseatedTable.hostHandle = "polite_bot";
        CHECK_FALSE(polDefault.isTableCompatible(ownUnseatedTable, "test_owner", "polite_bot"));
    }

    // 2. Loopback test: Bot hosts with seat=0 (wait_for_human), human picks Seat 2 (B) -> bot takes Seat 1 (A)
    VerifiedClaims claims;
    claims.provider = "google";
    claims.sub = "sub-polite-owner";
    claims.name = "polite_owner";
    claims.email = "polite@selfso.com";
    auto loginRes = env->store.loginWithSso(claims, "test-agent");
    int64_t humanId = loginRes.profile.playerId;
    CHECK_TRUE(humanId > 0);

    auto rBot = env->store.createRobot(humanId, "polite_bot", "Polite Host Bot");
    CHECK_EQUAL(static_cast<int>(RobotStatus::OK), static_cast<int>(rBot.status));

    OctiBotConfigStore conf;
    conf.set("server_host", "127.0.0.1");
    conf.set("server_port", std::to_string(env->port));
    conf.set("bot_token", rBot.rawToken);
    conf.set("table_behavior", "host_only");
    conf.set("host_seat", "wait_for_human");
    conf.set("min_move_think_ms", "0");

    auto agent = std::make_unique<StockAlphaBetaAgent>(conf);
    auto runner = std::make_unique<OctiBotRunner>(std::move(agent), conf);

    std::string err;
    CHECK_TRUE(runner->connectAndAuthenticate(err));
    runner->start();

    for (int i = 0; i < 40; ++i) {
        if (!runner->currentRoomId().empty()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::string roomId = runner->currentRoomId();
    CHECK_FALSE(roomId.empty());
    CHECK_EQUAL(0, runner->currentSeat());

    // Human explicitly sits in Seat 2 (Player B); bot should automatically sit in Seat 1 (Player A)
    RoomResult rRes = env->rooms->joinTable(roomId, humanId, 2, "");
    CHECK_EQUAL(static_cast<int>(RoomCode::OK), static_cast<int>(rRes.code));
    CHECK_EQUAL(2, rRes.seat);
    CHECK_FALSE(rRes.gameStarted);

    for (int i = 0; i < 50; ++i) {
        if (runner->currentPhase() == "IN_PROGRESS" && runner->currentSeat() == 1) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    CHECK_EQUAL("IN_PROGRESS", runner->currentPhase());
    CHECK_EQUAL(1, runner->currentSeat());

    runner->stop();
}

namespace {

#if !defined(__linux__)
static double getProcessCpuTimeMs() {
#if defined(_WIN32)
    FILETIME createTime{}, exitTime{}, kernelTime{}, userTime{};
    if (GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER k{}, u{};
        k.LowPart = kernelTime.dwLowDateTime;
        k.HighPart = kernelTime.dwHighDateTime;
        u.LowPart = userTime.dwLowDateTime;
        u.HighPart = userTime.dwHighDateTime;
        return static_cast<double>(k.QuadPart + u.QuadPart) / 10000.0;
    }
    return 0.0;
#else
    struct timespec ts{};
    if (::clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0) {
        return static_cast<double>(ts.tv_sec) * 1000.0 +
               static_cast<double>(ts.tv_nsec) / 1000000.0;
    }
    return 0.0;
#endif
}
#endif

struct LoopbackCountingProxy {
    int targetPort = 0;
    int listenFd = -1;
    int port = 0;
    std::atomic<bool> running{false};
    std::atomic<int> restPollCount{0};
    std::atomic<int> acceptedConnCount{0};
    std::atomic<std::int64_t> clientBytesSent{0};
    std::mutex fdsMutex;
    std::vector<int> activeFds;
    std::set<int> proxyTids;
    std::thread acceptThread;
    std::vector<std::thread> workerThreads;

    explicit LoopbackCountingProxy(int upstreamPort) : targetPort(upstreamPort) {
        listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (::bind(listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0 &&
            ::listen(listenFd, 32) == 0) {
            socklen_t len = sizeof(addr);
            if (::getsockname(listenFd, reinterpret_cast<sockaddr *>(&addr), &len) == 0) {
                port = ntohs(addr.sin_port);
                running.store(true);
                acceptThread = std::thread([this]() {
                    {
                        std::lock_guard<std::mutex> lock(fdsMutex);
                        proxyTids.insert( static_cast<int>(::gettid()) );
                    }
                    acceptLoop();
                });
            }
        }
    }

    ~LoopbackCountingProxy() {
        stop();
    }

    bool isProxyTid(int tid) {
        std::lock_guard<std::mutex> lock(fdsMutex);
        return proxyTids.count(tid) > 0;
    }

    void acceptLoop() {
        while (running.load()) {
            sockaddr_in cliAddr{};
            socklen_t cliLen = sizeof(cliAddr);
            int clientFd = ::accept(listenFd, reinterpret_cast<sockaddr *>(&cliAddr), &cliLen);
            if (clientFd < 0) {
                break;
            }
            acceptedConnCount.fetch_add(1);
            int upstreamFd = ::socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in upAddr{};
            upAddr.sin_family = AF_INET;
            upAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            upAddr.sin_port = htons(static_cast<uint16_t>(targetPort));
            if (::connect(upstreamFd, reinterpret_cast<sockaddr *>(&upAddr), sizeof(upAddr)) < 0) {
                ::close(clientFd);
                ::close(upstreamFd);
                continue;
            }

            std::lock_guard<std::mutex> lock(fdsMutex);
            if (!running.load()) {
                ::close(clientFd);
                ::close(upstreamFd);
                break;
            }
            activeFds.push_back(clientFd);
            activeFds.push_back(upstreamFd);

            workerThreads.emplace_back([this, clientFd, upstreamFd]() {
                {
                    std::lock_guard<std::mutex> tlock(fdsMutex);
                    proxyTids.insert(static_cast<int>(::gettid()));
                }
                char buf[4096];
                while (running.load()) {
                    ssize_t n = ::recv(clientFd, buf, sizeof(buf), 0);
                    if (n <= 0) break;
                    clientBytesSent.fetch_add(static_cast<std::int64_t>(n));
                    std::string chunk(buf, static_cast<std::size_t>(n));
                    std::size_t pos = 0;
                    while ((pos = chunk.find("GET /api/v1/", pos)) != std::string::npos) {
                        std::size_t lineEnd = chunk.find("\r\n", pos);
                        std::string reqLine = (lineEnd == std::string::npos)
                            ? chunk.substr(pos)
                            : chunk.substr(pos, lineEnd - pos);
                        if (reqLine.find("events?stream=1") == std::string::npos &&
                            (reqLine.find("GET /api/v1/games/") == 0 ||
                             reqLine.find("GET /api/v1/tables") == 0 ||
                             reqLine.find("GET /api/v1/lobby/state") == 0 ||
                             reqLine.find("GET /api/v1/health") == 0)) {
                            restPollCount.fetch_add(1);
                        }
                        pos += 12;
                    }
                    ssize_t sent = 0;
                    while (sent < n) {
                        ssize_t w = ::send(upstreamFd, buf + sent, static_cast<std::size_t>(n - sent), MSG_NOSIGNAL);
                        if (w <= 0) break;
                        sent += w;
                    }
                    if (sent < n) break;
                }
                ::shutdown(upstreamFd, SHUT_RDWR);
                ::shutdown(clientFd, SHUT_RDWR);
            });
            workerThreads.emplace_back([this, clientFd, upstreamFd]() {
                {
                    std::lock_guard<std::mutex> tlock(fdsMutex);
                    proxyTids.insert(static_cast<int>(::gettid()));
                }
                char buf[4096];
                while (running.load()) {
                    ssize_t n = ::recv(upstreamFd, buf, sizeof(buf), 0);
                    if (n <= 0) break;
                    ssize_t sent = 0;
                    while (sent < n) {
                        ssize_t w = ::send(clientFd, buf + sent, static_cast<std::size_t>(n - sent), MSG_NOSIGNAL);
                        if (w <= 0) break;
                        sent += w;
                    }
                    if (sent < n) break;
                }
                ::shutdown(clientFd, SHUT_RDWR);
                ::shutdown(upstreamFd, SHUT_RDWR);
            });
        }
    }

    void stop() {
        if (!running.exchange(false)) {
            return;
        }
        if (listenFd >= 0) {
            ::shutdown(listenFd, SHUT_RDWR);
            ::close(listenFd);
            listenFd = -1;
        }
        {
            std::lock_guard<std::mutex> lock(fdsMutex);
            for (int fd : activeFds) {
                ::shutdown(fd, SHUT_RDWR);
            }
        }
        if (acceptThread.joinable()) {
            acceptThread.join();
        }
        std::vector<std::thread> threadsToJoin;
        std::vector<int> fdsToClose;
        {
            std::lock_guard<std::mutex> lock(fdsMutex);
            threadsToJoin.swap(workerThreads);
            fdsToClose.swap(activeFds);
        }
        for (auto &t : threadsToJoin) {
            if (t.joinable()) {
                t.join();
            }
        }
        for (int fd : fdsToClose) {
            ::close(fd);
        }
    }
};

static std::set<int> snapshotCurrentProcessTids() {
    std::set<int> tids;
#if defined(__linux__)
    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator("/proc/self/task", ec)) {
        if (ec) break;
        std::string name = entry.path().filename().string();
        int tid = std::atoi(name.c_str());
        if (tid > 0) {
            tids.insert(tid);
        }
    }
#endif
    return tids;
}

static double getBotThreadsCpuTimeMs(const std::set<int> &excludedBaselineTids,
                                     LoopbackCountingProxy &proxy,
                                     int *outMeasuredThreads = nullptr) {
    int count = 0;
    double totalMs = 0.0;
#if defined(__linux__)
    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator("/proc/self/task", ec)) {
        if (ec) break;
        std::string name = entry.path().filename().string();
        int tid = std::atoi(name.c_str());
        if (tid <= 0 || excludedBaselineTids.count(tid) > 0 || proxy.isProxyTid(tid)) {
            continue;
        }
        clockid_t cid = static_cast<clockid_t>((~static_cast<std::uint32_t>(tid) << 3) | 6u);
        struct timespec ts{};
        if (::clock_gettime(cid, &ts) == 0) {
            ++count;
            totalMs += static_cast<double>(ts.tv_sec) * 1000.0 +
                       static_cast<double>(ts.tv_nsec) / 1000000.0;
        }
    }
#else
    (void) excludedBaselineTids;
    (void) proxy;
    count = 5;
    totalMs = getProcessCpuTimeMs();
#endif
    if (outMeasuredThreads) {
        *outMeasuredThreads = count;
    }
    return totalMs;
}

} // namespace

TEST(OctiBotIntegrationLoopbackTest, SeatedWaitingBotZeroIdleRestPollingAndInstantSseWakeup) {
    VerifiedClaims claims;
    claims.provider = "google";
    claims.sub = "sub-event-driven-owner";
    claims.name = "event_owner";
    claims.email = "event_owner@selfso.com";
    auto loginRes = env->store.loginWithSso(claims, "test-agent");
    int64_t humanId = loginRes.profile.playerId;
    CHECK_TRUE(humanId > 0);

    auto rBot = env->store.createRobot(humanId, "idle_zero_bot", "Idle Zero Poll Bot");
    CHECK_EQUAL(static_cast<int>(RobotStatus::OK), static_cast<int>(rBot.status));

    auto rLobby = env->store.createRobot(humanId, "idle_lobby_bot", "Idle Lobby-Only Bot");
    CHECK_EQUAL(static_cast<int>(RobotStatus::OK), static_cast<int>(rLobby.status));

    LoopbackCountingProxy proxy(env->port);
    CHECK_TRUE(proxy.port > 0);
    std::set<int> baselineTids = snapshotCurrentProcessTids();

    OctiBotConfigStore conf;
    conf.set("server_host", "127.0.0.1");
    conf.set("server_port", std::to_string(proxy.port));
    conf.set("bot_token", rBot.rawToken);
    conf.set("table_behavior", "host_only");
    conf.set("host_seat", "A");
    conf.set("min_move_think_ms", "0");

    auto agent = std::make_unique<StockAlphaBetaAgent>(conf);
    auto runner = std::make_unique<OctiBotRunner>(std::move(agent), conf);

    OctiBotConfigStore lobbyConf;
    lobbyConf.set("server_host", "127.0.0.1");
    lobbyConf.set("server_port", std::to_string(proxy.port));
    lobbyConf.set("bot_token", rLobby.rawToken);
    lobbyConf.set("lobby_only", "true");
    lobbyConf.set("table_behavior", "lobby_only");

    auto lobbyAgent = std::make_unique<StockAlphaBetaAgent>(lobbyConf);
    auto lobbyRunner = std::make_unique<OctiBotRunner>(std::move(lobbyAgent), lobbyConf);
    CHECK_TRUE(lobbyRunner->isLobbyOnly());

    std::string err;
    CHECK_TRUE(runner->connectAndAuthenticate(err));
    CHECK_TRUE(lobbyRunner->connectAndAuthenticate(err));
    runner->start();
    lobbyRunner->start();

    for (int i = 0; i < 60; ++i) {
        if (!runner->currentRoomId().empty() &&
            runner->currentPhase() == "WAITING_FOR_PLAYERS" &&
            runner->networkClient().isLobbyEventStreamActive() &&
            runner->networkClient().isGameEventStreamActive() &&
            lobbyRunner->networkClient().isLobbyEventStreamActive()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    std::string roomId = runner->currentRoomId();
    CHECK_FALSE(roomId.empty());
    CHECK_EQUAL("WAITING_FOR_PLAYERS", runner->currentPhase());
    CHECK_TRUE(runner->networkClient().isLobbyEventStreamActive());
    CHECK_TRUE(runner->networkClient().isGameEventStreamActive());
    CHECK_TRUE(lobbyRunner->networkClient().isLobbyEventStreamActive());

    // Allow initial table-host SSE event and first GameView fetch to settle
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    proxy.restPollCount.store(0);
    proxy.acceptedConnCount.store(0);
    proxy.clientBytesSent.store(0);

    // 1. Nominal Idle Network & CPU Regression Check:
    //    Over a 1,500 ms idle window with both a seated WAITING_FOR_PLAYERS bot and a lobby_only bot:
    //    - 0 REST polls, 0 new TCP accepts, 0 client-to-server bytes sent
    //    - All 5 bot threads (2 runner loops + 3 SSE client streams) consume < 10 ms CPU combined (< 0.67% of 1 core)
    int measuredBotThreadsBefore = 0;
    int measuredBotThreadsAfter = 0;
    double botCpuBeforeMs = getBotThreadsCpuTimeMs(baselineTids, proxy, &measuredBotThreadsBefore);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    double idleBotCpuDeltaMs = getBotThreadsCpuTimeMs(baselineTids, proxy, &measuredBotThreadsAfter) - botCpuBeforeMs;

    CHECK_EQUAL(0, proxy.restPollCount.load());
    CHECK_EQUAL(0, proxy.acceptedConnCount.load());
    CHECK_EQUAL(0, static_cast<int>(proxy.clientBytesSent.load()));
    CHECK_TRUE(measuredBotThreadsBefore >= 5);
    CHECK_TRUE(measuredBotThreadsAfter >= 5);
    DOUBLES_EQUAL(0.0, idleBotCpuDeltaMs, 10.0);

    // 2. Boundary: MCP waitForEvent() remains isolated from internal runner wakeups and does not trigger REST polls
    proxy.restPollCount.store(0);
    runner->notifyEvent("MCP_EXTERNAL_PING");
    std::string mcpReason;
    CHECK_TRUE(runner->waitForEvent(50, mcpReason));
    CHECK_EQUAL("MCP_EXTERNAL_PING", mcpReason);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    CHECK_EQUAL(0, proxy.restPollCount.load());

    // 3. Nominal SSE wakeup: human joins Seat 2 -> runner wakes via SSE and transitions to IN_PROGRESS in < 100 ms
    auto tJoin0 = std::chrono::steady_clock::now();
    RoomResult rRes = env->rooms->joinTable(roomId, humanId, 2, "");
    CHECK_EQUAL(static_cast<int>(RoomCode::OK), static_cast<int>(rRes.code));
    CHECK_TRUE(rRes.gameStarted);

    for (int i = 0; i < 100; ++i) {
        if (runner->currentPhase() == "IN_PROGRESS") {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto wakeElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - tJoin0).count();
    CHECK_EQUAL("IN_PROGRESS", runner->currentPhase());
    CHECK_TRUE(wakeElapsedMs < 100);

    // 4. Fault injection: superseding robot token emits SSE closed(ROBOT_TOKEN_SUPERSEDED) and immediately halts runner
    OctiNetworkClient takeoverClient;
    takeoverClient.setEndpoint("127.0.0.1", env->port);
    takeoverClient.setBotToken(rBot.rawToken);
    auto tkRes = takeoverClient.connectRobot(false);
    CHECK_TRUE(tkRes.ok);

    for (int i = 0; i < 60; ++i) {
        if (runner->isSuperseded()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK_TRUE(runner->isSuperseded());

    lobbyRunner->stop();
    runner->stop();
    takeoverClient.disconnectRobot();
    proxy.stop();
}

namespace {

class CustomGreetingOnStartAgent : public StockAlphaBetaAgent {
public:
    using StockAlphaBetaAgent::StockAlphaBetaAgent;

    std::atomic<int> startCalls{0};
    std::atomic<int> lastSeat{0};
    std::mutex mu;
    std::string lastRoomId;
    std::string greetingText = "Custom agent ready to play!";

    void onGameStarted(const GameViewData &game,
                       int mySeat,
                       std::string &outGreetingChat) override {
        ++startCalls;
        lastSeat.store(mySeat);
        {
            std::lock_guard<std::mutex> lock(mu);
            lastRoomId = game.roomId;
        }
        outGreetingChat = greetingText;
    }
};

} // namespace

TEST(OctiBotIntegrationLoopbackTest, OnGameStartedCallbackPostsGreetingOnceAndNotOnReconnect) {
    VerifiedClaims ownerClaims;
    ownerClaims.provider = "google";
    ownerClaims.sub = "sub-ongamestart-owner";
    ownerClaims.name = "owner_start";
    ownerClaims.email = "owner_start@example.com";
    auto ownerLogin = env->store.loginWithSso(ownerClaims, "test-agent");
    CHECK_TRUE(ownerLogin.profile.playerId > 0);

    auto botRec = env->store.createRobot(ownerLogin.profile.playerId, "starter_greet_bot", "Starter Greet Bot");
    CHECK_EQUAL(static_cast<int>(RobotStatus::OK), static_cast<int>(botRec.status));

    OctiNetworkClient humanClient;
    humanClient.setEndpoint("127.0.0.1", env->port);
    humanClient.setSessionToken(ownerLogin.sessionToken);

    CreateTableRequest req;
    req.mode = "2P";
    req.bases = 3;
    req.allowRobots = true;
    req.seat = 1; // Human moves first (ply 0)
    auto created = humanClient.createTable(req);
    CHECK_TRUE(created.ok);
    const std::string roomId = created.data.roomId;
    CHECK_FALSE(roomId.empty());

    OctiBotConfigStore cfg;
    cfg.set("server_host", "127.0.0.1");
    cfg.set("server_port", std::to_string(env->port));
    cfg.set("robot_name", "starter_greet_bot");
    cfg.set("bot_token", botRec.rawToken);
    cfg.set("table_behavior", "idle");
    cfg.set("allow_supersede", "true");
    cfg.set("verbosity", "0");
    cfg.set("max_search_depth", "2");
    cfg.set("time_budget_ms", "100");
    cfg.set("announce_move_search_chat", "false");

    auto agent1UPtr = std::make_unique<CustomGreetingOnStartAgent>(cfg);
    CustomGreetingOnStartAgent *agent1 = agent1UPtr.get();
    OctiBotRunner runner1(std::move(agent1UPtr), cfg);
    std::string err;
    CHECK_TRUE_TEXT(runner1.connectAndAuthenticate(err), err.c_str());
    runner1.start();

    int seat = 0;
    CHECK_TRUE_TEXT(runner1.joinRoomById(roomId, 2, seat, err), err.c_str());
    CHECK_EQUAL(2, seat);

    auto countChatOccurrences = [&](const std::string &needle) -> int {
        auto evs = humanClient.pollGameEvents(roomId, 0, 200);
        if (!evs.ok) {
            return 0;
        }
        int cnt = 0;
        for (const auto &ev : evs.data) {
            if (ev.event == "CHAT" || ev.event == "BOARD_CHAT") {
                try {
                    auto j = nlohmann::json::parse(ev.dataJson);
                    if (j.value("handle", "") == "starter_greet_bot" &&
                        j.value("text", "").find(needle) != std::string::npos) {
                        ++cnt;
                    }
                } catch (...) {}
            }
        }
        return cnt;
    };

    for (int i = 0; i < 100; ++i) {
        if (agent1->startCalls.load() >= 1 && countChatOccurrences("Custom agent ready to play!") >= 1) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK_EQUAL(1, agent1->startCalls.load());
    CHECK_EQUAL(2, agent1->lastSeat.load());
    {
        std::lock_guard<std::mutex> lock(agent1->mu);
        CHECK_EQUAL(roomId, agent1->lastRoomId);
    }
    CHECK_EQUAL(1, countChatOccurrences("Custom agent ready to play!"));
    // Base fallback greeting must not double-post when agent supplied its own greeting
    CHECK_EQUAL(0, countChatOccurrences("Good luck and have fun!"));

    // Human makes ply 1 so the room is mid-game, then runner1 stops and runner2 reconnects
    auto gv = humanClient.fetchGameView(roomId);
    CHECK_TRUE(gv.ok);
    OctiBoard b(GAME_MODE_2, false, false, GAME_BASES_3);
    b.fromPositionString(gv.data.fen);
    auto moves = b.getGame()->generateLegalMoves();
    CHECK_FALSE(moves.empty());
    CHECK_TRUE(humanClient.submitMove(roomId, moves.front().asString()).ok);

    // Wait briefly for runner1 to process move/respond, then stop runner1 and reconnect runner2
    for (int i = 0; i < 60; ++i) {
        auto gvCheck = humanClient.fetchGameView(roomId);
        if (gvCheck.ok && gvCheck.data.plyCount >= 2) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    runner1.stop();

    auto agent2UPtr = std::make_unique<CustomGreetingOnStartAgent>(cfg);
    CustomGreetingOnStartAgent *agent2 = agent2UPtr.get();
    OctiBotRunner runner2(std::move(agent2UPtr), cfg);
    CHECK_TRUE_TEXT(runner2.connectAndAuthenticate(err), err.c_str());
    CHECK_EQUAL(roomId, runner2.currentRoomId());
    runner2.start();

    for (int i = 0; i < 80; ++i) {
        if (agent2->startCalls.load() >= 1) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // New runner initializes its agent via onGameStarted, but does NOT emit a duplicate greeting mid-game
    CHECK_EQUAL(1, agent2->startCalls.load());
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    CHECK_EQUAL(1, countChatOccurrences("Custom agent ready to play!"));

    runner2.stop();
}

// ============================================================================
// 6. OctiBotLogBuffer Tests
// ============================================================================

TEST_GROUP(OctiBotLogBufferTest) {
};

TEST(OctiBotLogBufferTest, RingBufferCapacityAndEviction) {
    OctiBotConfigStore config;
    auto agent = std::make_unique<StockAlphaBetaAgent>(config);
    OctiBotRunner runner(std::move(agent), config);

    for (int i = 1; i <= 5100; ++i) {
        runner.appendStdoutLine("Log line " + std::to_string(i), false);
    }

    uint64_t oldest = 0;
    uint64_t latest = 0;
    auto allLogs = runner.getLogsAfter(0, &oldest, &latest);
    CHECK_EQUAL(5000, allLogs.size());
    CHECK_EQUAL(102, oldest);
    CHECK_EQUAL(5101, latest);
    CHECK_EQUAL("Log line 101", allLogs.front().line);
    CHECK_EQUAL("Log line 5100", allLogs.back().line);

    auto recentLogs = runner.getLogsAfter(5091, &oldest, &latest);
    CHECK_EQUAL(10, recentLogs.size());
    CHECK_EQUAL(5092, recentLogs.front().seq);
    CHECK_EQUAL(5101, recentLogs.back().seq);
}

// ============================================================================
// 7. OctiBotMoveTelemetry Tests
// ============================================================================

TEST_GROUP(OctiBotMoveTelemetryTest) {
};

TEST(OctiBotMoveTelemetryTest, AnnounceMoveSearchChatFlagAndHistoryCap) {
    OctiBotConfigStore config;
    CHECK_TRUE(config.getBool("announce_move_search_chat", true));

    auto agent = std::make_unique<StockAlphaBetaAgent>(config);
    OctiBotRunner runner(std::move(agent), config);
    CHECK_TRUE(runner.announceMoveSearchChat());

    std::map<std::string, std::string> updates;
    updates["announce_move_search_chat"] = "false";
    std::string err;
    CHECK_TRUE(runner.applyConfigUpdates(updates, false, err));
    CHECK_FALSE(runner.announceMoveSearchChat());

    for (int i = 1; i <= 60; ++i) {
        BotMoveTelemetry telem;
        telem.depthReached = 4;
        telem.nodesVisited = 100 * i;
        telem.evalScore = 10;
        telem.elapsedMs = 50;
        telem.present = true;
        runner.recordMoveHistory(i, "move" + std::to_string(i), telem);
    }

    auto history = runner.getMoveHistory();
    CHECK_EQUAL(50, history.size());
    CHECK_EQUAL(11, history.front().ply);
    CHECK_EQUAL(60, history.back().ply);
}

// ============================================================================
// 8. OctiBotConfigHotReload Tests
// ============================================================================

TEST_GROUP(OctiBotConfigHotReloadTest) {
};

TEST(OctiBotConfigHotReloadTest, LockedKeysRejectionAndHotReload) {
    OctiBotConfigStore config;
    auto agent = std::make_unique<StockAlphaBetaAgent>(config);
    OctiBotRunner runner(std::move(agent), config);

    std::map<std::string, std::string> badUpdates;
    badUpdates["server_host"] = "evil.com";
    std::string err;
    CHECK_FALSE(runner.applyConfigUpdates(badUpdates, false, err));
    CHECK_TRUE(err.find("server_host") != std::string::npos);

    badUpdates.clear();
    badUpdates["bot_token"] = "stolen";
    CHECK_FALSE(runner.applyConfigUpdates(badUpdates, false, err));

    std::map<std::string, std::string> validUpdates;
    validUpdates["verbosity"] = "1";
    validUpdates["table_behavior"] = "auto";
    validUpdates["publish_move_telemetry"] = "true";
    CHECK_TRUE(runner.applyConfigUpdates(validUpdates, false, err));
    CHECK_EQUAL(1, runner.verbosity());
    CHECK_TRUE(runner.publishMoveTelemetry());
}


namespace {
class TestMutableProfileAgent : public StockAlphaBetaAgent {
public:
    using StockAlphaBetaAgent::StockAlphaBetaAgent;

    int overrideMaxDepth = 2;
    int overrideTimeBudgetMs = 350;

    std::string computeMove(const OctiBoard &board,
                            const GameViewData &game,
                            int mySeat,
                            std::int64_t remainingClockMs,
                            BotMoveTelemetry *outTelemetry = nullptr) override {
        mutableProfile().maxSearchDepth = overrideMaxDepth;
        mutableProfile().timeBudgetMs = overrideTimeBudgetMs;
        return StockAlphaBetaAgent::computeMove(board, game, mySeat, remainingClockMs, outTelemetry);
    }
};

} // namespace

TEST_GROUP(OctiBotSdkExtensionHooks) {
};

TEST(OctiBotSdkExtensionHooks, H3StockAlphaBetaMutableProfileOverride) {
    OctiBotConfigStore cfg;
    cfg.set("max_search_depth", "6");
    cfg.set("time_budget_ms", "5000");
    cfg.set("use_opening_book", "false");

    TestMutableProfileAgent agent(cfg);
    CHECK_EQUAL(6, agent.profile().maxSearchDepth);
    CHECK_EQUAL(5000, agent.profile().timeBudgetMs);

    agent.overrideMaxDepth = 2;
    agent.overrideTimeBudgetMs = 350;

    OctiBoard board(GAME_MODE_2);
    board.getGame()->start(PLAYER_A);
    GameViewData game;
    game.mode = "2P";
    game.bases = 3;
    game.timeControlEnabled = false;

    BotMoveTelemetry telem;
    std::string mv = agent.computeMove(board, game, 1, 0, &telem);
    CHECK_FALSE(mv.empty());
    CHECK_EQUAL(2, agent.profile().maxSearchDepth);
    CHECK_EQUAL(350, agent.profile().timeBudgetMs);
    CHECK_EQUAL(2, agent.engine().tuningProfile().maxSearchDepth);
    CHECK_EQUAL(350, agent.engine().tuningProfile().timeBudgetMs);

    // Timed-game clock cap still clamps a larger mutableProfile().timeBudgetMs
    agent.overrideTimeBudgetMs = 10000;
    game.timeControlEnabled = true;
    game.incrementMs = 0;
    std::string timedMv = agent.computeMove(board, game, 1, 5000, &telem);
    CHECK_FALSE(timedMv.empty());
    CHECK_EQUAL(200, agent.engine().tuningProfile().timeBudgetMs);
}

// ============================================================================
// Envelope 4: OctiBotHostVariants Tests
// ============================================================================

TEST_GROUP(OctiBotHostVariants) {
};

TEST(OctiBotHostVariants, RoundRobinAll13VariantsAndWrapAround) {
    struct ExpectedVariant {
        const char *name;
        const char *mode;
        int bases;
        bool superProngs;
        bool edgeless;
    };
    static const ExpectedVariant kAll13[13] = {
        {"2P_3B",      "2P",  3, false, false},
        {"2P_3B_SP",   "2P",  3, true,  false},
        {"2P_3B_ED",   "2P",  3, false, true},
        {"2P_3B_SPED", "2P",  3, true,  true},
        {"2P_1B",      "2P",  1, false, false},
        {"2P_1B_SP",   "2P",  1, true,  false},
        {"2P_1B_ED",   "2P",  1, false, true},
        {"2P_1B_SPED", "2P",  1, true,  true},
        {"4P",         "4P",  1, false, false},
        {"4P_SP",      "4P",  1, true,  false},
        {"4P_ED",      "4P",  1, false, true},
        {"4P_SPED",    "4P",  1, true,  true},
        {"OFK",        "OFK", 1, false, false}
    };

    const std::string confText =
        "host_variants = 2P_3B, 2P_3B_SP, 2P_3B_ED, 2P_3B_SPED, "
        "2P_1B, 2P_1B_SP, 2P_1B_ED, 2P_1B_SPED, "
        "4P, 4P_SP, 4P_ED, 4P_SPED, OFK\n";

    OctiBotConfigStore cfg;
    std::string err;
    CHECK_TRUE_TEXT(cfg.parseString(confText, err), err.c_str());

    OctiBotPolicyEngine policy;
    CHECK_TRUE_TEXT(policy.configure(cfg, err), err.c_str());
    CHECK_EQUAL(13u, policy.hostVariants().size());

    // Two complete cycles (26 hosted tables) verify round-robin order and wrap-around
    for (int step = 0; step < 26; ++step) {
        const ExpectedVariant &exp = kAll13[step % 13];
        CreateTableRequest req = policy.buildHostTableRequest();
        CHECK_EQUAL(std::string(exp.mode), req.mode);
        CHECK_EQUAL(exp.bases, req.bases);
        CHECK_EQUAL(exp.superProngs, req.superProngs);
        CHECK_EQUAL(exp.edgeless, req.edgeless);
    }

    // Subset round-robin with 4P seat count check
    OctiBotConfigStore subCfg;
    CHECK_TRUE(subCfg.parseString("host_variants = 2P_1B_SPED, 4P_SP, OFK\nhost_seat = 3\n", err));
    OctiBotPolicyEngine subPolicy(subCfg);
    CHECK_EQUAL(3u, subPolicy.hostVariants().size());

    CreateTableRequest r0 = subPolicy.buildHostTableRequest();
    CHECK_EQUAL("2P", r0.mode);
    CHECK_EQUAL(1, r0.bases);
    CHECK_TRUE(r0.superProngs);
    CHECK_TRUE(r0.edgeless);

    CreateTableRequest r1 = subPolicy.buildHostTableRequest();
    CHECK_EQUAL("4P", r1.mode);
    CHECK_EQUAL(1, r1.bases);
    CHECK_TRUE(r1.superProngs);
    CHECK_FALSE(r1.edgeless);
    CHECK_EQUAL(3, r1.seat);

    CreateTableRequest r2 = subPolicy.buildHostTableRequest();
    CHECK_EQUAL("OFK", r2.mode);
    CHECK_EQUAL(1, r2.bases);
    CHECK_FALSE(r2.superProngs);
    CHECK_FALSE(r2.edgeless);

    CreateTableRequest r3 = subPolicy.buildHostTableRequest();
    CHECK_EQUAL("2P", r3.mode);
    CHECK_EQUAL(1, r3.bases);
    CHECK_TRUE(r3.superProngs);
    CHECK_TRUE(r3.edgeless);
}

TEST(OctiBotHostVariants, InvalidVariantNameRejectedAtLoad) {
    // 1. Rejected by OctiBotConfigStore::parseString (global key)
    {
        OctiBotConfigStore store;
        std::string err;
        CHECK_FALSE(store.parseString("host_variants = 2P_3B, INVALID_VARIANT\n", err));
        CHECK_TRUE(err.find("INVALID_VARIANT") != std::string::npos);
    }

    // 2. Rejected by OctiBotConfigStore::parseString (profile section)
    {
        OctiBotConfigStore store;
        std::string err;
        CHECK_FALSE(store.parseString("[stock]\nhost_variants = 4P_BOGUS\n", err));
        CHECK_TRUE(err.find("4P_BOGUS") != std::string::npos);
    }

    // 3. Rejected on OFK with flags (OFK has superprongs and edgeless forced off)
    {
        OctiBotConfigStore store;
        std::string err;
        CHECK_FALSE(store.parseString("host_variants = OFK_SP\n", err));
        CHECK_FALSE(err.empty());
    }

    // 4. Rejected on empty token in comma-separated list
    {
        OctiBotConfigStore store;
        std::string err;
        CHECK_FALSE(store.parseString("host_variants = 2P_3B, , OFK\n", err));
        CHECK_FALSE(err.empty());
    }

    // 5. Rejected by OctiBotConfigStore::loadFile
    {
        char tmpl[] = "/tmp/octibot_hv_bad_XXXXXX";
        char *dir = ::mkdtemp(tmpl);
        CHECK_TRUE(dir != nullptr);
        std::string path = std::string(dir) + "/bad.conf";
        {
            std::ofstream out(path);
            out << "host_variants = 2P_3B, 3P_INVALID\n";
        }
        OctiBotConfigStore store;
        std::string err;
        CHECK_FALSE(store.loadFile(path, err));
        CHECK_TRUE(err.find("3P_INVALID") != std::string::npos);
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    // 6. Rejected by OctiBotPolicyEngine::configure(config, err) and OctiBotRunner::applyConfigUpdates
    {
        OctiBotConfigStore cfg;
        cfg.set("host_variants", "UNKNOWN_MODE");
        OctiBotPolicyEngine policy;
        std::string err;
        CHECK_FALSE(policy.configure(cfg, err));
        CHECK_FALSE(err.empty());

        OctiBotConfigStore validCfg;
        auto agent = std::make_unique<StockAlphaBetaAgent>(validCfg);
        OctiBotRunner runner(std::move(agent), validCfg);
        std::map<std::string, std::string> badUpdates = {{"host_variants", "2P_3B, BAD_ONE"}};
        CHECK_FALSE(runner.applyConfigUpdates(badUpdates, false, err));
        CHECK_TRUE(err.find("BAD_ONE") != std::string::npos);
    }
}

TEST(OctiBotHostVariants, EmptyHostVariantsKeepsCurrentBehaviour) {
    OctiBotConfigStore cfg;
    std::string err;
    CHECK_TRUE(cfg.parseString(
        "host_variants = \n"
        "host_mode = 2P\n"
        "host_bases = 1\n"
        "host_super_prongs = true\n"
        "host_edgeless = true\n",
        err));

    OctiBotPolicyEngine policy;
    CHECK_TRUE(policy.configure(cfg, err));
    CHECK_TRUE(policy.hostVariants().empty());

    for (int i = 0; i < 5; ++i) {
        CreateTableRequest req = policy.buildHostTableRequest();
        CHECK_EQUAL("2P", req.mode);
        CHECK_EQUAL(1, req.bases);
        CHECK_TRUE(req.superProngs);
        CHECK_TRUE(req.edgeless);
    }

    // Default joining policy remains 'any' across all 13 variants
    static const struct {
        const char *mode;
        int bases;
        bool sp;
        bool ed;
    } kJoinVariants[13] = {
        {"2P", 3, false, false}, {"2P", 3, true, false}, {"2P", 3, false, true}, {"2P", 3, true, true},
        {"2P", 1, false, false}, {"2P", 1, true, false}, {"2P", 1, false, true}, {"2P", 1, true, true},
        {"4P", 1, false, false}, {"4P", 1, true, false}, {"4P", 1, false, true}, {"4P", 1, true, true},
        {"OFK", 1, false, false}
    };
    for (int i = 0; i < 13; ++i) {
        TableSummaryInfo t;
        t.roomId = "room_" + std::to_string(i);
        t.mode = kJoinVariants[i].mode;
        t.bases = kJoinVariants[i].bases;
        t.superProngs = kJoinVariants[i].sp;
        t.edgeless = kJoinVariants[i].ed;
        t.allowRobots = true;
        t.phase = "WAITING_FOR_PLAYERS";
        t.seatsTotal = (t.mode == "4P") ? 4 : 2;
        t.seatsFilled = 1;
        CHECK_TRUE(policy.isTableCompatible(t, "test_owner", "stock"));
    }
}

// ============================================================================
// Envelope 1: Multi-Runner MCP Routing, 13-Variant Matchmaking & Schema Tests
// ============================================================================


TEST_GROUP(OctiBotSupersededReclaimTest) {
    std::unique_ptr<TestServerEnv> env;

    void setup() {
        env = std::make_unique<TestServerEnv>();
    }

    void teardown() {
        env.reset();
    }
};

TEST(OctiBotSupersededReclaimTest, DefaultAllowSupersedeFalseProtectsActiveRunnerUntilExplicitTakeover) {
    VerifiedClaims claims;
    claims.provider = "google";
    claims.sub = "sub-no-supersede-owner";
    claims.name = "test_owner";
    claims.email = "test_owner@example.com";
    auto loginRes = env->store.loginWithSso(claims, "test-agent");
    int64_t ownerId = loginRes.profile.playerId;
    CHECK_TRUE(ownerId > 0);

    auto rBot = env->store.createRobot(ownerId, "protected-bot-1", "Protected Test Bot");
    CHECK_EQUAL(static_cast<int>(RobotStatus::OK), static_cast<int>(rBot.status));

    OctiBotConfigStore cfg1;
    cfg1.set("server_host", "127.0.0.1");
    cfg1.set("server_port", std::to_string(env->port));
    cfg1.set("robot_name", "protected-bot-1");
    cfg1.set("bot_token_protected-bot-1", rBot.rawToken);
    cfg1.set("table_behavior", "idle");
    cfg1.set("verbosity", "0");

    auto runner1 = std::make_unique<OctiBotRunner>(
        std::make_unique<StockAlphaBetaAgent>(cfg1), cfg1);
    const auto eff1 = runner1->buildEffectiveConfigMap();
    CHECK_TRUE(eff1.count("allow_supersede") > 0);
    CHECK_EQUAL("false", eff1.at("allow_supersede"));

    std::string err1;
    CHECK_TRUE_TEXT(runner1->connectAndAuthenticate(err1), err1.c_str());
    runner1->start();

    for (int i = 0; i < 80; ++i) {
        if (runner1->isRunning() &&
            !runner1->isSuperseded() &&
            runner1->networkClient().isLobbyEventStreamActive()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK_TRUE(runner1->isRunning());
    CHECK_FALSE(runner1->isSuperseded());
    CHECK_TRUE(runner1->networkClient().isLobbyEventStreamActive());

    // (a) Second runner with default config (allow_supersede omitted -> false) is blocked with 409 ROBOT_TOKEN_IN_USE
    OctiBotConfigStore cfg2Default = cfg1;
    auto runner2Default = std::make_unique<OctiBotRunner>(
        std::make_unique<StockAlphaBetaAgent>(cfg2Default), cfg2Default);
    std::string err2;
    CHECK_FALSE_TEXT(runner2Default->connectAndAuthenticate(err2),
                     "Expected second runner with default allow_supersede=false to fail connect when token is in use");
    CHECK_TRUE_TEXT(err2.find("ROBOT_TOKEN_IN_USE") != std::string::npos ||
                    err2.find("in use") != std::string::npos,
                    ("Expected ROBOT_TOKEN_IN_USE error, got: " + err2).c_str());

    // Explicit allow_supersede = false also blocks
    OctiBotConfigStore cfg2ExplicitFalse = cfg1;
    cfg2ExplicitFalse.set("allow_supersede", "false");
    auto runner2False = std::make_unique<OctiBotRunner>(
        std::make_unique<StockAlphaBetaAgent>(cfg2ExplicitFalse), cfg2ExplicitFalse);
    std::string err2False;
    CHECK_FALSE(runner2False->connectAndAuthenticate(err2False));

    // Verify runner1 was NOT kicked and remains active
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    CHECK_TRUE(runner1->isRunning());
    CHECK_FALSE(runner1->isSuperseded());
    CHECK_TRUE(runner1->networkClient().isLobbyEventStreamActive());

    // (b) Explicit allow_supersede = true permits takeover and supersedes runner1
    OctiBotConfigStore cfg3Takeover = cfg1;
    cfg3Takeover.set("allow_supersede", "true");
    auto runner3 = std::make_unique<OctiBotRunner>(
        std::make_unique<StockAlphaBetaAgent>(cfg3Takeover), cfg3Takeover);
    const auto eff3 = runner3->buildEffectiveConfigMap();
    CHECK_TRUE(eff3.count("allow_supersede") > 0);
    CHECK_EQUAL("true", eff3.at("allow_supersede"));
    std::string err3;
    CHECK_TRUE_TEXT(runner3->connectAndAuthenticate(err3), err3.c_str());

    auto waitStart = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - waitStart).count() < 5000) {
        if (runner1->isSuperseded() && !runner1->isRunning()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK_TRUE(runner1->isSuperseded());
    CHECK_FALSE(runner1->isRunning());

    runner3->stop();
    runner1->stop();
}

TEST(OctiBotSupersededReclaimTest, RunFleetCliVersionHelpAndUnknownFlagsExitBeforeConfigOrConnect) {
    VerifiedClaims claims;
    claims.provider = "google";
    claims.sub = "sub-cli-guard-owner";
    claims.name = "test_owner";
    claims.email = "test_owner@example.com";
    auto loginRes = env->store.loginWithSso(claims, "test-agent");
    int64_t ownerId = loginRes.profile.playerId;
    CHECK_TRUE(ownerId > 0);

    auto rBot = env->store.createRobot(ownerId, "cli-guard-bot", "CLI Guard Bot");
    CHECK_EQUAL(static_cast<int>(RobotStatus::OK), static_cast<int>(rBot.status));

    OctiBotConfigStore activeCfg;
    activeCfg.set("server_host", "127.0.0.1");
    activeCfg.set("server_port", std::to_string(env->port));
    activeCfg.set("robot_name", "cli-guard-bot");
    activeCfg.set("bot_token", rBot.rawToken);
    activeCfg.set("table_behavior", "idle");
    activeCfg.set("verbosity", "0");

    auto activeRunner = std::make_unique<OctiBotRunner>(
        std::make_unique<StockAlphaBetaAgent>(activeCfg), activeCfg);
    std::string connErr;
    CHECK_TRUE_TEXT(activeRunner->connectAndAuthenticate(connErr), connErr.c_str());
    activeRunner->start();

    for (int i = 0; i < 80; ++i) {
        if (activeRunner->isRunning() && activeRunner->networkClient().isLobbyEventStreamActive()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK_TRUE(activeRunner->isRunning());
    CHECK_FALSE(activeRunner->isSuperseded());

    char tmpTmpl[] = "/tmp/octibot_cli_guard_XXXXXX";
    char *td = ::mkdtemp(tmpTmpl);
    CHECK_TRUE(td != nullptr);
    const std::string dirPath = td;
    const std::string activeConfPath = dirPath + "/active_bot.conf";
    const std::string brokenConfPath = dirPath + "/broken.conf";
    {
        std::ofstream f(activeConfPath);
        f << "server_host = 127.0.0.1\n"
          << "server_port = " << env->port << "\n"
          << "verbosity = 0\n"
          << "default_profile = cli-guard-bot\n"
          << "[cli-guard-bot]\n"
          << "robot_name = cli-guard-bot\n"
          << "bot_token = " << rBot.rawToken << "\n"
          << "table_behavior = idle\n";
    }
    {
        std::ofstream f(brokenConfPath);
        f << "[unterminated_section\nthis_is_invalid_syntax\n";
    }

    auto invokeCli = [](std::vector<std::string> args, std::string &outStdout, std::string &outStderr) -> int {
        std::vector<char *> argv;
        for (auto &s : args) {
            argv.push_back(&s[0]);
        }
        std::ostringstream capOut, capErr;
        std::streambuf *oldOut = std::cout.rdbuf(capOut.rdbuf());
        std::streambuf *oldErr = std::cerr.rdbuf(capErr.rdbuf());
        int rc = runFleetCli(static_cast<int>(argv.size()), argv.data());
        std::cout.rdbuf(oldOut);
        std::cerr.rdbuf(oldErr);
        outStdout = capOut.str();
        outStderr = capErr.str();
        return rc;
    };

    // 1. --version and -V exit 0 immediately before loading broken config or connecting
    std::string outStr, errStr;
    CHECK_EQUAL(0, invokeCli({"octibot", "--config", brokenConfPath, "--version"}, outStr, errStr));
    CHECK_TRUE(outStr.find("OCTIBOT v") != std::string::npos);
    CHECK_EQUAL("", errStr);

    CHECK_EQUAL(0, invokeCli({"octibot", "-V"}, outStr, errStr));
    CHECK_TRUE(outStr.find("OCTIBOT v") != std::string::npos);

    // 2. --help and -h exit 0 immediately before loading broken config or connecting
    CHECK_EQUAL(0, invokeCli({"octibot", "--config", brokenConfPath, "--help"}, outStr, errStr));
    CHECK_TRUE(outStr.find("Usage:") != std::string::npos);
    CHECK_TRUE(outStr.find("--supersede") != std::string::npos);
    CHECK_TRUE(outStr.find("--no-supersede") != std::string::npos);
    CHECK_TRUE(outStr.find("--version") != std::string::npos);
    CHECK_EQUAL("", errStr);

    CHECK_EQUAL(0, invokeCli({"octibot", "-h"}, outStr, errStr));
    CHECK_TRUE(outStr.find("Usage:") != std::string::npos);

    // 3. Unknown flags (--bogus-flag, -x) and missing option values exit != 0 before loading config or connecting
    CHECK_TRUE(invokeCli({"octibot", "--config", activeConfPath, "--bogus-flag"}, outStr, errStr) != 0);
    CHECK_TRUE(errStr.find("--bogus-flag") != std::string::npos);

    CHECK_TRUE(invokeCli({"octibot", "--config", brokenConfPath, "-x"}, outStr, errStr) != 0);
    CHECK_TRUE(errStr.find("-x") != std::string::npos);

    CHECK_TRUE(invokeCli({"octibot", "--profile"}, outStr, errStr) != 0);
    CHECK_TRUE(errStr.find("--profile") != std::string::npos);

    // 4. Running runFleetCli against activeConfPath with default (or --no-supersede) fails closed without kicking activeRunner
    CHECK_EQUAL(0, invokeCli({"octibot", "--config", activeConfPath, "--no-supersede"}, outStr, errStr));
    CHECK_TRUE(errStr.find("Failed to connect") != std::string::npos);
    CHECK_TRUE(activeRunner->isRunning());
    CHECK_FALSE(activeRunner->isSuperseded());
    CHECK_TRUE(activeRunner->networkClient().isLobbyEventStreamActive());

    activeRunner->stop();
    std::error_code ec;
    std::filesystem::remove_all(dirPath, ec);
}

// ============================================================================
// Envelope 1: Variant Tuning Params, Complete Config JSON/UI & Slim Profile Save
// ============================================================================

TEST_GROUP(OctiVariantParamsAndSlimProfileConfig) {
};

TEST(OctiVariantParamsAndSlimProfileConfig, HasProfileKeyAndSaveFilePosix0600Mode) {
    const std::string confText =
        "server_host = 127.0.0.1\n"
        "lazy_smp_threads = 4\n"
        "bot_token = octi_bot_9999999999999999999999999999999999999999999999999999999999999999\n"
        "\n"
        "[slim_bot]\n"
        "robot_name = slim_bot_handle\n"
        "host_mode = OFK\n"
        "\n"
        "[sibling_bot]\n"
        "robot_name = sibling_bot\n"
        "host_mode = 4P\n"
        "four_player_eliminated_penalty = 25000\n";

    OctiBotConfigStore store;
    std::string err;
    CHECK_TRUE_TEXT(store.parseString(confText, err), err.c_str());

    CHECK_TRUE(store.hasProfileKey("slim_bot", "robot_name"));
    CHECK_TRUE(store.hasProfileKey("slim_bot", "host_mode"));
    CHECK_FALSE(store.hasProfileKey("slim_bot", "lazy_smp_threads"));
    CHECK_FALSE(store.hasProfileKey("slim_bot", "four_player_eliminated_penalty"));
    CHECK_TRUE(store.hasProfileKey("sibling_bot", "four_player_eliminated_penalty"));
    CHECK_FALSE(store.hasProfileKey("missing_profile", "host_mode"));

    OctiBotConfigStore resolved = store.resolveProfile("slim_bot");
    CHECK_EQUAL("slim_bot", resolved.activeProfile());
    CHECK_EQUAL("slim_bot_handle", resolved.getString("robot_name"));
    CHECK_EQUAL(4, resolved.getInt("lazy_smp_threads", 0));
    CHECK_TRUE(resolved.hasProfileKey("slim_bot", "host_mode"));
    CHECK_FALSE(resolved.hasProfileKey("slim_bot", "lazy_smp_threads"));

#if !defined(_WIN32)
    char tmpTmpl[] = "/tmp/octibot_e1_perm_XXXXXX";
    char *td = ::mkdtemp(tmpTmpl);
    CHECK_TRUE(td != nullptr);
    const std::string dirPath = td;
    const std::string confPath = dirPath + "/octibot.conf";

    CHECK_TRUE_TEXT(store.saveFile(confPath, err), err.c_str());
    struct stat st{};
    CHECK_EQUAL(0, ::stat(confPath.c_str(), &st));
    CHECK_EQUAL(0600, static_cast<int>(st.st_mode & 0777));

    // Loosen file permissions to 0644, re-save, and verify saveFile restores 0600
    CHECK_EQUAL(0, ::chmod(confPath.c_str(), 0644));
    CHECK_EQUAL(0, ::stat(confPath.c_str(), &st));
    CHECK_EQUAL(0644, static_cast<int>(st.st_mode & 0777));

    CHECK_TRUE_TEXT(store.saveFile(confPath, err), err.c_str());
    CHECK_EQUAL(0, ::stat(confPath.c_str(), &st));
    CHECK_EQUAL(0600, static_cast<int>(st.st_mode & 0777));

    std::ifstream savedIn(confPath);
    std::stringstream savedBuf;
    savedBuf << savedIn.rdbuf();
    CHECK_TRUE(savedBuf.str().find("octi_bot_9999") == std::string::npos);

    ::unlink(confPath.c_str());
    ::rmdir(dirPath.c_str());
#endif
}


namespace {
class CustomSdkTestEngine : public OctiBotEngine {
public:
    explicit CustomSdkTestEngine(bool ctorFinishedFlag)
        : OctiBotEngine(BotDifficulty::TACTICAL, 0x5EED2026ULL)
        , createdAfterSubclassCtor(ctorFinishedFlag)
    {
    }

    bool createdAfterSubclassCtor = false;
    mutable int customSearchMovesCalls = 0;

protected:
    void generateSearchMoves(const OctiGame &game,
                             OctiPlayer turn,
                             int ply,
                             std::vector<OctiMove> &outMoves) const override
    {
        ++customSearchMovesCalls;
        OctiBotEngine::generateSearchMoves(game, turn, ply, outMoves);
    }
};

class CustomFactoryTestAgent : public StockAlphaBetaAgent {
public:
    explicit CustomFactoryTestAgent(const OctiBotConfigStore &config)
        : StockAlphaBetaAgent(config)
    {
        subclassCtorFinished = true;
    }

    bool subclassCtorFinished = false;
    mutable int createEngineCalls = 0;

protected:
    std::unique_ptr<OctiBotEngine> createEngine() const override
    {
        ++createEngineCalls;
        return std::make_unique<CustomSdkTestEngine>(subclassCtorFinished);
    }
};

} // namespace

TEST(OctiBotSdkExtensionHooks, CustomEngineSubclassFactoryInvokedByStockAlphaBetaAgent) {
    OctiBotConfigStore cfg;
    std::string err;
    CHECK_TRUE_TEXT(cfg.parseString(
        "max_search_depth = 2\n"
        "time_budget_ms = 200\n"
        "use_opening_book = false\n"
        "lazy_smp_threads = 1\n", err), err.c_str());

    CustomFactoryTestAgent agent(cfg);
    // Base constructor StockAlphaBetaAgent(config) calls onConfigure(config),
    // which MUST NOT call ensureEngine() before CustomFactoryTestAgent ctor finishes.
    CHECK_TRUE(agent.subclassCtorFinished);
    CHECK_EQUAL(0, agent.createEngineCalls);

    OctiBotEngine *engPtr = &agent.engine();
    CHECK_EQUAL(1, agent.createEngineCalls);
    auto *customEng = dynamic_cast<CustomSdkTestEngine *>(engPtr);
    CHECK_TRUE(customEng != nullptr);
    CHECK_TRUE(customEng->createdAfterSubclassCtor);
    CHECK_EQUAL(2, customEng->tuningProfile().maxSearchDepth);

    // Re-configuring must update the existing engine in-place without reallocating it
    OctiBotConfigStore cfg2;
    CHECK_TRUE_TEXT(cfg2.parseString(
        "max_search_depth = 3\n"
        "time_budget_ms = 250\n"
        "use_opening_book = false\n"
        "lazy_smp_threads = 1\n", err), err.c_str());
    agent.onConfigure(cfg2);
    CHECK_EQUAL(1, agent.createEngineCalls);
    CHECK_EQUAL(engPtr, &agent.engine());
    CHECK_EQUAL(3, customEng->tuningProfile().maxSearchDepth);

    OctiBoard board(GAME_MODE_2, false, false, GAME_BASES_1);
    board.getGame()->start(PLAYER_A);
    GameViewData gv;
    gv.roomId = "factory-test";
    BotMoveTelemetry tel;
    std::string mv = agent.computeMove(board, gv, 1, 0, &tel);
    CHECK_FALSE(mv.empty());
    CHECK_TRUE(customEng->customSearchMovesCalls > 0);
}

// ============================================================================
// Envelope 1: Agent Variant Competency, Match Gating, Companion Decline & MCP
// ============================================================================

namespace {

class NonCartesian2P3BAnd4PAgent : public StockAlphaBetaAgent {
public:
    using StockAlphaBetaAgent::StockAlphaBetaAgent;

    std::atomic<bool> allow2P3B{true};
    std::atomic<bool> allow4P{true};
    std::atomic<int> computeCalls{0};

    bool supportsVariant(const OctiVariantSpec &spec,
                         std::string &outUnsupportedReason) const override {
        const std::string label = formatVariantLabel(spec);
        if (!spec.superProngs && !spec.edgeless) {
            if (spec.mode == "2P" && spec.bases == 3 && allow2P3B.load()) {
                outUnsupportedReason.clear();
                return true;
            }
            if (spec.mode == "4P" && allow4P.load()) {
                outUnsupportedReason.clear();
                return true;
            }
        }
        outUnsupportedReason = "I have not yet been trained to play " + label + ".";
        return false;
    }

    std::string computeMove(const OctiBoard &board,
                            const GameViewData &game,
                            int mySeat,
                            std::int64_t remainingClockMs,
                            BotMoveTelemetry *outTelemetry = nullptr) override {
        ++computeCalls;
        OctiVariantSpec spec = OctiVariantSpec::fromGameView(game);
        std::string reason;
        if (!supportsVariant(spec, reason)) {
            if (outTelemetry) {
                *outTelemetry = BotMoveTelemetry{};
            }
            return "";
        }
        return StockAlphaBetaAgent::computeMove(board, game, mySeat, remainingClockMs, outTelemetry);
    }
};

class EmptyMoveSpinTestAgent : public StockAlphaBetaAgent {
public:
    using StockAlphaBetaAgent::StockAlphaBetaAgent;

    std::atomic<int> computeCalls{0};

    std::string computeMove(const OctiBoard &board,
                            const GameViewData &game,
                            int mySeat,
                            std::int64_t remainingClockMs,
                            BotMoveTelemetry *outTelemetry = nullptr) override {
        (void) board;
        (void) game;
        (void) mySeat;
        (void) remainingClockMs;
        ++computeCalls;
        if (outTelemetry) {
            *outTelemetry = BotMoveTelemetry{};
        }
        return "";
    }
};

static LoginResult loginTestHuman(TestServerEnv &env,
                                  const std::string &sub,
                                  const std::string &handle) {
    VerifiedClaims claims;
    claims.provider = "google";
    claims.sub = sub;
    claims.name = handle;
    claims.email = handle + "@selfso.com";
    return env.store.loginWithSso(claims, "test-agent");
}

static bool waitForGameChatMatching(OctiNetworkClient &client,
                                    const std::string &roomId,
                                    const std::string &fromHandle,
                                    const std::string &substring,
                                    std::string &outMatchedText,
                                    int timeoutMs = 2000) {
    int loops = std::max(1, timeoutMs / 25);
    for (int i = 0; i < loops; ++i) {
        auto evs = client.pollGameEvents(roomId, 0, 100);
        if (evs.ok) {
            for (const auto &ev : evs.data) {
                if (ev.event == "CHAT" || ev.event == "BOARD_CHAT") {
                    try {
                        auto j = nlohmann::json::parse(ev.dataJson);
                        std::string h = j.value("handle", "");
                        std::string txt = j.value("text", "");
                        if ((fromHandle.empty() || h == fromHandle) &&
                            txt.find(substring) != std::string::npos) {
                            outMatchedText = txt;
                            return true;
                        }
                    } catch (...) {}
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

} // namespace

TEST_GROUP(AgentVariantCompetency) {
    std::unique_ptr<TestServerEnv> env;

    void setup() {
        env = std::make_unique<TestServerEnv>();
        if (env && env->rooms) {
            RoomLimits lim;
            lim.lobbyChatBurst = 100;
            lim.boardChatBurst = 100;
            env->rooms->setLimits(lim);
        }
    }

    void teardown() {
        env.reset();
    }
};

TEST(AgentVariantCompetency, ForcedInProgressUnsupportedVariantAndEmptyComputeMoveResignAndLeaveWithoutSpinning) {
    auto ownerLogin = loginTestHuman(*env, "sub-avc-spin-owner", "test_owner");
    auto aliceLogin = loginTestHuman(*env, "sub-avc-spin-alice", "alice");

    OctiNetworkClient aliceClient;
    aliceClient.setEndpoint("127.0.0.1", env->port);
    aliceClient.setSessionToken(aliceLogin.sessionToken);

    // 1. computeMove("") anti-spin test: must resign & leave after 1 call, NEVER spin every 250ms
    auto rSpin = env->store.createRobot(ownerLogin.profile.playerId, "bot-empty-move", "Empty Move Test Bot");
    OctiBotConfigStore spinCfg;
    spinCfg.set("server_host", "127.0.0.1");
    spinCfg.set("server_port", std::to_string(env->port));
    spinCfg.set("robot_name", "bot-empty-move");
    spinCfg.set("bot_token", rSpin.rawToken);
    spinCfg.set("table_behavior", "idle");
    spinCfg.set("verbosity", "0");

    auto spinAgentUPtr = std::make_unique<EmptyMoveSpinTestAgent>(spinCfg);
    EmptyMoveSpinTestAgent *spinAgent = spinAgentUPtr.get();
    OctiBotRunner spinRunner(std::move(spinAgentUPtr), spinCfg);
    std::string err;
    CHECK_TRUE(spinRunner.connectAndAuthenticate(err));
    spinRunner.start();

    CreateTableRequest req2p3b;
    req2p3b.mode = "2P";
    req2p3b.bases = 3;
    req2p3b.allowRobots = true;
    req2p3b.seat = 2; // Alice takes Seat 2 so bot gets Seat 1 and moves immediately on ply 0
    std::string spinRoomId = aliceClient.createTable(req2p3b).data.roomId;
    CHECK_FALSE(spinRoomId.empty());

    int joinedSeat = 0;
    CHECK_TRUE(spinRunner.joinRoomById(spinRoomId, 1, joinedSeat, err));
    CHECK_EQUAL(1, joinedSeat);

    for (int i = 0; i < 40; ++i) {
        if (spinAgent->computeCalls.load() >= 1 && spinRunner.currentRoomId().empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // Hold an extra 350ms to catch any 250ms spin-loop regression
    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    CHECK_EQUAL_TEXT(1, spinAgent->computeCalls.load(),
                     "computeMove returning '' must trigger immediate resign+leave after 1 call without spinning");
    CHECK_EQUAL_TEXT("", spinRunner.currentRoomId(),
                     "Runner must leave room after computeMove('')");
    CHECK_EQUAL("IDLE", spinRunner.currentPhase());
    CHECK_FALSE(spinRunner.matchOverride().active);

    auto gvSpin = aliceClient.fetchGameView(spinRoomId);
    CHECK_TRUE(gvSpin.ok);
    CHECK_TRUE_TEXT(gvSpin.data.phase == "FINISHED" || gvSpin.data.phase == "GAME_OVER",
                    ("Expected resigned game to be FINISHED, got " + gvSpin.data.phase).c_str());
    std::string spinChat;
    CHECK_TRUE_TEXT(
        waitForGameChatMatching(aliceClient, spinRoomId, "bot-empty-move", "Sorry,", spinChat, 500),
        "Expected game-chat explanation before resigning on computeMove('')");
    spinRunner.stop();

    // 2. Session-recovered forced IN_PROGRESS unsupported OFK room resigns immediately with 0 computeMove calls
    auto rForced = env->store.createRobot(ownerLogin.profile.playerId, "bot-forced-ofk", "Forced OFK Bot");
    OctiNetworkClient rawBotClient;
    rawBotClient.setEndpoint("127.0.0.1", env->port);
    rawBotClient.setBotToken(rForced.rawToken);
    CHECK_TRUE(rawBotClient.connectRobot(false).ok);

    CreateTableRequest ofkReq;
    ofkReq.mode = "OFK";
    ofkReq.bases = 1;
    ofkReq.allowRobots = true;
    ofkReq.seat = 2;
    std::string forcedOfkRoom = aliceClient.createTable(ofkReq).data.roomId;
    CHECK_TRUE(rawBotClient.joinTable(forcedOfkRoom, 1).ok);
    rawBotClient.disconnectRobot();

    OctiBotConfigStore forcedCfg = spinCfg;
    forcedCfg.set("robot_name", "bot-forced-ofk");
    forcedCfg.set("bot_token", rForced.rawToken);
    auto forcedAgentUPtr = std::make_unique<NonCartesian2P3BAnd4PAgent>(forcedCfg);
    forcedAgentUPtr->allow4P.store(false);
    NonCartesian2P3BAnd4PAgent *forcedAgent = forcedAgentUPtr.get();
    OctiBotRunner forcedRunner(std::move(forcedAgentUPtr), forcedCfg);
    CHECK_TRUE(forcedRunner.connectAndAuthenticate(err));
    CHECK_EQUAL(forcedOfkRoom, forcedRunner.currentRoomId());
    forcedRunner.start();

    for (int i = 0; i < 60 && !forcedRunner.currentRoomId().empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK_EQUAL_TEXT("", forcedRunner.currentRoomId(),
                     "Forced IN_PROGRESS unsupported room must be resigned and left immediately");
    CHECK_EQUAL_TEXT(0, forcedAgent->computeCalls.load(),
                     "Unsupported IN_PROGRESS room must resign before calling computeMove");
    std::string forcedChat;
    CHECK_TRUE(waitForGameChatMatching(aliceClient, forcedOfkRoom, "bot-forced-ofk", "Sorry,", forcedChat, 500));
    forcedRunner.stop();
}

TEST_GROUP(OctiBotPly0AbortAndHonestLeaveTest) {
};

TEST(OctiBotPly0AbortAndHonestLeaveTest,
     FailClosedAgainstUnUpgradedServerReturning400UnknownActionOnAbort)
{
    httplib::Server stubSrv;
    std::atomic<int> getGameCalls{0};
    std::atomic<int> abortActionCalls{0};
    std::atomic<int> resignActionCalls{0};
    std::atomic<int> leaveTableCalls{0};
    std::atomic<int> purgeTableCalls{0};

    stubSrv.Get(R"(/api/v1/games/([^/]+))", [&](const httplib::Request &req, httplib::Response &res) {
        ++getGameCalls;
        nlohmann::json body = {
            {"ok", true},
            {"room_id", std::string(req.matches[1])},
            {"mode", "2P"},
            {"bases", 3},
            {"phase", "IN_PROGRESS"},
            {"ply_count", 0},
            {"turn_seat", 1},
            {"seats", nlohmann::json::array({
                {{"seat", 1}, {"handle", "test_owner"}, {"is_robot", false}, {"connected", true}},
                {{"seat", 2}, {"handle", "bot-alpha"}, {"is_robot", true}, {"connected", true}}
            })}
        };
        res.status = 200;
        res.set_content(body.dump(), "application/json");
    });

    stubSrv.Post(R"(/api/v1/games/([^/]+)/action)", [&](const httplib::Request &req, httplib::Response &res) {
        auto j = nlohmann::json::parse(req.body, nullptr, false);
        std::string act = j.is_object() ? j.value("action", "") : "";
        if (act == "abort") {
            ++abortActionCalls;
            res.status = 400;
            res.set_content(
                R"({"ok":false,"code":"INVALID_ARGUMENT","message":"unknown action"})",
                "application/json");
            return;
        }
        if (act == "resign") {
            ++resignActionCalls;
            res.status = 200;
            res.set_content(R"({"ok":true,"action":"resign","game_over":true})", "application/json");
            return;
        }
        res.status = 400;
        res.set_content(R"({"ok":false,"code":"INVALID_ARGUMENT","message":"unknown action"})", "application/json");
    });

    stubSrv.Post(R"(/api/v1/tables/([^/]+)/leave)", [&](const httplib::Request &, httplib::Response &res) {
        ++leaveTableCalls;
        res.status = 200;
        res.set_content(R"({"ok":true})", "application/json");
    });

    stubSrv.Delete(R"(/api/v1/tables/([^/]+))", [&](const httplib::Request &, httplib::Response &res) {
        ++purgeTableCalls;
        res.status = 200;
        res.set_content(R"({"ok":true})", "application/json");
    });

    const int stubPort = stubSrv.bind_to_any_port("127.0.0.1");
    CHECK_TRUE(stubPort > 0);
    std::thread srvThread([&]() { stubSrv.listen_after_bind(); });

    OctiBotConfigStore cfg;
    cfg.set("server_host", "127.0.0.1");
    cfg.set("server_port", std::to_string(stubPort));
    cfg.set("server_use_ssl", "false");
    cfg.set("bot_token", "octi_bot_legacy_stub_token");
    cfg.set("robot_name", "bot-alpha");
    cfg.set("table_behavior", "idle");

    OctiBotRunner runner(std::make_unique<StockAlphaBetaAgent>(cfg), cfg);
    runner.networkClient().setEndpoint("127.0.0.1", stubPort);
    runner.networkClient().setBotToken("octi_bot_legacy_stub_token");

    LeaveRoomResult res = runner.leaveRoomWithOptions(
        "g-legacy-srv", /*allowResign=*/false, /*preferPly0Abort=*/true);

    CHECK_FALSE(res.ok);
    CHECK_TRUE(res.refusedWouldResign);
    CHECK_FALSE(res.resigned);
    CHECK_FALSE(res.stoodUpPly0);
    CHECK_EQUAL(static_cast<int>(LeaveRoomMode::REFUSED_WOULD_RESIGN), static_cast<int>(res.mode));
    STRCMP_EQUAL("IN_PROGRESS", res.phase.c_str());
    CHECK_EQUAL(0, res.plyCount);

    CHECK_EQUAL(1, getGameCalls.load());
    CHECK_EQUAL(1, abortActionCalls.load());
    CHECK_EQUAL_TEXT(0, resignActionCalls.load(),
                     "Un-upgraded server 400 on abort MUST fail closed without calling resignGame()");
    CHECK_EQUAL_TEXT(0, leaveTableCalls.load(),
                     "Un-upgraded server 400 on abort MUST fail closed without calling leaveTable()");
    CHECK_EQUAL(0, purgeTableCalls.load());

    stubSrv.stop();
    if (srvThread.joinable()) {
        srvThread.join();
    }
}

int main(int argc, char **argv) {
#if !defined(_WIN32)
    ::signal(SIGPIPE, SIG_IGN);
#endif
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
