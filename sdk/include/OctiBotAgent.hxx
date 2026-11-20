/*
 * OctiBotAgent.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_AGENT_HXX
#define OCTIBOT_AGENT_HXX

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "OctiBoard.hxx"
#include "OctiBotConfig.hxx"
#include "OctiBotEngine.hxx"
#include "OctiDefs.hxx"
#include "OctiNetworkClient.hxx"

namespace octi {

struct OctiVariantSpec {
    std::string mode = "2P"; // Canonical e.g. "2P", "4P", "OFK"
    int bases = 3;           // 1 or 3 (OFK normalized to 1)
    bool superProngs = false;
    bool edgeless = false;

    static OctiVariantSpec fromCode(const std::string &code) {
        OctiVariantSpec spec;
        OctiGameMode pm = GAME_MODE_2;
        int pb = 3;
        bool psp = false;
        bool ped = false;
        if (parseOctiVariantCode(code, pm, pb, psp, ped)) {
            spec.mode = (pm == GAME_MODE_4) ? "4P" : ((pm == GAME_MODE_OFK) ? "OFK" : "2P");
            spec.bases = pb;
            spec.superProngs = psp;
            spec.edgeless = ped;
        } else {
            spec.mode = code;
        }
        return spec;
    }

    static OctiVariantSpec fromTable(const TableSummaryInfo &t) {
        if (!t.variant.empty() && isValidOctiVariantCode(t.variant)) {
            return fromCode(t.variantCode());
        }
        OctiVariantSpec spec;
        spec.mode = t.mode;
        spec.bases = (t.mode == "OFK") ? 1 : t.bases;
        spec.superProngs = (t.mode == "OFK") ? false : t.superProngs;
        spec.edgeless = (t.mode == "OFK") ? false : t.edgeless;
        return spec;
    }

    static OctiVariantSpec fromGameView(const GameViewData &gv) {
        if (!gv.variant.empty() && isValidOctiVariantCode(gv.variant)) {
            return fromCode(gv.variantCode());
        }
        OctiVariantSpec spec;
        spec.mode = gv.mode;
        spec.bases = (gv.mode == "OFK") ? 1 : gv.bases;
        spec.superProngs = (gv.mode == "OFK") ? false : gv.superProngs;
        spec.edgeless = (gv.mode == "OFK") ? false : gv.edgeless;
        return spec;
    }

    static OctiVariantSpec fromHostRequest(const CreateTableRequest &req) {
        if (!req.variant.empty() && isValidOctiVariantCode(req.variant)) {
            return fromCode(req.variantCode());
        }
        OctiVariantSpec spec;
        spec.mode = req.mode;
        spec.bases = (req.mode == "OFK") ? 1 : req.bases;
        spec.superProngs = (req.mode == "OFK") ? false : req.superProngs;
        spec.edgeless = (req.mode == "OFK") ? false : req.edgeless;
        return spec;
    }

    static OctiVariantSpec fromMatchmaking(const MatchmakingRequest &req) {
        if (!req.variant.empty() && isValidOctiVariantCode(req.variant)) {
            return fromCode(req.variantCode());
        }
        OctiVariantSpec spec;
        spec.mode = req.mode;
        spec.bases = (req.mode == "OFK") ? 1 : req.bases;
        spec.superProngs = (req.mode == "OFK") ? false : req.superProngs;
        spec.edgeless = (req.mode == "OFK") ? false : req.edgeless;
        return spec;
    }
};

inline std::string formatVariantLabel(const OctiVariantSpec &spec) {
    return octiVariantCodeFromModeStr(spec.mode, spec.bases, spec.superProngs, spec.edgeless);
}

struct BotIdentity {
    std::string robotUuid;
    std::string handle;       // Registered Robot Name (e.g. "stock")
    std::string description;  // Registered Robot Description
    std::string ownerUuid;
    std::string ownerHandle;
    std::string activeRoomId; // Non-empty if resuming an active room on connect
    int gamesPlayed = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
};

struct BotThinkMilestoneEvent {
    int milestoneIndex = 0;
    std::int64_t elapsedMs = 0;
    std::int64_t maxBudgetMs = 0;
    int depthReached = 0;
    std::uint64_t nodesVisited = 0;
    int evalScore = 0;
    std::string reason;
};

struct BotMoveTelemetry {
    int depthReached = 0;
    std::int64_t nodesVisited = 0;
    int evalScore = 0;
    std::int64_t elapsedMs = 0;
    bool present = false;
    bool adaptiveExtended = false;
    bool hitAdaptiveMaxBudget = false;
    std::int64_t adaptiveMaxBudgetMs = 0;
};

struct AgentPromptRequest {
    std::string kind;            // "chat_lobby", "chat_game", "game_start", "game_over", "goodbye", "eval_swing", "thinking_mumble", "thinking_mumble_post_move"
    std::string roomId;
    std::string senderHandle;    // chat sender or opponent for events
    bool senderIsOwner = false;
    std::vector<GameSeatInfo> seats;
    int mySeat = 0;
};

struct AgentPromptAddition {
    std::string guidance;        // trusted text from agent code, placed before <UNTRUSTED_NETWORK_DATA>
    std::string contextLines;    // placed inside <UNTRUSTED_NETWORK_DATA>, after "Live Context"
    bool suppress = false;       // proactive events only: skip this dispatch
};

struct BotResourceRequirements {
    int cpuThreads = 1;                  // Concurrent CPU search/worker threads during computeMove()
    bool usesGpu = false;                // True when GPU/NPU execution provider is active
    int gpuStreams = 0;                  // Concurrent GPU compute streams required (0 or 1)
    std::uint64_t staticRamMiB = 0;      // Persistent per-agent RAM (TT, weights, NN cache, worker boards)
    std::uint64_t scratchRamMiB = 0;     // Per-search active CPU scratch RAM
    std::uint64_t staticVramMiB = 0;     // Persistent GPU VRAM (ORT session + loaded weights)
    std::uint64_t scratchVramMiB = 0;    // Active GPU inference workspace + batch tensors
    std::string backendLabel;            // e.g. "cc_play(cpu:36T)", "search_worker(gpu:4W,bs=64)"
};

class OctiBotAgent {
public:
    using ThinkMilestoneCallback = std::function<void(const BotThinkMilestoneEvent &)>;

    virtual ~OctiBotAgent() = default;

    virtual void onConfigure(const OctiBotConfigStore &config) { (void) config; }
    virtual void onConnected(const BotIdentity &identity) { (void) identity; }
    virtual void onLobbyChat(const LobbyChatEntry &msg,
                             std::string &outReply) {
        (void) msg;
        (void) outReply;
    }
    virtual void onGameStarted(const GameViewData &game,
                               int mySeat,
                               std::string &outGreetingChat) {
        (void) game;
        (void) mySeat;
        (void) outGreetingChat;
    }
    virtual std::string computeMove(const OctiBoard &board,
                                    const GameViewData &game,
                                    int mySeat,
                                    std::int64_t remainingClockMs,
                                    BotMoveTelemetry *outTelemetry = nullptr) = 0;
    virtual bool onProposal(const GameProposalInfo &proposal,
                            const GameViewData &game) {
        (void) proposal;
        (void) game;
        return true;
    }
    virtual void onGameChat(const std::string &senderHandle,
                            const std::string &text,
                            std::string &outReply) {
        (void) senderHandle;
        (void) text;
        (void) outReply;
    }
    virtual void onGameOver(const GameViewData &game, int mySeat) {
        (void) game;
        (void) mySeat;
    }
    virtual AgentPromptAddition onBuildPrompt(const AgentPromptRequest &req) {
        (void) req;
        return {};
    }
    virtual bool supportsVariant(const OctiVariantSpec &spec,
                                 std::string &outUnsupportedReason) const {
        (void) spec;
        outUnsupportedReason.clear();
        return true;
    }
    virtual BotResourceRequirements resourceRequirements() const {
        BotResourceRequirements req;
        req.cpuThreads = 1;
        req.staticRamMiB = 16;
        req.scratchRamMiB = 8;
        req.backendLabel = "generic";
        return req;
    }
    virtual void setSearchAffinityPlan(std::size_t slotOffset, bool allowPinning) {
        (void) slotOffset;
        (void) allowPinning;
    }
    virtual void setThinkMilestoneCallback(ThinkMilestoneCallback cb) {
        (void) cb;
    }
    virtual void setThinkMumbleIntervalForTest(int intervalMs) {
        (void) intervalMs;
    }
};

class StockAlphaBetaAgent : public OctiBotAgent {
public:
    using ProgressCallback = std::function<void(int depth, std::uint64_t nodes, int score)>;

    StockAlphaBetaAgent() = default;
    explicit StockAlphaBetaAgent(const OctiBotConfigStore &config);

    void onConfigure(const OctiBotConfigStore &config) override;
    BotResourceRequirements resourceRequirements() const override;
    std::string computeMove(const OctiBoard &board,
                            const GameViewData &game,
                            int mySeat,
                            std::int64_t remainingClockMs,
                            BotMoveTelemetry *outTelemetry = nullptr) override;

    OctiBotEngine &engine() { return ensureEngine(); }
    const BotTuningProfile &profile() const { return _profile; }
    void setProgressCallback(ProgressCallback cb) { _progressCallback = std::move(cb); }

protected:
    virtual std::unique_ptr<OctiBotEngine> createEngine() const;
    OctiBotEngine &ensureEngine();
    OctiBotEngine *existingEngine() noexcept { return _engine.get(); }
    const OctiBotEngine *existingEngine() const noexcept { return _engine.get(); }
    BotTuningProfile &mutableProfile() { return _profile; }

private:
    std::unique_ptr<OctiBotEngine> _engine;
    BotTuningProfile _profile;
    int _verbosity = 2;
    ProgressCallback _progressCallback;
};

class CentaurMcpAgent : public StockAlphaBetaAgent {
public:
    using StockAlphaBetaAgent::StockAlphaBetaAgent;
};

class MinimalStarterAgent : public OctiBotAgent {
public:
    MinimalStarterAgent() = default;
    explicit MinimalStarterAgent(const OctiBotConfigStore &config);

    void onConfigure(const OctiBotConfigStore &config) override;
    void onConnected(const BotIdentity &identity) override;
    void onGameStarted(const GameViewData &game, int mySeat,
                       std::string &outGreetingChat) override;
    BotResourceRequirements resourceRequirements() const override;
    std::string computeMove(const OctiBoard &board,
                            const GameViewData &game,
                            int mySeat,
                            std::int64_t remainingClockMs,
                            BotMoveTelemetry *outTelemetry = nullptr) override;

private:
    double _aggression = 1.0;
    bool _preferJumps = true;
};

class LobbyHelperAgent : public OctiBotAgent {
public:
    LobbyHelperAgent() = default;
    explicit LobbyHelperAgent(const OctiBotConfigStore &config);

    void onConfigure(const OctiBotConfigStore &config) override;
    BotResourceRequirements resourceRequirements() const override;
    std::string computeMove(const OctiBoard &board,
                            const GameViewData &game,
                            int mySeat,
                            std::int64_t remainingClockMs,
                            BotMoveTelemetry *outTelemetry = nullptr) override;
};

class OctiBotAgentRegistry {
public:
    using AgentFactory = std::function<std::unique_ptr<OctiBotAgent>(const OctiBotConfigStore &)>;

    static OctiBotAgentRegistry &instance();

    void registerAgent(const std::string &algorithmName, AgentFactory factory);
    bool hasAgent(const std::string &algorithmName) const;
    std::unique_ptr<OctiBotAgent> createAgent(const std::string &algorithmName,
                                              const OctiBotConfigStore &config) const;
    std::vector<std::string> registeredAlgorithms() const;

private:
    OctiBotAgentRegistry();
    mutable std::mutex _mutex;
    std::map<std::string, AgentFactory> _factories;
};

} // namespace octi

#endif /* OCTIBOT_AGENT_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
