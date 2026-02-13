/*
 * Kirin Chess Engine
 * position.cpp - Position management and FEN parsing
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
        copyBoard(); 
        
        if (!makeMove(moveList->moves[i], allMoves)) continue;
        
        perftDriver(depth - 1);
        restoreBoard();
    }
}

void perftTest(int depth) { 
    printf("\nPerformance Test\n");
    moves moveList[1];
    generateMoves(moveList); 
    long start = getTimeMS();
    
    for (int i = 0; i < moveList->count; i++) { 
        copyBoard(); 
        
        if (!makeMove(moveList->moves[i], allMoves)) continue;
        
        long cumulativeNodes = nodes; 
        perftDriver(depth - 1);
        long oldNodes = nodes - cumulativeNodes;
        
        restoreBoard();
        
        printf("move: %s%s%c  nodes: %ld\n", 
               squareCoordinates[getSource(moveList->moves[i])], 
               squareCoordinates[getTarget(moveList->moves[i])],
               promotedPieces[getPromoted(moveList->moves[i])],
               oldNodes);
    }
    
    printf("\nDepth: %d", depth);
    printf("\nNodes: %ld", nodes);
    printf("\nTime: %ld", getTimeMS() - start);
}
