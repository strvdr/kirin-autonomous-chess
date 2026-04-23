/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    gantry_controller.cpp - Physical coordinate translation and G-code generation
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
*   */

#include "gantry_controller.h"
#include <cstdio>
#include <cmath>
#include <cstring>

// Only include serial headers on systems that support them
#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#define HAS_SERIAL 1
#else
#define HAS_SERIAL 0
#endif

namespace Gantry {

/************ Coordinate Translation ************/

Position toPhysical(const BoardCoord& coord) {
    // BoardCoord: row 0 = rank 8, row 7 = rank 1
    //             col 0 = file a, col 7 = file h
    // Physical:   calibrated board has:
    //             - ranks increasing along +X
    //             - files increasing along +Y
    //             - a1 centered at (A1_CENTER_X, A1_CENTER_Y)
    double x = A1_CENTER_X + (7 - coord.row) * SQUARE_SIZE;
    double y = A1_CENTER_Y + coord.col * SQUARE_SIZE;
    return Position(x, y);
}

BoardCoord fromPhysical(const Position& pos) {
    int row = 7 - static_cast<int>((pos.x - A1_CENTER_X + SQUARE_SIZE / 2.0) / SQUARE_SIZE);
    int col = static_cast<int>((pos.y - A1_CENTER_Y + SQUARE_SIZE / 2.0) / SQUARE_SIZE);
    
    // Clamp to valid range
    if (col < 0) col = 0;
    if (col > 7) col = 7;
    if (row < 0) row = 0;
    if (row > 7) row = 7;
    
    return BoardCoord(row, col);
}

static bool isPawnSlot(StartingSlot slot) {
    return slot >= SLOT_P1;
}

static int captureRowIndexForSlot(StartingSlot slot) {
    switch (slot) {
        case SLOT_R1:
        case SLOT_P1: return 0;
        case SLOT_R2:
        case SLOT_P2: return 1;
        case SLOT_B1:
        case SLOT_P3: return 2;
        case SLOT_B2:
        case SLOT_P4: return 3;
        case SLOT_N1:
        case SLOT_P5: return 4;
        case SLOT_N2:
        case SLOT_P6: return 5;
        case SLOT_Q:
        case SLOT_P7: return 6;
        case SLOT_K:
        case SLOT_P8: return 7;
    }

    return 0;
}

Position getCapturePosition(bool isWhitePiece, StartingSlot slot) {
    const bool pawnSlot = isPawnSlot(slot);
    const int rowIndex = captureRowIndexForSlot(slot);
    const double y = CAPTURE_START_Y + rowIndex * CAPTURE_VERTICAL_SPACING;

    double x;
    if (isWhitePiece) {
        // White storage is to the right of file h: inner column is back-rank
        // pieces, outer column is pawns.
        x = pawnSlot ? WHITE_CAPTURE_COL2_X : WHITE_CAPTURE_COL1_X;
    } else {
        // Black storage is to the left of file a: outer column is pawns,
        // inner column is back-rank pieces.
        x = pawnSlot ? BLACK_CAPTURE_COL2_X : BLACK_CAPTURE_COL1_X;
    }

    return Position(x, y);
}

Position getStartingSlotPosition(bool isWhitePiece, StartingSlot slot) {
    return getCapturePosition(isWhitePiece, slot);
}

BoardCoord getStartingSquare(bool isWhite, StartingSlot slot) {
    int row = isWhite ? 7 : 0;
    int col = 0;

    switch (slot) {
        case SLOT_R1: col = 0; break;
        case SLOT_R2: col = 7; break;
        case SLOT_B1: col = 2; break;
        case SLOT_B2: col = 5; break;
        case SLOT_N1: col = 1; break;
        case SLOT_N2: col = 6; break;
        case SLOT_Q:  col = 3; break;
        case SLOT_K:  col = 4; break;
        case SLOT_P1: row = isWhite ? 6 : 1; col = 0; break;
        case SLOT_P2: row = isWhite ? 6 : 1; col = 1; break;
        case SLOT_P3: row = isWhite ? 6 : 1; col = 2; break;
        case SLOT_P4: row = isWhite ? 6 : 1; col = 3; break;
        case SLOT_P5: row = isWhite ? 6 : 1; col = 4; break;
        case SLOT_P6: row = isWhite ? 6 : 1; col = 5; break;
        case SLOT_P7: row = isWhite ? 6 : 1; col = 6; break;
        case SLOT_P8: row = isWhite ? 6 : 1; col = 7; break;
    }

    return BoardCoord(row, col);
}

/************ G-code Generation ************/

std::string moveCommand(const Position& pos) {
    char buf[64];
    snprintf(buf, sizeof(buf), "G1 X%.3f Y%.3f F%.0f", pos.x, pos.y, FEED_RATE);
    return std::string(buf);
}

std::string magnetOn() {
    return "M3";
}

std::string magnetOff() {
    return "M5";
}

std::string dwell(int milliseconds) {
    char buf[32];
    snprintf(buf, sizeof(buf), "G4 P%d", milliseconds);
    return std::string(buf);
}

std::vector<std::string> generatePickAndPlace(const Position& from, 
                                               const Position& to) {
    std::vector<std::string> gcode;
    
    // Move to pickup position
    gcode.push_back(moveCommand(from));
    
    // Engage magnet and dwell to ensure contact
    gcode.push_back(magnetOn());
    gcode.push_back(dwell(100));
    
    // Move to destination
    gcode.push_back(moveCommand(to));
    
    // Release magnet and dwell
    gcode.push_back(magnetOff());
    gcode.push_back(dwell(100));
    
    return gcode;
}

std::vector<std::string> generatePathGcode(const Position& start,
                                            const Path& path) {
    std::vector<std::string> gcode;
    (void)start;  // Not needed - we're already at start position
    
    // Move along each square in the path
    for (int i = 0; i < path.length(); i++) {
        Position pos = toPhysical(path.squares[i]);
        gcode.push_back(moveCommand(pos));
    }
    
    return gcode;
}

std::vector<std::string> generateCaptureGcode(const BoardCoord& square,
                                               bool isWhitePiece,
                                               StartingSlot slot) {
    std::vector<std::string> gcode;
    
    Position from = toPhysical(square);
    Position to = getStartingSlotPosition(isWhitePiece, slot);
    
    // Pick up piece
    gcode.push_back(moveCommand(from));
    gcode.push_back(magnetOn());
    gcode.push_back(dwell(100));
    
    // Move to capture slot
    gcode.push_back(moveCommand(to));
    
    // Release
    gcode.push_back(magnetOff());
    gcode.push_back(dwell(100));
    
    return gcode;
}

std::vector<std::string> generateMovePlanGcode(const MovePlan& plan,
                                                bool capturedPieceIsWhite,
                                                PieceType capturedPieceType) {
    std::vector<std::string> gcode;
    (void)capturedPieceType;
    
    if (!plan.isValid) {
        return gcode;
    }
    
    // Step 1: If capture, move captured piece to capture zone first
    // Note: The actual slot index must be determined by the caller (GrblController)
    // which tracks how many pieces have been captured. This function just generates
    // the G-code given a slot index. For now, we use a placeholder that will be
    // replaced by the caller with the actual slot.
    if (plan.primaryMove.isCapture) {
        // Placeholder exact slot for legacy simulation paths that do not
        // currently track identity. Real hardware execution uses the
        // identity-aware GrblController::executeMove path.
            std::vector<std::string> captureGcode = generateCaptureGcode(
                plan.primaryMove.to,
                capturedPieceIsWhite,
                SLOT_P1
            );
        gcode.insert(gcode.end(), captureGcode.begin(), captureGcode.end());
    }
    
    // Step 2: Execute all blocker relocations
    for (const RelocationPlan& reloc : plan.relocations) {
        Position from = toPhysical(reloc.from);
        
        // Pick up blocker
        gcode.push_back(moveCommand(from));
        gcode.push_back(magnetOn());
        gcode.push_back(dwell(100));
        
        // Follow relocation path
        std::vector<std::string> pathGcode = generatePathGcode(from, reloc.path);
        gcode.insert(gcode.end(), pathGcode.begin(), pathGcode.end());
        
        // Release at parking spot
        gcode.push_back(magnetOff());
        gcode.push_back(dwell(100));
    }
    
    // Step 3: Execute primary move
    {
        Position from = toPhysical(plan.primaryMove.from);
        
        // Pick up primary piece
        gcode.push_back(moveCommand(from));
        gcode.push_back(magnetOn());
        gcode.push_back(dwell(100));
        
        // Follow primary path
        std::vector<std::string> pathGcode = generatePathGcode(from, plan.primaryPath);
        gcode.insert(gcode.end(), pathGcode.begin(), pathGcode.end());
        
        // Release at destination
        gcode.push_back(magnetOff());
        gcode.push_back(dwell(100));
    }
    
    // Step 4: Restore all blockers to original positions
    for (const RelocationPlan& restore : plan.restorations) {
        Position from = toPhysical(restore.from);
        Position to = toPhysical(restore.to);
        
        // Pick up from parking spot
        gcode.push_back(moveCommand(from));
        gcode.push_back(magnetOn());
        gcode.push_back(dwell(100));
        
        // Move back to original square
        gcode.push_back(moveCommand(to));
        
        // Release
        gcode.push_back(magnetOff());
        gcode.push_back(dwell(100));
    }
    
    return gcode;
}

std::vector<std::string> generateNewGameSetup() {
    std::vector<std::string> gcode;
    
    // Move all 32 pieces from capture slots to starting squares
    for (int color = 0; color < 2; color++) {
        bool isWhite = (color == 0);
        
        for (int slot = 0; slot < 16; slot++) {
            StartingSlot ss = static_cast<StartingSlot>(slot);
            Position from = getStartingSlotPosition(isWhite, ss);
            BoardCoord toCoord = getStartingSquare(isWhite, ss);
            Position to = toPhysical(toCoord);
            
            std::vector<std::string> pickPlace = generatePickAndPlace(from, to);
            gcode.insert(gcode.end(), pickPlace.begin(), pickPlace.end());
        }
    }
    
    return gcode;
}

/************ GRBL Controller Implementation ************/

GrblController::GrblController() 
    : serialFd(-1), connected(false), dryRunMode(false),
      currentPos(0, 0), magnetEngaged(false),
      whitePawnsCaptured(0), whitePiecesCaptured(0),
      blackPawnsCaptured(0), blackPiecesCaptured(0) {
}

void GrblController::enableDryRun() {
    dryRunMode = true;
    connected = true;  // Bypass all isConnected() guards
    printf("[DRY-RUN] Dry-run mode enabled. No serial connection will be made.\n");
    printf("[DRY-RUN] All G-code will be printed to stdout.\n");
}

GrblController::~GrblController() {
    disconnect();
}

void GrblController::resetCaptureSlots() {
    whitePawnsCaptured = 0;
    whitePiecesCaptured = 0;
    blackPawnsCaptured = 0;
    blackPiecesCaptured = 0;
}

#if HAS_SERIAL

bool GrblController::connect(const char* port, int baudRate) {
    serialFd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (serialFd < 0) {
        perror("Error opening serial port");
        return false;
    }
    
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(serialFd, &tty) != 0) {
        perror("Error getting terminal attributes");
        close(serialFd);
        serialFd = -1;
        return false;
    }
    
    // Set baud rate
    speed_t speed;
    switch (baudRate) {
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        default:     speed = B115200; break;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);
    
    // 8N1 mode
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= (CLOCAL | CREAD);
    
    // Raw mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    
    // Read settings
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;  // 1 second timeout
    
    if (tcsetattr(serialFd, TCSANOW, &tty) != 0) {
        perror("Error setting terminal attributes");
        close(serialFd);
        serialFd = -1;
        return false;
    }
    
    // Flush buffers
    tcflush(serialFd, TCIOFLUSH);
    
    // Wait for GRBL to initialize
    usleep(2000000);  // 2 seconds
    
    // Read any startup messages
    char buf[256];
    while (read(serialFd, buf, sizeof(buf)) > 0) {
        // Discard startup messages
    }
    
    connected = true;
    return true;
}

void GrblController::disconnect() {
    if (serialFd >= 0) {
        close(serialFd);
        serialFd = -1;
    }
    connected = false;
}

bool GrblController::send(const std::string& cmd) {
    if (!connected) return false;
    
    std::string line = cmd + "\n";
    ssize_t written = write(serialFd, line.c_str(), line.length());
    return written == static_cast<ssize_t>(line.length());
}

std::string GrblController::readLine(int timeoutMs) {
    std::string line;
    char c;
    
    fd_set readfds;
    struct timeval tv;
    
    while (true) {
        FD_ZERO(&readfds);
        FD_SET(serialFd, &readfds);
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        
        int ret = select(serialFd + 1, &readfds, NULL, NULL, &tv);
        if (ret <= 0) {
            break;  // Timeout or error
        }
        
        if (read(serialFd, &c, 1) == 1) {
            if (c == '\n') {
                break;
            } else if (c != '\r') {
                line += c;
            }
        }
    }
    
    return line;
}

// Human-readable GRBL error descriptions (error codes 1-38 per GRBL 1.1 docs)
static const char* grblErrorDescription(int code) {
    switch (code) {
        case  1: return "G-code words consist of a letter and a value; letter was not found";
        case  2: return "Numeric value format is not valid or missing";
        case  3: return "Grbl '$' system command was not recognized";
        case  4: return "Negative value received for an expected positive value";
        case  5: return "Homing cycle is not enabled via settings";
        case  6: return "Minimum step pulse time must be greater than 3 usec";
        case  7: return "EEPROM read failed – reset and restored to defaults";
        case  8: return "'$' command cannot be used unless Grbl is IDLE";
        case  9: return "G-code locked out during alarm or jog state";
        case 10: return "Soft limits cannot be enabled without homing also enabled";
        case 11: return "Max characters per line exceeded (70 chars)";
        case 12: return "Grbl '$' setting value exceeds the maximum step rate supported";
        case 13: return "Safety door detected as opened and door state initiated";
        case 14: return "Build info or startup line exceeded EEPROM line length limit";
        case 15: return "Jog target exceeds machine travel; jog command ignored";
        case 16: return "Jog command with no '=' or contains prohibited g-code";
        case 17: return "Laser mode requires PWM output";
        case 20: return "Unsupported or invalid g-code command found in block";
        case 21: return "More than one g-code command from same modal group in block";
        case 22: return "Feed rate has not yet been set or is undefined";
        case 23: return "G-code command in block requires an integer value";
        case 24: return "Two G-code commands that both require the use of the XYZ axis words were detected in the block";
        case 25: return "A G-code word was repeated in the block";
        case 26: return "A G-code command implicitly or explicitly requires XYZ axis words in the block, but none were detected";
        case 27: return "N line number value is not within the valid range of 1-9,999,999";
        case 28: return "A G-code command was sent, but is missing some required P or L words in the line";
        case 29: return "Grbl supports six work coordinate systems G54-G59; G59.1, G59.2, and G59.3 are not supported";
        case 30: return "The G53 G-code command requires either a G0 seek or G1 feed motion mode to be active";
        case 31: return "There are unused axis words in the block and G80 cycle cancellation is missing";
        case 32: return "A G2 or G3 arc was detected and does not have purely XY planar motion";
        case 33: return "The motion command has an invalid target; G2, G3, and G38.2 arcs cannot be defined";
        case 34: return "A G2 or G3 arc, defined with the radius equation, had an error due to the computed arc radius";
        case 35: return "The G35 or G38.x probing cycle failed to trigger within the machine travel";
        case 36: return "The probe did not contact the workpiece within the programmed travel";
        case 37: return "Probe protection feature triggered during probing";
        case 38: return "Spindle control G-code (M3, M4, M5) is not supported in current state";
        default: return "Unknown error";
    }
}

// Human-readable GRBL alarm descriptions (alarm codes 1-11 per GRBL 1.1 docs)
static const char* grblAlarmDescription(int code) {
    switch (code) {
        case  1: return "Hard limit triggered – machine position is likely lost";
        case  2: return "G-code motion target exceeds machine travel; motion aborted";
        case  3: return "Reset while in motion; position lost, re-home required";
        case  4: return "Probe fail: the probe is not in the expected initial state";
        case  5: return "Probe fail: probe did not contact workpiece within travel";
        case  6: return "Homing fail: the active homing cycle was reset";
        case  7: return "Homing fail: safety door was opened during homing";
        case  8: return "Homing fail: cycle failed to clear the limit switch after pulling off";
        case  9: return "Homing fail: could not find limit switch within search distance";
        case 10: return "Homing fail: second dual-axis limit switch not found within search distance";
        case 11: return "Homing fail: axis travel is below the minimum pull-off distance";
        default: return "Unknown alarm";
    }
}

bool GrblController::waitForOk(int timeoutMs) {
    // GRBL may emit informational lines (e.g. "[MSG:...]") before the real
    // response, so we loop until we see "ok", "error:N", "ALARM:N", or timeout.
    while (true) {
        std::string response = readLine(timeoutMs);

        if (response.empty()) {
            // Timeout – readLine returned nothing within the window
            fprintf(stderr, "[GRBL] Timeout waiting for response (no data received)\n");
            return false;
        }

        if (response == "ok") {
            return true;
        }

        if (response.substr(0, 6) == "error:") {
            int code = 0;
            if (response.length() > 6) {
                code = std::stoi(response.substr(6));
            }
            fprintf(stderr, "[GRBL] error:%d – %s\n", code, grblErrorDescription(code));
            return false;
        }

        if (response.substr(0, 6) == "ALARM:") {
            int code = 0;
            if (response.length() > 6) {
                code = std::stoi(response.substr(6));
            }
            fprintf(stderr, "[GRBL] ALARM:%d – %s\n", code, grblAlarmDescription(code));
            fprintf(stderr, "[GRBL] Issuing soft-reset (Ctrl-X) to clear alarm state. Re-home before resuming.\n");
            // Soft reset: send 0x18, then flush the response lines
            char resetCmd = 0x18;
            write(serialFd, &resetCmd, 1);
            // Drain any lines GRBL emits after the reset (typically "Grbl 1.1x ...")
            for (int i = 0; i < 5; ++i) {
                std::string drain = readLine(500);
                if (!drain.empty()) {
                    fprintf(stderr, "[GRBL] (post-reset) %s\n", drain.c_str());
                }
            }
            return false;
        }

        // Informational / feedback lines – print and keep waiting
        fprintf(stderr, "[GRBL] info: %s\n", response.c_str());
    }
}

#else
// Stub implementations for non-Linux systems

bool GrblController::connect(const char*, int) {
    printf("Serial not supported on this platform\n");
    return false;
}

void GrblController::disconnect() {
    connected = false;
}

bool GrblController::send(const std::string&) {
    return false;
}

std::string GrblController::readLine(int) {
    return "";
}

bool GrblController::waitForOk(int) {
    return false;
}

#endif

// Annotate a G-code command for dry-run output
static const char* annotateGcode(const std::string& cmd) {
    if (cmd == "M3")                    return "  ; MAGNET ON";
    if (cmd == "M5")                    return "  ; MAGNET OFF";
    if (cmd == "$H")                    return "  ; HOME (seek limit switches)";
    if (cmd.substr(0, 2) == "G4")       return "  ; DWELL (settle)";
    if (cmd.substr(0, 2) == "G1")       return "  ; MOVE";
    return "";
}

int GrblController::getCommandTimeoutMs(const std::string& cmd) const {
    // Homing can take substantially longer than normal moves, especially if
    // the gantry starts far from the switches.
    if (cmd == "$H") {
        return 120000;
    }

    // Dwell waits for the requested settle time plus controller overhead.
    if (cmd.rfind("G4", 0) == 0) {
        int dwellMs = 0;
        if (sscanf(cmd.c_str(), "G4 P%d", &dwellMs) == 1) {
            return dwellMs + 5000;
        }
        return 10000;
    }

    // Estimate linear move duration from distance and feed rate, then add
    // generous slack for GRBL processing and acceleration.
    if (cmd.rfind("G1", 0) == 0) {
        double x = currentPos.x;
        double y = currentPos.y;
        double feed = FEED_RATE;
        if (sscanf(cmd.c_str(), "G1 X%lf Y%lf F%lf", &x, &y, &feed) >= 2) {
            double dx = x - currentPos.x;
            double dy = y - currentPos.y;
            double distance = std::sqrt(dx * dx + dy * dy);
            double minutes = (feed > 0.0) ? (distance / feed) : 0.0;
            int travelMs = static_cast<int>(minutes * 60.0 * 1000.0);
            return travelMs + 8000;
        }
        return 15000;
    }

    // Magnet toggles and other short commands should still allow some slack.
    return 5000;
}

bool GrblController::sendCommand(const std::string& cmd) {
    if (dryRunMode) {
        printf("  %-30s%s\n", cmd.c_str(), annotateGcode(cmd));
        // Update local state so position tracking stays accurate
        if (cmd.substr(0, 2) == "G1") {
            double x = currentPos.x, y = currentPos.y;
            sscanf(cmd.c_str(), "G1 X%lf Y%lf", &x, &y);
            currentPos = Position(x, y);
        } else if (cmd == "M3") {
            magnetEngaged = true;
        } else if (cmd == "M5") {
            magnetEngaged = false;
        }
        return true;
    }
    if (!send(cmd)) return false;
    bool ok = waitForOk(getCommandTimeoutMs(cmd));
    if (!ok) {
        return false;
    }

    if (cmd.rfind("G1", 0) == 0) {
        double x = currentPos.x, y = currentPos.y;
        if (sscanf(cmd.c_str(), "G1 X%lf Y%lf", &x, &y) >= 2) {
            currentPos = Position(x, y);
        }
    } else if (cmd == "M3") {
        magnetEngaged = true;
    } else if (cmd == "M5") {
        magnetEngaged = false;
    } else if (cmd == "$H") {
        currentPos = Position(0, 0);
    }

    return true;
}

bool GrblController::sendCommands(const std::vector<std::string>& cmds) {
    for (const std::string& cmd : cmds) {
        if (!sendCommand(cmd)) {
            return false;
        }
    }
    return true;
}

bool GrblController::home() {
    if (dryRunMode) {
        printf("[DRY-RUN] $H                          ; HOME gantry to (0,0)\n");
        currentPos = Position(0, 0);
        return true;
    }
    return sendCommand("$H");
}

bool GrblController::moveTo(const Position& pos) {
    return sendCommand(moveCommand(pos));
}

bool GrblController::moveTo(const BoardCoord& coord) {
    return moveTo(toPhysical(coord));
}

bool GrblController::setMagnet(bool on) {
    return sendCommand(on ? magnetOn() : magnetOff());
}

bool GrblController::executeCapture(const BoardCoord& square, 
                                     bool isWhitePiece,
                                     StartingSlot slot) {
    std::vector<std::string> gcode = generateCaptureGcode(square, isWhitePiece, slot);
    
    return sendCommands(gcode);
}

bool GrblController::executeMove(const MovePlan& plan, 
                                  bool capturedPieceIsWhite,
                                  int capturedSlot) {
    if (!plan.isValid) {
        return false;
    }
    
    std::vector<std::string> gcode;
    
    // Step 1: If capture, move captured piece to capture zone first
    // We generate this directly here to use the correct slot index
    if (plan.primaryMove.isCapture && capturedSlot >= 0) {
        std::vector<std::string> captureGcode = generateCaptureGcode(
            plan.primaryMove.to,
            capturedPieceIsWhite,
            static_cast<StartingSlot>(capturedSlot)
        );
        gcode.insert(gcode.end(), captureGcode.begin(), captureGcode.end());
    }
    
    // Step 2: Execute all blocker relocations
    for (const RelocationPlan& reloc : plan.relocations) {
        Position from = toPhysical(reloc.from);
        
        gcode.push_back(moveCommand(from));
        gcode.push_back(magnetOn());
        gcode.push_back(dwell(100));
        
        std::vector<std::string> pathGcode = generatePathGcode(from, reloc.path);
        gcode.insert(gcode.end(), pathGcode.begin(), pathGcode.end());
        
        gcode.push_back(magnetOff());
        gcode.push_back(dwell(100));
    }
    
    // Step 3: Execute primary move
    {
        Position from = toPhysical(plan.primaryMove.from);
        
        gcode.push_back(moveCommand(from));
        gcode.push_back(magnetOn());
        gcode.push_back(dwell(100));
        
        std::vector<std::string> pathGcode = generatePathGcode(from, plan.primaryPath);
        gcode.insert(gcode.end(), pathGcode.begin(), pathGcode.end());
        
        gcode.push_back(magnetOff());
        gcode.push_back(dwell(100));
    }
    
    // Step 4: Restore all blockers to original positions
    for (const RelocationPlan& restore : plan.restorations) {
        Position from = toPhysical(restore.from);
        Position to = toPhysical(restore.to);
        
        gcode.push_back(moveCommand(from));
        gcode.push_back(magnetOn());
        gcode.push_back(dwell(100));
        
        gcode.push_back(moveCommand(to));
        
        gcode.push_back(magnetOff());
        gcode.push_back(dwell(100));
    }
    
    return sendCommands(gcode);
}

bool GrblController::setupNewGame() {
    std::vector<std::string> gcode = generateNewGameSetup();
    
    if (!sendCommands(gcode)) {
        return false;
    }
    
    resetCaptureSlots();
    return true;
}

}
