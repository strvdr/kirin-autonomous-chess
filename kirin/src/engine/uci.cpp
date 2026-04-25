/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    uci.cpp - UCI protocol implementation
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "uci.h"
#include "types.h"
#include "bitboard.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "utils.h"

/************ Parse Move String ************/
int parseMove(const char *moveString) { 
    moves moveList[1];
    generateMoves(moveList);
    
    int sourceSquare = (moveString[0] - 'a') + (8 - (moveString[1] - '0')) * 8;
    int targetSquare = (moveString[2] - 'a') + (8 - (moveString[3] - '0')) * 8;

    auto isLegalMove = [](int move) {
        BoardState state = copyBoard();
        int legal = makeMove(move, allMoves);
        restoreBoard(state);
        return legal != 0;
    };
    
    for (int i = 0; i < moveList->count; i++) { 
        int move = moveList->moves[i];
        
        if (sourceSquare == getSource(move) && targetSquare == getTarget(move)) { 
            int promotedPiece = getPromoted(move);
            
            if (promotedPiece) {  
                if ((promotedPiece == Q || promotedPiece == q) && moveString[4] == 'q') { 
                    return isLegalMove(move) ? move : 0;
                } else if ((promotedPiece == R || promotedPiece == r) && moveString[4] == 'r') {
                    return isLegalMove(move) ? move : 0;
                } else if ((promotedPiece == B || promotedPiece == b) && moveString[4] == 'b') {
                    return isLegalMove(move) ? move : 0;
                } else if ((promotedPiece == N || promotedPiece == n) && moveString[4] == 'n') {
                    return isLegalMove(move) ? move : 0;
                }
                continue;
            }
            return isLegalMove(move) ? move : 0;
        }
    }
    
    return 0;  // Illegal move
}

/************ Parse UCI Position Command ************/
void parsePosition(const char *commandStr) { 
    const char *command = commandStr + 9;
    const char *currentChar = command;
    
    if (strncmp(command, "startpos", 8) == 0) {
        parseFEN(startPosition);
    } else { 
        currentChar = strstr(command, "fen");
        if (currentChar == NULL) { 
            parseFEN(startPosition);
        } else {
            currentChar += 4;
            parseFEN(currentChar);
        }
    }
    
    currentChar = strstr(command, "moves");
    if (currentChar != NULL) { 
        currentChar += 6; 
        
        while (*currentChar) { 
            int move = parseMove(currentChar);
            if (move == 0) break; 
            
            repetitionTable[repetitionIndex] = hashKey;
            repetitionIndex++;
            
            makeMove(move, allMoves);
            
            while (*currentChar && *currentChar != ' ') currentChar++;
            while (*currentChar == ' ') currentChar++;
        }
    }
}

/************ Parse UCI Go Command ************/
void parseGo(char *command) {
    // Reset time control
    resetTimeControl();
    
    int depth = -1;
    char *argument = NULL;
    
    // Infinite search
    if ((argument = strstr(command, "infinite"))) {}
    
    // Black increment
    if ((argument = strstr(command, "binc")) && side == black)
        inc = atoi(argument + 5);
    
    // White increment
    if ((argument = strstr(command, "winc")) && side == white)
        inc = atoi(argument + 5);
    
    // White time
    if ((argument = strstr(command, "wtime")) && side == white)
        timer = atoi(argument + 6);
    
    // Black time
    if ((argument = strstr(command, "btime")) && side == black)
        timer = atoi(argument + 6);
    
    // Moves to go
    if ((argument = strstr(command, "movestogo")))
        movesToGo = atoi(argument + 10);
    
    // Move time
    if ((argument = strstr(command, "movetime")))
        moveTime = atoi(argument + 9);
    
    // Depth
    if ((argument = strstr(command, "depth")))
        depth = atoi(argument + 6);
    
    // Perft
    if ((argument = strstr(command, "perft"))) {
        depth = atoi(argument + 6);
        perftTest(depth);
        return;
    }
    
    // If move time is specified
    if (moveTime != -1) {
        timer = moveTime;
        movesToGo = 1;
    }
    
    startTime = getTimeMS();
    
    // Time control
    if (timer != -1) {
        timeSet = 1;
        timer /= movesToGo;
        timer -= 50;  // Lag compensation
        
        if (timer < 0) {
            timer = 0;
            inc -= 50;
            if (inc < 0) inc = 1;
        }
        
        stopTime = startTime + timer + inc;        
    }
    
    // Default depth
    if (depth == -1)
        depth = 64;
    
    printf("time: %d  inc: %d  start: %lld  stop: %lld  depth: %d  timeset:%d\n",
           timer, inc,
           static_cast<long long>(startTime),
           static_cast<long long>(stopTime),
           depth, timeSet);
    
    searchPosition(depth);
}

/************ UCI Loop ************/
void uciLoop() {
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    
    char input[2000];
    
    printf("id name Kirin\n");
    printf("id author Strydr Silverberg\n");
    printf("option name Skill Level type spin default 2 min 0 max 2\n");
    printf("uciok\n");
    
    while (1) {
        memset(input, 0, sizeof(input));
        fflush(stdout);
        
        if (!fgets(input, 2000, stdin))
            break;
        
        if (input[0] == '\n')
            continue;
        
        if (strncmp(input, "isready", 7) == 0) {
            printf("readyok\n");
            continue;
        }
        
        if (strncmp(input, "position", 8) == 0) { 
            parsePosition(input);
            clearTranspositionTable();
        }
        else if (strncmp(input, "ucinewgame", 10) == 0) {
            parsePosition("position startpos");
            clearTranspositionTable();
        }
        else if (strncmp(input, "go", 2) == 0) {
            parseGo(input);
            if (quit) break;
        }
        else if (strncmp(input, "quit", 4) == 0) {
            break;
        }
        if (strncmp(input, "uci", 3) == 0) {
            printf("id name Kirin\n");
            printf("id author Strydr Silverberg\n");
            printf("option name Skill Level type spin default 2 min 0 max 2\n");
            printf("uciok\n");
        }
        else if (strncmp(input, "setoption name Skill Level value", 32) == 0) {
            int level = atoi(input + 33);
            if (level < 0) level = 0;
            if (level > 2) level = 2;
            skillLevel = level;
            printf("info string Skill Level set to %d\n", skillLevel);
        }
    }
}
