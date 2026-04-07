/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    gantry_controller.h - Physical coordinate translation and G-code generation
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
*              22" rail travel
*    |<------------------------------->|
*    |                                 |
*    |  +-+ +---------------------+ +-+|
*    |  |B| |                     | |W||
*    |  |L| |     16" board       | |H||
*    |  |A| |                     | |I||
*    |  |C| |      8x8 grid       | |T||
*    |  |K| |                     | |E||
*    |  +-+ +---------------------+ +-+|
*    |  3.0"         16"          3.0" |
*    |<--->|<------------------->|<--->|
*       ^                            ^
*       |                            |
*     Black captures            White captures
*     X = 1.5" (center)         X = 20.5" (center)
*     Y = 4" to 18"             Y = 4" to 18"
*/

#ifndef KIRIN_GANTRY_CONTROLLER_H
#define KIRIN_GANTRY_CONTROLLER_H

#include "board_interpreter.h"
#include <string>
#include <vector>
#include <cmath>

namespace Gantry {

/************ Physical Constants (inches) ************/
constexpr double RAIL_TRAVEL = 18.0;
constexpr double BOARD_SIZE = 12.0;
constexpr double SQUARE_SIZE = 1.5;
constexpr double BOARD_MARGIN = 3.0;

// Square a1 center position
constexpr double A1_CENTER_X = BOARD_MARGIN + (SQUARE_SIZE / 2.0);  // 4.0"
constexpr double A1_CENTER_Y = BOARD_MARGIN + (SQUARE_SIZE / 2.0);  // 4.0"

/************ Capture Zone Layout ************/
// 2" vertical spacing (matches board), 1.5" horizontal spacing (two columns)
constexpr double CAPTURE_VERTICAL_SPACING = 2.0;
constexpr double CAPTURE_HORIZONTAL_SPACING = 1.5;

// White capture zone (right side, X = 19" to 22")
constexpr double WHITE_CAPTURE_COL1_X = BOARD_MARGIN + BOARD_SIZE + 0.75;  // 19.75" (back rank)
constexpr double WHITE_CAPTURE_COL2_X = BOARD_MARGIN + BOARD_SIZE + 2.25;  // 21.25" (pawns)

// Black capture zone (left side, X = 0" to 3")
constexpr double BLACK_CAPTURE_COL1_X = 2.25;  // 2.25" (back rank)
constexpr double BLACK_CAPTURE_COL2_X = 0.75;  // 0.75" (pawns)

// Y positions: file a = 4", file h = 18"
constexpr double CAPTURE_START_Y = A1_CENTER_Y;  // 4.0"

/************ Movement Parameters ************/
constexpr double FEED_RATE = 1000.0;  // inches/min

/************ Capture Zone Slots ************/
// Capture tracking: just count pawns and pieces separately
// Column 1 (inner): back-rank pieces (max 8 per side)
// Column 2 (outer): pawns (max 8 per side)
// Y position determined by count of pieces already captured

// For new game setup, we still need to know starting positions
enum StartingSlot {
    SLOT_ROOK_A = 0,
    SLOT_KNIGHT_B = 1,
    SLOT_BISHOP_C = 2,
    SLOT_QUEEN_D = 3,
    SLOT_KING_E = 4,
    SLOT_BISHOP_F = 5,
    SLOT_KNIGHT_G = 6,
    SLOT_ROOK_H = 7,
    SLOT_PAWN_A = 8,
    SLOT_PAWN_B = 9,
    SLOT_PAWN_C = 10,
    SLOT_PAWN_D = 11,
    SLOT_PAWN_E = 12,
    SLOT_PAWN_F = 13,
    SLOT_PAWN_G = 14,
    SLOT_PAWN_H = 15
};

/************ Physical Position ************/
struct Position {
    double x;
    double y;
    
    Position() : x(0), y(0) {}
    Position(double px, double py) : x(px), y(py) {}
    
    bool operator==(const Position& other) const {
        constexpr double EPSILON = 0.001;
        return std::abs(x - other.x) < EPSILON && 
               std::abs(y - other.y) < EPSILON;
    }
};

/************ Coordinate Translation ************/

// Convert board coordinate to physical position (square center)
Position toPhysical(const BoardCoord& coord);

// Convert physical position back to nearest board coordinate
BoardCoord fromPhysical(const Position& pos);

// Get next available capture slot position based on piece type
// isPawn: true for pawns (column 2), false for back-rank pieces (column 1)
// slotIndex: which slot (0-7) in the respective column
Position getCapturePosition(bool isWhitePiece, bool isPawn, int slotIndex);

// Get starting slot position (for new game setup)
Position getStartingSlotPosition(bool isWhitePiece, StartingSlot slot);

// Get starting board position for a piece (for new game setup)
BoardCoord getStartingSquare(bool isWhite, StartingSlot slot);

/************ G-code Generation ************/

std::string moveCommand(const Position& pos);
std::string magnetOn();
std::string magnetOff();
std::string dwell(int milliseconds);

// Generate pick-and-place sequence (pickup from, move to, release)
std::vector<std::string> generatePickAndPlace(const Position& from, 
                                               const Position& to);

// Generate G-code for traversing a path (piece already held)
std::vector<std::string> generatePathGcode(const Position& start,
                                            const Path& path);

// Generate G-code for complete MovePlan (including captures, relocations)
// capturedPieceType: the type of piece being captured (PAWN, KNIGHT, etc.)
std::vector<std::string> generateMovePlanGcode(const MovePlan& plan,
                                                bool capturedPieceIsWhite,
                                                PieceType capturedPieceType);

// Generate G-code to move a captured piece to its slot
// slotIndex: the slot number (0-7) within the piece's column
std::vector<std::string> generateCaptureGcode(const BoardCoord& square,
                                               bool isWhitePiece,
                                               bool isPawn,
                                               int slotIndex);

// Generate G-code to set up a new game (move all pieces from capture zones to board)
std::vector<std::string> generateNewGameSetup();

/************ GRBL Controller ************/

class GrblController {
private:
    int serialFd;
    bool connected;
    bool dryRunMode;           // If true, print G-code instead of sending to serial
    Position currentPos;
    bool magnetEngaged;
    
    // Simplified capture tracking: count of pieces in each column
    // Column 1 = back-rank pieces, Column 2 = pawns
    int whitePawnsCaptured;    // 0-8
    int whitePiecesCaptured;   // 0-8 (non-pawn pieces)
    int blackPawnsCaptured;    // 0-8
    int blackPiecesCaptured;   // 0-8 (non-pawn pieces)
    
    bool send(const std::string& cmd);
    bool waitForOk(int timeoutMs = 5000);
    std::string readLine(int timeoutMs = 1000);

public:
    GrblController();
    ~GrblController();
    
    // Connection
    bool connect(const char* port, int baudRate = 115200);
    void disconnect();
    bool isConnected() const { return connected; }
    
    /**
     * Enable dry-run mode: no serial port is required. Every G-code command
     * that would be sent to GRBL is printed to stdout instead. The full
     * planning and G-code generation pipeline runs normally so you can audit
     * every command before powering the gantry.
     * 
     * Call this instead of connect() when you have no hardware attached.
     */
    void enableDryRun();
    bool isDryRun() const { return dryRunMode; }
    
    // Command execution
    bool sendCommand(const std::string& cmd);
    bool sendCommands(const std::vector<std::string>& cmds);
    
    // Homing
    bool home();
    
    // Get next available capture slot for a piece type
    // Returns the slot index (0-7) and increments the counter
    int getNextCaptureSlot(bool isWhitePiece, bool isPawn);
    
    // High-level chess operations
    bool executeMove(const MovePlan& plan, 
                     bool capturedPieceIsWhite,
                     PieceType capturedPieceType);
    
    bool executeCapture(const BoardCoord& square, 
                        bool isWhitePiece, 
                        PieceType pieceType);
    
    bool setupNewGame();
    
    // Manual control
    bool moveTo(const Position& pos);
    bool moveTo(const BoardCoord& coord);
    bool setMagnet(bool on);
    
    // State
    Position getCurrentPosition() const { return currentPos; }
    bool isMagnetEngaged() const { return magnetEngaged; }
    
    // Capture counts (for debugging/display)
    int getWhitePawnsCaptured() const { return whitePawnsCaptured; }
    int getWhitePiecesCaptured() const { return whitePiecesCaptured; }
    int getBlackPawnsCaptured() const { return blackPawnsCaptured; }
    int getBlackPiecesCaptured() const { return blackPiecesCaptured; }
    
    // Reset for new game
    void resetCaptureSlots();
};

} // namespace Gantry

#endif
