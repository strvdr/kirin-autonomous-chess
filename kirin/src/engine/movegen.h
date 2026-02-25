/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    movegen.h - Move generation and encoding
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

#ifndef KIRIN_MOVEGEN_H
#define KIRIN_MOVEGEN_H

#include "types.h"

/************ Move Encoding ************/
/*
 * Binary move representation:
 * 0000 0000 0000 0000 0011 1111 source square               0x3f
 * 0000 0000 0000 1111 1100 0000 target square               0xfc0
 * 0000 0000 1111 0000 0000 0000 piece                       0xf000
 * 0000 1111 0000 0000 0000 0000 promoted piece              0xf0000
 * 0001 0000 0000 0000 0000 0000 capture flag                0x100000
 * 0010 0000 0000 0000 0000 0000 double pawn push flag       0x200000
 * 0100 0000 0000 0000 0000 0000 enpassant capture flag      0x400000
 * 1000 0000 0000 0000 0000 0000 castle flag                 0x800000
 */

// Encode move functions
static inline int encodeMove(int source, int target, int piece, int promoted, int capture, int dpp, int enpassant, int castle) {
  return source | (target << 6) | (piece << 12) | (promoted << 16) | \
    (capture << 20) | (dpp << 21) | (enpassant << 22) | (castle << 23);

}

static inline int encodeMoveWithCapture(int source, int target, int piece, int promoted, int capture, int dpp, int enpassant, int castle, int capturedPiece) {
  return source | (target << 6) | (piece << 12) | (promoted << 16) | \
    (capture << 20) | (dpp << 21) | (enpassant << 22) | (castle << 23) | \
    (capturedPiece << 24);
}

// Extract components from move
inline int getSource(int move) { return move & 0x3f; }
inline int getTarget(int move) { return (move & 0xfc0) >> 6; }
inline int getPiece(int move) { return (move & 0xf000) >> 12; }
inline int getPromoted(int move) { return (move & 0xf0000) >> 16; }
inline int getCapture(int move) { return move & 0x100000; }
inline int getDpp(int move) { return move & 0x200000; }
inline int getEnpassant(int move) { return move & 0x400000; }
inline int getCastle(int move) { return move & 0x800000; }
inline int getCapturedPiece(int move) { return (move >> 24) & 0xf; }

/************ Move List Functions ************/
void addMove(moves *moveList, int move);

/************ Move Generation ************/
void generateMoves(moves *moveList);

/************ Make Move ************/
int makeMove(int move, int moveFlag);

/************ Board State Macros ************/
// Save board state before making a move
#define copyBoard()                                    \
    U64 bitboardsCopy[12], occupancyCopy[3];          \
    int sideCopy, enpassantCopy, castleCopy;          \
    memcpy(bitboardsCopy, bitboards, 96);              \
    memcpy(occupancyCopy, occupancy, 24);              \
    sideCopy = side;                                   \
    enpassantCopy = enpassant;                         \
    castleCopy = castle;                               \
    U64 hashKeyCopy = hashKey;

// Restore board state after unmaking a move
#define restoreBoard()                                 \
    memcpy(bitboards, bitboardsCopy, 96);              \
    memcpy(occupancy, occupancyCopy, 24);              \
    side = sideCopy;                                   \
    enpassant = enpassantCopy;                         \
    castle = castleCopy;                               \
    hashKey = hashKeyCopy;

/************ Debug Functions ************/
void printMove(int move);
void printMoveList(moves *moveList);

#endif // KIRIN_MOVEGEN_H
