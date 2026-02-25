/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    game_controller.h - Integration layer between engine and physical board
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
*    This module bridges the chess engine's internal representation with the
*    physical board interpreter and gantry controller. It handles:
*    - Converting engine move encoding to physical moves
*    - Converting engine piece types to physical piece types
*    - Orchestrating move execution on the physical board                    
*    - Special move handling (castling, en passant, promotion)
*     
*    IMPORTANT: This header is designed to be included AFTER board_interpreter.h
*    but BEFORE any engine headers to avoid macro conflicts.
*/

#ifndef KIRIN_GAME_CONTROLLER_H
#define KIRIN_GAME_CONTROLLER_H

#include "board_interpreter.h"
#include "gantry_controller.h"

/************ Engine Move Decoding Helpers ************/
// Inline wrappers around the engine's move encoding bit layout.
// Square, Piece, Color, MoveType enums come directly from types.h
// (included via bitboard.h in game_controller.cpp).
namespace EngineMove {
    inline int getSource(int move) { return move & 0x3f; }
    inline int getTarget(int move) { return (move & 0xfc0) >> 6; }
    inline int getPiece(int move) { return (move & 0xf000) >> 12; }
    inline int getPromoted(int move) { return (move & 0xf0000) >> 16; }
    inline bool getCapture(int move) { return (move & 0x100000) != 0; }
    inline bool getDpp(int move) { return (move & 0x200000) != 0; }
    inline bool getEnpassant(int move) { return (move & 0x400000) != 0; }
    inline bool getCastle(int move) { return (move & 0x800000) != 0; }
    inline int getCapturedPiece(int move) { return (move >> 24) & 0xf; }
}

// Move type enum values come from types.h (allMoves, onlyCaptures)

/************ Type Conversion Functions ************/

/**
 * Convert engine piece enum to physical PieceType
 * 
 * Engine pieces: P=0, N=1, B=2, R=3, Q=4, K=5, p=6, n=7, b=8, r=9, q=10, k=11
 * Physical pieces: PAWN=0, KNIGHT=1, BISHOP=2, ROOK=3, QUEEN=4, KING=5, UNKNOWN=6
 * 
 * @param enginePiece The engine's piece enum value (0-11)
 * @return The corresponding PieceType for path generation
 */
PieceType engineToPhysicalPiece(int enginePiece);

/**
 * Convert physical PieceType back to engine piece enum
 * 
 * @param pt The physical piece type
 * @param isWhite True if the piece is white
 * @return The engine's piece enum value (0-11)
 */
int physicalToEnginePiece(PieceType pt, bool isWhite);

/**
 * Check if an engine piece is white
 * 
 * @param enginePiece The engine's piece enum value (0-11)
 * @return True if the piece is white (0-5), false if black (6-11)
 */
inline bool isWhitePiece(int enginePiece) {
    return enginePiece < 6;
}

/************ Move Conversion Functions ************/

/**
 * Convert engine move encoding to PhysicalMove struct
 * 
 * Extracts source/target squares, piece type, and capture flag from
 * the engine's compact move encoding.
 * 
 * @param engineMove The engine's encoded move (see movegen.h for format)
 * @return PhysicalMove struct ready for path planning
 */
PhysicalMove engineToPhysicalMove(int engineMove);

/**
 * Convert a square index to BoardCoord
 * 
 * Engine squares: a8=0, b8=1, ..., h1=63
 * BoardCoord: row 0 = rank 8, col 0 = file a
 * 
 * @param square The engine's square index (0-63)
 * @return BoardCoord for the physical board
 */
inline BoardCoord squareToBoardCoord(int square) {
    return BoardCoord(square / 8, square % 8);
}

/**
 * Convert BoardCoord to engine square index
 * 
 * @param coord The board coordinate
 * @return Engine square index (0-63)
 */
inline int boardCoordToSquare(const BoardCoord& coord) {
    return coord.row * 8 + coord.col;
}

/************ Game Controller Class ************/

class GameController {
private:
    Gantry::GrblController gantry;
    PhysicalBoard physicalBoard;
    
    // State
    bool gameInProgress;
    bool waitingForHuman;  // If human plays one side
    bool enginePlaysWhite; // Which side the engine plays
    
    // Helper functions for special moves
    bool executeCastlingInternal(int engineMove);
    bool executeEnPassantInternal(int engineMove);
    bool executePromotionInternal(int engineMove);
    bool executeNormalMove(int engineMove);
    
public:
    GameController();
    ~GameController();
    
    /*** Setup ***/
    
    /**
     * Connect to the gantry controller hardware
     * @param port Serial port path (e.g., "/dev/ttyUSB0")
     * @return True if connection successful
     */
    bool connectHardware(const char* port);
    
    /**
     * Home the gantry (move to reference position)
     * @return True if homing successful
     */
    bool homeGantry();
    
    /**
     * Set up a new game - move all pieces from capture zones to starting positions
     * Also resets the physical board state tracking
     * @return True if setup successful
     */
    bool setupNewGame();
    
    /**
     * Synchronize physical board state with engine state
     * Call this after setting up a position via FEN
     */
    void syncWithEngine();
    
    /*** Game Flow ***/
    
    /**
     * Execute a move from the engine on the physical board
     * Handles normal moves, captures, and special moves (castling, en passant, promotion)
     * 
     * @param engineMove The engine's encoded move
     * @return True if move executed successfully
     */
    bool executeEngineMove(int engineMove);
    
    /**
     * Execute a human move given in algebraic notation
     * Parses the move string and converts to engine format
     * 
     * @param moveStr Move in format like "e2e4", "e7e8q" (promotion)
     * @return True if move executed successfully
     */
    bool executeHumanMove(const char* moveStr);
    
    /*** Special Moves ***/
    
    /**
     * Execute a castling move
     * Moves both king and rook in the correct order
     * 
     * @param engineMove The castling move from the engine
     * @return True if successful
     */
    bool executeCastling(int engineMove);
    
    /**
     * Execute an en passant capture
     * Handles the capture from the correct square (not destination)
     * 
     * @param engineMove The en passant move from the engine
     * @return True if successful
     */
    bool executeEnPassant(int engineMove);
    
    /**
     * Execute a pawn promotion
     * For now, just moves the pawn (simplified approach)
     * 
     * @param engineMove The promotion move from the engine
     * @return True if successful
     */
    bool executePromotion(int engineMove);
    
    /*** State ***/
    
    /**
     * Check if the game is over (checkmate, stalemate, etc.)
     * Queries the engine for game state
     * 
     * @return True if game has ended
     */
    bool isGameOver() const;
    
    /**
     * Get the physical board state (for debugging)
     */
    const PhysicalBoard& getPhysicalBoard() const { return physicalBoard; }
    
    /**
     * Get the gantry controller (for manual operations)
     */
    Gantry::GrblController& getGantry() { return gantry; }
    
    /**
     * Check if hardware is connected
     */
    bool isConnected() const { return gantry.isConnected(); }
    
    /**
     * Set which side the engine plays
     */
    void setEnginePlaysWhite(bool playsWhite) { enginePlaysWhite = playsWhite; }
    
    /**
     * Check if it's the engine's turn
     */
    bool isEngineTurn() const;
    
    /**
     * Start a new game with the engine playing the specified side
     */
    void startGame(bool enginePlaysWhite);
    
    /**
     * Stop the current game
     */
    void stopGame() { gameInProgress = false; }
    
    /**
     * Check if a game is in progress
     */
    bool isGameInProgress() const { return gameInProgress; }
};

/************ Utility Functions ************/

/**
 * Parse a move string (e.g., "e2e4") and find the matching move in the move list
 * 
 * @param moveStr The move string to parse
 * @return The encoded move if found, 0 otherwise
 */
int parseBoardMove(const char* moveStr);

/**
 * Print the current physical board state (for debugging)
 */
void printPhysicalBoard(const PhysicalBoard& board);

#endif // KIRIN_GAME_CONTROLLER_H
