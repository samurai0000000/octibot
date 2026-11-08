/*
 * OctiSquare.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_SQUARE_HXX
#define OCTI_SQUARE_HXX

#include <vector>
#include "OctiDefs.hxx"
#include "OctiPod.hxx"

namespace octi {

class OctiSquare {
public:
    OctiSquare();
    OctiSquare(OctiGameMode mode, OctiSquareType type, int x, int y);

    void configure(OctiGameMode mode, OctiSquareType type, int x, int y);
    void clear();

    OctiGameMode getGameMode() const;
    OctiSquareType getType() const;
    int getLocId() const;
    void getLocXY(int &x, int &y) const;
    int getLocX() const;
    int getLocY() const;

    int getNumPods() const;
    const OctiPod &getPodAt(int index) const;
    OctiPod &podAt(int index);
    const OctiPod &podAt(int index) const;
    const std::vector<OctiPod> &pods() const;

    void addPod(const OctiPod &pod);
    bool removePod(const OctiPod &pod);
    bool containsPod(const OctiPod &pod) const;

    bool isEmpty() const;
    bool isEmptyWithout(const std::vector<OctiPod> &ghostPods) const;

    bool isCaptured() const;
    bool isCapturedBy(OctiPlayer player) const;
    bool isCapturedByTeamOf(OctiPlayer player) const;
    bool isOccupiedBy(OctiPlayer player) const;
    bool hasEnemyOf(OctiPlayer player) const;
    bool isEnemyOf(OctiPlayer player) const;
    bool hasSameMaterialsAs(const OctiSquare &other) const;

private:
    OctiGameMode _mode;
    OctiSquareType _type;
    int _x;
    int _y;
    std::vector<OctiPod> _pods;
};

} // namespace octi

#endif // OCTI_SQUARE_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
