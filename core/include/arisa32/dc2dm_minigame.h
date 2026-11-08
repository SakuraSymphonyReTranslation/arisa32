#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dc4scene {
struct Image;
class FontRenderer;
class Dc2dmReversi;

class Dc2dmMinigame {
public:
    enum class Stage { Boot, Suit, Difficulty, Order, Playing, Thinking, Flipping, Result, Replay, Finished };
    enum class Key { Left, Right, Up, Down, Next, Previous, Accept, Cancel };
    using ImageLoader = std::function<bool(const std::string&, Image*, std::string*)>;

    Dc2dmMinigame();
    ~Dc2dmMinigame();
    bool start(int kind, int wins, int losses, uint32_t seed,
               const ImageLoader& load, std::string* error,
               bool animated = false, bool boot_skippable = false);
    void update(uint32_t elapsed_ms);
    void pointer(int action, int x, int y);
    void key(Key key);
    void draw(const FontRenderer* font, std::vector<uint8_t>* frame) const;
    bool active() const;
    bool confirming_exit() const;
    bool take_music_start();
    int take_sound_effect();
    Stage stage() const;
    int kind() const;
    int wins() const;
    int losses() const;
    const Dc2dmReversi& reversi() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Native 0x42AD20 expands the two fullwidth score markers before text layout.
std::string dc2dm_minigame_score_text(const std::string& text, int wins, int losses);
}
