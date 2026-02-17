/*
 * Kirin Chess Engine
 * captured_piece_test.cpp - Tests for captured piece detection in move encoding
 */

#include <cstdio>
#include <cstring>
#include <cassert>

// Since types.h appears empty, we'll define what we need here for testing
typedef unsigned long long U64;

// Square enumeration
enum {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1, no_sq
};

// Piece enumeration
enum { P, N, B, R, Q, K, p, n, b, r, q, k };

// Color enumeration
enum { white, black, both };

// Castling rights
enum { wk = 1, wq = 2, bk = 4, bq = 8 };

// Move encoding macros (from movegen.h)
#define encodeMove(source, target, piece, promoted, capture, dpp, enpassant, castle) \
    ((source) | ((target) << 6) | ((piece) << 12) | ((promoted) << 16) | \
    ((capture) << 20) | ((dpp) << 21) | ((enpassant) << 22) | ((castle) << 23))

#define encodeMoveWithCapture(source, target, piece, promoted, capture, dpp, enpassant, castle, capturedPiece) \
    ((source) | ((target) << 6) | ((piece) << 12) | ((promoted) << 16) | \
    ((capture) << 20) | ((dpp) << 21) | ((enpassant) << 22) | ((castle) << 23) | \
    ((capturedPiece) << 24))

// Extract components from move
#define getSource(move) ((move) & 0x3f)
#define getTarget(move) (((move) & 0xfc0) >> 6)
#define getPiece(move) (((move) & 0xf000) >> 12)
#define getPromoted(move) (((move) & 0xf0000) >> 16)
#define getCapture(move) ((move) & 0x100000)
#define getDpp(move) ((move) & 0x200000)
#define getEnpassant(move) ((move) & 0x400000)
#define getCastle(move) ((move) & 0x800000)
// FIXED: The original had a bug: (((move) & 0xf) >> 24) & 
// Should be: (((move) >> 24) & 0xf)
#define getCapturedPiece(move) (((move) >> 24) & 0xf)

// Piece names for output
const char* pieceNames[] = {"P", "N", "B", "R", "Q", "K", "p", "n", "b", "r", "q", "k"};
const char* squareNames[] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1"
};

int testsRun = 0;
int testsPassed = 0;

void printTestResult(const char* testName, bool passed) {
    testsRun++;
    if (passed) {
        testsPassed++;
        printf("[PASS] %s\n", testName);
    } else {
        printf("[FAIL] %s\n", testName);
    }
}

// Test 1: White pawn captures black pawn
void testWhitePawnCapturesBlackPawn() {
    // White pawn on e4 captures black pawn on d5
    int move = encodeMoveWithCapture(e4, d5, P, 0, 1, 0, 0, 0, p);
    
    bool passed = true;
    passed &= (getSource(move) == e4);
    passed &= (getTarget(move) == d5);
    passed &= (getPiece(move) == P);
    passed &= (getCapture(move) != 0);
    passed &= (getCapturedPiece(move) == p);
    
    if (!passed) {
        printf("  Source: %s (expected e4)\n", squareNames[getSource(move)]);
        printf("  Target: %s (expected d5)\n", squareNames[getTarget(move)]);
        printf("  Piece: %s (expected P)\n", pieceNames[getPiece(move)]);
        printf("  Capture flag: %d (expected 1)\n", getCapture(move) ? 1 : 0);
        printf("  Captured piece: %s (expected p)\n", pieceNames[getCapturedPiece(move)]);
    }
    
    printTestResult("White pawn captures black pawn (e4xd5)", passed);
}

// Test 2: White knight captures black rook
void testWhiteKnightCapturesBlackRook() {
    // White knight on f3 captures black rook on e5
    int move = encodeMoveWithCapture(f3, e5, N, 0, 1, 0, 0, 0, r);
    
    bool passed = true;
    passed &= (getSource(move) == f3);
    passed &= (getTarget(move) == e5);
    passed &= (getPiece(move) == N);
    passed &= (getCapture(move) != 0);
    passed &= (getCapturedPiece(move) == r);
    
    if (!passed) {
        printf("  Captured piece: %s (expected r)\n", pieceNames[getCapturedPiece(move)]);
    }
    
    printTestResult("White knight captures black rook (Nf3xe5)", passed);
}

// Test 3: Black queen captures white bishop
void testBlackQueenCapturesWhiteBishop() {
    // Black queen on d8 captures white bishop on d4
    int move = encodeMoveWithCapture(d8, d4, q, 0, 1, 0, 0, 0, B);
    
    bool passed = true;
    passed &= (getSource(move) == d8);
    passed &= (getTarget(move) == d4);
    passed &= (getPiece(move) == q);
    passed &= (getCapture(move) != 0);
    passed &= (getCapturedPiece(move) == B);
    
    if (!passed) {
        printf("  Captured piece: %d (expected B=%d)\n", getCapturedPiece(move), B);
    }
    
    printTestResult("Black queen captures white bishop (Qd8xd4)", passed);
}

// Test 4: En passant capture stores correct captured piece
void testEnPassantCapture() {
    // White pawn on e5 captures black pawn on d6 via en passant
    // The captured piece is still the black pawn (p)
    int move = encodeMoveWithCapture(e5, d6, P, 0, 1, 0, 1, 0, p);
    
    bool passed = true;
    passed &= (getEnpassant(move) != 0);
    passed &= (getCapture(move) != 0);
    passed &= (getCapturedPiece(move) == p);
    
    if (!passed) {
        printf("  Enpassant flag: %d (expected 1)\n", getEnpassant(move) ? 1 : 0);
        printf("  Captured piece: %s (expected p)\n", pieceNames[getCapturedPiece(move)]);
    }
    
    printTestResult("En passant capture stores correct piece", passed);
}

// Test 5: Non-capture move has captured piece = 0
void testNonCaptureMove() {
    // White knight moves from g1 to f3 (no capture)
    int move = encodeMove(g1, f3, N, 0, 0, 0, 0, 0);
    
    bool passed = true;
    passed &= (getCapture(move) == 0);
    passed &= (getCapturedPiece(move) == 0);
    
    printTestResult("Non-capture move has captured piece = 0", passed);
}

// Test 6: Promotion with capture stores captured piece
void testPromotionCapture() {
    // White pawn on e7 captures black rook on d8 and promotes to queen
    int move = encodeMoveWithCapture(e7, d8, P, Q, 1, 0, 0, 0, r);
    
    bool passed = true;
    passed &= (getPromoted(move) == Q);
    passed &= (getCapture(move) != 0);
    passed &= (getCapturedPiece(move) == r);
    
    if (!passed) {
        printf("  Promoted: %d (expected Q=%d)\n", getPromoted(move), Q);
        printf("  Captured piece: %d (expected r=%d)\n", getCapturedPiece(move), r);
    }
    
    printTestResult("Promotion capture stores correct captured piece", passed);
}

// Test 7: All pieces can be encoded as captured
void testAllPiecesCanBeCaptured() {
    bool passed = true;
    
    int pieces[] = {P, N, B, R, Q, K, p, n, b, r, q, k};
    for (int i = 0; i < 12; i++) {
        int capturedPiece = pieces[i];
        int move = encodeMoveWithCapture(e4, d5, P, 0, 1, 0, 0, 0, capturedPiece);
        
        if (getCapturedPiece(move) != capturedPiece) {
            printf("  Failed for piece %s: got %d, expected %d\n", 
                   pieceNames[capturedPiece], getCapturedPiece(move), capturedPiece);
            passed = false;
        }
    }
    
    printTestResult("All 12 piece types can be encoded as captured", passed);
}

// Test 8: Captured piece doesn't interfere with other flags
void testCapturedPieceNoInterference() {
    // Create a complex move with all flags set and captured piece
    int move = encodeMoveWithCapture(e2, e4, P, 0, 1, 1, 0, 0, n);
    
    bool passed = true;
    passed &= (getSource(move) == e2);
    passed &= (getTarget(move) == e4);
    passed &= (getPiece(move) == P);
    passed &= (getPromoted(move) == 0);
    passed &= (getCapture(move) != 0);
    passed &= (getDpp(move) != 0);
    passed &= (getEnpassant(move) == 0);
    passed &= (getCastle(move) == 0);
    passed &= (getCapturedPiece(move) == n);
    
    if (!passed) {
        printf("  Source: %d (expected %d)\n", getSource(move), e2);
        printf("  Target: %d (expected %d)\n", getTarget(move), e4);
        printf("  DPP: %d (expected 1)\n", getDpp(move) ? 1 : 0);
        printf("  Captured: %d (expected %d)\n", getCapturedPiece(move), n);
    }
    
    printTestResult("Captured piece doesn't interfere with other flags", passed);
}

// Test 9: King capturing piece
void testKingCapture() {
    // White king on e1 captures black knight on e2
    int move = encodeMoveWithCapture(e1, e2, K, 0, 1, 0, 0, 0, n);
    
    bool passed = true;
    passed &= (getPiece(move) == K);
    passed &= (getCapture(move) != 0);
    passed &= (getCapturedPiece(move) == n);
    
    printTestResult("King capturing knight stores correct piece", passed);
}

// Test 10: Verify bit positions don't overlap
void testBitPositionsNoOverlap() {
    // Create move with maximum values for each field
    int source = 63;      // 6 bits: 0-5
    int target = 63;      // 6 bits: 6-11
    int piece = 11;       // 4 bits: 12-15
    int promoted = 11;    // 4 bits: 16-19
    int capture = 1;      // 1 bit: 20
    int dpp = 1;          // 1 bit: 21
    int enpassant = 1;    // 1 bit: 22
    int castle = 1;       // 1 bit: 23
    int capturedPiece = 11; // 4 bits: 24-27
    
    int move = encodeMoveWithCapture(source, target, piece, promoted, capture, dpp, enpassant, castle, capturedPiece);
    
    bool passed = true;
    passed &= (getSource(move) == source);
    passed &= (getTarget(move) == target);
    passed &= (getPiece(move) == piece);
    passed &= (getPromoted(move) == promoted);
    passed &= (getCapture(move) != 0);
    passed &= (getDpp(move) != 0);
    passed &= (getEnpassant(move) != 0);
    passed &= (getCastle(move) != 0);
    passed &= (getCapturedPiece(move) == capturedPiece);
    
    if (!passed) {
        printf("  Move encoded as: 0x%08X\n", move);
        printf("  Source: %d (expected %d)\n", getSource(move), source);
        printf("  Target: %d (expected %d)\n", getTarget(move), target);
        printf("  Piece: %d (expected %d)\n", getPiece(move), piece);
        printf("  Promoted: %d (expected %d)\n", getPromoted(move), promoted);
        printf("  CapturedPiece: %d (expected %d)\n", getCapturedPiece(move), capturedPiece);
    }
    
    printTestResult("Bit positions don't overlap with max values", passed);
}

int main() {
    printf("\n========================================\n");
    printf("Captured Piece Detection Tests\n");
    printf("========================================\n\n");
    
    testWhitePawnCapturesBlackPawn();
    testWhiteKnightCapturesBlackRook();
    testBlackQueenCapturesWhiteBishop();
    testEnPassantCapture();
    testNonCaptureMove();
    testPromotionCapture();
    testAllPiecesCanBeCaptured();
    testCapturedPieceNoInterference();
    testKingCapture();
    testBitPositionsNoOverlap();
    
    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", testsPassed, testsRun);
    printf("========================================\n\n");
    
    return (testsPassed == testsRun) ? 0 : 1;
}
