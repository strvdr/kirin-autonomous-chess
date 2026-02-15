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

void PhysicalBoard::setOccupied(const BoardCoord& coord) { 
  if(!isValidSquare(coord)) return;
  int square = coord.toSquareIndex();
  setBit(occupied, square);
}

void PhysicalBoard::clearSquare(const BoardCoord& coord) { 
  if(!isValidSquare(coord)) return;
  int square = coord.toSquareIndex();
  clearBit(occupied, square);
}

void PhysicalBoard::movePiece(const BoardCoord& from, const BoardCoord& to) { 
  clearSquare(from);
  setOccupied(to);
}

void PhysicalBoard::print() const {
  printf("\n  Physical Board Occupancy:\n");
  printf("  +-----------------+\n");

  for(int row = 0; row < BOARD_SIZE; row++) {
    printf("%d | ", 8 - row);
    for(int col = 0; col < BOARD_SIZE; col++) { 
      if(isOccupied(row, col)) {
        printf("X ");
      } else {
        printf(". ");
      }
    }
    printf("|\n");
  }
  printf("  +-----------------+\n");
  printf("    a b c d e f g h\n\n");
}


/************ Utility Function Implementations ************/
std::vector<BoardCoord> getNeighbors(const BoardCoord& pos) { 
  std::vector<BoardCoord> neighbors;
  neighbors.reserve(8);
  for(int rowDelta = -1; rowDelta <= 1; rowDelta++) { 
    for(int colDelta = -1; colDelta <= 1; colDelta++) {
      if(rowDelta == 0 && colDelta == 0) continue;

      int newRow = pos.row + rowDelta;
      int newCol = pos.col + colDelta;

      if(PhysicalBoard::isValidSquare(newRow, newCol)) { 
        neighbors.push_back(BoardCoord(newRow, newCol));
      }
    }
  }

  return neighbors;
}

int chebyshevDistance(const BoardCoord& from, const BoardCoord& to) {
  int rowDiff = (from.row > to.row) ? (from.row - to.row) : (to.row - from.row);
  int colDiff = (from.col > to.col ) ? (from.col - to.col) : (to.col- from.col);
  return (rowDiff > colDiff) ? rowDiff : colDiff;
}

int manhattanDistance(const BoardCoord& from, const BoardCoord& to) {
  int rowDiff = (from.row > to.row) ? (from.row - to.row) : (to.row - from.row);
  int colDiff = (from.col > to.col ) ? (from.col - to.col) : (to.col- from.col);
  return rowDiff + colDiff; 
}

int countBits(Bitboard bb) { 
  int count = 0;
  while(bb) { 
    count++;
    bb &= bb - 1; //clear set lsb
  }
  return count;
}

int getLSB(Bitboard bb) {
  if(!bb) return -1;

  //count trailing zeros
  int index = 0;
  while(!(bb & 1)) { 
    bb >>= 1;
    index++;
  }

  return index;
}
