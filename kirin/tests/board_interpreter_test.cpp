/*
 * Simplified Test Suite for Kirin Board Interpreter
 * Uses a minimal set of carefully designed bitboards to test all key functionality
 */

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include "board_interpreter.h"

/************ Test Framework ************/
static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_BOLD          "\x1b[1m"

#define TEST_ASSERT(condition, message) \
  do { \
    testsRun++; \
    if (condition) { \
      testsPassed++; \
      printf("  " ANSI_COLOR_GREEN "✓ %s" ANSI_COLOR_RESET "\n", message); \
    } else { \
      testsFailed++; \
      printf("  " ANSI_COLOR_RED "✗ %s" ANSI_COLOR_RESET "\n", message); \
    } \
  } while(0)

#define TEST_SECTION(name) \
  printf("\n" ANSI_COLOR_CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" ANSI_COLOR_RESET "\n"); \
  printf("  " ANSI_BOLD "%s" ANSI_COLOR_RESET "\n", name); \
  printf(ANSI_COLOR_CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" ANSI_COLOR_RESET "\n\n")

/************ Position Setup Helpers ************/

// Parse piece placement: "Pa2 Pb2 Nc3 Ke1" etc.
Bitboard parsePosition(const char* pieces) {
  Bitboard occ = 0;
  const char* p = pieces;
  
  while (*p) {
    while (*p == ' ') p++;
    if (!*p) break;
    
    // Skip piece character
    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) p++;
    
    // Parse square
    if (*p >= 'a' && *p <= 'h' && *(p+1) >= '1' && *(p+1) <= '8') {
      BoardCoord coord = BoardCoord::fromFEN(p);
      bb_setBit(occ, coord.toSquareIndex());
      p += 2;
    }
  }
  return occ;
}

// Infer piece type from move pattern
PieceType inferPieceType(const BoardCoord& from, const BoardCoord& to) {
  int rowDiff = std::abs(to.row - from.row);
  int colDiff = std::abs(to.col - from.col);
  
  if ((rowDiff == 2 && colDiff == 1) || (rowDiff == 1 && colDiff == 2)) return KNIGHT;
  if (rowDiff <= 1 && colDiff <= 1) return KING;
  if (colDiff == 0 && rowDiff <= 2) return PAWN;
  if (colDiff == 1 && rowDiff == 1) return PAWN;
  if (rowDiff == colDiff && rowDiff > 1) return BISHOP;
  if ((rowDiff == 0 && colDiff > 0) || (colDiff == 0 && rowDiff > 0)) return ROOK;
  return QUEEN;
}

PhysicalMove createMove(const char* moveStr, PieceType pieceType = UNKNOWN) {
  BoardCoord from = BoardCoord::fromFEN(moveStr);
  BoardCoord to = BoardCoord::fromFEN(moveStr + 2); 
  if (pieceType == UNKNOWN) pieceType = inferPieceType(from, to);
  return PhysicalMove(from, to, pieceType, false);
}

void printTestMove(const MovePlan& plan) {
  if (!plan.isValid) {
    printf("    Error: %s\n", plan.errorMessage ? plan.errorMessage : "Unknown");
    return;
  }
  
  printf("    Path length: %d, Relocations: %zu, Restorations: %zu\n",
         plan.primaryPath.length(), plan.relocations.size(), plan.restorations.size());
}

/************ Test Bitboards ************/

/*
 * BITBOARD 1: Starting Position
 * Tests: Knight moves from corners, pawn pushes, blocked vs clear paths
 * 
 * 8 | r n b q k b n r
 * 7 | p p p p p p p p
 * 6 | . . . . . . . .
 * 5 | . . . . . . . .
 * 4 | . . . . . . . .
 * 3 | . . . . . . . .
 * 2 | P P P P P P P P
 * 1 | R N B Q K B N R
 *     a b c d e f g h
 */
Bitboard startingPosition() {
  Bitboard occ = 0;
  for (int col = 0; col < 8; col++) {
    bb_setBit(occ, 7 * 8 + col);  // Rank 1
    bb_setBit(occ, 6 * 8 + col);  // Rank 2
    bb_setBit(occ, 1 * 8 + col);  // Rank 7
    bb_setBit(occ, 0 * 8 + col);  // Rank 8
  }
  return occ;
}

/*
 * BITBOARD 2: Blocked File/Diagonal Test
 * Tests: Multiple blockers on file, diagonal blockers, parking spot selection
 * 
 * 8 | . . . . . . . .
 * 7 | P . . . . . . .
 * 6 | P . . . . B . .
 * 5 | P . . . . . . .
 * 4 | P . . . P . . .
 * 3 | P . . P . . . .
 * 2 | P . P . . . . .
 * 1 | R . . . . . . B
 *     a b c d e f g h
 */
Bitboard blockedPathsPosition() {
  return parsePosition(
    "Ra1 Pa2 Pa3 Pa4 Pa5 Pa6 Pa7 "  // Rook with 6 blockers on a-file
    "Pc2 Pd3 Pe4 "                   // Diagonal blockers
    "Bf6 Bh1"                        // Bishops for diagonal tests
  );
}

/*
 * BITBOARD 3: Knight Path Selection
 * Tests: Knight choosing between blocked/clear paths, corner knights
 * 
 * 8 | . . . . . . . .
 * 7 | . . . . . . . .
 * 6 | . . . . . . . .
 * 5 | . . . . . . . .
 * 4 | . . . . . . . .
 * 3 | . P . . . . . .
 * 2 | P . P . . . P .
 * 1 | N . . . . . . N
 *     a b c d e f g h
 */
Bitboard knightTestPosition() {
  return parsePosition(
    "Na1 Nh1 "     // Knights in corners
    "Pa2 Pb3 "     // Block some of Na1's paths
    "Pc2 "         // Block another path
    "Pg2"          // Block one of Nh1's paths
  );
}

/*
 * BITBOARD 4: Recursive Blocker / Nested Blocking
 * Tests: Blocker needs to move blocker, chain relocations
 * 
 * 8 | . . . . . . . .
 * 7 | . . . . . . . .
 * 6 | . . . . . . . .
 * 5 | . . . . . . . .
 * 4 | . . . . . . . .
 * 3 | P P P . . . . .
 * 2 | P P P . . . . .
 * 1 | R P P P . . . .
 *     a b c d e f g h
 */
Bitboard nestedBlockersPosition() {
  return parsePosition(
    "Ra1 "
    "Pb1 Pc1 Pd1 "  // First rank blockers
    "Pa2 Pb2 Pc2 "  // Second rank (block escape routes)
    "Pa3 Pb3 Pc3"   // Third rank (more blocking)
  );
}

/*
 * BITBOARD 5: Near-Impossible Position
 * Tests: Edge case where almost no parking spots exist
 * 
 * 8 | . P P P P P P P
 * 7 | P P P P P P P P
 * 6 | P P P P P P P P
 * 5 | P P P P P P P P
 * 4 | P P P P P P P P
 * 3 | P P P P P P P P
 * 2 | P P P P P P P P
 * 1 | R P P P P P P P
 *     a b c d e f g h
 */
Bitboard almostFullPosition() {
  return parsePosition(
    "Ra1 Pb1 Pc1 Pd1 Pe1 Pf1 Pg1 Ph1 "
    "Pa2 Pb2 Pc2 Pd2 Pe2 Pf2 Pg2 Ph2 "
    "Pa3 Pb3 Pc3 Pd3 Pe3 Pf3 Pg3 Ph3 "
    "Pa4 Pb4 Pc4 Pd4 Pe4 Pf4 Pg4 Ph4 "
    "Pa5 Pb5 Pc5 Pd5 Pe5 Pf5 Pg5 Ph5 "
    "Pa6 Pb6 Pc6 Pd6 Pe6 Pf6 Pg6 Ph6 "
    "Pa7 Pb7 Pc7 Pd7 Pe7 Pf7 Pg7 Ph7 "
    "Pb8 Pc8 Pd8 Pe8 Pf8 Pg8 Ph8"  // Only a8 empty
  );
}

/************ Test Suites ************/

void testStartingPosition() {
  TEST_SECTION("BITBOARD 1: Starting Position");
  
  PhysicalBoard board;
  board.setOccupancy(startingPosition());
  printf("Position:\n");
  board.print();
  
  // Knight moves (clear path exists)
  {
    PhysicalMove move = createMove("g1f3", KNIGHT);
    MovePlan plan = planMove(board, move);
    printf("  Ng1-f3: ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Ng1-f3 should be valid");
    
    // Verify path selection avoids f2
    std::vector<Path> paths = generateKnightPath(move.from, move.to);
    bool hasPathAvoidingF2 = false;
    for (const Path& p : paths) {
      bool avoidsF2 = true;
      for (const BoardCoord& sq : p.squares) {
        if (sq.row == 6 && sq.col == 5) avoidsF2 = false;
      }
      if (avoidsF2) hasPathAvoidingF2 = true;
    }
    TEST_ASSERT(hasPathAvoidingF2, "Knight should have path avoiding f2 pawn");
  }
  
  {
    PhysicalMove move = createMove("b1c3", KNIGHT);
    MovePlan plan = planMove(board, move);
    printf("  Nb1-c3: ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Nb1-c3 should be valid");
  }
  
  {
    PhysicalMove move = createMove("b1a3", KNIGHT);
    MovePlan plan = planMove(board, move);
    printf("  Nb1-a3: ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Nb1-a3 should be valid");
  }
  
  // Pawn moves (unblocked)
  {
    PhysicalMove move = createMove("e2e4", PAWN);
    MovePlan plan = planMove(board, move);
    printf("  e2-e4:  ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "e2-e4 should be valid");
    TEST_ASSERT(plan.relocations.empty(), "e2-e4 should have no blockers");
    TEST_ASSERT(plan.primaryPath.length() == 2, "e2-e4 path should be 2 squares");
  }
  
  {
    PhysicalMove move = createMove("d2d3", PAWN);
    MovePlan plan = planMove(board, move);
    printf("  d2-d3:  ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "d2-d3 should be valid");
    TEST_ASSERT(plan.primaryPath.length() == 1, "d2-d3 path should be 1 square");
  }
  
  // King move (1 square)
  {
    // First clear e2 to allow Ke1-e2
    PhysicalBoard tempBoard;
    Bitboard occ = startingPosition();
    bb_clearBit(occ, BoardCoord::fromFEN("e2").toSquareIndex());
    tempBoard.setOccupancy(occ);
    
    PhysicalMove move = createMove("e1e2", KING);
    MovePlan plan = planMove(tempBoard, move);
    printf("  Ke1-e2: ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Ke1-e2 should be valid");
    TEST_ASSERT(plan.primaryPath.length() == 1, "King path should be 1 square");
  }
}

void testBlockedPaths() {
  TEST_SECTION("BITBOARD 2: Blocked File & Diagonal");
  
  PhysicalBoard board;
  board.setOccupancy(blockedPathsPosition());
  printf("Position:\n");
  board.print();
  
  // Rook through 6 blockers
  {
    PhysicalMove move = createMove("a1a8", ROOK);
    MovePlan plan = planMove(board, move);
    printf("  Ra1-a8 (6 blockers): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Ra1-a8 should be valid with relocations");
    TEST_ASSERT(plan.relocations.size() == 6, "Should have 6 relocations");
    TEST_ASSERT(plan.restorations.size() == 6, "Should have 6 restorations");
    
    // Verify restorations are in reverse order
    if (plan.relocations.size() == 6 && plan.restorations.size() == 6) {
      bool reverseOrder = true;
      for (size_t i = 0; i < 6; i++) {
        if (plan.restorations[i].to != plan.relocations[5-i].from) {
          reverseOrder = false;
          break;
        }
      }
      TEST_ASSERT(reverseOrder, "Restorations should be in reverse order");
    }
  }
  
  // Bishop diagonal with blockers
  {
    PhysicalMove move = createMove("h1a8", BISHOP);
    MovePlan plan = planMove(board, move);
    printf("  Bh1-a8 (diagonal blockers): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Bh1-a8 should be valid");
    // Path: h1-g2-f3-e4-d5-c6-b7-a8, blockers at c2? No, at e4,d3,c2
    // Wait, path is h1->a8, so g2,f3,e4,d5,c6,b7 intermediate
    // Our blockers are at c2,d3,e4 which is on the path
    TEST_ASSERT(plan.relocations.size() >= 1, "Should have at least 1 diagonal blocker");
  }
  
  // Clear bishop move
  {
    PhysicalMove move = createMove("f6a1", BISHOP);
    MovePlan plan = planMove(board, move);
    printf("  Bf6-a1 (blocked by Ra1): ");
    printTestMove(plan);
    // Destination a1 has rook - this is a capture, not a blocker
    TEST_ASSERT(plan.isValid, "Bf6-a1 should be valid (capture)");
  }
  
  // Queen combining both directions
  {
    // Test as queen move
    PhysicalBoard qBoard;
    Bitboard occ = parsePosition("Qd1 Pd4 Pd6");
    qBoard.setOccupancy(occ);
    
    PhysicalMove move = createMove("d1d8", QUEEN);
    MovePlan plan = planMove(qBoard, move);
    printf("  Qd1-d8 (2 blockers): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Qd1-d8 should be valid");
    TEST_ASSERT(plan.relocations.size() == 2, "Should have 2 blockers");
  }
}

void testKnightPaths() {
  TEST_SECTION("BITBOARD 3: Knight Path Selection");
  
  PhysicalBoard board;
  board.setOccupancy(knightTestPosition());
  printf("Position:\n");
  board.print();
  
  // Na1 with some paths blocked
  {
    PhysicalMove move = createMove("a1b3", KNIGHT);
    MovePlan plan = planMove(board, move);
    printf("  Na1-b3 (dest occupied): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Na1-b3 should be valid (capture)");
  }
  
  {
    PhysicalMove move = createMove("a1c2", KNIGHT);
    MovePlan plan = planMove(board, move);
    printf("  Na1-c2 (dest occupied): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Na1-c2 should be valid (capture)");
  }
  
  // Nh1-f2 with one path blocked (g2 has pawn)
  {
    PhysicalMove move = createMove("h1f2", KNIGHT);
    MovePlan plan = planMove(board, move);
    printf("  Nh1-f2: ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Nh1-f2 should be valid");
    
    // Check if optimal path was chosen (avoiding g2)
    bool pathAvoidsG2 = true;
    for (const BoardCoord& sq : plan.primaryPath.squares) {
      if (sq.row == 6 && sq.col == 6) pathAvoidsG2 = false;
    }
    TEST_ASSERT(pathAvoidsG2 || plan.relocations.size() > 0, 
                "Should either avoid g2 or relocate it");
  }
  
  {
    PhysicalMove move = createMove("h1g3", KNIGHT);
    MovePlan plan = planMove(board, move);
    printf("  Nh1-g3: ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Nh1-g3 should be valid");
  }
}

void testNestedBlockers() {
  TEST_SECTION("BITBOARD 4: Nested/Recursive Blockers");
  
  PhysicalBoard board;
  board.setOccupancy(nestedBlockersPosition());
  printf("Position:\n");
  board.print();
  
  // Ra1-e1: blockers b1,c1,d1 need to move, but their paths are blocked too
  {
    PhysicalMove move = createMove("a1e1", ROOK);
    MovePlan plan = planMove(board, move);
    printf("  Ra1-e1 (nested blockers): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Ra1-e1 should be valid with recursive relocation");
    TEST_ASSERT(plan.relocations.size() >= 3, "Should have at least 3 blockers");
    
    if (plan.isValid) {
      printf("    Relocation sequence:\n");
      for (size_t i = 0; i < plan.relocations.size(); i++) {
        char from[3], to[3];
        plan.relocations[i].from.toFEN(from);
        plan.relocations[i].to.toFEN(to);
        printf("      %zu: %s -> %s\n", i + 1, from, to);
      }
    }
  }
  
  // Shorter move with fewer blockers
  {
    PhysicalMove move = createMove("a1c1", ROOK);
    MovePlan plan = planMove(board, move);
    printf("  Ra1-c1 (2 blockers): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Ra1-c1 should be valid");
  }
  
  // Test multiple blocker ordering
  {
    PhysicalBoard simpleBoard;
    simpleBoard.setOccupancy(parsePosition("Ra1 Pb1 Pc1 Pd1"));
    
    PhysicalMove move = createMove("a1e1", ROOK);
    MovePlan plan = planMove(simpleBoard, move);
    printf("  Ra1-e1 (simple 3 blockers): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Simple a1e1 should be valid");
    TEST_ASSERT(plan.relocations.size() == 3, "Should have exactly 3 relocations");
    TEST_ASSERT(plan.restorations.size() == 3, "Should have exactly 3 restorations");
  }
}

void testEdgeCases() {
  TEST_SECTION("BITBOARD 5: Edge Cases");
  
  // Nearly full board - should fail gracefully
  {
    PhysicalBoard board;
    board.setOccupancy(almostFullPosition());
    printf("Position (nearly full):\n");
    board.print();
    
    PhysicalMove move = createMove("a1a8", ROOK);
    MovePlan plan = planMove(board, move);
    printf("  Ra1-a8 (no parking): ");
    printTestMove(plan);
    TEST_ASSERT(!plan.isValid, "Should fail when no parking spots exist");
  }
  
  // Surrounded piece with escape route
  {
    PhysicalBoard board;
    board.setOccupancy(parsePosition(
      "Rd4 Pc4 Pd3 Pe4 Pd5 "  // Surrounded orthogonally
      "Pc3 Pe3 Pc5 Pe5"        // Diagonal squares occupied
    ));
    printf("\nPosition (surrounded rook):\n");
    board.print();
    
    PhysicalMove move = createMove("d4d8", ROOK);
    MovePlan plan = planMove(board, move);
    printf("  Rd4-d8 (surrounded): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Rd4-d8 should be valid");
    TEST_ASSERT(plan.relocations.size() == 1, "Should have 1 blocker (d5)");
  }
  
  // Clear moves (sanity checks)
  {
    PhysicalBoard board;
    board.setOccupancy(parsePosition("Qd1"));
    
    PhysicalMove move = createMove("d1h5", QUEEN);
    MovePlan plan = planMove(board, move);
    printf("\n  Qd1-h5 (clear diagonal): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Clear queen move should be valid");
    TEST_ASSERT(plan.relocations.empty(), "Should have no blockers");
    TEST_ASSERT(plan.primaryPath.length() == 4, "Diagonal path should be 4 squares");
  }
  
  // Castling path
  {
    PhysicalBoard board;
    board.setOccupancy(parsePosition("Ke1 Rh1"));
    
    PhysicalMove move = createMove("e1g1", KING);
    MovePlan plan = planMove(board, move);
    printf("  Ke1-g1 (castling): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Castling move should be valid");
    TEST_ASSERT(plan.relocations.empty(), "Clear castling should have no blockers");
  }
  
  // Castling blocked
  {
    PhysicalBoard board;
    board.setOccupancy(parsePosition("Ke1 Bf1 Ng1 Rh1"));
    
    PhysicalMove move = createMove("e1g1", KING);
    MovePlan plan = planMove(board, move);
    printf("  Ke1-g1 (blocked castle): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Blocked castling should still be valid with relocation");
  }
}

void testAStarPathfinding() {
  TEST_SECTION("A* Pathfinding Verification");
  
  // Test that blockers navigate around obstacles
  {
    PhysicalBoard board;
    board.setOccupancy(parsePosition(
      "Rd1 Pd4 "      // Rook and blocker
      "Pe3 Pe4 Pe5 Pf4"  // Obstacles around d4
    ));
    printf("Position (maze-like):\n");
    board.print();
    
    PhysicalMove move = createMove("d1d8", ROOK);
    MovePlan plan = planMove(board, move);
    printf("  Rd1-d8 (blocker must navigate): ");
    printTestMove(plan);
    TEST_ASSERT(plan.isValid, "Should find path for blocker");
    
    if (plan.isValid && !plan.relocations.empty()) {
      printf("    Blocker path length: %d\n", plan.relocations[0].path.length());
      TEST_ASSERT(plan.relocations[0].path.length() > 0, "Blocker should have valid path");
    }
  }
}

/************ Main ************/
int main() {
  printf("\n");
  printf(ANSI_COLOR_MAGENTA "╔═══════════════════════════════════════════════════════════════════╗\n");
  printf("║" ANSI_COLOR_RESET ANSI_BOLD "     KIRIN BOARD INTERPRETER - SIMPLIFIED TEST SUITE               " ANSI_COLOR_RESET ANSI_COLOR_MAGENTA "║\n");
  printf("╚═══════════════════════════════════════════════════════════════════╝" ANSI_COLOR_RESET "\n");
  
  testStartingPosition();
  testBlockedPaths();
  testKnightPaths();
  testNestedBlockers();
  testEdgeCases();
  testAStarPathfinding();
  
  // Print summary
  printf("\n");
  printf(ANSI_COLOR_CYAN "═══════════════════════════════════════════════════════════════════" ANSI_COLOR_RESET "\n");
  printf(ANSI_BOLD "                         TEST SUMMARY\n" ANSI_COLOR_RESET);
  printf(ANSI_COLOR_CYAN "═══════════════════════════════════════════════════════════════════" ANSI_COLOR_RESET "\n");
  printf("  Tests Run:    %d\n", testsRun);
  printf("  Tests Passed: " ANSI_COLOR_GREEN "%d" ANSI_COLOR_RESET "\n", testsPassed);
  if (testsFailed > 0) {
    printf("  Tests Failed: " ANSI_COLOR_RED "%d" ANSI_COLOR_RESET "\n", testsFailed);
  } else {
    printf("  Tests Failed: %d\n", testsFailed);
  }
  
  double passRate = testsRun > 0 ? (100.0 * testsPassed / testsRun) : 0.0;
  if (passRate == 100.0) {
    printf("  Pass Rate:    " ANSI_COLOR_GREEN ANSI_BOLD "%.1f%%" ANSI_COLOR_RESET "\n", passRate);
  } else if (passRate >= 80.0) {
    printf("  Pass Rate:    " ANSI_COLOR_YELLOW "%.1f%%" ANSI_COLOR_RESET "\n", passRate);
  } else {
    printf("  Pass Rate:    " ANSI_COLOR_RED "%.1f%%" ANSI_COLOR_RESET "\n", passRate);
  }
  printf(ANSI_COLOR_CYAN "═══════════════════════════════════════════════════════════════════" ANSI_COLOR_RESET "\n\n");
  
  return testsFailed > 0 ? 1 : 0;
}
