#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace dc4scene {

class Dc2dmReversi {
public:
    enum Cell { Empty = 0, Black = 1, White = 2 };
    using Board = std::array<Cell, 64>;

    Dc2dmReversi() { reset(); }
    void reset();
    void set_board(const Board& board, Cell turn);
    const Board& board() const { return board_; }
    Cell cell(int x, int y) const;
    Cell turn() const { return turn_; }
    Cell passed_turn() const { return passed_turn_; }
    bool finished() const { return finished_; }
    int count(Cell color) const;
    std::vector<int> legal_moves(Cell color) const;
    std::vector<int> flips(int index, Cell color) const;
    bool play(int index);
    bool pass();
    int result_for(Cell color) const;
    int choose_move(Cell color, int depth = 3) const;
    void set_random_seed(uint32_t seed);
    int choose_move_for_level(Cell color, int level);
    static Cell opponent(Cell color) { return color == Black ? White : Black; }

private:
    Board board_{};
    Cell turn_ = Black;
    Cell passed_turn_ = Empty;
    bool finished_ = false;
    uint32_t random_state_ = 1;
    int early_style_ = 1;
    int first_move_ = -1;
    bool opening_book_ = true;
    uint32_t next_random();
    void advance_turn();
};

}
