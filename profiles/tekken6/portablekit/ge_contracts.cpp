#include "clear_vertices.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>

namespace {
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class F> void rejects(F f, const char* why) {
    bool rejected = false;
    try { f(); } catch (const psprecomp::Error&) { rejected = true; }
    check(rejected, why);
}
}

int portablekit_decode_clear(const char* path) {
    if (std::filesystem::file_size(path) > 4096) throw psprecomp::Error("Clear evidence exceeds size budget");
    std::ifstream input(path);
    std::string magic; std::uint32_t count = 0;
    if (!(input >> magic >> count) || magic != "AZAZEL6_CLEAR_V1" || !count || count > 64 || (count & 1u))
        throw psprecomp::Error("Invalid clear evidence header");
    psprecomp::GuestMemory memory;
    constexpr auto base = 0x08004000u;
    for (std::uint32_t i = 0; i < count * 3; ++i) {
        std::uint64_t word;
        if (!(input >> word) || word > 0xFFFFFFFFu) throw psprecomp::Error("Invalid clear evidence word");
        memory.store32(base + i * 4, static_cast<std::uint32_t>(word));
    }
    std::string extra;
    if (input >> extra) throw psprecomp::Error("Trailing clear evidence");
    const auto vertices = azazel6::decode_clear_vertices(memory, base, count, base, base + count * 12);
    std::cout << "{\"scope\":\"DECODED_VERTEX_EVIDENCE_ONLY\",\"vertices\":[";
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const auto& v = vertices[i];
        if (i) std::cout << ',';
        std::cout << "{\"x\":" << v.position[0] << ",\"y\":" << v.position[1]
                  << ",\"z\":" << v.position[2] << ",\"color\":" << v.color << '}';
    }
    std::cout << "]}\n";
    return 0;
}

int portablekit_ge_contracts() {
    psprecomp::GuestMemory memory;
    constexpr auto base = 0x08004000u;
    // Independently authored fixture: fifteen adjacent clear rectangles.
    // Different nonzero colors/depths make packing and unsigned Z observable.
    for (std::uint32_t i = 0; i < 30; ++i) {
        const auto pair = i / 2, second = i & 1u, at = base + i * 12;
        memory.store32(at, 0x80402010u + pair);
        memory.store16(at + 4, static_cast<std::uint16_t>((pair + second) * 32));
        memory.store16(at + 6, static_cast<std::uint16_t>(second * 272));
        memory.store16(at + 8, static_cast<std::uint16_t>(0xFFF0u + pair));
        memory.store16(at + 10, 0xA55Au); // Padding must not become a component.
    }
    const auto vertices = azazel6::decode_clear_vertices(memory, base, 30, base, base + 360);
    for (std::uint32_t i = 0; i < vertices.size(); ++i) {
        const auto pair = i / 2, second = i & 1u; const auto& v = vertices[i];
        check(v.position[0] == (pair + second) * 32 && v.position[1] == second * 272 &&
              v.position[2] == 0xFFF0u + pair && v.position[3] == 1 && v.color == 0x80402010u + pair,
              "External decoder changed packed through-mode position/color/depth");
    }
    for (auto count : {0u, 1u, 29u, 66u, 0xFFFFFFFFu})
        rejects([&] { azazel6::decode_clear_vertices(memory, base, count, base, base + 1024); }, "Invalid count admitted");
    rejects([&] { azazel6::decode_clear_vertices(memory, base, 30, base, base + 356); }, "Truncated vertex lease admitted");
    rejects([&] { azazel6::decode_clear_vertices(memory, base + 1, 2, base, base + 360); }, "Unaligned input admitted");
    rejects([&] { azazel6::decode_clear_vertices(memory, base, 2, base + 4, base + 360); }, "Foreign vertex start admitted");
    rejects([&] { azazel6::decode_clear_vertices(memory, 0x88004000u, 2, 0x88004000u, 0x88004100u); }, "Aliased lease admitted");
    rejects([&] { azazel6::decode_clear_vertices(memory, 0xFFFFFFFCu, 2, base, base + 360); }, "Address overflow admitted");
    memory.store16(base + 4, 0xFFFF);
    rejects([&] { azazel6::decode_clear_vertices(memory, base, 2, base, base + 24); }, "Negative X admitted");
    memory.store16(base + 4, 0);
    memory.store16(base + 12 + 4, 0);
    rejects([&] { azazel6::decode_clear_vertices(memory, base, 2, base, base + 24); }, "Empty rectangle admitted");
    memory.store16(base + 12 + 4, 32);
    memory.store32(base + 12, 0);
    rejects([&] { azazel6::decode_clear_vertices(memory, base, 2, base, base + 24); }, "Varying color admitted");
    std::cout << "Synthetic PortableKit checked clear-vertex contracts passed; no rendering\n";
    return 0;
}
