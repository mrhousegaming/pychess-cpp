// Evaluation — bitboard-based, ported from Python v4 (pawn structure, passed
// pawns, rook files, bishop pair, development bonus, tapered king).
#include "eval.hpp"

namespace pychess {

const int PIECE_VALUES[6] = {100, 320, 330, 500, 900, 0};

// Tables are written visually top-down for black; white mirrors via sq ^ 56.
const int PST[6][64] = {
    // PAWN
    {0,0,0,0,0,0,0,0, 50,50,50,50,50,50,50,50,
     10,10,20,30,30,20,10,10, 5,5,10,25,25,10,5,5,
     0,0,0,20,20,0,0,0, 5,-5,-10,0,0,-10,-5,5,
     5,10,10,-20,-20,10,10,5, 0,0,0,0,0,0,0,0},
    // KNIGHT
    {-50,-40,-30,-30,-30,-30,-40,-50, -40,-20,0,0,0,0,-20,-40,
     -30,0,10,15,15,10,0,-30, -30,5,15,20,20,15,5,-30,
     -30,0,15,20,20,15,0,-30, -30,5,10,15,15,10,5,-30,
     -40,-20,0,5,5,0,-20,-40, -50,-30,-30,-30,-30,-30,-30,-50},
    // BISHOP
    {-20,-10,-10,-10,-10,-10,-10,-20, -10,0,0,0,0,0,0,-10,
     -10,0,5,10,10,5,0,-10, -10,5,5,10,10,5,5,-10,
     -10,0,10,10,10,10,0,-10, -10,10,10,10,10,10,10,-10,
     -10,5,0,0,0,0,5,-10, -20,-10,-10,-10,-10,-10,-10,-20},
    // ROOK
    {0,0,0,0,0,0,0,0, 5,10,10,10,10,10,10,5,
     -5,0,0,0,0,0,0,-5, -5,0,0,0,0,0,0,-5,
     -5,0,0,0,0,0,0,-5, -5,0,0,0,0,0,0,-5,
     -5,0,0,0,0,0,0,-5, 0,0,0,5,5,0,0,0},
    // QUEEN
    {-20,-10,-10,-5,-5,-10,-10,-20, -10,0,0,0,0,0,0,-10,
     -10,0,5,5,5,5,0,-10, -5,0,5,5,5,5,0,-5,
     0,0,5,5,5,5,0,-5, -10,5,5,5,5,5,0,-10,
     -10,0,5,0,0,0,0,-10, -20,-10,-10,-5,-5,-10,-10,-20},
    // KING placeholder (handled by tapered tables below)
    {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0}
};

const int KING_MID_PST[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20, -10,-20,-20,-20,-20,-20,-20,-10,
    20,20,0,0,0,0,20,20, 20,30,10,0,0,10,30,20
};

const int KING_END_PST[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50, -30,-20,-10,0,0,-10,-20,-30,
    -30,-10,20,30,30,20,-10,-30, -30,-10,30,40,40,30,-10,-30,
    -30,-10,30,40,40,30,-10,-30, -30,-10,20,30,30,20,-10,-30,
    -30,-30,0,0,0,0,-30,-30, -50,-30,-30,-30,-30,-30,-30,-50
};

int evaluate(const Board& b) {
    const U64 occ_w = b.occ[WHITE], occ_b = b.occ[BLACK];
    int score = 0;   // white POV

    int n_pawn = __builtin_popcountll(b.pieces[PAWN]);
    int n_knight = __builtin_popcountll(b.pieces[KNIGHT]);
    int n_bishop = __builtin_popcountll(b.pieces[BISHOP]);
    int n_rook = __builtin_popcountll(b.pieces[ROOK]);
    int n_queen = __builtin_popcountll(b.pieces[QUEEN]);
    int phase = std::min(n_pawn + 3 * n_knight + 3 * n_bishop + 5 * n_rook + 12 * n_queen, 24);
    const int ew_num = 24 - phase;
    // fixed-point: mw/ew as parts of 24
    int mw24 = phase, ew24 = ew_num;

    // Material + PST
    for (int pt = PAWN; pt <= QUEEN; pt++) {
        U64 w = b.pieces[pt] & occ_w, bl = b.pieces[pt] & occ_b;
        while (w) { int s = __builtin_ctzll(w); w &= w - 1; score += PIECE_VALUES[pt] + PST[pt][s ^ 56]; }
        while (bl) { int s = __builtin_ctzll(bl); bl &= bl - 1; score -= PIECE_VALUES[pt] + PST[pt][s]; }
    }

    // Tapered king PST
    {
        U64 k = b.pieces[KING];
        U64 kw = k & occ_w;
        if (kw) { int s = __builtin_ctzll(kw); score += (KING_MID_PST[s ^ 56] * mw24 + KING_END_PST[s ^ 56] * ew24) / 24; }
        U64 kb = k & occ_b;
        if (kb) { int s = __builtin_ctzll(kb); score -= (KING_MID_PST[s] * mw24 + KING_END_PST[s] * ew24) / 24; }
    }

    // Bishop pair
    int bw = __builtin_popcountll(b.pieces[BISHOP] & occ_w);
    int bb_ = __builtin_popcountll(b.pieces[BISHOP] & occ_b);
    if (bw >= 2) score += 30;
    if (bb_ >= 2) score -= 30;

    // Pawn files
    int files_w[8] = {0}, files_b[8] = {0};
    int adv_w[8], adv_b[8];   // most advanced rank (toward enemy)
    for (int i = 0; i < 8; i++) { adv_w[i] = -1; adv_b[i] = -1; }
    {
        U64 p = b.pieces[PAWN] & occ_w;
        while (p) { int s = __builtin_ctzll(p); p &= p - 1; int f = s & 7, r = s >> 3;
            files_w[f]++; if (r > adv_w[f]) adv_w[f] = r; }
        p = b.pieces[PAWN] & occ_b;
        while (p) { int s = __builtin_ctzll(p); p &= p - 1; int f = s & 7, r = s >> 3;
            files_b[f]++; if (7 - r > adv_b[f]) adv_b[f] = 7 - r; }
    }
    for (int f = 0; f < 8; f++) {
        if (files_w[f] > 1) score -= 12 * (files_w[f] - 1);
        if (files_b[f] > 1) score += 12 * (files_b[f] - 1);
        if (files_w[f] && (f == 0 || files_w[f-1] == 0) && (f == 7 || files_w[f+1] == 0)) score -= 15;
        if (files_b[f] && (f == 0 || files_b[f-1] == 0) && (f == 7 || files_b[f+1] == 0)) score += 15;
    }
    // Passed pawns: most advanced pawn per file vs enemy pawns on 3 files
    for (int f = 0; f < 8; f++) {
        if (adv_w[f] >= 0) {
            bool passed = true;
            for (int ff = std::max(0, f - 1); ff <= std::min(7, f + 1) && passed; ff++) {
                U64 eb = b.pieces[PAWN] & occ_b & (0xFFULL << (ff * 8));
                // file mask: squares with this file index — use file bits
                eb = b.pieces[PAWN] & occ_b &
                     ((ff == f ? 0x0101010101010101ULL : 0) |
                      (ff != 7 ? 0x0101010101010101ULL << (ff + 1) : 0) |
                      (ff != 0 ? 0x0101010101010101ULL << (ff - 1) : 0));
                while (eb) { int s = __builtin_ctzll(eb); eb &= eb - 1; if ((s >> 3) > adv_w[f]) { passed = false; break; } }
            }
            if (passed) score += 20 + 8 * adv_w[f];
        }
        if (adv_b[f] >= 0) {
            bool passed = true;
            int rr = 7 - adv_b[f];
            for (int ff = std::max(0, f - 1); ff <= std::min(7, f + 1) && passed; ff++) {
                U64 ep_ = b.pieces[PAWN] & occ_w &
                     ((ff != 7 ? 0x0101010101010101ULL << ff : 0) |
                      (ff != 0 ? 0x0101010101010101ULL << ff : 0));
                ep_ = b.pieces[PAWN] & occ_w & (0x0101010101010101ULL << ff);
                while (ep_) { int s = __builtin_ctzll(ep_); ep_ &= ep_ - 1; if ((s >> 3) < rr) { passed = false; break; } }
            }
            if (passed) score -= 20 + 8 * adv_b[f];
        }
    }
    // Rooks on open / semi-open files
    {
        U64 r = b.pieces[ROOK] & occ_w;
        while (r) { int s = __builtin_ctzll(r); r &= r - 1; int f = s & 7;
            if (!files_w[f]) { score += 12; if (!files_b[f]) score += 6; } }
        r = b.pieces[ROOK] & occ_b;
        while (r) { int s = __builtin_ctzll(r); r &= r - 1; int f = s & 7;
            if (!files_b[f]) { score -= 12; if (!files_w[f]) score -= 6; } }
    }
    // Development bonus in opening/middlegame
    if (phase >= 16) {
        auto dev = [&](int color, int sign) {
            int home = color == WHITE ? 0 : 7;
            U64 minors = (b.pieces[KNIGHT] | b.pieces[BISHOP]) & b.occ[color];
            U64 m = minors;
            while (m) { int s = __builtin_ctzll(m); m &= m - 1; if ((s >> 3) != home) score += sign * 6; }
            int ks = __builtin_ctzll(b.pieces[KING] & b.occ[color]);
            if ((ks >> 3) == home) {
                int kf = ks & 7;
                if (kf == 1 || kf == 2 || kf == 5 || kf == 6) score += sign * 15;
            }
        };
        dev(WHITE, 1); dev(BLACK, -1);
    }

    return b.side_to_move == WHITE ? score : -score;
}

} // namespace pychess
