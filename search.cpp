// Search: iterative deepening PVS with aspiration windows, killers, history
#include "search.hpp"
#include "eval.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace pychess {

Searcher::Searcher() {
    std::memset(killers_, -1, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));
    tt_ = std::make_unique<TTEntry[]>(TT_SIZE);
}

bool Searcher::time_up() {
    nodes_++;
    if ((nodes_ & 2047) == 0) {
        double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
        if (el > time_limit_) stop_ = true;
    }
    return stop_;
}

void Searcher::order_moves(const Board& b, std::vector<Move>& moves, int ply, const Move* tt_move, int depth) {
    (void)depth;  // Suppress unused warning
    
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
    if (time_up()) return alpha;  // Return best so far on timeout
    int stand = evaluate(b);
    if (stand >= beta) return beta;
    if (stand > alpha) alpha = stand;
    auto moves = b.generate_moves(true);
    std::vector<Move> caps(std::move(moves));
    order_moves(b, caps, 0, nullptr, 0);
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

int Searcher::alphabeta(Board& b, int depth, int alpha, int beta, int ply) {
    if (time_up()) return 0;
    
    bool in_check = b.in_check(b.side_to_move);
    if (in_check && ply < 20) depth++;   // check extension

    if (depth <= 0)
        return quiesce(b, alpha, beta);

    auto moves = b.generate_moves();
    order_moves(b, moves, ply, nullptr, depth);

    int best = -INF;
    Move best_move{};
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
        if (first) { 
            sc = -alphabeta(b, depth - 1, -beta, -alpha, ply + 1); 
            first = false; 
        }
        else {
            sc = -alphabeta(b, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1);
            if (sc > alpha && sc < beta)
                sc = -alphabeta(b, depth - 1, -beta, -alpha, ply + 1);
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

    return best;
}

Searcher::Result Searcher::search(Board& board, int max_depth, double time_seconds) {
    start_ = std::chrono::steady_clock::now();
    time_limit_ = time_seconds;
    stop_ = false;
    nodes_ = 0;
    std::memset(killers_, -1, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));

    Move best_move_from_root{};
    int best_score = -INF, completed = 0, prev_score = 0;

    for (int depth = 1; depth <= max_depth; depth++) {
        int alpha = -INF, beta = INF;
        
        // Aspiration window: use previous score ± delta for depths >= 4
        if (depth >= 4 && std::abs(prev_score) < MATE / 2 && prev_score != 0) {
            int delta = 40 + depth * 10;
            alpha = prev_score - delta;
            beta = prev_score + delta;
        }
        
        auto moves = board.generate_moves();
        if (moves.empty()) break;
        order_moves(board, moves, 0, nullptr, depth);
        
        // Root search with PVS-like approach
        int score = -INF;
        Move best_move;
        for (size_t i = 0; i < moves.size(); ++i) {
            auto& m = moves[i];
            board.make_move(m);
            int sc;
            if (i == 0) {
                sc = -alphabeta(board, depth - 1, -beta, -alpha, 1);
            } else {
                sc = -alphabeta(board, depth - 1, -alpha - 1, -alpha, 1);
                if (sc > alpha && sc < beta)
                    sc = -alphabeta(board, depth - 1, -beta, -alpha, 1);
            }
            board.unmake_move();

            if (sc > score) { 
                score = sc; 
                best_move = m; 
            }
            if (score > alpha) alpha = score;
            if (alpha >= beta) break;
        }
        
        // Check for aspiration window failure
        if (depth >= 4 && std::abs(prev_score) < MATE / 2 && prev_score != 0) {
            if (score <= alpha || score >= beta) {
                // Window failed - use full window for next iteration
                alpha = -INF;
                beta = INF;
                // Re-search with full window
                score = -INF;
                for (size_t i = 0; i < moves.size(); ++i) {
                    auto& m = moves[i];
                    board.make_move(m);
                    int sc = -alphabeta(board, depth - 1, -INF, INF, 1);
                    board.unmake_move();
                    if (sc > score) { score = sc; best_move = m; }
                }
            }
        }
        
        // Update best result from completed iteration
        if (score > best_score || completed == 0) {
            best_score = score;
            best_move_from_root = best_move;
        }
        
        if (stop_ && completed > 0) break;
        prev_score = score;
        completed = depth;
    }
    return {best_move_from_root, best_score, completed, nodes_};
}

} // namespace pychess