/*
 * OctiCore.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiCore.hxx"
#include "Version.hxx"
#include <sstream>

namespace octi {

OctiCore::OctiCore()
{
}

OctiCore::~OctiCore()
{
}

const char *OctiCore::versionString()
{
    return OCTI_VERSION_STRING;
}

std::string OctiCore::buildBanner(const char *componentName)
{
    const char *name = (componentName != nullptr && componentName[0] != '\0')
        ? componentName : "octi";
    std::ostringstream oss;
    oss << name << " version " << OCTI_VERSION_STRING << "\n"
        << "Built: " << OCTI_WHOAMI << "@" << OCTI_HOSTNAME
        << " " << OCTI_DATE << "\n";
    return oss.str();
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
