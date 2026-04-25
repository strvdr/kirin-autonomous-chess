/*
*       ,  ,
*           \\ \\                 
*           ) \\ \\    _p_        
*            )^\))\))  /  *\      KIRIN CHESS ENGINE v0.3.1 (Development Build)
*             \_|| || / /^`-'     
*    __       -\ \\--/ /          Author:      Strydr Silverberg
*  <'  \\___/   ___. )'                        Colorado School of Mines, Class of 2026
*       `====\ )___/\\            
*            //     `"            
*            \\    /  \           
*            `"                   
*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*   Copyright (C) 2026 Strydr Silverberg
*   engine.h - Main public interface
*   This header provides the complete public API for the Kirin chess engine.
*   Include this single header to access all engine functionality.
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
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

// Neural network evaluation
#include "nnue.h"

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
