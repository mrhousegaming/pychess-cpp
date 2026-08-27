// Search: iterative deepening PVS + aspiration windows, TT, killers, history,
// null-move pruning, LMR, check extensions, quiescence.
#pragma once
#include "board.hpp"
#include <chrono>
#include <array>
#include <cstdint>

namespace pychess {

class Searcher {
public:
    struct Result { Move best; int score; int depth; uint64_t nodes; };

    Searcher();

    Result search(Board& board, int max_depth, double time_seconds);

private:
    static constexpr int TT_EXACT = 0, TT_ALPHA = 1, TT_BETA = 2;
    struct TTEntry {
        uint64_t key = 0;
        int depth = 0;
        int score = 0;
        int flag = 0;
        Move move{};
        uint32_t generation = 0; // generation tag to avoid stale entries
    };
    // Fixed-size TT: 4M entries = ~256MB, power of 2 for fast masking
    static constexpr size_t TT_SIZE = 1 << 22;
    std::array<TTEntry, TT_SIZE> tt_;
    uint64_t tt_mask_ = TT_SIZE - 1;
    uint32_t current_generation_ = 0;
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

    // TT helpers
    TTEntry& tt_probe(uint64_t key);
    void tt_store(uint64_t key, int depth, int score, int flag, Move move);
};

uint64_t zobrist_key(const Board& b);   // in search.cpp

} // namespace pychess