// PyChess C++ Engine — bitboard core, movegen, evaluation
// Phase 1 port of the Python engine, targeting 10M+ nodes/s.
#pragma once
#include <cstdint>
#include <string>
#include <array>
#include <vector>

namespace pychess {

using U64 = uint64_t;
constexpr int MAX_PLY = 64;

enum Color : int { WHITE = 0, BLACK = 1 };
enum PieceType : int { PAWN = 0, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_PT };
// Square index: a1=0 ... h8=63 (matches python-chess)

struct Position {
    std::array<U64, 2> occ{};                 // occupancy per color
    std::array<U64, 6> pieces{};              // bitboard per piece type
    std::array<int, 64> board{};              // piece type per square (-1 empty)
    std::array<int, 64> board_color{};        // color per square
    int side_to_move = WHITE;
    int castling = 0;                          // bitmask: 1=WK 2=WQ 4=BK 8=BQ
    int ep_square = -1;                        // en passant target or -1
    int halfmove = 0;
    int fullmove = 1;

    [[nodiscard]] bool occupied(int sq) const {
        return (occ[WHITE] | occ[BLACK]) >> sq & 1;
    }
    void put(int sq, int pt, int color) {
        pieces[pt] |= 1ULL << sq;
        occ[color] |= 1ULL << sq;
        board[sq] = pt;
        board_color[sq] = color;
    }
    void remove(int sq) {
        if (!occupied(sq)) return;
        pieces[board[sq]] &= ~(1ULL << sq);
        occ[board_color[sq]] &= ~(1ULL << sq);
        board[sq] = -1;
    }
};

// ---------------- attack tables (knight/king/pawn precomputed) -------------
extern std::array<U64, 64> KNIGHT_ATT, KING_ATT;
extern std::array<std::array<U64, 64>, 2> PAWN_ATT;   // [color][sq]
void init_attack_tables();

U64 bishop_attacks(int sq, U64 occ);   // magic-based (or plain ray fallback)
U64 rook_attacks(int sq, U64 occ);
U64 queen_attacks(int sq, U64 occ);

} // namespace pychess
