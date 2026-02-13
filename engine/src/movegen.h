/*
 * Kirin Chess Engine
 * movegen.h - Move generation and encoding
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

// Encode move macro 
#define encodeMove(source, target, piece, promoted, capture, dpp, enpassant, castle) \
    ((source) | ((target) << 6) | ((piece) << 12) | ((promoted) << 16) | \
    ((capture) << 20) | ((dpp) << 21) | ((enpassant) << 22) | ((castle) << 23))

// Extract components from move
#define getSource(move) ((move) & 0x3f)
#define getTarget(move) (((move) & 0xfc0) >> 6)
#define getPiece(move) (((move) & 0xf000) >> 12)
#define getPromoted(move) (((move) & 0xf0000) >> 16)
#define getCapture(move) ((move) & 0x100000)
#define getDpp(move) ((move) & 0x200000)
#define getEnpassant(move) ((move) & 0x400000)
#define getCastle(move) ((move) & 0x800000)

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
