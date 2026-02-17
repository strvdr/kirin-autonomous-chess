# Kirin Implementation Plan: Path to Hardware Testing

## Overview

This document outlines the remaining work to get Kirin from its current state to hardware testing. The core components (engine, board interpreter, gantry controller) are complete — we need integration code and special case handling.

---

## Phase 1: Captured Piece Tracking

**Problem:** When a capture occurs, we need to know what piece type is being captured to place it in the correct capture zone slot. The engine knows this information but doesn't expose it cleanly.

### Task 1.1: Add Captured Piece to Move Encoding

Modify the move encoding to include the captured piece type.

**File:** `movegen.h`
```cpp
// Current encoding uses bits 0-23
// Add captured piece in bits 24-27 (4 bits for piece type 0-11)

#define encodeMoveWithCapture(source, target, piece, promoted, capture, dpp, enpassant, castle, capturedPiece) \
    ((source) | ((target) << 6) | ((piece) << 12) | ((promoted) << 16) | \
    ((capture) << 20) | ((dpp) << 21) | ((enpassant) << 22) | ((castle) << 23) | \
    ((capturedPiece) << 24))

#define getCapturedPiece(move) (((move) >> 24) & 0xf)
```

**File:** `movegen.cpp`
- Update `generateMoves()` to detect captured piece when generating capture moves
- Store captured piece type in the move encoding

### Task 1.2: Simplify Capture Zone Logic

**File:** `gantry_controller.h`
- Remove `CaptureSlot` enum complexity
- Simplify to two counters per side: `pawnsCaptured`, `piecesCaptured`

**File:** `gantry_controller.cpp`
```cpp
// New simplified approach
Position getNextCaptureSlot(bool isWhitePiece, bool isPawn) {
    // Returns next available slot in appropriate column
    // Pawns: column 2, Back-rank pieces: column 1
    // Y position based on how many already captured
}
```

**Update:** `generateMovePlanGcode()` signature:
```cpp
// FROM:
generateMovePlanGcode(const MovePlan& plan,
                      bool capturedPieceIsWhite,
                      int capturedPieceFile,      // REMOVE
                      bool capturedPieceIsPawn)

// TO:
generateMovePlanGcode(const MovePlan& plan,
                      bool capturedPieceIsWhite,
                      PieceType capturedPieceType)  // SIMPLER
```

---

## Phase 2: Engine ↔ Board Interpreter Integration

**Problem:** No code currently bridges the engine's output to the board interpreter's input.

### Task 2.1: Create Piece Type Conversion

**File:** `game_controller.h` (NEW)
```cpp
// Convert engine piece enum (P,N,B,R,Q,K,p,n,b,r,q,k) to interpreter PieceType
PieceType engineToPhysicalPiece(int enginePiece);

// Convert interpreter PieceType back if needed
int physicalToEnginePiece(PieceType pt, bool isWhite);
```

### Task 2.2: Create Move Conversion

**File:** `game_controller.h`
```cpp
// Convert engine move (int with encoded bits) to PhysicalMove
PhysicalMove engineToPhysicalMove(int engineMove);
```

This function:
1. Extracts source/target squares from move encoding
2. Extracts piece type
3. Extracts capture flag
4. Creates and returns `PhysicalMove` struct

---

## Phase 3: Special Move Handling

### Task 3.1: Castling

**Problem:** Castling moves two pieces (king and rook), but `MovePlan` assumes one primary piece.

**Solution:** Detect castling and generate two sequential `MovePlan`s.

**File:** `game_controller.cpp`
```cpp
std::vector<MovePlan> planCastling(PhysicalBoard& board, int engineMove) {
    std::vector<MovePlan> plans;
    
    bool isKingside = (getTarget(engineMove) == g1 || getTarget(engineMove) == g8);
    
    // Plan 1: Move the rook first (to avoid blocking king's path)
    PhysicalMove rookMove;
    if (isKingside) {
        // h1->f1 or h8->f8
    } else {
        // a1->d1 or a8->d8
    }
    plans.push_back(planMove(board, rookMove));
    
    // Update board state
    board.movePiece(rookMove.from, rookMove.to);
    
    // Plan 2: Move the king
    PhysicalMove kingMove = engineToPhysicalMove(engineMove);
    plans.push_back(planMove(board, kingMove));
    
    return plans;
}
```

**Execution order:**
1. Move rook to its destination
2. Move king to its destination

### Task 3.2: En Passant

**Problem:** The captured pawn is NOT on the destination square — it's on the square the capturing pawn passes through.

**Solution:** Detect en passant and handle the capture from the correct square.

**File:** `game_controller.cpp`
```cpp
MovePlan planEnPassant(PhysicalBoard& board, int engineMove) {
    PhysicalMove primaryMove = engineToPhysicalMove(engineMove);
    
    // The captured pawn is on the same file as destination, 
    // but same rank as the capturing pawn's origin
    BoardCoord capturedPawnSquare;
    capturedPawnSquare.col = primaryMove.to.col;  // Same file as destination
    capturedPawnSquare.row = primaryMove.from.row; // Same rank as origin
    
    // Generate MovePlan for the pawn move itself
    MovePlan plan = planMove(board, primaryMove);
    
    // Store the actual capture square (not destination)
    plan.enPassantCaptureSquare = capturedPawnSquare;  // NEW FIELD
    plan.isEnPassant = true;  // NEW FIELD
    
    return plan;
}
```

**File:** `board_interpreter.h`
- Add `BoardCoord enPassantCaptureSquare` to `MovePlan`
- Add `bool isEnPassant` flag to `MovePlan`

**File:** `gantry_controller.cpp`
- Update `generateMovePlanGcode()` to check `isEnPassant`
- If true, capture from `enPassantCaptureSquare` instead of `primaryMove.to`

### Task 3.3: Pawn Promotion

**Problem:** When a pawn promotes, we need to:
1. Move the pawn to the promotion square
2. Remove the pawn (put in capture zone? or separate "promoted pawns" area?)
3. Place the promoted piece (from where?)

**Discussion needed:** How will promotion work physically?

**Option A: Piece Swap**
- Have spare queens/rooks/bishops/knights in a "promotion zone"
- Move pawn to capture zone, move promoted piece to destination
- Requires: Extra pieces and storage area

**Option B: Pawn Stays (simplified for prototype)**
- Pawn physically stays on the board, logically becomes queen
- Player mentally tracks it as a queen
- Requires: Nothing extra, but less realistic

**Recommendation:** Start with Option B for initial hardware testing, implement Option A later.

**File:** `game_controller.cpp`
```cpp
// For now, just move the pawn normally
// The engine tracks it as a promoted piece
MovePlan planPromotion(PhysicalBoard& board, int engineMove) {
    PhysicalMove move = engineToPhysicalMove(engineMove);
    // Ignore promotion piece type for physical movement
    // Just move pawn to last rank
    return planMove(board, move);
}
```

---

## Phase 4: Main Game Loop

### Task 4.1: Create Game Controller

**File:** `game_controller.h` (NEW)
```cpp
#ifndef KIRIN_GAME_CONTROLLER_H
#define KIRIN_GAME_CONTROLLER_H

#include "engine.h"
#include "board_interpreter.h"
#include "gantry_controller.h"

class GameController {
private:
    Gantry::GrblController gantry;
    PhysicalBoard physicalBoard;
    
    // Capture tracking
    int whitePawnsCaptured;
    int whitePiecesCaptured;
    int blackPawnsCaptured;
    int blackPiecesCaptured;
    
    // State
    bool gameInProgress;
    bool waitingForHuman;  // If human plays one side
    
public:
    GameController();
    
    // Setup
    bool connectHardware(const char* port);
    bool homeGantry();
    bool setupNewGame();
    
    // Game flow
    bool executeEngineMove(int engineMove);
    bool executeHumanMove(const char* moveStr);  // For human input
    
    // Special moves
    bool executeCastling(int engineMove);
    bool executeEnPassant(int engineMove);
    bool executePromotion(int engineMove);
    
    // State
    void updatePhysicalBoard();
    bool isGameOver();
};

#endif
```

### Task 4.2: Implement Main Loop

**File:** `game_controller.cpp` (NEW)
```cpp
bool GameController::executeEngineMove(int engineMove) {
    // 1. Detect special moves
    if (getCastle(engineMove)) {
        return executeCastling(engineMove);
    }
    if (getEnpassant(engineMove)) {
        return executeEnPassant(engineMove);
    }
    if (getPromoted(engineMove)) {
        return executePromotion(engineMove);
    }
    
    // 2. Convert to physical move
    PhysicalMove physMove = engineToPhysicalMove(engineMove);
    
    // 3. Plan the move
    MovePlan plan = planMove(physicalBoard, physMove);
    if (!plan.isValid) {
        printf("Error: %s\n", plan.errorMessage);
        return false;
    }
    
    // 4. Handle capture
    PieceType capturedType = UNKNOWN;
    bool capturedIsWhite = false;
    if (getCapture(engineMove)) {
        int capturedPiece = getCapturedPiece(engineMove);
        capturedType = engineToPhysicalPiece(capturedPiece);
        capturedIsWhite = (capturedPiece < 6);  // P,N,B,R,Q,K are white
    }
    
    // 5. Execute on hardware
    if (!gantry.executeMove(plan, capturedIsWhite, capturedType)) {
        return false;
    }
    
    // 6. Update physical board state
    physicalBoard.movePiece(physMove.from, physMove.to);
    
    return true;
}
```

### Task 4.3: Main Entry Point

**File:** `main.cpp` (MODIFY)

Add new mode for physical board play:
```cpp
int main(int argc, char* argv[]) {
    initAll();
    
    if (argc > 1 && strcmp(argv[1], "--physical") == 0) {
        // Physical board mode
        GameController game;
        
        if (!game.connectHardware("/dev/ttyUSB0")) {
            printf("Failed to connect to gantry\n");
            return 1;
        }
        
        game.homeGantry();
        game.setupNewGame();
        
        // Game loop
        while (!game.isGameOver()) {
            // Engine thinks
            searchPosition(6);  // Or configurable depth
            int bestMove = pvTable[0][0];
            
            // Execute on hardware
            game.executeEngineMove(bestMove);
            
            // Update engine state
            makeMove(bestMove, allMoves);
            
            // TODO: Wait for human move input
        }
    } else {
        // Standard UCI mode
        uciLoop();
    }
    
    return 0;
}
```

---

## Phase 5: Restoration Path Computation

**Problem:** `MovePlan.restorations` stores from/to but the path is empty — comment says "computed at execution time" but this isn't implemented.

### Task 5.1: Compute Restoration Paths

**File:** `gantry_controller.cpp`

Update `generateMovePlanGcode()`:
```cpp
// Step 4: Restore all blockers to original positions
for (const RelocationPlan& restore : plan.restorations) {
    Position fromPos = toPhysical(restore.from);
    Position toPos = toPhysical(restore.to);
    
    // Pick up from parking spot
    gcode.push_back(moveCommand(fromPos));
    gcode.push_back(magnetOn());
    gcode.push_back(dwell(100));
    
    // If restoration has a path, follow it
    if (!restore.path.empty()) {
        std::vector<std::string> pathGcode = generatePathGcode(fromPos, restore.path);
        gcode.insert(gcode.end(), pathGcode.begin(), pathGcode.end());
    } else {
        // Direct move (current behavior)
        gcode.push_back(moveCommand(toPos));
    }
    
    // Release
    gcode.push_back(magnetOff());
    gcode.push_back(dwell(100));
}
```

**Alternative:** Compute restoration paths in `planMove()` after the primary move is planned, using the post-primary-move board state.

---

## Phase 6: Error Handling

### Task 6.1: GRBL Error Detection

**File:** `gantry_controller.cpp`
```cpp
bool GrblController::waitForOk(int timeoutMs) {
    std::string response = readLine(timeoutMs);
    
    if (response == "ok") {
        return true;
    }
    
    if (response.find("error") != std::string::npos) {
        printf("GRBL Error: %s\n", response.c_str());
        return false;
    }
    
    if (response.find("ALARM") != std::string::npos) {
        printf("GRBL Alarm: %s\n", response.c_str());
        // May need to home or reset
        return false;
    }
    
    // Timeout or unexpected response
    printf("Unexpected GRBL response: %s\n", response.c_str());
    return false;
}
```

### Task 6.2: Move Execution Recovery

**File:** `game_controller.cpp`
```cpp
bool GameController::executeEngineMove(int engineMove) {
    // ... planning code ...
    
    // Execute with retry
    int maxRetries = 3;
    for (int attempt = 0; attempt < maxRetries; attempt++) {
        if (gantry.executeMove(plan, capturedIsWhite, capturedType)) {
            // Success
            physicalBoard.movePiece(physMove.from, physMove.to);
            return true;
        }
        
        printf("Move execution failed, attempt %d/%d\n", attempt + 1, maxRetries);
        
        // Try to recover - home and retry
        if (!gantry.home()) {
            printf("Failed to home gantry\n");
            break;
        }
    }
    
    return false;
}
```

### Task 6.3: Graceful Shutdown

**File:** `game_controller.cpp`
```cpp
GameController::~GameController() {
    // Ensure magnet is off
    gantry.setMagnet(false);
    
    // Move to safe position
    gantry.moveTo(Gantry::Position(11.0, 11.0));  // Center of board
    
    // Disconnect
    gantry.disconnect();
}
```

---

## Phase 7: Testing & Validation

### Task 7.1: Dry Run Mode

Add a mode that generates G-code without sending to hardware:

**File:** `gantry_controller.h`
```cpp
class GrblController {
    bool dryRunMode;
public:
    void setDryRun(bool enabled) { dryRunMode = enabled; }
    // In sendCommand: if dryRunMode, just print instead of sending
};
```

### Task 7.2: Integration Tests

**File:** `game_controller_test.cpp` (NEW)
- Test move conversion (engine → physical)
- Test special move detection
- Test capture slot tracking
- Test full game simulation (dry run)

---

## Implementation Order

| Priority | Task | Estimated Time | Dependencies |
|----------|------|----------------|--------------|
| 1 | Task 1.1: Captured piece encoding | 1-2 hours | None |
| 2 | Task 1.2: Simplify capture zones | 1 hour | Task 1.1 |
| 3 | Task 2.1-2.2: Move conversion | 1-2 hours | Task 1.1 |
| 4 | Task 4.1-4.3: Game controller & main loop | 2-3 hours | Tasks 1-2 |
| 5 | Task 3.1: Castling | 1-2 hours | Task 4 |
| 6 | Task 3.2: En passant | 1 hour | Task 4 |
| 7 | Task 3.3: Promotion (simplified) | 30 min | Task 4 |
| 8 | Task 5.1: Restoration paths | 1 hour | Task 4 |
| 9 | Task 6.1-6.3: Error handling | 1-2 hours | Task 4 |
| 10 | Task 7.1-7.2: Testing | 1-2 hours | All above |

**Total estimated time: 11-17 hours**

---

## Files to Create

1. `game_controller.h` - Main integration header
2. `game_controller.cpp` - Main integration implementation
3. `game_controller_test.cpp` - Integration tests

## Files to Modify

1. `movegen.h` - Add captured piece encoding
2. `movegen.cpp` - Store captured piece in moves
3. `board_interpreter.h` - Add en passant fields to MovePlan
4. `gantry_controller.h` - Simplify capture slot API
5. `gantry_controller.cpp` - Update capture logic, restoration paths
6. `main.cpp` - Add physical board mode

---

## Open Questions for Next Session

1. **Promotion handling:** Go with simplified (pawn stays) or full (piece swap)?
2. **Human input:** How will the human player input their moves? UCI-style text? Physical detection?
3. **Game modes:** Engine vs human? Engine vs engine? Specific opening positions?
4. **Serial port:** Hardcoded `/dev/ttyUSB0` or configurable?

---

## Quick Start for Next Session

```bash
# Start with Task 1.1 - Captured piece encoding
# Open movegen.h and add the new encoding macros
# Then update movegen.cpp to use them in generateMoves()
```
