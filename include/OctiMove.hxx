/*
 * OctiMove.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_MOVE_HXX
#define OCTI_MOVE_HXX

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>
#include "OctiDefs.hxx"
#include "OctiExceptions.hxx"
#include "OctiPod.hxx"

namespace octi {

struct OctiHop {
    int x = -1;
    int y = -1;
    bool capture = false;

    bool operator==(const OctiHop &other) const
    {
        return x == other.x && y == other.y && capture == other.capture;
    }

    bool operator!=(const OctiHop &other) const
    {
        return !(*this == other);
    }
};

struct OctiMoveOp {
    int x0 = -1;
    int y0 = -1;
    OctiPod pod;
    bool addPod = false;
    bool liberate = false;
    OctiDir addProngDir = DIR_UNDEF;
    OctiDir rmProngDir = DIR_UNDEF;
    bool superProng = false;
    bool isSuperProngBonusMove = false;
    std::vector<OctiHop> hops;

    bool operator==(const OctiMoveOp &other) const
    {
        return x0 == other.x0 &&
               y0 == other.y0 &&
               pod == other.pod &&
               addPod == other.addPod &&
               liberate == other.liberate &&
               addProngDir == other.addProngDir &&
               rmProngDir == other.rmProngDir &&
               superProng == other.superProng &&
               isSuperProngBonusMove == other.isSuperProngBonusMove &&
               hops == other.hops;
    }

    bool operator!=(const OctiMoveOp &other) const
    {
        return !(*this == other);
    }
};

class OctiMove {
public:
    OctiMove();
    explicit OctiMove(const std::string &str);
    explicit OctiMove(const char *str);
    explicit OctiMove(const std::vector<OctiMoveOp> &ops);

    const std::vector<OctiMoveOp> &getOps() const;
    std::size_t size() const;
    bool isEmpty() const;
    std::string asString() const;

    bool operator==(const OctiMove &other) const;
    bool operator!=(const OctiMove &other) const;

    friend std::ostream &operator<<(std::ostream &os, const OctiMove &move);

    static void parseMoveString(const std::string &str,
                                std::vector<OctiMoveOp> &outOps);
    static std::string formatOps(const std::vector<OctiMoveOp> &ops);

private:
    std::vector<OctiMoveOp> _ops;
    std::string _str;
};

} // namespace octi

#endif // OCTI_MOVE_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
