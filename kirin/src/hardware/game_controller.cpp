/*
 * Kirin Chess Engine
 * game_controller.cpp - Integration layer implementation
 * 
 * This file bridges the engine and physical board systems.
 * It includes engine headers directly (after board_interpreter through game_controller.h)
 * and uses the namespaced wrappers for move decoding.
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

GameController::GameController()
    : gameInProgress(false), waitingForHuman(false), enginePlaysWhite(true) {
    // Initialize physical board to starting position occupancy
}

GameController::~GameController() {
    // Ensure we disconnect cleanly
    if (gantry.isConnected()) {
        gantry.setMagnet(false);  // Safety: ensure magnet is off
        gantry.disconnect();
    }
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
    
    // Sync physical board with engine's starting position
    syncWithEngine();
    
    printf("New game ready\n");
    return true;
}

void GameController::syncWithEngine() {
    // Set the physical board occupancy from the engine's state
    // occupancy[both] contains all pieces
    physicalBoard.setOccupancy(occupancy[both]);
}

/*** Game Flow ***/

bool GameController::executeEngineMove(int engineMove) {
    if (!gantry.isConnected()) {
        printf("Error: Gantry not connected\n");
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
    PieceType capturedType = UNKNOWN;
    bool capturedIsWhite = false;
    
    if (EngineMove::getCapture(engineMove)) {
        int capturedPiece = EngineMove::getCapturedPiece(engineMove);
        capturedType = engineToPhysicalPiece(capturedPiece);
        capturedIsWhite = isWhitePiece(capturedPiece);
    }
    
    // Execute on hardware
    if (!gantry.executeMove(plan, capturedIsWhite, capturedType)) {
        printf("Error: Hardware execution failed\n");
        return false;
    }
    
    // Update physical board state
    if (physMove.isCapture) {
        physicalBoard.clearSquare(physMove.to);
    }
    physicalBoard.movePiece(physMove.from, physMove.to);
    
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

/*** Special Moves ***/

bool GameController::executeCastling(int engineMove) {
    return executeCastlingInternal(engineMove);
}

bool GameController::executeCastlingInternal(int engineMove) {
    
    
    int source = EngineMove::getSource(engineMove);
    int target = EngineMove::getTarget(engineMove);
    int piece = EngineMove::getPiece(engineMove);
    
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
    
    if (!gantry.executeMove(rookPlan, false, UNKNOWN)) {
        printf("Error: Rook move failed during castling\n");
        return false;
    }
    
    // Update physical board for rook
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
        return false;
    }
    
    if (!gantry.executeMove(kingPlan, false, UNKNOWN)) {
        printf("Error: King move failed during castling\n");
        return false;
    }
    
    // Update physical board for king
    physicalBoard.movePiece(kingMove.from, kingMove.to);
    
    return true;
}

bool GameController::executeEnPassant(int engineMove) {
    return executeEnPassantInternal(engineMove);
}

bool GameController::executeEnPassantInternal(int engineMove) {
    int source = EngineMove::getSource(engineMove);
    int target = EngineMove::getTarget(engineMove);
    int piece = EngineMove::getPiece(engineMove);
    
    // The captured pawn is on the same file as destination,
    // but same rank as the capturing pawn's origin
    BoardCoord fromCoord = squareToBoardCoord(source);
    BoardCoord toCoord = squareToBoardCoord(target);
    
    // Captured pawn location: same column as destination, same row as source
    BoardCoord capturedPawnSquare(fromCoord.row, toCoord.col);
    
    // Determine the color of the captured pawn
    bool isWhiteCapturing = isWhitePiece(piece);
    bool capturedIsWhite = !isWhiteCapturing;  // Opposite color
    
    // Step 1: Remove the captured pawn first
    int captureSlotIndex = gantry.getNextCaptureSlot(capturedIsWhite, true);  // true = isPawn
    
    Gantry::Position capturedPos = Gantry::toPhysical(capturedPawnSquare);
    Gantry::Position captureZonePos = Gantry::getCapturePosition(capturedIsWhite, true, captureSlotIndex);
    
    std::vector<std::string> captureGcode;
    captureGcode.push_back(Gantry::moveCommand(capturedPos));
    captureGcode.push_back(Gantry::magnetOn());
    captureGcode.push_back(Gantry::dwell(100));
    captureGcode.push_back(Gantry::moveCommand(captureZonePos));
    captureGcode.push_back(Gantry::magnetOff());
    captureGcode.push_back(Gantry::dwell(100));
    
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
        return false;
    }
    
    if (!gantry.executeMove(pawnPlan, false, UNKNOWN)) {
        printf("Error: Pawn move failed during en passant\n");
        return false;
    }
    
    // Update physical board - move pawn
    physicalBoard.movePiece(pawnMove.from, pawnMove.to);
    
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
        // Handle as a capture, but we need to use the pawn's path generation
        int source = EngineMove::getSource(engineMove);
        int target = EngineMove::getTarget(engineMove);
        int capturedPiece = EngineMove::getCapturedPiece(engineMove);
        
        BoardCoord fromCoord = squareToBoardCoord(source);
        BoardCoord toCoord = squareToBoardCoord(target);
        
        // First capture the piece on the promotion square
        PieceType capturedType = engineToPhysicalPiece(capturedPiece);
        bool capturedIsWhite = isWhitePiece(capturedPiece);
        
        int slotIndex = gantry.getNextCaptureSlot(capturedIsWhite, capturedType == PAWN);
        
        std::vector<std::string> captureGcode = Gantry::generateCaptureGcode(
            toCoord, capturedIsWhite, capturedType == PAWN, slotIndex
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
            return false;
        }
        
        if (!gantry.executeMove(plan, false, UNKNOWN)) {
            printf("Error: Pawn move failed during promotion\n");
            return false;
        }
        
        physicalBoard.movePiece(pawnMove.from, pawnMove.to);
    } else {
        // Non-capture promotion - just move the pawn forward
        return executeNormalMove(engineMove);
    }
    
    return true;
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
