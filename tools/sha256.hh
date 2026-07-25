// -*- mode: c++; -*-
#pragma once

// Hand-rolled SHA-256 (FIPS 180-4), shared by the migration tools' manifest
// writers (pof2glb, vpstage). The PCX bargain again: a frozen algorithm is
// not worth a dependency, and the manifest gates recompute every digest with
// python's hashlib -- an independent implementation -- so a mistake here
// cannot survive.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace sha {

constexpr std::uint32_t sha_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

inline std::uint32_t
rotr(std::uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

inline void
sha_block(std::uint32_t h[8], const std::uint8_t *p)
{
    std::uint32_t w[64];

    for (int i = 0; i < 16; ++i)
        w[i] = std::uint32_t(p[4 * i]) << 24 |
            std::uint32_t(p[4 * i + 1]) << 16 |
            std::uint32_t(p[4 * i + 2]) << 8 | p[4 * i + 3];

    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 =
            rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 =
            rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

    for (int i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = hh + s1 + ch + sha_k[i] + w[i];
        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);

        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + s0 + maj;
    }

    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

// hex SHA-256 of a whole buffer (every file here is small enough to slurp)
inline std::string
sha256_hex(const std::uint8_t *p, std::size_t n)
{
    std::uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                           0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

    std::size_t off = 0;
    for (; off + 64 <= n; off += 64)
        sha_block(h, p + off);

    // pad: 0x80, zeros, the message bit length big-endian in the last 8
    std::uint8_t tail[128] = { };
    const std::size_t rest = n - off;
    std::memcpy(tail, p + off, rest);
    tail[rest] = 0x80;
    const std::size_t blocks = rest + 9 <= 64 ? 1 : 2;
    const std::uint64_t bits = std::uint64_t(n) * 8;
    for (int i = 0; i < 8; ++i)
        tail[blocks * 64 - 1 - i] = std::uint8_t(bits >> (8 * i));
    for (std::size_t i = 0; i < blocks; ++i)
        sha_block(h, tail + 64 * i);

    char hex[65];
    for (int i = 0; i < 8; ++i)
        snprintf(hex + 8 * i, 9, "%08x", h[i]);
    return hex;
}

// hex digest of a file's bytes; empty on read failure
inline std::string
sha256_file(const std::string &path)
{
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return "";

    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    std::vector< std::uint8_t > buf(sz > 0 ? sz : 0);
    const bool ok = sz >= 0 &&
        (buf.empty() ||
         std::fread(buf.data(), 1, buf.size(), f) == buf.size());
    std::fclose(f);

    return ok ? sha256_hex(buf.data(), buf.size()) : "";
}

} // namespace sha
