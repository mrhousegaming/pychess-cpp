// Move generation, make/unmake, FEN I/O.
#include "board.hpp"
#include <sstream>
#include <iostream>
#include <cctype>

namespace pychess {

int char_to_pt_pub(char c) {
    switch (std::tolower((unsigned char)c)) {
        case 'p': return PAWN; case 'n': return KNIGHT; case 'b': return BISHOP;
        case 'r': return ROOK; case 'q': return QUEEN; case 'k': return KING;
    }
    return NO_PT;
}


Board Board::from_fen(const std::string& fen) {
    Board b{};
    std::istringstream ss(fen);
    std::string board_str, stm, castling, ep, half, full;
    ss >> board_str >> stm >> castling >> ep >> half >> full;
    int f = 0, r = 7;
    for (char c : board_str) {
        if (c == '/') { r--; f = 0; continue; }
        if (isdigit(c)) { f += c - '0'; continue; }
        int color = isupper(c) ? WHITE : BLACK;
        b.put(r * 8 + f, char_to_pt_pub(c), color);
        f++;
    }
    b.side_to_move = (stm == "w") ? WHITE : BLACK;
    for (char c : castling) {
        switch (c) {
            case 'K': b.castling |= 1; break;
            case 'Q': b.castling |= 2; break;
            case 'k': b.castling |= 4; break;
            case 'q': b.castling |= 8; break;
        }
    }
    b.ep_square = (ep.size() == 2) ? (ep[1] - '1') * 8 + (ep[0] - 'a') : -1;
    b.halfmove = half.empty() ? 0 : std::stoi(half);
    b.fullmove = full.empty() ? 1 : std::stoi(full);
    return b;
}

std::string Board::to_fen() const {
    std::string out;
    const char pt_char[] = "pnbrqk";
    for (int r = 7; r >= 0; r--) {
        int empty = 0;
        for (int f = 0; f < 8; f++) {
            int sq = r * 8 + f;
            if (!occupied(sq)) { empty++; continue; }
            if (empty) { out += char('0' + empty); empty = 0; }
            char c = pt_char[board[sq]];
            out += board_color[sq] == WHITE ? toupper(c) : c;
        }
        if (empty) out += char('0' + empty);
        if (r) out += '/';
    }
    out += side_to_move == WHITE ? " w " : " b ";
    std::string cs;
    if (castling & 1) cs += 'K';
    if (castling & 2) cs += 'Q';
    if (castling & 4) cs += 'k';
    if (castling & 8) cs += 'q';
    out += cs.empty() ? "-" : cs;
    out += " ";
    if (ep_square >= 0) { out += char('a' + (ep_square & 7)); out += char('1' + (ep_square >> 3)); }
    else out += "-";
    out += " " + std::to_string(halfmove) + " " + std::to_string(fullmove);
    return out;
}

U64 Board::attackers_to(int sq, int by_color) const {
    U64 att = 0;
    U64 them = occ[by_color];
    att |= KNIGHT_ATT[sq] & pieces[KNIGHT] & them;
    att |= KING_ATT[sq] & pieces[KING] & them;
    att |= PAWN_ATT[by_color ^ 1][sq] & pieces[PAWN] & them;   // pawns attack opposite
    U64 allbb = all();
    U64 bq = bishop_attacks(sq, allbb);
    att |= bq & (pieces[BISHOP] | pieces[QUEEN]) & them;
    U64 rq = rook_attacks(sq, allbb);
    att |= rq & (pieces[ROOK] | pieces[QUEEN]) & them;
    return att;
}

bool Board::in_check(int color) const {
    int ksq = __builtin_ctzll(pieces[KING] & occ[color]);
    return attackers_to(ksq, color ^ 1) != 0;
}

void Board::add_pawn_moves(std::vector<Move>& out, bool captures_only) const {
    int us = side_to_move;
    U64 pawns = pieces[PAWN] & occ[us];
    int push = us == WHITE ? 8 : -8;
    int start_rank = us == WHITE ? 1 : 6;
    int promo_rank = us == WHITE ? 7 : 0;

    auto add = [&](Move m) {
        if ((m.to >> 3) == promo_rank && board[m.from] == PAWN) {
            for (int pr : {QUEEN, ROOK, BISHOP, KNIGHT}) {
                Move pm = m; pm.promo = pr;
                if (!captures_only || pm.flags & F_CAPTURE || true)
                    out.push_back(pm);
            }
        } else out.push_back(m);
    };

    while (pawns) {
        int from = __builtin_ctzll(pawns);
        pawns &= pawns - 1;
        // captures
        U64 caps = PAWN_ATT[us][from] & occ[us ^ 1];
        while (caps) {
            int to = __builtin_ctzll(caps); caps &= caps - 1;
            Move m{from, to, NO_PT, F_CAPTURE};
            add(m);
        }
        // en passant
        if (ep_square >= 0 && (PAWN_ATT[us][from] >> ep_square & 1)) {
            Move m{from, ep_square, NO_PT, F_CAPTURE | F_EP};
            if (!captures_only || true) out.push_back(m);
        }
        if (captures_only) continue;
        // pushes
        int to = from + push;
        if (!occupied(to)) {
            add(Move{from, to});
            if ((from >> 3) == start_rank) {
                int to2 = to + push;
                if (!occupied(to2)) out.push_back(Move{from, to2, NO_PT, F_DOUBLE});
            }
        }
    }
}

std::vector<Move> Board::generate_moves(bool captures_only) const {
    std::vector<Move> moves;
    moves.reserve(48);
    int us = side_to_move;
    U64 own = occ[us], enemy = occ[us ^ 1], allbb = all();

    // knights
    U64 kn = pieces[KNIGHT] & own;
    while (kn) {
        int from = __builtin_ctzll(kn); kn &= kn - 1;
        U64 att = KNIGHT_ATT[from] & ~own;
        if (captures_only) att &= enemy;
        while (att) {
            int to = __builtin_ctzll(att); att &= att - 1;
            moves.push_back({from, to, NO_PT, (enemy >> to & 1) ? F_CAPTURE : 0});
        }
    }
    // bishops/queens diagonal
    U64 bi = (pieces[BISHOP] | pieces[QUEEN]) & own;
    while (bi) {
        int from = __builtin_ctzll(bi); bi &= bi - 1;
        U64 att = bishop_attacks(from, allbb) & ~own;
        if (captures_only) att &= enemy;
        while (att) {
            int to = __builtin_ctzll(att); att &= att - 1;
            moves.push_back({from, to, NO_PT, (enemy >> to & 1) ? F_CAPTURE : 0});
        }
    }
    // rooks/queens straight
    U64 rk = (pieces[ROOK] | pieces[QUEEN]) & own;
    while (rk) {
        int from = __builtin_ctzll(rk); rk &= rk - 1;
        U64 att = rook_attacks(from, allbb) & ~own;
        if (captures_only) att &= enemy;
        while (att) {
            int to = __builtin_ctzll(att); att &= att - 1;
            moves.push_back({from, to, NO_PT, (enemy >> to & 1) ? F_CAPTURE : 0});
        }
    }
    // king
    {
        int from = __builtin_ctzll(pieces[KING] & own);
        U64 att = KING_ATT[from] & ~own;
        if (captures_only) att &= enemy;
        while (att) {
            int to = __builtin_ctzll(att); att &= att - 1;
            moves.push_back({from, to, NO_PT, (enemy >> to & 1) ? F_CAPTURE : 0});
        }
        // castling
        if (!captures_only) {
            int opp = us ^ 1;
            bool can_k = (us == WHITE) ? (castling & 1) : (castling & 4);
            bool can_q = (us == WHITE) ? (castling & 2) : (castling & 8);
            int home = us == WHITE ? 4 : 60;
            if (can_k && !(allbb >> (home + 1) & 1) && !(allbb >> (home + 2) & 1)
                && !in_check(us)
                && !attackers_to(home + 1, opp) && !attackers_to(home + 2, opp))
                moves.push_back(Move{home, home + 2, NO_PT, F_CASTLE});
            if (can_q && !(allbb >> (home - 1) & 1) && !(allbb >> (home - 2) & 1) && !(allbb >> (home - 3) & 1)
                && !in_check(us)
                && !attackers_to(home - 1, opp) && !attackers_to(home - 2, opp))
                moves.push_back(Move{home, home - 2, NO_PT, F_CASTLE});
        }
    }

    add_pawn_moves(moves, captures_only);

    // Filter illegal moves (leaving own king in check)
    std::vector<Move> legal;
    legal.reserve(moves.size());
    Board& self = const_cast<Board&>(*this);
    for (const Move& m : moves) {
        self.make_move(m);
        if (!in_check(us)) legal.push_back(m);
        self.unmake_move();
    }
    return legal;
}

void Board::make_move(const Move& m) {
    Undo u;
    u.move = m;
    u.captured = -1;
    u.castling = castling;
    u.ep = ep_square;
    u.halfmove = halfmove;
    int us = side_to_move;

    int moving = board[m.from];
    // captured piece (handle EP separately)
    if (m.flags & F_EP) {
        int cap_sq = m.to + (us == WHITE ? -8 : 8);
        u.captured = PAWN; u.cap_color = us ^ 1;
        remove(cap_sq);
    } else if (occupied(m.to)) {
        u.captured = board[m.to]; u.cap_color = us ^ 1;
        remove(m.to);
    }
    remove(m.from);
    put(m.to, moving, us);

    // promotion
    if (m.promo != NO_PT) {
        remove(m.to);
        put(m.to, m.promo, us);
    }
    // castle rook move
    if (m.flags & F_CASTLE) {
        int home = us == WHITE ? 4 : 60;
        if (m.to > m.from) { remove(home + 3); put(home + 1, ROOK, us); }
        else               { remove(home - 4); put(home - 1, ROOK, us); }
    }
    // update castling rights
    if (moving == KING) castling &= us == WHITE ? ~3 : ~12;
    if (m.from == 0 || m.to == 0) castling &= ~2;
    if (m.from == 7 || m.to == 7) castling &= ~1;
    if (m.from == 56 || m.to == 56) castling &= ~8;
    if (m.from == 63 || m.to == 63) castling &= ~4;

    ep_square = (m.flags & F_DOUBLE) ? (m.from + m.to) / 2 : -1;
    halfmove = (moving == PAWN || u.captured >= 0) ? 0 : halfmove + 1;
    if (us == BLACK) fullmove++;
    side_to_move = us ^ 1;
    undo_stack.push_back(u);
}

void Board::unmake_move() {
    Undo u = undo_stack.back();
    undo_stack.pop_back();
    int us = side_to_move ^ 1;   // side that made the move
    Move m = u.move;

    int moved_pt = board[m.to];
    if (m.promo != NO_PT) moved_pt = PAWN;
    remove(m.to);
    put(m.from, moved_pt, us);
    if (u.captured >= 0) {
        if (m.flags & F_EP) {
            int cap_sq = m.to + (us == WHITE ? -8 : 8);
            put(cap_sq, PAWN, u.cap_color);
        } else put(m.to, u.captured, u.cap_color);
    }
    if (m.flags & F_CASTLE) {
        int home = us == WHITE ? 4 : 60;
        if (m.to > m.from) { remove(home + 1); put(home + 3, ROOK, us); }
        else               { remove(home - 1); put(home - 4, ROOK, us); }
    }
    castling = u.castling;
    ep_square = u.ep;
    halfmove = u.halfmove;
    if (us == BLACK) fullmove--;
    side_to_move = us;
}

} // namespace pychess
