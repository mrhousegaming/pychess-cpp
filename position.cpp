// Fix: walk rays from the NEAREST square. For positive-direction rays (N, E, NE, NW)
// the nearest square is the LOWEST bit; for negative (S, W, SE, SW) it's the HIGHEST.
#include "board.hpp"

namespace pychess {

std::array<std::array<U64, 64>, 8> RAYS{};
std::array<U64, 64> KNIGHT_ATT{}, KING_ATT{};
std::array<std::array<U64, 64>, 2> PAWN_ATT{};

static inline bool on_board(int f, int r) { return f >= 0 && f < 8 && r >= 0 && r < 8; }
static inline int sq_of(int f, int r) { return r * 8 + f; }

void init_attack_tables() {
    const int kn[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
    const int kg[8][2] = {{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
    for (int sq = 0; sq < 64; sq++) {
        int f = sq & 7, r = sq >> 3;
        U64 nk = 0, ng = 0;
        for (auto& d : kn) if (on_board(f + d[0], r + d[1])) nk |= 1ULL << sq_of(f + d[0], r + d[1]);
        for (auto& d : kg) if (on_board(f + d[0], r + d[1])) ng |= 1ULL << sq_of(f + d[0], r + d[1]);
        KNIGHT_ATT[sq] = nk;
        KING_ATT[sq] = ng;
        U64 wp = 0, bp = 0;
        if (on_board(f - 1, r + 1)) wp |= 1ULL << sq_of(f - 1, r + 1);
        if (on_board(f + 1, r + 1)) wp |= 1ULL << sq_of(f + 1, r + 1);
        if (on_board(f - 1, r - 1)) bp |= 1ULL << sq_of(f - 1, r - 1);
        if (on_board(f + 1, r - 1)) bp |= 1ULL << sq_of(f + 1, r - 1);
        PAWN_ATT[WHITE][sq] = wp;
        PAWN_ATT[BLACK][sq] = bp;
    }
}

enum RayDir { RD_N = 0, RD_S = 1, RD_E = 2, RD_W = 3, RD_NE = 4, RD_NW = 5, RD_SE = 6, RD_SW = 7 };

static std::array<std::array<U64, 64>, 8> RAYS_INTERNAL{};
static bool rays_done = false;

// df/dr per ray index
static const int RAY_DF[8] = {0, 0, 1, -1, 1, -1, 1, -1};
static const int RAY_DR[8] = {1, -1, 0, 0, 1, 1, -1, -1};

static void init_rays() {
    if (rays_done) return;
    rays_done = true;
    for (int d = 0; d < 8; d++) {
        for (int sq = 0; sq < 64; sq++) {
            U64 mask = 0;
            int f = sq & 7, r = sq >> 3;
            int cf = f + RAY_DF[d], cr = r + RAY_DR[d];
            while (on_board(cf, cr)) { mask |= 1ULL << sq_of(cf, cr); cf += RAY_DF[d]; cr += RAY_DR[d]; }
            RAYS_INTERNAL[d][sq] = mask;
        }
    }
}

// positive direction: bits increase away from sq → scan lowest-first
// negative direction: bits decrease away from sq → scan highest-first
U64 ray_walk(int ray_idx, int sq, U64 occ) {
    init_rays();
    U64 att = 0;
    U64 ray = RAYS_INTERNAL[ray_idx][sq];
    bool negative = (ray_idx == RD_S || ray_idx == RD_W || ray_idx == RD_SE || ray_idx == RD_SW);
    while (ray) {
        int s = negative ? 63 - __builtin_clzll(ray) : __builtin_ctzll(ray);
        att |= 1ULL << s;
        if ((occ >> s) & 1) break;
        ray &= ~(1ULL << s);
    }
    return att;
}

U64 bishop_attacks(int sq, U64 occ) {
    return ray_walk(RD_NE, sq, occ) | ray_walk(RD_NW, sq, occ)
         | ray_walk(RD_SE, sq, occ) | ray_walk(RD_SW, sq, occ);
}

U64 rook_attacks(int sq, U64 occ) {
    return ray_walk(RD_N, sq, occ) | ray_walk(RD_S, sq, occ)
         | ray_walk(RD_E, sq, occ) | ray_walk(RD_W, sq, occ);
}

U64 queen_attacks(int sq, U64 occ) {
    return bishop_attacks(sq, occ) | rook_attacks(sq, occ);
}

} // namespace pychess
