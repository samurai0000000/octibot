/*
 * OctiRulebook.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_RULEBOOK_HXX
#define OCTI_RULEBOOK_HXX

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace octi {

constexpr std::size_t OCTI_RULEBOOK_IMAGE_COUNT = 12u;
constexpr std::size_t OCTI_RULEBOOK_CHAPTER_COUNT = 7u;

struct RulebookSection {
    const char *id;
    const char *number;
    const char *title;
    const char *summary;
};

std::vector<std::string> rulebookImageFileNames();

const std::uint8_t *rulebookImageBytes(const std::string &fileName,
                                       std::size_t &outSize);

const std::vector<std::string> &rulebookSupportedLanguages();

std::string normalizeRulebookLanguage(const std::string &lang);

const std::vector<RulebookSection> &rulebookSections();

const std::vector<RulebookSection> &rulebookSections(const std::string &lang);

std::string buildDesktopRulebookHtml(bool lightSurface = false);

std::string buildDesktopRulebookHtml(bool lightSurface,
                                     const std::string &lang);

} // namespace octi

#endif // OCTI_RULEBOOK_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
