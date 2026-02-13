/*
 * Kirin Chess Engine
 * position.h - Position management and FEN parsing
 */

#ifndef KIRIN_POSITION_H
#define KIRIN_POSITION_H

#include "types.h"

/************ FEN Parsing ************/
void parseFEN(const char *fenStr);

/************ Board Display ************/
void printBoard();

/************ Perft Testing ************/
void perftTest(int depth);

// Node counter for perft
extern long nodes;

#endif // KIRIN_POSITION_H
