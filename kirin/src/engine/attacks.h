/*
 * Kirin Chess Engine
 * attacks.h - Attack tables and magic bitboard lookups
 */

#ifndef KIRIN_ATTACKS_H
#define KIRIN_ATTACKS_H

#include "types.h"

/************ Attack Tables ************/
// Pawn attacks [side][square]
extern U64 pawnAttacks[2][64];

// Knight attacks [square]
extern U64 knightAttacks[64];

// King attacks [square]
extern U64 kingAttacks[64];

// Bishop masks [square]
extern U64 bishopMasks[64];

// Rook masks [square]
extern U64 rookMasks[64];

// Bishop attacks [square][magic_index]
extern U64 bishopAttacks[64][512];

// Rook attacks [square][magic_index]
extern U64 rookAttacks[64][4096];

/************ Magic Numbers ************/
extern U64 rookMagicNums[64];
extern U64 bishopMagicNums[64];

/************ Relevant Bits ************/
extern const int bishopRelevantBits[64];
extern const int rookRelevantBits[64];

/************ Initialization Functions ************/
void initLeaperAttacks();
void initSliderAttacks(int bishop);
void initMagicNum();

/************ Attack Mask Functions ************/
U64 maskPawnAttacks(int side, int square);
U64 maskKnightAttacks(int square);
U64 maskKingAttacks(int square);
U64 maskBishopAttacks(int square);
U64 maskRookAttacks(int square);

/************ On-the-fly Attack Functions ************/
U64 bishopAttacksOtf(int square, U64 block);
U64 rookAttacksOtf(int square, U64 block);

/************ Occupancy Set Functions ************/
U64 setOccupancy(int index, int bitsInMask, U64 attackMask);

/************ Magic Number Generation ************/
U64 findMagicNum(int square, int relevantBits, int bishop);

/************ Attack Lookup Functions ************/
static inline U64 getBishopAttacks(int square, U64 occ) { 
    occ &= bishopMasks[square];
    occ *= bishopMagicNums[square];
    occ >>= 64 - bishopRelevantBits[square];
    return bishopAttacks[square][occ];
}

static inline U64 getRookAttacks(int square, U64 occ) { 
    occ &= rookMasks[square];
    occ *= rookMagicNums[square];
    occ >>= 64 - rookRelevantBits[square];
    return rookAttacks[square][occ];
}

static inline U64 getQueenAttacks(int square, U64 occ) {
    U64 result = 0ULL;
    U64 bishopOcc = occ;
    U64 rookOcc = occ;
    
    bishopOcc &= bishopMasks[square];
    bishopOcc *= bishopMagicNums[square];
    bishopOcc >>= 64 - bishopRelevantBits[square];
    result = bishopAttacks[square][bishopOcc];
    
    rookOcc &= rookMasks[square];
    rookOcc *= rookMagicNums[square];
    rookOcc >>= 64 - rookRelevantBits[square];
    result |= rookAttacks[square][rookOcc];
    
    return result;
}

/************ Square Attack Check ************/
int isAttacked(int square, int side);

/************ Debug Functions ************/
void printAttacked(int side);

#endif // KIRIN_ATTACKS_H
