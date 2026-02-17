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
 * engine.h - Main public interface
 * 
 * This header provides the complete public API for the Kirin chess engine.
 * Include this single header to access all engine functionality.
 */

#ifndef KIRIN_ENGINE_H
#define KIRIN_ENGINE_H

// Core types and definitions
#include "types.h"

// Board representation
#include "bitboard.h"

// Zobrist hashing
#include "zobrist.h"

// Attack generation
#include "attacks.h"

// Move generation
#include "movegen.h"

// Position management
#include "position.h"

// Evaluation
#include "evaluation.h"

// Search
#include "search.h"

// UCI protocol
#include "uci.h"

// Utilities
#include "utils.h"

/************ Engine Initialization ************/
// Initialize all engine subsystems
// Call this once before using any other engine functions
void initAll();

#endif // KIRIN_ENGINE_H
