/*
 * OctiNetworkClient.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

#include "OctiNetworkClient.hxx"

#include <httplib.h>
#include <nlohmann/json.hpp>

#ifndef _WIN32
#include <csignal>
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <thread>

namespace octi {

namespace {

#ifdef _WIN32
std::wstring toWide(const std::string &str)
{
    if (str.empty()) {
        return std::wstring();
    }
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) {
        return std::wstring();
    }
    std::vector<wchar_t> buf(len);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf.data(), len);
    return std::wstring(buf.data());
}
#endif

typedef nlohmann::json Json;

std::string toLowerAscii(const std::string &s)
{
    std::string out = s;
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<char>(
            std::tolower(static_cast<unsigned char>(out[i])));
    }
    return out;
}

std::string toUpperAscii(const std::string &s)
{
    std::string out = s;
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(out[i])));
    }
    return out;
}

std::string urlEncode(const std::string &val)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(val.size() * 3);
    for (std::size_t i = 0; i < val.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(val[i]);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[(c >> 4) & 0x0F]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

LinkedIdentityInfo parseLinkedIdentity(const Json &j)
{
    LinkedIdentityInfo info;
    if (!j.is_object()) {
        return info;
    }
    info.provider = j.value("provider", "");
    info.providerEmail = j.value("provider_email", "");
    info.displayHint = j.value("display_hint", "");
    info.linkedAt = j.value("linked_at", static_cast<std::int64_t>(0));
    info.lastLoginAt = j.value("last_login_at", static_cast<std::int64_t>(0));
    return info;
}

std::vector<LinkedIdentityInfo> parseLinkedIdentities(const Json &arr)
{
    std::vector<LinkedIdentityInfo> out;
    if (!arr.is_array()) {
        return out;
    }
    for (std::size_t i = 0; i < arr.size(); ++i) {
        out.push_back(parseLinkedIdentity(arr[i]));
    }
    return out;
}

ArchivedGameSummary parseArchivedGameSummary(const Json &j)
{
    ArchivedGameSummary g;
    if (!j.is_object()) {
        return g;
    }
    g.gameId = j.value("game_id", j.value("room_id", ""));
    g.mode = j.value("mode", "2P");
    g.bases = j.value("bases", 3);
    g.superProngs = j.value("super_prongs", false);
    g.edgeless = j.value("edgeless", false);
    if (j.contains("time_control") && j["time_control"].is_string()) {
        g.timeControl = j["time_control"].get<std::string>();
    }
    if (j.contains("players") && j["players"].is_array()) {
        const Json &pl = j["players"];
        for (std::size_t i = 0; i < pl.size(); ++i) {
            if (!pl[i].is_object()) {
                continue;
            }
            ArchivedGamePlayer p;
            p.seat = pl[i].value("seat", static_cast<int>(i) + 1);
            p.handle = pl[i].value("handle", "");
            if (pl[i].contains("player_uuid") &&
                pl[i]["player_uuid"].is_string()) {
                p.playerUuid = pl[i]["player_uuid"].get<std::string>();
            }
            g.players.push_back(p);
        }
    }
    g.result = j.value("result", "");
    g.winnerSeat = j.value("winner_seat", 0);
    g.termination = j.value("termination", "");
    g.plyCount = j.value("ply_count", 0);
    g.startedAt = j.value("started_at", static_cast<std::int64_t>(0));
    g.endedAt = j.value("ended_at", static_cast<std::int64_t>(0));
    g.finalFen = j.value("final_fen", j.value("fen", ""));
    g.octiTranscript = j.value("octi_transcript", "");
    return g;
}

PlayerProfileInfo parsePlayerProfile(const Json &j)
{
    PlayerProfileInfo p;
    if (!j.is_object()) {
        return p;
    }
    p.uuid = j.value("uuid", "");
    p.handle = j.value("handle", "");
    p.displayName = j.value("display_name", "");
    p.avatarUrl = j.value("avatar_url", "");
    p.bio = j.value("bio", "");
    p.location = j.value("location", "");
    p.elo2p = j.value("elo_2p", 1200);
    p.elo4p = j.value("elo_4p", 1200);
    p.eloOfk = j.value("elo_ofk", 1200);
    p.gamesPlayed = j.value("games_played", 0);
    p.wins = j.value("wins", 0);
    p.losses = j.value("losses", 0);
    p.draws = j.value("draws", 0);
    p.createdAt = j.value("created_at", static_cast<std::int64_t>(0));
    p.lastSeenAt = j.value("last_seen_at", static_cast<std::int64_t>(0));
    p.isAdmin = j.value("is_admin", false);
    p.isRobot = j.value("is_robot", false);
    p.robotUuid = j.value("robot_uuid", "");
    p.description = j.value("description", j.value("robot_description", ""));
    p.ownerUuid = j.value("owner_uuid", "");
    p.ownerHandle = j.value("owner_handle", "");
    p.robotGamesPlayed = j.value("robot_games_played", p.isRobot ? p.gamesPlayed : 0);
    p.robotWins = j.value("robot_wins", p.isRobot ? p.wins : 0);
    p.robotLosses = j.value("robot_losses", p.isRobot ? p.losses : 0);
    p.robotDraws = j.value("robot_draws", p.isRobot ? p.draws : 0);
    if (j.contains("linked_identities")) {
        p.identities = parseLinkedIdentities(j["linked_identities"]);
    }
    if (j.contains("recent_games") && j["recent_games"].is_array()) {
        const Json &rg = j["recent_games"];
        for (std::size_t i = 0; i < rg.size(); ++i) {
            p.recentGames.push_back(parseArchivedGameSummary(rg[i]));
        }
    }
    return p;
}

TableSummaryInfo parseTableSummary(const Json &j)
{
    TableSummaryInfo t;
    if (!j.is_object()) {
        return t;
    }
    t.roomId = j.value("room_id", "");
    t.mode = j.value("mode", "2P");
    t.bases = j.value("bases", 3);
    t.superProngs = j.value("super_prongs", false);
    t.edgeless = j.value("edgeless", false);
    if (j.contains("time_control") && j["time_control"].is_object()) {
        const Json &tc = j["time_control"];
        t.timeControlEnabled = tc.value("enabled", false);
        t.initialMs = tc.value("initial_ms", static_cast<std::int64_t>(0));
        t.incrementMs = tc.value("increment_ms", static_cast<std::int64_t>(0));
        t.disconnectGraceMs =
            tc.value("disconnect_grace_ms", static_cast<std::int64_t>(90000));
    }
    t.rated = j.value("rated", true);
    t.allowTakeback = j.value("allow_takeback", false);
    t.observerPolicy = j.value("observer_policy", "OPEN");
    t.voicePolicy = j.value("voice_policy", "PLAYERS_ONLY");
    t.phase = j.value("phase", "WAITING_FOR_PLAYERS");
    t.hasPasscode = j.value("has_passcode", false);
    t.allowRobots = j.value("allow_robots", true);
    if (j.contains("host_uuid") && j["host_uuid"].is_string()) {
        t.hostUuid = j["host_uuid"].get<std::string>();
    }
    t.hostHandle = j.value("host_handle", "");
    t.createdAtMs =
        j.value("created_at_ms", static_cast<std::int64_t>(0));
    t.lastMoveAtMs =
        j.value("last_move_at_ms", static_cast<std::int64_t>(0));
    t.idleMs = j.value("idle_ms", static_cast<std::int64_t>(0));
    t.ttlRemainingMs =
        j.value("ttl_remaining_ms", static_cast<std::int64_t>(-1));
    t.seatsTotal = j.value("seats_total", 2);
    t.seatsFilled = j.value("seats_filled", 0);
    if (j.contains("seats") && j["seats"].is_array()) {
        const Json &seatsArr = j["seats"];
        for (std::size_t i = 0; i < seatsArr.size(); ++i) {
            if (!seatsArr[i].is_object()) {
                continue;
            }
            TableSeatInfo si;
            si.seat = seatsArr[i].value("seat", static_cast<int>(i) + 1);
            if (seatsArr[i].contains("player_uuid") &&
                seatsArr[i]["player_uuid"].is_string()) {
                si.playerUuid = seatsArr[i]["player_uuid"].get<std::string>();
            }
            if (seatsArr[i].contains("handle") &&
                seatsArr[i]["handle"].is_string()) {
                si.handle = seatsArr[i]["handle"].get<std::string>();
            }
            si.rating = seatsArr[i].value("rating", 1500);
            si.connected = seatsArr[i].value("connected", false);
            si.isRobot = seatsArr[i].value("is_robot", false);
            si.robotUuid = seatsArr[i].value("robot_uuid", "");
            si.description = seatsArr[i].value(
                "description", seatsArr[i].value("robot_description", ""));
            si.ownerUuid = seatsArr[i].value("owner_uuid", "");
            si.ownerHandle = seatsArr[i].value("owner_handle", "");
            si.avatarUrl = seatsArr[i].value("avatar_url", "");
            int gp = seatsArr[i].value("games_played", 0);
            int w = seatsArr[i].value("wins", 0);
            int l = seatsArr[i].value("losses", 0);
            int d = seatsArr[i].value("draws", 0);
            si.robotGamesPlayed =
                seatsArr[i].value("robot_games_played", si.isRobot ? gp : 0);
            si.robotWins = seatsArr[i].value("robot_wins", si.isRobot ? w : 0);
            si.robotLosses = seatsArr[i].value("robot_losses", si.isRobot ? l : 0);
            si.robotDraws = seatsArr[i].value("robot_draws", si.isRobot ? d : 0);
            t.seats.push_back(si);
        }
    }
    if (j.contains("players") && j["players"].is_array()) {
        for (std::size_t i = 0; i < j["players"].size(); ++i) {
            if (j["players"][i].is_string()) {
                t.players.push_back(j["players"][i].get<std::string>());
            }
        }
    }
    t.observerCount = j.value("observer_count", 0);
    return t;
}

GameViewData parseGameView(const Json &j)
{
    GameViewData v;
    if (!j.is_object()) {
        return v;
    }
    v.live = j.value("live", true);
    v.roomId = j.value("room_id", j.value("game_id", ""));
    v.mode = j.value("mode", "2P");
    v.bases = j.value("bases", 3);
    v.superProngs = j.value("super_prongs", false);
    v.edgeless = j.value("edgeless", false);
    if (j.contains("time_control") && j["time_control"].is_object()) {
        const Json &tc = j["time_control"];
        v.timeControlEnabled = tc.value("enabled", false);
        v.initialMs = tc.value("initial_ms", static_cast<std::int64_t>(0));
        v.incrementMs = tc.value("increment_ms", static_cast<std::int64_t>(0));
        v.disconnectGraceMs =
            tc.value("disconnect_grace_ms", static_cast<std::int64_t>(90000));
    }
    v.rated = j.value("rated", true);
    v.allowTakeback = j.value("allow_takeback", false);
    v.observerPolicy = j.value("observer_policy", "OPEN");
    v.voicePolicy = j.value("voice_policy", "PLAYERS_ONLY");
    v.phase = j.value("phase", "WAITING_FOR_PLAYERS");
    v.paused = j.value("paused", false);
    v.hasPasscode = j.value("has_passcode", false);
    v.hostUuid = j.value("host_uuid", "");
    v.allowRobots = j.value("allow_robots", true);
    v.viewerSeat = j.value("viewer_seat", 0);
    if (j.contains("seats") && j["seats"].is_array()) {
        const Json &arr = j["seats"];
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].is_object()) {
                continue;
            }
            GameSeatInfo s;
            s.seat = arr[i].value("seat", static_cast<int>(i) + 1);
            if (arr[i].contains("player_uuid") &&
                arr[i]["player_uuid"].is_string()) {
                s.playerUuid = arr[i]["player_uuid"].get<std::string>();
            }
            s.handle = arr[i].value("handle", "");
            s.rating = arr[i].value("rating", 1200);
            s.connected = arr[i].value("connected", true);
            s.disconnectedForMs = arr[i].value(
                "disconnected_for_ms", static_cast<std::int64_t>(0));
            s.forfeitClaimable = arr[i].value("forfeit_claimable", false);
            s.clockMs =
                arr[i].value("clock_ms", static_cast<std::int64_t>(-1));
            s.eliminated = arr[i].value("eliminated", false);
            s.isRobot = arr[i].value("is_robot", false);
            s.robotUuid = arr[i].value("robot_uuid", "");
            s.description = arr[i].value(
                "description", arr[i].value("robot_description", ""));
            s.ownerUuid = arr[i].value("owner_uuid", "");
            s.ownerHandle = arr[i].value("owner_handle", "");
            s.avatarUrl = arr[i].value("avatar_url", "");
            int gp = arr[i].value("games_played", 0);
            int w = arr[i].value("wins", 0);
            int l = arr[i].value("losses", 0);
            int d = arr[i].value("draws", 0);
            s.robotGamesPlayed =
                arr[i].value("robot_games_played", s.isRobot ? gp : 0);
            s.robotWins = arr[i].value("robot_wins", s.isRobot ? w : 0);
            s.robotLosses = arr[i].value("robot_losses", s.isRobot ? l : 0);
            s.robotDraws = arr[i].value("robot_draws", s.isRobot ? d : 0);
            v.seats.push_back(s);
        }
    }
    if (j.contains("observers") && j["observers"].is_array()) {
        for (std::size_t i = 0; i < j["observers"].size(); ++i) {
            if (j["observers"][i].is_string()) {
                v.observers.push_back(j["observers"][i].get<std::string>());
            }
        }
    }
    v.observerCount =
        j.value("observer_count", static_cast<int>(v.observers.size()));
    v.fen = j.value("fen", "");
    v.turnSeat = j.value("turn_seat", 0);
    v.plyCount = j.value("ply_count", 0);
    if (j.contains("moves") && j["moves"].is_array()) {
        for (std::size_t i = 0; i < j["moves"].size(); ++i) {
            if (j["moves"][i].is_string()) {
                v.moves.push_back(j["moves"][i].get<std::string>());
            }
        }
    }
    v.result = j.value("result", "");
    v.termination = j.value("termination", "");
    v.winnerSeat = j.value("winner_seat", 0);
    if (j.contains("proposals") && j["proposals"].is_array()) {
        const Json &pr = j["proposals"];
        for (std::size_t i = 0; i < pr.size(); ++i) {
            if (!pr[i].is_object()) {
                continue;
            }
            GameProposalInfo p;
            p.type = pr[i].value("type", "");
            p.proposerUuid = pr[i].value("proposer_uuid", "");
            if (pr[i].contains("accepted_by") &&
                pr[i]["accepted_by"].is_array()) {
                for (std::size_t k = 0; k < pr[i]["accepted_by"].size(); ++k) {
                    if (pr[i]["accepted_by"][k].is_string()) {
                        p.acceptedBy.push_back(
                            pr[i]["accepted_by"][k].get<std::string>());
                    }
                }
            }
            if (pr[i].contains("awaiting") && pr[i]["awaiting"].is_array()) {
                for (std::size_t k = 0; k < pr[i]["awaiting"].size(); ++k) {
                    if (pr[i]["awaiting"][k].is_string()) {
                        p.awaiting.push_back(
                            pr[i]["awaiting"][k].get<std::string>());
                    }
                }
            }
            p.expiresAtMs = pr[i].value(
                "expires_at_ms", static_cast<std::int64_t>(0));
            v.proposals.push_back(p);
        }
    }
    if (j.contains("voice_peers") && j["voice_peers"].is_array()) {
        const Json &vp = j["voice_peers"];
        for (std::size_t i = 0; i < vp.size(); ++i) {
            if (!vp[i].is_object()) {
                continue;
            }
            VoicePeerInfo peer;
            peer.playerUuid = vp[i].value("player_uuid", "");
            peer.micMuted = vp[i].value("mic_muted", false);
            v.voicePeers.push_back(peer);
        }
    }
    v.seq = j.value("seq", static_cast<std::int64_t>(0));
    v.archived = j.value("archived", false);
    v.rematchRoomId = j.value("rematch_room_id", "");
    v.createdAtMs = j.value("created_at_ms", static_cast<std::int64_t>(0));
    v.lastMoveAtMs =
        j.value("last_move_at_ms", static_cast<std::int64_t>(0));
    v.idleMs = j.value("idle_ms", static_cast<std::int64_t>(0));
    v.ttlRemainingMs =
        j.value("ttl_remaining_ms", static_cast<std::int64_t>(-1));
    return v;
}

} // namespace

/* ================================================================== */
/* OAuthLoopbackListener                                              */
/* ================================================================== */

struct OAuthLoopbackListener::Impl {
    httplib::Server server;
    std::thread listenThread;
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> running{false};
    std::atomic<int> port{0};
    bool received = false;
    std::string provider;
    std::string token;
    std::function<void(const std::string &, const std::string &)> callback;

    ~Impl()
    {
        stop();
    }

    bool start(
        std::function<void(const std::string &, const std::string &)> cb,
        std::string &errorOut)
    {
        stop();
        {
            std::lock_guard<std::mutex> lock(mutex);
            received = false;
            provider.clear();
            token.clear();
            callback = std::move(cb);
        }

        auto handleOAuthRedirect =
            [this](const httplib::Request &req, httplib::Response &res) {
                std::string prov = "google";
                if (req.has_param("provider")) {
                    prov = req.get_param_value("provider");
                }
                std::string tok;
                if (req.has_param("token")) {
                    tok = req.get_param_value("token");
                } else if (req.has_param("id_token")) {
                    tok = req.get_param_value("id_token");
                } else if (req.has_param("access_token")) {
                    tok = req.get_param_value("access_token");
                }

                if (tok.empty()) {
                    res.status = 400;
                    res.set_content(
                        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                        "<title>OCTI Sign-In Incomplete</title></head>"
                        "<body><h2>OCTI Sign-In Incomplete: missing token</h2>"
                        "</body></html>",
                        "text/html; charset=utf-8");
                    return;
                }

                std::function<void(const std::string &, const std::string &)>
                    cbCopy;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    received = true;
                    provider = prov;
                    token = tok;
                    cbCopy = callback;
                }
                cv.notify_all();
                if (cbCopy) {
                    cbCopy(prov, tok);
                }

                res.status = 200;
                res.set_content(
                    "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                    "<title>Sign-in complete</title></head>"
                    "<body style=\"background:#0B0F17;color:#F8FAFC;"
                    "font-family:sans-serif;text-align:center;padding:48px;\">"
                    "<h2 style=\"color:#C8A253;\">Sign-in complete — "
                    "you may return to OCTI</h2>"
                    "<p>Your desktop session has been authenticated.</p>"
                    "</body></html>",
                    "text/html; charset=utf-8");
            };

        server.Get("/callback", handleOAuthRedirect);
        server.Get("/oauth/callback", handleOAuthRedirect);

        int bound = server.bind_to_any_port("127.0.0.1");
        if (bound <= 0) {
            errorOut = "failed to bind OAuth loopback listener on 127.0.0.1:0";
            return false;
        }
        port.store(bound);
        running.store(true);
        listenThread = std::thread([this]() {
            server.listen_after_bind();
            running.store(false);
        });
        server.wait_until_ready();
        return true;
    }

    void stop()
    {
        if (server.is_running()) {
            server.stop();
        }
        running.store(false);
        cv.notify_all();
        if (listenThread.joinable()) {
            listenThread.join();
        }
        port.store(0);
    }
};

OAuthLoopbackListener::OAuthLoopbackListener()
    : _impl(new Impl())
{
}

OAuthLoopbackListener::~OAuthLoopbackListener()
{
    stop();
}

bool OAuthLoopbackListener::start(
    std::function<void(const std::string &provider,
                       const std::string &token)> onCallback,
    std::string &errorOut)
{
    return _impl->start(std::move(onCallback), errorOut);
}

void OAuthLoopbackListener::stop()
{
    _impl->stop();
}

bool OAuthLoopbackListener::isRunning() const
{
    return _impl->running.load();
}

int OAuthLoopbackListener::boundPort() const
{
    return _impl->port.load();
}

std::string OAuthLoopbackListener::callbackUrl() const
{
    int p = boundPort();
    if (p <= 0) {
        return std::string();
    }
    return "http://127.0.0.1:" + std::to_string(p) + "/callback";
}

bool OAuthLoopbackListener::waitForCallback(
    int timeoutMs,
    std::string &outProvider,
    std::string &outToken)
{
    std::unique_lock<std::mutex> lock(_impl->mutex);
    if (!_impl->received && timeoutMs > 0) {
        _impl->cv.wait_for(
            lock, std::chrono::milliseconds(timeoutMs),
            [this]() { return _impl->received || !_impl->running.load(); });
    }
    if (_impl->received) {
        outProvider = _impl->provider;
        outToken = _impl->token;
        return true;
    }
    return false;
}

/* ================================================================== */
/* OctiNetworkClient::parseSseBuffer                                  */
/* ================================================================== */

std::vector<SseEvent> OctiNetworkClient::parseSseBuffer(std::string &buffer)
{
    std::vector<SseEvent> events;

    for (;;) {
        std::size_t sepPos = std::string::npos;
        std::size_t sepLen = 0;
        for (std::size_t i = 0; i + 1 < buffer.size(); ++i) {
            if (buffer[i] == '\n' && buffer[i + 1] == '\n') {
                sepPos = i;
                sepLen = 2;
                break;
            }
            if (i + 3 < buffer.size() && buffer[i] == '\r' &&
                buffer[i + 1] == '\n' && buffer[i + 2] == '\r' &&
                buffer[i + 3] == '\n') {
                sepPos = i;
                sepLen = 4;
                break;
            }
        }
        if (sepPos == std::string::npos) {
            break;
        }

        std::string rawBlock = buffer.substr(0, sepPos);
        buffer.erase(0, sepPos + sepLen);

        SseEvent ev;
        bool hasEventOrData = false;
        std::istringstream iss(rawBlock);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty() || line[0] == ':') {
                continue;
            }
            if (line.compare(0, 3, "id:") == 0) {
                std::string val = line.substr(3);
                while (!val.empty() && val.front() == ' ') {
                    val.erase(val.begin());
                }
                char *endPtr = nullptr;
                unsigned long long parsed =
                    std::strtoull(val.c_str(), &endPtr, 10);
                if (endPtr != val.c_str()) {
                    ev.id = static_cast<std::uint64_t>(parsed);
                }
            } else if (line.compare(0, 6, "event:") == 0) {
                std::string val = line.substr(6);
                while (!val.empty() && val.front() == ' ') {
                    val.erase(val.begin());
                }
                ev.event = val;
                hasEventOrData = true;
            } else if (line.compare(0, 5, "data:") == 0) {
                std::string val = line.substr(5);
                if (!val.empty() && val.front() == ' ') {
                    val.erase(val.begin());
                }
                if (!ev.dataJson.empty()) {
                    ev.dataJson.push_back('\n');
                }
                ev.dataJson += val;
                hasEventOrData = true;
            }
        }

        if (hasEventOrData && !ev.event.empty()) {
            events.push_back(std::move(ev));
        }
    }

    return events;
}

/* ================================================================== */
/* OctiNetworkClient::Impl                                            */
/* ================================================================== */

struct OctiNetworkClient::Impl {
    struct RawResponse {
        bool transportOk = false;
        int status = 0;
        std::string body;
        std::string contentType;
        std::string transportError;
        int retryAfterSec = 0;
        Json json;
    };

    struct SseRunner {
        std::mutex mutex;
        std::condition_variable cv;
        std::shared_ptr<httplib::Client> activeClient;
#ifdef _WIN32
        HINTERNET activeRequest = nullptr;
#endif
        std::thread worker;
        std::atomic<bool> stopRequested{false};
        std::atomic<bool> active{false};
        std::atomic<std::uint64_t> lastId{0};

        ~SseRunner()
        {
            stop();
        }

        void stop()
        {
            stopRequested.store(true);
            std::shared_ptr<httplib::Client> cli;
#ifdef _WIN32
            HINTERNET req = nullptr;
#endif
            {
                std::lock_guard<std::mutex> lock(mutex);
                cli = activeClient;
#ifdef _WIN32
                req = activeRequest;
                activeRequest = nullptr;
#endif
            }
            if (cli) {
                cli->stop();
            }
#ifdef _WIN32
            if (req) {
                WinHttpCloseHandle(req);
            }
#endif
            cv.notify_all();
            if (worker.joinable()) {
                worker.join();
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                activeClient.reset();
            }
            active.store(false);
        }
    };

    std::string host;
    int port;
    mutable std::mutex tokenMutex;
    std::string sessionToken;
    PlayerProfileInfo cachedProfile;
    std::string installId;
    std::string clientType;
    bool explicitClientType = false;
    std::string robotLeaseId;

    SseRunner lobbyStream;
    SseRunner gameStream;

    Impl(const std::string &h, int p)
        : host(h), port(p), clientType("desktop"), explicitClientType(false)
    {
#ifndef _WIN32
        ::signal(SIGPIPE, SIG_IGN);
#endif
    }

    ~Impl()
    {
        lobbyStream.stop();
        gameStream.stop();
    }

    std::string currentToken() const
    {
        std::lock_guard<std::mutex> lock(tokenMutex);
        return sessionToken;
    }

    std::string currentInstallId() const
    {
        std::lock_guard<std::mutex> lock(tokenMutex);
        return installId;
    }

    void setInstallId(const std::string &id)
    {
        std::lock_guard<std::mutex> lock(tokenMutex);
        installId = id;
    }

    std::string currentClientType() const
    {
        std::lock_guard<std::mutex> lock(tokenMutex);
        return clientType.empty() ? "desktop" : clientType;
    }

    std::string currentRobotLeaseId() const
    {
        std::lock_guard<std::mutex> lock(tokenMutex);
        return robotLeaseId;
    }

    httplib::Headers authHeaders(
        const httplib::Headers &extra = httplib::Headers()) const
    {
        httplib::Headers h = extra;
        h.emplace("X-Octi-Protocol-Version",
                  std::to_string(OCTI_PROTOCOL_VERSION));
        std::string cType = currentClientType();
        h.emplace("X-Octi-Client-Type", cType);
        h.emplace("User-Agent", std::string("octi-") + cType + "/" + OCTI_APP_VERSION);
        std::string tok = currentToken();
        if (!tok.empty()) {
            h.emplace("Authorization", "Bearer " + tok);
        }
        std::string inst = currentInstallId();
        if (!inst.empty()) {
            h.emplace("X-Octi-Install-Id", inst);
        }
        std::string lease = currentRobotLeaseId();
        if (!lease.empty()) {
            h.emplace("X-Octi-Robot-Lease", lease);
        }
        return h;
    }

    std::unique_ptr<httplib::Client> makeClient(int readTimeoutSec = 10) const
    {
        if (port == 443) {
#ifdef CPPHTTPLIB_SSL_ENABLED
            try {
                std::unique_ptr<httplib::Client> cli(
                    new httplib::Client("https://" + host));
                cli->set_connection_timeout(5, 0);
                cli->set_read_timeout(readTimeoutSec, 0);
                cli->set_write_timeout(10, 0);
                return cli;
            } catch (...) {
                return nullptr;
            }
#else
            return nullptr;
#endif
        }

        try {
            std::unique_ptr<httplib::Client> cli(
                new httplib::Client(host, port));
            cli->set_connection_timeout(5, 0);
            cli->set_read_timeout(readTimeoutSec, 0);
            cli->set_write_timeout(10, 0);
            return cli;
        } catch (...) {
            return nullptr;
        }
    }

    static RawResponse wrapResult(const httplib::Result &res)
    {
        RawResponse r;
        if (!res) {
            r.transportOk = false;
            r.transportError = httplib::to_string(res.error());
            return r;
        }
        r.transportOk = true;
        r.status = res->status;
        r.body = res->body;
        r.contentType = res->get_header_value("Content-Type");
        if (res->has_header("Retry-After")) {
            r.retryAfterSec =
                std::max(0, std::atoi(res->get_header_value("Retry-After").c_str()));
        }
        if (!r.body.empty() && (r.body[0] == '{' || r.body[0] == '[')) {
            r.json = Json::parse(r.body, nullptr, false);
            if (r.json.is_discarded()) {
                r.json = Json();
            }
        }
        return r;
    }

    static httplib::Result executeHttplib(
        httplib::Client *cli,
        const std::string &verb,
        const std::string &path,
        const httplib::Headers &headers,
        const std::string &body,
        const std::string &contentType)
    {
        if (!cli) {
            return httplib::Result(nullptr, httplib::Error::Connection);
        }
        try {
            if (verb == "GET") {
                return cli->Get(path, headers);
            } else if (verb == "POST") {
                return cli->Post(path, headers, body,
                                 contentType.empty() ? "application/json" : contentType);
            } else if (verb == "PATCH") {
                return cli->Patch(path, headers, body,
                                  contentType.empty() ? "application/json" : contentType);
            } else if (verb == "DELETE") {
                return cli->Delete(path, headers);
            }
        } catch (...) {
            return httplib::Result(nullptr, httplib::Error::Connection);
        }
        return httplib::Result(nullptr, httplib::Error::Connection);
    }

#ifdef _WIN32
    httplib::Result executeWinHttp(
        const std::string &verb,
        const std::string &path,
        const httplib::Headers &headers,
        const std::string &body = "",
        const std::string &contentType = "",
        int timeoutSec = 10) const
    {
        try {
            std::string ua = std::string("octi-") + currentClientType() + "/" + OCTI_APP_VERSION;
            HINTERNET hSession = WinHttpOpen(
                toWide(ua).c_str(),
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0);
            if (!hSession) {
                return httplib::Result(nullptr, httplib::Error::Connection);
            }

            WinHttpSetTimeouts(
                hSession,
                5000,
                5000,
                timeoutSec * 1000,
                timeoutSec * 1000);

            std::wstring wideHost = toWide(host);
            HINTERNET hConnect = WinHttpConnect(
                hSession,
                wideHost.c_str(),
                static_cast<INTERNET_PORT>(port),
                0);
            if (!hConnect) {
                WinHttpCloseHandle(hSession);
                return httplib::Result(nullptr, httplib::Error::Connection);
            }

            std::wstring wideVerb = toWide(verb);
            std::wstring widePath = toWide(path);
            DWORD flags = (port == 443) ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(
                hConnect,
                wideVerb.c_str(),
                widePath.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                flags);
            if (!hRequest) {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return httplib::Result(nullptr, httplib::Error::Connection);
            }

            std::string headerStr;
            for (const auto &kv : headers) {
                headerStr += kv.first + ": " + kv.second + "\r\n";
            }
            if (!contentType.empty()) {
                headerStr += "Content-Type: " + contentType + "\r\n";
            }
            headerStr += "User-Agent: " + ua + "\r\n";

            std::wstring wideHeaders = toWide(headerStr);
            if (!wideHeaders.empty()) {
                WinHttpAddRequestHeaders(
                    hRequest,
                    wideHeaders.c_str(),
                    static_cast<DWORD>(wideHeaders.length()),
                    WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            }

            DWORD bodyLen = static_cast<DWORD>(body.size());
            LPVOID bodyPtr = bodyLen > 0
                ? const_cast<char *>(body.data())
                : WINHTTP_NO_REQUEST_DATA;

            BOOL sent = WinHttpSendRequest(
                hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS,
                0,
                bodyPtr,
                bodyLen,
                bodyLen,
                0);
            if (!sent) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return httplib::Result(nullptr, httplib::Error::Connection);
            }

            BOOL received = WinHttpReceiveResponse(hRequest, nullptr);
            if (!received) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return httplib::Result(nullptr, httplib::Error::Connection);
            }

            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            if (!WinHttpQueryHeaders(
                    hRequest,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    &statusCode,
                    &statusSize,
                    WINHTTP_NO_HEADER_INDEX)) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return httplib::Result(nullptr, httplib::Error::Connection);
            }

            std::unique_ptr<httplib::Response> res(new httplib::Response());
            res->status = static_cast<int>(statusCode);

            DWORD headersSize = 0;
            WinHttpQueryHeaders(
                hRequest,
                WINHTTP_QUERY_RAW_HEADERS_CRLF,
                WINHTTP_HEADER_NAME_BY_INDEX,
                nullptr,
                &headersSize,
                WINHTTP_NO_HEADER_INDEX);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && headersSize > 0) {
                std::vector<wchar_t> rawHdr(headersSize / sizeof(wchar_t) + 1, 0);
                if (WinHttpQueryHeaders(
                        hRequest,
                        WINHTTP_QUERY_RAW_HEADERS_CRLF,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        rawHdr.data(),
                        &headersSize,
                        WINHTTP_NO_HEADER_INDEX)) {
                    std::wstring wHeaders(rawHdr.data());
                    std::wistringstream wiss(wHeaders);
                    std::wstring wLine;
                    while (std::getline(wiss, wLine)) {
                        if (!wLine.empty() && wLine.back() == L'\r') {
                            wLine.pop_back();
                        }
                        size_t colon = wLine.find(L':');
                        if (colon != std::wstring::npos) {
                            std::wstring wKey = wLine.substr(0, colon);
                            std::wstring wVal = wLine.substr(colon + 1);
                            while (!wVal.empty() && wVal.front() == L' ') {
                                wVal.erase(wVal.begin());
                            }
                            int kLen = WideCharToMultiByte(
                                CP_UTF8, 0, wKey.c_str(), -1, nullptr, 0, nullptr, nullptr);
                            int vLen = WideCharToMultiByte(
                                CP_UTF8, 0, wVal.c_str(), -1, nullptr, 0, nullptr, nullptr);
                            if (kLen > 0 && vLen > 0) {
                                std::vector<char> kBuf(kLen);
                                std::vector<char> vBuf(vLen);
                                WideCharToMultiByte(
                                    CP_UTF8, 0, wKey.c_str(), -1, kBuf.data(), kLen, nullptr, nullptr);
                                WideCharToMultiByte(
                                    CP_UTF8, 0, wVal.c_str(), -1, vBuf.data(), vLen, nullptr, nullptr);
                                res->headers.emplace(kBuf.data(), vBuf.data());
                            }
                        }
                    }
                }
            }

            for (;;) {
                DWORD bytesAvail = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &bytesAvail) || bytesAvail == 0) {
                    break;
                }
                std::vector<char> chunk(bytesAvail);
                DWORD bytesRead = 0;
                if (!WinHttpReadData(hRequest, chunk.data(), bytesAvail, &bytesRead) || bytesRead == 0) {
                    break;
                }
                res->body.append(chunk.data(), bytesRead);
            }

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);

            return httplib::Result(std::move(res), httplib::Error::Success);
        } catch (...) {
            return httplib::Result(nullptr, httplib::Error::Connection);
        }
    }
#endif

    RawResponse executeWithRetry(
        const std::string &verb,
        const std::string &path,
        const httplib::Headers &headers,
        const std::string &body = "",
        const std::string &contentType = "") const
    {
        try {
            httplib::Result res;

#ifdef _WIN32
            if (port == 443) {
                for (int attempt = 1; attempt <= 5; ++attempt) {
                    res = executeWinHttp(verb, path, headers, body, contentType);
                    if (res) {
                        return wrapResult(res);
                    }
                    if (attempt < 5) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(100 * attempt));
                    }
                }
                if (host == OCTI_SERVER_HOST && port == OCTI_SERVER_PORT) {
                    try {
                        std::unique_ptr<httplib::Client> fbCli(
                            new httplib::Client(
                                OCTI_SERVER_FALLBACK_HOST,
                                OCTI_SERVER_FALLBACK_PORT));
                        fbCli->set_connection_timeout(5, 0);
                        fbCli->set_read_timeout(10, 0);
                        fbCli->set_write_timeout(10, 0);
                        httplib::Result fbRes = executeHttplib(
                            fbCli.get(), verb, path, headers, body, contentType);
                        if (fbRes) {
                            return wrapResult(fbRes);
                        }
                    } catch (...) {
                    }
                }
                return wrapResult(res);
            }
#endif

            for (int attempt = 1; attempt <= 5; ++attempt) {
                std::unique_ptr<httplib::Client> cli = makeClient();
                if (cli) {
                    res = executeHttplib(
                        cli.get(), verb, path, headers, body, contentType);
                    if (res) {
                        return wrapResult(res);
                    }
                } else {
                    res = httplib::Result(nullptr, httplib::Error::Connection);
                    break;
                }
                if (attempt < 5) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(100 * attempt));
                }
            }

            if (host == OCTI_SERVER_HOST && port == OCTI_SERVER_PORT) {
                try {
                    std::unique_ptr<httplib::Client> fbCli(
                        new httplib::Client(
                            OCTI_SERVER_FALLBACK_HOST,
                            OCTI_SERVER_FALLBACK_PORT));
                    fbCli->set_connection_timeout(5, 0);
                    fbCli->set_read_timeout(10, 0);
                    fbCli->set_write_timeout(10, 0);
                    httplib::Result fbRes = executeHttplib(
                        fbCli.get(), verb, path, headers, body, contentType);
                    if (fbRes) {
                        return wrapResult(fbRes);
                    }
                } catch (...) {
                }
            }
            return wrapResult(res);
        } catch (const std::exception &e) {
            RawResponse err;
            err.transportOk = false;
            err.transportError = e.what();
            return err;
        } catch (...) {
            RawResponse err;
            err.transportOk = false;
            err.transportError = "Unknown exception in network execution";
            return err;
        }
    }

    RawResponse doGet(const std::string &path,
                      const httplib::Headers &extra = httplib::Headers()) const
    {
        return executeWithRetry("GET", path, authHeaders(extra));
    }

    RawResponse doPost(const std::string &path, const Json &body) const
    {
        return executeWithRetry("POST", path, authHeaders(), body.dump(), "application/json");
    }

    RawResponse doPatch(const std::string &path, const Json &body) const
    {
        return executeWithRetry("PATCH", path, authHeaders(), body.dump(), "application/json");
    }

    RawResponse doDelete(const std::string &path) const
    {
        return executeWithRetry("DELETE", path, authHeaders());
    }

    template <typename T>
    static void populateError(const RawResponse &raw, ApiResult<T> &out)
    {
        out.ok = false;
        out.httpStatus = raw.status;
        out.retryAfterSec = raw.retryAfterSec;
        if (!raw.transportOk) {
            out.errorCode = "TRANSPORT_ERROR";
            out.errorMessage = raw.transportError;
            return;
        }
        if (raw.json.is_object()) {
            out.errorCode = raw.json.value("error", "HTTP_ERROR");
            std::string msg = raw.json.value("message", "");
            if (msg.empty()) {
                msg = raw.json.value("error", "");
            }
            if (msg.empty()) {
                msg = raw.body;
            }
            out.errorMessage = msg;
        } else {
            out.errorCode = "HTTP_" + std::to_string(raw.status);
            out.errorMessage = raw.body;
        }
    }

#ifdef _WIN32
    bool streamWinHttpSse(
        SseRunner &runner,
        const std::string &path,
        std::uint64_t curSeq,
        const std::function<void(const SseEvent &)> &onEvent) const
    {
        try {
            HINTERNET hSession = WinHttpOpen(
                L"octi-desktop/2.1",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0);
            if (!hSession) {
                return false;
            }

            WinHttpSetTimeouts(hSession, 5000, 5000, 30000, 30000);

            std::wstring wideHost = toWide(host);
            HINTERNET hConnect = WinHttpConnect(
                hSession, wideHost.c_str(), static_cast<INTERNET_PORT>(port), 0);
            if (!hConnect) {
                WinHttpCloseHandle(hSession);
                return false;
            }

            std::wstring widePath = toWide(path);
            HINTERNET hRequest = WinHttpOpenRequest(
                hConnect,
                L"GET",
                widePath.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE);
            if (!hRequest) {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            {
                std::lock_guard<std::mutex> lock(runner.mutex);
                if (runner.stopRequested.load()) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return false;
                }
                runner.activeRequest = hRequest;
            }

            std::string headerStr;
            httplib::Headers hdrs = authHeaders();
            for (const auto &kv : hdrs) {
                headerStr += kv.first + ": " + kv.second + "\r\n";
            }
            headerStr += "Accept: text/event-stream\r\n";
            if (curSeq > 0) {
                headerStr += "Last-Event-ID: " + std::to_string(curSeq) + "\r\n";
            }
            std::wstring wideHeaders = toWide(headerStr);
            WinHttpAddRequestHeaders(
                hRequest,
                wideHeaders.c_str(),
                static_cast<DWORD>(wideHeaders.length()),
                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

            BOOL sent = WinHttpSendRequest(
                hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS,
                0,
                WINHTTP_NO_REQUEST_DATA,
                0,
                0,
                0);
            if (!sent) {
                {
                    std::lock_guard<std::mutex> lock(runner.mutex);
                    runner.activeRequest = nullptr;
                }
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            BOOL received = WinHttpReceiveResponse(hRequest, nullptr);
            if (!received) {
                {
                    std::lock_guard<std::mutex> lock(runner.mutex);
                    runner.activeRequest = nullptr;
                }
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            WinHttpQueryHeaders(
                hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusSize,
                WINHTTP_NO_HEADER_INDEX);

            if (statusCode != 200) {
                if (statusCode >= 400 && statusCode < 500 && statusCode != 429) {
                    runner.stopRequested.store(true);
                }
                {
                    std::lock_guard<std::mutex> lock(runner.mutex);
                    runner.activeRequest = nullptr;
                }
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return (statusCode < 500);
            }

            std::string sseBuffer;
            bool terminalEventSeen = false;
            while (!runner.stopRequested.load()) {
                DWORD bytesAvail = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &bytesAvail) || bytesAvail == 0) {
                    break;
                }
                std::vector<char> buf(bytesAvail);
                DWORD bytesRead = 0;
                if (!WinHttpReadData(hRequest, buf.data(), bytesAvail, &bytesRead) || bytesRead == 0) {
                    break;
                }
                sseBuffer.append(buf.data(), bytesRead);
                std::vector<SseEvent> parsed =
                    OctiNetworkClient::parseSseBuffer(sseBuffer);
                for (std::size_t i = 0; i < parsed.size(); ++i) {
                    if (parsed[i].id > runner.lastId.load()) {
                        runner.lastId.store(parsed[i].id);
                    }
                    if (parsed[i].event == "closed") {
                        terminalEventSeen = true;
                    }
                    if (onEvent) {
                        onEvent(parsed[i]);
                    }
                }
                if (terminalEventSeen) {
                    break;
                }
            }

            {
                std::lock_guard<std::mutex> lock(runner.mutex);
                runner.activeRequest = nullptr;
            }
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return true;
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(runner.mutex);
                runner.activeRequest = nullptr;
            }
            return false;
        }
    }
#endif

    bool streamHttplibSse(
        SseRunner &runner,
        const std::string &targetHost,
        int targetPort,
        const std::string &path,
        std::uint64_t curSeq,
        const std::function<void(const SseEvent &)> &onEvent) const
    {
        try {
            std::shared_ptr<httplib::Client> cli;
            if (targetPort == 443) {
#ifdef CPPHTTPLIB_SSL_ENABLED
                try {
                    cli.reset(new httplib::Client("https://" + targetHost));
                } catch (...) {
                    return false;
                }
#else
                return false;
#endif
            } else {
                try {
                    cli.reset(new httplib::Client(targetHost, targetPort));
                } catch (...) {
                    return false;
                }
            }
            if (!cli) {
                return false;
            }
            cli->set_connection_timeout(5, 0);
            cli->set_read_timeout(30, 0);
            cli->set_write_timeout(5, 0);

            {
                std::lock_guard<std::mutex> lock(runner.mutex);
                if (runner.stopRequested.load()) {
                    return false;
                }
                runner.activeClient = cli;
            }

            httplib::Headers hdrs = authHeaders();
            hdrs.emplace("Accept", "text/event-stream");
            if (curSeq > 0) {
                hdrs.emplace("Last-Event-ID", std::to_string(curSeq));
            }

            int httpStatus = 0;
            bool terminalEventSeen = false;
            std::string chunkBuffer;

            httplib::Result res = cli->Get(
                path, hdrs,
                [&runner, &httpStatus](const httplib::Response &resp) {
                    httpStatus = resp.status;
                    return !runner.stopRequested.load() &&
                           resp.status == 200;
                },
                [&runner, &chunkBuffer, &terminalEventSeen, &onEvent](
                    const char *data, std::size_t len) {
                    if (runner.stopRequested.load()) {
                        return false;
                    }
                    chunkBuffer.append(data, len);
                    std::vector<SseEvent> parsed =
                        OctiNetworkClient::parseSseBuffer(chunkBuffer);
                    for (std::size_t i = 0; i < parsed.size(); ++i) {
                        if (parsed[i].id > runner.lastId.load()) {
                            runner.lastId.store(parsed[i].id);
                        }
                        if (parsed[i].event == "closed") {
                            terminalEventSeen = true;
                        }
                        if (onEvent) {
                            onEvent(parsed[i]);
                        }
                    }
                    return !runner.stopRequested.load();
                });

            {
                std::lock_guard<std::mutex> lock(runner.mutex);
                runner.activeClient.reset();
            }

            if (terminalEventSeen || (httpStatus >= 400 && httpStatus < 500 && httpStatus != 429)) {
                runner.stopRequested.store(true);
            }
            return (httpStatus == 200);
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(runner.mutex);
                runner.activeClient.reset();
            }
            return false;
        }
    }

    void launchSseStream(
        SseRunner &runner,
        const std::string &basePath,
        std::uint64_t sinceSeq,
        std::function<void(const SseEvent &)> onEvent)
    {
        runner.stop();
        runner.stopRequested.store(false);
        runner.lastId.store(sinceSeq);
        runner.active.store(true);

        runner.worker = std::thread(
            [this, &runner, basePath, onEvent]() {
                try {
                    while (!runner.stopRequested.load()) {
                        std::uint64_t curSeq = runner.lastId.load();
                        std::string path = basePath +
                                           (basePath.find('?') == std::string::npos
                                                ? "?stream=1&since_seq="
                                                : "&stream=1&since_seq=") +
                                           std::to_string(curSeq);
                        std::string lease = currentRobotLeaseId();
                        if (!lease.empty()) {
                            path += "&lease_id=" + urlEncode(lease);
                        }

#ifdef _WIN32
                        if (port == 443) {
                            bool winHttpOk = streamWinHttpSse(
                                runner, path, curSeq, onEvent);
                            if (runner.stopRequested.load()) {
                                break;
                            }
                            if (!winHttpOk && host == OCTI_SERVER_HOST && port == OCTI_SERVER_PORT) {
                                streamHttplibSse(
                                    runner, OCTI_SERVER_FALLBACK_HOST,
                                    OCTI_SERVER_FALLBACK_PORT, path, curSeq, onEvent);
                            }
                        } else {
                            streamHttplibSse(
                                runner, host, port, path, curSeq, onEvent);
                        }
#else
                        bool clientConstructed = false;
                        if (port == 443) {
#ifdef CPPHTTPLIB_SSL_ENABLED
                            clientConstructed = true;
                            streamHttplibSse(runner, host, port, path, curSeq, onEvent);
#else
                            clientConstructed = false;
#endif
                        } else {
                            clientConstructed = true;
                            streamHttplibSse(runner, host, port, path, curSeq, onEvent);
                        }
                        if (!clientConstructed && host == OCTI_SERVER_HOST && port == OCTI_SERVER_PORT) {
                            streamHttplibSse(
                                runner, OCTI_SERVER_FALLBACK_HOST,
                                OCTI_SERVER_FALLBACK_PORT, path, curSeq, onEvent);
                        }
#endif

                        if (runner.stopRequested.load()) {
                            break;
                        }

                        std::unique_lock<std::mutex> lock(runner.mutex);
                        runner.cv.wait_for(
                            lock, std::chrono::milliseconds(150),
                            [&runner]() { return runner.stopRequested.load(); });
                    }
                } catch (const std::exception &) {
                } catch (...) {
                }
                runner.active.store(false);
            });
    }
};

/* ================================================================== */
/* OctiNetworkClient                                                  */
/* ================================================================== */

OctiNetworkClient::OctiNetworkClient()
    : _impl(new Impl(OCTI_SERVER_HOST, OCTI_SERVER_PORT)),
      _host(OCTI_SERVER_HOST),
      _port(OCTI_SERVER_PORT)
{
}

OctiNetworkClient::OctiNetworkClient(const std::string &host, int port)
    : _impl(new Impl(host, port)),
      _host(host),
      _port(port)
{
}

OctiNetworkClient::~OctiNetworkClient()
{
    stopEventStreams();
}

std::unique_ptr<OctiNetworkClient> OctiNetworkClient::createForLoopbackTest(
    int loopbackPort)
{
    return std::unique_ptr<OctiNetworkClient>(
        new OctiNetworkClient("127.0.0.1", loopbackPort));
}

const std::string &OctiNetworkClient::host() const
{
    return _host;
}

int OctiNetworkClient::port() const
{
    return _port;
}

void OctiNetworkClient::setEndpoint(const std::string &host, int port)
{
    _host = host;
    _port = port;
    _impl->host = host;
    _impl->port = port;
}

std::string OctiNetworkClient::buildUrl(const std::string &path) const
{
    std::string prefix;
    if (_port == 443 || (_host == "octi.selfso.com" && _port == 443)) {
        prefix = "https://" + _host;
    } else {
        prefix = "http://" + _host + ":" + std::to_string(_port);
    }
    if (path.empty()) {
        return prefix;
    }
    if (path.front() != '/') {
        return prefix + "/" + path;
    }
    return prefix + path;
}

void OctiNetworkClient::setSessionToken(const std::string &token)
{
    std::lock_guard<std::mutex> lock(_impl->tokenMutex);
    _impl->sessionToken = token;
    if (!_impl->explicitClientType) {
        if (token.rfind("octi_bot_", 0) == 0) {
            _impl->clientType = "robot";
        } else {
            _impl->clientType = "desktop";
        }
    }
}

std::string OctiNetworkClient::sessionToken() const
{
    return _impl->currentToken();
}

void OctiNetworkClient::clearSessionToken()
{
    std::lock_guard<std::mutex> lock(_impl->tokenMutex);
    _impl->sessionToken.clear();
    _impl->cachedProfile = PlayerProfileInfo();
    if (!_impl->explicitClientType) {
        _impl->clientType = "desktop";
    }
}

PlayerProfileInfo OctiNetworkClient::currentProfile() const
{
    std::lock_guard<std::mutex> lock(_impl->tokenMutex);
    return _impl->cachedProfile;
}

void OctiNetworkClient::setCurrentProfile(const PlayerProfileInfo &profile)
{
    std::lock_guard<std::mutex> lock(_impl->tokenMutex);
    _impl->cachedProfile = profile;
}

void OctiNetworkClient::setBotToken(const std::string &token)
{
    std::lock_guard<std::mutex> lock(_impl->tokenMutex);
    _impl->sessionToken = token;
    if (!_impl->explicitClientType) {
        _impl->clientType = "robot";
    }
}

bool OctiNetworkClient::isRobotToken() const
{
    std::string tok = sessionToken();
    return tok.rfind("octi_bot_", 0) == 0;
}

void OctiNetworkClient::setClientType(const std::string &clientType)
{
    std::lock_guard<std::mutex> lock(_impl->tokenMutex);
    _impl->clientType = clientType;
    _impl->explicitClientType = true;
}

std::string OctiNetworkClient::clientType() const
{
    return _impl->currentClientType();
}

void OctiNetworkClient::setRobotLeaseId(const std::string &leaseId)
{
    std::lock_guard<std::mutex> lock(_impl->tokenMutex);
    _impl->robotLeaseId = leaseId;
}

std::string OctiNetworkClient::robotLeaseId() const
{
    return _impl->currentRobotLeaseId();
}

void OctiNetworkClient::clearRobotLeaseId()
{
    std::lock_guard<std::mutex> lock(_impl->tokenMutex);
    _impl->robotLeaseId.clear();
}

void OctiNetworkClient::setInstallId(const std::string &installId)
{
    _impl->setInstallId(installId);
}

std::string OctiNetworkClient::installId() const
{
    return _impl->currentInstallId();
}

/* ---- 1..10: Health, Auth, Profile & Leaderboard ---- */

ApiResult<HealthInfo> OctiNetworkClient::fetchHealth()
{
    ApiResult<HealthInfo> res;
    Impl::RawResponse raw = _impl->doGet("/api/v1/health");
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data.status = raw.json.value("status", "");
    res.data.version = raw.json.value("version", "");
    res.data.uptimeSec =
        raw.json.value("uptime_sec", static_cast<std::int64_t>(0));
    res.data.activeTables =
        raw.json.value("active_tables", static_cast<std::int64_t>(0));
    res.data.onlinePlayers =
        raw.json.value("online_players", static_cast<std::int64_t>(0));
    res.data.protocolVersion =
        raw.json.value("protocol_version", OCTI_PROTOCOL_VERSION);
    res.data.minProtocolVersion =
        raw.json.value("min_protocol_version", OCTI_MIN_PROTOCOL_VERSION);
    return res;
}

ApiResult<std::vector<AuthProviderInfo>> OctiNetworkClient::fetchAuthProviders()
{
    ApiResult<std::vector<AuthProviderInfo>> res;
    Impl::RawResponse raw = _impl->doGet("/api/v1/auth/providers");
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    if (raw.json.contains("providers") && raw.json["providers"].is_array()) {
        const Json &arr = raw.json["providers"];
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].is_object()) {
                continue;
            }
            AuthProviderInfo info;
            info.name = arr[i].value("name", "");
            info.displayName = arr[i].value("display_name", "");
            if (arr[i].contains("client_ids") &&
                arr[i]["client_ids"].is_array()) {
                for (std::size_t k = 0; k < arr[i]["client_ids"].size(); ++k) {
                    if (arr[i]["client_ids"][k].is_string()) {
                        info.clientIds.push_back(
                            arr[i]["client_ids"][k].get<std::string>());
                    }
                }
            }
            res.data.push_back(info);
        }
    }
    return res;
}

ApiResult<SsoLoginData> OctiNetworkClient::loginWithSso(
    const std::string &provider,
    const std::string &token,
    const std::string &desiredHandle,
    bool isAccessToken)
{
    ApiResult<SsoLoginData> res;
    Json body;
    body["provider"] = provider;
    body["client_agent"] = "octi-desktop";
    if (isAccessToken) {
        body["access_token"] = token;
    } else {
        body["id_token"] = token;
    }

    Impl::RawResponse raw = _impl->doPost("/api/v1/auth/sso", body);
    if (!raw.transportOk || (raw.status != 200 && raw.status != 201) ||
        !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }

    res.ok = true;
    res.httpStatus = raw.status;
    res.data.sessionToken = raw.json.value("session_token", "");
    res.data.expiresAt =
        raw.json.value("expires_at", static_cast<std::int64_t>(0));
    res.data.isNewAccount =
        (raw.status == 201) || raw.json.value("created", false);
    if (raw.json.contains("profile")) {
        res.data.profile = parsePlayerProfile(raw.json["profile"]);
    }
    if (raw.json.contains("linked_identities")) {
        res.data.identities =
            parseLinkedIdentities(raw.json["linked_identities"]);
        res.data.profile.identities = res.data.identities;
    }

    if (!res.data.sessionToken.empty()) {
        setSessionToken(res.data.sessionToken);
    }

    if (!desiredHandle.empty() && !res.data.sessionToken.empty()) {
        ApiResult<PlayerProfileInfo> patched = updateMyProfile(desiredHandle);
        if (patched.ok) {
            res.data.profile = patched.data;
            res.data.profile.identities = res.data.identities;
        }
    }

    setCurrentProfile(res.data.profile);

    return res;
}

ApiResult<LinkIdentityData> OctiNetworkClient::linkIdentity(
    const std::string &provider,
    const std::string &idToken,
    const std::string &accessToken,
    bool forceRelink)
{
    ApiResult<LinkIdentityData> res;
    Json body;
    body["provider"] = provider;
    if (!accessToken.empty()) {
        body["access_token"] = accessToken;
    }
    if (!idToken.empty() || accessToken.empty()) {
        body["id_token"] = idToken;
    }
    if (forceRelink) {
        body["force_relink"] = true;
    }

    Impl::RawResponse raw = _impl->doPost("/api/v1/auth/link", body);
    if (!raw.transportOk) {
        Impl::populateError(raw, res);
        return res;
    }

    if (raw.status == 409 && raw.json.is_object()) {
        Impl::populateError(raw, res);
        res.data.conflictAlreadyLinked = true;
        if (raw.json.contains("existing_profile") &&
            raw.json["existing_profile"].is_object()) {
            res.data.existingProfile =
                parsePlayerProfile(raw.json["existing_profile"]);
        }
        return res;
    }

    if (raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }

    res.ok = true;
    res.httpStatus = raw.status;
    res.data.status = raw.json.value("status", "linked");
    if (raw.json.contains("linked_identities")) {
        res.data.identities =
            parseLinkedIdentities(raw.json["linked_identities"]);
    }
    return res;
}

ApiResult<LinkIdentityData> OctiNetworkClient::linkIdentity(
    const std::string &provider,
    const std::string &token,
    bool isAccessToken,
    bool forceRelink)
{
    return linkIdentity(provider,
                        isAccessToken ? "" : token,
                        isAccessToken ? token : "",
                        forceRelink);
}

ApiResult<UnlinkIdentityData> OctiNetworkClient::unlinkIdentity(
    const std::string &provider)
{
    ApiResult<UnlinkIdentityData> res;
    Impl::RawResponse raw =
        _impl->doDelete("/api/v1/auth/link/" + urlEncode(provider));
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data.status = raw.json.value("status", "unlinked");
    if (raw.json.contains("linked_identities")) {
        res.data.identities =
            parseLinkedIdentities(raw.json["linked_identities"]);
    }
    return res;
}

ApiResult<bool> OctiNetworkClient::logout()
{
    ApiResult<bool> res;
    Impl::RawResponse raw = _impl->doPost("/api/v1/auth/logout", Json::object());
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    clearSessionToken();
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<PlayerProfileInfo> OctiNetworkClient::fetchMyProfile()
{
    ApiResult<PlayerProfileInfo> res;
    Impl::RawResponse raw = _impl->doGet("/api/v1/profile/me");
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = parsePlayerProfile(raw.json);
    setCurrentProfile(res.data);
    return res;
}

ApiResult<PlayerProfileInfo> OctiNetworkClient::updateMyProfile(
    const std::string &handle,
    const std::string &avatarUrl,
    const std::string &displayName,
    const std::string &bio,
    const std::string &location,
    bool clearAvatar)
{
    ApiResult<PlayerProfileInfo> res;
    Json body = Json::object();
    if (!handle.empty()) {
        body["handle"] = handle;
    }
    if (!displayName.empty()) {
        body["display_name"] = displayName;
    }
    body["bio"] = bio;
    body["location"] = location;
    if (clearAvatar) {
        body["avatar_url"] = "";
    } else if (!avatarUrl.empty()) {
        body["avatar_url"] = avatarUrl;
    }
    Impl::RawResponse raw = _impl->doPatch("/api/v1/profile/me", body);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = parsePlayerProfile(raw.json);
    setCurrentProfile(res.data);
    return res;
}

ApiResult<PlayerProfileInfo> OctiNetworkClient::updateMyProfile(
    const std::string &handle,
    const std::string &avatarUrl,
    const std::string &displayName)
{
    ApiResult<PlayerProfileInfo> res;
    Json body = Json::object();
    if (!handle.empty()) {
        body["handle"] = handle;
    }
    if (!displayName.empty()) {
        body["display_name"] = displayName;
    }
    if (!avatarUrl.empty()) {
        body["avatar_url"] = avatarUrl;
    }
    Impl::RawResponse raw = _impl->doPatch("/api/v1/profile/me", body);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = parsePlayerProfile(raw.json);
    return res;
}

bool OctiNetworkClient::fetchAvatarBytes(const std::string &avatarUrl,
                                        std::string &outBytes,
                                        std::string &outContentType)
{
    outBytes.clear();
    outContentType.clear();
    if (avatarUrl.empty() || avatarUrl.rfind("data:", 0) == 0) {
        return false;
    }
    std::string path = avatarUrl;
    if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) {
        auto slash = path.find('/', 8);
        if (slash != std::string::npos) {
            path = path.substr(slash);
        } else {
            path = "/";
        }
    }
    if (path.empty() || path[0] != '/') {
        path = "/" + path;
    }
    Impl::RawResponse raw = _impl->doGet(path);
    if (!raw.transportOk || raw.status != 200) {
        return false;
    }
    outBytes = std::move(raw.body);
    outContentType = std::move(raw.contentType);
    return true;
}

ApiResult<PlayerProfileInfo> OctiNetworkClient::fetchPlayerProfile(
    const std::string &playerUuid)
{
    ApiResult<PlayerProfileInfo> res;
    Impl::RawResponse raw =
        _impl->doGet("/api/v1/players/" + urlEncode(playerUuid));
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = parsePlayerProfile(raw.json);
    return res;
}

ApiResult<std::vector<LeaderboardRow>> OctiNetworkClient::fetchLeaderboard(
    const std::string &mode,
    int limit)
{
    ApiResult<std::vector<LeaderboardRow>> res;
    std::string path = "/api/v1/leaderboard?mode=" + urlEncode(mode) +
                       "&limit=" + std::to_string(limit);
    Impl::RawResponse raw = _impl->doGet(path);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    if (raw.json.contains("entries") && raw.json["entries"].is_array()) {
        const Json &arr = raw.json["entries"];
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].is_object()) {
                continue;
            }
            LeaderboardRow row;
            row.rank = arr[i].value("rank", static_cast<int>(i) + 1);
            row.uuid = arr[i].value("uuid", "");
            row.handle = arr[i].value("handle", "");
            row.displayName = arr[i].value("display_name", "");
            row.elo = arr[i].value("elo", 1200);
            row.gamesPlayed = arr[i].value("games_played", 0);
            row.wins = arr[i].value("wins", 0);
            row.losses = arr[i].value("losses", 0);
            row.draws = arr[i].value("draws", 0);
            res.data.push_back(row);
        }
    }
    return res;
}

/* ---- 11..22: Lobby, Tables & Matchmaking ---- */

ApiResult<LobbyStateData> OctiNetworkClient::fetchLobbyState()
{
    ApiResult<LobbyStateData> res;
    Impl::RawResponse raw = _impl->doGet("/api/v1/lobby/state");
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    if (raw.json.contains("players") && raw.json["players"].is_array()) {
        const Json &pl = raw.json["players"];
        for (std::size_t i = 0; i < pl.size(); ++i) {
            if (!pl[i].is_object()) {
                continue;
            }
            LobbyPlayerInfo p;
            p.playerUuid = pl[i].value("player_uuid", "");
            p.handle = pl[i].value("handle", "");
            p.avatarUrl = pl[i].value("avatar_url", "");
            p.status = pl[i].value("status", "IN_LOBBY");
            p.roomId = pl[i].value("room_id", "");
            p.onlineSec = pl[i].value("online_sec", static_cast<std::int64_t>(0));
            p.idleSec = pl[i].value("idle_sec", static_cast<std::int64_t>(0));
            p.isRobot = pl[i].value("is_robot", false);
            p.robotUuid = pl[i].value("robot_uuid", "");
            p.description = pl[i].value(
                "description", pl[i].value("robot_description", ""));
            p.ownerUuid = pl[i].value("owner_uuid", "");
            p.ownerHandle = pl[i].value("owner_handle", "");
            int gp = pl[i].value("games_played", 0);
            int w = pl[i].value("wins", 0);
            int l = pl[i].value("losses", 0);
            int d = pl[i].value("draws", 0);
            p.robotGamesPlayed =
                pl[i].value("robot_games_played", p.isRobot ? gp : 0);
            p.robotWins = pl[i].value("robot_wins", p.isRobot ? w : 0);
            p.robotLosses = pl[i].value("robot_losses", p.isRobot ? l : 0);
            p.robotDraws = pl[i].value("robot_draws", p.isRobot ? d : 0);
            if (pl[i].contains("clients") && pl[i]["clients"].is_array()) {
                for (std::size_t ci = 0; ci < pl[i]["clients"].size(); ++ci) {
                    if (pl[i]["clients"][ci].is_string()) {
                        p.clients.push_back(pl[i]["clients"][ci].get<std::string>());
                    }
                }
            }
            res.data.players.push_back(p);
        }
    }
    if (raw.json.contains("tables") && raw.json["tables"].is_array()) {
        const Json &tb = raw.json["tables"];
        for (std::size_t i = 0; i < tb.size(); ++i) {
            res.data.tables.push_back(parseTableSummary(tb[i]));
        }
    }
    if (raw.json.contains("recent_chat") && raw.json["recent_chat"].is_array()) {
        const Json &ch = raw.json["recent_chat"];
        for (std::size_t i = 0; i < ch.size(); ++i) {
            if (!ch[i].is_object()) {
                continue;
            }
            LobbyChatEntry c;
            c.seq = ch[i].value("seq", static_cast<std::int64_t>(0));
            std::int64_t createdMs =
                ch[i].value("created_at_ms", static_cast<std::int64_t>(0));
            std::int64_t rawAtMs =
                ch[i].value("at_ms", static_cast<std::int64_t>(0));
            c.createdAtMs = createdMs > 0 ? createdMs : rawAtMs;
            c.atMs = c.createdAtMs;
            c.playerUuid = ch[i].value("player_uuid", "");
            c.handle = ch[i].value("handle", "");
            c.text = ch[i].value("text", "");
            c.isRobot = ch[i].value("is_robot", false);
            c.robotUuid = ch[i].value("robot_uuid", "");
            c.ownerUuid = ch[i].value("owner_uuid", "");
            c.ownerHandle = ch[i].value("owner_handle", "");
            res.data.recentChat.push_back(c);
        }
    }
    res.data.latestSeq =
        raw.json.value("latest_seq", static_cast<std::int64_t>(0));
    return res;
}

ApiResult<std::int64_t> OctiNetworkClient::postLobbyChat(
    const std::string &text)
{
    ApiResult<std::int64_t> res;
    Json body;
    body["text"] = text;
    Impl::RawResponse raw = _impl->doPost("/api/v1/lobby/chat", body);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = raw.json.value("seq", static_cast<std::int64_t>(0));
    return res;
}

ApiResult<std::vector<TableSummaryInfo>> OctiNetworkClient::listTables(
    bool includeFinished)
{
    ApiResult<std::vector<TableSummaryInfo>> res;
    std::string path =
        includeFinished ? "/api/v1/tables?include_finished=1" : "/api/v1/tables";
    Impl::RawResponse raw = _impl->doGet(path);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    if (raw.json.contains("tables") && raw.json["tables"].is_array()) {
        const Json &arr = raw.json["tables"];
        for (std::size_t i = 0; i < arr.size(); ++i) {
            res.data.push_back(parseTableSummary(arr[i]));
        }
    }
    return res;
}

ApiResult<CreateTableResponse> OctiNetworkClient::createTable(
    const CreateTableRequest &req)
{
    ApiResult<CreateTableResponse> res;
    Json body;
    body["mode"] = req.mode;
    body["bases"] = req.bases;
    body["super_prongs"] = req.superProngs;
    body["edgeless"] = req.edgeless;
    body["allow_robots"] = req.allowRobots;
    body["rated"] = req.rated;
    body["allow_takeback"] = req.allowTakeback;
    body["observer_policy"] = req.observerPolicy;
    body["voice_policy"] = req.voicePolicy;
    if (!req.passcode.empty()) {
        body["passcode"] = req.passcode;
    }
    if (req.timeControlEnabled && req.initialMs > 0) {
        Json tc;
        tc["initial_ms"] = req.initialMs;
        tc["increment_ms"] = req.incrementMs;
        tc["disconnect_grace_ms"] = req.disconnectGraceMs;
        body["time_control"] = tc;
    } else {
        body["time_control"] = "untimed";
    }

    Impl::RawResponse raw = _impl->doPost("/api/v1/tables", body);
    if (!raw.transportOk || (raw.status != 201 && raw.status != 200) ||
        !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data.roomId = raw.json.value("room_id", "");
    res.data.seat = raw.json.value("seat", 1);
    return res;
}

ApiResult<JoinTableResponse> OctiNetworkClient::joinTable(
    const std::string &roomId,
    int seat,
    const std::string &passcode)
{
    ApiResult<JoinTableResponse> res;
    Json body;
    body["seat"] = seat;
    if (!passcode.empty()) {
        body["passcode"] = passcode;
    }
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/tables/" + urlEncode(roomId) + "/join", body);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data.roomId = raw.json.value("room_id", roomId);
    res.data.seat = raw.json.value("seat", seat);
    res.data.gameStarted = raw.json.value("game_started", false);
    return res;
}

ApiResult<bool> OctiNetworkClient::leaveTable(const std::string &roomId)
{
    ApiResult<bool> res;
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/tables/" + urlEncode(roomId) + "/leave", Json::object());
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<bool> OctiNetworkClient::purgeTable(const std::string &roomId)
{
    ApiResult<bool> res;
    Impl::RawResponse raw =
        _impl->doDelete("/api/v1/tables/" + urlEncode(roomId));
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<bool> OctiNetworkClient::kickSeat(
    const std::string &roomId,
    const std::string &targetPlayerUuid)
{
    ApiResult<bool> res;
    Json body;
    body["target"] = targetPlayerUuid;
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/tables/" + urlEncode(roomId) + "/kick", body);
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<bool> OctiNetworkClient::kickSeat(
    const std::string &roomId,
    int seat)
{
    ApiResult<GameViewData> gv = fetchGameView(roomId);
    if (!gv.ok) {
        ApiResult<bool> res;
        res.ok = false;
        res.httpStatus = gv.httpStatus;
        res.errorCode = gv.errorCode;
        res.errorMessage = gv.errorMessage;
        return res;
    }
    for (std::size_t i = 0; i < gv.data.seats.size(); ++i) {
        if (gv.data.seats[i].seat == seat &&
            !gv.data.seats[i].playerUuid.empty()) {
            return kickSeat(roomId, gv.data.seats[i].playerUuid);
        }
    }
    return kickSeat(roomId, std::string());
}

ApiResult<bool> OctiNetworkClient::observeTable(
    const std::string &roomId,
    const std::string &passcode)
{
    ApiResult<bool> res;
    Json body = Json::object();
    if (!passcode.empty()) {
        body["passcode"] = passcode;
    }
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/tables/" + urlEncode(roomId) + "/observe", body);
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<bool> OctiNetworkClient::unobserveTable(const std::string &roomId)
{
    ApiResult<bool> res;
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/tables/" + urlEncode(roomId) + "/unobserve", Json::object());
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<bool> OctiNetworkClient::enqueueMatchmaking(
    const MatchmakingRequest &req)
{
    ApiResult<bool> res;
    Json body;
    body["mode"] = req.mode;
    body["bases"] = req.bases;
    body["super_prongs"] = req.superProngs;
    body["edgeless"] = req.edgeless;
    body["allow_robots"] = req.allowRobots;
    if (req.timeControlEnabled && req.initialMs > 0) {
        Json tc;
        tc["initial_ms"] = req.initialMs;
        tc["increment_ms"] = req.incrementMs;
        body["time_control"] = tc;
    } else {
        body["time_control"] = "untimed";
    }
    Impl::RawResponse raw =
        _impl->doPost("/api/v1/matchmaking/enqueue", body);
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<bool> OctiNetworkClient::dequeueMatchmaking()
{
    ApiResult<bool> res;
    Impl::RawResponse raw =
        _impl->doPost("/api/v1/matchmaking/dequeue", Json::object());
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<MatchmakingStatusData> OctiNetworkClient::fetchMatchmakingStatus()
{
    ApiResult<MatchmakingStatusData> res;
    Impl::RawResponse raw = _impl->doGet("/api/v1/matchmaking/status");
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data.queued = raw.json.value("queued", false);
    res.data.waitedMs =
        raw.json.value("waited_ms", static_cast<std::int64_t>(0));
    res.data.window = raw.json.value("window", 0);
    res.data.rating = raw.json.value("rating", 1200);
    res.data.matchedRoomId = raw.json.value("matched_room_id", "");
    return res;
}

/* ---- 23..32: Live Game, Chat, Voice Signaling & Moderation ---- */

ApiResult<GameViewData> OctiNetworkClient::fetchGameView(
    const std::string &roomId)
{
    ApiResult<GameViewData> res;
    Impl::RawResponse raw =
        _impl->doGet("/api/v1/games/" + urlEncode(roomId));
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = parseGameView(raw.json);
    return res;
}

ApiResult<MoveSubmitData> OctiNetworkClient::submitMove(
    const std::string &roomId,
    const std::string &moveStr)
{
    return submitMove(roomId, moveStr, MoveTelemetryData());
}

ApiResult<MoveSubmitData> OctiNetworkClient::submitMove(
    const std::string &roomId,
    const std::string &moveStr,
    const MoveTelemetryData &telemetry)
{
    ApiResult<MoveSubmitData> res;
    Json body;
    body["move"] = moveStr;
    if (telemetry.present) {
        Json ai;
        ai["depth"] = telemetry.depth;
        ai["nodes"] = telemetry.nodes;
        ai["score"] = telemetry.score;
        ai["elapsed_ms"] = telemetry.elapsedMs;
        body["ai"] = ai;
    }
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/games/" + urlEncode(roomId) + "/move", body);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data.move = raw.json.value("move", moveStr);
    res.data.clockMs =
        raw.json.value("clock_ms", static_cast<std::int64_t>(-1));
    res.data.elapsedMs =
        raw.json.value("elapsed_ms", static_cast<std::int64_t>(0));
    res.data.gameOver = raw.json.value("game_over", false);
    res.data.seq = raw.json.value("seq", static_cast<std::int64_t>(0));
    return res;
}

ApiResult<bool> OctiNetworkClient::resignGame(const std::string &roomId)
{
    ApiResult<bool> res;
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/games/" + urlEncode(roomId) + "/resign", Json::object());
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = raw.json.value("game_over", true);
    return res;
}

ApiResult<GameActionData> OctiNetworkClient::submitGameAction(
    const std::string &roomId,
    const std::string &action)
{
    ApiResult<GameActionData> res;
    Json body;
    body["action"] = action;
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/games/" + urlEncode(roomId) + "/action", body);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data.action = raw.json.value("action", action);
    res.data.roomId = raw.json.value("room_id", roomId);
    res.data.gameOver = raw.json.value("game_over", false);
    res.data.seq = raw.json.value("seq", static_cast<std::int64_t>(0));
    return res;
}

ApiResult<std::int64_t> OctiNetworkClient::postGameChat(
    const std::string &roomId,
    const std::string &scope,
    const std::string &text)
{
    ApiResult<std::int64_t> res;
    Json body;
    body["scope"] = toLowerAscii(scope.empty() ? "all" : scope);
    body["text"] = text;
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/games/" + urlEncode(roomId) + "/chat", body);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = raw.json.value("seq", static_cast<std::int64_t>(0));
    return res;
}

ApiResult<std::int64_t> OctiNetworkClient::submitVoiceSignal(
    const std::string &roomId,
    const std::string &toPlayerUuid,
    const std::string &signalType,
    const std::string &payloadJson)
{
    ApiResult<std::int64_t> res;
    Json body;
    body["target"] = toPlayerUuid;
    body["type"] = signalType;
    Json parsedPayload = Json::parse(payloadJson, nullptr, false);
    if (!parsedPayload.is_discarded()) {
        body["payload"] = parsedPayload;
    } else {
        body["payload"] = payloadJson;
    }
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/games/" + urlEncode(roomId) + "/voice-signal", body);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = raw.json.value("seq", static_cast<std::int64_t>(0));
    return res;
}

ApiResult<std::int64_t> OctiNetworkClient::submitVoiceMeta(
    const std::string &roomId,
    const std::string &eventType,
    const std::string &targetPlayerUuid)
{
    (void)targetPlayerUuid;
    ApiResult<std::int64_t> res;
    std::string action = toUpperAscii(eventType);
    if (action == "SPEAKING_START") {
        action = "MIC_UNMUTED";
    } else if (action == "SPEAKING_STOP") {
        action = "MIC_MUTED";
    }
    Json body;
    body["action"] = action;
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/games/" + urlEncode(roomId) + "/voice-meta", body);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = raw.json.value("seq", static_cast<std::int64_t>(0));
    return res;
}

ApiResult<bool> OctiNetworkClient::banObserver(
    const std::string &roomId,
    const std::string &targetPlayerUuid,
    const std::string &reason)
{
    ApiResult<bool> res;
    Json body;
    body["target"] = targetPlayerUuid;
    if (!reason.empty()) {
        body["reason"] = reason;
    }
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/games/" + urlEncode(roomId) + "/ban", body);
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<bool> OctiNetworkClient::muteParticipant(
    const std::string &roomId,
    const std::string &targetPlayerUuid,
    bool muteChat,
    bool muteVoice)
{
    ApiResult<bool> res;
    Json body;
    body["target"] = targetPlayerUuid;
    body["mute_chat"] = muteChat;
    body["mute_voice"] = muteVoice;
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/games/" + urlEncode(roomId) + "/mute", body);
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<RoomSettingsData> OctiNetworkClient::updateRoomSettings(
    const std::string &roomId,
    const std::string &observerPolicy,
    const std::string &voicePolicy)
{
    ApiResult<RoomSettingsData> res;
    Json body = Json::object();
    if (!observerPolicy.empty()) {
        body["observer_policy"] = observerPolicy;
    }
    if (!voicePolicy.empty()) {
        body["voice_policy"] = voicePolicy;
    }
    Impl::RawResponse raw = _impl->doPost(
        "/api/v1/games/" + urlEncode(roomId) + "/settings", body);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data.observerPolicy =
        raw.json.value("observer_policy", observerPolicy);
    res.data.voicePolicy = raw.json.value("voice_policy", voicePolicy);
    res.data.seq = raw.json.value("seq", static_cast<std::int64_t>(0));
    return res;
}

/* ---- 33..35: Game Archive & .octi Transcript ---- */

ApiResult<ArchivedGamesPage> OctiNetworkClient::listArchivedGames(
    const std::string &mode,
    const std::string &playerUuid,
    int limit,
    int offset)
{
    ApiResult<ArchivedGamesPage> res;
    std::string path = "/api/v1/archive/games?limit=" +
                       std::to_string(limit) +
                       "&offset=" + std::to_string(offset);
    if (!mode.empty()) {
        path += "&mode=" + urlEncode(mode);
    }
    if (!playerUuid.empty()) {
        path += "&player=" + urlEncode(playerUuid);
    }
    Impl::RawResponse raw = _impl->doGet(path);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data.total = raw.json.value("total", static_cast<std::int64_t>(0));
    res.data.limit = raw.json.value("limit", limit);
    res.data.offset = raw.json.value("offset", offset);
    if (raw.json.contains("games") && raw.json["games"].is_array()) {
        const Json &arr = raw.json["games"];
        for (std::size_t i = 0; i < arr.size(); ++i) {
            res.data.games.push_back(parseArchivedGameSummary(arr[i]));
        }
    }
    return res;
}

ApiResult<ArchivedGameSummary> OctiNetworkClient::fetchArchivedGame(
    const std::string &gameId)
{
    ApiResult<ArchivedGameSummary> res;
    Impl::RawResponse raw =
        _impl->doGet("/api/v1/archive/games/" + urlEncode(gameId));
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = parseArchivedGameSummary(raw.json);
    return res;
}

ApiResult<bool> OctiNetworkClient::purgeArchivedGame(
    const std::string &gameId)
{
    ApiResult<bool> res;
    Impl::RawResponse raw =
        _impl->doDelete("/api/v1/archive/games/" + urlEncode(gameId));
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<std::string> OctiNetworkClient::downloadOctiTranscript(
    const std::string &gameId)
{
    ApiResult<std::string> res;
    Impl::RawResponse raw =
        _impl->doGet("/api/v1/archive/games/" + urlEncode(gameId) + "/transcript");
    if (!raw.transportOk || raw.status != 200) {
        raw = _impl->doGet("/api/v1/archive/games/" + urlEncode(gameId) + "/octi");
        if (!raw.transportOk || raw.status != 200) {
            Impl::populateError(raw, res);
            return res;
        }
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = raw.body;
    return res;
}

ApiResult<UploadTranscriptResponse> OctiNetworkClient::uploadTrainingTranscript(
    const UploadTranscriptRequest &req)
{
    ApiResult<UploadTranscriptResponse> res;
    Json body;
    Json parsed = Json::parse(req.transcriptJson, nullptr, false);
    if (!parsed.is_discarded() && parsed.is_object()) {
        body["transcript"] = parsed;
    } else {
        body["transcript"] = req.transcriptJson;
    }
    if (!req.clientId.empty()) {
        body["clientId"] = req.clientId;
    }

    Impl::RawResponse raw = _impl->doPost("/api/v1/archive/upload", body);
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }

    res.ok = true;
    res.httpStatus = raw.status;
    if (raw.json.is_object()) {
        if (raw.json.contains("game_id") && raw.json["game_id"].is_string()) {
            res.data.gameId = raw.json["game_id"].get<std::string>();
        } else if (raw.json.contains("gameId") && raw.json["gameId"].is_string()) {
            res.data.gameId = raw.json["gameId"].get<std::string>();
        }
        if (raw.json.contains("ply_count") && raw.json["ply_count"].is_number_integer()) {
            res.data.plyCount = raw.json["ply_count"].get<int>();
        } else if (raw.json.contains("plyCount") && raw.json["plyCount"].is_number_integer()) {
            res.data.plyCount = raw.json["plyCount"].get<int>();
        }
    }
    return res;
}

CrashReportResponse OctiNetworkClient::uploadCrashReport(
    const CrashReportRequest &req)
{
    CrashReportResponse res;
    Json body;
    if (!req.crashId.empty()) {
        body["crash_id"] = req.crashId;
    }
    if (req.createdAt > 0) {
        body["created_at"] = req.createdAt;
    }
    body["platform"] = req.platform;
    body["app_version"] = req.appVersion;
    body["os_version"] = req.osVersion;
    body["exception_type"] = req.exceptionType;
    body["exception_message"] = req.exceptionMessage;
    body["thread_name"] = req.threadName;
    body["stack_trace"] = req.stackTrace;

    Json ctx = Json::parse(req.contextJson, nullptr, false);
    if (!ctx.is_discarded() && ctx.is_object()) {
        body["context"] = ctx;
    } else if (!req.contextJson.empty()) {
        body["context"] = req.contextJson;
    } else {
        body["context"] = Json::object();
    }

    if (!req.installId.empty()) {
        body["install_id"] = req.installId;
    }

    Impl::RawResponse raw = _impl->doPost("/api/v1/telemetry/crash", body);
    res.httpStatus = raw.status;

    if (raw.transportOk && (raw.status == 200 || raw.status == 201)) {
        res.ok = true;
        if (raw.json.is_object()) {
            res.crashId = raw.json.value("crash_id", "");
            res.deduplicated = raw.json.value("deduplicated", false);
        }
    } else {
        res.ok = false;
        if (raw.json.is_object()) {
            res.errorMessage =
                raw.json.value("message", raw.json.value("error", ""));
        }
        if (res.errorMessage.empty() && !raw.transportError.empty()) {
            res.errorMessage = raw.transportError;
        }
    }
    return res;
}

ApiResult<int> OctiNetworkClient::deleteInstallArchiveData(
    const std::string &installId)
{
    ApiResult<int> res;
    Impl::RawResponse raw =
        _impl->doDelete("/api/v1/archive/install/" + urlEncode(installId));
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    if (raw.json.is_object() && raw.json.contains("deleted") &&
        raw.json["deleted"].is_number_integer()) {
        res.data = raw.json["deleted"].get<int>();
    }
    return res;
}

/* ---- 36..38: Robot Protocol & Administration ---- */

ApiResult<RobotConnectData> OctiNetworkClient::connectRobot(bool ifUnused)
{
    ApiResult<RobotConnectData> res;
    Json body;
    body["if_unused"] = ifUnused;
    Impl::RawResponse raw = _impl->doPost("/api/v1/robots/connect", body);
    if (!raw.transportOk || (raw.status != 200 && raw.status != 201) ||
        !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    const Json &d = (raw.json.contains("data") && raw.json["data"].is_object())
        ? raw.json["data"]
        : raw.json;
    res.data.leaseId = d.value("lease_id", "");
    res.data.robotUuid = d.value("robot_uuid", "");
    res.data.handle = d.value("handle", d.value("robot_name", ""));
    res.data.description =
        d.value("description", d.value("robot_description", ""));
    res.data.ownerUuid = d.value("owner_uuid", "");
    res.data.ownerHandle = d.value("owner_handle", "");
    res.data.avatarUrl = d.value("avatar_url", "");
    res.data.activeRoomId = d.value("active_room_id", "");
    res.data.gamesPlayed =
        d.value("games_played", d.value("robot_games_played", 0));
    res.data.wins = d.value("wins", d.value("robot_wins", 0));
    res.data.losses = d.value("losses", d.value("robot_losses", 0));
    res.data.draws = d.value("draws", d.value("robot_draws", 0));

    if (!res.data.leaseId.empty()) {
        setRobotLeaseId(res.data.leaseId);
    }
    return res;
}

ApiResult<bool> OctiNetworkClient::disconnectRobot()
{
    ApiResult<bool> res;
    Impl::RawResponse raw =
        _impl->doPost("/api/v1/robots/disconnect", Json::object());
    clearRobotLeaseId();
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    res.data = true;
    return res;
}

ApiResult<std::string> OctiNetworkClient::exportRobotTokensConf()
{
    ApiResult<std::string> res;
    Impl::RawResponse raw = _impl->doGet("/api/v1/robots/export-conf");
    if (!raw.transportOk || raw.status != 200) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    if (raw.json.is_object() && raw.json.contains("conf") &&
        raw.json["conf"].is_string()) {
        res.data = raw.json["conf"].get<std::string>();
    } else {
        res.data = raw.body;
    }
    return res;
}

/* ---- Poll & Server-Sent Events (SSE) Live Streams ---- */

ApiResult<std::vector<SseEvent>> OctiNetworkClient::pollLobbyEvents(
    std::uint64_t sinceSeq,
    int limit)
{
    ApiResult<std::vector<SseEvent>> res;
    std::string path = "/api/v1/lobby/events?since_seq=" +
                       std::to_string(sinceSeq) +
                       "&limit=" + std::to_string(limit);
    std::string lease = _impl->currentRobotLeaseId();
    if (!lease.empty()) {
        path += "&lease_id=" + urlEncode(lease);
    }
    Impl::RawResponse raw = _impl->doGet(path);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    if (raw.json.contains("events") && raw.json["events"].is_array()) {
        const Json &arr = raw.json["events"];
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].is_object()) {
                continue;
            }
            SseEvent ev;
            ev.id = arr[i].value("seq", static_cast<std::uint64_t>(0));
            ev.event = arr[i].value("type", "");
            ev.dataJson = arr[i].dump();
            res.data.push_back(std::move(ev));
        }
    }
    return res;
}

ApiResult<std::vector<SseEvent>> OctiNetworkClient::pollGameEvents(
    const std::string &roomId,
    std::uint64_t sinceSeq,
    int limit)
{
    ApiResult<std::vector<SseEvent>> res;
    std::string path = "/api/v1/games/" + urlEncode(roomId) +
                       "/events?since_seq=" + std::to_string(sinceSeq) +
                       "&limit=" + std::to_string(limit);
    std::string lease = _impl->currentRobotLeaseId();
    if (!lease.empty()) {
        path += "&lease_id=" + urlEncode(lease);
    }
    Impl::RawResponse raw = _impl->doGet(path);
    if (!raw.transportOk || raw.status != 200 || !raw.json.is_object()) {
        Impl::populateError(raw, res);
        return res;
    }
    res.ok = true;
    res.httpStatus = raw.status;
    if (raw.json.contains("events") && raw.json["events"].is_array()) {
        const Json &arr = raw.json["events"];
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].is_object()) {
                continue;
            }
            SseEvent ev;
            ev.id = arr[i].value("seq", static_cast<std::uint64_t>(0));
            ev.event = arr[i].value("type", "");
            ev.dataJson = arr[i].dump();
            res.data.push_back(std::move(ev));
        }
    }
    return res;
}

void OctiNetworkClient::startLobbyEventStream(
    std::uint64_t sinceSeq,
    std::function<void(const SseEvent &)> onEvent)
{
    _impl->launchSseStream(_impl->lobbyStream, "/api/v1/lobby/events",
                           sinceSeq, std::move(onEvent));
}

void OctiNetworkClient::stopLobbyEventStream()
{
    _impl->lobbyStream.stop();
}

bool OctiNetworkClient::isLobbyEventStreamActive() const
{
    return _impl->lobbyStream.active.load();
}

std::uint64_t OctiNetworkClient::lastLobbyEventId() const
{
    return _impl->lobbyStream.lastId.load();
}

void OctiNetworkClient::startGameEventStream(
    const std::string &roomId,
    std::uint64_t sinceSeq,
    std::function<void(const SseEvent &)> onEvent)
{
    _impl->launchSseStream(
        _impl->gameStream,
        "/api/v1/games/" + urlEncode(roomId) + "/events",
        sinceSeq,
        std::move(onEvent));
}

void OctiNetworkClient::stopGameEventStream()
{
    _impl->gameStream.stop();
}

bool OctiNetworkClient::isGameEventStreamActive() const
{
    return _impl->gameStream.active.load();
}

std::uint64_t OctiNetworkClient::lastGameEventId() const
{
    return _impl->gameStream.lastId.load();
}

void OctiNetworkClient::stopEventStreams()
{
    stopLobbyEventStream();
    stopGameEventStream();
}

} // namespace octi

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
