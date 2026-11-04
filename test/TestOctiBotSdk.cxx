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
#include "OctiBotMcpServer.hxx"
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
root_score_jitter = 10
captured_enemy_base_bonus = 30000
lost_allied_base_penalty = 35000
neutral_octi_square_bonus = 4500
closest_pod_dist_weight = 350
second_closest_pod_dist_weight = 160
one_step_base_threat_bonus = 9000
one_jump_base_threat_bonus = 13000
two_ply_base_threat_weight = 2600
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
lone_one_prong_rush_penalty = 600
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
    CHECK_EQUAL(10, profile.rootScoreJitter);
    CHECK_EQUAL(30000, profile.capturedEnemyBaseBonus);
    CHECK_EQUAL(35000, profile.lostAlliedBasePenalty);
    CHECK_EQUAL(4500, profile.neutralOctiSquareBonus);
    CHECK_EQUAL(350, profile.closestPodDistWeight);
    CHECK_EQUAL(160, profile.secondClosestPodDistWeight);
    CHECK_EQUAL(9000, profile.oneStepBaseThreatBonus);
    CHECK_EQUAL(13000, profile.oneJumpBaseThreatBonus);
    CHECK_EQUAL(2600, profile.twoPlyBaseThreatWeight);
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
    CHECK_EQUAL(600, profile.loneOneProngRushPenalty);
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

    CHECK_TRUE(policy.isTableCompatible(validTable, "samurai", "stock"));

    TableSummaryInfo noRobots = validTable;
    noRobots.allowRobots = false;
    CHECK_FALSE(policy.isTableCompatible(noRobots, "samurai", "stock"));

    TableSummaryInfo mode4p = validTable;
    mode4p.mode = "4P";
    CHECK_FALSE(policy.isTableCompatible(mode4p, "samurai", "stock"));

    TableSummaryInfo bases1 = validTable;
    bases1.bases = 1;
    CHECK_FALSE(policy.isTableCompatible(bases1, "samurai", "stock"));

    TableSummaryInfo superP = validTable;
    superP.superProngs = true;
    CHECK_FALSE(policy.isTableCompatible(superP, "samurai", "stock"));
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
    CHECK_FALSE(policy.isBanned("samurai"));

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
    t3.hostHandle = "samurai";
    t3.seatsTotal = 2;
    t3.seatsFilled = 1;
    t3.phase = "WAITING_FOR_PLAYERS";

    std::vector<TableSummaryInfo> tables = {t1, t2, t3};
    auto ranked = policy.rankCompatibleTables(tables, "samurai", "stock");

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

TEST_GROUP(OctiBotConversationTest) {
};

TEST(OctiBotConversationTest, PublicAndOwnerCommands) {
    OctiBotConfigStore config;
    OctiBotPolicyEngine policy(config);
    OctiBotConversationEngine conv(config);

    LobbyChatEntry helpMsg;
    helpMsg.handle = "alice";
    helpMsg.text = "@stock help";
    helpMsg.isRobot = false;

    auto rHelp = conv.processLobbyMessage(
        helpMsg, "stock", "samurai", policy, config,
        "", "IDLE", 5, 2, 1, "Stock Alpha-Beta bot", true, "nodes=1000");
    CHECK_EQUAL(static_cast<int>(ChatCommandActionType::REPLY_ONLY), static_cast<int>(rHelp.action));
    CHECK_TRUE(rHelp.replyText.find("Stock Alpha-Beta bot") != std::string::npos);

    LobbyChatEntry statusMsg;
    statusMsg.handle = "samurai";
    statusMsg.text = "@stock status";
    auto rStatus = conv.processLobbyMessage(
        statusMsg, "stock", "samurai", policy, config,
        "room_123", "IN_PROGRESS", 5, 2, 1, "Stock bot", true, "");
    CHECK_EQUAL(static_cast<int>(ChatCommandActionType::REPLY_ONLY), static_cast<int>(rStatus.action));
    CHECK_TRUE(rStatus.replyText.find("room=room_123") != std::string::npos);

    LobbyChatEntry unauthMsg;
    unauthMsg.handle = "stranger";
    unauthMsg.text = "@stock host 4P";
    auto rUnauth = conv.processLobbyMessage(
        unauthMsg, "stock", "samurai", policy, config,
        "", "IDLE", 0, 0, 0, "Bot", true, "");
    CHECK_TRUE(rUnauth.replyText.find("Unauthorized") != std::string::npos);

    LobbyChatEntry botMsg;
    botMsg.handle = "other_bot";
    botMsg.text = "@stock help";
    botMsg.isRobot = true;
    auto rBot = conv.processLobbyMessage(
        botMsg, "stock", "samurai", policy, config,
        "", "IDLE", 0, 0, 0, "Bot", true, "");
    CHECK_EQUAL(static_cast<int>(ChatCommandActionType::NONE), static_cast<int>(rBot.action));
}

TEST(OctiBotConversationTest, ConversationInboxBuffering) {
    OctiBotConfigStore config;
    OctiBotConversationEngine conv(config);

    CHECK_EQUAL(0, conv.getUnreadInbox().size());

    uint64_t id1 = conv.pushInbox("lobby", "", "alice", "uuid1", false, "Hello bot", 1000);
    uint64_t id2 = conv.pushInbox("game", "r1", "bob", "uuid2", false, "Nice move", 1005);

    auto unread = conv.getUnreadInbox();
    CHECK_EQUAL(2, unread.size());
    CHECK_EQUAL(id1, unread[0].id);
    CHECK_EQUAL(id2, unread[1].id);

    conv.markInboxRead(id1);
    CHECK_EQUAL(1, conv.getUnreadInbox().size());

    conv.clearInbox();
    CHECK_EQUAL(0, conv.getAllInbox().size());
}

// ============================================================================
// 4. OctiBotMcpServer Tests
// ============================================================================

TEST_GROUP(OctiBotMcpServerTest) {
};

TEST(OctiBotMcpServerTest, JsonRpcProtocolAndToolsList) {
    OctiBotConfigStore config;
    auto agent = std::make_unique<StockAlphaBetaAgent>(config);
    OctiBotRunner runner(std::move(agent), config);
    OctiBotMcpServer server(runner);

    // initialize
    std::string initResp = server.handleJsonRpc("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\"}");
    auto jInit = nlohmann::json::parse(initResp);
    CHECK_EQUAL("octibot", jInit["result"]["serverInfo"]["name"].get<std::string>());

    // ping
    std::string pingResp = server.handleJsonRpc("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\"}");
    auto jPing = nlohmann::json::parse(pingResp);
    CHECK_TRUE(jPing["result"].is_object());

    // resources/list & resources/read
    std::string rListResp = server.handleJsonRpc("{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"resources/list\"}");
    auto jRList = nlohmann::json::parse(rListResp);
    CHECK_EQUAL(1, jRList["result"]["resources"].size());

    std::string rReadResp = server.handleJsonRpc("{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"resources/read\",\"params\":{\"uri\":\"octi://skill/octibot\"}}");
    auto jRRead = nlohmann::json::parse(rReadResp);
    CHECK_TRUE(jRRead["result"]["contents"][0]["text"].get<std::string>().find("OCTIBOT Agentic AI Skill Guide") != std::string::npos);

    // tools/list: must verify all 11 tools
    std::string tListResp = server.handleJsonRpc("{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/list\"}");
    auto jTList = nlohmann::json::parse(tListResp);
    CHECK_EQUAL(11, jTList["result"]["tools"].size());

    // octibot_get_skill_guide tool call
    std::string sgCall = "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{\"name\":\"octibot_get_skill_guide\",\"arguments\":{\"topic\":\"rules\"}}}";
    auto jSg = nlohmann::json::parse(server.handleJsonRpc(sgCall));
    CHECK_TRUE(jSg["result"]["content"][0]["text"].get<std::string>().find("Section: rules") != std::string::npos);

    // octibot_analyze_position tool call on 2P start board
    std::string apCall = "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/call\",\"params\":{\"name\":\"octibot_analyze_position\",\"arguments\":{\"top_k\":2}}}";
    auto jAp = nlohmann::json::parse(server.handleJsonRpc(apCall));
    CHECK_TRUE(jAp["result"]["content"][0]["text"].get<std::string>().find("top_candidates") != std::string::npos);
}

TEST(OctiBotMcpServerTest, HttpTransportAndLoopbackPort) {
    OctiBotConfigStore config;
    auto agent = std::make_unique<StockAlphaBetaAgent>(config);
    OctiBotRunner runner(std::move(agent), config);
    OctiBotMcpServer server(runner);

    CHECK_TRUE(server.startHttpServer(0, "127.0.0.1"));
    int port = server.boundHttpPort();
    CHECK_TRUE(port > 0);

    httplib::Client client("127.0.0.1", port);
    auto res = client.Post("/mcp", "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}", "application/json");
    CHECK_TRUE(res != nullptr);
    CHECK_EQUAL(200, res->status);

    server.stopHttpServer();
    CHECK_EQUAL(0, server.boundHttpPort());
}

TEST(OctiBotMcpServerTest, AimonMockGatewayBridge) {
    int servSock = ::socket(AF_INET, SOCK_STREAM, 0);
    CHECK_TRUE(servSock >= 0);

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    socklen_t len = sizeof(addr);
    CHECK_EQUAL(0, ::bind(servSock, (struct sockaddr *)&addr, sizeof(addr)));
    CHECK_EQUAL(0, ::listen(servSock, 1));
    CHECK_EQUAL(0, ::getsockname(servSock, (struct sockaddr *)&addr, &len));
    int port = ntohs(addr.sin_port);

    OctiBotConfigStore config;
    config.set("aimon_enabled", "true");
    config.set("aimon_host", "127.0.0.1");
    config.set("aimon_port", std::to_string(port));

    auto agent = std::make_unique<StockAlphaBetaAgent>(config);
    OctiBotRunner runner(std::move(agent), config);
    OctiBotMcpServer server(runner);

    int clientSock = ::accept(servSock, nullptr, nullptr);
    CHECK_TRUE(clientSock >= 0);

    char buf[16384];
    ssize_t n = ::recv(clientSock, buf, sizeof(buf) - 1, 0);
    CHECK_TRUE(n > 0);
    if (n > 0) {
        buf[n] = '\0';
    } else {
        buf[0] = '\0';
    }
    std::string msg(buf);
    CHECK_TRUE(msg.find("gateway/register") != std::string::npos);
    CHECK_TRUE(msg.find("\"tools\"") != std::string::npos);

    std::string regResp = "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"status\":\"registered\",\"registered_tools\":11,\"capability_score\":0}}\n";
    ::send(clientSock, regResp.c_str(), regResp.size(), 0);

    std::string pingReq = "{\"jsonrpc\":\"2.0\",\"id\":99,\"method\":\"ping\"}\n";
    ::send(clientSock, pingReq.c_str(), pingReq.size(), 0);

    n = ::recv(clientSock, buf, sizeof(buf) - 1, 0);
    CHECK_TRUE(n > 0);
    if (n > 0) {
        buf[n] = '\0';
    } else {
        buf[0] = '\0';
    }
    std::string pingResp(buf);
    CHECK_TRUE(pingResp.find("\"id\":99") != std::string::npos);

    server.stopAimonBridge();
    ::close(clientSock);
    ::close(servSock);
}

// ============================================================================
// 5. OctiBotAgentCliLauncher Tests
// ============================================================================

TEST_GROUP(OctiBotAgentCliLauncherTest) {
};

TEST(OctiBotAgentCliLauncherTest, BuildCommandArgs) {
    auto argsAgy = OctiBotAgentCliLauncher::buildCommandArgs(
        "agy", "", 12345, "flash", "My prompt", false, "");
    CHECK_EQUAL("agy", argsAgy[0]);
    CHECK_EQUAL("--model", argsAgy[1]);
    CHECK_EQUAL("flash", argsAgy[2]);
    CHECK_EQUAL("--sandbox", argsAgy[3]);
    CHECK_EQUAL("--disable-slash-commands", argsAgy[4]);
    CHECK_EQUAL("--settings", argsAgy[5]);
    CHECK_TRUE(argsAgy[6].find("mcp(octibot/*)") != std::string::npos);
    CHECK_TRUE(argsAgy[6].find("deny") != std::string::npos);

    auto argsClaude = OctiBotAgentCliLauncher::buildCommandArgs(
        "claude", "", 54321, "", "Prompt", true, "");
    CHECK_EQUAL("claude", argsClaude[0]);
    CHECK_EQUAL("--restricted", argsClaude[1]);
    CHECK_EQUAL("--strict-mcp-config", argsClaude[2]);
    CHECK_EQUAL("--tools", argsClaude[3]);
    CHECK_EQUAL("", argsClaude[4]);
    CHECK_EQUAL("--disable-slash-commands", argsClaude[5]);
    CHECK_EQUAL("--no-session-persistence", argsClaude[6]);
    CHECK_EQUAL("--permission-prompts", argsClaude[7]);
    CHECK_EQUAL("none", argsClaude[8]);
    CHECK_EQUAL("--mcp-config", argsClaude[9]);
    CHECK_TRUE(argsClaude[10].find("54321") != std::string::npos);

    auto argsCursor = OctiBotAgentCliLauncher::buildCommandArgs(
        "cursor-agent", "", 12345, "pro", "Prompt", false, "/tmp/sdir");
    CHECK_EQUAL("cursor-agent", argsCursor[0]);
    CHECK_EQUAL("--model", argsCursor[1]);
    CHECK_EQUAL("pro", argsCursor[2]);
    CHECK_EQUAL("--mode", argsCursor[3]);
    CHECK_EQUAL("ask", argsCursor[4]);
    CHECK_EQUAL("--sandbox", argsCursor[5]);
    CHECK_EQUAL("enabled", argsCursor[6]);
    CHECK_EQUAL("--workspace", argsCursor[7]);
    CHECK_EQUAL("/tmp/sdir", argsCursor[8]);
}

TEST(OctiBotAgentCliLauncherTest, BuildChatCommandArgsAndSecuritySanitization) {
    auto agyChat = OctiBotAgentCliLauncher::buildChatCommandArgs(
        "agy", "", 3888, "flash", "Hello chat", "/tmp/sdir");
    for (const auto &arg : agyChat) {
        CHECK_FALSE(arg == "--dangerously-skip-permissions");
    }
    CHECK_TRUE(agyChat[5] == "--settings");
    CHECK_TRUE(agyChat[6].find("mcp(octibot:octibot_get_*)") != std::string::npos);
    CHECK_TRUE(agyChat[6].find("command(*)") != std::string::npos);

    auto claudeChat = OctiBotAgentCliLauncher::buildChatCommandArgs(
        "claude", "", 3888, "sonnet", "Hello chat", "/tmp/sdir");
    for (const auto &arg : claudeChat) {
        CHECK_FALSE(arg == "--dangerously-skip-permissions");
    }
    CHECK_TRUE(claudeChat[1] == "--model");
    CHECK_TRUE(claudeChat[3] == "--restricted");

    auto cursorChat = OctiBotAgentCliLauncher::buildChatCommandArgs(
        "cursor-agent", "", 3888, "pro", "Hello chat", "/tmp/sdir");
    for (const auto &arg : cursorChat) {
        CHECK_FALSE(arg == "--trust");
        CHECK_FALSE(arg == "--force");
        CHECK_FALSE(arg == "--yolo");
        CHECK_FALSE(arg == "--approve-mcps");
    }
}

TEST(OctiBotAgentCliLauncherTest, MaterializeSandboxWorkspace) {
    std::string testSandbox = "/tmp/test-octibot-sandbox-" + std::to_string(::getpid());
    std::error_code ec;
    std::filesystem::remove_all(testSandbox, ec);

    CHECK_TRUE(OctiBotAgentCliLauncher::materializeSandboxWorkspace(testSandbox, 3888));
    CHECK_TRUE(std::filesystem::exists(testSandbox + "/.agents/plugins/octibot/plugin.json"));
    CHECK_TRUE(std::filesystem::exists(testSandbox + "/.agents/plugins/octibot/mcp_config.json"));
    CHECK_TRUE(std::filesystem::exists(testSandbox + "/.agents/plugins/octibot/skills/octibot/SKILL.md"));
    CHECK_TRUE(std::filesystem::exists(testSandbox + "/.cursor/mcp.json"));
    CHECK_TRUE(std::filesystem::exists(testSandbox + "/.cursor/skills/octibot/SKILL.md"));

    struct stat st;
    CHECK_EQUAL(0, ::stat(testSandbox.c_str(), &st));
    CHECK_EQUAL(0700, st.st_mode & 0777);

    std::ifstream ifs(testSandbox + "/.cursor/mcp.json");
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    CHECK_TRUE(content.find("3888") != std::string::npos);

    std::filesystem::remove_all(testSandbox, ec);
}

TEST(OctiBotAgentCliLauncherTest, SpawnAndTerminateChildProcess) {
    OctiBotConfigStore config;
    config.set("ai_cli_backend", "sleep");
    config.set("ai_cli_executable", "/bin/sleep");
    config.set("ai_cli_prompt", "60");

    auto agent = std::make_unique<StockAlphaBetaAgent>(config);
    OctiBotRunner runner(std::move(agent), config);
    OctiBotMcpServer server(runner);
    OctiBotAgentCliLauncher launcher(runner, server);

    CHECK_TRUE(launcher.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK_TRUE(launcher.isRunning());

    launcher.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CHECK_FALSE(launcher.isRunning());
}

// ============================================================================
// 6. Real End-to-End Loopback Gameplay & Superseding Lease Tests
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
    claims.sub = "sub-samurai";
    claims.name = "samurai";
    claims.email = "samurai@selfso.com";
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

    stockRunner->conversationEngine().processGameMessage(
        "samurai", claims.sub, false, "@stock status",
        "stock", "samurai", stockRunner->policyEngine(),
        stockRunner->config(), stockRunner->currentRoomId(),
        stockRunner->currentPhase(), 0, 0, 0, "Stock bot", true, "");

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
    claims.sub = "sub-samurai2";
    claims.name = "samurai2";
    claims.email = "samurai2@selfso.com";
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

// ============================================================================
// 9. OctiBotWebDashboard Tests
// ============================================================================

TEST_GROUP(OctiBotWebDashboardTest) {
};

TEST(OctiBotWebDashboardTest, HttpEndpointsAndDashboardHtml) {
    OctiBotConfigStore config;
    auto agent = std::make_unique<StockAlphaBetaAgent>(config);
    OctiBotRunner runner(std::move(agent), config);
    runner.appendStdoutLine("Dashboard boot log line 1", false);

    OctiBotMcpServer server(runner);
    CHECK_TRUE(server.startHttpServer(0, "127.0.0.1"));
    int port = server.boundHttpPort();
    CHECK_TRUE(port > 0);

    httplib::Client client("127.0.0.1", port);

    // Record sample conversation
    auto cseq = runner.recordConversation("lobby", "", "samurai", "hello bot", "", "seen");
    CHECK_TRUE(cseq > 0);
    runner.updateConversationReply(cseq, "hello samurai", "command");
    auto chist = runner.getConversationHistory();
    CHECK_EQUAL(1, chist.size());
    CHECK_EQUAL("hello samurai", chist[0].replyText);
    CHECK_EQUAL("command", chist[0].replySource);

    // 1. GET / -> HTML
    auto resIndex = client.Get("/");
    CHECK_TRUE(resIndex != nullptr);
    CHECK_EQUAL(200, resIndex->status);
    CHECK_TRUE(resIndex->body.find("OCTIBOT Telemetry & Control") != std::string::npos);
    CHECK_TRUE(resIndex->body.find("Live stdout Console") != std::string::npos);
    CHECK_TRUE(resIndex->body.find("<textarea class=\"console-output\" id=\"consoleOutput\" readonly spellcheck=\"false\">") != std::string::npos);
    CHECK_TRUE(resIndex->body.find("Agent Conversation Log") != std::string::npos);
    CHECK_TRUE(resIndex->body.find("telemetryGraphSvg") != std::string::npos);

    // 2. GET /api/v1/status -> JSON
    auto resStatus = client.Get("/api/v1/status");
    CHECK_TRUE(resStatus != nullptr);
    CHECK_EQUAL(200, resStatus->status);
    auto statusJson = nlohmann::json::parse(resStatus->body);
    CHECK_TRUE(statusJson.contains("server"));
    CHECK_TRUE(statusJson.contains("robot"));
    CHECK_TRUE(statusJson["robot"].contains("handle"));
    CHECK_TRUE(statusJson.contains("match"));
    CHECK_TRUE(statusJson.contains("engine"));
    CHECK_TRUE(statusJson.contains("conversations"));
    CHECK_TRUE(statusJson["conversations"].is_array());
    CHECK_EQUAL(1, statusJson["conversations"].size());
    CHECK_EQUAL("samurai", statusJson["conversations"][0]["sender_handle"].get<std::string>());
    CHECK_EQUAL("hello bot", statusJson["conversations"][0]["seen_text"].get<std::string>());
    CHECK_EQUAL("hello samurai", statusJson["conversations"][0]["reply_text"].get<std::string>());
    CHECK_EQUAL("command", statusJson["conversations"][0]["reply_source"].get<std::string>());

    // 3. GET /api/v1/logs -> JSON
    auto resLogs = client.Get("/api/v1/logs");
    CHECK_TRUE(resLogs != nullptr);
    CHECK_EQUAL(200, resLogs->status);
    auto logsJson = nlohmann::json::parse(resLogs->body);
    CHECK_TRUE(logsJson.contains("logs"));
    CHECK_TRUE(logsJson.contains("oldest_seq"));
    CHECK_TRUE(logsJson.contains("latest_seq"));
    CHECK_TRUE(logsJson["logs"].is_array());
    CHECK_TRUE(!logsJson["logs"].empty());

    // 4. GET /api/v1/config -> JSON
    auto resConfig = client.Get("/api/v1/config");
    CHECK_TRUE(resConfig != nullptr);
    CHECK_EQUAL(200, resConfig->status);
    auto configJson = nlohmann::json::parse(resConfig->body);
    CHECK_TRUE(configJson.contains("entries"));
    CHECK_TRUE(configJson.contains("locked_keys"));
    CHECK_TRUE(configJson.contains("active_profile"));
    CHECK_FALSE(configJson["entries"].contains("bot_token"));
    // Verify non-empty effective defaults for board & tactical weights and policy
    CHECK_TRUE(configJson["entries"].contains("captured_enemy_base_bonus"));
    CHECK_TRUE(!configJson["entries"]["captured_enemy_base_bonus"].get<std::string>().empty());
    CHECK_TRUE(configJson["entries"].contains("pod_zero_prong_val"));
    CHECK_TRUE(!configJson["entries"]["pod_zero_prong_val"].get<std::string>().empty());
    CHECK_TRUE(configJson["entries"].contains("leapfrog_battery_weight"));
    CHECK_TRUE(!configJson["entries"]["leapfrog_battery_weight"].get<std::string>().empty());
    CHECK_TRUE(configJson["entries"].contains("table_behavior"));
    CHECK_EQUAL("auto", configJson["entries"]["table_behavior"].get<std::string>());
    CHECK_TRUE(configJson["entries"].contains("accept_modes"));
    CHECK_TRUE(configJson["entries"].contains("ai_personality"));
    CHECK_TRUE(configJson["entries"]["ai_personality"].get<std::string>().find("mellow, friendly") != std::string::npos);

    // 5. POST /api/v1/config -> JSON update
    nlohmann::json updateReq;
    updateReq["updates"]["verbosity"] = "2";
    updateReq["updates"]["captured_enemy_base_bonus"] = "35000";
    updateReq["save_to_disk"] = false;
    auto resPost = client.Post("/api/v1/config", updateReq.dump(), "application/json");
    CHECK_TRUE(resPost != nullptr);
    CHECK_EQUAL(200, resPost->status);
    CHECK_EQUAL(2, runner.verbosity());

    // 6. POST /api/v1/config with locked key -> 400
    nlohmann::json badPost;
    badPost["updates"]["aimon_port"] = "9999";
    auto resBad = client.Post("/api/v1/config", badPost.dump(), "application/json");
    CHECK_TRUE(resBad != nullptr);
    CHECK_EQUAL(400, resBad->status);

    server.stopHttpServer();
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
