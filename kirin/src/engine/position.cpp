/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    position.cpp - Position management and FEN parsing
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
#include <cstring>
#include "position.h"
#include "bitboard.h"
#include "zobrist.h"
#include "movegen.h"
#include "utils.h"

// Node counter for perft
long nodes = 0;

/************ FEN Parsing ************/
void parseFEN(const char *fenStr) { 
    // Reset board position and state variables
    memset(bitboards, 0ULL, sizeof(bitboards));
    memset(occupancy, 0ULL, sizeof(occupancy));
    side = 0; 
    enpassant = no_sq; 
    castle = 0;
    hashKey = 0ULL;
    repetitionIndex = 0;
    memset(repetitionTable, 0ULL, sizeof(repetitionTable));
    ply = 0;
    
    const char *fen = fenStr;
    
    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file; 
            
            // Match ascii pieces within FEN string
            if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
                int piece = charPieces[(int)*fen]; 
                setBit(bitboards[piece], square);
                fen++;
            }
            
            // Match empty square numbers
            if (*fen >= '0' && *fen <= '9') {
                int offset = *fen - '0';
                
                int piece = -1;
                for (int bbPiece = P; bbPiece <= k; bbPiece++) {
                    if (getBit(bitboards[bbPiece], square)) piece = bbPiece;
                }
                
                if (piece == -1) file--;
                
                file += offset; 
                fen++;
            }
            
            if (*fen == '/') fen++;
        }
    }
    
    // Parse side to move
    fen++;
    (*fen == 'w') ? (side = white) : (side = black);
    
    // Parse castling rights
    fen += 2;
    while (*fen != ' ') {
        switch (*fen) {
            case 'K': castle |= wk; break;
            case 'Q': castle |= wq; break;
            case 'k': castle |= bk; break;
            case 'q': castle |= bq; break;
            case '-': break;
        }
        fen++;
    }
    
    // Parse en passant
    fen++;
    if (*fen != '-') {
        int file = fen[0] - 'a';
        int rank = 8 - (fen[1] - '0');
        enpassant = rank * 8 + file;
    } else {
        enpassant = no_sq;
    }
    
    // Populate occupancy bitboards
    for (int piece = P; piece <= K; piece++) {
        occupancy[white] |= bitboards[piece];
    }
    for (int piece = p; piece <= k; piece++) {
        occupancy[black] |= bitboards[piece];
    }
    occupancy[both] = occupancy[white] | occupancy[black];
    
    // Generate hash key
    hashKey = generateHashKey();
}

/************ Board Display ************/
void printBoard() {
    printf("\n");
    for (int rank = 0; rank < 8; rank++) { 
        for (int file = 0; file < 8; file++) { 
            int square = rank * 8 + file;
            if (!file) printf(" %d ", 8 - rank);
            int piece = -1;
            
            for (int bbPiece = P; bbPiece <= k; bbPiece++) {
                if (getBit(bitboards[bbPiece], square)) piece = bbPiece;
            }
            printf(" %s", (piece == -1) ? "." : unicodePieces[piece]); 
        }
        printf("\n");
    }
    printf("\n    a b c d e f g h \n\n");
    
    printf("    STM:      %s\n", (!side && (side != -1)) ? "white" : "black");
    printf("    Enpassant:   %s\n", (enpassant != no_sq) ? squareCoordinates[enpassant] : "no");
    printf("    Castling:  %c%c%c%c\n\n", 
           (castle & wk) ? 'K' : '-', 
           (castle & wq) ? 'Q' : '-', 
           (castle & bk) ? 'k' : '-',  
           (castle & bq) ? 'q' : '-'); 
    printf("    Hash Key: %llx\n", hashKey);
}

/************ Perft Testing ************/
static inline void perftDriver(int depth) { 
    if (depth == 0) {
        nodes++;
        return; 
    }
    
    moves moveList[1];
    generateMoves(moveList); 
    
    for (int i = 0; i < moveList->count; i++) { 
        BoardState state = copyBoard();
        
        if (!makeMove(moveList->moves[i], allMoves)) continue;
        
        perftDriver(depth - 1);
        restoreBoard(state);
    }
}

void perftTest(int depth) { 
    printf("\nPerformance Test\n");
    nodes = 0;
    moves moveList[1];
    generateMoves(moveList); 
    long start = getTimeMS();
    
    for (int i = 0; i < moveList->count; i++) { 
        BoardState state = copyBoard();
        
        if (!makeMove(moveList->moves[i], allMoves)) continue;
        
        long cumulativeNodes = nodes; 
        perftDriver(depth - 1);
        long oldNodes = nodes - cumulativeNodes;
        
        restoreBoard(state);
        
        int promoted = getPromoted(moveList->moves[i]);
        if (promoted) {
            printf("move: %s%s%c  nodes: %ld\n",
                   squareCoordinates[getSource(moveList->moves[i])],
                   squareCoordinates[getTarget(moveList->moves[i])],
                   promotedPieces[promoted],
                   oldNodes);
        } else {
            printf("move: %s%s  nodes: %ld\n",
                   squareCoordinates[getSource(moveList->moves[i])],
                   squareCoordinates[getTarget(moveList->moves[i])],
                   oldNodes);
        }
    }
    
    printf("\nDepth: %d", depth);
    printf("\nNodes: %ld", nodes);
    printf("\nTime: %ld", getTimeMS() - start);
}
