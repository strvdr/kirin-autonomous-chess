/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    utils.h - Time control, random numbers, and I/O utilities
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

#ifndef KIRIN_UTILS_H
#define KIRIN_UTILS_H

#include "types.h"

/************ Time Control Variables ************/
extern int quit;
extern int movesToGo;
extern int moveTime;
extern int timer;
extern int inc;
extern int startTime;
extern int stopTime;
extern int timeSet;
extern int stopped;

/************ Time Control Functions ************/
int getTimeMS();
int inputWaiting();
void readInput();
void communicate();
void resetTimeControl();

/************ Random Number Generation ************/
// Pseudo random number state
extern unsigned int randState;

// Generate 32-bit pseudo random number
unsigned int getRandU32();

// Generate 64-bit pseudo random number
U64 getRandU64();

// Generate magic number candidate
U64 generateMagicNum();

#endif // KIRIN_UTILS_H
