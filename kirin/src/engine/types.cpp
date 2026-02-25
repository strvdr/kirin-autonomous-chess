/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    types.cpp - Implementation of global constants
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

#include "types.h"

/************ Square Coordinates ************/
const char *squareCoordinates[64] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1"
};

/************ Piece Characters ************/
char asciiPieces[13] = "PNBRQKpnbrqk";
const char *unicodePieces[12] = {"♙", "♘", "♗", "♖", "♕", "♔", "♟", "♞", "♝", "♜", "♛", "♚"};

// Array indexed by ASCII value for piece character lookup
int charPieces[128] = {0};

// Array indexed by piece enum value for promotion character lookup
char promotedPieces[12] = {0};

// Helper struct to initialize charPieces at startup
static struct CharPiecesInitializer {
    CharPiecesInitializer() {
        charPieces['P'] = P;
        charPieces['N'] = N;
        charPieces['B'] = B;
        charPieces['R'] = R;
        charPieces['Q'] = Q;
        charPieces['K'] = K;
        charPieces['p'] = p;
        charPieces['n'] = n;
        charPieces['b'] = b;
        charPieces['r'] = r;
        charPieces['q'] = q;
        charPieces['k'] = k;
    }
} charPiecesInit;

// Helper struct to initialize promotedPieces at startup
static struct PromotedPiecesInitializer {
    PromotedPiecesInitializer() {
        promotedPieces[Q] = 'q';
        promotedPieces[R] = 'r';
        promotedPieces[B] = 'b';
        promotedPieces[N] = 'n';
        promotedPieces[q] = 'q';
        promotedPieces[r] = 'r';
        promotedPieces[b] = 'b';
        promotedPieces[n] = 'n';
    }
} promotedPiecesInit;

/************ Castling Rights Update Constants ************/
const int castlingRights[64] = {
    7, 15, 15, 15,  3, 15, 15, 11,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   13, 15, 15, 15, 12, 15, 15, 14 
};
