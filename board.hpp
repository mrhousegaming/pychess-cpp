// Move representation + generation + make/unmake for PyChess C++.
#pragma once
#include "position.hpp"
#include <vector>

namespace pychess {

struct Move {
    int from = -1, to = -1;
    int promo = NO_PT;      // promotion piece type or NO_PT
    int flags = 0;          // 1=capture 2=double-push 4=ep 8=castle
    bool operator==(const Move& o) const { return from == o.from && to == o.to && promo == o.promo; }
};

constexpr int F_CAPTURE = 1, F_DOUBLE = 2, F_EP = 4, F_CASTLE = 8;

class Board : public Position {
public:
    // undo stack entry
    struct Undo { Move move; int captured = -1; int cap_color = WHITE; int castling = 0; int ep = -1; int halfmove = 0; };

    std::vector<Undo> undo_stack;

    static Board from_fen(const std::string& fen);
    std::string to_fen() const;

    [[nodiscard]] U64 all() const { return occ[WHITE] | occ[BLACK]; }
    [[nodiscard]] U64 attackers_to(int sq, int by_color) const;
    [[nodiscard]] bool in_check(int color) const;

    std::vector<Move> generate_moves(bool captures_only = false) const;
    void make_move(const Move& m);
    void unmake_move();

private:
    void add_pawn_moves(std::vector<Move>& out, bool captures_only) const;
};

} // namespace pychess
