/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    nnue.h - Efficiently updatable neural network evaluation scaffold
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

#ifndef KIRIN_NNUE_H
#define KIRIN_NNUE_H

/************ NNUE Initialization ************/
void initNNUE();
bool isNNUEInitialized();

/************ NNUE Evaluation ************/
int evaluateNNUE();

/************ NNUE Network Loading ************/
/*
 * Kirin NNUE v1 binary layout, little-endian:
 *   char[8]  magic: "KIRINNUE"
 *   u32      version: 1
 *   u32      feature count: 768 (12 pieces * 64 squares)
 *   u32      hidden size
 *   i32      clipped ReLU activation limit
 *   i32      output bias
 *   i32[]    hidden biases      [hidden size]
 *   i32[]    output weights     [hidden size]
 *   i32[]    feature weights    [feature count * hidden size], feature-major
 */
bool loadNNUE(const char *path);
void resetNNUEToBootstrap();
const char *getNNUELastError();
const char *getNNUESource();
bool hasExternalNNUE();

/************ NNUE Configuration ************/
void setNNUEEnabled(bool enabled);
bool isNNUEEnabled();

#endif // KIRIN_NNUE_H
