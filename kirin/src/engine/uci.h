/*
 * Kirin Chess Engine
 * uci.h - UCI protocol interface
 */

#ifndef KIRIN_UCI_H
#define KIRIN_UCI_H

/************ UCI Functions ************/
// Parse move string (e.g., "e7e8q")
int parseMove(const char *moveString);

// Parse UCI position command
void parsePosition(const char *command);

// Parse UCI go command
void parseGo(char *command);

// Main UCI loop
void uciLoop();

#endif // KIRIN_UCI_H
