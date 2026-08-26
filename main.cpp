// UCI interface for PyChess C++.
#include "board.hpp"
#include "search.hpp"
#include "eval.hpp"
#include <iostream>
#include <sstream>

using namespace pychess;

namespace pychess { int char_to_pt_pub(char c); }   // defined in board.cpp

static std::string move_to_uci(const Move& m) {
    if (m.from < 0) return "0000";
    std::string s;
    s += char('a' + (m.from & 7)); s += char('1' + (m.from >> 3));
    s += char('a' + (m.to & 7));   s += char('1' + (m.to >> 3));
    if (m.promo != NO_PT) {
        const char pc[] = "pnbrqk";
        s += pc[m.promo];
    }
    return s;
}

static Move uci_to_move(const Board& b, const std::string& uci) {
    int from = (uci[1] - '1') * 8 + (uci[0] - 'a');
    int to = (uci[3] - '1') * 8 + (uci[2] - 'a');
    Move m{from, to};
    if (uci.size() >= 5) m.promo = char_to_pt_pub(uci[4]);
    // derive flags
    if (b.board[to] >= 0 || ((b.pieces[PAWN] & b.occ[b.side_to_move] >> from & 1)
        && (to & 7) != (from & 7) && !b.occupied(to)))
        m.flags |= F_CAPTURE;
    if (b.board[from] == PAWN && std::abs(to - from) == 16) m.flags |= F_DOUBLE;
    if (b.board[from] == KING && std::abs((to & 7) - (from & 7)) == 2) m.flags |= F_CASTLE;
    return m;
}

int main() {
    init_attack_tables();
    Board board = Board::from_fen(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Searcher searcher;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream ss(line);
        std::string cmd; ss >> cmd;
        if (cmd == "uci") {
            std::cout << "id name PyChess-CPP 0.1\nid author Youssef\nuciok" << std::endl;
        } else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (cmd == "ucinewgame") {
            board = Board::from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            searcher = Searcher();
        } else if (cmd == "position") {
            std::string tok; ss >> tok;
            if (tok == "startpos") {
                board = Board::from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                ss >> tok;   // "moves"
            } else if (tok == "fen") {
                std::string fen, part;
                for (int i = 0; i < 6 && (ss >> part); i++) { fen += part + " "; }
                board = Board::from_fen(fen);
                ss >> tok;
            }
            std::string mv;
            while (ss >> mv) {
                Move m = uci_to_move(board, mv);
                board.make_move(m);
            }
        } else if (cmd == "go") {
            double movetime = 1.0;
            double clock_ms = -1, inc_ms = 0;
            std::string tok;
            while (ss >> tok) {
                if (tok == "movetime") ss >> movetime, movetime /= 1000.0;
                else if (tok == "wtime") { double v; ss >> v; clock_ms = v; }
                else if (tok == "btime") { double v; ss >> v; clock_ms = v; }
                else if (tok == "winc" || tok == "binc") { double v; ss >> v; inc_ms = v; }
                else if (tok == "depth") { ss >> movetime; movetime = 30; }   // depth-limited: generous time
            }
            if (clock_ms >= 0)
                movetime = std::max(0.05, std::min(clock_ms / 40.0 / 1000.0 + 0.8 * inc_ms / 1000.0, 5.0));
            auto r = searcher.search(board, 64, movetime);
            std::cout << "info depth " << r.depth << " score cp " << r.score
                      << " nodes " << r.nodes << " pv " << move_to_uci(r.best) << "\n";
            std::cout << "bestmove " << move_to_uci(r.best) << std::endl;
        } else if (cmd == "quit") break;
    }
}
