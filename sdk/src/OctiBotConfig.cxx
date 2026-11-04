/*
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiBotConfig.hxx"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

namespace octi {

namespace {

std::string trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return s.substr(start, end - start);
}

std::string toLower(const std::string &s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return res;
}

#if !defined(_WIN32)
void ensureDirectoryPermissions(const std::string &dirPath) {
    if (dirPath.empty()) {
        return;
    }
    std::string current;
    for (size_t i = 0; i < dirPath.size(); ++i) {
        current += dirPath[i];
        if (dirPath[i] == '/' || i + 1 == dirPath.size()) {
            struct stat st;
            if (::stat(current.c_str(), &st) != 0) {
                ::mkdir(current.c_str(), 0700);
            }
            ::chmod(current.c_str(), 0700);
        }
    }
}
#endif

} // namespace

std::string OctiBotConfigStore::defaultConfigDir() {
#if defined(_WIN32)
    const char *appData = std::getenv("APPDATA");
    if (appData && *appData) {
        return std::string(appData) + "\\OCTIBOT";
    }
    const char *userProfile = std::getenv("USERPROFILE");
    if (userProfile && *userProfile) {
        return std::string(userProfile) + "\\.config\\octibot";
    }
    return "C:\\OCTIBOT";
#else
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        return std::string(xdg) + "/octibot";
    }
    const char *home = std::getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.config/octibot";
    }
    return "/tmp/octibot";
#endif
}

std::string OctiBotConfigStore::defaultConfigFilePath() {
#if defined(_WIN32)
    return defaultConfigDir() + "\\octibot.conf";
#else
    return defaultConfigDir() + "/octibot.conf";
#endif
}

std::string OctiBotConfigStore::defaultTokensFilePath() {
#if defined(_WIN32)
    return defaultConfigDir() + "\\tokens.conf";
#else
    return defaultConfigDir() + "/tokens.conf";
#endif
}

bool OctiBotConfigStore::parseString(const std::string &content, std::string &errorOut) {
    _entries.clear();
    _profiles.clear();

    std::istringstream stream(content);
    std::string line;
    std::string currentProfile;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        // Section header: [profile_name]
        if (trimmed[0] == '[') {
            if (trimmed.back() != ']') {
                errorOut = "Malformed section header on line " + std::to_string(lineNum) + ": missing ']'";
                return false;
            }
            std::string secName = trim(trimmed.substr(1, trimmed.size() - 2));
            if (secName.empty()) {
                errorOut = "Empty section name on line " + std::to_string(lineNum);
                return false;
            }
            for (char c : secName) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
                    errorOut = "Invalid character in section name '" + secName + "' on line " + std::to_string(lineNum);
                    return false;
                }
            }
            currentProfile = secName;
            if (_profiles.find(currentProfile) == _profiles.end()) {
                _profiles[currentProfile] = {};
            }
            continue;
        }

        // key = value parser
        size_t eqPos = std::string::npos;
        bool inSingle = false;
        bool inDouble = false;
        for (size_t i = 0; i < trimmed.size(); ++i) {
            char c = trimmed[i];
            if (c == '\\' && inDouble && i + 1 < trimmed.size()) {
                i++;
                continue;
            }
            if (c == '\'' && !inDouble) {
                inSingle = !inSingle;
            } else if (c == '"' && !inSingle) {
                inDouble = !inDouble;
            } else if (!inSingle && !inDouble) {
                if (c == '=') {
                    eqPos = i;
                    break;
                }
            }
        }

        if (eqPos == std::string::npos) {
            errorOut = "Missing '=' on line " + std::to_string(lineNum) + ": " + trimmed;
            return false;
        }

        std::string rawKey = trim(trimmed.substr(0, eqPos));
        if (rawKey.empty()) {
            errorOut = "Empty key on line " + std::to_string(lineNum);
            return false;
        }

        // Parse value taking quotes and comments into account
        std::string rawVal = trim(trimmed.substr(eqPos + 1));
        std::string parsedVal;
        inSingle = false;
        inDouble = false;
        char quoteChar = '\0';

        if (!rawVal.empty() && (rawVal[0] == '"' || rawVal[0] == '\'')) {
            quoteChar = rawVal[0];
            size_t idx = 1;
            while (idx < rawVal.size()) {
                char c = rawVal[idx];
                if (c == '\\' && quoteChar == '"' && idx + 1 < rawVal.size()) {
                    char next = rawVal[idx + 1];
                    if (next == 'n') {
                        parsedVal.push_back('\n');
                    } else if (next == 't') {
                        parsedVal.push_back('\t');
                    } else if (next == '\\' || next == '"') {
                        parsedVal.push_back(next);
                    } else {
                        parsedVal.push_back(next);
                    }
                    idx += 2;
                    continue;
                }
                if (c == quoteChar) {
                    // Closed quote
                    idx++;
                    // Any trailing characters after closing quote must only be comment or whitespace
                    std::string trailing = trim(rawVal.substr(idx));
                    if (!trailing.empty() && trailing[0] != '#' && trailing[0] != ';') {
                        errorOut = "Extra characters after closing quote on line " + std::to_string(lineNum);
                        return false;
                    }
                    break;
                }
                parsedVal.push_back(c);
                idx++;
            }
            if (idx > rawVal.size() || (idx == rawVal.size() && rawVal[idx - 1] != quoteChar)) {
                errorOut = "Unterminated quote on line " + std::to_string(lineNum);
                return false;
            }
        } else {
            // Unquoted value: strip trailing comment outside quotes
            for (size_t i = 0; i < rawVal.size(); ++i) {
                char c = rawVal[i];
                if (c == '#' || c == ';') {
                    rawVal = rawVal.substr(0, i);
                    break;
                }
            }
            parsedVal = trim(rawVal);
        }

        if (currentProfile.empty()) {
            _entries[rawKey] = parsedVal;
        } else {
            _profiles[currentProfile][rawKey] = parsedVal;
        }
    }

    return true;
}

bool OctiBotConfigStore::loadFile(const std::string &path, std::string &errorOut) {
    std::ifstream file(path);
    if (!file.is_open()) {
        errorOut = "Failed to open config file: " + path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parseString(buffer.str(), errorOut);
}

bool OctiBotConfigStore::loadTokensFile(const std::string &path, std::string &errorOut) {
    std::ifstream file(path);
    if (!file.is_open()) {
        errorOut = "Failed to open tokens file: " + path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    OctiBotConfigStore tmp;
    if (!tmp.parseString(buffer.str(), errorOut)) {
        return false;
    }
    for (const auto &kv : tmp._entries) {
        _entries[kv.first] = kv.second;
    }
    return true;
}

bool OctiBotConfigStore::saveFile(const std::string &path, std::string &errorOut) const {
#if !defined(_WIN32)
    size_t slashPos = path.find_last_of('/');
    if (slashPos != std::string::npos) {
        std::string parentDir = path.substr(0, slashPos);
        ensureDirectoryPermissions(parentDir);
    }
#endif

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        errorOut = "Failed to create/write config file: " + path;
        return false;
    }

    file << "# OCTIBOT Configuration\n";
    for (const auto &kv : _entries) {
        file << kv.first << " = " << kv.second << "\n";
    }

    for (const auto &prof : _profiles) {
        file << "\n[" << prof.first << "]\n";
        for (const auto &kv : prof.second) {
            file << kv.first << " = " << kv.second << "\n";
        }
    }

    file.flush();
    file.close();

#if !defined(_WIN32)
    ::chmod(path.c_str(), 0600);
#endif

    return true;
}

std::vector<std::string> OctiBotConfigStore::profileNames() const {
    std::vector<std::string> res;
    for (const auto &kv : _profiles) {
        res.push_back(kv.first);
    }
    return res;
}

bool OctiBotConfigStore::hasProfile(const std::string &profileName) const {
    return _profiles.find(profileName) != _profiles.end();
}

OctiBotConfigStore OctiBotConfigStore::resolveProfile(const std::string &profileName) const {
    OctiBotConfigStore res;
    res._entries = _entries; // Inherit global entries
    res._profiles = _profiles;

    auto it = _profiles.find(profileName);
    if (it != _profiles.end()) {
        for (const auto &kv : it->second) {
            res._entries[kv.first] = kv.second;
        }
    }

    if (res.getString("robot_name").empty() && !profileName.empty()) {
        res.set("robot_name", profileName);
    }

    return res;
}

std::vector<std::pair<std::string, std::string>> OctiBotConfigStore::tokenPool() const {
    std::vector<std::pair<std::string, std::string>> pool;
    const std::string prefix = "bot_token_";
    for (const auto &kv : _entries) {
        if (kv.first.rfind(prefix, 0) == 0 && kv.first.size() > prefix.size()) {
            std::string robotName = kv.first.substr(prefix.size());
            if (!robotName.empty() && !kv.second.empty()) {
                pool.emplace_back(robotName, kv.second);
            }
        }
    }
    std::sort(pool.begin(), pool.end());
    return pool;
}

std::string OctiBotConfigStore::resolveTokenForRobot(const std::string &robotName) const {
    std::string directToken = getString("bot_token");
    if (!directToken.empty() && directToken != "auto" &&
        directToken.rfind("octi_bot_", 0) == 0) {
        return directToken;
    }
    if (!robotName.empty()) {
        std::string poolToken = getString("bot_token_" + robotName);
        if (!poolToken.empty() && poolToken.rfind("octi_bot_", 0) == 0) {
            return poolToken;
        }
    }
    return "";
}

bool OctiBotConfigStore::hasKey(const std::string &key) const {
    return _entries.find(key) != _entries.end();
}

std::vector<std::string> OctiBotConfigStore::keys() const {
    std::vector<std::string> res;
    for (const auto &kv : _entries) {
        res.push_back(kv.first);
    }
    return res;
}

void OctiBotConfigStore::set(const std::string &key, const std::string &value) {
    _entries[key] = value;
}

std::string OctiBotConfigStore::getString(const std::string &key,
                                         const std::string &defaultVal) const {
    auto it = _entries.find(key);
    if (it != _entries.end()) {
        return it->second;
    }
    return defaultVal;
}

int OctiBotConfigStore::getInt(const std::string &key, int defaultVal) const {
    auto it = _entries.find(key);
    if (it == _entries.end() || it->second.empty()) {
        return defaultVal;
    }
    char *endptr = nullptr;
    errno = 0;
    long val = std::strtol(it->second.c_str(), &endptr, 10);
    if (errno != 0 || endptr == it->second.c_str() || *endptr != '\0') {
        return defaultVal;
    }
    return static_cast<int>(val);
}

std::int64_t OctiBotConfigStore::getInt64(const std::string &key,
                                         std::int64_t defaultVal) const {
    auto it = _entries.find(key);
    if (it == _entries.end() || it->second.empty()) {
        return defaultVal;
    }
    char *endptr = nullptr;
    errno = 0;
    long long val = std::strtoll(it->second.c_str(), &endptr, 10);
    if (errno != 0 || endptr == it->second.c_str() || *endptr != '\0') {
        return defaultVal;
    }
    return static_cast<std::int64_t>(val);
}

double OctiBotConfigStore::getDouble(const std::string &key, double defaultVal) const {
    auto it = _entries.find(key);
    if (it == _entries.end() || it->second.empty()) {
        return defaultVal;
    }
    char *endptr = nullptr;
    errno = 0;
    double val = std::strtod(it->second.c_str(), &endptr);
    if (errno != 0 || endptr == it->second.c_str() || *endptr != '\0') {
        return defaultVal;
    }
    return val;
}

bool OctiBotConfigStore::getBool(const std::string &key, bool defaultVal) const {
    auto it = _entries.find(key);
    if (it == _entries.end() || it->second.empty()) {
        return defaultVal;
    }
    std::string lower = toLower(trim(it->second));
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
        return false;
    }
    return defaultVal;
}

int OctiBotConfigStore::parseVerbosityLevel(const std::string &val, int defaultLevel) {
    std::string s = toLower(trim(val));
    if (s.empty()) {
        return defaultLevel;
    }
    if (s == "verbose" || s == "debug" || s == "trace" || s == "max" || s == "3") {
        return 3;
    }
    if (s == "info" || s == "normal" || s == "2") {
        return 2;
    }
    if (s == "error" || s == "warn" || s == "1") {
        return 1;
    }
    if (s == "quiet" || s == "silent" || s == "none" || s == "0" || s == "false") {
        return 0;
    }
    return defaultLevel;
}

int OctiBotConfigStore::getVerbosityLevel(int defaultLevel) const {
    auto it = _entries.find("verbosity");
    if (it == _entries.end() || it->second.empty()) {
        return defaultLevel;
    }
    return parseVerbosityLevel(it->second, defaultLevel);
}

void applyConfigToTuningProfile(const OctiBotConfigStore &config,
                               BotTuningProfile &profile) {
    profile.timeBudgetMs = config.getInt("time_budget_ms", profile.timeBudgetMs);
    profile.maxSearchDepth = config.getInt("max_search_depth", profile.maxSearchDepth);
    profile.maxQuiesceDepth = config.getInt("max_quiesce_depth", profile.maxQuiesceDepth);
    profile.quiesceBranchCap = config.getInt("quiesce_branch_cap", profile.quiesceBranchCap);
    profile.rootBranchCap = config.getInt("root_branch_cap", profile.rootBranchCap);
    profile.treeBranchCap = config.getInt("tree_branch_cap", profile.treeBranchCap);
    profile.lazySmpThreads = config.getInt("lazy_smp_threads", profile.lazySmpThreads);
    profile.compositeMoveMaxPly = config.getInt("composite_move_max_ply", profile.compositeMoveMaxPly);
    profile.rootScoreJitter = config.getInt("root_score_jitter", profile.rootScoreJitter);

    profile.capturedEnemyBaseBonus = config.getInt("captured_enemy_base_bonus", profile.capturedEnemyBaseBonus);
    profile.lostAlliedBasePenalty = config.getInt("lost_allied_base_penalty", profile.lostAlliedBasePenalty);
    profile.neutralOctiSquareBonus = config.getInt("neutral_octi_square_bonus", profile.neutralOctiSquareBonus);
    profile.closestPodDistWeight = config.getInt("closest_pod_dist_weight", profile.closestPodDistWeight);
    profile.secondClosestPodDistWeight = config.getInt("second_closest_pod_dist_weight", profile.secondClosestPodDistWeight);
    profile.oneStepBaseThreatBonus = config.getInt("one_step_base_threat_bonus", profile.oneStepBaseThreatBonus);
    profile.oneJumpBaseThreatBonus = config.getInt("one_jump_base_threat_bonus", profile.oneJumpBaseThreatBonus);
    profile.twoPlyBaseThreatWeight = config.getInt("two_ply_base_threat_weight", profile.twoPlyBaseThreatWeight);

    profile.podZeroProngVal = config.getInt("pod_zero_prong_val", profile.podZeroProngVal);
    profile.podOneProngVal = config.getInt("pod_one_prong_val", profile.podOneProngVal);
    profile.podTwoProngVal = config.getInt("pod_two_prong_val", profile.podTwoProngVal);
    profile.podThreePlusProngVal = config.getInt("pod_three_plus_prong_val", profile.podThreePlusProngVal);
    profile.normalProngVal = config.getInt("normal_prong_val", profile.normalProngVal);
    profile.superProngVal = config.getInt("super_prong_val", profile.superProngVal);
    profile.orthDiagVersatilityBonus = config.getInt("orth_diag_versatility_bonus", profile.orthDiagVersatilityBonus);
    profile.diagOnlyParityPenalty = config.getInt("diag_only_parity_penalty", profile.diagOnlyParityPenalty);
    profile.firstArmedPodBonus = config.getInt("first_armed_pod_bonus", profile.firstArmedPodBonus);
    profile.secondArmedPodBonus = config.getInt("second_armed_pod_bonus", profile.secondArmedPodBonus);
    profile.thirdArmedPodBonus = config.getInt("third_armed_pod_bonus", profile.thirdArmedPodBonus);
    profile.singlePodMonopolyPenalty = config.getInt("single_pod_monopoly_penalty", profile.singlePodMonopolyPenalty);
    profile.loneOneProngRushPenalty = config.getInt("lone_one_prong_rush_penalty", profile.loneOneProngRushPenalty);

    profile.leapfrogBatteryWeight = config.getInt("leapfrog_battery_weight", profile.leapfrogBatteryWeight);
    profile.leapfrogForwardBonus = config.getInt("leapfrog_forward_bonus", profile.leapfrogForwardBonus);
    profile.jumpReachableSquareWeight = config.getInt("jump_reachable_square_weight", profile.jumpReachableSquareWeight);
    profile.jumpReachStepWeight = config.getInt("jump_reach_step_weight", profile.jumpReachStepWeight);
    profile.jumpCapturablePodWeight = config.getInt("jump_capturable_pod_weight", profile.jumpCapturablePodWeight);
    profile.jumpCapturableProngWeight = config.getInt("jump_capturable_prong_weight", profile.jumpCapturableProngWeight);

    profile.hangingPodBasePenalty = config.getInt("hanging_pod_base_penalty", profile.hangingPodBasePenalty);
    profile.hangingProngWeight = config.getInt("hanging_prong_weight", profile.hangingProngWeight);
    profile.ofkProngSeizureWeight = config.getInt("ofk_prong_seizure_weight", profile.ofkProngSeizureWeight);
    profile.opponentTurnHangingPct = config.getInt("opponent_turn_hanging_pct", profile.opponentTurnHangingPct);
    profile.soleArmedPodDangerPenalty = config.getInt("sole_armed_pod_danger_penalty", profile.soleArmedPodDangerPenalty);
    profile.wipeoutDangerPenalty = config.getInt("wipeout_danger_penalty", profile.wipeoutDangerPenalty);
    profile.moveOrderCaptureBonus = config.getInt("move_order_capture_bonus", profile.moveOrderCaptureBonus);
    profile.moveOrderBaseEntryBonus = config.getInt("move_order_base_entry_bonus", profile.moveOrderBaseEntryBonus);
    profile.moveOrderLeapfrogBonus = config.getInt("move_order_leapfrog_bonus", profile.moveOrderLeapfrogBonus);
    profile.moveOrderBlunderPenalty = config.getInt("move_order_blunder_penalty", profile.moveOrderBlunderPenalty);
}

} // namespace octi

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set ft=cpp et ts=4 sts=4 sw=4:
 */
