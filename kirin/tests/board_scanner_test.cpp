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
#include "../src/hardware/piece_tracker.h"

// Engine headers
#include "../src/engine/engine.h"
#include "../src/engine/position.h"
#include "../src/engine/movegen.h"
#include "../src/engine/bitboard.h"

// Bring Gantry::StartingSlot enum values (SLOT_PAWN_E, etc.) into scope
using namespace Gantry;

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

    // Expose private matchLegalMove for direct testing.
    // New signature: takes a specific capture slot, side, and tracker.
    bool testMatchLegalMove(Bitboard before, Bitboard after,
                            int captureSlot, bool capturedIsWhite,
                            const PieceTracker& tracker, int& move) {
        return matchLegalMove(before, after, captureSlot, capturedIsWhite,
                              tracker, move);
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
// Helper: create a PieceTracker initialized from the starting
// position and optionally apply a sequence of moves to it.
// ─────────────────────────────────────────────────────────────
static PieceTracker makeTracker() {
    PieceTracker t;
    t.initStartingPosition();
    return t;
}

// Apply a move to a tracker (mirrors GameController::updateTracker)
static void applyMoveToTracker(PieceTracker& tracker, int move) {
    int source = getSource(move);
    int target = getTarget(move);
    int piece = getPiece(move);
    bool isWhite = piece < 6;  // P=0..K=5 are white, p=6..k=11 are black
    bool isCap = getCapture(move) != 0;

    if (isCap) {
        if (getEnpassant(move)) {
            int capturedSquare = (source / 8) * 8 + (target % 8);
            tracker.applyMove(source, target, isWhite, true,
                              capturedSquare, !isWhite);
        } else {
            tracker.applyMove(source, target, isWhite, true,
                              target, !isWhite);
        }
    } else {
        tracker.applyMove(source, target, isWhite, false);
    }

    if (getCastle(move)) {
        if (target == g1) tracker.applyRookCastle(h1, f1, true);
        else if (target == c1) tracker.applyRookCastle(a1, d1, true);
        else if (target == g8) tracker.applyRookCastle(h8, f8, false);
        else if (target == c8) tracker.applyRookCastle(a8, d8, false);
    }
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
    PieceTracker tracker = makeTracker();

    // e2e4: double pawn push
    {
        int move = findLegalMove(e2, e4);
        TEST_ASSERT(move != 0, "e2e4 is a legal move");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_NONE, false, tracker, matched);
        TEST_ASSERT(found, "e2e4 matched with no storage change");
        TEST_ASSERT(matched == move, "matched move equals expected move");
    }

    // Nf3: knight move
    {
        int move = findLegalMove(g1, f3);
        TEST_ASSERT(move != 0, "Nf3 is a legal move");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_NONE, false, tracker, matched);
        TEST_ASSERT(found, "Nf3 matched with no storage change");
        TEST_ASSERT(matched == move, "matched move equals expected Nf3");
    }

    // Normal move must NOT match if a storage slot changed
    {
        int move = findLegalMove(e2, e4);
        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        // Claim SLOT_PAWN_E of black side gained a piece (bogus)
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_PAWN_E, false, tracker, matched);
        TEST_ASSERT(!found, "e2e4 does NOT match when a storage slot changed");
    }
}

// ─────────────────────────────────────────────────────────────
// 2. Captures — requires correct storage slot
// ─────────────────────────────────────────────────────────────
static void testCaptures() {
    printHeader("Captures (storage-slot aware)");

    // Position where white pawn on d4 can capture black pawn on e5
    // To get here: 1. d4 e5. Tracker needs those moves applied.
    parseFEN("rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2");
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    // Build tracker: start pos, then d2d4 and e7e5
    PieceTracker tracker = makeTracker();
    // d2→d4 (white pawn)
    tracker.applyMove(d2, d4, true, false);
    // e7→e5 (black pawn)
    tracker.applyMove(e7, e5, false, false);

    int move = findLegalMove(d4, e5, true);
    TEST_ASSERT(move != 0, "d4xe5 is a legal capture");

    Bitboard after = predictOccupancy(before, move);

    // The captured piece is the black e-pawn → SLOT_PAWN_E (slot 12)
    // in black's storage (capturedIsWhite=false)
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_PAWN_E, false, tracker, matched);
        TEST_ASSERT(found, "d4xe5 matched with correct slot (SLOT_PAWN_E)");
        TEST_ASSERT(matched == move, "matched move equals expected d4xe5");
    }

    // With no storage change → should NOT match (it's a capture move)
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_NONE, false, tracker, matched);
        TEST_ASSERT(!found, "d4xe5 does NOT match with no storage change");
    }

    // With WRONG slot (e.g., SLOT_PAWN_D instead of SLOT_PAWN_E) → no match
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_PAWN_D, false, tracker, matched);
        TEST_ASSERT(!found, "d4xe5 does NOT match with wrong slot (SLOT_PAWN_D)");
    }
}

// ─────────────────────────────────────────────────────────────
// 3. The pondering problem — the core scenario we solved
// ─────────────────────────────────────────────────────────────
static void testPonderingDisambiguation() {
    printHeader("Pondering disambiguation (capture vs thinking)");

    parseFEN(startPosition);
    Bitboard before = occupancy[both];
    TestableScanner scanner;
    PieceTracker tracker = makeTracker();

    // Simulate: player lifts the pawn from e2 (square 52)
    Bitboard pieceLiftedOnly = before & ~(1ULL << e2);

    // No storage change — player is just thinking
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, pieceLiftedOnly,
                     SLOT_NONE, false, tracker, matched);
        TEST_ASSERT(!found,
            "Piece lifted from e2 + no storage change → no match (pondering)");
    }

    // Even with a storage slot set (shouldn't happen, but test defense)
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, pieceLiftedOnly,
                     SLOT_PAWN_E, false, tracker, matched);
        TEST_ASSERT(!found,
            "Piece lifted from e2 + storage change → still no match (incomplete)");
    }

    // Now simulate a position where a capture IS possible.
    // White knight on f3 can capture black pawn on e5.
    parseFEN("rnbqkbnr/pppp1ppp/8/4p3/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 2");
    before = occupancy[both];

    // Build tracker for this position
    PieceTracker tracker2 = makeTracker();
    tracker2.applyMove(g1, f3, true, false);  // Nf3
    tracker2.applyMove(e7, e5, false, false);  // e5

    // Player lifts knight from f3 — thinking about Nxe5
    Bitboard knightLifted = before & ~(1ULL << f3);

    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, knightLifted,
                     SLOT_NONE, false, tracker2, matched);
        TEST_ASSERT(!found,
            "Knight lifted from f3 + no storage → no match (still thinking)");
    }

    // Player completes the capture: f3 cleared, e5 unchanged (knight replaces pawn),
    // black pawn placed in SLOT_PAWN_E
    int nxe5 = findLegalMove(f3, e5, true);
    TEST_ASSERT(nxe5 != 0, "Nxe5 is legal");
    Bitboard afterCapture = predictOccupancy(before, nxe5);

    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, afterCapture,
                     SLOT_PAWN_E, false, tracker2, matched);
        TEST_ASSERT(found, "Nxe5 complete + correct slot → match");
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
    PieceTracker tracker = makeTracker();

    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, before,
                     SLOT_NONE, false, tracker, matched);
        TEST_ASSERT(!found, "Identical before/after → no match (take-back)");
    }
}

// ─────────────────────────────────────────────────────────────
// 5. Castling — two pieces move, no storage change
// ─────────────────────────────────────────────────────────────
static void testCastling() {
    printHeader("Castling");

    // Position where white can castle both sides
    parseFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    // Build tracker: pieces that moved off back rank to allow castling
    // For simplicity, use a tracker that matches this FEN.
    // In this FEN, the king and rooks are on their starting squares,
    // but the knights/bishops/queen have been removed. We can init
    // from start and manually clear those pieces.
    PieceTracker tracker = makeTracker();
    // The FEN has pieces removed from b1,c1,d1,f1,g1 (white) and
    // b8,c8,d8,f8,g8 (black). These pieces were captured.
    // For the tracker, we just need king and rooks to be on starting squares.
    // The removed pieces don't affect castling matching.
    // Since they're not on the board, their tracker entries are irrelevant
    // for this test — we only care about non-capture move matching.

    // White kingside castle: e1→g1
    {
        int move = findLegalMove(e1, g1, false, false, true);
        TEST_ASSERT(move != 0, "O-O is a legal move");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_NONE, false, tracker, matched);
        TEST_ASSERT(found, "Kingside castle matched with no storage change");
        TEST_ASSERT(matched == move, "matched move is O-O");
    }

    // White queenside castle: e1→c1
    {
        int move = findLegalMove(e1, c1, false, false, true);
        TEST_ASSERT(move != 0, "O-O-O is a legal move");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_NONE, false, tracker, matched);
        TEST_ASSERT(found, "Queenside castle matched with no storage change");
        TEST_ASSERT(matched == move, "matched move is O-O-O");
    }

    // Castle must NOT match with a storage change
    {
        int move = findLegalMove(e1, g1, false, false, true);
        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_PAWN_A, false, tracker, matched);
        TEST_ASSERT(!found, "Castle does NOT match with a storage slot change");
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

    // Build tracker for this position
    PieceTracker tracker = makeTracker();
    tracker.applyMove(e2, e4, true, false);   // 1. e4
    tracker.applyMove(a7, a6, false, false);  // 1... a6 (some move)
    tracker.applyMove(e4, e5, true, false);   // 2. e5
    tracker.applyMove(d7, d5, false, false);  // 2... d5

    int move = findLegalMove(e5, d6, true, true);
    TEST_ASSERT(move != 0, "exd6 en passant is legal");

    Bitboard after = predictOccupancy(before, move);

    // En passant captures the d-pawn → SLOT_PAWN_D in black's storage
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_PAWN_D, false, tracker, matched);
        TEST_ASSERT(found, "En passant matched with correct slot (SLOT_PAWN_D)");
        TEST_ASSERT(matched == move, "matched move is exd6 e.p.");
    }

    // Must not match with no storage change
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_NONE, false, tracker, matched);
        TEST_ASSERT(!found, "En passant does NOT match with no storage change");
    }

    // Must not match with wrong slot
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_PAWN_E, false, tracker, matched);
        TEST_ASSERT(!found, "En passant does NOT match with wrong slot");
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
    PieceTracker tracker = makeTracker();
    // Pawn has moved from e2 to e7 through several moves — for this test
    // the tracker identity is tracked but doesn't affect promotion matching.
    tracker.applyMove(e2, e4, true, false);
    tracker.applyMove(e4, e5, true, false);
    tracker.applyMove(e5, e6, true, false);
    tracker.applyMove(e6, e7, true, false);

    // All four promotions produce the same occupancy diff
    int queenPromo = findLegalMove(e7, e8, false, false, false, Q);
    TEST_ASSERT(queenPromo != 0, "e8=Q is legal");

    Bitboard after = predictOccupancy(before, queenPromo);

    // No storage change (non-capture promotion)
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_NONE, false, tracker, matched);
        TEST_ASSERT(found, "Promotion matched");
        TEST_ASSERT(getPromoted(matched) == Q || getPromoted(matched) == q,
            "Ambiguous promotion defaults to queen");
    }

    // Capture-promotion
    parseFEN("3r4/4P3/8/8/8/8/8/4K2k w - - 0 1");
    before = occupancy[both];

    // The captured piece is black rook on d8 → SLOT_QUEEN_D (slot 3)?
    // Actually, d8 is the queen's starting square, but the piece there
    // is a rook (it could have moved there). For this FEN we need a
    // tracker that reflects a rook on d8. Since it's a simplified
    // position, let's just use a fresh tracker where d8 has the queen
    // slot identity (the piece that started on d8 was the queen).
    PieceTracker tracker2 = makeTracker();
    tracker2.applyMove(e2, e7, true, false);  // simplified

    int capPromo = findLegalMove(e7, d8, true, false, false, Q);
    TEST_ASSERT(capPromo != 0, "exd8=Q is legal");

    after = predictOccupancy(before, capPromo);

    // d8 originally held black's queen → SLOT_QUEEN_D (slot 3)
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_QUEEN_D, false, tracker2, matched);
        TEST_ASSERT(found, "Capture-promotion matched with correct slot");
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
    PieceTracker tracker = makeTracker();
    tracker.applyMove(g1, f3, true, false);
    tracker.applyMove(e7, e5, false, false);

    // State 1: Player removes captured pawn from e5 to storage, but hasn't
    // moved their knight yet. Board: e5 empty, f3 still occupied.
    Bitboard capturedPawnRemoved = before & ~(1ULL << e5);
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, capturedPawnRemoved,
                     SLOT_PAWN_E, false, tracker, matched);
        TEST_ASSERT(!found,
            "Captured pawn removed but knight not moved → no match (mid-capture)");
    }

    // State 2: Player has lifted knight from f3 AND removed pawn from e5,
    // but hasn't placed knight on e5 yet.
    Bitboard bothLifted = before & ~(1ULL << f3) & ~(1ULL << e5);
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, bothLifted,
                     SLOT_PAWN_E, false, tracker, matched);
        TEST_ASSERT(!found,
            "Knight and pawn both lifted → no match (piece in hand)");
    }

    // State 3: Castling mid-move — king lifted but rook hasn't moved yet
    parseFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    before = occupancy[both];
    PieceTracker trackerCastle = makeTracker();

    Bitboard kingLifted = before & ~(1ULL << e1);
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, kingLifted,
                     SLOT_NONE, false, trackerCastle, matched);
        TEST_ASSERT(!found,
            "King lifted for castling but rook not moved → no match");
    }

    // Rook moved but king not yet placed
    Bitboard rookMoved = before & ~(1ULL << e1) & ~(1ULL << h1);
    rookMoved |= (1ULL << f1);
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, rookMoved,
                     SLOT_NONE, false, trackerCastle, matched);
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

        int from = -1, to = -1;
        bool ok = BoardScanner::inferMove(before, after, from, to);
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

    BoardScanner::squaresToMoveString(e2, e4, buf);
    TEST_ASSERT(strcmp(buf, "e2e4") == 0, "e2e4 string");

    BoardScanner::squaresToMoveString(a7, a8, buf);
    TEST_ASSERT(strcmp(buf, "a7a8") == 0, "a7a8 string");

    BoardScanner::squaresToMoveString(e7, e8, buf, 'q');
    TEST_ASSERT(strcmp(buf, "e7e8q") == 0, "e7e8q promotion string");

    BoardScanner::squaresToMoveString(a2, a1, buf, 'n');
    TEST_ASSERT(strcmp(buf, "a2a1n") == 0, "a2a1n underpromotion string");
}

// ─────────────────────────────────────────────────────────────
// 11. Multiple captures from same source — NOW RESOLVED by slot identity
//
//     When a queen can capture on two different squares, both
//     produce the same occupancy diff. But the storage slot
//     identifies WHICH piece was captured, resolving the ambiguity.
// ─────────────────────────────────────────────────────────────
static void testMultipleCaptureTargets() {
    printHeader("Multiple capture targets — slot-based disambiguation");

    // Queen on d4, can capture on d7 AND g7.
    // Previously ambiguous — now resolved by storage slot.
    parseFEN("3k4/3p2p1/8/8/3Q4/8/8/4K3 w - - 0 1");
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    // Build tracker: white queen moved from d1 to d4
    PieceTracker tracker = makeTracker();
    tracker.applyMove(d1, d4, true, false);  // simplified: queen to d4

    int qxd7 = findLegalMove(d4, d7, true);
    int qxg7 = findLegalMove(d4, g7, true);
    TEST_ASSERT(qxd7 != 0, "Qxd7 is legal");
    TEST_ASSERT(qxg7 != 0, "Qxg7 is legal");

    Bitboard afterQxd7 = predictOccupancy(before, qxd7);
    Bitboard afterQxg7 = predictOccupancy(before, qxg7);

    // With the correct slot for d7 (SLOT_PAWN_D = slot 11, black's d-pawn)
    // → should uniquely match Qxd7
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, afterQxd7,
                     SLOT_PAWN_D, false, tracker, matched);
        TEST_ASSERT(found, "Qxd7 resolved by SLOT_PAWN_D");
        if (found) {
            TEST_ASSERT(getTarget(matched) == d7, "matched target is d7");
        }
    }

    // With the correct slot for g7 (SLOT_PAWN_G = slot 14, black's g-pawn)
    // → should uniquely match Qxg7
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, afterQxg7,
                     SLOT_PAWN_G, false, tracker, matched);
        TEST_ASSERT(found, "Qxg7 resolved by SLOT_PAWN_G");
        if (found) {
            TEST_ASSERT(getTarget(matched) == g7, "matched target is g7");
        }
    }

    // With WRONG slot (e.g., SLOT_PAWN_A) → no match
    {
        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, afterQxd7,
                     SLOT_PAWN_A, false, tracker, matched);
        TEST_ASSERT(!found, "Wrong slot (SLOT_PAWN_A) → no match");
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
    PieceTracker tracker = makeTracker();
    tracker.applyMove(e2, e4, true, false);  // 1. e4

    // e7e5: black double pawn push
    {
        int move = findLegalMove(e7, e5);
        TEST_ASSERT(move != 0, "e7e5 is legal for black");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_NONE, false, tracker, matched);
        TEST_ASSERT(found, "Black e7e5 matched");
        TEST_ASSERT(matched == move, "matched move is e7e5");
    }

    // Nc6: black knight
    {
        int move = findLegalMove(b8, c6);
        TEST_ASSERT(move != 0, "Nc6 is legal for black");

        Bitboard after = predictOccupancy(before, move);

        int matched = 0;
        bool found = scanner.testMatchLegalMove(before, after,
                     SLOT_NONE, false, tracker, matched);
        TEST_ASSERT(found, "Black Nc6 matched");
    }
}

// ─────────────────────────────────────────────────────────────
// 13. PieceTracker — unit tests for the tracker itself
// ─────────────────────────────────────────────────────────────
static void testPieceTracker() {
    printHeader("PieceTracker identity tracking");

    PieceTracker tracker;
    tracker.initStartingPosition();

    // Verify starting positions
    // Black rook on a8 (square 0) → SLOT_ROOK_A (0)
    TEST_ASSERT(tracker.getSlotAt(a8) == SLOT_ROOK_A,
        "a8 has SLOT_ROOK_A at start");
    // White king on e1 (square 60) → SLOT_KING_E (4)
    TEST_ASSERT(tracker.getSlotAt(e1) == SLOT_KING_E,
        "e1 has SLOT_KING_E at start");
    // Black pawn on d7 (square 11) → SLOT_PAWN_D (11)
    TEST_ASSERT(tracker.getSlotAt(d7) == SLOT_PAWN_D,
        "d7 has SLOT_PAWN_D at start");
    // Empty square e4 → SLOT_NONE
    TEST_ASSERT(tracker.getSlotAt(e4) == SLOT_NONE,
        "e4 is SLOT_NONE at start");

    // Reverse lookup: white's SLOT_PAWN_E should be on e2
    TEST_ASSERT(tracker.getSquareForSlot(true, SLOT_PAWN_E) == e2,
        "White SLOT_PAWN_E is on e2 at start");

    // Apply 1. e4
    tracker.applyMove(e2, e4, true, false);
    TEST_ASSERT(tracker.getSlotAt(e2) == SLOT_NONE,
        "e2 is empty after e4");
    TEST_ASSERT(tracker.getSlotAt(e4) == SLOT_PAWN_E,
        "e4 has SLOT_PAWN_E after e4");
    TEST_ASSERT(tracker.getSquareForSlot(true, SLOT_PAWN_E) == e4,
        "White SLOT_PAWN_E is on e4 after 1.e4");

    // Apply 1... d5
    tracker.applyMove(d7, d5, false, false);
    TEST_ASSERT(tracker.getSlotAt(d5) == SLOT_PAWN_D,
        "d5 has SLOT_PAWN_D after d5");

    // Apply 2. exd5 (capture)
    tracker.applyMove(e4, d5, true, true, d5, false);
    TEST_ASSERT(tracker.getSlotAt(d5) == SLOT_PAWN_E,
        "d5 has SLOT_PAWN_E after exd5 (white pawn captured black pawn)");
    TEST_ASSERT(tracker.getSlotAt(e4) == SLOT_NONE,
        "e4 is empty after exd5");
    TEST_ASSERT(!tracker.isPieceAlive(false, SLOT_PAWN_D),
        "Black SLOT_PAWN_D is captured after exd5");
    TEST_ASSERT(tracker.isPieceAlive(true, SLOT_PAWN_E),
        "White SLOT_PAWN_E is still alive after exd5");
    TEST_ASSERT(tracker.getSquareForSlot(true, SLOT_PAWN_E) == d5,
        "White SLOT_PAWN_E is on d5 after exd5");
}

// ─────────────────────────────────────────────────────────────
// 14. PieceTracker castling
// ─────────────────────────────────────────────────────────────
static void testPieceTrackerCastling() {
    printHeader("PieceTracker castling");

    PieceTracker tracker;
    tracker.initStartingPosition();

    // Simulate clearing the way for kingside castle
    // (just move pieces out, doesn't matter where for tracker test)
    tracker.applyMove(f1, e3, true, false);  // bishop
    tracker.applyMove(g1, f3, true, false);  // knight

    // Now castle: king e1→g1, rook h1→f1
    tracker.applyMove(e1, g1, true, false);
    tracker.applyRookCastle(h1, f1, true);

    TEST_ASSERT(tracker.getSlotAt(g1) == SLOT_KING_E,
        "g1 has SLOT_KING_E after O-O");
    TEST_ASSERT(tracker.getSlotAt(f1) == SLOT_ROOK_H,
        "f1 has SLOT_ROOK_H after O-O");
    TEST_ASSERT(tracker.getSlotAt(e1) == SLOT_NONE,
        "e1 is empty after O-O");
    TEST_ASSERT(tracker.getSlotAt(h1) == SLOT_NONE,
        "h1 is empty after O-O");
}

// ─────────────────────────────────────────────────────────────
// 15. Wrong slot detection — already-captured piece's slot
// ─────────────────────────────────────────────────────────────
static void testWrongSlotAlreadyCaptured() {
    printHeader("Wrong slot — piece already captured");

    // Set up position after some captures
    parseFEN("rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2");
    Bitboard before = occupancy[both];
    TestableScanner scanner;

    PieceTracker tracker = makeTracker();
    tracker.applyMove(d2, d4, true, false);
    tracker.applyMove(e7, e5, false, false);

    // Now do the capture d4xe5
    int dxe5 = findLegalMove(d4, e5, true);
    TEST_ASSERT(dxe5 != 0, "d4xe5 is legal");
    // (We don't need afterDxe5 here — this test is about the tracker state,
    // not about matching a move against occupancy.)

    // Simulate the tracker having already captured the e-pawn
    PieceTracker trackerAfterCapture = tracker;
    applyMoveToTracker(trackerAfterCapture, dxe5);

    // Now if someone tries to put a piece in SLOT_PAWN_E again,
    // isPieceAlive returns false
    TEST_ASSERT(!trackerAfterCapture.isPieceAlive(false, SLOT_PAWN_E),
        "Black SLOT_PAWN_E is dead after capture");

    // matchLegalMove with a dead piece's slot should return false
    // (This simulates the player putting a piece in the wrong slot
    //  on a subsequent move)
    parseFEN("rnbqkbnr/pppp1ppp/8/4P3/8/8/PPP1PPPP/RNBQKBNR b KQkq - 0 2");
    before = occupancy[both];

    // Black tries to capture: say Nc6 (not a capture), but player
    // accidentally puts something in SLOT_PAWN_E which is already dead
    int nc6 = findLegalMove(b8, c6);
    TEST_ASSERT(nc6 != 0, "Nc6 is legal");
    Bitboard afterNc6 = predictOccupancy(before, nc6);

    {
        int matched = 0;
        // SLOT_PAWN_E is dead → this should fail in waitForLegalMove's
        // validation, but at the matchLegalMove level it returns false
        // because no capture targets the dead piece's (non-existent) square
        bool found = scanner.testMatchLegalMove(before, afterNc6,
                     SLOT_PAWN_E, false, trackerAfterCapture, matched);
        TEST_ASSERT(!found,
            "Dead piece slot → matchLegalMove returns false");
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
    testPieceTracker();
    testPieceTrackerCastling();
    testWrongSlotAlreadyCaptured();

    printf("\n" ANSI_BOLD "═══ Results: %d/%d passed" ANSI_RESET, testsPassed, testsRun);
    if (testsFailed > 0) {
        printf(ANSI_RED " (%d failed)" ANSI_RESET, testsFailed);
    }
    printf("\n\n");

    return (testsFailed == 0) ? 0 : 1;
}
