/*
 * Kirin Board Interpreter
 * board_interpreter.h - Physical board path planning and state management
 * 
 * This module handles the translation between chess moves and physical
 * gantry movements, including:
 * - Temporary board state tracking during piece relocation
 * - Path generation for different piece types
 * - Blocker detection and parking spot selection
 * - A* pathfinding for clear routes
 * 
 * This is a standalone program that communicates with:
 * - The Kirin chess engine (receives moves + occupancy bitboard)
 * - The gantry controller (sends movement commands)
 */

#ifndef KIRIN_BOARD_INTERPRETER_H
#define KIRIN_BOARD_INTERPRETER_H

#include <cstdint>
#include <vector>
#include <set>

/************ Type Definitions ************/
using Bitboard = uint64_t;

/************ Piece Types (path gen logic) ************/
enum PieceType {
  PAWN,
  KNIGHT,
  BISHOP,
  ROOK,
  QUEEN,
  KING,
  UNKNOWN
};

/************ Physical Board Coordinates ************/
// Represents a physical square on the board (row, col)
// Row 0 = rank 8 (top), Row 7 = rank 1 (bottom)
// Col 0 = file a (left), Col 7 = file h (right)
struct BoardCoord {
  int row;
  int col;

  BoardCoord() : row(0), col(0) {}
  BoardCoord(int r, int c) : row(r), col(c) {}

  bool operator==(const BoardCoord& other) const {
    return row == other.row && col == other.col;
  }

  bool operator!=(const BoardCoord& other) const {
    return !(*this == other);
  }

  //so we can use boardcoord in sets and maps
  bool operator<(const BoardCoord& other) const {
    if(row != other.row) return row < other.row;
    return col < other.col; 
  }

  //convert square to index
  int toSquareIndex() const {
    return row * 8 + col;
  }
 
  //create from square index
  static BoardCoord fromSquareIndex(int square) {
    return BoardCoord(square / 8, square % 8);
  }

  static BoardCoord fromFEN(const char* str);

  void toFEN(char* out) const;
};

/************ Path Representation ************/
// A path is an ordered sequence of squares traversed by a piece
struct Path {
  std::vector<BoardCoord> squares;

  Path() {}

  void append(const BoardCoord& coord) {
    squares.push_back(coord);
  }

  void clear() { 
    squares.clear();
  }

  int length() const {
    return static_cast<int>(squares.size());
  }

  bool empty() const {
    return squares.empty();
  }

  BoardCoord destination() const {
    return squares.empty() ? BoardCoord() : squares.back();
  }
};

/************ Physical Move Representation ************/
// A path is an ordered sequence of squares traversed by a piece
struct PhysicalMove {
  BoardCoord from;
  BoardCoord to;
  PieceType pieceType;
  bool isCapture;

  PhysicalMove() : from(), to(), pieceType(UNKNOWN), isCapture(false) {}

  PhysicalMove(BoardCoord f, BoardCoord t, PieceType pt = UNKNOWN, bool cap = false) :
    from(f), to(t), pieceType(pt), isCapture(cap) {} 
};

/************ Blocker Relocation ************/
// Represents the plan to temporarily relocate a blocking piece
struct RelocationPlan {
  BoardCoord from;
  BoardCoord to;
  
  Path path;

  RelocationPlan() : from(), to(), path() {}
};

/************ Complete Move Execution ************/
// Represents the full plan for executing a chess move on the physical board
struct MovePlan {
  std::vector<RelocationPlan> relocations;
  PhysicalMove primaryMove;
  Path primaryPath;
  std::vector<RelocationPlan> restorations;
  bool isValid;
  const char* errorMessage;

  MovePlan() : relocations(), primaryMove(), primaryPath(), restorations(), isValid(false), errorMessage(nullptr) {}
};

/************ Physical Board State ************/
// Represents the current state of the physical board
// Tracks occupancy for path planning (we don't care what pieces are where)
class PhysicalBoard {
public:
  static const int BOARD_SIZE = 8;

private: 
  //Occupancy bitboard - bit set = square occupied
  // Bit 0 = a8, bit 63 = h1 (matches engine convention)
  Bitboard occupied;

public: 
  PhysicalBoard();

  //initialize from occupancy bitboard, received from engine
  void setOccupancy(Bitboard occ);
  
  Bitboard getOccupancy() const { return occupied; }

  bool isOccupied(const BoardCoord& coord) const;
  bool isOccupied(int row, int col) const;
  bool isEmpty(const BoardCoord& coord) const { return !isOccupied(coord); }
  bool isEmpty(int row, int col) const { return !isOccupied(row, col); }

  static bool isValidSquare(const BoardCoord& coord);
  static bool isValidSquare(int row, int col);

  //temp state modification for planning
  void setOccupied(const BoardCoord& coord);
  void clearSquare(const BoardCoord& coord);
  void movePiece(const BoardCoord& from, const BoardCoord& to);

  //debug
  void print() const;
};

/************ Utility Functions ************/
// Get sign of integer (-1, 0, 1)
inline int sign(int val) { 
  return (val > 0) - (val < 0);
}

//get all 8 neighboring squares (only valid ones)
std::vector<BoardCoord> getNeighbors(const BoardCoord& pos);

//Chebyshev Distance (max of row/col difference, allows for diagonal movement)
int chebyshevDistance(const BoardCoord& from, const BoardCoord& to);

//Manhattan Distance (sum of row/col difference, allows for orthogonal movement)
int manhattanDistance(const BoardCoord& from, const BoardCoord& to);

/************ Bitboard Utilities ************/
inline void bb_setBit(Bitboard& bb, int square) { 
  bb |= (1ULL << square);
}

inline void bb_clearBit(Bitboard& bb, int square) { 
  bb &= ~(1ULL << square);
}

inline bool bb_getBit(Bitboard bb, int square) { 
  return (bb & (1ULL << square)) != 0;
}

int bb_countBits(Bitboard bb);
int bb_getLSB(Bitboard bb);

/************ Path Generation ************/
//Generate all possible paths for a piece to move (some have multiple options)
//Returns a vector of paths - caller should select the best move
std::vector<Path> generatePath(const PhysicalMove& move);

std::vector<Path> generatePawnPath(const BoardCoord& from, const BoardCoord& to);
std::vector<Path> generateKnightPath(const BoardCoord& from, const BoardCoord& to);
std::vector<Path> generateBishopPath(const BoardCoord& from, const BoardCoord& to);
std::vector<Path> generateRookPath(const BoardCoord& from, const BoardCoord& to);
std::vector<Path> generateQueenPath(const BoardCoord& from, const BoardCoord& to);
std::vector<Path> generateKingPath(const BoardCoord& from, const BoardCoord& to);

//Helpers for path generation functions
Path generateDiagonalPath(const BoardCoord& from, const BoardCoord& to);
Path generateOrthogonalPath(const BoardCoord& from, const BoardCoord& to);

/************ Blocker Detection ************/
//Find all blocking pieces on a path (excludes destination)
std::vector<BoardCoord> findBlockers(const PhysicalBoard& board, const Path& path);

//Select the path with the fewest blockers
Path selectBestPath(const PhysicalBoard& board, const std::vector<Path>& paths);

/************ Parking Spot Selection ************/
//find nearest empty square for temporary blocker parking
//use bfs to search outward from blocker position
//excludeSquares: squares that cannot be used (primary path of main piece)
BoardCoord findParkingSpot(const PhysicalBoard& board, const BoardCoord& blockerPos, const std::set<BoardCoord>& excludeSquares);

/************ A* Pathfinding ************/
//Find a clear path from one square to another, avoiding occupied squares
//Used for moving blockers to their parking spot
//Returns empty path if no path exists
Path findClearPath(const PhysicalBoard& board, const BoardCoord& from, const BoardCoord& to);

/************ Move Planning ************/
//generate complete execution plan for a given chess move
//handles blocker relocation, primary move, and restoration
MovePlan planMove(PhysicalBoard& board, const PhysicalMove& move);


/************ Position Setup ************/
Bitboard parseBoardPosition(const char* pieces);
/************ Debug Functions ************/
void printPath(const Path& path);
void printMovePlan(const MovePlan& plan);
#endif
