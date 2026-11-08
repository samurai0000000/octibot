/*
 * OctiSquare.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiSquare.hxx"
#include <algorithm>
#include <stdexcept>

namespace octi {

OctiSquare::OctiSquare()
    : _mode(GAME_MODE_UNDEF)
    , _type(NEUTRAL_SQUARE)
    , _x(-1)
    , _y(-1)
{
}

OctiSquare::OctiSquare(OctiGameMode mode, OctiSquareType type, int x, int y)
    : _mode(mode)
    , _type(type)
    , _x(x)
    , _y(y)
{
}

void OctiSquare::configure(OctiGameMode mode, OctiSquareType type, int x, int y)
{
    _mode = mode;
    _type = type;
    _x = x;
    _y = y;
}

void OctiSquare::clear()
{
    _pods.clear();
}

OctiGameMode OctiSquare::getGameMode() const
{
    return _mode;
}

OctiSquareType OctiSquare::getType() const
{
    return _type;
}

int OctiSquare::getLocId() const
{
    return (_x + 1) * 10 + (_y + 1);
}

void OctiSquare::getLocXY(int &x, int &y) const
{
    x = _x;
    y = _y;
}

int OctiSquare::getLocX() const
{
    return _x;
}

int OctiSquare::getLocY() const
{
    return _y;
}

int OctiSquare::getNumPods() const
{
    return static_cast<int>(_pods.size());
}

const OctiPod &OctiSquare::getPodAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(_pods.size())) {
        throw std::out_of_range("OctiSquare::getPodAt: index out of range");
    }
    return _pods[static_cast<std::size_t>(index)];
}

OctiPod &OctiSquare::podAt(int index)
{
    if (index < 0 || index >= static_cast<int>(_pods.size())) {
        throw std::out_of_range("OctiSquare::podAt: index out of range");
    }
    return _pods[static_cast<std::size_t>(index)];
}

const OctiPod &OctiSquare::podAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(_pods.size())) {
        throw std::out_of_range("OctiSquare::podAt: index out of range");
    }
    return _pods[static_cast<std::size_t>(index)];
}

const std::vector<OctiPod> &OctiSquare::pods() const
{
    return _pods;
}

void OctiSquare::addPod(const OctiPod &pod)
{
    _pods.push_back(pod);
}

bool OctiSquare::removePod(const OctiPod &pod)
{
    for (int i = static_cast<int>(_pods.size()) - 1; i >= 0; --i) {
        if (_pods[static_cast<std::size_t>(i)] == pod) {
            _pods.erase(_pods.begin() + i);
            return true;
        }
    }
    return false;
}

bool OctiSquare::containsPod(const OctiPod &pod) const
{
    for (const auto &p : _pods) {
        if (p == pod) {
            return true;
        }
    }
    return false;
}

bool OctiSquare::isEmpty() const
{
    return _pods.empty();
}

bool OctiSquare::isEmptyWithout(const std::vector<OctiPod> &ghostPods) const
{
    int count = static_cast<int>(_pods.size());
    std::vector<bool> matched(ghostPods.size(), false);

    for (const auto &p : _pods) {
        for (std::size_t j = 0; j < ghostPods.size(); ++j) {
            if (!matched[j] && ghostPods[j] == p) {
                matched[j] = true;
                count--;
                break;
            }
        }
    }

    return count <= 0;
}

bool OctiSquare::isCaptured() const
{
    if (_pods.empty()) {
        return false;
    }

    OctiPlayer baseOwner = PLAYER_UNDEF;
    switch (_type) {
    case A_HOME_SQUARE: baseOwner = PLAYER_A; break;
    case B_HOME_SQUARE: baseOwner = PLAYER_B; break;
    case C_HOME_SQUARE: baseOwner = PLAYER_C; break;
    case D_HOME_SQUARE: baseOwner = PLAYER_D; break;
    default:            return false;
    }

    for (const auto &p : _pods) {
        OctiPlayer podPlayer = podTypeToPlayer(p.getType());
        if (podPlayer != PLAYER_UNDEF &&
            !areAlliedPlayers(_mode, baseOwner, podPlayer)) {
            return true;
        }
    }

    return false;
}

bool OctiSquare::isCapturedBy(OctiPlayer player) const
{
    if (!isCaptured()) {
        return false;
    }

    OctiPlayer baseOwner = PLAYER_UNDEF;
    switch (_type) {
    case A_HOME_SQUARE: baseOwner = PLAYER_A; break;
    case B_HOME_SQUARE: baseOwner = PLAYER_B; break;
    case C_HOME_SQUARE: baseOwner = PLAYER_C; break;
    case D_HOME_SQUARE: baseOwner = PLAYER_D; break;
    default:            return false;
    }

    if (areAlliedPlayers(_mode, baseOwner, player)) {
        return false;
    }

    return isOccupiedBy(player);
}

bool OctiSquare::isCapturedByTeamOf(OctiPlayer player) const
{
    if (!isCaptured()) {
        return false;
    }

    OctiPlayer baseOwner = PLAYER_UNDEF;
    switch (_type) {
    case A_HOME_SQUARE: baseOwner = PLAYER_A; break;
    case B_HOME_SQUARE: baseOwner = PLAYER_B; break;
    case C_HOME_SQUARE: baseOwner = PLAYER_C; break;
    case D_HOME_SQUARE: baseOwner = PLAYER_D; break;
    default:            return false;
    }

    if (areAlliedPlayers(_mode, baseOwner, player)) {
        return false;
    }

    for (const auto &p : _pods) {
        OctiPlayer podPlayer = podTypeToPlayer(p.getType());
        if (podPlayer != PLAYER_UNDEF &&
            areAlliedPlayers(_mode, player, podPlayer)) {
            return true;
        }
    }

    return false;
}

bool OctiSquare::isOccupiedBy(OctiPlayer player) const
{
    if (player == PLAYER_UNDEF) {
        return false;
    }

    for (const auto &p : _pods) {
        if (podTypeToPlayer(p.getType()) == player) {
            return true;
        }
    }

    return false;
}

bool OctiSquare::hasEnemyOf(OctiPlayer player) const
{
    if (player == PLAYER_UNDEF) {
        return false;
    }

    for (const auto &p : _pods) {
        OctiPlayer podPlayer = podTypeToPlayer(p.getType());
        if (podPlayer != PLAYER_UNDEF &&
            !areAlliedPlayers(_mode, player, podPlayer)) {
            return true;
        }
    }

    return false;
}

bool OctiSquare::isEnemyOf(OctiPlayer player) const
{
    if (player == PLAYER_UNDEF) {
        return false;
    }

    OctiPlayer baseOwner = PLAYER_UNDEF;
    switch (_type) {
    case A_HOME_SQUARE: baseOwner = PLAYER_A; break;
    case B_HOME_SQUARE: baseOwner = PLAYER_B; break;
    case C_HOME_SQUARE: baseOwner = PLAYER_C; break;
    case D_HOME_SQUARE: baseOwner = PLAYER_D; break;
    default:            return false;
    }

    return !areAlliedPlayers(_mode, player, baseOwner);
}

bool OctiSquare::hasSameMaterialsAs(const OctiSquare &other) const
{
    if (_pods.size() != other._pods.size()) {
        return false;
    }

    std::vector<OctiPod> s1 = _pods;
    std::vector<OctiPod> s2 = other._pods;
    std::sort(s1.begin(), s1.end());
    std::sort(s2.begin(), s2.end());

    return s1 == s2;
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
