#include "arisa32/dc2dm_reversi.h"

#include <algorithm>
#include <limits>

namespace dc4scene {
namespace {

bool valid_color(Dc2dmReversi::Cell color) {
    return color == Dc2dmReversi::Black || color == Dc2dmReversi::White;
}

constexpr int weights[64] = {
    120, -25, 20, 5, 5, 20, -25, 120,
    -25, -45, -5, -5, -5, -5, -45, -25,
     20, -5, 15, 3, 3, 15, -5, 20,
      5, -5, 3, 3, 3, 3, -5, 5,
      5, -5, 3, 3, 3, 3, -5, 5,
     20, -5, 15, 3, 3, 15, -5, 20,
    -25, -45, -5, -5, -5, -5, -45, -25,
    120, -25, 20, 5, 5, 20, -25, 120
};

// DC2DM.EXE 0x4D6E80, 0x4D6F80 and 0x4D7080; the middle table is asymmetric.
constexpr int native_weights[3][64] = {
    {30,-12,0,-1,-1,0,-12,30, -12,-15,-3,-3,-3,-3,-15,-12,
     0,-3,0,-1,-1,0,-3,0, -1,-3,-1,-1,-1,-1,-3,-1,
     -1,-3,-1,-1,-1,-1,-3,-1, 0,-3,0,-1,-1,0,-3,0,
     -12,-15,-3,-3,-3,-3,-15,-12, 30,-12,0,-1,-1,0,-12,30},
    {100,-11,4,-1,-1,4,-11,100, -11,-16,-1,-3,-3,-1,-16,-11,
     4,-1,2,-1,-1,2,-3,4, -1,-3,-1,0,0,-1,-3,-1,
     -1,-3,-1,0,0,-1,-3,-1, 4,-1,2,-1,-1,2,-3,4,
     -11,-16,-1,-3,-3,-1,-16,-11, 100,-11,4,-1,-1,4,-11,100},
    {80,-12,0,-1,-1,0,-12,80, -12,-15,-3,-3,-3,-3,-15,-12,
     0,-3,0,-1,-1,0,-3,0, -1,-3,-1,-1,-1,-1,-3,-1,
     -1,-3,-1,-1,-1,-1,-3,-1, 0,-3,0,-1,-1,0,-3,0,
     -12,-15,-3,-3,-3,-3,-15,-12, 80,-12,0,-1,-1,0,-12,80}
};

int apply_native_move(Dc2dmReversi::Board& board, int move, Dc2dmReversi::Cell color, bool apply = true) {
    if (move < 0 || move >= 64 || board[move] != Dc2dmReversi::Empty) return 0;
    const int x = move % 8, y = move / 8;
    const auto other = Dc2dmReversi::opponent(color);
    int flipped = 0;
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) continue;
        int nx = x + dx, ny = y + dy, length = 0;
        while (nx >= 0 && nx < 8 && ny >= 0 && ny < 8 && board[ny * 8 + nx] == other) {
            ++length;
            nx += dx;
            ny += dy;
        }
        if (length == 0 || nx < 0 || nx >= 8 || ny < 0 || ny >= 8 || board[ny * 8 + nx] != color)
            continue;
        flipped += length;
        if (apply) for (int i = 1; i <= length; ++i) board[(y + dy * i) * 8 + x + dx * i] = color;
    }
    if (apply && flipped != 0) board[move] = color;
    return flipped;
}

int native_position_value(const Dc2dmReversi::Board& board, Dc2dmReversi::Cell color, int table) {
    int value = 0;
    const auto other = Dc2dmReversi::opponent(color);
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
        const int weight = native_weights[table][std::min(y, 7 - y) * 8 + std::min(x, 7 - x)];
        if (board[y * 8 + x] == color) value += weight;
        else if (board[y * 8 + x] == other) value -= weight;
    }
    return value;
}

// 0x43DB30 chooses an opponent reply recursively, then scores that one resulting board.
int native_recursive_move(const Dc2dmReversi::Board& board, Dc2dmReversi::Cell color, int depth, int table) {
    const int empty = static_cast<int>(std::count(board.begin(), board.end(), Dc2dmReversi::Empty));
    const double flip_weight = 5.0 * (1.0 - empty / 60.0) + empty / 60.0;
    int best = -1;
    double best_value = -1000000.0;
    for (int move = 0; move < 64; ++move) {
        auto next = board;
        const int flipped = apply_native_move(next, move, color);
        if (flipped == 0) continue;
        if (depth > 0) {
            const auto other = Dc2dmReversi::opponent(color);
            const int reply = native_recursive_move(next, other, depth - 1, table);
            if (reply != -1) apply_native_move(next, reply, other);
        }
        double value = native_position_value(next, color, table) + flipped * flip_weight;
        const int x = move % 8, y = move / 8;
        if (((x == 1 || x == 6) && (y <= 1 || y >= 6)) ||
            ((x == 0 || x == 7) && (y == 1 || y == 6))) value -= 30.0;
        if (value > best_value) { best_value = value; best = move; }
    }
    return best;
}

int native_edge_value(const Dc2dmReversi::Board& board, int start, int step, int inside_a, int inside_b) {
    int edge[8];
    for (int i = 0; i < 8; ++i) edge[i] = board[start + i * step];
    if (edge[0] == 0 && edge[7] == 0) {
        if (edge[1] == 0 && edge[6] == 0) return 0;
        if (edge[1] != 0 && edge[2] == 0 && edge[3] == 0) return -3 * edge[1];
        if (edge[6] != 0 && edge[5] == 0 && edge[4] == 0) return -3 * edge[6];
        int sum = 0;
        for (int value : edge) sum += value;
        if (sum == 6) return 2;
        if (sum == -6) return -2;
        if (sum != 5 && sum != -5) return 0;
        const int sign = sum > 0 ? 1 : -1;
        return (edge[1] == sign && board[inside_a] == sign) ||
               (edge[6] == sign && board[inside_b] == sign) ? -2 * sign : 0;
    }
    int sum = 0;
    if (edge[0] != 0 && edge[7] != 0) {
        for (int value : edge) sum += value;
    } else if (edge[0] != 0) {
        for (int i = 0; i < 8 && edge[i] == edge[0]; ++i) sum += edge[i];
    } else {
        for (int i = 7; i >= 0 && edge[i] == edge[7]; --i) sum += edge[i];
    }
    return 5 * sum;
}

int native_alternate_value(Dc2dmReversi::Board& board, int delta) {
    int value = delta + native_edge_value(board, 0, 1, 13, 10) +
                native_edge_value(board, 56, 1, 53, 50) +
                native_edge_value(board, 0, 8, 41, 17) +
                native_edge_value(board, 7, 8, 46, 22);
    for (int move = 0; move < 64; ++move) {
        const int x = move % 8, y = move / 8;
        if ((x == 1 || x == 6) && (y == 1 || y == 6)) continue;
        if (apply_native_move(board, move, Dc2dmReversi::Black, false) != 0) value += 2;
        if (apply_native_move(board, move, Dc2dmReversi::White, false) != 0) value -= 2;
    }
    return value;
}

int native_alternate_delta(const Dc2dmReversi::Board& before, const Dc2dmReversi::Board& after, int move) {
    int value = 0;
    for (int square = 0; square < 64; ++square) {
        if (before[square] == after[square]) continue;
        const int x = square % 8, y = square / 8;
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            const int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8 && after[ny * 8 + nx] == Dc2dmReversi::Empty)
                ++value;
        }
    }
    const int x = move % 8, y = move / 8;
    if ((x == 1 || x == 6) && (y == 1 || y == 6)) value -= 100 * after[x * 8 + y];
    return value;
}

// 0x43E7C0 places the CPU color during every search ply; its animation path leaves flips unapplied.
int native_alternate_search(Dc2dmReversi::Board& board, Dc2dmReversi::Cell cpu, Dc2dmReversi::Cell side,
                            int depth, int alpha, int beta, bool passed, int delta, int table, int* selected) {
    if (depth == 0) return native_alternate_value(board, delta);
    bool found = false;
    if (selected) *selected = -1;
    for (int x = 0; x < 8; ++x) for (int y = 0; y < 8; ++y) {
        const int move = y * 8 + x;
        if (apply_native_move(board, move, side, false) == 0) continue;
        if (!found && selected) *selected = move;
        found = true;
        auto next = board;
        if (apply_native_move(next, move, cpu, false) != 0) next[move] = cpu;
        const int change = native_alternate_delta(board, next, move);
        const int value = -native_alternate_search(next, cpu, Dc2dmReversi::opponent(side), depth - 1,
                                                   -beta, -alpha, false, change, table, nullptr);
        if (value > alpha) {
            alpha = value;
            if (selected) *selected = move;
            if (alpha >= beta) return alpha;
        }
    }
    if (found) return alpha;
    if (passed) {
        int value = 0;
        for (int i = 0; i < 64; ++i) if (board[i] == side) value += native_weights[table][i];
        return value;
    }
    return -native_alternate_search(board, cpu, Dc2dmReversi::opponent(side), depth,
                                    -beta, -alpha, true, delta, table, nullptr);
}

int evaluate(const Dc2dmReversi& game, Dc2dmReversi::Cell color) {
    const auto other = Dc2dmReversi::opponent(color);
    const int difference = game.count(color) - game.count(other);
    if (game.finished()) return difference == 0 ? 0 : (difference > 0 ? 100000 : -100000) + difference;
    int value = 0;
    for (int i = 0; i < 64; ++i) {
        if (game.board()[i] == color) value += weights[i];
        else if (game.board()[i] == other) value -= weights[i];
    }
    value += 8 * (static_cast<int>(game.legal_moves(color).size()) -
                  static_cast<int>(game.legal_moves(other).size()));
    if (game.count(Dc2dmReversi::Empty) < 16) value += 4 * difference;
    return value;
}

int search(const Dc2dmReversi& game, Dc2dmReversi::Cell color, int depth, int alpha, int beta) {
    if (depth <= 0 || game.finished()) return evaluate(game, color);
    auto moves = game.legal_moves(game.turn());
    if (moves.empty()) {
        auto next = game;
        next.pass();
        return next.finished() ? evaluate(next, color) : search(next, color, depth, alpha, beta);
    }
    std::stable_sort(moves.begin(), moves.end(), [](int a, int b) { return weights[a] > weights[b]; });
    const bool maximize = game.turn() == color;
    int value = maximize ? -1000000 : 1000000;
    for (int move : moves) {
        auto next = game;
        next.play(move);
        const int score = search(next, color, depth - 1, alpha, beta);
        value = maximize ? std::max(value, score) : std::min(value, score);
        if (maximize) alpha = std::max(alpha, value);
        else beta = std::min(beta, value);
        if (alpha >= beta) break;
    }
    return value;
}

}

void Dc2dmReversi::reset() {
    board_.fill(Empty);
    board_[3 + 3 * 8] = board_[4 + 4 * 8] = White;
    board_[4 + 3 * 8] = board_[3 + 4 * 8] = Black;
    turn_ = Black;
    passed_turn_ = Empty;
    finished_ = false;
    first_move_ = -1;
    opening_book_ = true;
}

void Dc2dmReversi::set_board(const Board& board, Cell turn) {
    board_ = board;
    for (auto& cell : board_) if (!valid_color(cell)) cell = Empty;
    turn_ = valid_color(turn) ? turn : Black;
    passed_turn_ = Empty;
    finished_ = legal_moves(Black).empty() && legal_moves(White).empty();
    first_move_ = -1;
    opening_book_ = false;
}

Dc2dmReversi::Cell Dc2dmReversi::cell(int x, int y) const {
    return x >= 0 && x < 8 && y >= 0 && y < 8 ? board_[y * 8 + x] : Empty;
}

int Dc2dmReversi::count(Cell color) const {
    return static_cast<int>(std::count(board_.begin(), board_.end(), color));
}

std::vector<int> Dc2dmReversi::flips(int index, Cell color) const {
    std::vector<int> captured;
    if (index < 0 || index >= 64 || !valid_color(color) || board_[index] != Empty) return captured;
    const int x = index % 8, y = index / 8;
    const Cell other = opponent(color);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            const auto start = captured.size();
            int nx = x + dx, ny = y + dy;
            while (nx >= 0 && nx < 8 && ny >= 0 && ny < 8 && board_[ny * 8 + nx] == other) {
                captured.push_back(ny * 8 + nx);
                nx += dx;
                ny += dy;
            }
            if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8 || board_[ny * 8 + nx] != color)
                captured.resize(start);
        }
    }
    return captured;
}

std::vector<int> Dc2dmReversi::legal_moves(Cell color) const {
    std::vector<int> moves;
    if (!valid_color(color)) return moves;
    for (int i = 0; i < 64; ++i) if (!flips(i, color).empty()) moves.push_back(i);
    return moves;
}

void Dc2dmReversi::advance_turn() {
    turn_ = opponent(turn_);
    if (!legal_moves(turn_).empty()) return;
    const Cell skipped = turn_;
    turn_ = opponent(turn_);
    if (!legal_moves(turn_).empty()) passed_turn_ = skipped;
    else finished_ = true;
}

bool Dc2dmReversi::play(int index) {
    if (finished_) return false;
    const auto captured = flips(index, turn_);
    if (captured.empty()) return false;
    if (opening_book_ && count(Empty) == 60) first_move_ = index;
    board_[index] = turn_;
    for (int position : captured) board_[position] = turn_;
    passed_turn_ = Empty;
    advance_turn();
    return true;
}

bool Dc2dmReversi::pass() {
    if (finished_ || !legal_moves(turn_).empty()) return false;
    passed_turn_ = turn_;
    turn_ = opponent(turn_);
    if (legal_moves(turn_).empty()) finished_ = true;
    return true;
}

int Dc2dmReversi::result_for(Cell color) const {
    if (!valid_color(color)) return 0;
    const int a = count(color), b = count(opponent(color));
    return a == b ? 0 : a > b ? 1 : 2;
}

int Dc2dmReversi::choose_move(Cell color, int depth) const {
    if (finished_ || !valid_color(color)) return -1;
    auto position = *this;
    position.turn_ = color;
    const auto moves = position.legal_moves(color);
    int best = -1, value = std::numeric_limits<int>::min();
    depth = std::max(1, std::min(depth, 5));
    for (int move : moves) {
        auto next = position;
        next.play(move);
        const int score = search(next, color, depth - 1, -1000000, 1000000);
        if (score > value) { value = score; best = move; }
    }
    return best;
}

uint32_t Dc2dmReversi::next_random() {
    random_state_ = random_state_ * 214013u + 2531011u;
    return (random_state_ >> 16) & 0x7fffu;
}

void Dc2dmReversi::set_random_seed(uint32_t seed) {
    random_state_ = seed;
    early_style_ = static_cast<int>(next_random() % 3);
}

int Dc2dmReversi::choose_move_for_level(Cell color, int level) {
    if (finished_ || !valid_color(color)) return -1;
    auto moves = legal_moves(color);
    if (moves.empty()) return -1;
    // Native candidates scan x first, then y, before selecting rand() % count.
    std::sort(moves.begin(), moves.end(), [](int a, int b) {
        return (a % 8) * 8 + a / 8 < (b % 8) * 8 + b / 8;
    });
    if (level <= 0) return moves[next_random() % moves.size()];
    const int occupied = 64 - count(Empty);
    if (level == 1) {
        const int table = occupied >= 45 ? 2 : occupied >= 12 ? 1 : early_style_;
        int best = std::numeric_limits<int>::min();
        for (int move : moves) best = std::max(best, native_weights[table][move]);
        // Native retries random candidates, preserving the distribution of tied squares.
        for (int tries = 0; tries < 9999; ++tries) {
            const int move = moves[next_random() % moves.size()];
            if (native_weights[table][move] == best) return move;
        }
        for (int move : moves) if (native_weights[table][move] == best) return move;
    }
    if (occupied < 45) {
        int corner = -1;
        for (int move : moves)
            if ((move % 8 == 0 || move % 8 == 7) && (move / 8 == 0 || move / 8 == 7)) corner = move;
        if (corner != -1) return corner;
    }
    if (opening_book_ && occupied == 4 && color == Black) return 37;
    if (opening_book_ && occupied == 5 && color == White && first_move_ != -1) {
        int x = next_random() % 2 == 0 ? 5 : 3, y = 5;
        if (first_move_ == 19 || first_move_ == 26) { x = 7 - x; y = 7 - y; }
        if (first_move_ == 19 || first_move_ == 44) std::swap(x, y);
        const int move = y * 8 + x;
        if (std::find(moves.begin(), moves.end(), move) != moves.end()) return move;
    }
    if (occupied <= 12 && next_random() % 100 % 5 == 0) {
        auto position = board_;
        int move = -1;
        native_alternate_search(position, color, color, 6, -9999, 9999, false, 0,
                                occupied >= 12 ? 1 : early_style_, &move);
        if (move != -1) return move;
    }
    const int table = occupied >= 45 ? 2 : occupied >= 12 ? 1 : 0;
    return native_recursive_move(board_, color, 3, table);
}

}
