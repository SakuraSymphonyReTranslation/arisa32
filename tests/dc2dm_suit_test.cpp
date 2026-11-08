#include "arisa32/dc2dm_suit.h"

#include <algorithm>
#include <iostream>

namespace {
int failures = 0;
void check(bool value, const char* expression, int line) {
    if (!value) { std::cerr << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(value) check(bool(value), #value, __LINE__)
using Game = dc4scene::Dc2dmSuit;
using Hand = dc4scene::Dc2dmSuitHand;
using Result = dc4scene::Dc2dmSuitResult;
using Stage = dc4scene::Dc2dmSuitStage;

void all_matchups() {
    const Result expected[3][3] = {
        {Result::Draw, Result::Win, Result::Loss},
        {Result::Loss, Result::Draw, Result::Win},
        {Result::Win, Result::Loss, Result::Draw}};
    for (int player = 0; player < 3; ++player) for (int cpu = 0; cpu < 3; ++cpu) {
        Game game;
        game.start(4, 5);
        CHECK(game.play(Hand(player), Hand(cpu)));
        const auto result = expected[player][cpu];
        CHECK(game.result() == result);
        CHECK(game.player_counts()[player] == 1);
        CHECK(game.computer_counts()[cpu] == 1);
        CHECK(game.wins() == 4 + int(result == Result::Win));
        CHECK(game.losses() == 5 + int(result == Result::Loss));
        CHECK(game.stage() == (result == Result::Draw ? Stage::Choosing : Stage::Replay));
        CHECK(game.face_index() == (result == Result::Draw ? 0 : result == Result::Win ? 1 : 2));
        CHECK(game.history() == (result == Result::Draw ? u"" : result == Result::Win ? u"○" : u"×"));
    }
}

void draws_and_replay() {
    Game game;
    CHECK(!game.play(Hand::Rock, Hand::Scissors));
    game.start(7, 7);
    CHECK(game.aggregate_result() == 0);
    CHECK(!game.replay(true));
    CHECK(!game.play(Hand(-1), Hand::Rock));
    CHECK(!game.play(Hand::Rock, Hand(3)));
    for (int i = 0; i < 30; ++i) CHECK(game.play(Hand::Paper, Hand::Paper));
    CHECK(game.wins() == 7 && game.losses() == 7);
    CHECK(game.player_counts()[2] == 30 && game.computer_counts()[2] == 30);
    CHECK(game.history().empty() && game.stage() == Stage::Choosing);
    CHECK(game.console_lines().size() == 25);
    CHECK((game.choice_rows() == std::vector<int>{21,22,23}));
    CHECK(game.console_lines()[game.choice_rows()[0]] == u"　【グー】");
    CHECK(game.console_lines()[game.choice_rows()[2]] == u"　【パー】");

    CHECK(game.play(Hand::Rock, Hand::Scissors));
    CHECK(game.aggregate_result() == 1);
    const auto counts = game.player_counts();
    CHECK(!game.play(Hand::Rock, Hand::Scissors));
    CHECK(game.player_counts() == counts && game.wins() == 8);
    CHECK((game.choice_rows() == std::vector<int>{23,24}));
    CHECK(game.replay(true));
    CHECK(game.face_index() == 0 && game.stage() == Stage::Choosing);
    CHECK(game.console_lines()[0] == u"◆ Hasil Pertandingan" && game.console_lines()[1] == u"○");
    CHECK(game.console_lines()[2] == u"Pilihan Pemain: Batu １ kali / Gunting ０ kali / Kertas ３０ kali");
    CHECK(game.play(Hand::Scissors, Hand::Rock));
    CHECK(game.history() == u"○×" && game.aggregate_result() == 0);
    CHECK(game.replay(false));
    CHECK(game.stage() == Stage::Finished && game.choice_rows().empty());
    CHECK(!game.replay(true) && !game.play(Hand::Rock, Hand::Rock));
    game.start(2, 9);
    CHECK(game.aggregate_result() == 2);
    CHECK(game.player_counts()[2] == 0 && game.history().empty());
    CHECK(game.stage() == Stage::Choosing && game.face_index() == 0);
}

void persistent_score_and_history_limits() {
    Game game;
    game.start(254, 254);
    CHECK(game.play(Hand::Rock, Hand::Scissors));
    CHECK(game.wins() == 255 && game.losses() == 254);
    CHECK(game.replay(true) && game.play(Hand::Rock, Hand::Scissors));
    CHECK(game.wins() == 255);
    CHECK(game.replay(true) && game.play(Hand::Rock, Hand::Paper));
    CHECK(game.losses() == 255);
    CHECK(game.replay(true) && game.play(Hand::Rock, Hand::Paper));
    CHECK(game.losses() == 255 && game.history() == u"○○××");

    game.start(0, 0);
    for (int i = 0; i < 254; ++i) {
        CHECK(game.play(Hand::Paper, Hand::Rock));
        CHECK(game.stage() == Stage::Replay);
        CHECK(game.replay(true));
    }
    CHECK(game.history().size() == 254 && game.wins() == 254);
    CHECK(game.console_lines()[1].size() == 43);
    CHECK(game.console_lines()[6].size() == 39);
    CHECK(game.play(Hand::Paper, Hand::Paper));
    CHECK(game.stage() == Stage::Choosing && game.history().size() == 254);
    CHECK(game.play(Hand::Paper, Hand::Rock));
    CHECK(game.stage() == Stage::Finished);
    CHECK(game.history().size() == 254 && game.wins() == 254);
    CHECK(game.player_counts()[2] == 256 && game.choice_rows().empty());
    game.start(-1, 900);
    CHECK(game.wins() == 0 && game.losses() == 255);
}

void paced_transcript() {
    Game game;
    game.start(0, 0);
    auto batch = game.take_console_batch();
    CHECK(batch.reset && batch.lines == game.console_lines());
    batch = game.take_console_batch();
    CHECK(!batch.reset && batch.lines.empty());
    for (int i = 0; i < 4; ++i) CHECK(game.play(Hand::Rock, Hand::Rock));
    batch = game.take_console_batch();
    CHECK(!batch.reset && batch.lines.size() == 36);
    CHECK(batch.lines.front() == u"<<Anda: Batu>>");
    CHECK(game.console_lines().size() == 25);
    CHECK(game.play(Hand::Rock, Hand::Scissors));
    CHECK(game.replay(true));
    batch = game.take_console_batch();
    CHECK(batch.reset && batch.lines == game.console_lines());
    CHECK(batch.lines.front() == u"◆ Hasil Pertandingan");
}
}

int main() {
    all_matchups();
    draws_and_replay();
    persistent_score_and_history_limits();
    paced_transcript();
    if (failures) return 1;
    std::cout << "DC2DM suit rules passed\n";
    return 0;
}
