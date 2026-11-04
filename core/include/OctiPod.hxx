/*
 * OctiPod.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_POD_HXX
#define OCTI_POD_HXX

#include <cstdint>
#include <iosfwd>
#include <string>
#include "OctiDefs.hxx"

namespace octi {

class OctiPod {
public:
    static const OctiPod EMPTY_POD;

    OctiPod();
    explicit OctiPod(OctiPodType type);
    explicit OctiPod(std::uint32_t rawConf);

    void clear();

    OctiPodType getType() const;
    void setType(OctiPodType type);

    OctiProngType prongAt(OctiDir dir) const;
    void addProng(OctiDir dir, bool super = false);
    void removeProng(OctiDir dir);

    bool hasSuperProng() const;
    int getNumProngs() const;
    int getNumNormalProngs() const;
    int getNumSuperProngs() const;

    std::string getConfigString() const;
    std::uint32_t rawConfig() const;

    bool operator==(const OctiPod &other) const;
    bool operator!=(const OctiPod &other) const;
    bool operator<(const OctiPod &other) const;
    bool operator>(const OctiPod &other) const;
    bool operator<=(const OctiPod &other) const;
    bool operator>=(const OctiPod &other) const;

    friend std::ostream &operator<<(std::ostream &os, const OctiPod &pod);

private:
    std::uint32_t _conf;
};

} // namespace octi

#endif // OCTI_POD_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
