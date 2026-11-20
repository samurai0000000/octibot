/*
 * OctiBotPolicy.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_POLICY_HXX
#define OCTIBOT_POLICY_HXX

#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "OctiBotConfig.hxx"
#include "OctiNetworkClient.hxx"

namespace octi {

struct HostVariantSpec {
    std::string name;
    std::string mode = "2P";
    int bases = 3;
    bool superProngs = false;
    bool edgeless = false;
};

class OctiBotPolicyEngine {
public:
    OctiBotPolicyEngine() = default;
    explicit OctiBotPolicyEngine(const OctiBotConfigStore &config);

    static bool parseHostVariants(const std::string &raw,
                                  std::vector<HostVariantSpec> &outSpecs,
                                  std::string &errorOut);

    void configure(const OctiBotConfigStore &config);
    bool configure(const OctiBotConfigStore &config, std::string &errorOut);
    const std::vector<HostVariantSpec> &hostVariants() const { return _hostVariants; }

    bool isFriend(const std::string &handle) const;
    bool isBanned(const std::string &handle) const;
    void addFriend(const std::string &handle);
    void removeFriend(const std::string &handle);
    void addBan(const std::string &handle);
    void removeBan(const std::string &handle);

    const std::set<std::string> &friends() const { return _friendList; }
    const std::set<std::string> &banned() const { return _banList; }

    bool isTableCompatible(const TableSummaryInfo &table,
                           const std::string &ownerHandle,
                           const std::string &myHandle) const;

    std::vector<TableSummaryInfo> rankCompatibleTables(
        const std::vector<TableSummaryInfo> &tables,
        const std::string &ownerHandle,
        const std::string &myHandle) const;

    bool shouldKickSeatedPlayer(const TableSeatInfo &seat,
                                const std::string &ownerHandle) const;

    bool shouldAcceptProposal(const GameProposalInfo &proposal,
                              const std::string &proposerHandle,
                              int currentEvalScore,
                              int consecutiveRematches) const;

    CreateTableRequest buildHostTableRequest() const;
    MatchmakingRequest buildMatchmakingRequest() const;

    const std::string &tableBehavior() const { return _tableBehavior; }
    void setTableBehavior(const std::string &b) { _tableBehavior = b; }

    bool friendsOnly() const { return _friendsOnly; }
    void setFriendsOnly(bool fo) { _friendsOnly = fo; }
    const std::string &opponentFilter() const { return _opponentFilter; }
    const std::string &hostSeat() const { return _hostSeat; }
    void setHostSeat(const std::string &s) { _hostSeat = s; }

    int claimForfeitDelaySec() const { return _claimForfeitDelaySec; }
    int minMoveThinkMs() const { return _minMoveThinkMs; }
    int maxConsecutiveRematches() const { return _maxConsecutiveRematches; }

private:
    mutable std::mutex _mutex;
    std::string _tableBehavior = "auto";
    std::set<std::string> _acceptModes = {"2P", "4P", "OFK"};
    std::string _acceptBases = "any";
    std::string _acceptSuperProngs = "any";
    std::string _acceptEdgeless = "any";
    std::string _acceptTimeControl = "any";
    int _minInitialTimeSec = 0;
    int _maxInitialTimeSec = 14400;
    int _minIncrementSec = 0;
    std::string _acceptTakeback = "any";

    std::set<std::string> _friendList;
    std::set<std::string> _banList;
    bool _friendsOnly = false;
    std::string _opponentFilter = "any";

    int _acceptDrawEvalThreshold = -1500;
    std::string _acceptUndo = "friends_only";
    std::string _acceptPause = "always";
    std::string _acceptRematch = "always";
    int _maxConsecutiveRematches = 5;
    int _claimForfeitDelaySec = 10;
    int _minMoveThinkMs = 0;

    // Hosting defaults
    std::string _hostMode = "2P";
    int _hostBases = 3;
    bool _hostSuperProngs = false;
    bool _hostEdgeless = false;
    bool _hostTimeControlEnabled = false;
    std::int64_t _hostInitialMs = 600000;
    std::int64_t _hostIncrementMs = 5000;
    bool _hostAllowRobots = true;
    std::string _hostSeat = "wait_for_human";
    std::vector<HostVariantSpec> _hostVariants;
    mutable std::size_t _nextHostVariantIndex = 0;
};

} // namespace octi

#endif /* OCTIBOT_POLICY_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
