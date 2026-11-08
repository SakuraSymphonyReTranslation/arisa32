#include "arisa32/dc2dm_suit.h"

#include <algorithm>
#include <utility>

namespace dc4scene {
namespace {
std::u16string fullwidth_number(std::uint32_t number) {
    const auto digits = std::to_string(number);
    std::u16string text;
    for (char digit : digits) text.push_back(char16_t(u'０' + digit - '0'));
    return text;
}

std::u16string statistics(const char16_t* label, const std::array<std::uint32_t, 3>& counts) {
    return std::u16string(label) + u"Batu " + fullwidth_number(counts[0]) + u" kali / Gunting " +
        fullwidth_number(counts[1]) + u" kali / Kertas " + fullwidth_number(counts[2]) + u" kali";
}
}

void Dc2dmSuit::start(int wins, int losses) {
    wins_ = std::clamp(wins, 0, 255);
    losses_ = std::clamp(losses, 0, 255);
    player_counts_.fill(0);
    computer_counts_.fill(0);
    history_.clear();
    console_lines_.clear();
    choice_rows_.clear();
    console_batch_ = {true, {}};
    stage_ = Dc2dmSuitStage::Choosing;
    result_ = Dc2dmSuitResult::Draw;
    face_index_ = 0;
    append_statistics();
    append_hand_prompt(false);
}

bool Dc2dmSuit::play(Dc2dmSuitHand player, Dc2dmSuitHand computer) {
    const int player_index = static_cast<int>(player);
    const int computer_index = static_cast<int>(computer);
    if (stage_ != Dc2dmSuitStage::Choosing || player_index < 0 || player_index > 2 ||
        computer_index < 0 || computer_index > 2) return false;

    static constexpr const char16_t* player_text[] = {
        u"<<Anda: Batu>>", u"<<Anda: Gunting>>", u"<<Anda: Kertas>>"};
    static constexpr const char16_t* computer_text[] = {
        u"<<CPU: Batu>>", u"<<CPU: Gunting>>", u"<<CPU: Kertas>>"};
    choice_rows_.clear();
    ++player_counts_[player_index];
    ++computer_counts_[computer_index];
    append_line(player_text[player_index]);
    append_line(computer_text[computer_index]);

    if (player == computer) {
        result_ = Dc2dmSuitResult::Draw;
        append_hand_prompt(true);
        return true;
    }

    result_ = (player_index + 1) % 3 == computer_index ?
        Dc2dmSuitResult::Win : Dc2dmSuitResult::Loss;
    // Native 0x439BC0/0x439C60 caps the CP932 outcome buffer before awarding points.
    if (history_.size() >= 254) {
        stage_ = Dc2dmSuitStage::Finished;
        return true;
    }

    const bool won = result_ == Dc2dmSuitResult::Win;
    history_.push_back(won ? u'○' : u'×');
    if (won) wins_ = std::min(255, wins_ + 1);
    else losses_ = std::min(255, losses_ + 1);
    face_index_ = won ? 1 : 2;
    stage_ = Dc2dmSuitStage::Replay;
    append_line(u"《結果》");
    append_line(won ? u"あなたの勝ちです" : u"あなたの負けです");
    append_line(u"　");
    append_line(u"《もう一度対戦しますか？》");
    append_line(u"　");
    append_line(u"　【はい】", true);
    append_line(u"　【いいえ】", true);
    return true;
}

bool Dc2dmSuit::replay(bool yes) {
    if (stage_ != Dc2dmSuitStage::Replay) return false;
    choice_rows_.clear();
    if (!yes) {
        stage_ = Dc2dmSuitStage::Finished;
        return true;
    }
    stage_ = Dc2dmSuitStage::Choosing;
    face_index_ = 0;
    console_lines_.clear();
    console_batch_ = {true, {}};
    append_history();
    append_statistics();
    append_hand_prompt(false);
    return true;
}

int Dc2dmSuit::aggregate_result() const noexcept {
    return wins_ == losses_ ? 0 : wins_ > losses_ ? 1 : 2;
}

Dc2dmSuit::ConsoleBatch Dc2dmSuit::take_console_batch() {
    auto batch = std::move(console_batch_);
    console_batch_ = {};
    return batch;
}

void Dc2dmSuit::append_line(std::u16string text, bool choice) {
    // Native 0x43A09B reserves the last of its 26 rows while scrolling.
    if (console_lines_.size() == 25) {
        console_lines_.erase(console_lines_.begin());
        for (auto& row : choice_rows_) --row;
        choice_rows_.erase(std::remove(choice_rows_.begin(), choice_rows_.end(), -1), choice_rows_.end());
    }
    if (choice) choice_rows_.push_back(static_cast<int>(console_lines_.size()));
    console_batch_.lines.push_back(text);
    console_lines_.push_back(std::move(text));
}

void Dc2dmSuit::append_hand_prompt(bool draw) {
    append_line(u"　");
    append_line(draw ? u"《あーいこーで》" : u"《じゃーん、けーん》");
    append_line(u"　");
    append_line(u"　【グー】", true);
    append_line(u"　【チョキ】", true);
    append_line(u"　【パー】", true);
    if (draw) append_line(u"　");
}

void Dc2dmSuit::append_statistics() {
    append_line(statistics(u"Pilihan Pemain: ", player_counts_));
    append_line(statistics(u"Pilihan CPU: ", computer_counts_));
}

void Dc2dmSuit::append_history() {
    append_line(u"◆ Hasil Pertandingan");
    // Native 0x43999C wraps the two-byte result marks at 86 bytes.
    for (size_t offset = 0; offset < history_.size(); offset += 43)
        append_line(history_.substr(offset, 43));
}
}
