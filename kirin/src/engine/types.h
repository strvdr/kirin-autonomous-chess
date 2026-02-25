/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    types.h - Basic type definitions, constants, and enums
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

#ifndef KIRIN_TYPES_H
#define KIRIN_TYPES_H

/************ Bitboard Type ************/
#define U64 unsigned long long

/************ Board Squares ************/
enum Square {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1, 
    no_sq
};

/************ Piece Types ************/
enum Piece { P, N, B, R, Q, K, p, n, b, r, q, k };

/************ Colors ************/
enum Color { white, black, both };

/************ Slider Types ************/
enum SliderType { rook_slider, bishop_slider };

/************ Castling Rights ************/
/*
    bin  dec
   0001    1  white king can castle to the king side
   0010    2  white king can castle to the queen side
   0100    4  black king can castle to the king side
   1000    8  black king can castle to the queen side
*/
enum CastlingRights { wk = 1, wq = 2, bk = 4, bq = 8 };

/************ Move Types ************/
enum MoveType { allMoves, onlyCaptures };

/************ Search Constants ************/
constexpr int maxPly = 64;
constexpr int infinity = 50000;
constexpr int mateValue = 49000;
constexpr int mateScore = 48000;

/************ Transposition Table Constants ************/
constexpr int hashSize = 0x400000;
constexpr int noHashEntry = 100000;
constexpr int hashFlagExact = 0;
constexpr int hashFlagAlpha = 1;
constexpr int hashFlagBeta = 2;

/************ Move List Structure ************/
typedef struct {
    int moves[256]; 
    int count; 
} moves;

/************ Transposition Table Entry ************/
typedef struct {
    U64 key; 
    int depth;
    int flag;
    int score;
} tt;

/************ FEN Debug Positions ************/
#define emptyBoard "8/8/8/8/8/8/8/8 b - - "
#define startPosition "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "
#define killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 "
#define new_pos "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 "
#define repetitions "2r3k1/R7/8/1R6/8/8/P4KPP/8 w - - 0 40 "

/************ Square Coordinates ************/
extern const char *squareCoordinates[64];

/************ Piece Characters ************/
extern char asciiPieces[13];
extern const char *unicodePieces[12];
extern int charPieces[128];
extern char promotedPieces[12];

/************ Castling Rights Update Constants ************/
extern const int castlingRights[64];

#endif // KIRIN_TYPES_H
