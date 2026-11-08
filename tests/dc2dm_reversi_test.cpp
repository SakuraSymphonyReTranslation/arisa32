#include "arisa32/dc2dm_reversi.h"

#include <algorithm>
#include <cstdint>
#include <iostream>

namespace {
int failures = 0;
void check(bool value, const char* expression, int line) {
    if (!value) { std::cerr << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(value) check(bool(value), #value, __LINE__)
using Game = dc4scene::Dc2dmReversi;

void opening_and_illegal_moves() {
    Game game;
    CHECK(game.count(Game::Black) == 2);
    CHECK(game.count(Game::White) == 2);
    CHECK(game.turn() == Game::Black);
    CHECK((game.legal_moves(Game::Black) == std::vector<int>{19, 26, 37, 44}));
    const auto opening = game.board();
    CHECK(!game.play(-1));
    CHECK(!game.play(64));
    CHECK(!game.play(27));
    CHECK(!game.play(0));
    CHECK(!game.pass());
    CHECK(game.board() == opening);
    CHECK(game.play(19));
    CHECK(game.cell(3, 3) == Game::Black);
    CHECK(game.count(Game::Black) == 4);
    CHECK(game.count(Game::White) == 1);
    CHECK(game.turn() == Game::White);
}

void captures_and_edges() {
    Game game;
    Game::Board board{};
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
        if (!dx && !dy) continue;
        board[(3 + dy) * 8 + 3 + dx] = Game::White;
        board[(3 + 2 * dy) * 8 + 3 + 2 * dx] = Game::Black;
    }
    game.set_board(board, Game::Black);
    CHECK(game.flips(27, Game::Black).size() == 8);
    CHECK(game.play(27));
    CHECK(game.count(Game::Black) == 17);
    CHECK(game.count(Game::White) == 0);
    CHECK(game.finished());
    CHECK(game.result_for(Game::Black) == 1);
    CHECK(game.result_for(Game::White) == 2);

    board.fill(Game::Empty);
    board[7] = Game::White;
    board[8] = Game::Black;
    game.set_board(board, Game::Black);
    CHECK(game.flips(6, Game::Black).empty());
    CHECK(!game.play(6));
    board.fill(Game::Empty);
    board[1] = Game::White;
    board[2] = Game::White;
    game.set_board(board, Game::Black);
    CHECK(game.flips(0, Game::Black).empty());
}

void passes_and_results() {
    Game game;
    Game::Board board;
    board.fill(Game::Black);
    board[0] = board[63] = Game::Empty;
    board[1] = board[62] = Game::White;
    game.set_board(board, Game::Black);
    CHECK(game.play(0));
    CHECK(!game.finished());
    CHECK(game.turn() == Game::Black);
    CHECK(game.passed_turn() == Game::White);
    CHECK(game.play(63));
    CHECK(game.finished());
    CHECK(game.count(Game::Black) == 64);
    CHECK(!game.play(0));

    board.fill(Game::White);
    board[0] = Game::Empty;
    board[1] = Game::Black;
    game.set_board(board, Game::Black);
    CHECK(!game.finished());
    CHECK(game.choose_move(Game::Black) == -1);
    CHECK(game.pass());
    CHECK(game.turn() == Game::White);
    CHECK(game.passed_turn() == Game::Black);
    CHECK(game.play(0));
    CHECK(game.finished());
    CHECK(game.result_for(Game::Black) == 2);

    board.fill(Game::Black);
    board[0] = Game::Empty;
    game.set_board(board, Game::White);
    CHECK(game.finished());
    CHECK(game.count(Game::Empty) == 1);

    for (int i = 0; i < 64; ++i) board[i] = i < 32 ? Game::Black : Game::White;
    game.set_board(board, Game::Black);
    CHECK(game.finished());
    CHECK(game.result_for(Game::Black) == 0);
    CHECK(game.result_for(Game::White) == 0);
}

void computer_play() {
    Game game;
    Game::Board board{};
    board[2] = board[24] = Game::Black;
    board[1] = board[16] = Game::White;
    game.set_board(board, Game::Black);
    CHECK(game.choose_move(Game::Black, 1) == 0);
    CHECK(game.choose_move(Game::Black, 3) == game.choose_move(Game::Black, 3));
    CHECK(game.choose_move_for_level(Game::Black, 1) == 0);
    CHECK(game.choose_move_for_level(Game::Black, 2) == 0);
    game.reset();
    game.set_random_seed(281);
    Game identical = game;
    for (int attempt = 0; attempt < 20; ++attempt) {
        const int move = game.choose_move_for_level(Game::Black, attempt % 3);
        CHECK(move == identical.choose_move_for_level(Game::Black, attempt % 3));
        const auto legal = game.legal_moves(Game::Black);
        CHECK(std::find(legal.begin(), legal.end(), move) != legal.end());
    }
    uint32_t random = 731;
    for (int trial = 0; trial < 8; ++trial) {
        game.reset();
        int moves = 0;
        while (!game.finished() && moves < 61) {
            const auto legal = game.legal_moves(game.turn());
            CHECK(!legal.empty());
            if (legal.empty()) break;
            const auto before = game.board();
            const int empty = game.count(Game::Empty);
            const auto side = game.turn();
            random = random * 1664525u + 1013904223u;
            const int move = (moves + trial) % 2 == 0 ? game.choose_move(side, 1) : legal[random % legal.size()];
            CHECK(std::find(legal.begin(), legal.end(), move) != legal.end());
            CHECK(game.board() == before);
            CHECK(game.play(move));
            CHECK(game.count(Game::Empty) == empty - 1);
            CHECK(game.board()[move] == side);
            ++moves;
        }
        CHECK(game.finished());
        CHECK(moves <= 60);
        CHECK(game.legal_moves(Game::Black).empty());
        CHECK(game.legal_moves(Game::White).empty());
        CHECK(game.result_for(Game::Black) + game.result_for(Game::White) != 2);
    }
}

void native_ai_fixtures() {
    Game game;
    game.set_random_seed(4);
    CHECK(game.choose_move_for_level(Game::Black, 2) == 37);
    const int first_moves[] = {37, 44, 19, 26};
    const int replies[2][4] = {{45, 45, 18, 18}, {43, 29, 34, 20}};
    for (int variant = 0; variant < 2; ++variant) for (int i = 0; i < 4; ++i) {
        game.reset();
        game.set_random_seed(variant == 0 ? 2 : 4);
        CHECK(game.play(first_moves[i]));
        const auto before = game.board();
        CHECK(game.choose_move_for_level(Game::White, 2) == replies[variant][i]);
        CHECK(game.board() == before);
    }

    game.reset();
    const auto opening = game.board();
    game.set_board(opening, Game::Black);
    game.set_random_seed(1);
    // Isolated native 0x43DB30 and 0x43E7C0 calls return 19 and 26 on this board.
    CHECK(game.choose_move_for_level(Game::Black, 2) == 19);
    game.set_random_seed(4);
    CHECK(game.choose_move_for_level(Game::Black, 2) == 26);
    CHECK(game.board() == opening);

    const int cells[64] = {
        1,1,2,2,2,2,0,1, 0,1,2,2,2,1,1,1,
        2,2,1,1,2,2,2,1, 2,0,2,1,1,2,2,1,
        2,1,1,2,1,2,2,1, 2,1,1,2,2,2,2,0,
        2,2,1,1,2,2,2,1, 2,2,2,2,2,2,2,2
    };
    Game::Board endgame;
    for (int i = 0; i < 64; ++i) endgame[i] = static_cast<Game::Cell>(cells[i]);
    game.set_board(endgame, Game::Black);
    CHECK(game.choose_move_for_level(Game::Black, 2) == 47);
    CHECK(game.board() == endgame);
}
}

int main() {
    opening_and_illegal_moves();
    captures_and_edges();
    passes_and_results();
    computer_play();
    native_ai_fixtures();
    if (failures) return 1;
    std::cout << "DC2DM Reversi rules passed\n";
    return 0;
}
