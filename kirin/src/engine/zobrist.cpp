/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    zobrist.cpp - Zobrist hashing implementation
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

#include "zobrist.h"
#include "bitboard.h"
#include "utils.h"

/************ Zobrist Keys ************/
U64 pieceKeys[12][64];
U64 enpassantKeys[64];
U64 castlingKeys[16];
U64 sideKey;

/************ Initialize Random Keys ************/
void initRandKeys() { 
    // Reset random state for reproducible keys
    randState = 1804289383;
    
    // Generate piece keys
    for (int piece = P; piece <= k; piece++) { 
        for (int square = 0; square < 64; square++) { 
            pieceKeys[piece][square] = getRandU64();
        }
    }
    
    // Generate en passant keys
    for (int square = 0; square < 64; square++) { 
        enpassantKeys[square] = getRandU64();
    }
    
    // Generate castling keys
    for (int key = 0; key < 16; key++) { 
        castlingKeys[key] = getRandU64();
    }
    
    // Generate side key
    sideKey = getRandU64(); 
}

/************ Generate Hash Key ************/
U64 generateHashKey() { 
    U64 finalKey = 0ULL; 
    U64 bb;
    
    // Hash pieces
    for (int piece = P; piece <= k; piece++) { 
        bb = bitboards[piece];
        while (bb) { 
            int square = getLSBindex(bb);
            finalKey ^= pieceKeys[piece][square];
            popBit(bb, square);
        }
    }
    
    // Hash en passant
    if (enpassant != no_sq) {
        finalKey ^= enpassantKeys[enpassant];
    }
    
    // Hash castling rights
    finalKey ^= castlingKeys[castle];
    
    // Hash side to move
    if (side == black) {
        finalKey ^= sideKey;
    }
    
    return finalKey;
}
