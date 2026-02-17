// gantry_controller.cpp - Physical coordinate translation and G-code generation

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
    // Physical:   a1 is at (4.0, 4.0), h8 is at (18.0, 18.0)
    double x = A1_CENTER_X + coord.col * SQUARE_SIZE;
    double y = A1_CENTER_Y + (7 - coord.row) * SQUARE_SIZE;
    return Position(x, y);
}

BoardCoord fromPhysical(const Position& pos) {
    int col = static_cast<int>((pos.x - A1_CENTER_X + SQUARE_SIZE / 2.0) / SQUARE_SIZE);
    int row = 7 - static_cast<int>((pos.y - A1_CENTER_Y + SQUARE_SIZE / 2.0) / SQUARE_SIZE);
    
    // Clamp to valid range
    if (col < 0) col = 0;
    if (col > 7) col = 7;
    if (row < 0) row = 0;
    if (row > 7) row = 7;
    
    return BoardCoord(row, col);
}

Position getCapturePosition(bool isWhitePiece, bool isPawn, int slotIndex) {
    double x, y;
    
    // Y position based on slot index (0-7), matching board rank spacing
    y = CAPTURE_START_Y + slotIndex * CAPTURE_VERTICAL_SPACING;
    
    if (isWhitePiece) {
        // White pieces captured go on the right side
        x = isPawn ? WHITE_CAPTURE_COL2_X : WHITE_CAPTURE_COL1_X;
    } else {
        // Black pieces captured go on the left side
        x = isPawn ? BLACK_CAPTURE_COL2_X : BLACK_CAPTURE_COL1_X;
    }
    
    return Position(x, y);
}

Position getStartingSlotPosition(bool isWhitePiece, StartingSlot slot) {
    bool isPawn = (slot >= SLOT_PAWN_A);
    int slotIndex = isPawn ? (slot - SLOT_PAWN_A) : static_cast<int>(slot);
    return getCapturePosition(isWhitePiece, isPawn, slotIndex);
}

BoardCoord getStartingSquare(bool isWhite, StartingSlot slot) {
    int col = (slot >= SLOT_PAWN_A) ? (slot - SLOT_PAWN_A) : static_cast<int>(slot);
    int row;
    
    if (isWhite) {
        row = (slot >= SLOT_PAWN_A) ? 6 : 7;  // rank 2 or rank 1
    } else {
        row = (slot >= SLOT_PAWN_A) ? 1 : 0;  // rank 7 or rank 8
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
                                               bool isPawn,
                                               int slotIndex) {
    std::vector<std::string> gcode;
    
    Position from = toPhysical(square);
    Position to = getCapturePosition(isWhitePiece, isPawn, slotIndex);
    
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
    
    if (!plan.isValid) {
        return gcode;
    }
    
    // Step 1: If capture, move captured piece to capture zone first
    // Note: The actual slot index must be determined by the caller (GrblController)
    // which tracks how many pieces have been captured. This function just generates
    // the G-code given a slot index. For now, we use a placeholder that will be
    // replaced by the caller with the actual slot.
    if (plan.primaryMove.isCapture) {
        bool isPawn = (capturedPieceType == PAWN);
        // Note: slot index 0 is a placeholder - the GrblController will generate
        // the correct G-code with the proper slot index
        std::vector<std::string> captureGcode = generateCaptureGcode(
            plan.primaryMove.to,
            capturedPieceIsWhite,
            isPawn,
            0  // Placeholder - actual slot determined by GrblController
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
    : serialFd(-1), connected(false), currentPos(0, 0), magnetEngaged(false),
      whitePawnsCaptured(0), whitePiecesCaptured(0),
      blackPawnsCaptured(0), blackPiecesCaptured(0) {
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

int GrblController::getNextCaptureSlot(bool isWhitePiece, bool isPawn) {
    int slotIndex;
    if (isWhitePiece) {
        if (isPawn) {
            slotIndex = whitePawnsCaptured++;
        } else {
            slotIndex = whitePiecesCaptured++;
        }
    } else {
        if (isPawn) {
            slotIndex = blackPawnsCaptured++;
        } else {
            slotIndex = blackPiecesCaptured++;
        }
    }
    return slotIndex;
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

bool GrblController::waitForOk(int timeoutMs) {
    std::string response = readLine(timeoutMs);
    return (response == "ok");
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

bool GrblController::sendCommand(const std::string& cmd) {
    if (!send(cmd)) return false;
    return waitForOk();
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
    return sendCommand("$H");
}

bool GrblController::moveTo(const Position& pos) {
    currentPos = pos;
    return sendCommand(moveCommand(pos));
}

bool GrblController::moveTo(const BoardCoord& coord) {
    return moveTo(toPhysical(coord));
}

bool GrblController::setMagnet(bool on) {
    magnetEngaged = on;
    return sendCommand(on ? magnetOn() : magnetOff());
}

bool GrblController::executeCapture(const BoardCoord& square, 
                                     bool isWhitePiece, 
                                     PieceType pieceType) {
    bool isPawn = (pieceType == PAWN);
    int slotIndex = getNextCaptureSlot(isWhitePiece, isPawn);
    
    std::vector<std::string> gcode = generateCaptureGcode(square, isWhitePiece, isPawn, slotIndex);
    
    return sendCommands(gcode);
}

bool GrblController::executeMove(const MovePlan& plan, 
                                  bool capturedPieceIsWhite,
                                  PieceType capturedPieceType) {
    if (!plan.isValid) {
        return false;
    }
    
    std::vector<std::string> gcode;
    
    // Step 1: If capture, move captured piece to capture zone first
    // We generate this directly here to use the correct slot index
    if (plan.primaryMove.isCapture && capturedPieceType != UNKNOWN) {
        bool isPawn = (capturedPieceType == PAWN);
        int slotIndex = getNextCaptureSlot(capturedPieceIsWhite, isPawn);
        
        std::vector<std::string> captureGcode = generateCaptureGcode(
            plan.primaryMove.to,
            capturedPieceIsWhite,
            isPawn,
            slotIndex
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
