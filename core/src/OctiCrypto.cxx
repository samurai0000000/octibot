/*
 * OctiCrypto.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiCrypto.hxx"
#include <cstring>
#include <iomanip>
#include <sstream>

namespace octi {

namespace {

inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n)
{
    return (x >> n) | (x << (32 - n));
}

inline std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
    return (x & y) ^ ((~x) & z);
}

inline std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

inline std::uint32_t sigma0(std::uint32_t x)
{
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline std::uint32_t sigma1(std::uint32_t x)
{
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline std::uint32_t gamma0(std::uint32_t x)
{
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline std::uint32_t gamma1(std::uint32_t x)
{
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

const std::uint32_t K[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

struct Sha256Context {
    std::uint32_t state[8];
    std::uint64_t totalBits;
    std::uint8_t buffer[64];
    std::size_t bufferLen;

    Sha256Context()
        : totalBits(0)
        , bufferLen(0)
    {
        state[0] = 0x6a09e667U;
        state[1] = 0xbb67ae85U;
        state[2] = 0x3c6ef372U;
        state[3] = 0xa54ff53aU;
        state[4] = 0x510e527fU;
        state[5] = 0x9b05688cU;
        state[6] = 0x1f83d9abU;
        state[7] = 0x5be0cd19U;
        std::memset(buffer, 0, sizeof(buffer));
    }

    void transform(const std::uint8_t block[64])
    {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                   (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                   (static_cast<std::uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
        }

        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];
        std::uint32_t f = state[5];
        std::uint32_t g = state[6];
        std::uint32_t h = state[7];

        for (int i = 0; i < 64; ++i) {
            std::uint32_t t1 = h + sigma1(e) + ch(e, f, g) + K[i] + w[i];
            std::uint32_t t2 = sigma0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    void update(const std::uint8_t *input, std::size_t len)
    {
        totalBits += static_cast<std::uint64_t>(len) * 8ULL;
        while (len > 0) {
            std::size_t toCopy = std::min(len, 64 - bufferLen);
            std::memcpy(buffer + bufferLen, input, toCopy);
            bufferLen += toCopy;
            input += toCopy;
            len -= toCopy;

            if (bufferLen == 64) {
                transform(buffer);
                bufferLen = 0;
            }
        }
    }

    void finalize(std::uint8_t digest[32])
    {
        buffer[bufferLen++] = 0x80;
        if (bufferLen > 56) {
            std::memset(buffer + bufferLen, 0, 64 - bufferLen);
            transform(buffer);
            bufferLen = 0;
        }
        std::memset(buffer + bufferLen, 0, 56 - bufferLen);

        for (int i = 7; i >= 0; --i) {
            buffer[56 + (7 - i)] = static_cast<std::uint8_t>((totalBits >> (i * 8)) & 0xffULL);
        }
        transform(buffer);

        for (int i = 0; i < 8; ++i) {
            digest[i * 4] = static_cast<std::uint8_t>((state[i] >> 24) & 0xffU);
            digest[i * 4 + 1] = static_cast<std::uint8_t>((state[i] >> 16) & 0xffU);
            digest[i * 4 + 2] = static_cast<std::uint8_t>((state[i] >> 8) & 0xffU);
            digest[i * 4 + 3] = static_cast<std::uint8_t>(state[i] & 0xffU);
        }
    }
};

} // namespace

std::array<std::uint8_t, 32> sha256Bytes(const void *data, std::size_t len)
{
    Sha256Context ctx;
    ctx.update(static_cast<const std::uint8_t *>(data), len);
    std::array<std::uint8_t, 32> out{};
    ctx.finalize(out.data());
    return out;
}

std::string sha256Hex(const void *data, std::size_t len)
{
    std::array<std::uint8_t, 32> bytes = sha256Bytes(data, len);
    static const char hexDigits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(64);
    for (std::uint8_t b : bytes) {
        hex.push_back(hexDigits[(b >> 4) & 0x0f]);
        hex.push_back(hexDigits[b & 0x0f]);
    }
    return hex;
}

std::string sha256Hex(const std::string &data)
{
    return sha256Hex(data.data(), data.size());
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
