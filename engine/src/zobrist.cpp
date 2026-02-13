/*
 * Kirin Chess Engine
 * zobrist.cpp - Zobrist hashing implementation
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
