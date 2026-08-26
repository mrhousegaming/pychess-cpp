// Evaluation — ported from the tuned Python v4 engine.
#pragma once
#include "board.hpp"

namespace pychess {

constexpr int MATE_SCORE = 1000000;
constexpr int INF_SCORE = 2000000;

extern const int PIECE_VALUES[6];
extern const int PST[6][64];          // [piece type][square], white POV
extern const int KING_MID_PST[64];
extern const int KING_END_PST[64];

// Returns eval in centipawns from side-to-move's perspective.
int evaluate(const Board& b);

} // namespace pychess
