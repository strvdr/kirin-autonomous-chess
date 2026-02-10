# Kirin Chess Engine - Architecture Walkthrough

This document explains the logic and structure of the chess engine (v0.2). The engine is a UCI-compliant chess engine built using **bitboards** and implements standard chess programming techniques.

---

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        UCI INTERFACE                        │
│                   (uciLoop, parsePosition, parseGo)         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                          SEARCH                             │
│         (negamax with alpha-beta, quiescence search)        │
│    ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│    │  Move Order  │  │  Null Move   │  │     LMR      │     │
│    │   (MVV-LVA)  │  │   Pruning    │  │  (Late Move) │     │
│    └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                       EVALUATION                            │
│    (Material + Piece-Square Tables + Pawn Structure)        │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    MOVE GENERATION                          │
│              (Bitboard-based, Magic Bitboards)              │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   BOARD REPRESENTATION                      │
│                   (12 Bitboards + State)                    │
└─────────────────────────────────────────────────────────────┘
```

---

## 1. Board Representation (Lines 21-138)

### Bitboards
The board uses **bitboards** - 64-bit integers where each bit represents a square:

```
  a8=bit0   b8=bit1  ... h8=bit7
  a7=bit8   b7=bit9  ... h7=bit15
  ...
  a1=bit56  b1=bit57 ... h1=bit63
```

**Key data structures:**
```c
U64 bitboards[12];   // One bitboard per piece type (P,N,B,R,Q,K,p,n,b,r,q,k)
U64 occupancy[3];    // white pieces, black pieces, all pieces
int side;            // 0=white, 1=black
int enpassant;       // en passant target square (or no_sq)
int castle;          // 4-bit castling rights (wk|wq|bk|bq)
```

### Piece Encoding
```c
enum { P, N, B, R, Q, K, p, n, b, r, q, k };  // 0-11
//     0  1  2  3  4  5  6  7  8  9 10 11
```
- Uppercase = White pieces (0-5)
- Lowercase = Black pieces (6-11)

### Bit Manipulation Macros (Lines 222-224)
```c
#define setBit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define getBit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define popBit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))
```

---

## 2. Attack Tables & Magic Bitboards (Lines 440-900)

### Why Magic Bitboards?
Sliding pieces (rooks, bishops, queens) can be blocked. Computing their attacks naively is slow. **Magic bitboards** use precomputed lookup tables with "magic" multiplication to instantly retrieve attacks.

### How It Works:

1. **Relevant occupancy bits**: For each square, identify which squares could block the piece
2. **Magic number**: A special constant that, when multiplied with the occupancy, maps to a unique index
3. **Attack table**: Precomputed attacks for every possible blocker configuration

```c
// Getting rook attacks (Line ~920)
static inline U64 getRookAttacks(int square, U64 occupancy) {
    occupancy &= rookMasks[square];           // Mask relevant squares
    occupancy *= rookMagicNums[square];       // Magic multiplication  
    occupancy >>= 64 - rookRelevantBits[square]; // Shift to get index
    return rookAttacks[square][occupancy];    // Lookup attacks
}
```

### Leaper Attacks (Knights & Kings)
These don't need magic - their attacks are fixed and precomputed:
```c
U64 knightAttacks[64];  // Knight attacks from each square
U64 kingAttacks[64];    // King attacks from each square
U64 pawnAttacks[2][64]; // Pawn attacks [side][square]
```

---

## 3. Move Encoding (Lines 950-1000)

Moves are packed into a single 32-bit integer:

```
    0000 0000 0000 0000 0011 1111    source square     (6 bits)
    0000 0000 0000 1111 1100 0000    target square     (6 bits)  
    0000 0000 1111 0000 0000 0000    piece             (4 bits)
    0000 1111 0000 0000 0000 0000    promoted piece    (4 bits)
    0001 0000 0000 0000 0000 0000    capture flag      (1 bit)
    0010 0000 0000 0000 0000 0000    double pawn push  (1 bit)
    0100 0000 0000 0000 0000 0000    en passant        (1 bit)
    1000 0000 0000 0000 0000 0000    castling          (1 bit)
```

**Encoding macro:**
```c
#define encodeMove(source, target, piece, promoted, capture, dpp, enpassant, castling) \
    (source) | (target << 6) | (piece << 12) | (promoted << 16) | \
    (capture << 20) | (dpp << 21) | (enpassant << 22) | (castling << 23)
```

**Decoding macros:**
```c
#define getSource(move)    ((move) & 0x3f)
#define getTarget(move)    (((move) & 0xfc0) >> 6)
#define getPiece(move)     (((move) & 0xf000) >> 12)
#define getPromoted(move)  (((move) & 0xf0000) >> 16)
#define getCapture(move)   ((move) & 0x100000)
#define getDpp(move)       ((move) & 0x200000)
#define getEnpassant(move) ((move) & 0x400000)
#define getCastle(move)    ((move) & 0x800000)
```

---

## 4. Move Generation (Lines 1244-1500)

The `generateMoves()` function creates all pseudo-legal moves for the current position.

### Flow:
1. Loop through each piece type
2. For each piece of that type on the board:
   - Generate attacks/moves using the attack tables
   - Encode each move and add to move list

### Pawn Moves (Most Complex):
- Single push (if square ahead is empty)
- Double push (from starting rank, if both squares empty)
- Captures (diagonal, including en passant)
- Promotions (when reaching 8th/1st rank)

### Castling:
Checks multiple conditions:
- King and rook haven't moved (castling rights)
- Squares between king and rook are empty
- King doesn't pass through check

---

## 5. Make/Unmake Move (Lines 1064-1241)

### `makeMove(move, flag)`
Executes a move on the board:

1. **Copy board state** (for undo)
2. **Move the piece**: Pop from source, set at target
3. **Handle captures**: Remove captured piece from its bitboard
4. **Special moves**:
   - En passant: Remove pawn behind target
   - Castling: Also move the rook
   - Promotion: Replace pawn with promoted piece
   - Double pawn push: Set en passant square
5. **Update castling rights** based on king/rook movement
6. **Update Zobrist hash** incrementally
7. **Legality check**: If king is in check after move, restore and return 0

### `copyBoard()` / `restoreBoard()` Macros
These save/restore the entire board state, enabling efficient undo during search.

---

## 6. Zobrist Hashing (Lines 247-295)

Each position has a nearly-unique 64-bit hash for:
- **Transposition table** lookups
- **Repetition detection**

**Components:**
```c
U64 pieceKeys[12][64];   // Random key for each piece on each square
U64 enpassantKeys[64];   // Random key for en passant square
U64 castlingKeys[16];    // Random key for each castling rights combo
U64 sideKey;             // XOR'd when black to move
```

**Incremental update**: When making a move, XOR out old state, XOR in new state.

---

## 7. Evaluation (Lines 1758-1806)

The `evaluate()` function returns a score in centipawns (100 = 1 pawn advantage).

### Components:

**1. Material Score:**
```c
int materialScore[12] = { 
    100, 300, 350, 500, 1000, 10000,  // White: P, N, B, R, Q, K
   -100,-300,-350,-500,-1000,-10000   // Black: p, n, b, r, q, k
};
```

**2. Piece-Square Tables:**
Bonuses/penalties for piece placement:
- Pawns: Encourage center control, advancement
- Knights: Prefer center, penalize edges
- Bishops: Prefer diagonals, active squares
- Rooks: Prefer open files, 7th rank
- King: Safety in opening (corner), activity in endgame

**3. Pawn Structure:**
- **Double pawns**: Penalty for pawns on same file
- **Isolated pawns**: Penalty for pawns with no friendly pawns on adjacent files
- **Passed pawns**: Bonus for pawns with no enemy pawns blocking advancement

**Return value:** Positive = White advantage, Negative = Black advantage. Negated if black to move (negamax convention).

---

## 8. Search (Lines 2084-2200)

### Negamax with Alpha-Beta Pruning
```c
int negamax(int alpha, int beta, int depth)
```

**Core idea**: Recursively search moves, pruning branches that can't improve the position.

- **alpha**: Best score we can guarantee
- **beta**: Opponent's best guaranteed score
- If we find `score >= beta`, prune (opponent won't allow this)

### Search Enhancements:

**1. Quiescence Search (Lines 2030-2078)**
At depth 0, continue searching captures to avoid horizon effect:
```c
int quiescence(int alpha, int beta)
```

**2. Null Move Pruning (Lines 2112-2131)**
If our position is so good that passing still beats beta, prune:
```c
if(depth >= 3 && !inCheck) {
    // Try passing (null move)
    score = -negamax(-beta, -beta+1, depth - 3);
    if(score >= beta) return beta;
}
```

**3. Late Move Reduction (LMR)**
Moves searched later (less likely to be good) get reduced depth.

**4. Move Ordering (Lines 1940-2010)**
Critical for alpha-beta efficiency:
1. **PV move** (from previous iteration)
2. **Captures** ordered by MVV-LVA (Most Valuable Victim - Least Valuable Attacker)
3. **Killer moves** (quiet moves that caused cutoffs at this ply)
4. **History heuristic** (moves that were good in similar positions)

**5. Transposition Table (Lines 1880-1940)**
Cache of previously searched positions:
```c
typedef struct {
    U64 hashKey;
    int depth;
    int flag;   // EXACT, ALPHA, or BETA bound
    int score;
} tt;
```

**6. Principal Variation (PV) Table**
Triangular table storing the best line found:
```c
int pvTable[maxPly][maxPly];
int pvLength[maxPly];
```

### Iterative Deepening
```c
void searchPosition(int depth) {
    for(int currentDepth = 1; currentDepth <= depth; currentDepth++) {
        score = negamax(-infinity, infinity, currentDepth);
        // Print info and best move
    }
}
```
Benefits: Time management, move ordering from previous iteration.

---

## 9. UCI Protocol (Lines 2290-2550)

The engine communicates with chess GUIs via UCI (Universal Chess Interface).

### Key Commands:

| GUI → Engine | Engine Response |
|--------------|-----------------|
| `uci` | `id name Kirin`, `uciok` |
| `isready` | `readyok` |
| `position startpos` | Sets up starting position |
| `position fen <fen>` | Sets up position from FEN |
| `position ... moves e2e4 e7e5` | Apply moves |
| `go depth 10` | Search to depth 10 |
| `go wtime 60000 btime 60000` | Search with time control |
| `quit` | Exit |

### Output During Search:
```
info score cp 25 depth 8 nodes 123456 time 500 pv e2e4 e7e5 g1f3 b8c6
bestmove e2e4
```

---

## 10. Time Management (Lines 128-183, 2380-2497)

### Variables:
```c
int moveTime;   // Fixed time per move (if set)
int timer;      // Time remaining on clock
int inc;        // Increment per move
int movesToGo;  // Moves until time control
int stopTime;   // Absolute time to stop searching
```

### Logic:
```c
timer /= movesToGo;      // Divide remaining time
timer -= 50;             // Lag compensation
stopTime = startTime + timer + inc;
```

### Interruption:
Every 2048 nodes, check if time expired:
```c
void communicate() {
    if(timeSet && getTimeMS() > stopTime) stopped = 1;
}
```

---

## File Organization Summary

| Lines | Section |
|-------|---------|
| 1-140 | Constants, enums, global state |
| 140-250 | Time control, random numbers, bit operations |
| 250-350 | Zobrist hashing |
| 350-450 | FEN parsing, board printing |
| 450-600 | Magic numbers for sliding pieces |
| 600-950 | Attack table generation |
| 950-1050 | Move encoding/decoding |
| 1050-1250 | Make/unmake move |
| 1250-1500 | Move generation |
| 1500-1750 | Evaluation masks (pawn structure) |
| 1750-1850 | Static evaluation |
| 1850-2030 | Transposition table, move ordering |
| 2030-2300 | Search (quiescence, negamax) |
| 2300-2550 | UCI interface |
| 2550-2582 | Initialization and main() |

---

## Key Algorithms Quick Reference

| Technique | Purpose | Location |
|-----------|---------|----------|
| Magic Bitboards | Fast sliding piece attacks | Lines 458-600 |
| Zobrist Hashing | Position fingerprinting | Lines 247-295 |
| Alpha-Beta | Prune bad branches | Lines 2084-2200 |
| Quiescence | Avoid horizon effect | Lines 2030-2078 |
| Null Move Pruning | Skip bad positions | Lines 2112-2131 |
| MVV-LVA | Order captures | Lines 1826-1841 |
| Killer Heuristic | Order quiet moves | Lines 1845-1846 |
| Transposition Table | Cache positions | Lines 1880-1940 |
| Iterative Deepening | Time management + PV | Lines 2230-2288 |

---

This should help you navigate the codebase! Let me know if you'd like me to dive deeper into any specific section.
