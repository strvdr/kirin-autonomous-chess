/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    evaluation.h - Position evaluation
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

#ifndef KIRIN_EVALUATION_H
#define KIRIN_EVALUATION_H

#include "types.h"

/************ Material Scores ************/
extern int materialScore[12];

/************ Piece-Square Tables ************/
extern const int pawnScore[64];
extern const int knightScore[64];
extern const int bishopScore[64];
extern const int rookScore[64];
extern const int kingScore[64];
extern const int mirrorScore[128];

/************ Pawn Structure ************/
extern const int getRank[64];
extern const int doublePawnPenalty;
extern const int isolatedPawnPenalty;
extern const int passedPawnBonus[8];

/************ Evaluation Masks ************/
extern U64 fileMasks[64];
extern U64 rankMasks[64];
extern U64 isolatedMasks[64];
extern U64 passedWhiteMasks[64];
extern U64 passedBlackMasks[64];

/************ Initialization ************/
void initEvaluationMasks();

/************ Evaluation Function ************/
int evaluateClassical();
int evaluate();

/************ NNUE Evaluation Mode ************/
void setUseNNUE(bool enabled);
bool getUseNNUE();

#endif // KIRIN_EVALUATION_H
