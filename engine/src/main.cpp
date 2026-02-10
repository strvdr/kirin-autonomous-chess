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
 * ══════════════════════════════════════════════════════════════════════════════
 * PROJECT OVERVIEW
 * ══════════════════════════════════════════════════════════════════════════════
 * 
 * Kirin is the computational engine powering an autonomous chess system
 * designed to deliver a complete single-player experience. By integrating
 * intelligent move generation with a physical board capable of independent
 * piece manipulation, the system eliminates the need for a human opponent
 * while preserving the tactile satisfaction of over-the-board play.
 * 
 * ══════════════════════════════════════════════════════════════════════════════
 * FUNDING & ACKNOWLEDGMENTS
 * ══════════════════════════════════════════════════════════════════════════════
 * 
 * 2026 Colorado School of Mines ProtoFund Recipient
 * Team: Strydr Silverberg, Colin Dake
 * 
 * This prototype represents the practical application of engineering
 * principles and computer science fundamentals developed through our
 * coursework at the Colorado School of Mines.
 */

#include <cstdio>
#include "engine.h"

// Forward declarations for slider types from engine
extern const int rook;
extern const int bishop;

/************ Init All ************/
void initAll() {
    initLeaperAttacks(); 
    initSliderAttacks(1);  // bishop
    initSliderAttacks(0);  // rook
    initRandKeys();
    initEvaluationMasks();
    clearTranspositionTable();
}

/************ Main Driver ************/
int main() { 
    initAll(); 
    
    int debug = 0;
    
    if (debug) { 
        parseFEN("8/8/8/P1P4/8/5p1p/8/8 w - - ");
        printBoard(); 
        printf("Score: %d\n", evaluate());
    } else {
        uciLoop();
    }
    
    return 0;
}

