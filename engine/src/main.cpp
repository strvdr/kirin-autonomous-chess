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

/************ Initialize All Subsystems ************/
void initAll() {
    // Initialize attack tables
    initLeaperAttacks(); 
    initSliderAttacks(bishop_slider);
    initSliderAttacks(rook_slider);
    
    // Initialize Zobrist keys
    initRandKeys();
    
    // Initialize evaluation masks
    initEvaluationMasks();
    
    // Clear transposition table
    clearTranspositionTable();
}

/************ Main Driver ************/
int main() { 
    // Initialize engine
    initAll(); 
    
    int debug = 0;
    
    if (debug) { 
        // Debug mode: load position and evaluate
        parseFEN("8/8/8/P1P4/8/5p1p/8/8 w - - ");
        printBoard(); 
        printf("Score: %d\n", evaluate());
    } else {
        // Normal mode: run UCI loop
        uciLoop();
    }
    
    return 0;
}
