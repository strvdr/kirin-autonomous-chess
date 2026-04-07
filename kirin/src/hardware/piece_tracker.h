/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    piece_tracker.h - Piece identity tracking for storage-based capture disambiguation
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
*    The chess engine doesn't track individual piece identity — it only
*    knows that "there's a black knight on d5," not "the b8-knight is on
*    d5." But the physical board has labeled storage slots where each
*    captured piece has a designated home (SLOT_KNIGHT_B, SLOT_PAWN_A,
*    etc.). To disambiguate captures using storage slot identity, we need
*    to track which original piece is on which square.
*
*    This module maintains a 64-entry map: for each board square, the
*    StartingSlot of the piece currently sitting there (or SLOT_NONE if
*    empty). It's initialized from the starting position and updated on
*    every move.
*
*    Example:
*      - Game starts: square b8 (index 1) → SLOT_KNIGHT_B
*      - Black plays Nf6: square b8 → SLOT_NONE, square f6 → SLOT_KNIGHT_B
*      - White captures Nxf6: square f6 → SLOT_NONE, piece goes to
*        SLOT_KNIGHT_B in black's storage zone
*
*    When the human captures a piece and places it in a storage slot,
*    the scanner sees which slot gained a piece. The PieceTracker tells
*    us which square that piece is currently on, so we can match it to
*    the correct capture move.
*/

#ifndef KIRIN_PIECE_TRACKER_H
#define KIRIN_PIECE_TRACKER_H

#include "gantry_controller.h"  // For StartingSlot enum
#include <cstdint>
#include <cstdio>

using Gantry::StartingSlot;

/************ Constants ************/

// Sentinel value: no piece on this square (or piece has been captured)
constexpr int SLOT_NONE = -1;

// Sentinel value: piece identity unknown (e.g., after FEN setup mid-game)
constexpr int SLOT_UNKNOWN = -2;

/************ Piece Tracker ************/

class PieceTracker {
private:
    // For each board square (0-63), which StartingSlot piece is there?
    // SLOT_NONE if empty, SLOT_UNKNOWN if identity can't be determined.
    int squareToSlot[64];

    // Reverse map: for each (side, StartingSlot), which square is the piece on?
    // -1 if the piece has been captured.
    // Index: [side][slot] where side 0 = white, side 1 = black
    int slotToSquare[2][16];

public:
    PieceTracker() {
        clear();
    }

    /**
     * Clear all tracking (all squares empty, all pieces unplaced).
     */
    void clear() {
        for (int i = 0; i < 64; i++) squareToSlot[i] = SLOT_NONE;
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < 16; i++) slotToSquare[s][i] = -1;
    }

    /**
     * Initialize for the standard starting position.
     * White pieces on ranks 1-2 (rows 6-7, squares 48-63).
     * Black pieces on ranks 7-8 (rows 0-1, squares 0-15).
     */
    void initStartingPosition() {
        clear();

        // Black back rank: a8=0, b8=1, ..., h8=7
        // Maps to SLOT_ROOK_A(0) .. SLOT_ROOK_H(7)
        for (int col = 0; col < 8; col++) {
            int square = col;  // row 0, col
            int slot = col;    // SLOT_ROOK_A=0 .. SLOT_ROOK_H=7
            squareToSlot[square] = slot;
            slotToSquare[1][slot] = square;  // side 1 = black
        }

        // Black pawns: a7=8, b7=9, ..., h7=15
        // Maps to SLOT_PAWN_A(8) .. SLOT_PAWN_H(15)
        for (int col = 0; col < 8; col++) {
            int square = 8 + col;    // row 1, col
            int slot = 8 + col;      // SLOT_PAWN_A=8 .. SLOT_PAWN_H=15
            squareToSlot[square] = slot;
            slotToSquare[1][slot] = square;
        }

        // White pawns: a2=48, b2=49, ..., h2=55
        // Maps to SLOT_PAWN_A(8) .. SLOT_PAWN_H(15)
        for (int col = 0; col < 8; col++) {
            int square = 48 + col;   // row 6, col
            int slot = 8 + col;      // SLOT_PAWN_A=8 .. SLOT_PAWN_H=15
            squareToSlot[square] = slot;
            slotToSquare[0][slot] = square;  // side 0 = white
        }

        // White back rank: a1=56, b1=57, ..., h1=63
        // Maps to SLOT_ROOK_A(0) .. SLOT_ROOK_H(7)
        for (int col = 0; col < 8; col++) {
            int square = 56 + col;   // row 7, col
            int slot = col;          // SLOT_ROOK_A=0 .. SLOT_ROOK_H=7
            squareToSlot[square] = slot;
            slotToSquare[0][slot] = square;
        }
    }

    /**
     * Update tracking after a move.
     *
     * @param from     Source square (0-63)
     * @param to       Target square (0-63)
     * @param isWhite  True if the moving piece is white
     * @param isCapture True if this move captures a piece
     * @param capturedSquare  The square where the captured piece was
     *                        (usually == to, except for en passant)
     * @param capturedIsWhite True if the captured piece is white
     */
    void applyMove(int from, int to, bool isWhite, bool isCapture,
                   int capturedSquare = -1, bool capturedIsWhite = false) {
        // Handle capture: remove the captured piece from tracking
        if (isCapture && capturedSquare >= 0) {
            int capturedSlot = squareToSlot[capturedSquare];
            if (capturedSlot >= 0) {
                int capturedSide = capturedIsWhite ? 0 : 1;
                slotToSquare[capturedSide][capturedSlot] = -1;  // piece captured
            }
            squareToSlot[capturedSquare] = SLOT_NONE;
        }

        // Move the piece: transfer identity from source to destination
        int movingSlot = squareToSlot[from];
        squareToSlot[from] = SLOT_NONE;

        // For captures where capturedSquare == to, we already cleared it above.
        // For en passant, capturedSquare != to, so 'to' might still have SLOT_NONE
        // (en passant target square was empty before the move).
        squareToSlot[to] = movingSlot;

        // Update reverse map
        if (movingSlot >= 0) {
            int movingSide = isWhite ? 0 : 1;
            slotToSquare[movingSide][movingSlot] = to;
        }
    }

    /**
     * Update tracking for castling rook movement.
     * Call this in addition to applyMove for the king.
     *
     * @param rookFrom  Rook's source square
     * @param rookTo    Rook's destination square
     * @param isWhite   True if white is castling
     */
    void applyRookCastle(int rookFrom, int rookTo, bool isWhite) {
        int slot = squareToSlot[rookFrom];
        squareToSlot[rookFrom] = SLOT_NONE;
        squareToSlot[rookTo] = slot;

        if (slot >= 0) {
            int side = isWhite ? 0 : 1;
            slotToSquare[side][slot] = rookTo;
        }
    }

    /**
     * Update tracking for pawn promotion.
     * The piece identity stays the same — a promoted pawn is still
     * tracked as SLOT_PAWN_x. The physical piece doesn't change
     * (the system uses the same magnet), and when captured, the
     * promoted piece goes back to its pawn slot in storage.
     *
     * No special handling needed — applyMove already preserves the
     * slot identity through the move. This method exists only for
     * documentation clarity.
     */
    // (promotion is handled naturally by applyMove — the slot follows the piece)

    /*** Queries ***/

    /**
     * Get the StartingSlot of the piece on a given square.
     * @return StartingSlot (0-15), SLOT_NONE if empty, SLOT_UNKNOWN if unknown
     */
    int getSlotAt(int square) const {
        if (square < 0 || square >= 64) return SLOT_NONE;
        return squareToSlot[square];
    }

    /**
     * Get the current square of a piece identified by its starting slot.
     * @param isWhite  True for white pieces, false for black
     * @param slot     The StartingSlot (0-15)
     * @return Square index (0-63), or -1 if the piece has been captured
     */
    int getSquareForSlot(bool isWhite, int slot) const {
        if (slot < 0 || slot >= 16) return -1;
        int side = isWhite ? 0 : 1;
        return slotToSquare[side][slot];
    }

    /**
     * Check if a piece (by starting slot) is still on the board.
     */
    bool isPieceAlive(bool isWhite, int slot) const {
        return getSquareForSlot(isWhite, slot) >= 0;
    }

    /**
     * Print the current piece identity map (for debugging).
     */
    void print() const {
        printf("\n  Piece Identity Map:\n");
        printf("    a  b  c  d  e  f  g  h\n");

        const char* slotNames[] = {
            "Ra", "Nb", "Bc", "Qd", "Ke", "Bf", "Ng", "Rh",
            "Pa", "Pb", "Pc", "Pd", "Pe", "Pf", "Pg", "Ph"
        };

        for (int row = 0; row < 8; row++) {
            printf("  %d ", 8 - row);
            for (int col = 0; col < 8; col++) {
                int sq = row * 8 + col;
                int slot = squareToSlot[sq];
                if (slot == SLOT_NONE) {
                    printf(".. ");
                } else if (slot == SLOT_UNKNOWN) {
                    printf("?? ");
                } else if (slot >= 0 && slot < 16) {
                    printf("%s ", slotNames[slot]);
                } else {
                    printf("XX ");
                }
            }
            printf("%d\n", 8 - row);
        }
        printf("    a  b  c  d  e  f  g  h\n\n");
    }
};

#endif // KIRIN_PIECE_TRACKER_H
