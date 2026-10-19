#pragma once
#include <string>
#include <vector>

namespace dc4 {
enum class Language {
    Japanese = 0,
    Indonesian = 1,
    English = 2
};

// Configure only while no engine is executing. Overlays replace GRP/MES/MOVIE
// resources individually; everything else, including saves, keeps its path.
void configure_resources(const std::string& base, const std::string& overlay = {},
                         const std::string& text_encoding = "windows-31j");
std::string resolve_asset(const std::string& path);
bool asset_exists(const std::string& path);
bool prefers_utf8_text();
bool decode_utf8(const std::string& bytes, std::u16string* text);

void set_language(Language lang);
Language get_language();
bool is_latin();
std::string translate_ui(const std::string& text);
std::u16string translate_ui(const std::u16string& text);
int romanized_face_id(const std::u16string& name);

// Modular game & cutscene detection for Da Capo 4 & Da Capo 4 Plus Harmony
bool is_dc4_or_dc4ph_game(const std::string& data_dir);
bool is_kotori_song_movie(const std::string& asset);
}  // namespace dc4

