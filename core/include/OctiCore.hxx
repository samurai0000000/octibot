/*
 * OctiCore.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_CORE_OCTI_CORE_HXX
#define OCTI_CORE_OCTI_CORE_HXX

#include <string>

namespace octi {

class OctiCore {
public:
    OctiCore();
    ~OctiCore();

    static const char *versionString();
    static std::string buildBanner(const char *componentName);
};

} // namespace octi

#endif // OCTI_CORE_OCTI_CORE_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
