/*
 * Kirin Chess Engine
 * bitboard.cpp - Bit manipulation and board representation
 */

#include <cstdio>
#include "bitboard.h"

/************ File Constants ************/
const U64 notAfile = 18374403900871474942ULL;
const U64 notHfile = 9187201950435737471ULL;
const U64 notHGfile = 4557430888798830399ULL;
const U64 notABfile = 18229723555195321596ULL;

/************ Board State Variables ************/
U64 bitboards[12]; 
U64 occupancy[3];
int side = 0;
int enpassant = no_sq;
int castle = 0;
U64 hashKey = 0ULL;
U64 repetitionTable[1000]; 
int repetitionIndex = 0;
int ply = 0;

/************ Debug Functions ************/
void printBitboard(U64 bitboard) {
    printf("\n");
    
    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            if (!file) printf("  %d ", 8 - rank);
            printf(" %d", getBit(bitboard, square) ? 1 : 0);
        }
        printf("\n");
    }
    printf("\n     a b c d e f g h\n\n");
    printf("     Bitboard: %llud\n\n", bitboard);
}
