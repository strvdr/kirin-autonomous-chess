/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    bitboard.h - Bit manipulation and board representation
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

#ifndef KIRIN_BITBOARD_H
#define KIRIN_BITBOARD_H

#include "types.h"

/************ Bit Manipulation Functions ************/
static inline void setBit(U64& bitboard, int square) { 
  bitboard |= (1ULL << square);
}

static inline U64 getBit(U64 bitboard, int square) { 
  return bitboard & (1ULL << square);
}

static inline void popBit(U64& bitboard, int square) { 
  bitboard &= ~(1ULL << square);
}


/************ File Constants ************/
extern const U64 notAfile;
extern const U64 notHfile;
extern const U64 notHGfile;
extern const U64 notABfile;

/************ Bit Counting Functions ************/
// Count bits within bitboard
static inline int countBits(U64 bitboard) {
    int count = 0;
    while (bitboard) {
        count++;
        bitboard &= bitboard - 1;
    }
    return count;
}

// Get least significant first bit index 
static inline int getLSBindex(U64 bitboard) { 
    if (bitboard) {
        return countBits((bitboard & -bitboard) - 1);
    }
    return -1;
}

/************ Board State Variables ************/
// Piece bitboards [12 pieces]
extern U64 bitboards[12]; 

// Occupancy bitboards [white, black, both]
extern U64 occupancy[3];

// Side to move
extern int side;

// En passant square
extern int enpassant;

// Castling rights
extern int castle;

// Position hash key
extern U64 hashKey;

// Repetition detection
extern U64 repetitionTable[1000]; 
extern int repetitionIndex;

// Half move counter (ply)
extern int ply;

/************ Debug Functions ************/
void printBitboard(U64 bitboard);

#endif // KIRIN_BITBOARD_H
