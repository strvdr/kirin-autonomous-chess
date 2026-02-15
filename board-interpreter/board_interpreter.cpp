/*
 * Kirin Board Interpreter
 * board_interpreter.cpp - Physical board path planning and state management
 */

#include <cstdio>
#include <cstdlib>
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

/************ Path Generation Implementation ************/
//Generate diagonal path(for bishops and diagonal queen moves)
Path generateDiagonalPath(const BoardCoord& from, const BoardCoord& to) {
  Path path;
  int rowStep = sign(to.row - from.row);
  int colStep = sign(to.col - from.col);
  BoardCoord current = from;

  while(current != to) {
    current = BoardCoord(current.row + rowStep, current.col + colStep);
    path.append(current);
  }

  return path;
}

Path generateOrthogonalPath(const BoardCoord& from, const BoardCoord& to) {
  Path path;
  int rowStep = sign(to.row - from.row);
  int colStep = sign(to.col - from.col);
  BoardCoord current = from;

  while(current != to) {
    current = BoardCoord(current.row + rowStep, current.col + colStep);
    path.append(current);
  }

  return path;
}

//Pawn path: straight line forward(1 or 2 squares)
std::vector<Path> generatePawnPath(const BoardCoord& from, const BoardCoord& to) { 
  std::vector<Path> paths;
  Path path;
  
  int rowStep = sign(to.row - from.row);
  int colStep = sign(to.col - from.col);
  BoardCoord current = from;

  while(current != to) {
    current = BoardCoord(current.row + rowStep, current.col + colStep);
    path.append(current);
  }

  paths.push_back(path);
  return paths;
}

//Knight path: L-shaped, two possible routes (vertical->horizontal, horizontal->vertical)
std::vector<Path> generateKnightPath(const BoardCoord& from, const BoardCoord& to) { 
  std::vector<Path> paths;
  
  int rowDiff = to.row - from.row;
  int colDiff = to.col - from.col;
  int rowStep = sign(rowDiff);
  int colStep = sign(colDiff);
  int absRowDiff = std::abs(rowDiff);
  int absColDiff = std::abs(colDiff);

  Path pathVerticalFirst;
  BoardCoord current = from;

  for(int i = 0; i < absRowDiff; i++) { 
    current = BoardCoord(current.row + rowStep, current.col);
    pathVerticalFirst.append(current);
  }

  for(int i = 0; i < absColDiff; i++) { 
    current = BoardCoord(current.row, current.col + colStep);
    pathVerticalFirst.append(current);
  }

  paths.push_back(pathVerticalFirst);

  Path pathHorizontalFirst;
  current = from;
 
  for(int i = 0; i < absColDiff; i++) { 
    current = BoardCoord(current.row, current.col + colStep);
    pathHorizontalFirst.append(current);
  }

  for(int i = 0; i < absRowDiff; i++) { 
    current = BoardCoord(current.row + rowStep, current.col);
    pathHorizontalFirst.append(current);
  }

  paths.push_back(pathHorizontalFirst);

  return paths;
}

//Bishop path: Diagonal only
std::vector<Path> generateBishopPath(const BoardCoord& from, const BoardCoord& to) { 
  std::vector<Path> paths;
  paths.push_back(generateDiagonalPath(from, to));
  return paths;
}

//Rook path: Orthogonal only
std::vector<Path> generateRookPath(const BoardCoord& from, const BoardCoord& to) { 
  std::vector<Path> paths;
  paths.push_back(generateOrthogonalPath(from, to));
  return paths;
}

//Queen path: Orthogonal or Diagonal depending on move
std::vector<Path> generateQueenPath(const BoardCoord& from, const BoardCoord& to) { 
  std::vector<Path> paths;
  int rowDiff = std::abs(to.row - from.row);
  int colDiff = std::abs(to.col - from.col);

  if(rowDiff == colDiff) {
    paths.push_back(generateDiagonalPath(from, to));
  } else {
    paths.push_back(generateOrthogonalPath(from, to));
  }

  return paths;
}

//King path: Single square move
std::vector<Path> generateKingPath(const BoardCoord&, const BoardCoord& to) {
  std::vector<Path> paths;
  Path path;
  path.append(to);
  paths.push_back(path);
  return paths;
}

std::vector<Path> generatePath(const PhysicalMove& move) {
  switch(move.pieceType) { 
    case PAWN:
      return generatePawnPath(move.from, move.to);
    case KNIGHT:
      return generateKnightPath(move.from, move.to);
    case BISHOP:
      return generateBishopPath(move.from, move.to);
    case ROOK:
      return generateRookPath(move.from, move.to);
    case QUEEN:
      return generateQueenPath(move.from, move.to);
    case KING:
      return generateKingPath(move.from, move.to);
    //unknown piece type, try to infer from movement pattern
    default: 
      {
        int rowDiff = std::abs(move.to.row - move.from.row);
        int colDiff = std::abs(move.to.col - move.from.col);
        
        //assume knight
        if((rowDiff == 2 && colDiff == 1) || (rowDiff == 1 && colDiff == 2)) { 
          return generateKnightPath(move.from, move.to);
        } 
        //assume diagonal pattern
        else if(rowDiff == colDiff) { 
          return generateBishopPath(move.from, move.to);
        }
        //assume orthogonal pattern
        else {
          return generateRookPath(move.from, move.to);
        }
      }
  }
}

/************ Blocker Detection Implementation ************/
// find all blocking pieces on a path (excluding dest)
std::vector<BoardCoord> findBlockers(const PhysicalBoard& board, const Path& path) { 
  std::vector<BoardCoord> blockers;

  //check all squares except dest
  //dest may have a capture,but that is handled separately
  for(int i = 0; i < path.length() - 1; i++) { 
    const BoardCoord& square = path.squares[i];
    if(board.isOccupied(square)) { 
      blockers.push_back(square);
    }
  }

  return blockers;
}

//select the path with fewest blockers 
Path selectBestPath(const PhysicalBoard& board, const std::vector<Path>& paths) { 
  if(paths.empty()) {
    return Path();
  }

  Path bestPath = paths[0];
  int fewestBlockers = static_cast<int>(findBlockers(board, paths[0]).size());

  for(size_t i = 1; i < paths.size(); i++) { 
    int numBlockers = static_cast<int>(findBlockers(board, paths[i]).size());
    if(numBlockers < fewestBlockers) { 
      fewestBlockers = numBlockers;
      bestPath = paths[i];
    }
  }

  return bestPath;
}



