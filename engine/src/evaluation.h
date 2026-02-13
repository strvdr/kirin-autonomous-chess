/*
 * Kirin Chess Engine
 * evaluation.h - Position evaluation
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
int evaluate();

#endif // KIRIN_EVALUATION_H
