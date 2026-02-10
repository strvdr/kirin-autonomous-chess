/*
 *       ,  ,
 *           \\ \\                 
 *           ) \\ \\    _p_        
 *            )^\))\))  /  *\      KIRIN CHESS ENGINE v0.2 (Development Build)
 *             \_|| || / /^`-'     
 *    __       -\ \\--/ /          Author:      Strydr Silverberg
 *  <'  \\___/   ___. )'                        Colorado School of Mines, Class of 2026
 *       `====\ )___/\\            
 *            //     `"            
 *            \\    /  \           
 *            `"                   
 */

/*
 * Kirin Chess Engine
 * engine.h - Public interface for the chess engine
 */

#ifndef KIRIN_ENGINE_H
#define KIRIN_ENGINE_H

// Initialization functions
void initLeaperAttacks();
void initSliderAttacks(int bishop);
void initRandKeys();
void initEvaluationMasks();
void clearTranspositionTable();

// UCI interface
void uciLoop();

// Debug/utility functions
void parseFEN(const char *fen);
void printBoard();
int evaluate();

// Piece type constants needed for initSliderAttacks
enum { rook_slider = 0, bishop_slider = 1 };

#endif // KIRIN_ENGINE_H
