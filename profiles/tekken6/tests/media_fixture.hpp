#pragma once
// Independently authored ISO/SFO fixture; contains no game bytes.
#include "azazel6/media.hpp"
#include "psprecomp/sha256.hpp"
#include <chrono>
#include <functional>

namespace media_fixture {
inline void put(std::vector<std::uint8_t>& data, std::size_t p, std::uint32_t value, unsigned n = 4) {
    for (unsigned i = 0; i < n; ++i) data.at(p + i) = static_cast<std::uint8_t>(value >> (i * 8));
}
inline void dual(std::vector<std::uint8_t>& data, std::size_t p, std::uint32_t value, unsigned n = 4) {
    put(data, p, value, n);
    for (unsigned i = 0; i < n; ++i) data.at(p + n + i) = static_cast<std::uint8_t>(value >> ((n - i - 1) * 8));
}
inline void text(std::vector<std::uint8_t>& data, std::size_t p, const std::string& value) {
    std::copy(value.begin(), value.end(), data.begin() + p);
}
inline unsigned record(std::vector<std::uint8_t>& data, std::size_t p, const std::string& name,
                       unsigned sector, unsigned size, bool directory = false) {
    const unsigned length = (33 + static_cast<unsigned>(name.size()) + 1) & ~1u;
    data.at(p) = static_cast<std::uint8_t>(length);
    dual(data, p + 2, sector); dual(data, p + 10, size);
    data.at(p + 25) = directory ? 2 : 0; dual(data, p + 28, 1, 2);
    data.at(p + 32) = static_cast<std::uint8_t>(name.size()); text(data, p + 33, name);
    return length;
}
inline std::vector<std::uint8_t> image() {
    std::vector<std::uint8_t> data(40 * 2048);
    const auto p = 16 * 2048;
    data[p] = 1; text(data, p + 1, "CD001"); data[p + 6] = 1; data[p + 881] = 1;
    dual(data, p + 80, 40); dual(data, p + 120, 1, 2); dual(data, p + 124, 1, 2); dual(data, p + 128, 2048, 2);
    record(data, p + 156, std::string(1, '\0'), 20, 2048, true);
    data[17 * 2048] = 255; text(data, 17 * 2048 + 1, "CD001"); data[17 * 2048 + 6] = 1;
    record(data, 20 * 2048, "PSP_GAME", 21, 2048, true);
    auto pos = 21 * 2048;
    pos += record(data, pos, "SYSDIR", 22, 2048, true);
    pos += record(data, pos, "USRDIR", 23, 2048, true);
    record(data, pos, "PARAM.SFO;1", 24, 512);
    record(data, 22 * 2048, "EBOOT.BIN;1", 25, 4);
    record(data, 23 * 2048, "DATA.BIN;1", 26, 4);
    // 2000-03-01 00:15:22 at UTC+01:00, crossing leap day in UTC.
    const std::array<std::uint8_t, 7> stamp{100, 3, 1, 0, 15, 22, 4};
    std::copy(stamp.begin(), stamp.end(), data.begin() + 23 * 2048 + 18);
    text(data, 25 * 2048, "fake"); text(data, 26 * 2048, "test");
    const auto s = 24 * 2048;
    put(data, s, 0x46535000); put(data, s + 4, 0x101); put(data, s + 8, 84); put(data, s + 12, 160); put(data, s + 16, 4);
    unsigned key = 0, value = 0, i = 0;
    for (const auto& [name, contents] : std::vector<std::pair<std::string, std::string>>{
            {"DISC_ID", "ULUS10466"}, {"DISC_VERSION", "1.00"}, {"TITLE", "TEKKEN 6"}, {"CATEGORY", "UG"}}) {
        const auto entry = s + 20 + i++ * 16;
        put(data, entry, key, 2); put(data, entry + 2, 0x204, 2);
        put(data, entry + 4, static_cast<unsigned>(contents.size()) + 1); put(data, entry + 8, static_cast<unsigned>(contents.size()) + 1);
        put(data, entry + 12, value); text(data, s + 84 + key, name); text(data, s + 160 + value, contents);
        key += static_cast<unsigned>(name.size()) + 1; value += static_cast<unsigned>(contents.size()) + 1;
    }
    return data;
}
struct File {
    std::filesystem::path path = std::filesystem::current_path() /
        ("synthetic-media-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".iso");
    std::string sha;
    explicit File(const std::vector<std::uint8_t>& data = image()) : sha(psprecomp::sha256_bytes(data)) {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(data.data()), data.size());
        if (!out) throw std::runtime_error("Cannot write synthetic ISO");
    }
    ~File() { std::error_code ignored; std::filesystem::remove(path, ignored); }
};
}
