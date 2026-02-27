/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    game_controller_test.cpp - Generates verifiable g-code to be input to the gantry
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*    Test Suite for Kirin Game Controller
*   
*    Tests the integration layer between the chess engine and physical board,
*    covering all logic that does not require connected gantry hardware:
*   
*      - Move encoding/decoding (EngineMove namespace)
*      - Coordinate conversion (squareToBoardCoord / boardCoordToSquare)
*      - Piece type conversion (engineToPhysicalPiece / physicalToEnginePiece)
*      - isWhitePiece
*      - PhysicalBoard state tracking (syncWithEngine, movePiece, clearSquare)
*      - parseBoardMove (move string → engine move)
*      - isGameOver  (checkmate / stalemate / mid-game)
*      - isEngineTurn / startGame / stopGame / isGameInProgress
*      - executeEngineMove / executeCastling / executeEnPassant / executePromotion
*        all gate on gantry.isConnected(), so they return false without hardware —
*        we verify that behaviour explicitly.
*/

#include <cstdio>
#include <cstring>

// Hardware layer (brings in board_interpreter.h / gantry_controller.h)
#include "../src/hardware/game_controller.h"

// Engine headers (for parseFEN, initAll, encodeMove, global state)
#include "../src/engine/engine.h"
#include "../src/engine/position.h"
#include "../src/engine/movegen.h"
#include "../src/engine/attacks.h"

// ─────────────────────────────────────────────────────────────
// Minimal test framework (mirrors board_interpreter_test style)
// ─────────────────────────────────────────────────────────────

#define ANSI_RED    "\x1b[31m"
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_CYAN   "\x1b[36m"
#define ANSI_BOLD   "\x1b[1m"
#define ANSI_RESET  "\x1b[0m"

static int testsRun    = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST_ASSERT(cond, msg)                                              \
    do {                                                                    \
        testsRun++;                                                         \
        if (cond) {                                                         \
            testsPassed++;                                                  \
            printf("  " ANSI_GREEN "✓ %s" ANSI_RESET "\n", msg);           \
        } else {                                                            \
            testsFailed++;                                                  \
            printf("  " ANSI_RED   "✗ %s" ANSI_RESET                       \
                   "  [line %d]\n", msg, __LINE__);                        \
        }                                                                   \
    } while(0)

static void printHeader(const char* name) {
    printf("\n" ANSI_BOLD ANSI_CYAN "── %s ──" ANSI_RESET "\n", name);
}

// ─────────────────────────────────────────────────────────────
// Helper: load a FEN position into the engine
// ─────────────────────────────────────────────────────────────
static void loadFEN(const char* fen) {
    parseFEN(fen);
}

// ─────────────────────────────────────────────────────────────
// 1. EngineMove namespace – bit-packing round-trips
// ─────────────────────────────────────────────────────────────
static void testEngineMoveDecoding() {
    printHeader("EngineMove decoding");

    // Normal pawn push e2→e4: source=e2(52), target=e4(36), piece=P(0),
    // promoted=0, capture=0, dpp=1, enpassant=0, castle=0
    int move = encodeMove(52, 36, P, 0, 0, 1, 0, 0);
    TEST_ASSERT(EngineMove::getSource(move)   == 52,   "source square e2 (52)");
    TEST_ASSERT(EngineMove::getTarget(move)   == 36,   "target square e4 (36)");
    TEST_ASSERT(EngineMove::getPiece(move)    == P,    "piece is White Pawn");
    TEST_ASSERT(EngineMove::getPromoted(move) == 0,    "no promotion");
    TEST_ASSERT(EngineMove::getCapture(move)  == false,"no capture flag");
    TEST_ASSERT(EngineMove::getDpp(move)      == true, "double pawn push flag");
    TEST_ASSERT(EngineMove::getEnpassant(move)== false,"no en-passant flag");
    TEST_ASSERT(EngineMove::getCastle(move)   == false,"no castle flag");

    // Capture: d5 captures e4, piece=P(0), capturedPiece=p(6)
    int capMove = encodeMoveWithCapture(27, 36, P, 0, 1, 0, 0, 0, p);
    TEST_ASSERT(EngineMove::getCapture(capMove)      == true, "capture flag set");
    TEST_ASSERT(EngineMove::getCapturedPiece(capMove)== p,    "captured piece is black pawn");

    // En-passant flag
    int epMove = encodeMove(27, 20, P, 0, 1, 0, 1, 0);
    TEST_ASSERT(EngineMove::getEnpassant(epMove) == true, "en-passant flag set");

    // Castle flag
    int castleMove = encodeMove(60, 62, K, 0, 0, 0, 0, 1);
    TEST_ASSERT(EngineMove::getCastle(castleMove) == true, "castle flag set");

    // Promotion: pawn on e7(12) → e8(4), promotes to Q(4)
    int promoMove = encodeMove(12, 4, P, Q, 0, 0, 0, 0);
    TEST_ASSERT(EngineMove::getPromoted(promoMove) == Q, "promotion piece is Queen");
}

// ─────────────────────────────────────────────────────────────
// 2. Coordinate conversion
// ─────────────────────────────────────────────────────────────
static void testCoordinateConversion() {
    printHeader("Coordinate conversion");

    // Engine square layout: a8=0, h8=7, a1=56, h1=63
    // BoardCoord layout:    row 0 = rank 8, col 0 = file a

    BoardCoord a8 = squareToBoardCoord(0);
    TEST_ASSERT(a8.row == 0 && a8.col == 0, "a8 (sq 0)  → row 0, col 0");

    BoardCoord h8 = squareToBoardCoord(7);
    TEST_ASSERT(h8.row == 0 && h8.col == 7, "h8 (sq 7)  → row 0, col 7");

    BoardCoord a1 = squareToBoardCoord(56);
    TEST_ASSERT(a1.row == 7 && a1.col == 0, "a1 (sq 56) → row 7, col 0");

    BoardCoord h1 = squareToBoardCoord(63);
    TEST_ASSERT(h1.row == 7 && h1.col == 7, "h1 (sq 63) → row 7, col 7");

    BoardCoord e4 = squareToBoardCoord(36);
    TEST_ASSERT(e4.row == 4 && e4.col == 4, "e4 (sq 36) → row 4, col 4");

    // Round-trip: square → coord → square
    for (int sq = 0; sq < 64; sq++) {
        TEST_ASSERT(boardCoordToSquare(squareToBoardCoord(sq)) == sq,
                    "round-trip squareToBoardCoord / boardCoordToSquare");
    }
}

// ─────────────────────────────────────────────────────────────
// 3. Piece type conversion
// ─────────────────────────────────────────────────────────────
static void testPieceConversion() {
    printHeader("Piece type conversion");

    // engineToPhysicalPiece: white pieces
    TEST_ASSERT(engineToPhysicalPiece(P) == PAWN,   "P  → PAWN");
    TEST_ASSERT(engineToPhysicalPiece(N) == KNIGHT, "N  → KNIGHT");
    TEST_ASSERT(engineToPhysicalPiece(B) == BISHOP, "B  → BISHOP");
    TEST_ASSERT(engineToPhysicalPiece(R) == ROOK,   "R  → ROOK");
    TEST_ASSERT(engineToPhysicalPiece(Q) == QUEEN,  "Q  → QUEEN");
    TEST_ASSERT(engineToPhysicalPiece(K) == KING,   "K  → KING");

    // engineToPhysicalPiece: black pieces normalise to same physical type
    TEST_ASSERT(engineToPhysicalPiece(p) == PAWN,   "p  → PAWN");
    TEST_ASSERT(engineToPhysicalPiece(n) == KNIGHT, "n  → KNIGHT");
    TEST_ASSERT(engineToPhysicalPiece(b) == BISHOP, "b  → BISHOP");
    TEST_ASSERT(engineToPhysicalPiece(r) == ROOK,   "r  → ROOK");
    TEST_ASSERT(engineToPhysicalPiece(q) == QUEEN,  "q  → QUEEN");
    TEST_ASSERT(engineToPhysicalPiece(k) == KING,   "k  → KING");

    // physicalToEnginePiece: white
    TEST_ASSERT(physicalToEnginePiece(PAWN,   true) == P, "PAWN   white → P");
    TEST_ASSERT(physicalToEnginePiece(KNIGHT, true) == N, "KNIGHT white → N");
    TEST_ASSERT(physicalToEnginePiece(BISHOP, true) == B, "BISHOP white → B");
    TEST_ASSERT(physicalToEnginePiece(ROOK,   true) == R, "ROOK   white → R");
    TEST_ASSERT(physicalToEnginePiece(QUEEN,  true) == Q, "QUEEN  white → Q");
    TEST_ASSERT(physicalToEnginePiece(KING,   true) == K, "KING   white → K");

    // physicalToEnginePiece: black
    TEST_ASSERT(physicalToEnginePiece(PAWN,   false) == p, "PAWN   black → p");
    TEST_ASSERT(physicalToEnginePiece(QUEEN,  false) == q, "QUEEN  black → q");
    TEST_ASSERT(physicalToEnginePiece(KING,   false) == k, "KING   black → k");

    // Invalid piece
    TEST_ASSERT(physicalToEnginePiece(UNKNOWN, true) == -1, "UNKNOWN → -1");

    // isWhitePiece
    TEST_ASSERT(isWhitePiece(P)  == true,  "P  is white");
    TEST_ASSERT(isWhitePiece(K)  == true,  "K  is white");
    TEST_ASSERT(isWhitePiece(p)  == false, "p  is black");
    TEST_ASSERT(isWhitePiece(k)  == false, "k  is black");
    TEST_ASSERT(isWhitePiece(5)  == true,  "piece 5 (K) is white");
    TEST_ASSERT(isWhitePiece(6)  == false, "piece 6 (p) is black");
}

// ─────────────────────────────────────────────────────────────
// 4. PhysicalBoard state tracking via syncWithEngine
// ─────────────────────────────────────────────────────────────
static void testPhysicalBoardSync() {
    printHeader("PhysicalBoard sync with engine");

    loadFEN(startPosition);

    GameController gc;
    gc.syncWithEngine();

    const PhysicalBoard& board = gc.getPhysicalBoard();

    // Ranks 1,2,7,8 must all be occupied in starting position
    for (int col = 0; col < 8; col++) {
        TEST_ASSERT(board.isOccupied(0, col), "rank 8 occupied after sync");
        TEST_ASSERT(board.isOccupied(1, col), "rank 7 occupied after sync");
        TEST_ASSERT(board.isOccupied(6, col), "rank 2 occupied after sync");
        TEST_ASSERT(board.isOccupied(7, col), "rank 1 occupied after sync");
    }

    // Ranks 3-6 (rows 2-5) must be empty
    for (int row = 2; row <= 5; row++) {
        for (int col = 0; col < 8; col++) {
            TEST_ASSERT(board.isEmpty(row, col), "middle ranks empty after sync");
        }
    }
}

// ─────────────────────────────────────────────────────────────
// 5. parseBoardMove – legal move lookup from string
// ─────────────────────────────────────────────────────────────
static void testParseBoardMove() {
    printHeader("parseBoardMove");

    loadFEN(startPosition);

    // Legal first moves
    int e2e4 = parseBoardMove("e2e4");
    TEST_ASSERT(e2e4 != 0, "e2e4 found in move list");
    TEST_ASSERT(EngineMove::getSource(e2e4) == e2, "e2e4 source is e2");
    TEST_ASSERT(EngineMove::getTarget(e2e4) == e4, "e2e4 target is e4");
    TEST_ASSERT(EngineMove::getPiece(e2e4)  == P,  "e2e4 piece is White Pawn");
    TEST_ASSERT(EngineMove::getDpp(e2e4)    == true, "e2e4 double-pawn-push flag");

    int g1f3 = parseBoardMove("g1f3");
    TEST_ASSERT(g1f3 != 0, "g1f3 found in move list");
    TEST_ASSERT(EngineMove::getPiece(g1f3) == N, "g1f3 piece is White Knight");

    // Illegal / nonsense moves
    TEST_ASSERT(parseBoardMove("e2e5") == 0, "e2e5 not a legal pawn move");
    TEST_ASSERT(parseBoardMove("a1a1") == 0, "a1a1 no-op not legal");
    TEST_ASSERT(parseBoardMove("xyz")  == 0, "short garbage string returns 0");
    TEST_ASSERT(parseBoardMove("e9e1") == 0, "out-of-range rank returns 0");

    // Promotion: set up a position where white has a pawn on e7
    loadFEN("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
    int e7e8q = parseBoardMove("e7e8q");
    TEST_ASSERT(e7e8q != 0, "e7e8q promotion found");
    TEST_ASSERT(EngineMove::getPromoted(e7e8q) == Q, "e7e8q promoted to Queen");

    int e7e8r = parseBoardMove("e7e8r");
    TEST_ASSERT(e7e8r != 0, "e7e8r promotion found");
    TEST_ASSERT(EngineMove::getPromoted(e7e8r) == R, "e7e8r promoted to Rook");
}

// ─────────────────────────────────────────────────────────────
// 6. isGameOver – mid-game, checkmate, stalemate
// ─────────────────────────────────────────────────────────────
static void testIsGameOver() {
    printHeader("isGameOver");

    // Starting position is not over
    loadFEN(startPosition);
    GameController gc1;
    TEST_ASSERT(gc1.isGameOver() == false, "starting position not game over");

    // Scholar's mate: white has just delivered checkmate
    // FEN: white queen on f7, black king on e8, no legal moves for black
    loadFEN("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4");
    GameController gc2;
    TEST_ASSERT(gc2.isGameOver() == true, "Scholar's mate is game over");

    // Stalemate: black king on a8, white queen on b6, white king on c6 — black to move
    loadFEN("k7/8/1QK5/8/8/8/8/8 b - - 0 1");
    GameController gc3;
    TEST_ASSERT(gc3.isGameOver() == true, "stalemate is game over");

    // Position with moves available mid-game
    loadFEN("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");
    GameController gc4;
    TEST_ASSERT(gc4.isGameOver() == false, "normal mid-game not game over");
}

// ─────────────────────────────────────────────────────────────
// 7. isEngineTurn / startGame / stopGame / isGameInProgress
// ─────────────────────────────────────────────────────────────
static void testGameFlow() {
    printHeader("Game flow state");

    loadFEN(startPosition);  // white to move

    GameController gc;

    // Before startGame: not in progress
    TEST_ASSERT(gc.isGameInProgress() == false, "not in progress before startGame");

    // Engine plays white, it's white's turn → engine's turn
    gc.startGame(true);
    TEST_ASSERT(gc.isGameInProgress()  == true,  "in progress after startGame");
    TEST_ASSERT(gc.isEngineTurn()      == true,  "engine plays white, white to move");

    // Engine plays black, it's white's turn → not engine's turn
    gc.startGame(false);
    TEST_ASSERT(gc.isEngineTurn()      == false, "engine plays black, white to move");

    gc.stopGame();
    TEST_ASSERT(gc.isGameInProgress()  == false, "not in progress after stopGame");

    // Switch to black-to-move position
    loadFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    gc.startGame(false);  // engine plays black, black to move
    TEST_ASSERT(gc.isEngineTurn() == true, "engine plays black, black to move");

    gc.startGame(true);   // engine plays white, but it's black to move
    TEST_ASSERT(gc.isEngineTurn() == false, "engine plays white, black to move");
}

// ─────────────────────────────────────────────────────────────
// 8. Hardware-gated functions return false when not connected
// ─────────────────────────────────────────────────────────────
static void testHardwareGating() {
    printHeader("Hardware-gated functions (no hardware)");

    loadFEN(startPosition);
    GameController gc;

    // None of these should crash — they must simply return false
    TEST_ASSERT(gc.isConnected() == false, "not connected without hardware");

    int dummyMove = encodeMove(52, 36, P, 0, 0, 1, 0, 0);  // e2e4
    TEST_ASSERT(gc.executeEngineMove(dummyMove) == false,
                "executeEngineMove returns false without hardware");

    int castleMove = encodeMove(60, 62, K, 0, 0, 0, 0, 1);
    TEST_ASSERT(gc.executeCastling(castleMove) == false,
                "executeCastling returns false without hardware");

    int epMove = encodeMove(27, 20, P, 0, 1, 0, 1, 0);
    TEST_ASSERT(gc.executeEnPassant(epMove) == false,
                "executeEnPassant returns false without hardware");

    int promoMove = encodeMove(12, 4, P, Q, 0, 0, 0, 0);
    TEST_ASSERT(gc.executePromotion(promoMove) == false,
                "executePromotion returns false without hardware");

    TEST_ASSERT(gc.connectHardware("/dev/null") == false,
                "connectHardware bad port returns false");

    TEST_ASSERT(gc.homeGantry() == false,
                "homeGantry returns false without hardware");

    TEST_ASSERT(gc.setupNewGame() == false,
                "setupNewGame returns false without hardware");
}

// ─────────────────────────────────────────────────────────────
// 9. engineToPhysicalMove – end-to-end struct population
// ─────────────────────────────────────────────────────────────
static void testEngineToPhysicalMove() {
    printHeader("engineToPhysicalMove");

    // e2(52) → e4(36), white pawn, not a capture
    int move = encodeMove(52, 36, P, 0, 0, 1, 0, 0);
    PhysicalMove pm = engineToPhysicalMove(move);

    TEST_ASSERT(pm.from.row == 6 && pm.from.col == 4, "e2 from → row 6, col 4");
    TEST_ASSERT(pm.to.row   == 4 && pm.to.col   == 4, "e4 to   → row 4, col 4");
    TEST_ASSERT(pm.pieceType  == PAWN,  "piece type is PAWN");
    TEST_ASSERT(pm.isCapture  == false, "not a capture");

    // d5(27) captures e4(36) — black pawn capture, white pawn captured
    int capMove = encodeMoveWithCapture(27, 36, p, 0, 1, 0, 0, 0, P);
    PhysicalMove cpm = engineToPhysicalMove(capMove);
    TEST_ASSERT(cpm.pieceType == PAWN, "capture move piece type is PAWN");
    TEST_ASSERT(cpm.isCapture == true, "capture flag propagated");

    // Knight move: g1(62) → f3(45)
    int knightMove = encodeMove(62, 45, N, 0, 0, 0, 0, 0);
    PhysicalMove npm = engineToPhysicalMove(knightMove);
    TEST_ASSERT(npm.pieceType == KNIGHT, "knight move piece type is KNIGHT");
    TEST_ASSERT(npm.from.row == 7 && npm.from.col == 6, "g1 from → row 7, col 6");
    TEST_ASSERT(npm.to.row   == 5 && npm.to.col   == 5, "f3 to   → row 5, col 5");
}

// ──────────────────────────────────────────────────────────────────────────
// 10. Restoration paths via engine->physical pipeline
//     Verifies that planMove() populates restoration.path for moves that
//     require blocker relocation, and that paths are geometrically correct.
// ──────────────────────────────────────────────────────────────────────────
static void testRestorationPathsViaEngine() {
    printHeader("Restoration paths (engine -> physical pipeline)");

    // Test 1: Knight from starting position -- all restoration paths populated
    {
        loadFEN(startPosition);
        GameController gc;
        gc.syncWithEngine();

        // Ng1-f3: g1=sq62, f3=sq45
        int engineMove = encodeMove(62, 45, N, 0, 0, 0, 0, 0);
        PhysicalMove pm = engineToPhysicalMove(engineMove);
        MovePlan plan = planMove(const_cast<PhysicalBoard&>(gc.getPhysicalBoard()), pm);

        TEST_ASSERT(plan.isValid, "Ng1-f3 from start valid");
        TEST_ASSERT(plan.restorations.size() == plan.relocations.size(),
                    "restoration count matches relocation count");

        bool allPopulated = true;
        for (const RelocationPlan& r : plan.restorations)
            if (r.path.empty()) { allPopulated = false; break; }
        TEST_ASSERT(allPopulated, "all restoration paths populated for Ng1-f3");
    }

    // Test 2: Rook with two explicit blockers -- paths non-empty, endpoints correct
    {
        PhysicalBoard board;
        board.setOccupancy(parseBoardPosition("Ra1 Pb1 Pc1"));

        PhysicalMove pm(BoardCoord::fromFEN("a1"), BoardCoord::fromFEN("e1"), ROOK, false);
        MovePlan plan = planMove(board, pm);

        TEST_ASSERT(plan.isValid, "Ra1-e1 with 2 blockers valid");
        TEST_ASSERT(plan.relocations.size() == 2, "2 relocations (b1,c1)");
        TEST_ASSERT(plan.restorations.size() == 2, "2 restorations (b1,c1)");

        if (plan.restorations.size() == 2) {
            bool allNonEmpty = true;
            bool endpointsCorrect = true;
            for (size_t i = 0; i < 2; i++) {
                const RelocationPlan& r = plan.restorations[i];
                if (r.path.empty()) { allNonEmpty = false; continue; }
                const BoardCoord& last = r.path.squares[r.path.length() - 1];
                if (last != r.to) endpointsCorrect = false;
            }
            TEST_ASSERT(allNonEmpty, "both restoration paths non-empty");
            TEST_ASSERT(endpointsCorrect,
                        "restoration path endpoints match original squares");
        }
    }

    // Test 3: Destination square must not appear mid-path in any restoration.
    // Ra1->h1 with blocker on g1. After the move h1 is occupied by the rook;
    // g1's piece must route home without passing through h1.
    {
        PhysicalBoard board;
        board.setOccupancy(parseBoardPosition("Ra1 Pg1"));

        PhysicalMove pm(BoardCoord::fromFEN("a1"), BoardCoord::fromFEN("h1"), ROOK, false);
        MovePlan plan = planMove(board, pm);

        TEST_ASSERT(plan.isValid, "Ra1-h1 with g1 blocker valid");
        TEST_ASSERT(plan.restorations.size() == 1, "1 restoration for g1");

        if (!plan.restorations.empty() && !plan.restorations[0].path.empty()) {
            BoardCoord h1(7, 7);
            bool avoidsH1 = true;
            const Path& p = plan.restorations[0].path;
            // Exclude the final square (the destination itself)
            for (int i = 0; i < p.length() - 1; i++) {
                if (p.squares[i] == h1) { avoidsH1 = false; break; }
            }
            TEST_ASSERT(avoidsH1, "restoration path avoids h1 (rook is there now)");
        }
    }

    // Test 4: No blockers -- restoration list must be empty
    {
        loadFEN(startPosition);
        GameController gc;
        gc.syncWithEngine();

        // e2(52)->e4(36), clear pawn push
        int engineMove = encodeMove(52, 36, P, 0, 0, 1, 0, 0);
        PhysicalMove pm = engineToPhysicalMove(engineMove);
        MovePlan plan = planMove(const_cast<PhysicalBoard&>(gc.getPhysicalBoard()), pm);

        TEST_ASSERT(plan.isValid,              "e2-e4 from start valid");
        TEST_ASSERT(plan.relocations.empty(),  "e2-e4 has no relocations");
        TEST_ASSERT(plan.restorations.empty(), "e2-e4 has no restorations");
    }

    // Test 5: Capture -- captured square must not be a restoration destination.
    // Ra1 captures pa5 through blockers a2,a3,a4. a5 is vacated; restorations
    // should only target a2, a3, a4.
    {
        PhysicalBoard board;
        board.setOccupancy(parseBoardPosition("Ra1 Pa2 Pa3 Pa4 pa5"));

        PhysicalMove pm(BoardCoord::fromFEN("a1"), BoardCoord::fromFEN("a5"), ROOK, true);
        MovePlan plan = planMove(board, pm);

        TEST_ASSERT(plan.isValid, "Ra1xa5 capture through 3 blockers valid");
        TEST_ASSERT(plan.relocations.size() == 3, "3 blockers (a2,a3,a4)");
        TEST_ASSERT(plan.restorations.size() == 3, "3 restorations");

        if (plan.restorations.size() == 3) {
            BoardCoord a5 = BoardCoord::fromFEN("a5");
            bool noReturnToA5 = true;
            for (const RelocationPlan& r : plan.restorations)
                if (r.to == a5) { noReturnToA5 = false; break; }
            TEST_ASSERT(noReturnToA5,
                        "no restoration targets a5 (captured piece was there)");
        }
    }
}

// ─────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────
int main() {
    printf(ANSI_BOLD "\n═══ Kirin Game Controller Test Suite ═══\n" ANSI_RESET);

    // Initialise all engine subsystems once
    initAll();

    testEngineMoveDecoding();
    testCoordinateConversion();
    testPieceConversion();
    testPhysicalBoardSync();
    testParseBoardMove();
    testIsGameOver();
    testGameFlow();
    testHardwareGating();
    testEngineToPhysicalMove();
    testRestorationPathsViaEngine();

    printf("\n" ANSI_BOLD "═══ Results: %d/%d passed" ANSI_RESET, testsPassed, testsRun);
    if (testsFailed > 0) {
        printf(ANSI_RED " (%d failed)" ANSI_RESET, testsFailed);
    }
    printf("\n\n");

    return (testsFailed == 0) ? 0 : 1;
}
