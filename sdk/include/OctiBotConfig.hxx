/*
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTIBOT_CONFIG_HXX
#define OCTIBOT_CONFIG_HXX

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "OctiBotEngine.hxx"

namespace octi {

class OctiBotConfigStore {
public:
    static std::string defaultConfigFilePath();
    static std::string defaultTokensFilePath();
    static std::string defaultConfigDir();

    bool loadFile(const std::string &path, std::string &errorOut);
    bool parseString(const std::string &content, std::string &errorOut);
    bool saveFile(const std::string &path, std::string &errorOut) const;

    bool loadTokensFile(const std::string &path, std::string &errorOut);

    std::vector<std::string> profileNames() const;
    bool hasProfile(const std::string &profileName) const;
    OctiBotConfigStore resolveProfile(const std::string &profileName) const;

    std::vector<std::pair<std::string, std::string>> tokenPool() const;
    std::string resolveTokenForRobot(const std::string &robotName) const;

    bool hasKey(const std::string &key) const;
    std::vector<std::string> keys() const;
    void set(const std::string &key, const std::string &value);

    std::string getString(const std::string &key,
                          const std::string &defaultVal = "") const;
    int getInt(const std::string &key, int defaultVal = 0) const;
    std::int64_t getInt64(const std::string &key,
                          std::int64_t defaultVal = 0) const;
    double getDouble(const std::string &key,
                     double defaultVal = 0.0) const;
    bool getBool(const std::string &key,
                 bool defaultVal = false) const;

    static int parseVerbosityLevel(const std::string &val, int defaultLevel = 3);
    int getVerbosityLevel(int defaultLevel = 3) const;

    const std::map<std::string, std::string> &entries() const { return _entries; }

private:
    std::map<std::string, std::string> _entries;
    std::map<std::string, std::map<std::string, std::string>> _profiles;
};

void applyConfigToTuningProfile(const OctiBotConfigStore &config,
                               BotTuningProfile &profile);

} // namespace octi

#endif /* OCTIBOT_CONFIG_HXX */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set ft=cpp et ts=4 sts=4 sw=4:
 */
