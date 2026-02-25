/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    movegen.cpp - Move generation and encoding
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
#include "movegen.h"
#include "bitboard.h"
#include "attacks.h"
#include "zobrist.h"

/************ Move List Functions ************/
void addMove(moves *moveList, int move) {
    moveList->moves[moveList->count] = move;
    moveList->count++;
}

/************ Debug Functions ************/
void printMove(int move) { 
    if (getPromoted(move)) {
        printf("%s%s%c", squareCoordinates[getSource(move)], 
                         squareCoordinates[getTarget(move)],
                         promotedPieces[getPromoted(move)]);
    } else {
        printf("%s%s", squareCoordinates[getSource(move)], 
                       squareCoordinates[getTarget(move)]);
    }
}

void printMoveList(moves *moveList) { 
    if (!moveList->count) {
        printf("    No moves in the move list!\n");
        return;
    }
    
    printf("\n    move    piece   capture   double    enpassant   castle\n\n");
    
    for (int numMoves = 0; numMoves < moveList->count; numMoves++) {
        int move = moveList->moves[numMoves];
        printf("    %s%s%c   %s       %d         %d         %d           %d\n", 
               squareCoordinates[getSource(move)], 
               squareCoordinates[getTarget(move)],
               getPromoted(move) ? promotedPieces[getPromoted(move)] : ' ',
               unicodePieces[getPiece(move)],
               getCapture(move) ? 1 : 0,
               getDpp(move) ? 1 : 0,
               getEnpassant(move) ? 1 : 0,
               getCastle(move) ? 1 : 0);
    }
    printf("\n\n    Total number of moves: %d\n\n", moveList->count);
}

/************ Get Piece On Square ************/
inline int getPieceOnSquare(int square, int attackingSide) { 
  int startPiece, endPiece;
  if(attackingSide == white) {
    //white is attacking, so capturing black pieces
    startPiece = p;
    endPiece = k;
  } else {
    startPiece = P;
    endPiece = K;
  }

  for(int bbPiece = startPiece; bbPiece <= endPiece; bbPiece++) { 
    if(getBit(bitboards[bbPiece], square)) { 
      return bbPiece;
    }
  }

  return 0;
}

/************ Make Move ************/
int makeMove(int move, int moveFlag) { 
    if (moveFlag == allMoves) {
        // Preserve board state
        copyBoard();
        
        // Parse move
        int sourceSquare = getSource(move);
        int targetSquare = getTarget(move);
        int piece = getPiece(move);
        int promoted = getPromoted(move);
        int capture = getCapture(move);
        int dpp = getDpp(move);
        int enpass = getEnpassant(move);
        int castling = getCastle(move);
        
        // Move piece
        popBit(bitboards[piece], sourceSquare);
        setBit(bitboards[piece], targetSquare);
        
        // Hash piece (remove from source, add to target)
        hashKey ^= pieceKeys[piece][sourceSquare]; 
        hashKey ^= pieceKeys[piece][targetSquare]; 
        
        // Handle captures
        if (capture) { 
            int startPiece, endPiece;
            if (side == white) { 
                startPiece = p; 
                endPiece = k;
            } else {
                startPiece = P;
                endPiece = K;
            }
            
            for (int bbPiece = startPiece; bbPiece <= endPiece; bbPiece++) { 
                if (getBit(bitboards[bbPiece], targetSquare)) {
                    popBit(bitboards[bbPiece], targetSquare);
                    hashKey ^= pieceKeys[bbPiece][targetSquare];
                    break;
                }
            }
        }
        
        // Handle pawn promotion
        if (promoted) {  
            if (side == white) {
                popBit(bitboards[P], targetSquare);
                hashKey ^= pieceKeys[P][targetSquare];
            } else {
                popBit(bitboards[p], targetSquare);
                hashKey ^= pieceKeys[p][targetSquare];
            }
            setBit(bitboards[promoted], targetSquare);
            hashKey ^= pieceKeys[promoted][targetSquare];
        }
        
        // Handle en passant captures
        if (enpass) { 
            if (side == white) { 
                popBit(bitboards[p], targetSquare + 8);
                hashKey ^= pieceKeys[p][targetSquare + 8];
            } else {
                popBit(bitboards[P], targetSquare - 8);
                hashKey ^= pieceKeys[P][targetSquare - 8];
            }
        }
        
        // Hash out old en passant
        if (enpassant != no_sq) hashKey ^= enpassantKeys[enpassant];
        enpassant = no_sq;
        
        // Handle double pawn push
        if (dpp) { 
            if (side == white) { 
                enpassant = targetSquare + 8;
                hashKey ^= enpassantKeys[targetSquare + 8];
            } else {
                enpassant = targetSquare - 8;
                hashKey ^= enpassantKeys[targetSquare - 8];
            }
        }
        
        // Handle castling
        if (castling) { 
            switch (targetSquare) {
                case g1:
                    popBit(bitboards[R], h1);
                    setBit(bitboards[R], f1); 
                    hashKey ^= pieceKeys[R][h1];
                    hashKey ^= pieceKeys[R][f1];
                    break;
                case c1:
                    popBit(bitboards[R], a1);
                    setBit(bitboards[R], d1);
                    hashKey ^= pieceKeys[R][a1];
                    hashKey ^= pieceKeys[R][d1];
                    break;
                case g8:
                    popBit(bitboards[r], h8);
                    setBit(bitboards[r], f8); 
                    hashKey ^= pieceKeys[r][h8];
                    hashKey ^= pieceKeys[r][f8];
                    break;
                case c8:
                    popBit(bitboards[r], a8);
                    setBit(bitboards[r], d8); 
                    hashKey ^= pieceKeys[r][a8];
                    hashKey ^= pieceKeys[r][d8];
                    break;
            }
        }
        
        // Hash castling rights
        hashKey ^= castlingKeys[castle];
        
        // Update castling rights
        castle &= castlingRights[sourceSquare];
        castle &= castlingRights[targetSquare];
        
        hashKey ^= castlingKeys[castle];
        
        // Update occupancies
        memset(occupancy, 0ULL, 24);
        
        for (int bbPiece = P; bbPiece <= K; bbPiece++) { 
            occupancy[white] |= bitboards[bbPiece];
        }
        for (int bbPiece = p; bbPiece <= k; bbPiece++) { 
            occupancy[black] |= bitboards[bbPiece];
        }
        occupancy[both] = occupancy[white] | occupancy[black];
        
        // Change side
        side ^= 1;
        hashKey ^= sideKey;
        
        // Make sure the king isn't in check
        if (isAttacked((side == white) ? getLSBindex(bitboards[k]) : getLSBindex(bitboards[K]), side)) { 
            restoreBoard();
            return 0; 
        }
        
        return 1;
    } else {
        if (getCapture(move))
            return makeMove(move, allMoves);
        return 0;
    }
}

/************ Move Generation ************/
void generateMoves(moves *moveList) { 
    moveList->count = 0;
    int sourceSquare, targetSquare; 
    U64 bitboard, attacks;
    
    for (int piece = P; piece <= k; piece++) { 
        bitboard = bitboards[piece]; 
        
        // Generate white pawn and king castling moves
        if (side == white) { 
            if (piece == P) {
                while (bitboard) {
                    sourceSquare = getLSBindex(bitboard);
                    targetSquare = sourceSquare - 8;
                    
                    // Generate quiet pawn moves
                    if (!(targetSquare < a8) && !getBit(occupancy[both], targetSquare)) {
                        // Pawn promotion
                        if (sourceSquare >= a7 && sourceSquare <= h7) {
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, Q, 0, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, R, 0, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, B, 0, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, N, 0, 0, 0, 0));
                        } else {
                            // One square ahead
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, 0, 0, 0, 0, 0));
                            // Two squares ahead
                            if ((sourceSquare >= a2 && sourceSquare <= h2) && 
                                !getBit(occupancy[both], targetSquare - 8)) { 
                                addMove(moveList, encodeMove(sourceSquare, targetSquare - 8, piece, 0, 0, 1, 0, 0));
                            }
                        }
                    }
                    
                    // Pawn captures
                    attacks = pawnAttacks[side][sourceSquare] & occupancy[black];
                    while (attacks) { 
                        targetSquare = getLSBindex(attacks); 
                        if (sourceSquare >= a7 && sourceSquare <= h7) {
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, Q, 1, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, R, 1, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, B, 1, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, N, 1, 0, 0, 0));
                        } else {
                            int capturedPiece = getPieceOnSquare(targetSquare, side);
                            addMove(moveList, encodeMoveWithCapture(sourceSquare, targetSquare, piece, 0, 1, 0, 0, 0, capturedPiece));
                        }
                        popBit(attacks, targetSquare); 
                    }
                    
                    // En passant captures
                    if (enpassant != no_sq) { 
                        U64 enpassantAttacks = pawnAttacks[side][sourceSquare] & (1ULL << enpassant);
                        if (enpassantAttacks) { 
                            int targetEnpassant = getLSBindex(enpassantAttacks);
                            int capturedPiece = (side == white) ? p : P;
                            addMove(moveList, encodeMoveWithCapture(sourceSquare, targetEnpassant, piece, 0, 1, 0, 1, 0, capturedPiece));
                        }
                    }
                    popBit(bitboard, sourceSquare);
                }
            }
            
            // Castling moves
            if (piece == K) { 
                // King side castle
                if (castle & wk) {
                    if ((!getBit(occupancy[both], f1)) && (!getBit(occupancy[both], g1))) {      
                        if ((!isAttacked(e1, black)) && !isAttacked(f1, black)) { 
                            addMove(moveList, encodeMove(e1, g1, piece, 0, 0, 0, 0, 1));
                        }
                    }
                }
                // Queen side castle
                if (castle & wq) {
                    if ((!getBit(occupancy[both], d1)) && (!getBit(occupancy[both], c1)) && 
                        (!getBit(occupancy[both], b1))) {      
                        if ((!isAttacked(e1, black)) && !isAttacked(d1, black)) { 
                            addMove(moveList, encodeMove(e1, c1, piece, 0, 0, 0, 0, 1));
                        }
                    }
                }
            }
        }
        // Generate black pawn and king castling moves
        else {
            if (piece == p) {
                while (bitboard) {
                    sourceSquare = getLSBindex(bitboard);
                    targetSquare = sourceSquare + 8;
                    
                    // Generate quiet pawn moves
                    if (!(targetSquare > h1) && !getBit(occupancy[both], targetSquare)) {
                        // Pawn promotion
                        if (sourceSquare >= a2 && sourceSquare <= h2) {
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, q, 0, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, r, 0, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, b, 0, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, n, 0, 0, 0, 0));
                        } else {
                            // One square ahead
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, 0, 0, 0, 0, 0));
                            // Two squares ahead
                            if ((sourceSquare >= a7 && sourceSquare <= h7) && 
                                !getBit(occupancy[both], targetSquare + 8)) {
                                addMove(moveList, encodeMove(sourceSquare, targetSquare + 8, piece, 0, 0, 1, 0, 0));
                            }
                        }
                    }
                    
                    // Pawn captures
                    attacks = pawnAttacks[side][sourceSquare] & occupancy[white];
                    while (attacks) { 
                        targetSquare = getLSBindex(attacks); 
                        if (sourceSquare >= a2 && sourceSquare <= h2) {
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, q, 1, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, r, 1, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, b, 1, 0, 0, 0));
                            addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, n, 1, 0, 0, 0));
                        } else {
                            int capturedPiece = getPieceOnSquare(targetSquare, side);
                            addMove(moveList, encodeMoveWithCapture(sourceSquare, targetSquare, piece, 0, 1, 0, 0, 0, capturedPiece));
                        }
                        popBit(attacks, targetSquare); 
                    }
                    
                    // En passant captures
                    if (enpassant != no_sq) { 
                        U64 enpassantAttacks = pawnAttacks[side][sourceSquare] & (1ULL << enpassant);
                        if (enpassantAttacks) { 
                            int targetEnpassant = getLSBindex(enpassantAttacks);
                            int capturedPiece = (side == white) ? p : P;
                            addMove(moveList, encodeMoveWithCapture(sourceSquare, targetEnpassant, piece, 0, 1, 0, 1, 0, capturedPiece));
                        }
                    }
                    popBit(bitboard, sourceSquare);
                }
            }     
            
            // Castling moves
            if (piece == k) { 
                // King side castle
                if (castle & bk) {
                    if (!getBit(occupancy[both], f8) && !getBit(occupancy[both], g8)) {
                        if (!isAttacked(e8, white) && !isAttacked(f8, white)) { 
                            addMove(moveList, encodeMove(e8, g8, piece, 0, 0, 0, 0, 1));
                        }
                    }
                }
                // Queen side castle
                if (castle & bq) {
                    if (!getBit(occupancy[both], d8) && !getBit(occupancy[both], c8) && 
                        !getBit(occupancy[both], b8)) {  
                        if (!isAttacked(e8, white) && !isAttacked(d8, white)) {
                            addMove(moveList, encodeMove(e8, c8, piece, 0, 0, 0, 0, 1));
                        }
                    }
                }
            }
        }
        
        // Generate knight moves
        if ((side == white) ? piece == N : piece == n) {
            while (bitboard) {
                sourceSquare = getLSBindex(bitboard); 
                attacks = knightAttacks[sourceSquare] & 
                          ((side == white) ? ~occupancy[white] : ~occupancy[black]);
                while (attacks) { 
                    targetSquare = getLSBindex(attacks);
                    if (!getBit((side == white) ? occupancy[black] : occupancy[white], targetSquare)) { 
                        addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, 0, 0, 0, 0, 0));
                    } else {
                        int capturedPiece = getPieceOnSquare(targetSquare, side);
                        addMove(moveList, encodeMoveWithCapture(sourceSquare, targetSquare, piece, 0, 1, 0, 0, 0, capturedPiece));
                    }
                    popBit(attacks, targetSquare);
                }
                popBit(bitboard, sourceSquare);
            }
        }
        
        // Generate bishop moves
        if ((side == white) ? piece == B : piece == b) {
            while (bitboard) {
                sourceSquare = getLSBindex(bitboard); 
                attacks = getBishopAttacks(sourceSquare, occupancy[both]) & 
                          ((side == white) ? ~occupancy[white] : ~occupancy[black]);
                while (attacks) { 
                    targetSquare = getLSBindex(attacks);
                    if (!getBit((side == white) ? occupancy[black] : occupancy[white], targetSquare)) { 
                        addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, 0, 0, 0, 0, 0));
                    } else {
                        int capturedPiece = getPieceOnSquare(targetSquare, side);
                        addMove(moveList, encodeMoveWithCapture(sourceSquare, targetSquare, piece, 0, 1, 0, 0, 0, capturedPiece));
                    }
                    popBit(attacks, targetSquare);
                }
                popBit(bitboard, sourceSquare);
            }
        }
        
        // Generate rook moves 
        if ((side == white) ? piece == R : piece == r) {
            while (bitboard) {
                sourceSquare = getLSBindex(bitboard); 
                attacks = getRookAttacks(sourceSquare, occupancy[both]) & 
                          ((side == white) ? ~occupancy[white] : ~occupancy[black]);
                while (attacks) { 
                    targetSquare = getLSBindex(attacks);
                    if (!getBit((side == white) ? occupancy[black] : occupancy[white], targetSquare)) { 
                        addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, 0, 0, 0, 0, 0));
                    } else {
                        int capturedPiece = getPieceOnSquare(targetSquare, side);
                        addMove(moveList, encodeMoveWithCapture(sourceSquare, targetSquare, piece, 0, 1, 0, 0, 0, capturedPiece));
                    }
                    popBit(attacks, targetSquare);
                }
                popBit(bitboard, sourceSquare);
            }
        }
        
        // Generate queen moves
        if ((side == white) ? piece == Q : piece == q) {
            while (bitboard) {
                sourceSquare = getLSBindex(bitboard); 
                attacks = getQueenAttacks(sourceSquare, occupancy[both]) & 
                          ((side == white) ? ~occupancy[white] : ~occupancy[black]);
                while (attacks) { 
                    targetSquare = getLSBindex(attacks);
                    if (!getBit((side == white) ? occupancy[black] : occupancy[white], targetSquare)) { 
                        addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, 0, 0, 0, 0, 0));
                    } else {
                        int capturedPiece = getPieceOnSquare(targetSquare, side);
                        addMove(moveList, encodeMoveWithCapture(sourceSquare, targetSquare, piece, 0, 1, 0, 0, 0, capturedPiece));
                    }
                    popBit(attacks, targetSquare);
                }
                popBit(bitboard, sourceSquare);
            }
        }
        
        // Generate king moves
        if ((side == white) ? piece == K : piece == k) {
            while (bitboard) {
                sourceSquare = getLSBindex(bitboard); 
                attacks = kingAttacks[sourceSquare] & 
                          ((side == white) ? ~occupancy[white] : ~occupancy[black]);
                while (attacks) { 
                    targetSquare = getLSBindex(attacks);
                    if (!getBit((side == white) ? occupancy[black] : occupancy[white], targetSquare)) { 
                        addMove(moveList, encodeMove(sourceSquare, targetSquare, piece, 0, 0, 0, 0, 0));
                    } else {
                        int capturedPiece = getPieceOnSquare(targetSquare, side);
                        addMove(moveList, encodeMoveWithCapture(sourceSquare, targetSquare, piece, 0, 1, 0, 0, 0, capturedPiece));
                    }
                    popBit(attacks, targetSquare);
                }
                popBit(bitboard, sourceSquare);
            }
        }
    }
}
