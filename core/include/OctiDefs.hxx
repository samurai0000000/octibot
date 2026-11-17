/*
 * OctiDefs.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_DEFS_HXX
#define OCTI_DEFS_HXX

#include "Version.hxx"

#include <string>
#include <vector>

#ifndef OCTI_APP_VERSION
#define OCTI_APP_VERSION OCTI_VERSION_STRING
#endif

namespace octi {

constexpr const char *OCTI_SERVER_HOST = "octi.selfso.com";
constexpr int OCTI_SERVER_PORT = 443;
constexpr const char *OCTI_SERVER_FALLBACK_HOST = "trumpet.selfso.com";
constexpr int OCTI_SERVER_FALLBACK_PORT = 6874;
constexpr int OCTI_SERVER_DAEMON_PORT = 6874;
constexpr int OCTI_PROTOCOL_VERSION = 1;
constexpr int OCTI_MIN_PROTOCOL_VERSION = 1;

constexpr int OCTI_NO_PROGRESS_DRAW_PLIES_2P = 30;
constexpr int OCTI_NO_PROGRESS_DRAW_PLIES_4P = 60;

enum OctiGameMode {
    GAME_MODE_UNDEF = 0,
    GAME_MODE_2     = 1,
    GAME_MODE_4     = 2,
    GAME_MODE_OFK   = 3,
};

enum OctiGameBases {
    GAME_BASES_1 = 1,
    GAME_BASES_3 = 3,
};

enum OctiPlayer {
    PLAYER_UNDEF = 0,
    PLAYER_A     = 1,
    PLAYER_B     = 2,
    PLAYER_C     = 3,
    PLAYER_D     = 4,
};

enum OctiPodType {
    POD_UNDEF = 0,
    A_POD     = 1,
    B_POD     = 2,
    C_POD     = 3,
    D_POD     = 4,
};

enum OctiProngType {
    NO_PRONG     = 0,
    NORMAL_PRONG = 1,
    SUPER_PRONG  = 2,
};

enum OctiSquareType {
    NEUTRAL_SQUARE      = 0,
    A_HOME_SQUARE       = 1,
    B_HOME_SQUARE       = 2,
    C_HOME_SQUARE       = 3,
    D_HOME_SQUARE       = 4,
    NEUTRAL_OCTI_SQUARE = 5,
};

enum OctiDir {
    DIR_UNDEF = -1,
    DIR_N     = 0,
    DIR_NE    = 1,
    DIR_E     = 2,
    DIR_SE    = 3,
    DIR_S     = 4,
    DIR_SW    = 5,
    DIR_W     = 6,
    DIR_NW    = 7,
    DIR_MAX   = 8,
};

inline OctiDir charToDir(char c)
{
    if (c >= 'A' && c <= 'H') {
        return static_cast<OctiDir>(c - 'A');
    }
    if (c >= 'a' && c <= 'h') {
        return static_cast<OctiDir>(c - 'a');
    }
    return DIR_UNDEF;
}

inline char dirToChar(OctiDir dir)
{
    if (dir >= DIR_N && dir <= DIR_NW) {
        return static_cast<char>('a' + static_cast<int>(dir));
    }
    return '?';
}

inline int dirDeltaX(OctiDir dir)
{
    switch (dir) {
    case DIR_N:  return 0;
    case DIR_NE: return 1;
    case DIR_E:  return 1;
    case DIR_SE: return 1;
    case DIR_S:  return 0;
    case DIR_SW: return -1;
    case DIR_W:  return -1;
    case DIR_NW: return -1;
    default:     return 0;
    }
}

inline int dirDeltaY(OctiDir dir)
{
    switch (dir) {
    case DIR_N:  return 1;
    case DIR_NE: return 1;
    case DIR_E:  return 0;
    case DIR_SE: return -1;
    case DIR_S:  return -1;
    case DIR_SW: return -1;
    case DIR_W:  return 0;
    case DIR_NW: return 1;
    default:     return 0;
    }
}

inline OctiDir deltaToDir(int dx, int dy)
{
    if (dx == 0 && dy == 1)   return DIR_N;
    if (dx == 1 && dy == 1)   return DIR_NE;
    if (dx == 1 && dy == 0)   return DIR_E;
    if (dx == 1 && dy == -1)  return DIR_SE;
    if (dx == 0 && dy == -1)  return DIR_S;
    if (dx == -1 && dy == -1) return DIR_SW;
    if (dx == -1 && dy == 0)  return DIR_W;
    if (dx == -1 && dy == 1)  return DIR_NW;
    return DIR_UNDEF;
}

inline OctiPodType playerToPodType(OctiPlayer p)
{
    switch (p) {
    case PLAYER_A: return A_POD;
    case PLAYER_B: return B_POD;
    case PLAYER_C: return C_POD;
    case PLAYER_D: return D_POD;
    default:       return POD_UNDEF;
    }
}

inline OctiPlayer podTypeToPlayer(OctiPodType t)
{
    switch (t) {
    case A_POD: return PLAYER_A;
    case B_POD: return PLAYER_B;
    case C_POD: return PLAYER_C;
    case D_POD: return PLAYER_D;
    default:    return PLAYER_UNDEF;
    }
}

inline bool areAlliedPlayers(OctiGameMode mode, OctiPlayer p1, OctiPlayer p2)
{
    if (p1 == PLAYER_UNDEF || p2 == PLAYER_UNDEF) {
        return false;
    }
    if (p1 == p2) {
        return true;
    }
    if (mode == GAME_MODE_4) {
        if ((p1 == PLAYER_A && p2 == PLAYER_C) || (p1 == PLAYER_C && p2 == PLAYER_A)) {
            return true;
        }
        if ((p1 == PLAYER_B && p2 == PLAYER_D) || (p1 == PLAYER_D && p2 == PLAYER_B)) {
            return true;
        }
    }
    return false;
}

inline bool areAlliedPodTypes(OctiGameMode mode, OctiPodType t1, OctiPodType t2)
{
    return areAlliedPlayers(mode, podTypeToPlayer(t1), podTypeToPlayer(t2));
}

#ifndef OCTI_VARIANT_CODE_HELPERS_DEFINED
#define OCTI_VARIANT_CODE_HELPERS_DEFINED

inline std::vector<std::string> allOctiVariantCodes()
{
    return {
        "2P_3B",    "2P_3B_SP", "2P_3B_ED", "2P_3B_SPED",
        "2P_1B",    "2P_1B_SP", "2P_1B_ED", "2P_1B_SPED",
        "4P",       "4P_SP",    "4P_ED",    "4P_SPED",
        "OFK"
    };
}

inline std::string octiVariantCode(OctiGameMode mode, int bases,
                                   bool superProngs, bool edgeless)
{
    if (mode == GAME_MODE_OFK) {
        return "OFK";
    }
    std::string code;
    if (mode == GAME_MODE_4) {
        code = "4P";
    } else {
        code = (bases == 1) ? "2P_1B" : "2P_3B";
    }
    if (superProngs && edgeless) {
        code += "_SPED";
    } else if (superProngs) {
        code += "_SP";
    } else if (edgeless) {
        code += "_ED";
    }
    return code;
}

inline std::string octiVariantCode(OctiGameMode mode, OctiGameBases bases,
                                   bool superProngs, bool edgeless)
{
    return octiVariantCode(mode, static_cast<int>(bases), superProngs, edgeless);
}

inline bool parseOctiVariantCode(const std::string &code,
                                 OctiGameMode &modeOut,
                                 int &basesOut,
                                 bool &superProngsOut,
                                 bool &edgelessOut)
{
    std::string u = code;
    for (size_t i = 0; i < u.size(); ++i) {
        if (u[i] >= 'a' && u[i] <= 'z') {
            u[i] = static_cast<char>(u[i] - 'a' + 'A');
        }
    }
    if (u == "OFK") {
        modeOut = GAME_MODE_OFK;
        basesOut = 1;
        superProngsOut = false;
        edgelessOut = false;
        return true;
    }
    if (u == "4P" || u == "4P_SP" || u == "4P_ED" || u == "4P_SPED") {
        modeOut = GAME_MODE_4;
        basesOut = 3;
        superProngsOut = (u == "4P_SP" || u == "4P_SPED");
        edgelessOut = (u == "4P_ED" || u == "4P_SPED");
        return true;
    }
    if (u == "2P_3B" || u == "2P_3B_SP" || u == "2P_3B_ED" || u == "2P_3B_SPED") {
        modeOut = GAME_MODE_2;
        basesOut = 3;
        superProngsOut = (u == "2P_3B_SP" || u == "2P_3B_SPED");
        edgelessOut = (u == "2P_3B_ED" || u == "2P_3B_SPED");
        return true;
    }
    if (u == "2P_1B" || u == "2P_1B_SP" || u == "2P_1B_ED" || u == "2P_1B_SPED") {
        modeOut = GAME_MODE_2;
        basesOut = 1;
        superProngsOut = (u == "2P_1B_SP" || u == "2P_1B_SPED");
        edgelessOut = (u == "2P_1B_ED" || u == "2P_1B_SPED");
        return true;
    }
    return false;
}

inline bool parseOctiVariantCode(const std::string &code,
                                 OctiGameMode &modeOut,
                                 OctiGameBases &basesOut,
                                 bool &superProngsOut,
                                 bool &edgelessOut)
{
    int b = 3;
    if (!parseOctiVariantCode(code, modeOut, b, superProngsOut, edgelessOut)) {
        return false;
    }
    basesOut = (b == 1) ? GAME_BASES_1 : GAME_BASES_3;
    return true;
}

inline bool isValidOctiVariantCode(const std::string &code)
{
    OctiGameMode m = GAME_MODE_2;
    int b = 3;
    bool sp = false;
    bool ed = false;
    return parseOctiVariantCode(code, m, b, sp, ed);
}

inline std::string octiVariantCodeFromModeStr(const std::string &modeStr,
                                              int bases,
                                              bool superProngs,
                                              bool edgeless)
{
    std::string u = modeStr;
    for (size_t i = 0; i < u.size(); ++i) {
        if (u[i] >= 'a' && u[i] <= 'z') {
            u[i] = static_cast<char>(u[i] - 'a' + 'A');
        }
    }
    if (u == "OFK") {
        return "OFK";
    }
    if (u == "4P" || u == "4") {
        return octiVariantCode(GAME_MODE_4, bases, superProngs, edgeless);
    }
    if (u == "2P" || u == "2" || u.empty()) {
        return octiVariantCode(GAME_MODE_2, bases, superProngs, edgeless);
    }
    OctiGameMode pm = GAME_MODE_2;
    int pb = 3;
    bool psp = false;
    bool ped = false;
    if (parseOctiVariantCode(u, pm, pb, psp, ped)) {
        return octiVariantCode(pm, pb, psp, ped);
    }
    return octiVariantCode(GAME_MODE_2, bases, superProngs, edgeless);
}

inline bool isOfficialRatedVariant(OctiGameMode mode, int bases,
                                   bool superProngs, bool edgeless)
{
    std::string v = octiVariantCode(mode, bases, superProngs, edgeless);
    return v == "2P_3B" || v == "2P_1B" || v == "OFK";
}

inline bool isOfficialRatedVariant(OctiGameMode mode, OctiGameBases bases,
                                   bool superProngs, bool edgeless)
{
    return isOfficialRatedVariant(mode, static_cast<int>(bases),
                                  superProngs, edgeless);
}

inline bool isOfficialRatedVariantCode(const std::string &code)
{
    OctiGameMode m = GAME_MODE_2;
    int b = 3;
    bool sp = false;
    bool ed = false;
    if (!parseOctiVariantCode(code, m, b, sp, ed)) {
        return false;
    }
    return isOfficialRatedVariant(m, b, sp, ed);
}

#endif // OCTI_VARIANT_CODE_HELPERS_DEFINED

} // namespace octi

#endif // OCTI_DEFS_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
