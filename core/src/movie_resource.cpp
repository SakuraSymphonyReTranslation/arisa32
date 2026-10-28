#include "arisa32/movie.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

namespace dc4 {
namespace {
namespace fs = std::filesystem;
std::string lower(std::string value) {
    for (char& c : value) c = char(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
const std::vector<std::string> extensions = {
    ".mpg", ".mpeg", ".wmv", ".asf", ".mp4", ".m4v", ".ogv", ".mkv", ".avi", ".webm", ".ts"
};
fs::path child(const fs::path& directory, const std::string& name) {
    std::error_code ec;
    const auto direct = directory / fs::u8path(name);
    if (fs::exists(direct, ec)) return direct;
    ec.clear();
    for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec))
        if (lower(it->path().filename().u8string()) == lower(name)) return it->path();
    return {};
}
size_t opening_position(const std::string& value) {
    auto pos = value.find("opening");
    if (pos != std::string::npos) return pos + 7;
    for (pos = value.find("op"); pos != std::string::npos; pos = value.find("op", pos+2)) {
        const bool prefix = pos == 0 || value[pos-1] == '_' || value[pos-1] == '-' || std::isdigit(static_cast<unsigned char>(value[pos-1]));
        const size_t end = pos + 2;
        const bool suffix = end == value.size() || value[end] == '_' || value[end] == '-' || std::isdigit(static_cast<unsigned char>(value[end]));
        if (prefix && suffix) return end;
    }
    pos = value.find("mv001");
    if (pos != std::string::npos) return pos + 2;
    pos = value.find("mv01");
    return pos == std::string::npos ? pos : pos + 2;
}
char opening_number(const std::string& stem) {
    const auto value = lower(stem);
    auto pos = opening_position(value);
    if (pos == std::string::npos) return 0;
    while (pos < value.size() && (value[pos] == '_' || value[pos] == '-' || value[pos] == '0')) ++pos;
    return pos < value.size() && value[pos] >= '1' && value[pos] <= '9' ? value[pos] : 0;
}
bool opening(const std::string& name) {
    const auto value = lower(name);
    return opening_position(value) != std::string::npos;
}
}
std::string resolve_movie_file(const std::string& data_dir, const std::string& overlay,
                               const std::string& language, const std::string& asset) {
    std::string relative = asset;
    std::replace(relative.begin(), relative.end(), '\\', '/');
    if (lower(relative).rfind("movie/", 0) == 0) relative.erase(0, 6);
    if (relative.empty() || relative.size() > 260 || relative.find('\0') != std::string::npos ||
        relative.find('/') != std::string::npos || relative.find(':') != std::string::npos || relative.find("..") != std::string::npos) return {};
    const auto resource = fs::u8path(relative);
    const auto extension = lower(resource.extension().u8string());
    if (!extension.empty() && std::find(extensions.begin(), extensions.end(), extension) == extensions.end()) return {};
    const std::string stem = resource.stem().u8string();
    fs::path data = fs::u8path(data_dir), root = data;
    const auto leaf = lower(data.filename().u8string());
    if (leaf == "advdata" || leaf == "id_data" || leaf == "eng_data") root = data.parent_path();
    std::vector<fs::path> directories;
    auto add = [&](const fs::path& directory) {
        std::error_code ec;
        if (!directory.empty() && fs::is_directory(directory, ec) &&
            std::find(directories.begin(), directories.end(), directory) == directories.end()) directories.push_back(directory);
    };
    auto add_content = [&](const fs::path& directory) {
        if (directory.empty()) return;
        add(child(directory, "Movie")); add(child(directory, "Movies")); add(directory);
    };
    // Explicit imported content wins, then the selected language's sibling data.
    if (!overlay.empty() && lower(language) != "jp") add_content(fs::u8path(overlay));
    if (lower(language) == "id") { add_content(child(root, "id_Data")); add_content(child(root, "id")); }
    if (lower(language) == "en") { add_content(child(root, "eng_Data")); add_content(child(root, "en_Data")); add_content(child(root, "en")); }
    // Some patches ship loose movies under patch/, others overwrite Movie/.
    add_content(child(root, "patch")); add_content(child(data, "patch"));
    add_content(data); add_content(child(root, "AdvData")); add_content(root);
    std::vector<std::string> names;
    if (!extension.empty()) names.push_back(relative);
    for (const auto& ext : extensions) if (ext != extension) names.push_back(stem + ext);
    std::error_code ec;
    // Directory precedence applies to renamed openings too, preserving the overlay.
    for (const auto& directory : directories) {
        for (const auto& name : names) {
            const auto file = child(directory, name);
            if (!file.empty() && fs::is_regular_file(file, ec) && fs::file_size(file, ec) > 0) return file.u8string();
        }
        if (!opening(stem)) continue;
        std::vector<fs::path> matches;
        ec.clear();
        for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            const auto file_stem = it->path().stem().u8string();
            const auto file_ext = lower(it->path().extension().u8string());
            const char requested = opening_number(stem);
            if (opening(file_stem) && (!requested || requested == opening_number(file_stem)) &&
                std::find(extensions.begin(), extensions.end(), file_ext) != extensions.end() && it->file_size(ec) > 0)
                matches.push_back(it->path());
        }
        std::sort(matches.begin(), matches.end(), [](const fs::path& a, const fs::path& b) {
            const auto ai = std::find(extensions.begin(), extensions.end(), lower(a.extension().u8string()));
            const auto bi = std::find(extensions.begin(), extensions.end(), lower(b.extension().u8string()));
            return ai != bi ? ai < bi : a.u8string() < b.u8string();
        });
        if (!matches.empty()) return matches.front().u8string();
    }
    return {};
}
} // namespace dc4
