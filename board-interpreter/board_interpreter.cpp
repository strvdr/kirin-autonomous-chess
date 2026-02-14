/*
 * Kirin Board Interpreter
 * board_interpreter.cpp - Physical board path planning and state management
 */

#include <cstdio>
#include "board_interpreter.h"

/************ BoardCoord Implementation ************/
BoardCoord BoardCoord::fromFEN(const char* str) {
  if(!str || !str[0] ||! str[1]) {
    return BoardCoord();
  }

  int file = str[0] - 'a'; //0-7
  int rank = str[1] - '1'; //0-7, but we need to flip
  
  if(file < 0 || file > 7 || rank < 0 || rank > 7) { 
    return BoardCoord();
  }

  int row = 7 - rank;
  int col = file;

  return BoardCoord(row, col);
}

void BoardCoord::toFEN(char* out) const {
  out[0] = 'a' + col;
  out[1] = '8' - row;
  out[2] = '\0';
}


/************ Physical Board Implementation ************/
PhysicalBoard::PhysicalBoard() : occupied(0) {}

void PhysicalBoard::setOccupancy(Bitboard occ) { 
  occupied = occ;
}

bool PhysicalBoard::isOccupied(const BoardCoord& coord) const { 
  return isOccupied(coord.row, coord.col);
}

bool PhysicalBoard::isOccupied(int row, int col) const { 
  if(!isValidSquare(row, col)) return false;
  int square = row * 8 + col;
  return getBit(occupied, square);
}

bool PhysicalBoard::isValidSquare(const BoardCoord& coord) { 
  return isValidSquare(coord.row, coord.col);
}

bool PhysicalBoard::isValidSquare(int row, int col) { 
  return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE;
}
