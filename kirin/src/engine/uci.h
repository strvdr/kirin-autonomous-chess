/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    uci.h - UCI protocol interface
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
