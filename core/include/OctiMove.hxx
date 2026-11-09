/*
 * OctiMove.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_MOVE_HXX
#define OCTI_MOVE_HXX

#include <cstddef>
#include <cstdint>
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
    explicit OctiMove(std::vector<OctiMoveOp> &&ops);

    static OctiMove fromValidatedSingleOp(OctiMoveOp &&op);
    static OctiMove fromValidatedOps(std::vector<OctiMoveOp> &&ops);

    const std::vector<OctiMoveOp> &getOps() const
    {
        return _ops;
    }

    std::size_t size() const
    {
        return _ops.size();
    }

    bool isEmpty() const
    {
        return _ops.empty();
    }

    std::uint64_t structuralKey() const
    {
        return _key;
    }

    std::string asString() const;

    bool operator==(const OctiMove &other) const;
    bool operator!=(const OctiMove &other) const;

    friend std::ostream &operator<<(std::ostream &os, const OctiMove &move);

    static void parseMoveString(const std::string &str,
                                std::vector<OctiMoveOp> &outOps);
    static std::string formatOps(const std::vector<OctiMoveOp> &ops);
    static std::uint64_t computeStructuralKey(const OctiMoveOp &op);
    static std::uint64_t computeStructuralKey(const std::vector<OctiMoveOp> &ops);

private:
    struct ValidatedTag {};
    OctiMove(ValidatedTag, OctiMoveOp &&op);
    OctiMove(ValidatedTag, std::vector<OctiMoveOp> &&ops);
    void validateOpsOrThrow() const;

    std::vector<OctiMoveOp> _ops;
    std::uint64_t _key = 0;
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
