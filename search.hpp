// Search: iterative deepening PVS + aspiration windows, TT, killers, history,
// null-move pruning, LMR, check extensions, quiescence.
#pragma once
#include "board.hpp"
#include <chrono>
#include <unordered_map>

namespace pychess {

class Searcher {
public:
    struct Result { Move best; int score; int depth; uint64_t nodes; };

    Searcher();

    Result search(Board& board, int max_depth, double time_seconds);

private:
    static constexpr int TT_EXACT = 0, TT_ALPHA = 1, TT_BETA = 2;
    struct TTEntry { int depth; int score; int flag; Move move; };
    std::unordered_map<uint64_t, TTEntry> tt_;
    int killers_[MAX_PLY][2]{};
    int history_[2][64][64]{};
    uint64_t nodes_ = 0;
    std::chrono::steady_clock::time_point start_;
    double time_limit_ = 1.0;
    bool stop_ = false;

    bool time_up();
    void order_moves(const Board& b, std::vector<Move>& moves, int ply, const Move* tt_move);
    int quiesce(Board& b, int alpha, int beta);
    int alphabeta(Board& b, int depth, int alpha, int beta, int ply, bool allow_null);
};

uint64_t zobrist_key(const Board& b);   // in search.cpp

} // namespace pychess
