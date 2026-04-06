/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    board_scanner_test.cpp - Tests for sensor-based move detection logic
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
*    Test Suite for Kirin Board Scanner
*
*    Tests the sensor-based move detection logic without any GPIO hardware.
*    All tests run against the engine's move generator in simulation mode,
*    verifying that:
*
*      - matchLegalMove correctly identifies moves from occupancy diffs
*      - The storage-zone signal (storageChanged) disambiguates captures
*        from pondering (the core problem we solved)
*      - All special moves are detected: castling, en passant, promotion
*      - Ambiguous promotion defaults to queen
*      - Legacy inferMove still works for diagnostics
*      - squaresToMoveString produces correct UCI notation
*/

#include <cstdio>
#include <cstring>

// Hardware layer
#include "../src/hardware/board_scanner.h"
#include "../src/hardware/game_controller.h"

// Engine headers
#include "../src/engine/engine.h"
#include "../src/engine/position.h"
#include "../src/engine/movegen.h"
#include "../src/engine/bitboard.h"

// ─────────────────────────────────────────────────────────────
// Minimal test framework (matches project conventions)
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
                   "  [line %d]\n", msg, __LINE__);                         \
        }                                                                   \
    } while(0)

static void printHeader(const char* name) {
    printf("\n" ANSI_BOLD ANSI_CYAN "── %s ──" ANSI_RESET "\n", name);
}

// ─────────────────────────────────────────────────────────────
// Test-friendly subclass: exposes the private matchLegalMove
// so we can test it directly without needing GPIO or timers.
// ─────────────────────────────────────────────────────────────
class TestableScanner : public BoardScanner {
public:
    TestableScanner() : BoardScanner() {
        enableSimulation();
    }

    // Expose private matchLegalMove for direct testing
    bool testMatchLegalMove(Bitboard before, Bitboard after,
                            bool storageChanged, int& move) {
        return matchLegalMove(before, after, storageChanged, move);
    }
};

// ─────────────────────────────────────────────────────────────
// Helper: compute the predicted occupancy after applying a move
// to a given starting occupancy. This mirrors the logic inside
// matchLegalMove so we can construct the "sensor reading" that
// the scanner would see after a move is completed.
// ─────────────────────────────────────────────────────────────
static Bitboard predictOccupancy(Bitboard before, int move) {
    Bitboard predicted = before;

    int source = getSource(move);
    int target = getTarget(move);

    // Clear source
    predicted &= ~(1ULL << source);

    // Handle en passant capture
    if (getCapture(move) && getEnpassant(move)) {
        int capturedPawnSquare = (source / 8) * 8 + (target % 8);
        predicted &= ~(1ULL << capturedPawnSquare);
    }

    // Set target
    predicted |= (1ULL << target);

    // Handle castling rook movement
    if (getCastle(move)) {
        if (target == g1) {       // White kingside
            predicted &= ~(1ULL << h1); predicted |= (1ULL << f1);
        } else if (target == c1) { // White queenside
            predicted &= ~(1ULL << a1); predicted |= (1ULL << d1);
        } else if (target == g8) { // Black kingside
            predicted &= ~(1ULL << h8); predicted |= (1ULL << f8);
        } else if (target == c8) { // Black queenside
            predicted &= ~(1ULL << a8); predicted |= (1ULL << d8);
        }
    }

    return predicted;
}

// ─────────────────────────────────────────────────────────────
// Helper: find a specific legal move by source/target/flags
// ─────────────────────────────────────────────────────────────
static int findLegalMove(int src, int tgt, bool mustCapture = false,
                         bool mustEP = false, bool mustCastle = false,
                         int mustPromo = 0) {
    moves moveList;
    moveList.count = 0;
    generateMoves(&moveList);

    // Save state for legality check
    U64 bbCopy[12], occCopy[3];
    int sideCopy = side, epCopy = enpassant, castCopy = castle;
    U64 hashCopy = hashKey;
    memcpy(bbCopy, bitboards, sizeof(bbCopy));
    memcpy(occCopy, occupancy, sizeof(occCopy));

    for (int i = 0; i < moveList.count; i++) {
        int m = moveList.moves[i];
        if (getSource(m) != src || getTarget(m) != tgt) continue;
        if (mustCapture && !getCapture(m)) continue;
        if (mustEP && !getEnpassant(m)) continue;
        if (mustCastle && !getCastle(m)) continue;
        if (mustPromo && getPromoted(m) != mustPromo) continue;

        // Check legality
        if (makeMove(m, allMoves)) {
            // Restore
            memcpy(bitboards, bbCopy, sizeof(bbCopy));
            memcpy(occupancy, occCopy, sizeof(occCopy));
            side = sideCopy; enpassant = epCopy;
            castle = castCopy; hashKey = hashCopy;
            return m;
        }
        memcpy(bitboards, bbCopy, sizeof(bbCopy));
        memcpy(occupancy, occCopy, sizeof(occCopy));
        side = sideCopy; enpassant = epCopy;
        castle = castCopy; hashKey = hashCopy;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────
// 1. Normal moves — non-capture
// ─────────────────────────────────────────────────────────────
static void testNormalMoves() {
    printHeader("Normal moves (non-capture)");

    // Set up starting position
    parseFEN(startPosition);
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    // e2e4: double pawn push
    {
        int move = findLegalMove(e2, e4);
        TEST_ASSERT(move != 0, "e2e4 is a legal move");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, false, matched);
        TEST_ASSERT(found, "e2e4 matched with storageChanged=false");
        TEST_ASSERT(matched == move, "matched move equals expected move");
    }

    // Nf3: knight move
    {
        int move = findLegalMove(g1, f3);
        TEST_ASSERT(move != 0, "Nf3 is a legal move");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, false, matched);
        TEST_ASSERT(found, "Nf3 matched with storageChanged=false");
        TEST_ASSERT(matched == move, "matched move equals expected Nf3");
    }

    // Normal move must NOT match if storageChanged=true
    {
        int move = findLegalMove(e2, e4);
        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, true, matched);
        TEST_ASSERT(!found, "e2e4 does NOT match when storageChanged=true");
    }
}

// ─────────────────────────────────────────────────────────────
// 2. Captures — requires storageChanged=true
// ─────────────────────────────────────────────────────────────
static void testCaptures() {
    printHeader("Captures (storage-aware)");

    // Position where white pawn on d4 can capture black pawn on e5
    parseFEN("rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2");
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    int move = findLegalMove(d4, e5, true);
    TEST_ASSERT(move != 0, "d4xe5 is a legal capture");

    Bitboard after = predictOccupancy(before, move);

    // With storageChanged=true (captured piece placed in storage) → should match
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, true, matched);
        TEST_ASSERT(found, "d4xe5 matched with storageChanged=true");
        TEST_ASSERT(matched == move, "matched move equals expected d4xe5");
    }

    // With storageChanged=false → should NOT match (it's a capture move)
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, false, matched);
        TEST_ASSERT(!found, "d4xe5 does NOT match with storageChanged=false");
    }
}

// ─────────────────────────────────────────────────────────────
// 3. The pondering problem — the core scenario we solved
//
//    Player picks up a piece, thinks for 30 seconds, board shows
//    one square cleared. Storage is unchanged. This must NOT
//    match any move.
// ─────────────────────────────────────────────────────────────
static void testPonderingDisambiguation() {
    printHeader("Pondering disambiguation (capture vs thinking)");

    parseFEN(startPosition);
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    // Simulate: player lifts the pawn from e2 (square 52)
    // Board shows e2 empty, everything else unchanged
    Bitboard pieceLiftedOnly = before & ~(1ULL << e2);

    // No storage change — player is just thinking
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, pieceLiftedOnly, false, matched);
        TEST_ASSERT(!found,
            "Piece lifted from e2 + no storage change → no match (pondering)");
    }

    // Even with storage changed (shouldn't happen, but test defense)
    // this board state doesn't match any capture either (no piece landed)
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, pieceLiftedOnly, true, matched);
        TEST_ASSERT(!found,
            "Piece lifted from e2 + storage change → still no match (incomplete)");
    }

    // Now simulate a position where a capture IS possible.
    // White knight on f3 can capture black pawn on e5.
    parseFEN("rnbqkbnr/pppp1ppp/8/4p3/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 2");
    before = occupancy[both];

    // Player lifts knight from f3 — thinking about Nxe5
    Bitboard knightLifted = before & ~(1ULL << f3);

    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, knightLifted, false, matched);
        TEST_ASSERT(!found,
            "Knight lifted from f3 + no storage → no match (still thinking)");
    }

    // Player completes the capture: f3 cleared, e5 unchanged (knight replaces pawn),
    // storage gained a piece
    int nxe5 = findLegalMove(f3, e5, true);
    TEST_ASSERT(nxe5 != 0, "Nxe5 is legal");
    Bitboard afterCapture = predictOccupancy(before, nxe5);

    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, afterCapture, true, matched);
        TEST_ASSERT(found, "Nxe5 complete + storage change → match");
        TEST_ASSERT(matched == nxe5, "matched move is Nxe5");
    }
}

// ─────────────────────────────────────────────────────────────
// 4. Take-back — player lifts a piece, puts it back down
// ─────────────────────────────────────────────────────────────
static void testTakeBack() {
    printHeader("Take-back (piece returned to original square)");

    parseFEN(startPosition);
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    // Board returns to exact original state — no match needed
    // (waitForLegalMove handles this by continuing to poll,
    //  but matchLegalMove should find no match since before == after)
    {
        int matched = 0;
        // When before == after, the diff is 0 — no move matches
        bool found = scanner.testMatchLegalMove(before, before, false, matched);
        TEST_ASSERT(!found, "Identical before/after → no match (take-back)");
    }
}

// ─────────────────────────────────────────────────────────────
// 5. Castling — two pieces move, no storage change
// ─────────────────────────────────────────────────────────────
static void testCastling() {
    printHeader("Castling");

    // Position where white can castle kingside
    parseFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    // White kingside castle: e1→g1
    {
        int move = findLegalMove(e1, g1, false, false, true);
        TEST_ASSERT(move != 0, "O-O is a legal move");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, false, matched);
        TEST_ASSERT(found, "Kingside castle matched with storageChanged=false");
        TEST_ASSERT(matched == move, "matched move is O-O");
    }

    // White queenside castle: e1→c1
    {
        int move = findLegalMove(e1, c1, false, false, true);
        TEST_ASSERT(move != 0, "O-O-O is a legal move");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, false, matched);
        TEST_ASSERT(found, "Queenside castle matched with storageChanged=false");
        TEST_ASSERT(matched == move, "matched move is O-O-O");
    }

    // Castle must NOT match with storageChanged=true
    {
        int move = findLegalMove(e1, g1, false, false, true);
        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, true, matched);
        TEST_ASSERT(!found, "Castle does NOT match with storageChanged=true");
    }
}

// ─────────────────────────────────────────────────────────────
// 6. En passant — two pieces cleared, one set, storage changes
// ─────────────────────────────────────────────────────────────
static void testEnPassant() {
    printHeader("En passant");

    // White pawn on e5, black pawn just pushed d7→d5 (en passant available)
    parseFEN("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    int move = findLegalMove(e5, d6, true, true);
    TEST_ASSERT(move != 0, "exd6 en passant is legal");

    Bitboard after = predictOccupancy(before, move);

    // En passant is a capture — needs storageChanged=true
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, true, matched);
        TEST_ASSERT(found, "En passant matched with storageChanged=true");
        TEST_ASSERT(matched == move, "matched move is exd6 e.p.");
    }

    // Must not match without storage change
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, false, matched);
        TEST_ASSERT(!found, "En passant does NOT match with storageChanged=false");
    }
}

// ─────────────────────────────────────────────────────────────
// 7. Promotion — ambiguous (4 promotions produce same occupancy)
//    Should default to queen.
// ─────────────────────────────────────────────────────────────
static void testPromotion() {
    printHeader("Promotion");

    // White pawn on e7, about to promote (non-capture)
    parseFEN("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    // All four promotions produce the same occupancy diff
    int queenPromo = findLegalMove(e7, e8, false, false, false, Q);
    TEST_ASSERT(queenPromo != 0, "e8=Q is legal");

    Bitboard after = predictOccupancy(before, queenPromo);

    // storageChanged=false (non-capture promotion)
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, false, matched);
        TEST_ASSERT(found, "Promotion matched");
        TEST_ASSERT(getPromoted(matched) == Q || getPromoted(matched) == q,
            "Ambiguous promotion defaults to queen");
    }

    // Capture-promotion
    parseFEN("3r4/4P3/8/8/8/8/8/4K2k w - - 0 1");
    before = occupancy[both];

    int capPromo = findLegalMove(e7, d8, true, false, false, Q);
    TEST_ASSERT(capPromo != 0, "exd8=Q is legal");

    after = predictOccupancy(before, capPromo);

    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, true, matched);
        TEST_ASSERT(found, "Capture-promotion matched with storageChanged=true");
        TEST_ASSERT(getPromoted(matched) == Q || getPromoted(matched) == q,
            "Capture-promotion defaults to queen");
    }
}

// ─────────────────────────────────────────────────────────────
// 8. Mid-move transients — incomplete actions that should NOT match
// ─────────────────────────────────────────────────────────────
static void testMidMoveTransients() {
    printHeader("Mid-move transient states");

    // Position: white can play Nxe5
    parseFEN("rnbqkbnr/pppp1ppp/8/4p3/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 2");
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    // State 1: Player removes captured pawn from e5 to storage, but hasn't
    // moved their knight yet. Board: e5 empty, f3 still occupied.
    Bitboard capturedPawnRemoved = before & ~(1ULL << e5);
    {
        int matched = 0;
        // Storage changed (pawn went to tray), but board doesn't match any move
        bool found = scanner.testMatchLegalMove(before, capturedPawnRemoved, true, matched);
        TEST_ASSERT(!found,
            "Captured pawn removed but knight not moved → no match (mid-capture)");
    }

    // State 2: Player has lifted knight from f3 AND removed pawn from e5,
    // but hasn't placed knight on e5 yet. Both squares empty.
    Bitboard bothLifted = before & ~(1ULL << f3) & ~(1ULL << e5);
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, bothLifted, true, matched);
        TEST_ASSERT(!found,
            "Knight and pawn both lifted → no match (piece in hand)");
    }

    // State 3: Castling mid-move — king lifted but rook hasn't moved yet
    parseFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    before = occupancy[both];

    Bitboard kingLifted = before & ~(1ULL << e1);
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, kingLifted, false, matched);
        TEST_ASSERT(!found,
            "King lifted for castling but rook not moved → no match");
    }

    // Rook moved but king not yet placed
    Bitboard rookMoved = before & ~(1ULL << e1) & ~(1ULL << h1);
    rookMoved |= (1ULL << f1);
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, rookMoved, false, matched);
        TEST_ASSERT(!found,
            "Rook moved to f1 but king not placed → no match (mid-castle)");
    }
}

// ─────────────────────────────────────────────────────────────
// 9. Legacy inferMove — raw diff-based inference
// ─────────────────────────────────────────────────────────────
static void testLegacyInferMove() {
    printHeader("Legacy inferMove");

    // Normal move: e2→e4
    {
        Bitboard before = 0;
        before |= (1ULL << e2);
        Bitboard after = 0;
        after |= (1ULL << e4);
        // Add some other pieces that don't move
        before |= (1ULL << a1) | (1ULL << h8);
        after  |= (1ULL << a1) | (1ULL << h8);

        int from = -1, to = -1;
        bool ok = BoardScanner::inferMove(before, after, from, to);
        TEST_ASSERT(ok, "inferMove: normal move detected");
        TEST_ASSERT(from == e2, "inferMove: source is e2");
        TEST_ASSERT(to == e4, "inferMove: dest is e4");
    }

    // Capture: e2→d3 (d3 was occupied, still occupied after)
    {
        Bitboard before = (1ULL << e2) | (1ULL << d3) | (1ULL << a1);
        Bitboard after  = (1ULL << d3) | (1ULL << a1);
        // After capture: e2 cleared, d3 still set

        int from = -1, to = -1;
        bool ok = BoardScanner::inferMove(before, after, from, to);
        // inferMove returns false for captures (can't determine dest)
        TEST_ASSERT(!ok, "inferMove: capture returns false (needs legal moves)");
        TEST_ASSERT(from == e2, "inferMove: capture source is e2");
    }

    // Castling: e1→g1 + h1→f1
    {
        Bitboard before = (1ULL << e1) | (1ULL << h1);
        Bitboard after  = (1ULL << g1) | (1ULL << f1);

        int from = -1, to = -1;
        bool ok = BoardScanner::inferMove(before, after, from, to);
        TEST_ASSERT(ok, "inferMove: castling pattern detected");
        // King is the one that moved 2 squares
        TEST_ASSERT(from == e1, "inferMove: castling king source is e1");
        TEST_ASSERT(to == g1, "inferMove: castling king dest is g1");
    }
}

// ─────────────────────────────────────────────────────────────
// 10. squaresToMoveString — UCI notation
// ─────────────────────────────────────────────────────────────
static void testSquaresToMoveString() {
    printHeader("squaresToMoveString");

    char buf[6];

    // e2e4
    BoardScanner::squaresToMoveString(e2, e4, buf);
    TEST_ASSERT(strcmp(buf, "e2e4") == 0, "e2e4 string");

    // a7a8 (no promotion)
    BoardScanner::squaresToMoveString(a7, a8, buf);
    TEST_ASSERT(strcmp(buf, "a7a8") == 0, "a7a8 string");

    // e7e8q (queen promotion)
    BoardScanner::squaresToMoveString(e7, e8, buf, 'q');
    TEST_ASSERT(strcmp(buf, "e7e8q") == 0, "e7e8q promotion string");

    // a2a1n (knight underpromotion)
    BoardScanner::squaresToMoveString(a2, a1, buf, 'n');
    TEST_ASSERT(strcmp(buf, "a2a1n") == 0, "a2a1n underpromotion string");
}

// ─────────────────────────────────────────────────────────────
// 11. Multiple captures from same source — ambiguous occupancy
//
//     When a queen can capture on two different squares, both
//     produce the same occupancy diff (source clears, destination
//     stays occupied). The sensors can't distinguish them.
//     matchLegalMove correctly returns false (ambiguous).
//
//     In practice this is rare: it requires two capture targets
//     where the piece count stays the same. Non-capture moves
//     from the same square (e.g., Qd4-d5 vs Qd4-d6) DO produce
//     different occupancy and resolve uniquely.
// ─────────────────────────────────────────────────────────────
static void testMultipleCaptureTargets() {
    printHeader("Multiple capture targets from same source");

    // Queen on d4, can capture on d7 AND g7 — both produce the same
    // board occupancy (d4 clears, target stays set), so this is
    // genuinely ambiguous from the sensor perspective.
    parseFEN("3k4/3p2p1/8/8/3Q4/8/8/4K3 w - - 0 1");
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    int qxd7 = findLegalMove(d4, d7, true);
    TEST_ASSERT(qxd7 != 0, "Qxd7 is legal");

    Bitboard afterQxd7 = predictOccupancy(before, qxd7);

    // Both Qxd7 and Qxg7 produce the same occupancy (d4 clears,
    // everything else stays the same), so matchLegalMove can't
    // distinguish them — it correctly returns false (ambiguous).
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, afterQxd7, true, matched);
        TEST_ASSERT(!found,
            "Ambiguous capture (Qxd7 vs other captures) → no unique match");
    }

    // A capture where only ONE legal capture exists from the source
    // DOES resolve uniquely. Put the queen where it can only capture one piece.
    parseFEN("3k4/6p1/8/8/6Q1/8/8/4K3 w - - 0 1");
    before = occupancy[both];

    int qxg7 = findLegalMove(g4, g7, true);
    TEST_ASSERT(qxg7 != 0, "Qxg7 is legal (only capture from g4 on g-file)");

    Bitboard afterQxg7 = predictOccupancy(before, qxg7);

    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, afterQxg7, true, matched);
        TEST_ASSERT(found, "Unambiguous capture (only one target) → unique match");
        if (found) {
            TEST_ASSERT(getTarget(matched) == g7, "matched target is g7");
        }
    }
}

// ─────────────────────────────────────────────────────────────
// 12. Black's turn — verify it works for both sides
// ─────────────────────────────────────────────────────────────
static void testBlackMoves() {
    printHeader("Black moves");

    // After 1. e4, it's black's turn
    parseFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    // e7e5: black double pawn push
    {
        int move = findLegalMove(e7, e5);
        TEST_ASSERT(move != 0, "e7e5 is legal for black");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, false, matched);
        TEST_ASSERT(found, "Black e7e5 matched");
        TEST_ASSERT(matched == move, "matched move is e7e5");
    }

    // Nc6: black knight
    {
        int move = findLegalMove(b8, c6);
        TEST_ASSERT(move != 0, "Nc6 is legal for black");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after, false, matched);
        TEST_ASSERT(found, "Black Nc6 matched");
    }
}

// ─────────────────────────────────────────────────────────────
// 13. Board verification — verifyBoardState
// ─────────────────────────────────────────────────────────────
static void testBoardVerification() {
    printHeader("Board verification (verifyBoardState)");

    // Set up starting position in the engine
    parseFEN(startPosition);

    // Create a GameController and scanner in simulation mode
    GameController gc;
    gc.getGantry().enableDryRun();
    gc.getScanner().enableSimulation();
    gc.syncWithEngine();

    // Test 1: Board matches — simulate sensors returning the correct occupancy
    {
        gc.getScanner().setSimulatedOccupancy(occupancy[both]);
        Bitboard expected = 0, actual = 0;
        bool ok = gc.verifyBoardState(expected, actual);
        TEST_ASSERT(ok, "Board matches engine state → verify returns true");
        TEST_ASSERT(expected == actual, "expected == actual when board matches");
    }

    // Test 2: Missing piece — e2 pawn fell off
    {
        Bitboard tampered = occupancy[both] & ~(1ULL << e2);
        gc.getScanner().setSimulatedOccupancy(tampered);
        Bitboard expected = 0, actual = 0;
        bool ok = gc.verifyBoardState(expected, actual);
        TEST_ASSERT(!ok, "Missing piece on e2 → verify returns false");
        TEST_ASSERT(expected != actual, "expected != actual when piece missing");

        // Check the diff identifies e2
        Bitboard diff = expected ^ actual;
        Bitboard missing = expected & diff;
        TEST_ASSERT((missing & (1ULL << e2)) != 0,
            "Mismatch diff identifies e2 as missing");
    }

    // Test 3: Extra piece — a piece appeared on e4 (shouldn't be there in starting pos)
    {
        Bitboard tampered = occupancy[both] | (1ULL << e4);
        gc.getScanner().setSimulatedOccupancy(tampered);
        Bitboard expected = 0, actual = 0;
        bool ok = gc.verifyBoardState(expected, actual);
        TEST_ASSERT(!ok, "Extra piece on e4 → verify returns false");

        Bitboard diff = expected ^ actual;
        Bitboard extra = actual & diff;
        TEST_ASSERT((extra & (1ULL << e4)) != 0,
            "Mismatch diff identifies e4 as extra");
    }

    // Test 4: Multiple differences — two pieces displaced
    {
        Bitboard tampered = occupancy[both];
        tampered &= ~(1ULL << a2);  // a2 pawn missing
        tampered &= ~(1ULL << h7);  // h7 pawn missing
        tampered |= (1ULL << d4);   // piece appeared on d4
        gc.getScanner().setSimulatedOccupancy(tampered);
        Bitboard expected = 0, actual = 0;
        bool ok = gc.verifyBoardState(expected, actual);
        TEST_ASSERT(!ok, "Multiple differences → verify returns false");

        Bitboard diff = expected ^ actual;
        int diffCount = countBits(diff);
        TEST_ASSERT(diffCount == 3,
            "3 squares differ (a2, h7 missing + d4 extra)");
    }

    // Test 5: Scanner not initialized — should return true (can't verify, assume OK)
    {
        GameController gc2;  // No scanner init, no dry run
        // gc2.scanner is not initialized
        Bitboard expected = 0, actual = 0;
        bool ok = gc2.verifyBoardState(expected, actual);
        TEST_ASSERT(ok, "No scanner initialized → verify returns true (skip)");
    }
}

// ─────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────
int main() {
    printf(ANSI_BOLD "\n═══ Kirin Board Scanner Test Suite ═══\n" ANSI_RESET);

    // Initialize all engine subsystems
    initAll();

    testNormalMoves();
    testCaptures();
    testPonderingDisambiguation();
    testTakeBack();
    testCastling();
    testEnPassant();
    testPromotion();
    testMidMoveTransients();
    testLegacyInferMove();
    testSquaresToMoveString();
    testMultipleCaptureTargets();
    testBlackMoves();
    testBoardVerification();

    printf("\n" ANSI_BOLD "═══ Results: %d/%d passed" ANSI_RESET, testsPassed, testsRun);
    if (testsFailed > 0) {
        printf(ANSI_RED " (%d failed)" ANSI_RESET, testsFailed);
    }
    printf("\n\n");

    return (testsFailed == 0) ? 0 : 1;
}
