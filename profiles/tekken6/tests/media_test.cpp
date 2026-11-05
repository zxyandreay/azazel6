#include "media_fixture.hpp"
#include <iostream>
#include <sstream>

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class F> void rejects(F operation) {
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "Invalid media accepted");
}
int main() {
    try {
        for (auto size : {0, 1, 55, 56, 63, 64, 65, 65535, 65536, 65537, 131072}) {
            std::string data(size, 'a'); std::istringstream stream(data);
            const auto expected = psprecomp::sha256_bytes({reinterpret_cast<const std::uint8_t*>(data.data()), data.size()});
            require(azazel6::media_sha256(stream) == expected && stream.tellg() == 0, "Streaming SHA boundary");
        }
        media_fixture::File file;
        azazel6::IsoMedia media(file.path, file.sha);
        require(media.size() == 40 * 2048 && media.file_count() == 6, "ISO directory census");
        auto data = media.find("disc0:/psp_game/usrdir/data.bin;1");
        require(data && data->sector == 26 && data->size == 4, "ISO file metadata");
        require(media.read(*data, 0, 9) == std::vector<std::uint8_t>({'t','e','s','t'}), "Read clamps at EOF");
        require(media.read(*data, 3, 9) == std::vector<std::uint8_t>({'t'}) && media.read(*data, 4, 1).empty(), "Partial and EOF reads");
        require(!media.find("disc0:/MISSING.BIN"), "Missing file");
        for (const auto path : {"ms0:/x", "disc0:/../x", "disc0:/PSP_GAME//X", "disc0:/PSP_GAME\\X", "disc0:/", "disc0:/PSP_GAME/", "disc0:/X;2"})
            rejects([&] { (void)media.find(path); });
        rejects([&] { (void)media.read(*media.find("disc0:/PSP_GAME"), 0, 1); });
        rejects([&] { (void)media.read({0xFFFFFFFFu, 4, false}, 0, 4); });
        rejects([&] { (void)media.read(*data, 0, 0xFFFFFFFFu); });
        rejects([&] { azazel6::IsoMedia invalid(file.path, std::string(64, '0')); });
        {
            auto bytes = media_fixture::image(); bytes[16 * 2048 + 881] = 2;
            media_fixture::text(bytes, 23 * 2048 + 33, "data.bin;1");
            media_fixture::File version_two(bytes);
            azazel6::IsoMedia checked(version_two.path, version_two.sha);
            require(checked.file_count() == 6, "UMD file-structure version 2");
            require(checked.find("disc0:/PSP_GAME/USRDIR/DATA.BIN").has_value(), "Lowercase ISO identifiers");
        }
        for (unsigned scenario = 0; scenario < 12; ++scenario) {
            auto bytes = media_fixture::image();
            switch (scenario) {
            case 0: bytes.resize(17 * 2048); break;
            case 1: bytes[16 * 2048 + 1] = 'X'; break;
            case 2: bytes[16 * 2048 + 84] = 1; break;
            case 3: media_fixture::dual(bytes, 20 * 2048 + 2, 0xFFFFFFFFu); break;
            case 4: bytes[20 * 2048 + 25] |= 0x80; break;
            case 5: bytes[20 * 2048] = 10; break;
            case 6: media_fixture::dual(bytes, 21 * 2048 + 2, 20); break;
            case 7: bytes[24 * 2048 + 160] = 'X'; break;
            case 8: media_fixture::put(bytes, 24 * 2048 + 12, 0xFFFFFFFFu); break;
            case 9: media_fixture::put(bytes, 24 * 2048 + 24, 0xFFFFFFFFu); break;
            case 10: bytes[20 * 2048 + 26] = 1; break;
            case 11: bytes[20 * 2048 + 33] = '/'; break;
            }
            media_fixture::File bad(bytes);
            rejects([&] { azazel6::IsoMedia invalid(bad.path, bad.sha); });
        }
        std::cout << "Synthetic checked ISO and streaming hash contracts passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
