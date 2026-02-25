/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    zobrist.h - Zobrist hashing for position identification
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

#ifndef KIRIN_ZOBRIST_H
#define KIRIN_ZOBRIST_H

#include "types.h"

/************ Zobrist Keys ************/
// Random piece keys [piece][square]
extern U64 pieceKeys[12][64];

// Random en passant keys [square]
extern U64 enpassantKeys[64];

// Random castling keys [16 possible states]
extern U64 castlingKeys[16];

// Random side key
extern U64 sideKey;

/************ Zobrist Functions ************/
// Initialize random keys
void initRandKeys();

// Generate hash key from current position
U64 generateHashKey();

#endif // KIRIN_ZOBRIST_H
