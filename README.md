# PyChess C++ Engine

C++ rewrite of the Python pychess-engine. Phase 1 of the road to 3000 Elo.

## Status
- Full legal move generation — **perft-verified** to depth 4+ on 4 standard test
  positions (startpos, Kiwipete, Position 3, Position 4): all counts exact.
- Bitboard eval ported from the tuned Python v4 engine (pawn structure, passed
  pawns, rook files, bishop pair, development bonus, tapered king).
- Search: iterative deepening PVS, aspiration windows, transposition table,
  killer moves + history heuristic, null-move pruning, LMR, check extensions,
  quiescence search.
- **~1.2M nodes/s** (vs ~30k in Python) and reaches **depth 10–12** in 3–5 s/move.

## Build (MinGW g++ / any C++20 compiler)
```bash
g++ -O3 -std=c++20 -march=native -o pychess.exe position.cpp board.cpp eval.cpp search.cpp main.cpp
```

## Run
UCI mode: plug into any GUI or the lichess-bot bridge.
```
./pychess.exe
> uci
> position startpos
> go movetime 3000
```

## Roadmap
1. ✅ C++ core with verified movegen + search
2. Head-to-head vs Python engine, then benchmark vs Stockfish Elo ladder
3. Magic bitboards + multithreading
4. NNUE evaluation
