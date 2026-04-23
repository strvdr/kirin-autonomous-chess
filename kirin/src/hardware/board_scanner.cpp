/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    board_scanner.cpp - Hall effect sensor board scanning implementation
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
*/

#include "board_scanner.h"
#include "piece_tracker.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

// Engine headers for legal move matching
#include "bitboard.h"
#include "movegen.h"

// Use libgpiod on Linux for GPIO access (modern interface, no root required)
// Define HAS_GPIOD at build time (e.g. -DHAS_GPIOD) when libgpiod-dev is installed.
// On the Pi: sudo apt install libgpiod-dev
//
// This code targets libgpiod v2.x (shipped with Raspberry Pi OS Bookworm+).
// The v2 API uses gpiod_line_request objects instead of individual gpiod_line handles.
#ifdef HAS_GPIOD
#include <gpiod.h>
#define HAS_GPIO 1
#else
#define HAS_GPIO 0
#endif

/************ GPIO Handle (internal) ************/
// Wraps libgpiod v2 state so the header doesn't need to include gpiod.h
#if HAS_GPIO
static struct gpiod_chip* gpioChip = nullptr;

// In v2, lines are requested in bulk via gpiod_line_request objects.
// We use two requests: one for the 4 output select lines, one for the 6 input mux lines.
static struct gpiod_line_request* selectRequest = nullptr;  // 4 output lines (S0-S3)
static struct gpiod_line_request* muxRequest    = nullptr;  // 6 input lines (mux SIG)

// Store the GPIO offsets so we can reference them in set/get calls
static unsigned int selectOffsets[4] = {};
static unsigned int muxOffsets[TOTAL_MUX_COUNT] = {};
#endif

namespace {

constexpr int BLACK_STORAGE_MUX = 0;
constexpr int WHITE_STORAGE_MUX = 5;
constexpr int BOARD_MUX_INDICES[BOARD_MUX_COUNT] = {1, 2, 3, 4};

} // namespace

/************ Constructor / Destructor ************/

BoardScanner::BoardScanner()
    : initialized(false), simulationMode(false),
      simulatedOccupancy(0), simulatedBlackStorage(0), simulatedWhiteStorage(0),
      lastStableScan(0), lastBlackStorage(0), lastWhiteStorage(0) {
    selectPins[0] = DEFAULT_PIN_S0;
    selectPins[1] = DEFAULT_PIN_S1;
    selectPins[2] = DEFAULT_PIN_S2;
    selectPins[3] = DEFAULT_PIN_S3;

    muxPins[0] = DEFAULT_PIN_MUX0;
    muxPins[1] = DEFAULT_PIN_MUX1;
    muxPins[2] = DEFAULT_PIN_MUX2;
    muxPins[3] = DEFAULT_PIN_MUX3;
    muxPins[4] = DEFAULT_PIN_MUX4;
    muxPins[5] = DEFAULT_PIN_MUX5;
}

BoardScanner::BoardScanner(const int sPins[4], const int mPins[TOTAL_MUX_COUNT])
    : initialized(false), simulationMode(false),
      simulatedOccupancy(0), simulatedBlackStorage(0), simulatedWhiteStorage(0),
      lastStableScan(0), lastBlackStorage(0), lastWhiteStorage(0) {
    for (int i = 0; i < 4; i++) {
        selectPins[i] = sPins[i];
    }
    for (int i = 0; i < TOTAL_MUX_COUNT; i++) {
        muxPins[i] = mPins[i];
    }
}

BoardScanner::~BoardScanner() {
#if HAS_GPIO
    if (initialized && !simulationMode) {
        if (selectRequest) {
            gpiod_line_request_release(selectRequest);
            selectRequest = nullptr;
        }
        if (muxRequest) {
            gpiod_line_request_release(muxRequest);
            muxRequest = nullptr;
        }
        if (gpioChip) {
            gpiod_chip_close(gpioChip);
            gpioChip = nullptr;
        }
    }
#endif
}

/************ Initialization ************/

bool BoardScanner::init() {
    if (simulationMode) {
        printf("[SCANNER] Simulation mode — no GPIO initialization\n");
        initialized = true;
        return true;
    }

#if HAS_GPIO
    // Open the GPIO chip (Pi 5 uses gpiochip4, Pi 4 uses gpiochip0)
    const char* chipPaths[] = { "/dev/gpiochip4", "/dev/gpiochip0", nullptr };
    for (int i = 0; chipPaths[i] != nullptr; i++) {
        gpioChip = gpiod_chip_open(chipPaths[i]);
        if (gpioChip) {
            printf("[SCANNER] Opened GPIO chip: %s\n", chipPaths[i]);
            break;
        }
    }
    if (!gpioChip) {
        fprintf(stderr, "[SCANNER] Error: could not open GPIO chip\n");
        return false;
    }

    // --- Request select lines as outputs (S0-S3) ---
    {
        struct gpiod_line_settings* settings = gpiod_line_settings_new();
        if (!settings) {
            fprintf(stderr, "[SCANNER] Error: could not create line settings\n");
            return false;
        }
        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

        struct gpiod_line_config* lineConfig = gpiod_line_config_new();
        if (!lineConfig) {
            gpiod_line_settings_free(settings);
            fprintf(stderr, "[SCANNER] Error: could not create line config\n");
            return false;
        }

        for (int i = 0; i < 4; i++) {
            selectOffsets[i] = static_cast<unsigned int>(selectPins[i]);
        }
        if (gpiod_line_config_add_line_settings(lineConfig, selectOffsets, 4, settings) < 0) {
            fprintf(stderr, "[SCANNER] Error: could not add select line settings\n");
            gpiod_line_settings_free(settings);
            gpiod_line_config_free(lineConfig);
            return false;
        }

        struct gpiod_request_config* reqConfig = gpiod_request_config_new();
        if (reqConfig) {
            gpiod_request_config_set_consumer(reqConfig, "kirin-scanner");
        }

        selectRequest = gpiod_chip_request_lines(gpioChip, reqConfig, lineConfig);

        if (reqConfig) gpiod_request_config_free(reqConfig);
        gpiod_line_config_free(lineConfig);
        gpiod_line_settings_free(settings);

        if (!selectRequest) {
            fprintf(stderr, "[SCANNER] Error: could not request select lines (GPIO %d,%d,%d,%d)\n",
                    selectPins[0], selectPins[1], selectPins[2], selectPins[3]);
            return false;
        }
    }

    // --- Request mux SIG lines as inputs (6 lines) ---
    {
        struct gpiod_line_settings* settings = gpiod_line_settings_new();
        if (!settings) {
            fprintf(stderr, "[SCANNER] Error: could not create line settings\n");
            return false;
        }
        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);

        struct gpiod_line_config* lineConfig = gpiod_line_config_new();
        if (!lineConfig) {
            gpiod_line_settings_free(settings);
            fprintf(stderr, "[SCANNER] Error: could not create line config\n");
            return false;
        }

        for (int i = 0; i < TOTAL_MUX_COUNT; i++) {
            muxOffsets[i] = static_cast<unsigned int>(muxPins[i]);
        }
        if (gpiod_line_config_add_line_settings(lineConfig, muxOffsets, TOTAL_MUX_COUNT, settings) < 0) {
            fprintf(stderr, "[SCANNER] Error: could not add mux line settings\n");
            gpiod_line_settings_free(settings);
            gpiod_line_config_free(lineConfig);
            return false;
        }

        struct gpiod_request_config* reqConfig = gpiod_request_config_new();
        if (reqConfig) {
            gpiod_request_config_set_consumer(reqConfig, "kirin-scanner");
        }

        muxRequest = gpiod_chip_request_lines(gpioChip, reqConfig, lineConfig);

        if (reqConfig) gpiod_request_config_free(reqConfig);
        gpiod_line_config_free(lineConfig);
        gpiod_line_settings_free(settings);

        if (!muxRequest) {
            fprintf(stderr, "[SCANNER] Error: could not request mux lines (GPIO %d,%d,%d,%d,%d,%d)\n",
                    muxPins[0], muxPins[1], muxPins[2], muxPins[3], muxPins[4], muxPins[5]);
            return false;
        }
    }

    printf("[SCANNER] GPIO initialized successfully (6 muxes: 4 board + 2 storage)\n");
    printf("  Select lines: GPIO %d, %d, %d, %d\n",
           selectPins[0], selectPins[1], selectPins[2], selectPins[3]);
    printf("  Board muxes:  MUX 1 GPIO %d, MUX 2 GPIO %d, MUX 3 GPIO %d, MUX 4 GPIO %d\n",
           muxPins[1], muxPins[2], muxPins[3], muxPins[4]);
    printf("  Storage muxes: MUX 0 GPIO %d (black), MUX 5 GPIO %d (white)\n",
           muxPins[BLACK_STORAGE_MUX], muxPins[WHITE_STORAGE_MUX]);

    initialized = true;
    return true;

#else
    fprintf(stderr, "[SCANNER] Error: GPIO not supported on this platform\n");
    fprintf(stderr, "          Use enableSimulation() for testing without hardware\n");
    return false;
#endif
}

void BoardScanner::enableSimulation() {
    simulationMode = true;
    initialized = true;
    printf("[SCANNER] Simulation mode enabled\n");
}

/************ Low-Level GPIO Operations ************/

void BoardScanner::setMuxChannel(int channel) {
#if HAS_GPIO
    if (simulationMode) return;

    // Set S0-S3 from the 4 bits of the channel number.
    // In libgpiod v2, we set values on the request object using the line offset.
    for (int i = 0; i < 4; i++) {
        int bit = (channel >> i) & 1;
        enum gpiod_line_value val = bit ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
        gpiod_line_request_set_value(selectRequest, selectOffsets[i], val);
    }

    // Allow the mux output to settle
    std::this_thread::sleep_for(std::chrono::microseconds(MUX_SETTLE_US));
#else
    (void)channel;
#endif
}

bool BoardScanner::readMuxOutput(int muxIndex) {
#if HAS_GPIO
    if (simulationMode) return false;

    enum gpiod_line_value val = gpiod_line_request_get_value(muxRequest, muxOffsets[muxIndex]);
    // A3144 is active-low: INACTIVE (low) = magnet present, ACTIVE (high) = no magnet
    return (val == GPIOD_LINE_VALUE_INACTIVE);
#else
    (void)muxIndex;
    return false;
#endif
}

int BoardScanner::toSquareIndex(int muxIndex, int channel) {
    if (muxIndex < BOARD_MUX_INDICES[0] || muxIndex > BOARD_MUX_INDICES[BOARD_MUX_COUNT - 1] ||
        channel < 0 || channel >= CHANNELS_PER_MUX) {
        return -1;
    }

    const int filePair = muxIndex - BOARD_MUX_INDICES[0];
    const int col = filePair * 2 + (channel / 8);
    const int row = channel % 8;
    return row * 8 + col;
}

/************ Board Scanning ************/

Bitboard BoardScanner::scanBoard() {
    if (!initialized) {
        fprintf(stderr, "[SCANNER] Error: not initialized\n");
        return 0;
    }

    if (simulationMode) {
        return simulatedOccupancy;
    }

    Bitboard result = 0;

    // Cycle through all 16 channels
    for (int channel = 0; channel < CHANNELS_PER_MUX; channel++) {
        setMuxChannel(channel);

        // Read the 4 board muxes at this channel address
        for (int i = 0; i < BOARD_MUX_COUNT; i++) {
            const int mux = BOARD_MUX_INDICES[i];
            if (readMuxOutput(mux)) {
                int square = toSquareIndex(mux, channel);
                if (square < 0) continue;
                result |= (1ULL << square);
            }
        }
    }

    return result;
}

Bitboard BoardScanner::scanBoardDebounced() {
    Bitboard current = scanBoard();
    int stableCount = 1;

    while (stableCount < DEBOUNCE_SCANS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(DEBOUNCE_DELAY_MS));
        Bitboard next = scanBoard();

        if (next == current) {
            stableCount++;
        } else {
            current = next;
            stableCount = 1;
        }
    }

    lastStableScan = current;
    return current;
}

/************ Storage Zone Scanning ************/

uint16_t BoardScanner::scanStorage(bool isWhiteSide) {
    if (!initialized) {
        fprintf(stderr, "[SCANNER] Error: not initialized\n");
        return 0;
    }

    if (simulationMode) {
        return isWhiteSide ? simulatedWhiteStorage : simulatedBlackStorage;
    }

    // MUX 0 = black storage, MUX 5 = white storage
    int muxIndex = isWhiteSide ? WHITE_STORAGE_MUX : BLACK_STORAGE_MUX;
    uint16_t result = 0;

    for (int channel = 0; channel < CHANNELS_PER_MUX; channel++) {
        setMuxChannel(channel);

        if (readMuxOutput(muxIndex)) {
            result |= (1 << channel);
        }
    }

    return result;
}

uint16_t BoardScanner::scanStorageDebounced(bool isWhiteSide) {
    uint16_t current = scanStorage(isWhiteSide);
    int stableCount = 1;

    while (stableCount < DEBOUNCE_SCANS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(DEBOUNCE_DELAY_MS));
        uint16_t next = scanStorage(isWhiteSide);

        if (next == current) {
            stableCount++;
        } else {
            current = next;
            stableCount = 1;
        }
    }

    return current;
}

/************ Legal Move Matching ************/

bool BoardScanner::matchLegalMove(Bitboard before, Bitboard after,
                                  int captureSlot, bool capturedIsWhite,
                                  const PieceTracker& tracker,
                                  int& matchedMove) {
    // If a storage slot gained a piece, find where that piece currently
    // sits on the board. This is the key to disambiguation: the slot
    // tells us the exact identity of the captured piece.
    int capturedPieceSquare = -1;
    bool isCapture = (captureSlot != SLOT_NONE);

    if (isCapture) {
        // The captured piece is on the opponent's side.
        // capturedIsWhite tells us which side the piece belongs to.
        capturedPieceSquare = tracker.getSquareForSlot(capturedIsWhite, captureSlot);

        if (capturedPieceSquare < 0) {
            // The slot says this piece should be on the board, but the
            // tracker says it's already been captured. Wrong slot.
            return false;
        }
    }

    // Generate all legal moves from the current engine state
    moves moveList;
    moveList.count = 0;
    generateMoves(&moveList);

    // Save engine state so we can test legality (makeMove modifies globals)
    U64 bitboardsCopy[12], occupancyCopy[3];
    int sideCopy = side, enpassantCopy = enpassant, castleCopy = castle;
    U64 hashKeyCopy = hashKey;

    memcpy(bitboardsCopy, bitboards, sizeof(bitboardsCopy));
    memcpy(occupancyCopy, occupancy, sizeof(occupancyCopy));

    int matchCount = 0;
    int lastMatch = 0;

    for (int i = 0; i < moveList.count; i++) {
        int move = moveList.moves[i];

        // Check legality by trying to make the move
        if (!makeMove(move, allMoves)) {
            memcpy(bitboards, bitboardsCopy, sizeof(bitboardsCopy));
            memcpy(occupancy, occupancyCopy, sizeof(occupancyCopy));
            side = sideCopy;
            enpassant = enpassantCopy;
            castle = castleCopy;
            hashKey = hashKeyCopy;
            continue;
        }

        // Move is legal — restore state and check if it matches the sensor diff
        memcpy(bitboards, bitboardsCopy, sizeof(bitboardsCopy));
        memcpy(occupancy, occupancyCopy, sizeof(occupancyCopy));
        side = sideCopy;
        enpassant = enpassantCopy;
        castle = castleCopy;
        hashKey = hashKeyCopy;

        // Gate 1: capture flag must agree with storage change.
        bool moveIsCapture = getCapture(move) != 0;
        if (moveIsCapture != isCapture) {
            continue;
        }

        // Gate 2: if this is a capture, the target must be the piece
        // identified by the storage slot.
        if (isCapture) {
            int moveTarget = getTarget(move);

            if (getEnpassant(move)) {
                // En passant: the captured pawn is on a different square
                int epCapturedSquare = (getSource(move) / 8) * 8 + (moveTarget % 8);
                if (epCapturedSquare != capturedPieceSquare) {
                    continue;
                }
            } else {
                // Normal capture: target square holds the captured piece
                if (moveTarget != capturedPieceSquare) {
                    continue;
                }
            }
        }

        // Gate 3: predict the board occupancy after this move and compare
        Bitboard predicted = before;

        int source = getSource(move);
        int target = getTarget(move);

        // Clear source square
        bb_clearBit(predicted, source);

        // Handle captures
        if (getCapture(move)) {
            if (getEnpassant(move)) {
                int capturedPawnSquare = (source / 8) * 8 + (target % 8);
                bb_clearBit(predicted, capturedPawnSquare);
            }
            // Normal captures: target was occupied and stays occupied
            // (capturing piece replaces captured piece). No change needed.
        }

        // Set target square (piece lands here)
        bb_setBit(predicted, target);

        // Handle castling: also move the rook
        if (getCastle(move)) {
            if (target == g1) {        // White kingside
                bb_clearBit(predicted, h1);
                bb_setBit(predicted, f1);
            } else if (target == c1) { // White queenside
                bb_clearBit(predicted, a1);
                bb_setBit(predicted, d1);
            } else if (target == g8) { // Black kingside
                bb_clearBit(predicted, h8);
                bb_setBit(predicted, f8);
            } else if (target == c8) { // Black queenside
                bb_clearBit(predicted, a8);
                bb_setBit(predicted, d8);
            }
        }

        // Compare predicted occupancy to actual sensor reading
        if (predicted == after) {
            matchCount++;
            lastMatch = move;
        }
    }

    if (matchCount == 1) {
        matchedMove = lastMatch;
        return true;
    }

    // matchCount > 1: ambiguous. With storage slot identity, the only
    // remaining ambiguity is promotion variants (same source, same target,
    // same capture, different promoted piece). Default to queen.
    if (matchCount > 1) {
        // All matches should be promotion variants. Find the queen promotion.
        // Re-scan: regenerate and find queen promotion among matches.
        for (int i = 0; i < moveList.count; i++) {
            int move = moveList.moves[i];
            int promoted = getPromoted(move);
            if (promoted && (promoted == Q || promoted == q)) {
                // Verify it matches by re-checking gates
                bool moveIsCapture = getCapture(move) != 0;
                if (moveIsCapture != isCapture) continue;

                if (isCapture) {
                    int moveTarget = getTarget(move);
                    if (getEnpassant(move)) {
                        int epSq = (getSource(move) / 8) * 8 + (moveTarget % 8);
                        if (epSq != capturedPieceSquare) continue;
                    } else {
                        if (moveTarget != capturedPieceSquare) continue;
                    }
                }

                Bitboard predicted = before;
                bb_clearBit(predicted, getSource(move));
                if (getCapture(move) && getEnpassant(move)) {
                    int epSquare = (getSource(move) / 8) * 8 + (getTarget(move) % 8);
                    bb_clearBit(predicted, epSquare);
                }
                bb_setBit(predicted, getTarget(move));

                if (predicted == after) {
                    matchedMove = move;
                    return true;
                }
            }
        }
    }

    return false;
}

/************ Legal Move Detection ************/

ScannerMoveResult BoardScanner::waitForLegalMove(
    Bitboard currentOccupancy,
    uint16_t currentBlackStorage,
    uint16_t currentWhiteStorage,
    const PieceTracker& tracker,
    int timeoutMs,
    void (*illegalStateCallback)(),
    void (*wrongSlotCallback)()
) {
    ScannerMoveResult result;
    auto startTime = std::chrono::steady_clock::now();

    // Track how long we've been in an unrecognized stable state
    // so we can alert the player if something looks wrong.
    auto unrecognizedSince = std::chrono::steady_clock::time_point{};
    bool inUnrecognizedState = false;
    bool alertFired = false;

    while (true) {
        // Check timeout
        if (timeoutMs > 0) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            int elapsedMs = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
            if (elapsedMs >= timeoutMs) {
                result.timeout = true;
                return result;
            }
        }

        // Take a raw scan of the board
        Bitboard rawBoard = scanBoard();

        // Also do a quick check of the storage zones
        uint16_t rawBlack = scanStorage(false);
        uint16_t rawWhite = scanStorage(true);

        // If nothing changed at all (board and storage both unchanged), keep polling
        if (rawBoard == currentOccupancy &&
            rawBlack == currentBlackStorage &&
            rawWhite == currentWhiteStorage) {
            // Everything matches the baseline — no activity.
            inUnrecognizedState = false;
            alertFired = false;

            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
            continue;
        }

        // Something changed — wait for BOTH board and storage to stabilize.
        // We debounce them together: scan all 6 muxes and require all of
        // them to be stable for DEBOUNCE_SCANS consecutive reads.
        Bitboard stableBoard = rawBoard;
        uint16_t stableBlack = rawBlack;
        uint16_t stableWhite = rawWhite;
        int stableCount = 1;

        while (stableCount < DEBOUNCE_SCANS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(DEBOUNCE_DELAY_MS));

            Bitboard nextBoard = scanBoard();
            uint16_t nextBlack = scanStorage(false);
            uint16_t nextWhite = scanStorage(true);

            if (nextBoard == stableBoard &&
                nextBlack == stableBlack &&
                nextWhite == stableWhite) {
                stableCount++;
            } else {
                stableBoard = nextBoard;
                stableBlack = nextBlack;
                stableWhite = nextWhite;
                stableCount = 1;
            }
        }

        // If debounce settled back to the original state, nothing happened
        // (player picked up a piece, thought about it, put it back)
        if (stableBoard == currentOccupancy &&
            stableBlack == currentBlackStorage &&
            stableWhite == currentWhiteStorage) {
            inUnrecognizedState = false;
            alertFired = false;
            continue;
        }

        // Identify which specific storage slot changed (if any).
        // New bits in storage = slots that gained a piece.
        uint16_t newBlackSlots = stableBlack & ~currentBlackStorage;
        uint16_t newWhiteSlots = stableWhite & ~currentWhiteStorage;

        int captureSlot = SLOT_NONE;
        bool capturedIsWhite = false;

        // Check if exactly one new slot appeared
        int newBlackCount = __builtin_popcount(newBlackSlots);
        int newWhiteCount = __builtin_popcount(newWhiteSlots);

        if (newBlackCount == 1 && newWhiteCount == 0) {
            // A piece was placed in black's storage → a black piece was captured
            captureSlot = __builtin_ctz(newBlackSlots);  // Which slot (0-15)
            capturedIsWhite = false;  // Black piece captured
        } else if (newWhiteCount == 1 && newBlackCount == 0) {
            // A piece was placed in white's storage → a white piece was captured
            captureSlot = __builtin_ctz(newWhiteSlots);
            capturedIsWhite = true;   // White piece captured
        } else if (newBlackCount > 1 || newWhiteCount > 1 ||
                   (newBlackCount == 1 && newWhiteCount == 1)) {
            // Multiple slots changed at once — something is wrong.
            // Keep waiting; this might be a transient state.
            if (!inUnrecognizedState) {
                inUnrecognizedState = true;
                unrecognizedSince = std::chrono::steady_clock::now();
                alertFired = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
            continue;
        }
        // else: no new slots (newBlackCount == 0 && newWhiteCount == 0)
        // → non-capture move or mid-move state. captureSlot stays SLOT_NONE.

        // Validate the slot: if a slot gained a piece, but the piece
        // that belongs to that slot is already captured, the player
        // used the wrong slot.
        if (captureSlot != SLOT_NONE) {
            if (!tracker.isPieceAlive(capturedIsWhite, captureSlot)) {
                // Wrong slot: this piece was already captured.
                if (wrongSlotCallback) {
                    wrongSlotCallback();
                }
                result.wrongSlot = true;
                // Don't return — the player needs to fix this.
                // Keep waiting for them to move the piece to the right slot.
                std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
                continue;
            }
        }

        // We have a stable state that differs from the original.
        // Try to match it against a legal move, using the specific storage
        // slot to identify which piece was captured.
        int matchedMove = 0;
        if (matchLegalMove(currentOccupancy, stableBoard,
                           captureSlot, capturedIsWhite, tracker, matchedMove)) {
            // We found exactly one legal move that produces this board state.
            result.engineMove = matchedMove;
            result.success = true;

            // Build the UCI string
            int source = getSource(matchedMove);
            int target = getTarget(matchedMove);
            int promoted = getPromoted(matchedMove);
            char promoChar = '\0';
            if (promoted) {
                int normalizedPromo = promoted % 6;
                const char promoChars[] = "pnbrqk";
                promoChar = promoChars[normalizedPromo];
            }
            squaresToMoveString(source, target, result.uciString, promoChar);

            // Update baselines
            lastStableScan = stableBoard;
            lastBlackStorage = stableBlack;
            lastWhiteStorage = stableWhite;

            return result;
        }

        // No legal move matches. Could be:
        // - Mid-move (player hasn't finished moving yet)
        // - Wrong storage slot (captureSlot doesn't match any legal capture)
        //
        // If a storage slot changed but no capture matches, that's a wrong-slot
        // situation — the piece identity doesn't correspond to any legal capture.
        if (captureSlot != SLOT_NONE) {
            // A piece was placed in storage but it doesn't match any legal
            // capture from the current board state. Wrong slot.
            if (wrongSlotCallback && !alertFired) {
                wrongSlotCallback();
                alertFired = true;
            }
            result.wrongSlot = true;
            // Keep waiting — player needs to fix it
            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
            continue;
        }

        // No storage change and no match — player is mid-move.
        // Track how long we've been in this unrecognized state.
        if (!inUnrecognizedState) {
            inUnrecognizedState = true;
            unrecognizedSince = std::chrono::steady_clock::now();
            alertFired = false;
        } else if (!alertFired && illegalStateCallback) {
            auto unrecognizedDuration = std::chrono::steady_clock::now() - unrecognizedSince;
            int unrecognizedMs = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(unrecognizedDuration).count());
            if (unrecognizedMs >= ILLEGAL_STATE_ALERT_MS) {
                illegalStateCallback();
                alertFired = true;
                result.illegalAlert = true;
                // Don't return — keep waiting. The player might fix it.
            }
        }

        // Keep polling — the move isn't finished yet
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    }
}

/************ Legacy Move Inference ************/

bool BoardScanner::inferMove(Bitboard before, Bitboard after, int& from, int& to) {
    Bitboard changed = before ^ after;

    // Bits that were set (occupied) and are now clear (empty) = pieces lifted
    Bitboard cleared = before & changed;

    // Bits that were clear (empty) and are now set (occupied) = pieces placed
    Bitboard set = after & changed;

    int clearedCount = countBits(cleared);
    int setCount     = countBits(set);

    // Normal move: one square cleared, one square set
    if (clearedCount == 1 && setCount == 1) {
        from = getLSBindex(cleared);
        to   = getLSBindex(set);
        return true;
    }

    // Capture: one square cleared, zero squares set
    if (clearedCount == 1 && setCount == 0) {
        from = getLSBindex(cleared);
        to = -1;
        return false;  // Caller must use legal move list to resolve
    }

    // En passant: two squares cleared, one square set
    if (clearedCount == 2 && setCount == 1) {
        to = getLSBindex(set);
        int sq1 = getLSBindex(cleared);
        Bitboard remaining = cleared & (cleared - 1);
        int sq2 = getLSBindex(remaining);

        int toFile = to % 8;
        if ((sq1 % 8) != toFile) {
            from = sq1;
        } else {
            from = sq2;
        }
        return true;
    }

    // Castling: two squares cleared, two squares set
    if (clearedCount == 2 && setCount == 2) {
        int sq1 = getLSBindex(cleared);
        Bitboard remaining = cleared & (cleared - 1);
        int sq2 = getLSBindex(remaining);

        int dst1 = getLSBindex(set);
        Bitboard remainingSet = set & (set - 1);
        int dst2 = getLSBindex(remainingSet);

        int sq1File = sq1 % 8;
        int sq2File = sq2 % 8;
        int dst1File = dst1 % 8;
        int dst2File = dst2 % 8;

        if (abs(sq1File - dst1File) == 2 && (sq1 / 8) == (dst1 / 8)) {
            from = sq1; to = dst1;
        } else if (abs(sq1File - dst2File) == 2 && (sq1 / 8) == (dst2 / 8)) {
            from = sq1; to = dst2;
        } else if (abs(sq2File - dst1File) == 2 && (sq2 / 8) == (dst1 / 8)) {
            from = sq2; to = dst1;
        } else if (abs(sq2File - dst2File) == 2 && (sq2 / 8) == (dst2 / 8)) {
            from = sq2; to = dst2;
        } else {
            return false;
        }
        return true;
    }

    // Unrecognized change pattern
    return false;
}

void BoardScanner::squaresToMoveString(int from, int to, char* moveStr, char promotion) {
    // Engine encoding: square = row * 8 + col
    // row 0 = rank 8, col 0 = file a
    moveStr[0] = 'a' + (from % 8);   // source file
    moveStr[1] = '8' - (from / 8);   // source rank
    moveStr[2] = 'a' + (to % 8);     // dest file
    moveStr[3] = '8' - (to / 8);     // dest rank
    if (promotion != '\0') {
        moveStr[4] = promotion;
        moveStr[5] = '\0';
    } else {
        moveStr[4] = '\0';
    }
}

/************ Diagnostics ************/

void BoardScanner::printScan(Bitboard scan) {
    printf("\n  Sensor Scan:\n");
    printf("    a b c d e f g h\n");

    for (int row = 0; row < 8; row++) {
        printf("  %d ", 8 - row);
        for (int col = 0; col < 8; col++) {
            int square = row * 8 + col;
            if (scan & (1ULL << square)) {
                printf("■ ");
            } else {
                printf("· ");
            }
        }
        printf("%d\n", 8 - row);
    }

    printf("    a b c d e f g h\n");
    printf("  Pieces detected: %d\n\n", countBits(scan));
}

void BoardScanner::printStorageScan(uint16_t storage, bool isWhiteSide) {
    printf("\n  %s Storage Zone:\n", isWhiteSide ? "White" : "Black");
    printf("    Pieces (slots 0-7):  ");
    for (int i = 0; i < 8; i++) {
        printf("%c ", (storage & (1 << i)) ? '#' : '.');
    }
    printf("\n    Pawns  (slots 8-15): ");
    for (int i = 8; i < 16; i++) {
        printf("%c ", (storage & (1 << i)) ? '#' : '.');
    }
    printf("\n\n");
}

void BoardScanner::diagnosticScan() {
    printf("\n[SCANNER] Diagnostic scan — reading all 96 sensors individually\n\n");

    if (!initialized) {
        printf("  Error: scanner not initialized\n");
        return;
    }

    // Board sensors (muxes 1-4)
    for (int i = 0; i < BOARD_MUX_COUNT; i++) {
        const int mux = BOARD_MUX_INDICES[i];
        const char firstFile = 'a' + (i * 2);
        const char secondFile = firstFile + 1;
        printf("  MUX %d (files %c-%c):\n", mux, firstFile, secondFile);

        for (int channel = 0; channel < CHANNELS_PER_MUX; channel++) {
            int square = toSquareIndex(mux, channel);
            if (square < 0) continue;
            int row = square / 8;
            int col = square % 8;

            setMuxChannel(channel);

            // Small extra delay for diagnostic clarity
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            bool present = readMuxOutput(mux);

            printf("    ch%2d → %c%d : %s\n",
                   channel,
                   'a' + col,
                   8 - row,
                   present ? "PIECE" : "empty");
        }
        printf("\n");
    }

    // Storage sensors (muxes 0 and 5)
    const int storageMuxes[STORAGE_MUX_COUNT] = { BLACK_STORAGE_MUX, WHITE_STORAGE_MUX };
    const char* sideNames[STORAGE_MUX_COUNT] = { "Black storage", "White storage" };
    for (int i = 0; i < STORAGE_MUX_COUNT; i++) {
        const int mux = storageMuxes[i];
        printf("  MUX %d (%s):\n", mux, sideNames[i]);

        for (int channel = 0; channel < CHANNELS_PER_MUX; channel++) {
            setMuxChannel(channel);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            bool present = readMuxOutput(mux);

            const char* slotType = (channel < 8) ? "piece" : "pawn";
            printf("    ch%2d → slot %2d (%s): %s\n",
                   channel, channel, slotType,
                   present ? "PIECE" : "empty");
        }
        printf("\n");
    }

    // Full board view
    Bitboard fullScan = scanBoard();
    printScan(fullScan);

    // Storage views
    uint16_t blackStorage = scanStorage(false);
    uint16_t whiteStorage = scanStorage(true);
    printStorageScan(blackStorage, false);
    printStorageScan(whiteStorage, true);
}
