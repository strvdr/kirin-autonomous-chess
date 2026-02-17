/*
 * Kirin Chess Engine
 * bitboard.h - Bit manipulation and board representation
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
