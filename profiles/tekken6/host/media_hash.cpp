// SHA-256 compression adapted from pinned Yakumo src/sha256.cpp (MIT).
// Copyright (c) 2026 PSPRecomp contributors; Copyright (c) 2026 Yakumo contributors.
// See THIRD_PARTY_LICENSE_YAKUMO.txt. The external checkout remains unchanged.
#include "azazel6/media.hpp"
#include "psprecomp/common.hpp"
#include <array>
#include <algorithm>
#include <iomanip>
#include <sstream>
namespace azazel6 {
namespace {
constexpr std::array<std::uint32_t, 64> K{0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u,
    0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u, 0x2748774cu,
    0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
constexpr std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
    return (x >> n) | (x << (32u - n));
}
void transform(std::array<std::uint32_t, 8> &h, const std::uint8_t *block) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i)
        w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24u) |
            (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16u) |
            (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8u) | block[i * 4 + 3];
    for (std::size_t i = 16; i < 64; ++i) {
        const auto s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3u);
        const auto s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10u);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    auto a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (std::size_t i = 0; i < 64; ++i) {
        const auto s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const auto ch = (e & f) ^ ((~e) & g);
        const auto t1 = hh + s1 + ch + K[i] + w[i];
        const auto s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const auto maj = (a & b) ^ (a & c) ^ (b & c);
        const auto t2 = s0 + maj;
        hh = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
}
}
std::string media_sha256(std::istream& input) {
    input.clear(); input.seekg(0);
    std::array<std::uint32_t, 8> h{
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    std::array<std::uint8_t, 65536> buffer{};
    std::uint64_t total = 0;
    std::size_t tail = 0;
    for (;;) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const auto count = static_cast<std::size_t>(input.gcount());
        if (input.bad() || (!input.eof() && input.fail()))
            throw psprecomp::Error("Cannot hash media stream");
        total += count;
        if (total > 0x80000000ull) throw psprecomp::Error("Media exceeds bounded 2 GiB limit");
        for (std::size_t i = 0; i + 64 <= count; i += 64) transform(h, buffer.data() + i);
        if (count < buffer.size()) {
            tail = count % 64;
            std::copy_n(buffer.data() + count - tail, tail, buffer.data());
            break;
        }
    }
    buffer[tail++] = 0x80;
    const std::size_t end = tail <= 56 ? 64 : 128;
    std::fill(buffer.begin() + tail, buffer.begin() + end, 0);
    for (unsigned i = 0; i < 8; ++i) buffer[end - 1 - i] = static_cast<std::uint8_t>((total * 8) >> (i * 8));
    transform(h, buffer.data());
    if (end == 128) transform(h, buffer.data() + 64);
    std::ostringstream output; output << std::hex << std::setfill('0');
    for (auto word : h) output << std::setw(8) << word;
    input.clear(); input.seekg(0);
    if (!input) throw psprecomp::Error("Media stream cannot rewind");
    return output.str();
}
} // namespace azazel6
