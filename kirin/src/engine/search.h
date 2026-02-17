/*
 * Kirin Chess Engine
 * search.h - Search algorithms and transposition table
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
void recordHash(int score, int depth, int hashFlag);

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

/************ Debug Functions ************/
void printMoveScores(moves *moveList);

#endif // KIRIN_SEARCH_
