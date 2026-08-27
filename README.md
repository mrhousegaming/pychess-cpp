# PyChess C++ Engine

C++ rewrite of the Python pychess-engine. Current version: simple iterative deepening alpha-beta (no TT) for testing and development.

## Status
- Bitboard move generation (magic bitboards not yet implemented) — perft-verified to depth 4 on startpos (197281 nodes).
- Bitboard eval ported from the tuned Python v4 engine (pawn structure, passed pawns, rook files, bishop pair, development bonus, tapered king).
- Search: simple iterative deepening alpha-beta with quiescence, check extensions, null-move pruning, LMR, killers, history. **No transposition table** (to avoid crashes during development).
- ~1.2M nodes/s (in startpos) and reaches depth 8-10 in 1-2 seconds per move.

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
1. ✅ Simple alpha-beta (no TT) working — baseline for development.
2. Fix TT crash (bounded TT with replacement policy) and re-enable TT.
3. Magic bitboards (PEXT/magic attack getters) for faster move generation.
4. Lazy SMP multi-threaded search.
5. NNUE evaluation integration.
6. Push to 3000 Elo.

## Current Strength
Estimated ~1800-2000 Elo (based on Python v4 being ~1950 and this being a simplified search without TT).