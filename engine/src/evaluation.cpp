/*
 * Kirin Chess Engine
 * evaluation.cpp - Position evaluation
 */

#include "evaluation.h"
#include "bitboard.h"

/************ Material Scores ************/
int materialScore[12] = {
    100,    // white pawn
    300,    // white knight
    350,    // white bishop 
    500,    // white rook
    1000,   // white queen
    10000,  // white king 
    -100,   // black pawn
    -300,   // black knight
    -350,   // black bishop 
    -500,   // black rook
    -1000,  // black queen
    -10000  // black king
};

/************ Piece-Square Tables ************/
const int pawnScore[64] = {
    90,  90,  90,  90,  90,  90,  90,  90,
    30,  30,  30,  40,  40,  30,  30,  30,
    20,  20,  20,  30,  30,  30,  20,  20,
    10,  10,  10,  20,  20,  10,  10,  10,
     5,   5,  10,  20,  20,   5,   5,   5,
     0,   0,   0,   5,   5,   0,   0,   0,
     0,   0,   0, -10, -10,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0
};

const int knightScore[64] = {
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,  10,  10,   0,   0,  -5,
    -5,   5,  20,  20,  20,  20,   5,  -5,
    -5,  10,  20,  30,  30,  20,  10,  -5,
    -5,  10,  20,  30,  30,  20,  10,  -5,
    -5,   5,  20,  10,  10,  20,   5,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5, -10,   0,   0,   0,   0, -10,  -5
};

const int bishopScore[64] = {
     0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,  10,  10,   0,   0,   0,
     0,   0,  10,  20,  20,  10,   0,   0,
     0,   0,  10,  20,  20,  10,   0,   0,
     0,  10,   0,   0,   0,   0,  10,   0,
     0,  30,   0,   0,   0,   0,  30,   0,
     0,   0, -10,   0,   0, -10,   0,   0
};

const int rookScore[64] = {
    50,  50,  50,  50,  50,  50,  50,  50,
    50,  50,  50,  50,  50,  50,  50,  50,
     0,   0,  10,  20,  20,  10,   0,   0,
     0,   0,  10,  20,  20,  10,   0,   0,
     0,   0,  10,  20,  20,  10,   0,   0,
     0,   0,  10,  20,  20,  10,   0,   0,
     0,   0,  10,  20,  20,  10,   0,   0,
     0,   0,   0,  20,  20,   0,   0,   0
};

const int kingScore[64] = {
     0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   5,   5,   5,   5,   0,   0,
     0,   5,   5,  10,  10,   5,   5,   0,
     0,   5,  10,  20,  20,  10,   5,   0,
     0,   5,  10,  20,  20,  10,   5,   0,
     0,   0,   5,  10,  10,   5,   0,   0,
     0,   5,   5,  -5,  -5,   0,   5,   0,
     0,   0,   5,   0, -15,   0,  10,   0
};

const int mirrorScore[128] = {
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8
};

/************ Pawn Structure Constants ************/
const int getRank[64] = {
    7, 7, 7, 7, 7, 7, 7, 7,
    6, 6, 6, 6, 6, 6, 6, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int doublePawnPenalty = -10;
const int isolatedPawnPenalty = -10;
const int passedPawnBonus[8] = { 0, 10, 30, 50, 75, 100, 150, 200 };

/************ Evaluation Masks ************/
U64 fileMasks[64];
U64 rankMasks[64];
U64 isolatedMasks[64];
U64 passedWhiteMasks[64];
U64 passedBlackMasks[64];

/************ Helper Function ************/
static U64 setFileRankMask(int fileNumber, int rankNumber) { 
    U64 mask = 0ULL;
    
    for (int rank = 0; rank < 8; rank++) { 
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            if (fileNumber != -1) {
                if (file == fileNumber) mask |= setBit(mask, square);
            } else if (rankNumber != -1) {
                if (rank == rankNumber) mask |= setBit(mask, square);
            }
        }
    }
    
    return mask;
}

/************ Initialize Evaluation Masks ************/
void initEvaluationMasks() { 
    // File masks
    for (int rank = 0; rank < 8; rank++) { 
        for (int file = 0; file < 8; file++) { 
            int square = rank * 8 + file;
            fileMasks[square] = setFileRankMask(file, -1);
        }
    }
    
    // Rank masks
    for (int rank = 0; rank < 8; rank++) { 
        for (int file = 0; file < 8; file++) { 
            int square = rank * 8 + file;
            rankMasks[square] = setFileRankMask(-1, rank);
        }
    }
    
    // Isolated pawn masks
    for (int rank = 0; rank < 8; rank++) { 
        for (int file = 0; file < 8; file++) { 
            int square = rank * 8 + file;
            isolatedMasks[square] |= setFileRankMask(file - 1, -1);
            isolatedMasks[square] |= setFileRankMask(file + 1, -1);
        }
    }
    
    // White passed pawn masks
    for (int rank = 0; rank < 8; rank++) { 
        for (int file = 0; file < 8; file++) { 
            int square = rank * 8 + file;
            passedWhiteMasks[square] |= setFileRankMask(file - 1, -1);
            passedWhiteMasks[square] |= setFileRankMask(file, -1);
            passedWhiteMasks[square] |= setFileRankMask(file + 1, -1);
            
            for (int i = 0; i < (7 - rank); i++) {
                passedWhiteMasks[square] &= ~rankMasks[(7 - i) * 8 + file];
            }
        }
    }
    
    // Black passed pawn masks
    for (int rank = 0; rank < 8; rank++) { 
        for (int file = 0; file < 8; file++) { 
            int square = rank * 8 + file;
            passedBlackMasks[square] |= setFileRankMask(file - 1, -1);
            passedBlackMasks[square] |= setFileRankMask(file, -1);
            passedBlackMasks[square] |= setFileRankMask(file + 1, -1);
            
            for (int i = 0; i < rank + 1; i++) {
                passedBlackMasks[square] &= ~rankMasks[i * 8 + file];
            }
        }
    }
}

/************ Evaluation Function ************/
int evaluate() { 
    int score = 0; 
    U64 bb;
    int piece, square; 
    int doublePawns;
    
    for (int bbPiece = P; bbPiece <= k; bbPiece++) { 
        bb = bitboards[bbPiece];
        while (bb) { 
            piece = bbPiece;
            square = getLSBindex(bb);
            
            // Score material
            score += materialScore[piece];
            
            switch (piece) {
                // Evaluate white pieces
                case P: 
                    score += pawnScore[square]; 
                    doublePawns = countBits(bitboards[P] & fileMasks[square]);
                    if (doublePawns > 1) score += doublePawns * doublePawnPenalty;
                    if ((bitboards[P] & isolatedMasks[square]) == 0) score += isolatedPawnPenalty;
                    if ((passedWhiteMasks[square] & bitboards[p]) == 0) score += passedPawnBonus[getRank[square]];
                    break;
                case N:
                    score += knightScore[square];
                    break;
                case B:
                    score += bishopScore[square];
                    break;
                case R:
                    score += rookScore[square];
                    break;
                case K:
                    score += kingScore[square];
                    break; 
                    
                // Evaluate black pieces
                case p:  
                    score -= pawnScore[mirrorScore[square]]; 
                    doublePawns = countBits(bitboards[p] & fileMasks[square]);
                    if (doublePawns > 1) score -= doublePawns * doublePawnPenalty;
                    if ((bitboards[p] & isolatedMasks[square]) == 0) score -= isolatedPawnPenalty;
                    if ((passedBlackMasks[square] & bitboards[P]) == 0) score -= passedPawnBonus[getRank[mirrorScore[square]]];
                    break;
                case n:
                    score -= knightScore[mirrorScore[square]];
                    break;
                case b:
                    score -= bishopScore[mirrorScore[square]];
                    break;
                case r:
                    score -= rookScore[mirrorScore[square]];
                    break;
                case k:
                    score -= kingScore[mirrorScore[square]];
                    break; 
            }
            popBit(bb, square);
        }
    }
    
    // Return score relative to side to move
    return (side == white) ? score : -score;
}
