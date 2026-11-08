/*
 * OctiDefs.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_DEFS_HXX
#define OCTI_DEFS_HXX

#include "Version.hxx"

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
