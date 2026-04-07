/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    board_scanner.h - Hall effect sensor board scanning via multiplexed GPIO
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
*    This module reads 96 A3144 hall effect sensors through 6x 16:1
*    multiplexers to detect piece presence on the physical chess board
*    and in the two capture/storage zones flanking it.
*
*    Hardware Layout:
*    ───────────────
*    6x CD74HC4067 (or equivalent) 16:1 digital mux:
*
*      Board sensors (64 total, one per square):
*        MUX 0: ranks 8-7  (rows 0-1, squares a8-h7)  → 16 sensors
*        MUX 1: ranks 6-5  (rows 2-3, squares a6-h5)  → 16 sensors
*        MUX 2: ranks 4-3  (rows 4-5, squares a4-h3)  → 16 sensors
*        MUX 3: ranks 2-1  (rows 6-7, squares a2-h1)  → 16 sensors
*
*      Storage zone sensors (32 total, 16 per side):
*        MUX 4: black storage (left side of board)     → 16 sensors
*        MUX 5: white storage (right side of board)    → 16 sensors
*
*      Each storage mux has 16 channels mapped to slots:
*        Channels 0-7:  back-rank piece slots (col 1, inner)
*        Channels 8-15: pawn slots (col 2, outer)
*
*    Wiring:
*    ───────
*    4 shared select lines (S0-S3)   → Pi GPIO, drive all 6 muxes
*    6 mux output lines (SIG)        → Pi GPIO, one per mux
*
*    Sensor mapping within each board mux (channel → square):
*      Channel 0  = rank N, file a   (col 0)
*      Channel 1  = rank N, file b   (col 1)
*      ...
*      Channel 7  = rank N, file h   (col 7)
*      Channel 8  = rank N-1, file a (col 0)
*      ...
*      Channel 15 = rank N-1, file h (col 7)
*
*    A3144 sensor output:
*      LOW  = magnet detected (piece present)
*      HIGH = no magnet (square empty)
*      (active-low with 10K pull-up resistor per sensor)
*/

#ifndef KIRIN_BOARD_SCANNER_H
#define KIRIN_BOARD_SCANNER_H

#include "board_interpreter.h"
#include "piece_tracker.h"
#include <cstdint>

/************ GPIO Pin Configuration ************/
// Default GPIO pin assignments (BCM numbering)
// These can be overridden at construction time.
//
// Select lines (active on all 6 muxes simultaneously):
constexpr int DEFAULT_PIN_S0 = 17;
constexpr int DEFAULT_PIN_S1 = 27;
constexpr int DEFAULT_PIN_S2 = 22;
constexpr int DEFAULT_PIN_S3 = 23;

// Mux signal output pins (active-low: LOW = piece present):
constexpr int DEFAULT_PIN_MUX0 = 5;   // Ranks 8-7
constexpr int DEFAULT_PIN_MUX1 = 6;   // Ranks 6-5
constexpr int DEFAULT_PIN_MUX2 = 13;  // Ranks 4-3
constexpr int DEFAULT_PIN_MUX3 = 19;  // Ranks 2-1
constexpr int DEFAULT_PIN_MUX4 = 26;  // Black storage zone
constexpr int DEFAULT_PIN_MUX5 = 16;  // White storage zone

/************ Scanning Parameters ************/
constexpr int BOARD_MUX_COUNT  = 4;
constexpr int STORAGE_MUX_COUNT = 2;
constexpr int TOTAL_MUX_COUNT  = 6;
constexpr int CHANNELS_PER_MUX = 16;
constexpr int TOTAL_SQUARES    = 64;
constexpr int STORAGE_SLOTS_PER_SIDE = 16;

// Settling time after changing mux address (microseconds)
constexpr int MUX_SETTLE_US    = 10;

// Debounce: board must be stable for this many consecutive scans
constexpr int DEBOUNCE_SCANS   = 3;

// Delay between debounce scans (milliseconds)
constexpr int DEBOUNCE_DELAY_MS = 50;

// Poll interval during move wait (milliseconds)
constexpr int POLL_INTERVAL_MS = 20;

// How long the board must be in an unrecognized state before we
// alert the player that something looks wrong (milliseconds).
// This is NOT a commit timeout — we never auto-commit a move.
constexpr int ILLEGAL_STATE_ALERT_MS = 10000;

/************ Move Detection Result ************/

struct ScannerMoveResult {
    int engineMove;      // The matched engine move (0 if none)
    bool success;        // True if a legal move was matched
    bool timeout;        // True if the wait timed out
    bool illegalAlert;   // True if we alerted about an unrecognized state
    bool wrongSlot;      // True if a piece was placed in the wrong storage slot
    char uciString[6];   // UCI string representation (e.g. "e2e4\0")

    ScannerMoveResult()
        : engineMove(0), success(false), timeout(false), illegalAlert(false),
          wrongSlot(false) {
        uciString[0] = '\0';
    }
};

/************ Board Scanner ************/

class BoardScanner {
    // Allow test subclass to access private matchLegalMove
    friend class TestableScanner;

private:
    // GPIO pin assignments
    int selectPins[4];          // S0-S3
    int muxPins[TOTAL_MUX_COUNT];  // MUX 0-5 signal outputs

    bool initialized;
    bool simulationMode;

    // Simulation state
    Bitboard simulatedOccupancy;
    uint16_t simulatedBlackStorage;
    uint16_t simulatedWhiteStorage;

    // Previous stable scan (for change detection)
    Bitboard lastStableScan;

    // Storage zone baselines (set at game start / after each engine move)
    uint16_t lastBlackStorage;
    uint16_t lastWhiteStorage;

    /**
     * Set the mux address (0-15) on the shared select lines
     */
    void setMuxChannel(int channel);

    /**
     * Read a single mux output pin
     * @return true if piece is present (sensor active-low, so LOW = present)
     */
    bool readMuxOutput(int muxIndex);

    /**
     * Map a (mux index, channel) pair to a bitboard square index.
     *
     * Mux 0, channel 0  → square 0  (a8)
     * Mux 0, channel 15 → square 15 (h7)
     * Mux 3, channel 15 → square 63 (h1)
     */
    int toSquareIndex(int muxIndex, int channel);

    /**
     * Try to match an observed board + storage state against the engine's
     * legal moves.
     *
     * Capture disambiguation uses storage slot identity:
     *   - When a storage slot gains a piece, the slot tells us which
     *     specific piece was captured (by its starting position).
     *   - The PieceTracker tells us where that piece currently sits.
     *   - We only match captures whose target is that square.
     *
     * For non-capture moves, no storage slot should have changed.
     *
     * @param before          Board occupancy before the move
     * @param after           Board occupancy after the move
     * @param captureSlot     The specific storage slot that gained a piece,
     *                        or SLOT_NONE if no storage change detected
     * @param capturedIsWhite True if the captured piece is white (i.e., it
     *                        appeared in white's storage zone)
     * @param tracker         PieceTracker for mapping slot → current square
     * @param[out] move       The matched engine move (if exactly one matches)
     * @return true if exactly one legal move matches the observed change
     */
    bool matchLegalMove(Bitboard before, Bitboard after,
                        int captureSlot, bool capturedIsWhite,
                        const PieceTracker& tracker, int& move);

public:
    /**
     * Construct with default GPIO pin assignments
     */
    BoardScanner();

    /**
     * Construct with custom GPIO pin assignments
     * @param sPins  Array of 4 select-line GPIO pins (BCM)
     * @param mPins  Array of 6 mux-output GPIO pins (BCM)
     */
    BoardScanner(const int sPins[4], const int mPins[TOTAL_MUX_COUNT]);

    ~BoardScanner();

    /*** Initialization ***/

    /**
     * Initialize GPIO and configure pins.
     * Must be called before scanning.
     * @return true if GPIO setup succeeded
     */
    bool init();

    /**
     * Enable simulation mode: no GPIO access, scanBoard() returns
     * the value set by setSimulatedOccupancy(). Use this for testing
     * on machines without GPIO or without sensor hardware attached.
     */
    void enableSimulation();
    bool isSimulation() const { return simulationMode; }

    /**
     * Set the simulated occupancy (only used in simulation mode)
     */
    void setSimulatedOccupancy(Bitboard occ) { simulatedOccupancy = occ; }

    /**
     * Set simulated storage zone occupancy (only in simulation mode)
     * Each uint16_t is a bitmask: bit 0-7 = piece slots, 8-15 = pawn slots
     */
    void setSimulatedStorage(uint16_t blackStorage, uint16_t whiteStorage) {
        simulatedBlackStorage = blackStorage;
        simulatedWhiteStorage = whiteStorage;
    }

    /*** Board Scanning ***/

    /**
     * Perform a single raw scan of all 64 board squares.
     * Cycles through all 16 mux channels, reading 4 board mux outputs per channel.
     *
     * @return 64-bit occupancy bitboard (bit 0 = a8, bit 63 = h1)
     */
    Bitboard scanBoard();

    /**
     * Perform a debounced board scan: repeats scanBoard() until the result
     * is stable for DEBOUNCE_SCANS consecutive reads.
     *
     * @return Stable 64-bit occupancy bitboard
     */
    Bitboard scanBoardDebounced();

    /*** Storage Zone Scanning ***/

    /**
     * Scan one side's storage zone (16 slots).
     *
     * @param isWhiteSide  true for white storage (MUX 5), false for black (MUX 4)
     * @return 16-bit bitmask of occupied slots
     */
    uint16_t scanStorage(bool isWhiteSide);

    /**
     * Scan storage with debouncing.
     */
    uint16_t scanStorageDebounced(bool isWhiteSide);

    /*** Legal Move Detection ***/

    /**
     * Wait for the human player to make a legal move on the physical board.
     *
     * This is the primary interface for human move input. It works by:
     *   1. Polling until the board OR storage state changes
     *   2. Waiting for both board and storage to stabilize (debounce)
     *   3. Diffing the new state against the baselines
     *   4. Checking if the diff matches exactly one legal engine move,
     *      using the specific storage slot to disambiguate captures:
     *        - If a storage slot gained a piece → identify which piece
     *          was captured via PieceTracker, match the capture
     *        - If storage is unchanged → this is a non-capture move
     *        - If the wrong slot was used → reject and alert the player
     *   5. If no legal move matches, going back to step 1 (the move
     *      isn't finished yet — the player is still mid-move)
     *   6. If the board sits in an unrecognized state for too long,
     *      calling illegalStateCallback so the caller can alert the player
     *
     * This approach cleanly handles:
     *   - Long pauses (piece held in the air — board unstable or unchanged,
     *     storage unchanged → keep waiting)
     *   - Captures (source square clears on board, storage slot gains a piece
     *     → unambiguous via slot identity)
     *   - Take-backs (piece lifted and put back → board returns to original,
     *     storage unchanged → keep waiting)
     *   - Castling (intermediate state doesn't match; final state with both
     *     king and rook in new positions does, storage unchanged)
     *   - En passant (board shows 2 cleared + 1 set, storage gains a piece)
     *
     * @param currentOccupancy  The board occupancy before the expected move
     * @param currentBlackStorage  Black storage zone baseline
     * @param currentWhiteStorage  White storage zone baseline
     * @param tracker           PieceTracker for mapping storage slots to pieces
     * @param timeoutMs         Maximum time to wait (0 = wait forever)
     * @param illegalStateCallback  Optional callback invoked when the board
     *                              has been in an unrecognized state for
     *                              ILLEGAL_STATE_ALERT_MS. Called once per
     *                              unrecognized-state episode. Can be nullptr.
     * @param wrongSlotCallback  Optional callback invoked when a captured
     *                           piece is placed in the wrong storage slot.
     *                           Can be nullptr.
     * @return ScannerMoveResult with the matched engine move, or failure info
     */
    ScannerMoveResult waitForLegalMove(
        Bitboard currentOccupancy,
        uint16_t currentBlackStorage,
        uint16_t currentWhiteStorage,
        const PieceTracker& tracker,
        int timeoutMs = 0,
        void (*illegalStateCallback)() = nullptr,
        void (*wrongSlotCallback)() = nullptr
    );

    /*** Legacy / Low-Level (still useful for diagnostics) ***/

    /**
     * Determine the source and destination squares from an occupancy change.
     * This is a raw diff-based inference — it does NOT validate against
     * legal moves. Prefer waitForLegalMove() for game play.
     *
     * For a normal move:    one bit clears (source), one bit sets (destination)
     * For a capture:        one bit clears (source), destination stays set
     * For castling:         two bits clear, two bits set (king + rook)
     * For en passant:       two bits clear, one bit sets
     *
     * @param before    Occupancy before the move
     * @param after     Occupancy after the move
     * @param[out] from Source square index (0-63)
     * @param[out] to   Destination square index (0-63)
     * @return true if a valid move pattern was detected
     */
    static bool inferMove(Bitboard before, Bitboard after, int& from, int& to);

    /**
     * Build a UCI move string (e.g. "e2e4") from source and destination squares.
     *
     * @param from  Source square index (0-63, engine encoding)
     * @param to    Destination square index (0-63, engine encoding)
     * @param[out] moveStr  Buffer for the move string (at least 6 chars)
     * @param promotion  Promotion character ('q','r','b','n') or '\0' for none
     */
    static void squaresToMoveString(int from, int to, char* moveStr, char promotion = '\0');

    /*** State ***/

    /**
     * Get the last stable scan result
     */
    Bitboard getLastScan() const { return lastStableScan; }

    /**
     * Update the last known stable state (call after engine processes a move)
     */
    void setLastScan(Bitboard occ) { lastStableScan = occ; }

    /**
     * Get/set storage zone baselines.
     * These track what the storage zones looked like before the human's turn
     * so we can detect when a new piece arrives (capture).
     */
    uint16_t getLastBlackStorage() const { return lastBlackStorage; }
    uint16_t getLastWhiteStorage() const { return lastWhiteStorage; }
    void setLastStorage(uint16_t blackSt, uint16_t whiteSt) {
        lastBlackStorage = blackSt;
        lastWhiteStorage = whiteSt;
    }

    /**
     * Check if the scanner has been initialized
     */
    bool isInitialized() const { return initialized; }

    /*** Diagnostics ***/

    /**
     * Print a visual representation of the current scan
     */
    void printScan(Bitboard scan);

    /**
     * Print a visual representation of a storage zone scan
     */
    void printStorageScan(uint16_t storage, bool isWhiteSide);

    /**
     * Scan and print each sensor individually (for hardware debugging).
     * Slowly cycles through every channel and prints the raw reading.
     */
    void diagnosticScan();
};

#endif // KIRIN_BOARD_SCANNER_H
