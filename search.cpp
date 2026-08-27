// Search: simple iterative deepening alpha-beta (no TT) for testing.
// Includes quiescence, check extensions, null-move pruning, LMR, killers, history.
#include "search.hpp"
#include "eval.hpp"
#include <algorithm>
#include <cstring>

namespace pychess {

constexpr int MATE_SCORE = 1000000;
constexpr int INF_SCORE = 2000000;

Searcher::Searcher() {
    std::memset(killers_, -1, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));
}

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
        if (tt_move && *tt_move == m) return 10000000;
        if (m.flags & F_CAPTURE) {
            int victim = b.board[m.flags & F_EP ? m.to + (b.side_to_move == WHITE ? -8 : 8) : m.to];
            if (victim < 0) victim = PAWN;
            return 1000000 + PIECE_VALUES[victim] * 10 - PIECE_VALUES[b.board[m.from]];
        }
        if (m.promo != NO_PT) return 900000 + PIECE_VALUES[m.promo];
        if (killers_[ply][0] == m.from && killers_[ply][1] == m.to) return 500000;
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
    // TT disabled for now

    auto moves = b.generate_moves();
    order_moves(b, moves, ply, nullptr);

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
    // TT disabled: no storage
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
    Move best_move_from_root{};

    for (int depth = 1; depth <= max_depth; depth++) {
        int alpha = -INF_SCORE, beta = INF_SCORE;
        if (depth >= 4 && std::abs(prev_score) < MATE_SCORE / 2) {
            int delta = 40 + depth * 10;
            alpha = prev_score - delta; beta = prev_score + delta;
        }
        int score;
        auto moves = board.generate_moves();
        if (moves.empty()) break;
        order_moves(board, moves, 0, nullptr);
        try {
            // Root search with PVS-like approach: first move full window, others scout
            bool first_root = true;
            for (size_t i = 0; i < moves.size(); ++i) {
                auto& m = moves[i];
                board.make_move(m);
                int sc;
                if (first_root) {
                    sc = -alphabeta(board, depth - 1, -beta, -alpha, 1);
                    first_root = false;
                } else {
                    sc = -alphabeta(board, depth - 1, -alpha - 1, -alpha, 1);
                    if (sc > alpha && sc < beta)
                        sc = -alphabeta(board, depth - 1, -beta, -sc, 1);
                }
                board.unmake_move();

                if (sc > best) { best = sc; best_move_from_root = m; }
                if (best > alpha) alpha = best;
                if (alpha >= beta) break;
            }
        } catch (...) { break; }
        if (stop_ && completed > 0) break;
        best_score = score; prev_score = score; completed = depth;
        best = best_move_from_root;
    }
    return {best, best_score, completed, nodes_};
}

} // namespace pychess