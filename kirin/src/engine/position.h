/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    position.h - Position management and FEN parsing
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

#ifndef KIRIN_POSITION_H
#define KIRIN_POSITION_H

#include "types.h"

/************ FEN Parsing ************/
void parseFEN(const char *fenStr);

/************ Board Display ************/
void printBoard();

/************ Perft Testing ************/
long perft(int depth);
void perftTest(int depth);

// Node counter for perft
extern long nodes;

#endif // KIRIN_POSITION_H
