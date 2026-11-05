/*
 * OctiOpeningBookData.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_OPENING_BOOK_DATA_HXX
#define OCTI_OPENING_BOOK_DATA_HXX

#include <cstddef>
#include <cstdint>
#include "OctiBotEngine.hxx"

namespace octi {

static constexpr OpeningBookEntry g_openingBookEntries[] = {
    // 2P 1-Base ply 1 (after 33+b / 73+h)
    { 0x0B45FE3410182A4DULL, 8, 3, { { "57+e", 12 }, { "77+f", 8 }, { "37+d", 8 } } },
    // 2P 3-Base ply 1 (after 33+b / 73+h)
    { 0x1636F05EC19C04C8ULL, 8, 3, { { "57+e", 12 }, { "77+f", 8 }, { "37+d", 8 } } },
    // 2P 1-Base ply 0 (A to move)
    { 0x26BE80163B6578BBULL, 8, 3, { { "53+a", 55 }, { "33+b", 48 }, { "73+h", 48 } } },
    // 2P 3-Base ply 0 (A to move)
    { 0x3BCD8E7CEAE1563EULL, 8, 3, { { "53+a", 45 }, { "33+b", 40 }, { "73+h", 40 } } },
    // OFK ply 1 (after 22+b / 52+h)
    { 0x3E7C56E08875ECA9ULL, 8, 3, { { "46+e", 12 }, { "36+e", 12 }, { "56+f", 8 } } },
    // 2P 3-Base ply 1 (after 53+b / 53+h)
    { 0x42F7FBED36E9148FULL, 8, 3, { { "57+e", 12 }, { "37+d", 8 }, { "77+f", 8 } } },
    // 4P ply 0 (A to move)
    { 0x5C3D5244B85BC3A6ULL, 8, 3, { { "33+a", 40 }, { "33+b", 35 }, { "33+c", 25 } } },
    // 2P 3-Base ply 1 (after 33+a / 73+a)
    { 0x62F499414419B482ULL, 8, 3, { { "57+e", 15 }, { "77+e", 10 }, { "37+d", 8 } } },
    // OFK ply 1 (after 32+a / 42+a)
    { 0x97220033605ECA2DULL, 8, 3, { { "46+e", 10 }, { "36+e", 10 }, { "56+f", 5 } } },
    // OFK ply 0 (A to move)
    { 0xBF20205531084330ULL, 8, 3, { { "32+a", 50 }, { "42+a", 50 }, { "22+b", 42 } } },
    // 2P 3-Base ply 1 (after 53+a)
    { 0xE61535559B2C9569ULL, 8, 3, { { "57+e", 10 }, { "37+d", 5 }, { "77+f", 5 } } },
    // 2P 1-Base ply 1 (after 53+a)
    { 0xFB663B3F4AA8BBECULL, 8, 3, { { "57+e", 10 }, { "37+d", 5 }, { "77+f", 5 } } }
};

static constexpr std::size_t g_openingBookEntryCount =
    sizeof(g_openingBookEntries) / sizeof(g_openingBookEntries[0]);

} // namespace octi

#endif // OCTI_OPENING_BOOK_DATA_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
