// Search implementation + zobrist.
#include "search.hpp"
#include "eval.hpp"
#include <algorithm>
#include <cstring>

namespace pychess {

// ---------------- zobrist ----------------
static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;
static uint64_t next_rand() {
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17;
    return rng_state;
}
static uint64_t Z_PIECE[2][6][64];
static uint64_t Z_STM, Z_CASTLE[16], Z_EP[64];
static bool z_init = false;

static void init_zobrist() {
    if (z_init) return;
    z_init = true;
    for (int c = 0; c < 2; c++) for (int p = 0; p < 6; p++) for (int s = 0; s < 64; s++)
        Z_PIECE[c][p][s] = next_rand();
    Z_STM = next_rand();
    for (int i = 0; i < 16; i++) Z_CASTLE[i] = next_rand();
    for (int i = 0; i < 64; i++) Z_EP[i] = next_rand();
}

uint64_t zobrist_key(const Board& b) {
    init_zobrist();
    uint64_t h = 0;
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++) {
            U64 bb = b.pieces[p] & b.occ[c];
            while (bb) { int s = __builtin_ctzll(bb); bb &= bb - 1; h ^= Z_PIECE[c][p][s]; }
        }
    if (b.side_to_move == BLACK) h ^= Z_STM;
    h ^= Z_CASTLE[b.castling & 15];
    if (b.ep_square >= 0) h ^= Z_EP[b.ep_square];
    return h;
}

// ---------------- searcher ----------------
Searcher::Searcher() { init_zobrist(); }

bool Searcher::time_up() {
    nodes_++;
    if ((nodes_ & 2047) == 0) {
        double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
        if (el > time_limit_) stop_ = true;
    }
    return stop_;
}

void Searcher::order_moves(const Board& b, std::vector<Move>& moves, int ply, const Move* tt_move) {
    auto score_of = [&](const Move& m) -> int {
        if (tt_move && *tt_move == m) return 10'000'000;
        if (m.flags & F_CAPTURE) {
            int victim = b.board[m.flags & F_EP ? m.to + (b.side_to_move == WHITE ? -8 : 8) : m.to];
            if (victim < 0) victim = PAWN;
            return 1'000'000 + PIECE_VALUES[victim] * 10 - PIECE_VALUES[b.board[m.from]];
        }
        if (m.promo != NO_PT) return 900'000 + PIECE_VALUES[m.promo];
        if (killers_[ply][0] == m.from && killers_[ply][1] == m.to) return 500'000;
        return history_[b.side_to_move][m.from][m.to];
    };
    std::stable_sort(moves.begin(), moves.end(),
                     [&](const Move& a, const Move& c) { return score_of(a) > score_of(c); });
}

int Searcher::quiesce(Board& b, int alpha, int beta) {
    if (time_up()) return 0;
    int stand = evaluate(b);
    if (stand >= beta) return beta;
    if (stand > alpha) alpha = stand;
    auto moves = b.generate_moves(true);
    std::vector<Move> caps(std::move(moves));
    order_moves(b, caps, 0, nullptr);
    for (auto& m : caps) {
        // skip underpromotions in qsearch
        if (m.promo != NO_PT && m.promo != QUEEN) continue;
        b.make_move(m);
        int sc = -quiesce(b, -beta, -alpha);
        b.unmake_move();
        if (sc >= beta) return beta;
        if (sc > alpha) alpha = sc;
    }
    return alpha;
}

int Searcher::alphabeta(Board& b, int depth, int alpha, int beta, int ply, bool allow_null) {
    if (time_up()) return 0;
    bool in_check = b.in_check(b.side_to_move);
    if (in_check && ply < 20) depth++;   // check extension

    if (depth <= 0)
        return quiesce(b, alpha, beta);

    uint64_t key = zobrist_key(b);
    Move tt_move{};
    bool have_tt = false;
    auto it = tt_.find(key);
    if (it != tt_.end()) {
        tt_move = it->second.move; have_tt = true;
        if (it->second.depth >= depth && ply > 0) {
            if (it->second.flag == TT_EXACT) return it->second.score;
            if (it->second.flag == TT_ALPHA && it->second.score <= alpha) return alpha;
            if (it->second.flag == TT_BETA && it->second.score >= beta) return beta;
        }
    }

    // null-move pruning
    if (allow_null && !in_check && depth >= 3 && std::abs(beta) < MATE_SCORE / 2
        && (b.occ[b.side_to_move] & ~b.pieces[PAWN] & ~b.pieces[KING])) {
        Move null{-1, -1};
        // manual null move: flip stm, save ep
        int saved_ep = b.ep_square;
        b.side_to_move ^= 1; b.ep_square = -1;
        int sc = -alphabeta(b, depth - 3, -beta, -beta + 1, ply + 1, false);
        b.side_to_move ^= 1; b.ep_square = saved_ep;
        if (sc >= beta) return beta;
    }

    auto moves = b.generate_moves();
    order_moves(b, moves, ply, have_tt ? &tt_move : nullptr);

    int best = -INF_SCORE;
    Move best_move{};
    int old_alpha = alpha;
    bool first = true;
    int searched = 0;

    for (auto& m : moves) {
        b.make_move(m);
        bool gives_check = b.in_check(b.side_to_move);
        int reduction = 0;
        if (depth >= 3 && searched >= 4 && !(m.flags & F_CAPTURE) && m.promo == NO_PT
            && !in_check && !gives_check)
            reduction = 1;
        int sc;
        if (first) { sc = -alphabeta(b, depth - 1, -beta, -alpha, ply + 1, true); first = false; }
        else {
            sc = -alphabeta(b, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, true);
            if (sc > alpha && (reduction || sc < beta))
                sc = -alphabeta(b, depth - 1, -beta, -alpha, ply + 1, true);
        }
        b.unmake_move();
        searched++;

        if (sc > best) { best = sc; best_move = m; }
        if (best > alpha) alpha = best;
        if (alpha >= beta) {
            if (!(m.flags & F_CAPTURE)) {
                killers_[ply][0] = m.from; killers_[ply][1] = m.to;
                history_[b.side_to_move][m.from][m.to] += depth * depth;
            }
            break;
        }
    }

    int flag = best <= old_alpha ? TT_ALPHA : (best >= beta ? TT_BETA : TT_EXACT);
    tt_[key] = {depth, best, flag, best_move};
    return best;
}

Searcher::Result Searcher::search(Board& board, int max_depth, double time_seconds) {
    start_ = std::chrono::steady_clock::now();
    time_limit_ = time_seconds;
    stop_ = false;
    nodes_ = 0;
    std::memset(killers_, -1, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));

    Move best{};
    int best_score = 0, completed = 0, prev_score = 0;

    for (int depth = 1; depth <= max_depth; depth++) {
        int alpha = -INF_SCORE, beta = INF_SCORE;
        if (depth >= 4 && std::abs(prev_score) < MATE_SCORE / 2) {
            int delta = 40 + depth * 10;
            alpha = prev_score - delta; beta = prev_score + delta;
        }
        int score;
        // full-window root search (simple, correct)
        auto moves = board.generate_moves();
        if (moves.empty()) break;
        order_moves(board, moves, 0, nullptr);
        try {
            score = alphabeta(board, depth, alpha, beta, 0, true);
            if (score <= alpha || score >= beta)
                score = alphabeta(board, depth, -INF_SCORE, INF_SCORE, 0, true);
        } catch (...) { break; }
        if (stop_ && completed > 0) break;
        best_score = score; prev_score = score; completed = depth;
        auto it = tt_.find(zobrist_key(board));
        if (it != tt_.end()) best = it->second.move;
        else if (moves.size()) best = moves[0];
    }
    return {best, best_score, completed, nodes_};
}

} // namespace pychess
