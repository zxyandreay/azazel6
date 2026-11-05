#include "azazel6/media.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>
#include <set>
#include <string_view>

namespace azazel6 {
namespace {
using Bytes = std::span<const std::uint8_t>;
void check(bool valid, const char* message) { if (!valid) throw psprecomp::Error(message); }
std::uint32_t le(Bytes b, std::size_t p, unsigned n = 4) {
    check(p <= b.size() && n <= b.size() - p, "Truncated media metadata");
    std::uint32_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= std::uint32_t(b[p + i]) << (8 * i);
    return v;
}
std::uint32_t both(Bytes b, std::size_t p, unsigned n = 4) {
    const auto v = le(b, p, n);
    check(p + 2 * n <= b.size(), "Truncated dual-endian field");
    std::uint32_t big = 0;
    for (unsigned i = 0; i < n; ++i) big = (big << 8) | b[p + n + i];
    check(v == big, "Inconsistent ISO byte orders");
    return v;
}
std::string text(Bytes b, std::size_t p, std::size_t n) {
    check(p <= b.size() && n <= b.size() - p, "Truncated media string");
    return {reinterpret_cast<const char*>(b.data() + p), n};
}
bool component(const std::string& name) {
    return !name.empty() && name != "." && name != ".." && name.size() <= 255 &&
        std::all_of(name.begin(), name.end(), [](unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
        });
}
std::string canonical(std::string path) {
    check(path.starts_with("disc0:/"), "Only absolute disc0 paths are supported");
    path.erase(0, 7);
    check(!path.empty() && path.size() <= 1024, "Invalid disc path length");
    for (char& c : path) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (path.ends_with(";1")) path.resize(path.size() - 2);
    std::size_t pos = 0;
    while (pos < path.size()) {
        const auto end = path.find('/', pos);
        check(component(path.substr(pos, end == std::string::npos ? end : end - pos)), "Invalid disc path component");
        if (end == std::string::npos) break;
        pos = end + 1;
        check(pos < path.size(), "Trailing disc path separator");
    }
    return path;
}
}

std::vector<std::uint8_t> IsoMedia::bytes(std::uint64_t offset, std::uint32_t count) {
    check(count <= 8 * 1024 * 1024 && offset <= size_ && count <= size_ - offset,
          "Media read outside bounded image");
    std::vector<std::uint8_t> out(count);
    input_.clear(); input_.seekg(static_cast<std::streamoff>(offset));
    if (count) input_.read(reinterpret_cast<char*>(out.data()), count);
    check(bool(input_), "Short or failed media read");
    return out;
}

DiscFile IsoMedia::record(Bytes r) {
    check(r.size() >= 34 && r[0] == r.size() && r[32] && 33u + r[32] <= r.size(), "Malformed ISO directory record");
    check(!r[1] && !(r[25] & ~3u) && !r[26] && !r[27] && both(r, 28, 2) == 1,
          "Unsupported ISO record attributes, extent or interleave");
    DiscFile file{both(r, 2), both(r, 10), bool(r[25] & 2)};
    const auto offset = std::uint64_t(file.sector) * 2048;
    check(offset <= size_ && file.size <= size_ - offset, "ISO extent outside image");
    return file;
}

void IsoMedia::directory(const std::string& prefix, DiscFile dir, unsigned depth) {
    check(depth <= 16 && dir.directory && dir.size && dir.size <= 1024 * 1024,
          "Invalid or oversized ISO directory");
    const auto key = std::pair{dir.sector, dir.size};
    check(std::find(directories_.begin(), directories_.end(), key) == directories_.end(), "Cyclic or aliased ISO directory");
    check(directories_.size() < 1024 && metadata_bytes_ + dir.size <= 8 * 1024 * 1024,
          "ISO metadata budget exceeded");
    directories_.push_back(key); metadata_bytes_ += dir.size;
    const auto data = bytes(std::uint64_t(dir.sector) * 2048, dir.size);
    for (std::size_t pos = 0; pos < data.size();) {
        const auto length = data[pos];
        if (!length) { pos = (pos / 2048 + 1) * 2048; continue; }
        check(length <= data.size() - pos && length <= 2048 - pos % 2048, "ISO record crosses directory or sector boundary");
        const auto r = Bytes(data).subspan(pos, length);
        const auto entry = record(r);
        auto name = text(r, 33, r[32]);
        pos += length;
        if (name == std::string(1, '\0') || name == std::string(1, '\1')) continue;
        if (name.ends_with(";1")) name.resize(name.size() - 2);
        for (char& c : name) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        check(component(name), "Unsupported ISO filename");
        const auto path = prefix + name;
        check(path.size() <= 1024 && files_.size() < 8192 && files_.emplace(path, entry).second,
              "Duplicate ISO filename or metadata budget exceeded");
        if (entry.directory) directory(path + '/', entry, depth + 1);
    }
}

IsoMedia::IsoMedia(const std::filesystem::path& path, const std::string& expected)
    : input_(path, std::ios::binary) {
    check(bool(input_), "Cannot open selected ISO");
    input_.seekg(0, std::ios::end);
    const auto length = input_.tellg();
    check(length >= 18 * 2048 && length <= 0x80000000ll && length % 2048 == 0, "Invalid ISO image size");
    size_ = static_cast<std::uint64_t>(length);
    check(expected.size() == 64 && std::all_of(expected.begin(), expected.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }), "Invalid expected media fingerprint");
    fingerprint_ = media_sha256(input_);
    check(fingerprint_ == expected, "Unsupported ISO SHA-256");
    const auto pvd = bytes(16 * 2048, 2048);
    // The exact UMD uses file-structure version 2 with these same checked
    // single-extent directory records. Other extensions remain rejecting.
    check(pvd[0] == 1 && text(pvd, 1, 5) == "CD001" && pvd[6] == 1 && (pvd[881] == 1 || pvd[881] == 2),
          "Unsupported ISO primary volume descriptor");
    check(both(pvd, 80) * 2048ull == size_ && both(pvd, 120, 2) == 1 &&
          both(pvd, 124, 2) == 1 && both(pvd, 128, 2) == 2048, "Unsupported ISO volume geometry");
    bool terminated = false;
    for (unsigned sector = 17; sector < 32; ++sector) {
        const auto descriptor = bytes(sector * 2048ull, 2048);
        check(text(descriptor, 1, 5) == "CD001" && descriptor[6] == 1, "Invalid ISO volume descriptor");
        if (descriptor[0] == 255) { terminated = true; break; }
        check(descriptor[0] == 0 || descriptor[0] == 2, "Unsupported ISO volume descriptor type");
    }
    check(terminated, "Missing bounded ISO descriptor terminator");
    directory("", record(Bytes(pvd).subspan(156, pvd[156])), 0);
    validate_identity();
}

std::optional<DiscFile> IsoMedia::find(const std::string& guest_path) const {
    const auto found = files_.find(canonical(guest_path));
    return found == files_.end() ? std::nullopt : std::optional(found->second);
}

std::vector<std::uint8_t> IsoMedia::read(const DiscFile& file, std::uint64_t offset, std::uint32_t count) {
    check(!file.directory, "Cannot read a disc directory as a file");
    check(count <= 8 * 1024 * 1024, "Media read budget exceeded");
    const auto base = std::uint64_t(file.sector) * 2048;
    check(base <= size_ && file.size <= size_ - base, "Invalid disc file extent");
    if (offset >= file.size) return {};
    return bytes(base + offset, static_cast<std::uint32_t>(std::min<std::uint64_t>(count, file.size - offset)));
}

void IsoMedia::validate_identity() {
    const auto file = find("disc0:/PSP_GAME/PARAM.SFO");
    check(file && !file->directory && file->size <= 65536, "Missing bounded PARAM.SFO");
    const auto data = read(*file, 0, file->size);
    check(le(data, 0) == 0x46535000 && le(data, 4) == 0x101, "Unsupported SFO header");
    const auto keys = le(data, 8), values = le(data, 12), count = le(data, 16);
    check(count <= 128 && keys >= 20 + count * 16 && keys <= values && values <= data.size(), "Invalid SFO tables");
    std::map<std::string, std::string> fields;
    for (unsigned i = 0; i < count; ++i) {
        const auto p = 20 + i * 16, key = le(data, p, 2), format = le(data, p + 2, 2);
        const auto length = le(data, p + 4), capacity = le(data, p + 8), value = le(data, p + 12);
        check(key < values - keys && value <= data.size() - values && length <= capacity &&
              capacity <= data.size() - values - value, "Invalid SFO entry ranges");
        auto key_end = keys + key;
        while (key_end < values && data[key_end]) ++key_end;
        check(key_end < values, "Unterminated SFO key");
        const auto name = text(data, keys + key, key_end - keys - key);
        check(!fields.contains(name), "Duplicate SFO key");
        if (format == 0x204) {
            check(length && data[values + value + length - 1] == 0, "Unterminated SFO text");
            fields[name] = text(data, values + value, length - 1);
        } else fields[name] = "";
    }
    check(fields["DISC_ID"] == "ULUS10466" && fields["DISC_VERSION"] == "1.00" &&
          fields["TITLE"] == "TEKKEN 6" && fields["CATEGORY"] == "UG", "Unsupported disc title or region");
    const auto data_dir = find("disc0:/PSP_GAME/USRDIR");
    const auto executable = find("disc0:/PSP_GAME/SYSDIR/EBOOT.BIN");
    check(data_dir && data_dir->directory && executable && !executable->directory && executable->size,
          "Disc lacks game data or executable");
}
} // namespace azazel6
