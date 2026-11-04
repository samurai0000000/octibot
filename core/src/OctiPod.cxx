/*
 * OctiPod.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiPod.hxx"
#include <ostream>

namespace octi {

const OctiPod OctiPod::EMPTY_POD;

static constexpr std::uint32_t OWNER_MASK = 0x00070000u;
static constexpr unsigned int OWNER_SHIFT = 16u;
static constexpr std::uint32_t PRONGS_MASK = 0x0000FFFFu;

OctiPod::OctiPod()
    : _conf(0)
{
}

OctiPod::OctiPod(OctiPodType type)
    : _conf(0)
{
    setType(type);
}

OctiPod::OctiPod(std::uint32_t rawConf)
    : _conf(rawConf)
{
}

void OctiPod::clear()
{
    _conf = 0;
}

OctiPodType OctiPod::getType() const
{
    std::uint32_t val = (_conf & OWNER_MASK) >> OWNER_SHIFT;
    return static_cast<OctiPodType>(val);
}

void OctiPod::setType(OctiPodType type)
{
    std::uint32_t val = static_cast<std::uint32_t>(type) & 0x7u;
    _conf = (_conf & ~OWNER_MASK) | (val << OWNER_SHIFT);
}

OctiProngType OctiPod::prongAt(OctiDir dir) const
{
    if (dir < DIR_N || dir > DIR_NW) {
        return NO_PRONG;
    }

    unsigned int shift = static_cast<unsigned int>(dir) * 2u;
    std::uint32_t val = (_conf >> shift) & 0x3u;
    return static_cast<OctiProngType>(val);
}

void OctiPod::addProng(OctiDir dir, bool super)
{
    if (dir < DIR_N || dir > DIR_NW) {
        return;
    }

    unsigned int shift = static_cast<unsigned int>(dir) * 2u;
    std::uint32_t mask = 0x3u << shift;
    std::uint32_t pval = super ? static_cast<std::uint32_t>(SUPER_PRONG)
                               : static_cast<std::uint32_t>(NORMAL_PRONG);
    _conf = (_conf & ~mask) | ((pval & 0x3u) << shift);
}

void OctiPod::removeProng(OctiDir dir)
{
    if (dir < DIR_N || dir > DIR_NW) {
        return;
    }

    unsigned int shift = static_cast<unsigned int>(dir) * 2u;
    std::uint32_t mask = 0x3u << shift;
    _conf &= ~mask;
}

bool OctiPod::hasSuperProng() const
{
    for (int d = DIR_N; d <= DIR_NW; ++d) {
        if (prongAt(static_cast<OctiDir>(d)) == SUPER_PRONG) {
            return true;
        }
    }
    return false;
}

int OctiPod::getNumProngs() const
{
    int count = 0;
    for (int d = DIR_N; d <= DIR_NW; ++d) {
        if (prongAt(static_cast<OctiDir>(d)) != NO_PRONG) {
            count++;
        }
    }
    return count;
}

int OctiPod::getNumNormalProngs() const
{
    int count = 0;
    for (int d = DIR_N; d <= DIR_NW; ++d) {
        if (prongAt(static_cast<OctiDir>(d)) == NORMAL_PRONG) {
            count++;
        }
    }
    return count;
}

int OctiPod::getNumSuperProngs() const
{
    int count = 0;
    for (int d = DIR_N; d <= DIR_NW; ++d) {
        if (prongAt(static_cast<OctiDir>(d)) == SUPER_PRONG) {
            count++;
        }
    }
    return count;
}

std::string OctiPod::getConfigString() const
{
    std::string s;
    for (int d = DIR_N; d <= DIR_NW; ++d) {
        OctiDir dir = static_cast<OctiDir>(d);
        OctiProngType pt = prongAt(dir);
        if (pt == NORMAL_PRONG) {
            s.push_back(dirToChar(dir));
        } else if (pt == SUPER_PRONG) {
            s.push_back(dirToChar(dir));
            s.push_back('!');
        }
    }
    return s;
}

std::uint32_t OctiPod::rawConfig() const
{
    return _conf;
}

bool OctiPod::operator==(const OctiPod &other) const
{
    return _conf == other._conf;
}

bool OctiPod::operator!=(const OctiPod &other) const
{
    return _conf != other._conf;
}

bool OctiPod::operator<(const OctiPod &other) const
{
    int myProngs = getNumProngs();
    int otherProngs = other.getNumProngs();
    if (myProngs != otherProngs) {
        return myProngs < otherProngs;
    }

    int mySuper = getNumSuperProngs();
    int otherSuper = other.getNumSuperProngs();
    if (mySuper != otherSuper) {
        return mySuper < otherSuper;
    }

    std::uint32_t myP = _conf & PRONGS_MASK;
    std::uint32_t otherP = other._conf & PRONGS_MASK;
    if (myP != otherP) {
        return myP < otherP;
    }

    return (_conf >> OWNER_SHIFT) < (other._conf >> OWNER_SHIFT);
}

bool OctiPod::operator>(const OctiPod &other) const
{
    return other < *this;
}

bool OctiPod::operator<=(const OctiPod &other) const
{
    return !(other < *this);
}

bool OctiPod::operator>=(const OctiPod &other) const
{
    return !(*this < other);
}

std::ostream &operator<<(std::ostream &os, const OctiPod &pod)
{
    char ownerChar = '?';
    switch (pod.getType()) {
    case A_POD: ownerChar = 'A'; break;
    case B_POD: ownerChar = 'B'; break;
    case C_POD: ownerChar = 'C'; break;
    case D_POD: ownerChar = 'D'; break;
    default:    ownerChar = '?'; break;
    }

    os << ownerChar << pod.getConfigString();
    return os;
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
