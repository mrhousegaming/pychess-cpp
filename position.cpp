// Attack generation: simple ray-based attacks (not magic bitboards) + zobrist hashing.
#include "position.hpp"
#include "board.hpp"  // for Board type
#include <cstring>

namespace pychess {

std::array<U64, 64> KNIGHT_ATT{}, KING_ATT{};
std::array<std::array<U64, 64>, 2> PAWN_ATT{};

// ---------------- simple attack tables ----------------

static inline bool on_board_xy(int f, int r) { return f >= 0 && f < 8 && r >= 0 && r < 8; }
static inline int sq_of(int f, int r) { return r * 8 + f; }

// Sliding attacks along 4 directions, stopping at blockers in occ.
static const int ROOK_DIRS[4][2] = {{0,1},{0,-1},{1,0},{-1,0}};
static const int BISHOP_DIRS[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};

static U64 sliding_attacks(int sq, U64 occ, const int dirs[4][2]) {
    U64 att = 0;
    int f = sq & 7, r = sq >> 3;
    for (int d = 0; d < 4; d++) {
        int cf = f + dirs[d][0], cr = r + dirs[d][1];
        while (on_board_xy(cf, cr)) {
            int s = sq_of(cf, cr);
            att |= 1ULL << s;
            if ((occ >> s) & 1) break;
            cf += dirs[d][0]; cr += dirs[d][1];
        }
    }
    return att;
}

void init_attack_tables() {
    const int kn[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
    const int kg[8][2] = {{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
    for (int sq = 0; sq < 64; sq++) {
        int f = sq & 7, r = sq >> 3;
        U64 nk = 0, ng = 0;
        for (int i = 0; i < 8; i++) {
            if (on_board_xy(f + kn[i][0], r + kn[i][1])) nk |= 1ULL << sq_of(f + kn[i][0], r + kn[i][1]);
            if (on_board_xy(f + kg[i][0], r + kg[i][1])) ng |= 1ULL << sq_of(f + kg[i][0], r + kg[i][1]);
        }
        KNIGHT_ATT[sq] = nk;
        KING_ATT[sq] = ng;
        U64 wp = 0, bp = 0;
        if (on_board_xy(f - 1, r + 1)) wp |= 1ULL << sq_of(f - 1, r + 1);
        if (on_board_xy(f + 1, r + 1)) wp |= 1ULL << sq_of(f + 1, r + 1);
        if (on_board_xy(f - 1, r - 1)) bp |= 1ULL << sq_of(f - 1, r - 1);
        if (on_board_xy(f + 1, r - 1)) bp |= 1ULL << sq_of(f + 1, r - 1);
        PAWN_ATT[WHITE][sq] = wp;
        PAWN_ATT[BLACK][sq] = bp;
    }
    // Note: magic bitboards would be initialized here, but using slow ray method for now
}

U64 rook_attacks(int sq, U64 occ) {
    return sliding_attacks(sq, occ, ROOK_DIRS);
}
U64 bishop_attacks(int sq, U64 occ) {
    return sliding_attacks(sq, occ, BISHOP_DIRS);
}
U64 queen_attacks(int sq, U64 occ) {
    return rook_attacks(sq, occ) | bishop_attacks(sq, occ);
}

// ---------------- zobrist ----------------
static U64 ZPIECE[2][6][64];
static U64 ZCASTLE[16];
static U64 ZEP[8];
static U64 ZSIDE;
static bool zobrist_ready = false;

static void init_zobrist() {
    if (zobrist_ready) return;
    zobrist_ready = true;
    U64 s = 0x9E3779B97F4A7C15ULL;
    auto rnd = [&]() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; };
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++)
            for (int q = 0; q < 64; q++)
                ZPIECE[c][p][q] = rnd();
    for (int i = 0; i < 16; i++) ZCASTLE[i] = rnd();
    for (int i = 0; i < 8; i++) ZEP[i] = rnd();
    ZSIDE = rnd();
}

U64 zobrist_key(const Board& b) {
    init_zobrist();
    U64 h = 0;
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++) {
            U64 bb = b.pieces[p] & b.occ[c];
            while (bb) {
                int sq = __builtin_ctzll(bb); bb &= bb - 1;
                h ^= ZPIECE[c][p][sq];
            }
        }
    h ^= ZCASTLE[b.castling & 15];
    if (b.ep_square >= 0) h ^= ZEP[b.ep_square & 7];
    if (b.side_to_move == BLACK) h ^= ZSIDE;
    return h;
}

} // namespace pychess