/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    utils.cpp - Time control, random numbers, and I/O utilities
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

#include <unistd.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <sys/time.h>
#include "utils.h"
#include "attacks.h"
#include "zobrist.h"
#include "evaluation.h"
#include "search.h"

/************ Time Control Variables ************/
int quit = 0;
int movesToGo = 30;
int moveTime = -1;
int timer = -1;
int inc = 0;
std::int64_t startTime = 0;
std::int64_t stopTime = 0;
int timeSet = 0;
int stopped = 0;

/************ Random Number State ************/
unsigned int randState = 1804289383;

/************ Time Control Functions (from VICE by Richard Allbert) ************/
std::int64_t getTimeMS() {
    struct timeval timeValue;
    gettimeofday(&timeValue, NULL);
    return static_cast<std::int64_t>(timeValue.tv_sec) * 1000 +
           static_cast<std::int64_t>(timeValue.tv_usec) / 1000;
}

int inputWaiting() {
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    const int stdinFd = fileno(stdin);
    FD_SET(stdinFd, &readfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if (select(stdinFd + 1, &readfds, nullptr, nullptr, &tv) <= 0) {
        return 0;
    }
    return FD_ISSET(stdinFd, &readfds);
}

void readInput() { 
    int bytes;
    char input[256] = "", *endc;

    if (inputWaiting()) { 
        do {
            bytes = read(fileno(stdin), input, 256);
        } while (bytes < 0);

        if (bytes <= 0) {
            return;
        }

        stopped = 1;
        if (bytes < static_cast<int>(sizeof(input))) {
            input[bytes] = 0;
        } else {
            input[sizeof(input) - 1] = 0;
        }

        endc = strchr(input, '\n');
        if (endc) *endc = 0;

        if (strlen(input) > 0) { 
            if (!strncmp(input, "quit", 4)) quit = 1;
            else if (!strncmp(input, "stop", 4)) quit = 1;
        }
    }
}

void communicate() { 
    if (timeSet == 1 && getTimeMS() > stopTime) stopped = 1;
    readInput();
}

void resetTimeControl() {
    quit = 0;
    movesToGo = 30;
    moveTime = -1;
    timer = -1;
    inc = 0;
    startTime = 0;
    stopTime = 0;
    timeSet = 0;
    stopped = 0;
}

/************ Random Number Generation (from Tord Romstad) ************/
unsigned int getRandU32() {
    unsigned int num = randState;
    
    // XOR shift algorithm 
    num ^= num << 13;
    num ^= num >> 17;
    num ^= num << 5;
    
    randState = num; 
    return num;
}

U64 getRandU64() {
    U64 n1, n2, n3, n4;
    
    n1 = (U64)(getRandU32()) & 0xFFFF;
    n2 = (U64)(getRandU32()) & 0xFFFF;
    n3 = (U64)(getRandU32()) & 0xFFFF;
    n4 = (U64)(getRandU32()) & 0xFFFF;
    
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

U64 generateMagicNum() {
    return getRandU64() & getRandU64() & getRandU64();
}

void initAll() {
    initLeaperAttacks();
    initSliderAttacks(bishop_slider);
    initSliderAttacks(rook_slider);
    initRandKeys();
    initEvaluationMasks();
    clearTranspositionTable();
}
