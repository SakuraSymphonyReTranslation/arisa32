#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dc4scene {

enum class Dc2dmSuitHand { Rock = 0, Scissors = 1, Paper = 2 };
enum class Dc2dmSuitResult { Win = 0, Loss = 1, Draw = 2 };
enum class Dc2dmSuitStage { Choosing, Replay, Finished };

class Dc2dmSuit {
public:
    struct ConsoleBatch {
        bool reset = false;
        std::vector<std::u16string> lines;
    };

    void start(int wins, int losses);
    bool play(Dc2dmSuitHand player, Dc2dmSuitHand computer);
    bool replay(bool yes);
    ConsoleBatch take_console_batch();

    Dc2dmSuitStage stage() const noexcept { return stage_; }
    Dc2dmSuitResult result() const noexcept { return result_; }
    int wins() const noexcept { return wins_; }
    int losses() const noexcept { return losses_; }
    int aggregate_result() const noexcept;
    int face_index() const noexcept { return face_index_; }
    const std::array<std::uint32_t, 3>& player_counts() const noexcept { return player_counts_; }
    const std::array<std::uint32_t, 3>& computer_counts() const noexcept { return computer_counts_; }
    const std::u16string& history() const noexcept { return history_; }
    const std::vector<std::u16string>& console_lines() const noexcept { return console_lines_; }
    const std::vector<int>& choice_rows() const noexcept { return choice_rows_; }

private:
    void append_line(std::u16string text, bool choice = false);
    void append_hand_prompt(bool draw);
    void append_statistics();
    void append_history();

    Dc2dmSuitStage stage_ = Dc2dmSuitStage::Finished;
    Dc2dmSuitResult result_ = Dc2dmSuitResult::Draw;
    int wins_ = 0;
    int losses_ = 0;
    int face_index_ = 0;
    std::array<std::uint32_t, 3> player_counts_{};
    std::array<std::uint32_t, 3> computer_counts_{};
    std::u16string history_;
    std::vector<std::u16string> console_lines_;
    std::vector<int> choice_rows_;
    ConsoleBatch console_batch_;
};

}
