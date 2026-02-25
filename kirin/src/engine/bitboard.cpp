/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    bitboard.cpp - Bit manipulation and board representation
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
