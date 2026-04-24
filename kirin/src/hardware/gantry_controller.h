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
*              22" physical rail travel
*    |<------------------------------->|
*    |                                 |
*    |  +-+ +---------------------+ +-+|
*    |  |B| |                     | |W||
*    |  |L| |     12" board       | |H||
*    |  |A| |                     | |I||
*    |  |C| |      8x8 grid       | |T||
*    |  |K| |                     | |E||
*    |  +-+ +---------------------+ +-+|
*    |  3.0"  .5"    12"    .5"  3.0" |
*    |<---------- 19" occupied width ---------->|
*       ^                            ^
*       |                            |
*     Black captures            White captures
*     X = 1.5", 3.0"            X = 17.5", 19.0"
*     Y = 4.420" to 14.920"     Y = 4.420" to 14.920"
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

// Calibrated square-center origin for the physical board in the same G54
// work-coordinate system used by Kirin's normal G1 moves.
// Current corner calibration:
//   a1 = (5.000, 4.420)
//   a8 = (15.500, 4.420)
//   h1 = (5.000, 14.920)
//   h8 = (15.500, 14.920)
// This means ranks advance along +X and files advance along +Y.
constexpr double A1_CENTER_X = 5.000;
constexpr double A1_CENTER_Y = 4.420;

/************ Capture Zone Layout ************/
// Two vertical columns per side:
//   Black side (left of file 1):  P1 R1 / P2 R2 / P3 N1 / P4 N2 / ...
//   White side (right of file 8): R1 P1 / R2 P2 / N1 P3 / N2 P4 / ...
// The inner column (closer to the board) holds back-rank pieces.
// The outer column holds pawns P1-P8.
// Storage squares are the same 1.5" grid as the board, so rows align exactly
// with board-file Y coordinates.
constexpr double CAPTURE_VERTICAL_SPACING = 1.5;
constexpr double CAPTURE_HORIZONTAL_SPACING = 1.5;

// Storage-column centers derived from the board calibration plus hardware
// geometry: 0.5" gap from board edge to nearest storage-square edge.
// White storage is to the right of the h-file.
constexpr double WHITE_CAPTURE_COL1_X = 17.500;  // inner, back-rank pieces
constexpr double WHITE_CAPTURE_COL2_X = 19.000;  // outer, pawns

// Black storage is to the left of the a-file.
constexpr double BLACK_CAPTURE_COL1_X = 3.000;  // inner, back-rank pieces
constexpr double BLACK_CAPTURE_COL2_X = 1.500;  // outer, pawns

// Bottom storage row aligns with a1/h1 file height in the calibrated G54 frame.
constexpr double CAPTURE_START_Y = A1_CENTER_Y;

/************ Movement Parameters ************/
constexpr double FEED_RATE = 1000.0;  // inches/min

/************ Capture Zone Slots ************/
// Exact slot identities are preserved in the tracker. The storage layout uses
// fixed physical bins from top (rank 8) to bottom (rank 1):
//   inner column: R1, R2, B1, B2, N1, N2, Q, K
//   outer column: P1, P2, P3, P4, P5, P6, P7, P8
// Origin mapping is:
//   a-rook -> R1, h-rook -> R2
//   c-bishop -> B1, f-bishop -> B2
//   b-knight -> N1, g-knight -> N2
//   a-pawn..h-pawn -> P1..P8
enum StartingSlot {
    SLOT_R1 = 0,
    SLOT_R2 = 1,
    SLOT_B1 = 2,
    SLOT_B2 = 3,
    SLOT_N1 = 4,
    SLOT_N2 = 5,
    SLOT_Q  = 6,
    SLOT_K  = 7,
    SLOT_P1 = 8,
    SLOT_P2 = 9,
    SLOT_P3 = 10,
    SLOT_P4 = 11,
    SLOT_P5 = 12,
    SLOT_P6 = 13,
    SLOT_P7 = 14,
    SLOT_P8 = 15
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

// Get capture/storage position for an exact designated slot.
Position getCapturePosition(bool isWhitePiece, StartingSlot slot);

// Get starting slot position (for new game setup)
Position getStartingSlotPosition(bool isWhitePiece, StartingSlot slot);

// Get starting board position for a piece (for new game setup)
BoardCoord getStartingSquare(bool isWhite, StartingSlot slot);

/************ G-code Generation ************/

std::string moveCommand(const Position& pos);
std::string magnetOn();
std::string magnetOff();
std::string dwell(int milliseconds);
std::vector<std::string> motionSetupCommands();

// Generate pick-and-place sequence (pickup from, move to, release)
std::vector<std::string> generatePickAndPlace(const Position& from, 
                                               const Position& to);

// Generate G-code for traversing a path (piece already held)
std::vector<std::string> generatePathGcode(const Position& start,
                                            const Path& path);

// Generate G-code to move a captured piece to an exact designated slot.
std::vector<std::string> generateCaptureGcode(const BoardCoord& square,
                                               bool isWhitePiece,
                                               StartingSlot slot);

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
    
    int getCommandTimeoutMs(const std::string& cmd) const;
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
    
    // High-level chess operations
    bool executeMove(const MovePlan& plan, 
                     bool capturedPieceIsWhite,
                     int capturedSlot);
    
    bool executeCapture(const BoardCoord& square, 
                        bool isWhitePiece, 
                        StartingSlot slot);
    
    bool setupNewGame();
    
    // Manual control
    bool moveTo(const Position& pos);
    bool moveTo(const BoardCoord& coord);
    bool setMagnet(bool on);
    
    // State
    Position getCurrentPosition() const { return currentPos; }
    bool isMagnetEngaged() const { return magnetEngaged; }
    
};

} // namespace Gantry

#endif
