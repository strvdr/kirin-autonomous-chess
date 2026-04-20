/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    game_controller.cpp - Integration layer implementation
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
*    This file bridges the engine and physical board systems.
*    It includes engine headers directly (after board_interpreter through game_controller.h)
*    and uses the namespaced wrappers for move decoding.
*/ 

#include "game_controller.h"
#include <cstdio>
#include <cstring>

// Include engine headers for access to global state and move generation
#include "bitboard.h"
#include "movegen.h"

/************ Type Conversion Functions ************/

PieceType engineToPhysicalPiece(int enginePiece) {
    // Engine: P=0, N=1, B=2, R=3, Q=4, K=5, p=6, n=7, b=8, r=9, q=10, k=11
    // Map to physical: PAWN=0, KNIGHT=1, BISHOP=2, ROOK=3, QUEEN=4, KING=5
    
    // Normalize to 0-5 range (white piece types)
    int normalizedPiece = enginePiece % 6;
    
    switch (normalizedPiece) {
        case 0: return PAWN;    // P or p
        case 1: return KNIGHT;  // N or n
        case 2: return BISHOP;  // B or b
        case 3: return ROOK;    // R or r
        case 4: return QUEEN;   // Q or q
        case 5: return KING;    // K or k
        default: return UNKNOWN;
    }
}

int physicalToEnginePiece(PieceType pt, bool isWhite) {
    int basePiece;
    
    switch (pt) {
        case PAWN:   basePiece = 0; break;  // P
        case KNIGHT: basePiece = 1; break;  // N
        case BISHOP: basePiece = 2; break;  // B
        case ROOK:   basePiece = 3; break;  // R
        case QUEEN:  basePiece = 4; break;  // Q
        case KING:   basePiece = 5; break;  // K
        default:     return -1;             // Invalid
    }
    
    // Add 6 for black pieces
    return isWhite ? basePiece : basePiece + 6;
}

/************ Move Conversion Functions ************/

PhysicalMove engineToPhysicalMove(int engineMove) {
    // Extract components from engine move encoding using namespaced functions
    int source = EngineMove::getSource(engineMove);
    int target = EngineMove::getTarget(engineMove);
    int piece = EngineMove::getPiece(engineMove);
    bool isCapture = EngineMove::getCapture(engineMove);
    
    // Convert to physical board coordinates
    BoardCoord from = squareToBoardCoord(source);
    BoardCoord to = squareToBoardCoord(target);
    
    // Convert piece type
    PieceType pieceType = engineToPhysicalPiece(piece);
    
    return PhysicalMove(from, to, pieceType, isCapture);
}

/************ GameController Implementation ************/

int GameController::getCapturedSlotForMove(int engineMove) const {
    if (!EngineMove::getCapture(engineMove) || !hasExactTracker()) {
        return SLOT_NONE;
    }

    if (EngineMove::getEnpassant(engineMove)) {
        int source = EngineMove::getSource(engineMove);
        int target = EngineMove::getTarget(engineMove);
        int capturedSquare = (source / 8) * 8 + (target % 8);
        return pieceTracker.getSlotAt(capturedSquare);
    }

    return pieceTracker.getSlotAt(EngineMove::getTarget(engineMove));
}

GameController::GameController()
    : gameInProgress(false),
      waitingForHuman(false),
      enginePlaysWhite(true),
      trackerValidity(TrackerValidity::Unknown) {
    // Initialize physical board to starting position occupancy
}

GameController::~GameController() {
    // In dry-run mode there is no real connection — nothing to clean up.
    if (gantry.isDryRun()) return;
    
    // Ensure we disconnect cleanly from real hardware
    if (gantry.isConnected()) {
        gantry.setMagnet(false);  // Safety: ensure magnet is off
        gantry.disconnect();
    }
}

/*** Illegal State Callback ***/

void GameController::onIllegalState() {
    printf("\n");
    printf("  *** BOARD STATE NOT RECOGNIZED ***\n");
    printf("  The current board position does not match any legal move.\n");
    printf("  Please check that all pieces are placed correctly.\n");
    printf("  (The system will continue waiting for a valid move.)\n");
    printf("\n");
    // TODO: In the future, this could trigger a buzzer, LED, or
    // other physical feedback on the board to alert the player.
}

void GameController::onWrongSlot() {
    printf("\n");
    printf("  *** WRONG STORAGE SLOT ***\n");
    printf("  The captured piece was placed in the wrong storage slot.\n");
    printf("  Each piece has a designated slot matching its starting position.\n");
    printf("  Please move the piece to the correct labeled slot.\n");
    printf("  (The system will continue waiting.)\n");
    printf("\n");
}

/*** Setup ***/

bool GameController::connectHardware(const char* port) {
    if (!gantry.connect(port)) {
        printf("Error: Failed to connect to gantry on %s\n", port);
        return false;
    }
    printf("Connected to gantry on %s\n", port);
    return true;
}

bool GameController::initScanner() {
    if (!scanner.init()) {
        printf("Error: Failed to initialize board scanner\n");
        return false;
    }
    printf("Board scanner initialized\n");
    return true;
}

bool GameController::homeGantry() {
    if (!gantry.isConnected()) {
        printf("Error: Gantry not connected\n");
        return false;
    }
    
    printf("Homing gantry...\n");
    if (!gantry.home()) {
        printf("Error: Homing failed\n");
        return false;
    }
    printf("Homing complete\n");
    return true;
}

bool GameController::setupNewGame() {
    if (!gantry.isConnected()) {
        printf("Error: Gantry not connected\n");
        return false;
    }
    
    printf("Setting up new game...\n");
    
    // Move all pieces from capture zones to starting positions
    if (!gantry.setupNewGame()) {
        printf("Error: Failed to set up pieces\n");
        return false;
    }

    // A fresh setup restores standard piece identities.
    initTrackerForStartingPosition();
    
    // Sync physical board with engine's starting position
    syncWithEngine();
    
    // Sync the scanner's baseline state as well
    scanner.setLastScan(occupancy[both]);
    
    printf("New game ready\n");
    return true;
}

void GameController::syncWithEngine() {
    // Set the physical board occupancy from the engine's state
    // occupancy[both] contains all pieces
    physicalBoard.setOccupancy(occupancy[both]);
    
    // Sync the scanner's board baseline
    scanner.setLastScan(occupancy[both]);

    // If scanner is initialized, also snapshot the storage zones
    // so we have a fresh baseline for capture detection
    if (scanner.isInitialized()) {
        uint16_t blackSt = scanner.scanStorageDebounced(false);
        uint16_t whiteSt = scanner.scanStorageDebounced(true);
        scanner.setLastStorage(blackSt, whiteSt);
    }
}

void GameController::initTrackerForStartingPosition() {
    pieceTracker.initStartingPosition();
    trackerValidity = TrackerValidity::Exact;
}

void GameController::invalidateTracker() {
    trackerValidity = TrackerValidity::Unknown;
}

/*** Game Flow ***/

bool GameController::executeEngineMove(int engineMove) {
    if (!gantry.isConnected()) {
        printf("Error: Gantry not connected\n");
        return false;
    }

    if (EngineMove::getCapture(engineMove) && !hasExactTracker()) {
        printf("Error: Exact piece identity is required for engine captures.\n");
        printf("  Start from a new game before executing capture moves.\n");
        return false;
    }
    
    // Check for special moves first using namespaced functions
    if (EngineMove::getCastle(engineMove)) {
        return executeCastlingInternal(engineMove);
    }
    if (EngineMove::getEnpassant(engineMove)) {
        return executeEnPassantInternal(engineMove);
    }
    if (EngineMove::getPromoted(engineMove)) {
        return executePromotionInternal(engineMove);
    }
    
    // Normal move (including captures)
    return executeNormalMove(engineMove);
}

bool GameController::executeNormalMove(int engineMove) {
    // Convert to physical move
    PhysicalMove physMove = engineToPhysicalMove(engineMove);
    
    // Plan the move
    MovePlan plan = planMove(physicalBoard, physMove);
    if (!plan.isValid) {
        printf("Error planning move: %s\n", plan.errorMessage ? plan.errorMessage : "unknown error");
        return false;
    }
    
    // Determine captured piece info
    bool capturedIsWhite = false;
    int capturedSlot = SLOT_NONE;
    
    if (EngineMove::getCapture(engineMove)) {
        int capturedPiece = EngineMove::getCapturedPiece(engineMove);
        capturedIsWhite = isWhitePiece(capturedPiece);
        capturedSlot = getCapturedSlotForMove(engineMove);
        if (capturedSlot == SLOT_NONE) {
            printf("Error: Could not resolve exact capture slot for engine move\n");
            return false;
        }
    }
    
    // Execute on hardware
    if (!gantry.executeMove(plan, capturedIsWhite, capturedSlot)) {
        printf("Error: Hardware execution failed\n");
        return false;
    }
    
    // Update physical board state
    if (physMove.isCapture) {
        physicalBoard.clearSquare(physMove.to);
    }
    physicalBoard.movePiece(physMove.from, physMove.to);
    
    // Update scanner baseline to match new board state
    scanner.setLastScan(physicalBoard.getOccupancy());
    
    return true;
}

bool GameController::executeHumanMove(const char* moveStr) {
    // Parse the move string to find the matching engine move
    int move = parseBoardMove(moveStr);
    if (move == 0) {
        printf("Error: Invalid move '%s'\n", moveStr);
        return false;
    }
    
    // Execute it like an engine move
    return executeEngineMove(move);
}

bool GameController::waitForHumanMove(int& engineMove, int timeoutMs) {
    if (!scanner.isInitialized()) {
        printf("Error: Board scanner not initialized\n");
        return false;
    }

    if (!hasExactTracker()) {
        printf("Error: Piece identity tracking is unavailable for this position.\n");
        printf("  Sensor-based move detection requires exact tracker state.\n");
        printf("  Start from a new game or use manual move entry.\n");
        return false;
    }
    
    // The scanner needs the current board occupancy and storage baselines.
    Bitboard currentOccupancy = occupancy[both];
    
    // Read the current storage zone state as our baseline.
    // Any new piece appearing in storage during the human's move = a capture.
    uint16_t blackStorage = scanner.scanStorageDebounced(false);
    uint16_t whiteStorage = scanner.scanStorageDebounced(true);
    
    printf("Waiting for your move...\n");
    
    ScannerMoveResult result = scanner.waitForLegalMove(
        currentOccupancy,
        blackStorage,
        whiteStorage,
        pieceTracker,
        timeoutMs,
        &GameController::onIllegalState,
        &GameController::onWrongSlot
    );
    
    if (result.success) {
        engineMove = result.engineMove;
        
        printf("Detected move: %s\n", result.uciString);
        
        // Update the physical board state to reflect the human's move.
        // The engine state will be updated by the caller via makeMove().
        PhysicalMove physMove = engineToPhysicalMove(engineMove);
        
        if (EngineMove::getCapture(engineMove)) {
            if (EngineMove::getEnpassant(engineMove)) {
                // En passant: clear the captured pawn's actual square
                BoardCoord fromCoord = squareToBoardCoord(EngineMove::getSource(engineMove));
                BoardCoord toCoord = squareToBoardCoord(EngineMove::getTarget(engineMove));
                BoardCoord capturedSquare(fromCoord.row, toCoord.col);
                physicalBoard.clearSquare(capturedSquare);
            } else {
                physicalBoard.clearSquare(physMove.to);
            }
        }
        
        if (EngineMove::getCastle(engineMove)) {
            // Castling: also move the rook in our physical board tracking
            int target = EngineMove::getTarget(engineMove);
            if (target == g1) {
                physicalBoard.movePiece(squareToBoardCoord(h1), squareToBoardCoord(f1));
            } else if (target == c1) {
                physicalBoard.movePiece(squareToBoardCoord(a1), squareToBoardCoord(d1));
            } else if (target == g8) {
                physicalBoard.movePiece(squareToBoardCoord(h8), squareToBoardCoord(f8));
            } else if (target == c8) {
                physicalBoard.movePiece(squareToBoardCoord(a8), squareToBoardCoord(d8));
            }
        }
        
        physicalBoard.movePiece(physMove.from, physMove.to);
        
        // Update scanner baseline
        scanner.setLastScan(physicalBoard.getOccupancy());
        
        return true;
    }
    
    if (result.timeout) {
        printf("Move detection timed out.\n");
    }
    
    return false;
}

/*** Special Moves ***/

bool GameController::executeCastling(int engineMove) {
    return executeCastlingInternal(engineMove);
}

bool GameController::executeCastlingInternal(int engineMove) {
    
    int source = EngineMove::getSource(engineMove);
    int target = EngineMove::getTarget(engineMove);
    int piece = EngineMove::getPiece(engineMove);
    
    // Save physical board state before multi-step move.
    // If any step fails, we restore to this snapshot so the
    // physicalBoard tracking doesn't end up half-updated.
    Bitboard savedOccupancy = physicalBoard.getOccupancy();
    
    // Determine if it's white or black castling
    bool isWhite = isWhitePiece(piece);
    
    // Determine if it's kingside or queenside
    bool isKingside = (target == g1 || target == g8);
    
    // Determine rook squares
    int rookFrom, rookTo;
    if (isWhite) {
        if (isKingside) {
            rookFrom = h1;
            rookTo = f1;
        } else {
            rookFrom = a1;
            rookTo = d1;
        }
    } else {
        if (isKingside) {
            rookFrom = h8;
            rookTo = f8;
        } else {
            rookFrom = a8;
            rookTo = d8;
        }
    }
    
    // Step 1: Move the rook first (to avoid blocking king's path with some gantry designs)
    PhysicalMove rookMove(
        squareToBoardCoord(rookFrom),
        squareToBoardCoord(rookTo),
        ROOK,
        false
    );
    
    MovePlan rookPlan = planMove(physicalBoard, rookMove);
    if (!rookPlan.isValid) {
        printf("Error planning rook move for castling: %s\n", 
               rookPlan.errorMessage ? rookPlan.errorMessage : "unknown error");
        return false;
    }
    
    if (!gantry.executeMove(rookPlan, false, SLOT_NONE)) {
        printf("Error: Rook move failed during castling\n");
        return false;
    }
    
    // Update physical board for rook (needed for king path planning)
    physicalBoard.movePiece(rookMove.from, rookMove.to);
    
    // Step 2: Move the king
    PhysicalMove kingMove(
        squareToBoardCoord(source),
        squareToBoardCoord(target),
        KING,
        false
    );
    
    MovePlan kingPlan = planMove(physicalBoard, kingMove);
    if (!kingPlan.isValid) {
        printf("Error planning king move for castling: %s\n",
               kingPlan.errorMessage ? kingPlan.errorMessage : "unknown error");
        // Rook already moved physically but king planning failed.
        // Restore physicalBoard to pre-castling state.
        physicalBoard.setOccupancy(savedOccupancy);
        return false;
    }
    
    if (!gantry.executeMove(kingPlan, false, SLOT_NONE)) {
        printf("Error: King move failed during castling\n");
        // Rook moved physically but king failed on hardware.
        // Restore physicalBoard to pre-castling state.
        physicalBoard.setOccupancy(savedOccupancy);
        return false;
    }
    
    // Update physical board for king
    physicalBoard.movePiece(kingMove.from, kingMove.to);
    
    // Update scanner baseline
    scanner.setLastScan(physicalBoard.getOccupancy());
    
    return true;
}

bool GameController::executeEnPassant(int engineMove) {
    return executeEnPassantInternal(engineMove);
}

bool GameController::executeEnPassantInternal(int engineMove) {
    int source = EngineMove::getSource(engineMove);
    int target = EngineMove::getTarget(engineMove);
    int piece = EngineMove::getPiece(engineMove);
    
    // Save physical board state before multi-step move.
    Bitboard savedOccupancy = physicalBoard.getOccupancy();
    
    // The captured pawn is on the same file as destination,
    // but same rank as the capturing pawn's origin
    BoardCoord fromCoord = squareToBoardCoord(source);
    BoardCoord toCoord = squareToBoardCoord(target);
    
    // Captured pawn location: same column as destination, same row as source
    BoardCoord capturedPawnSquare(fromCoord.row, toCoord.col);
    
    // Determine the color of the captured pawn
    bool isWhiteCapturing = isWhitePiece(piece);
    bool capturedIsWhite = !isWhiteCapturing;  // Opposite color
    int capturedSlot = getCapturedSlotForMove(engineMove);
    if (capturedSlot == SLOT_NONE) {
        printf("Error: Could not resolve exact capture slot for en passant\n");
        return false;
    }
    
    // Step 1: Remove the captured pawn first
    std::vector<std::string> captureGcode = Gantry::generateCaptureGcode(
        capturedPawnSquare,
        capturedIsWhite,
        static_cast<Gantry::StartingSlot>(capturedSlot)
    );

    if (!gantry.sendCommands(captureGcode)) {
        printf("Error: Failed to capture en passant pawn\n");
        return false;
    }
    
    // Update physical board - remove captured pawn
    physicalBoard.clearSquare(capturedPawnSquare);
    
    // Step 2: Move the capturing pawn
    PhysicalMove pawnMove(fromCoord, toCoord, PAWN, false);  // Not a capture on target square
    
    MovePlan pawnPlan = planMove(physicalBoard, pawnMove);
    if (!pawnPlan.isValid) {
        printf("Error planning en passant pawn move: %s\n",
               pawnPlan.errorMessage ? pawnPlan.errorMessage : "unknown error");
        // Captured pawn already removed physically, but pawn move planning failed.
        // Restore physicalBoard to pre-move state.
        physicalBoard.setOccupancy(savedOccupancy);
        return false;
    }
    
    if (!gantry.executeMove(pawnPlan, false, SLOT_NONE)) {
        printf("Error: Pawn move failed during en passant\n");
        // Captured pawn removed but capturing pawn didn't move.
        // Restore physicalBoard to pre-move state.
        physicalBoard.setOccupancy(savedOccupancy);
        return false;
    }
    
    // Update physical board - move pawn
    physicalBoard.movePiece(pawnMove.from, pawnMove.to);
    
    // Update scanner baseline
    scanner.setLastScan(physicalBoard.getOccupancy());
    
    return true;
}

bool GameController::executePromotion(int engineMove) {
    return executePromotionInternal(engineMove);
}

bool GameController::executePromotionInternal(int engineMove) {
    // For now, simplified approach: just move the pawn to the last rank
    // The pawn physically stays there but is tracked as a promoted piece by the engine
    
    // Check if it's also a capture
    if (EngineMove::getCapture(engineMove)) {
        // Save physical board state before multi-step move.
        Bitboard savedOccupancy = physicalBoard.getOccupancy();
        
        // Handle as a capture, but we need to use the pawn's path generation
        int source = EngineMove::getSource(engineMove);
        int target = EngineMove::getTarget(engineMove);
        int capturedPiece = EngineMove::getCapturedPiece(engineMove);
        BoardCoord fromCoord = squareToBoardCoord(source);
        BoardCoord toCoord = squareToBoardCoord(target);
        
        // First capture the piece on the promotion square
        bool capturedIsWhite = isWhitePiece(capturedPiece);
        int capturedSlot = getCapturedSlotForMove(engineMove);
        if (capturedSlot == SLOT_NONE) {
            printf("Error: Could not resolve exact capture slot for promotion\n");
            return false;
        }

        std::vector<std::string> captureGcode = Gantry::generateCaptureGcode(
            toCoord,
            capturedIsWhite,
            static_cast<Gantry::StartingSlot>(capturedSlot)
        );
        
        if (!gantry.sendCommands(captureGcode)) {
            printf("Error: Failed to capture piece during promotion\n");
            return false;
        }
        
        physicalBoard.clearSquare(toCoord);
        
        // Now move the pawn
        PhysicalMove pawnMove(fromCoord, toCoord, PAWN, false);
        MovePlan plan = planMove(physicalBoard, pawnMove);
        
        if (!plan.isValid) {
            printf("Error planning promotion move: %s\n",
                   plan.errorMessage ? plan.errorMessage : "unknown error");
            // Captured piece removed but pawn move planning failed.
            // Restore physicalBoard to pre-move state.
            physicalBoard.setOccupancy(savedOccupancy);
            return false;
        }
        
        if (!gantry.executeMove(plan, false, SLOT_NONE)) {
            printf("Error: Pawn move failed during promotion\n");
            // Captured piece removed but pawn didn't move.
            // Restore physicalBoard to pre-move state.
            physicalBoard.setOccupancy(savedOccupancy);
            return false;
        }
        
        physicalBoard.movePiece(pawnMove.from, pawnMove.to);
    } else {
        // Non-capture promotion - just move the pawn forward
        return executeNormalMove(engineMove);
    }
    
    // Update scanner baseline
    scanner.setLastScan(physicalBoard.getOccupancy());
    
    return true;
}

/*** Board Verification ***/

bool GameController::verifyBoardState(Bitboard& expectedOcc, Bitboard& actualOcc) {
    if (!scanner.isInitialized()) {
        // No scanner — can't verify, assume OK
        return true;
    }

    expectedOcc = occupancy[both];
    actualOcc = scanner.scanBoardDebounced();

    return (expectedOcc == actualOcc);
}

bool GameController::verifyBoardState() {
    Bitboard expected = 0, actual = 0;

    if (verifyBoardState(expected, actual)) {
        return true;
    }

    printf("\n");
    printf("  *** BOARD MISMATCH DETECTED ***\n");
    printf("  The physical board does not match the expected position.\n");
    printf("  A piece may have been dropped or displaced.\n");
    printf("\n");

    printBoardMismatch(expected, actual);
    return false;
}

void GameController::printBoardMismatch(Bitboard expected, Bitboard actual) {
    Bitboard diff = expected ^ actual;

    // Squares that should have a piece but don't
    Bitboard missing = expected & diff;
    // Squares that have a piece but shouldn't
    Bitboard extra = actual & diff;

    int missingCount = countBits(missing);
    int extraCount = countBits(extra);

    printf("  Expected vs Actual:\n");
    printf("    a b c d e f g h        a b c d e f g h\n");

    for (int row = 0; row < 8; row++) {
        // Expected
        printf("  %d ", 8 - row);
        for (int col = 0; col < 8; col++) {
            int sq = row * 8 + col;
            if (expected & (1ULL << sq)) {
                printf("■ ");
            } else {
                printf("· ");
            }
        }

        printf("    %d ", 8 - row);

        // Actual
        for (int col = 0; col < 8; col++) {
            int sq = row * 8 + col;
            bool isDiff = (diff & (1ULL << sq)) != 0;
            if (actual & (1ULL << sq)) {
                printf("%s ", isDiff ? "+" : "■");
            } else {
                printf("%s ", isDiff ? "X" : "·");
            }
        }
        printf("%d\n", 8 - row);
    }

    printf("    a b c d e f g h        a b c d e f g h\n");
    printf("    (expected)              (actual: X=missing, +=extra)\n");
    printf("\n");

    if (missingCount > 0) {
        printf("  Missing pieces (%d): ", missingCount);
        Bitboard m = missing;
        while (m) {
            int sq = getLSBindex(m);
            printf("%c%d ", 'a' + (sq % 8), 8 - (sq / 8));
            m &= m - 1;
        }
        printf("\n");
    }

    if (extraCount > 0) {
        printf("  Unexpected pieces (%d): ", extraCount);
        Bitboard e = extra;
        while (e) {
            int sq = getLSBindex(e);
            printf("%c%d ", 'a' + (sq % 8), 8 - (sq / 8));
            e &= e - 1;
        }
        printf("\n");
    }
    printf("\n");
}

/*** Piece Tracker Updates ***/

void GameController::updateTracker(int engineMove) {
    int source = EngineMove::getSource(engineMove);
    int target = EngineMove::getTarget(engineMove);
    int piece = EngineMove::getPiece(engineMove);
    bool isWhite = isWhitePiece(piece);
    bool isCapture = EngineMove::getCapture(engineMove);

    if (isCapture) {
        if (EngineMove::getEnpassant(engineMove)) {
            // En passant: captured pawn is on a different square
            int capturedSquare = (source / 8) * 8 + (target % 8);
            pieceTracker.applyMove(source, target, isWhite, true,
                                   capturedSquare, !isWhite);
        } else {
            // Normal capture: captured piece is on the target square
            pieceTracker.applyMove(source, target, isWhite, true,
                                   target, !isWhite);
        }
    } else {
        pieceTracker.applyMove(source, target, isWhite, false);
    }

    // Handle castling: also move the rook
    if (EngineMove::getCastle(engineMove)) {
        if (target == g1) {        // White kingside
            pieceTracker.applyRookCastle(h1, f1, true);
        } else if (target == c1) { // White queenside
            pieceTracker.applyRookCastle(a1, d1, true);
        } else if (target == g8) { // Black kingside
            pieceTracker.applyRookCastle(h8, f8, false);
        } else if (target == c8) { // Black queenside
            pieceTracker.applyRookCastle(a8, d8, false);
        }
    }
}

/*** State ***/

bool GameController::isGameOver() const {
    // Generate all legal moves
    moves moveList;
    moveList.count = 0;
    generateMoves(&moveList);
    
    // Check if any move is legal
    // We need to save and restore board state
    // Store current state
    unsigned long long bitboardsCopy[12], occupancyCopy[3];
    int sideCopy = side, enpassantCopy = enpassant, castleCopy = castle;
    unsigned long long hashKeyCopy = hashKey;
    
    memcpy(bitboardsCopy, bitboards, sizeof(bitboardsCopy));
    memcpy(occupancyCopy, occupancy, sizeof(occupancyCopy));
    
    for (int i = 0; i < moveList.count; i++) {
        if (makeMove(moveList.moves[i], allMoves)) {
            // Found a legal move - restore and return false (game not over)
            memcpy(bitboards, bitboardsCopy, sizeof(bitboardsCopy));
            memcpy(occupancy, occupancyCopy, sizeof(occupancyCopy));
            side = sideCopy;
            enpassant = enpassantCopy;
            castle = castleCopy;
            hashKey = hashKeyCopy;
            return false;
        }
        
        // Restore for next iteration
        memcpy(bitboards, bitboardsCopy, sizeof(bitboardsCopy));
        memcpy(occupancy, occupancyCopy, sizeof(occupancyCopy));
        side = sideCopy;
        enpassant = enpassantCopy;
        castle = castleCopy;
        hashKey = hashKeyCopy;
    }
    
    // No legal moves - either checkmate or stalemate
    return true;
}

bool GameController::isEngineTurn() const {
    // Engine plays white: it's engine's turn when side == white
    // Engine plays black: it's engine's turn when side == black
    return (enginePlaysWhite && side == white) || 
           (!enginePlaysWhite && side == black);
}

void GameController::startGame(bool playsWhite) {
    enginePlaysWhite = playsWhite;
    gameInProgress = true;
    waitingForHuman = !isEngineTurn();
    
    // Sync physical board with engine
    syncWithEngine();
}

/************ Utility Functions ************/

int parseBoardMove(const char* moveStr) {
    // Generate all legal moves
    moves moveList;
    moveList.count = 0;
    generateMoves(&moveList);
    
    // Parse the move string
    if (strlen(moveStr) < 4) {
        return 0;
    }
    
    // Extract source and target squares
    int sourceFile = moveStr[0] - 'a';
    int sourceRank = 8 - (moveStr[1] - '0');
    int targetFile = moveStr[2] - 'a';
    int targetRank = 8 - (moveStr[3] - '0');
    
    if (sourceFile < 0 || sourceFile > 7 || sourceRank < 0 || sourceRank > 7 ||
        targetFile < 0 || targetFile > 7 || targetRank < 0 || targetRank > 7) {
        return 0;
    }
    
    int source = sourceRank * 8 + sourceFile;
    int target = targetRank * 8 + targetFile;
    
    // Check for promotion piece
    int promotedPiece = 0;
    if (strlen(moveStr) >= 5) {
        bool isWhitePromotion = (side == white);
        switch (moveStr[4]) {
            case 'q': case 'Q': 
                promotedPiece = isWhitePromotion ? Q : q; 
                break;
            case 'r': case 'R': 
                promotedPiece = isWhitePromotion ? R : r; 
                break;
            case 'b': case 'B': 
                promotedPiece = isWhitePromotion ? B : b; 
                break;
            case 'n': case 'N': 
                promotedPiece = isWhitePromotion ? N : n; 
                break;
        }
    }
    
    // Store current state for legality check
    unsigned long long bitboardsCopy[12], occupancyCopy[3];
    int sideCopy = side, enpassantCopy = enpassant, castleCopy = castle;
    unsigned long long hashKeyCopy = hashKey;
    
    memcpy(bitboardsCopy, bitboards, sizeof(bitboardsCopy));
    memcpy(occupancyCopy, occupancy, sizeof(occupancyCopy));
    
    // Find matching move in move list
    for (int i = 0; i < moveList.count; i++) {
        int move = moveList.moves[i];
        
        if (EngineMove::getSource(move) == source && EngineMove::getTarget(move) == target) {
            // Check promotion match
            int movePromoted = EngineMove::getPromoted(move);
            
            if (promotedPiece) {
                // Must match promotion piece
                if (movePromoted == promotedPiece) {
                    // Verify move is legal
                    if (makeMove(move, allMoves)) {
                        // Restore state
                        memcpy(bitboards, bitboardsCopy, sizeof(bitboardsCopy));
                        memcpy(occupancy, occupancyCopy, sizeof(occupancyCopy));
                        side = sideCopy;
                        enpassant = enpassantCopy;
                        castle = castleCopy;
                        hashKey = hashKeyCopy;
                        return move;
                    }
                    // Restore state
                    memcpy(bitboards, bitboardsCopy, sizeof(bitboardsCopy));
                    memcpy(occupancy, occupancyCopy, sizeof(occupancyCopy));
                    side = sideCopy;
                    enpassant = enpassantCopy;
                    castle = castleCopy;
                    hashKey = hashKeyCopy;
                }
            } else if (movePromoted == 0) {
                // Non-promotion move
                if (makeMove(move, allMoves)) {
                    // Restore state
                    memcpy(bitboards, bitboardsCopy, sizeof(bitboardsCopy));
                    memcpy(occupancy, occupancyCopy, sizeof(occupancyCopy));
                    side = sideCopy;
                    enpassant = enpassantCopy;
                    castle = castleCopy;
                    hashKey = hashKeyCopy;
                    return move;
                }
                // Restore state
                memcpy(bitboards, bitboardsCopy, sizeof(bitboardsCopy));
                memcpy(occupancy, occupancyCopy, sizeof(occupancyCopy));
                side = sideCopy;
                enpassant = enpassantCopy;
                castle = castleCopy;
                hashKey = hashKeyCopy;
            }
        }
    }
    
    return 0;  // Move not found
}

void printPhysicalBoard(const PhysicalBoard& board) {
    printf("\n  Physical Board Occupancy:\n");
    printf("    a b c d e f g h\n");
    
    for (int row = 0; row < 8; row++) {
        printf("  %d ", 8 - row);
        for (int col = 0; col < 8; col++) {
            if (board.isOccupied(row, col)) {
                printf("X ");
            } else {
                printf(". ");
            }
        }
        printf("%d\n", 8 - row);
    }
    
    printf("    a b c d e f g h\n\n");
}
