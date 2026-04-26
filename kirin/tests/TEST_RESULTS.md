# Kirin Chess Engine — Test Results

> Generated: 2026-04-26 13:56 UTC  
> Run `cmake --build build --target test_report` to refresh.

![tests](https://img.shields.io/badge/tests-418%2F418+passing-brightgreen)

## Summary

| Test Suite | Result | Passed | Failed | Time |
|---|---|---|---|---|
| Captured Piece Detection | ✅ | 10 | 0 | 0.00 s |
| Board Interpreter | ✅ | 63 | 0 | 0.00 s |
| Game Controller | ✅ | 260 | 0 | 0.01 s |
| Engine Validity & Skill Level | ✅ | 85 | 0 | 0.22 s |
| **Total** | ✅ | **418** | **0** | — |

## Detail

### ✅ Captured Piece Detection

_Move encoding — captured piece bit-field round-trips for all 12 piece types._

<details>
<summary><b>General &nbsp;—&nbsp; 10/10 passed</b></summary>

- ✅ White pawn captures black pawn (e4xd5)
- ✅ White knight captures black rook (Nf3xe5)
- ✅ Black queen captures white bishop (Qd8xd4)
- ✅ En passant capture stores correct piece
- ✅ Non-capture move has captured piece = 0
- ✅ Promotion capture stores correct captured piece
- ✅ All 12 piece types can be encoded as captured
- ✅ Captured piece doesn't interfere with other flags
- ✅ King capturing knight stores correct piece
- ✅ Bit positions don't overlap with max values

</details>


### ✅ Board Interpreter

_Physical board path planning, relocation/restoration logic, knight routing, edge cases._

<details>
<summary><b>General &nbsp;—&nbsp; 63/63 passed</b></summary>

- ✅ Ng1-f3 should be valid
- ✅ Knight should have path avoiding f2 pawn
- ✅ Nb1-c3 should be valid
- ✅ Nb1-a3 should be valid
- ✅ e2-e4 should be valid
- ✅ e2-e4 should have no blockers
- ✅ e2-e4 path should be 2 squares
- ✅ d2-d3 should be valid
- ✅ d2-d3 path should be 1 square
- ✅ Ke1-e2 should be valid
- ✅ King path should be 1 square
- ✅ Ra1-a8 should be valid with relocations
- ✅ Should have 6 relocations
- ✅ Should have 6 restorations
- ✅ Restorations should be in reverse order
- ✅ Bh1-a8 should be valid
- ✅ Should have at least 1 diagonal blocker
- ✅ Bf6-a1 should be valid (capture)
- ✅ Qd1-d8 should be valid
- ✅ Should have 2 blockers
- ✅ Na1-b3 should be valid (capture)
- ✅ Na1-c2 should be valid (capture)
- ✅ Nh1-f2 should be valid
- ✅ Should either avoid g2 or relocate it
- ✅ Nh1-g3 should be valid
- ✅ Ra1-e1 should be valid with recursive relocation
- ✅ Should have at least 3 blockers
- ✅ Ra1-c1 should be valid
- ✅ Simple a1e1 should be valid
- ✅ Should have exactly 3 relocations
- ✅ Should have exactly 3 restorations
- ✅ Should fail when no parking spots exist
- ✅ Rd4-d8 should be valid
- ✅ Should have 1 blocker (d5)
- ✅ Clear queen move should be valid
- ✅ Should have no blockers
- ✅ Diagonal path should be 4 squares
- ✅ Castling move should be valid
- ✅ Clear castling should have no blockers
- ✅ Blocked castling should still be valid with relocation
- ✅ Should find path for blocker
- ✅ Blocker should have valid path
- ✅ Ra1-e1 with one blocker should be valid
- ✅ should have 1 restoration
- ✅ restoration path should be non-empty
- ✅ restoration returns to original square
- ✅ restoration path endpoint matches destination
- ✅ Ra1-e1 with 3 blockers should be valid
- ✅ should have 3 relocations
- ✅ should have 3 restorations
- ✅ all 3 restoration paths should be non-empty
- ✅ restorations are in reverse relocation order
- ✅ all restoration path endpoints match destinations
- ✅ Ra1-h1 should be valid
- ✅ should have 1 restoration
- ✅ restoration path should not  through h1 (occupied by rook)
- ✅ clear Ra1-h1 should be valid
- ✅ no blockers means no restorations
- ✅ Rd1-d8 with 2 blockers should be valid
- ✅ should have 2 restorations
- ✅ both restoration paths should be non-empty
- ✅ both restoration paths should end at original squares
- ✅ Rate:    100.0%

</details>


### ✅ Game Controller

_Integration layer: coordinate conversion, piece conversion, PhysicalBoard sync, parseBoardMove, isGameOver, hardware-gated functions._

<details>
<summary><b>EngineMove decoding &nbsp;—&nbsp; 13/13 passed</b></summary>

- ✅ source square e2 (52)
- ✅ target square e4 (36)
- ✅ piece is White Pawn
- ✅ no promotion
- ✅ no capture flag
- ✅ double pawn push flag
- ✅ no en-passant flag
- ✅ no castle flag
- ✅ capture flag set
- ✅ captured piece is black pawn
- ✅ en-passant flag set
- ✅ castle flag set
- ✅ promotion piece is Queen

</details>

<details>
<summary><b>Coordinate conversion &nbsp;—&nbsp; 69/69 passed</b></summary>

- ✅ a8 (sq 0)  → row 0, col 0
- ✅ h8 (sq 7)  → row 0, col 7
- ✅ a1 (sq 56) → row 7, col 0
- ✅ h1 (sq 63) → row 7, col 7
- ✅ e4 (sq 36) → row 4, col 4
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare
- ✅ round-trip squareToBoardCoord / boardCoordToSquare

</details>

<details>
<summary><b>Piece type conversion &nbsp;—&nbsp; 28/28 passed</b></summary>

- ✅ P  → PAWN
- ✅ N  → KNIGHT
- ✅ B  → BISHOP
- ✅ R  → ROOK
- ✅ Q  → QUEEN
- ✅ K  → KING
- ✅ p  → PAWN
- ✅ n  → KNIGHT
- ✅ b  → BISHOP
- ✅ r  → ROOK
- ✅ q  → QUEEN
- ✅ k  → KING
- ✅ PAWN   white → P
- ✅ KNIGHT white → N
- ✅ BISHOP white → B
- ✅ ROOK   white → R
- ✅ QUEEN  white → Q
- ✅ KING   white → K
- ✅ PAWN   black → p
- ✅ QUEEN  black → q
- ✅ KING   black → k
- ✅ UNKNOWN → -1
- ✅ P  is white
- ✅ K  is white
- ✅ p  is black
- ✅ k  is black
- ✅ piece 5 (K) is white
- ✅ piece 6 (p) is black

</details>

<details>
<summary><b>PhysicalBoard sync with engine &nbsp;—&nbsp; 64/64 passed</b></summary>

- ✅ rank 8 occupied after sync
- ✅ rank 7 occupied after sync
- ✅ rank 2 occupied after sync
- ✅ rank 1 occupied after sync
- ✅ rank 8 occupied after sync
- ✅ rank 7 occupied after sync
- ✅ rank 2 occupied after sync
- ✅ rank 1 occupied after sync
- ✅ rank 8 occupied after sync
- ✅ rank 7 occupied after sync
- ✅ rank 2 occupied after sync
- ✅ rank 1 occupied after sync
- ✅ rank 8 occupied after sync
- ✅ rank 7 occupied after sync
- ✅ rank 2 occupied after sync
- ✅ rank 1 occupied after sync
- ✅ rank 8 occupied after sync
- ✅ rank 7 occupied after sync
- ✅ rank 2 occupied after sync
- ✅ rank 1 occupied after sync
- ✅ rank 8 occupied after sync
- ✅ rank 7 occupied after sync
- ✅ rank 2 occupied after sync
- ✅ rank 1 occupied after sync
- ✅ rank 8 occupied after sync
- ✅ rank 7 occupied after sync
- ✅ rank 2 occupied after sync
- ✅ rank 1 occupied after sync
- ✅ rank 8 occupied after sync
- ✅ rank 7 occupied after sync
- ✅ rank 2 occupied after sync
- ✅ rank 1 occupied after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync
- ✅ middle ranks empty after sync

</details>

<details>
<summary><b>PieceTracker survives syncWithEngine &nbsp;—&nbsp; 8/8 passed</b></summary>

- ✅ e2e4 found for tracker persistence
- ✅ e2e4 applied to engine state
- ✅ white e-pawn remains tracked on e4 after sync
- ✅ e4 square retains e-pawn identity after sync
- ✅ g8f6 found for tracker persistence
- ✅ g8f6 applied to engine state
- ✅ black g-knight remains tracked on f6 after sync
- ✅ white e-pawn identity survives later sync

</details>

<details>
<summary><b>Tracker validity &nbsp;—&nbsp; 3/3 passed</b></summary>

- ✅ tracker starts unknown before explicit initialization
- ✅ tracker becomes exact after starting-position init
- ✅ tracker invalidation clears exact identity state

</details>

<details>
<summary><b>Engine capture requires exact tracker &nbsp;—&nbsp; 3/3 passed</b></summary>

- ✅ d4xe5 found for engine capture test
- ✅ engine capture succeeds with exact tracker state
- ✅ engine capture is rejected with unknown tracker state

</details>

<details>
<summary><b>Capture G-code exact slot routing &nbsp;—&nbsp; 3/3 passed</b></summary>

- ✅ capture starts from occupied board square e5
- ✅ captured black e-pawn routes to SLOT_P5
- ✅ capture path does not fall back to SLOT_P1

</details>

<details>
<summary><b>Gantry rejects slotless capture &nbsp;—&nbsp; 1/1 passed</b></summary>

- ✅ capture execution fails without exact storage slot

</details>

<details>
<summary><b>GRBL timing and motion setup &nbsp;—&nbsp; 7/7 passed</b></summary>

- ✅ 100ms dwell is encoded as GRBL seconds
- ✅ 750ms dwell is encoded as GRBL seconds
- ✅ motion setup has four commands
- ✅ motion setup selects inches
- ✅ motion setup selects absolute positioning
- ✅ motion setup selects units-per-minute feed
- ✅ motion setup leaves magnet off

</details>

<details>
<summary><b>parseBoardMove &nbsp;—&nbsp; 15/15 passed</b></summary>

- ✅ e2e4 found in move list
- ✅ e2e4 source is e2
- ✅ e2e4 target is e4
- ✅ e2e4 piece is White Pawn
- ✅ e2e4 double-pawn-push flag
- ✅ g1f3 found in move list
- ✅ g1f3 piece is White Knight
- ✅ e2e5 not a legal pawn move
- ✅ a1a1 no-op not legal
- ✅ short garbage string returns 0
- ✅ out-of-range rank returns 0
- ✅ e7e8q promotion found
- ✅ e7e8q promoted to Queen
- ✅ e7e8r promotion found
- ✅ e7e8r promoted to Rook

</details>

<details>
<summary><b>isGameOver &nbsp;—&nbsp; 4/4 passed</b></summary>

- ✅ starting position not game over
- ✅ Scholar's mate is game over
- ✅ stalemate is game over
- ✅ normal mid-game not game over

</details>

<details>
<summary><b>Game flow state &nbsp;—&nbsp; 7/7 passed</b></summary>

- ✅ not in progress before startGame
- ✅ in progress after startGame
- ✅ engine plays white, white to move
- ✅ engine plays black, white to move
- ✅ not in progress after stopGame
- ✅ engine plays black, black to move
- ✅ engine plays white, black to move

</details>

<details>
<summary><b>Hardware-gated functions (no hardware) &nbsp;—&nbsp; 8/8 passed</b></summary>

- ✅ not connected without hardware
- ✅ executeEngineMove returns false without hardware
- ✅ executeCastling returns false without hardware
- ✅ executeEnPassant returns false without hardware
- ✅ executePromotion returns false without hardware
- ✅ connectHardware bad port returns false
- ✅ homeGantry returns false without hardware
- ✅ setupNewGame returns false without hardware

</details>

<details>
<summary><b>engineToPhysicalMove &nbsp;—&nbsp; 9/9 passed</b></summary>

- ✅ e2 from → row 6, col 4
- ✅ e4 to   → row 4, col 4
- ✅ piece type is PAWN
- ✅ not a capture
- ✅ capture move piece type is PAWN
- ✅ capture flag propagated
- ✅ knight move piece type is KNIGHT
- ✅ g1 from → row 7, col 6
- ✅ f3 to   → row 5, col 5

</details>

<details>
<summary><b>Restoration paths (engine -> physical pipeline) &nbsp;—&nbsp; 18/18 passed</b></summary>

- ✅ Ng1-f3 from start valid
- ✅ restoration count matches relocation count
- ✅ all restoration paths populated for Ng1-f3
- ✅ Ra1-e1 with 2 blockers valid
- ✅ 2 relocations (b1,c1)
- ✅ 2 restorations (b1,c1)
- ✅ both restoration paths non-empty
- ✅ restoration path endpoints match original squares
- ✅ Ra1-h1 with g1 blocker valid
- ✅ 1 restoration for g1
- ✅ restoration path avoids h1 (rook is there now)
- ✅ e2-e4 from start valid
- ✅ e2-e4 has no relocations
- ✅ e2-e4 has no restorations
- ✅ Ra1xa5 capture through 3 blockers valid
- ✅ 3 blockers (a2,a3,a4)
- ✅ 3 restorations
- ✅ no restoration targets a5 (captured piece was there)

</details>


### ✅ Engine Validity & Skill Level

_Perft node counts (4 positions × up to depth 4), evaluation sanity, tactical position structure, skill-level globals, repetition detection._

<details>
<summary><b>Perft (move-generation correctness) &nbsp;—&nbsp; 13/13 passed</b></summary>

- ✅ start pos depth 1 = 20
- ✅ start pos depth 2 = 400
- ✅ start pos depth 3 = 8902
- ✅ start pos depth 4 = 197281
- ✅ kiwipete depth 1 = 48
- ✅ kiwipete depth 2 = 2039
- ✅ kiwipete depth 3 = 97862
- ✅ pos3 depth 1 = 14
- ✅ pos3 depth 2 = 191
- ✅ pos3 depth 3 = 2812
- ✅ pos5 depth 1 = 44
- ✅ pos5 depth 2 = 1486
- ✅ pos5 depth 3 = 62379

</details>

<details>
<summary><b>Evaluation sanity &nbsp;—&nbsp; 7/7 passed</b></summary>

- ✅ starting position evaluates near 0 (|score| < 100cp)
- ✅ white queen vs bare king: eval > +800cp (white to move)
- ✅ black queen vs bare king: eval < -800cp (white to move)
- ✅ symmetric K+R vs k+r evaluates near 0 (|score| < 200cp)
- ✅ K+Q scores higher than K+R
- ✅ K+R scores higher than K+P
- ✅ single pawn up is positive for side to move

</details>

<details>
<summary><b>NNUE scaffold &nbsp;—&nbsp; 22/22 passed</b></summary>

- ✅ initAll() initializes the built-in NNUE bootstrap network
- ✅ setUseNNUE(true) enables NNUE evaluation mode
- ✅ NNUE blend defaults to 0 percent for conservative competitive play
- ✅ NNUE mode with blend 0 preserves classical evaluation
- ✅ NNUE Blend 50 mixes classical and NNUE scores evenly
- ✅ blended NNUE starting position evaluates near 0 (|score| < 100cp)
- ✅ NNUE Blend 100 routes evaluate() to pure evaluateNNUE()
- ✅ NNUE residual mode adds a scaled NNUE correction to classical evaluation
- ✅ NNUE evaluation is relative to side to move
- ✅ NNUE bootstrap scores K+Q higher than K+R
- ✅ NNUE bootstrap scores K+R higher than K+P
- ✅ NNUE bootstrap scores a single pawn as positive
- ✅ setUseNNUE(false) restores classical evaluation mode
- ✅ classical evaluation remains callable after NNUE mode is disabled
- ✅ test harness writes a valid Kirin NNUE file
- ✅ loadNNUE() accepts a valid Kirin NNUE file
- ✅ hasExternalNNUE() reports a loaded external network
- ✅ getNNUESource() reports the loaded NNUE file path
- ✅ loaded NNUE network produces the expected side-relative score
- ✅ loadNNUE() rejects missing files
- ✅ failed NNUE loads leave the active network unchanged
- ✅ resetNNUEToBootstrap() restores the built-in fallback network

</details>

<details>
<summary><b>Tactical position structure &nbsp;—&nbsp; 18/18 passed</b></summary>

- ✅ checkmate (Fool's Mate): 0 legal moves for white
- ✅ checkmate (Fool's Mate): white king is in check
- ✅ checkmate (Scholar's Mate): 0 legal moves for black
- ✅ checkmate (Scholar's Mate): black king is in check
- ✅ stalemate: 0 legal moves for black
- ✅ stalemate: black king is NOT in check
- ✅ forced win: Rd1xd5 (takes free queen) is in the legal move list
- ✅ pin: Be2-d3 is illegal (bishop is pinned to the king)
- ✅ pin: Be2-f3 is illegal (bishop is pinned to the king)
- ✅ parseMove rejects pseudo-legal pinned moves
- ✅ en passant: e5xd6 is in the legal move list
- ✅ promotion: a7-a8=Q is legal
- ✅ promotion: a7-a8=R is legal
- ✅ promotion: a7-a8=B is legal
- ✅ promotion: a7-a8=N is legal
- ✅ promotion capture stores the captured piece for move ordering
- ✅ castling: white kingside O-O is legal
- ✅ castling: white queenside O-O-O is legal

</details>

<details>
<summary><b>Skill level &nbsp;—&nbsp; 18/18 passed</b></summary>

- ✅ skillLevel = 0 reads back correctly
- ✅ skillLevel = 1 reads back correctly
- ✅ skillLevel = 2 reads back correctly
- ✅ perft(3) = 8902 (depth-3 baseline)
- ✅ depth 3 visits fewer nodes than depth 4
- ✅ depth 4 visits fewer nodes than depth 5
- ✅ skill 0, fen 0: no legal move leaves king in check
- ✅ skill 0, fen 1: no legal move leaves king in check
- ✅ skill 0, fen 2: no legal move leaves king in check
- ✅ skill 0, fen 3: no legal move leaves king in check
- ✅ skill 1, fen 0: no legal move leaves king in check
- ✅ skill 1, fen 1: no legal move leaves king in check
- ✅ skill 1, fen 2: no legal move leaves king in check
- ✅ skill 1, fen 3: no legal move leaves king in check
- ✅ skill 2, fen 0: no legal move leaves king in check
- ✅ skill 2, fen 1: no legal move leaves king in check
- ✅ skill 2, fen 2: no legal move leaves king in check
- ✅ skill 2, fen 3: no legal move leaves king in check

</details>

<details>
<summary><b>Repetition detection &nbsp;—&nbsp; 3/3 passed</b></summary>

- ✅ parsePosition records the pre-move hash before incrementing repetitionIndex
- ✅ isRepeating() returns 1 after current hash is recorded
- ✅ isRepeating() returns 0 for an unseen position

</details>

<details>
<summary><b>Search API &nbsp;—&nbsp; 4/4 passed</b></summary>

- ✅ communicate() does not read stdin when UCI input polling is disabled
- ✅ searchPosition() completes under CTest without stdin side effects
- ✅ searchPosition() returns a legal best move through the public API
- ✅ searchPosition() stores the root best move in the transposition table

</details>


---

_Kirin Chess Engine · Colorado School of Mines ProtoFund 2026_
