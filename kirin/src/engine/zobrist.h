/*
 * Kirin Chess Engine
 * zobrist.h - Zobrist hashing for position identification
 */

#ifndef KIRIN_ZOBRIST_H
#define KIRIN_ZOBRIST_H

#include "types.h"

/************ Zobrist Keys ************/
// Random piece keys [piece][square]
extern U64 pieceKeys[12][64];

// Random en passant keys [square]
extern U64 enpassantKeys[64];

// Random castling keys [16 possible states]
extern U64 castlingKeys[16];

// Random side key
extern U64 sideKey;

/************ Zobrist Functions ************/
// Initialize random keys
void initRandKeys();

// Generate hash key from current position
U64 generateHashKey();

#endif // KIRIN_ZOBRIST_H
