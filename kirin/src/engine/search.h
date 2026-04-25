/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    movegen.cpp - Move generation and encoding
*    search.h - Search algorithms and transposition table
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

#ifndef KIRIN_SEARCH_H
#define KIRIN_SEARCH_H

#include "types.h"

/************ Transposition Table ************/
extern tt transpositionTable[hashSize];

/************ Search Tables ************/
// Killer moves [id][ply]
extern int killerMoves[2][maxPly];

// History moves [piece][square]
extern int historyMoves[12][64];

// Principal variation
extern int pvLength[maxPly];
extern int pvTable[maxPly][maxPly];
extern int followPV;
extern int scorePV;
extern int bestMove;

/************ MVV-LVA Table ************/
extern int mvvLVA[12][12];

/************ Transposition Table Functions ************/
void clearTranspositionTable();
int readHashEntry(int alpha, int beta, int depth);
void recordHash(int score, int depth, int hashFlag, int move);

/************ Move Ordering ************/
int scoreMove(int move);
void sortMoves(moves *moveList);
void enablePvScoring(moves *moveList);

/************ Repetition Detection ************/
int isRepeating();

/************ Search Functions ************/
int quiescence(int alpha, int beta);
int negamax(int alpha, int beta, int depth);
void searchPosition(int depth);

/************ Skill / Difficulty ************/
// 0 = easy (depth 3, ±150cp noise)
// 1 = medium (depth 5, ±50cp noise)
// 2 = hard (depth 64, no noise)
extern int skillLevel;

/************ Debug Functions ************/
void printMoveScores(moves *moveList);

#endif // KIRIN_SEARCH_
