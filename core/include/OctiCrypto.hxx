/*
 * OctiCrypto.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_CRYPTO_HXX
#define OCTI_CRYPTO_HXX

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace octi {

std::array<std::uint8_t, 32> sha256Bytes(const void *data, std::size_t len);
std::string sha256Hex(const void *data, std::size_t len);
std::string sha256Hex(const std::string &data);

struct OctiCrypto {
    static inline std::array<std::uint8_t, 32> sha256Bytes(const void *data, std::size_t len)
    {
        return octi::sha256Bytes(data, len);
    }

    static inline std::string sha256Hex(const void *data, std::size_t len)
    {
        return octi::sha256Hex(data, len);
    }

    static inline std::string sha256Hex(const std::string &data)
    {
        return octi::sha256Hex(data);
    }
};

} // namespace octi

#endif // OCTI_CRYPTO_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
