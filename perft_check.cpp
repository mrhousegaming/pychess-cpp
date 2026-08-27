#include "board.hpp"
#include <iostream>
using namespace pychess;
uint64_t perft(Board& b, int d) {
    if (d == 0) return 1;
    auto moves = b.generate_moves();
    if (d == 1) return moves.size();
    uint64_t n = 0;
    for (auto& m : moves) { b.make_move(m); n += perft(b, d - 1); b.unmake_move(); }
    return n;
}
int main() {
    init_attack_tables();
    Board b = Board::from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    std::cout << "perft(1)=" << perft(b,1) << " (20)\n";
    std::cout << "perft(2)=" << perft(b,2) << " (400)\n";
    std::cout << "perft(3)=" << perft(b,3) << " (8902)\n";
    std::cout << "perft(4)=" << perft(b,4) << " (197281)\n";
    return 0;
}