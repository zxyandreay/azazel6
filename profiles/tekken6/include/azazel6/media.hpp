#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace azazel6 {
// Hash an already opened stream in bounded memory; resets it to byte zero.
std::string media_sha256(std::istream& input);

struct DiscFile {
    std::uint32_t sector = 0, size = 0;
    bool directory = false;
};

// Read-only, single-volume ISO9660. No host directory mapping or decompression.
class IsoMedia {
public:
    IsoMedia(const std::filesystem::path&, const std::string& expected_sha256);
    std::optional<DiscFile> find(const std::string& guest_path) const;
    std::vector<std::uint8_t> read(const DiscFile&, std::uint64_t offset, std::uint32_t count);
    std::uint64_t size() const { return size_; }
    const std::string& fingerprint() const { return fingerprint_; }
    std::size_t file_count() const { return files_.size(); }
private:
    std::ifstream input_;
    std::uint64_t size_ = 0, metadata_bytes_ = 0;
    std::string fingerprint_;
    std::map<std::string, DiscFile> files_;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> directories_;
    std::vector<std::uint8_t> bytes(std::uint64_t offset, std::uint32_t count);
    DiscFile record(std::span<const std::uint8_t>);
    void directory(const std::string&, DiscFile, unsigned depth);
    void validate_identity();
};
} // namespace azazel6
