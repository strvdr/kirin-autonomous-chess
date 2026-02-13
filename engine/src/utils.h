/*
 * Kirin Chess Engine
 * utils.h - Time control, random numbers, and I/O utilities
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
