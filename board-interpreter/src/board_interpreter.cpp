/*
 * Kirin Board Interpreter
 * board_interpreter.cpp - Physical board path planning and state management
 */

#include <cstdio>
#include <cstdlib>
#include <queue>
#include <map>
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

/************ Parking Spot Selection Implementation ************/
//find nearest empty square for temp. parking a blocker
//use bfs to search outward from blocker position
BoardCoord findParkingSpot(const PhysicalBoard& board, const BoardCoord& blockerPos, const std::set<BoardCoord>& excludeSquares) { 
  std::set<BoardCoord> visited;
  std::queue<BoardCoord> queue;
  queue.push(blockerPos);

  while(!queue.empty()) {
    BoardCoord current = queue.front();
    queue.pop();

    if(visited.count(current)) { 
      continue;
    }
    visited.insert(current);

    if(current != blockerPos) {
      if(board.isEmpty(current) && excludeSquares.count(current) == 0) { 
        return current;
      }
    }

    std::vector<BoardCoord> neighbors = getNeighbors(current);
    for(const BoardCoord& neighbor : neighbors) { 
      if(visited.count(neighbor) == 0) {
        queue.push(neighbor);
      }
    }
  }

  //no parking spot found - return invalid coord
  return BoardCoord(-1, -1);
}

/************ A* Pathfinding ************/
struct AStarNode {
  BoardCoord coord;
  int fScore; //gScore + hScore

  AStarNode(const BoardCoord& c, int f) : coord(c), fScore(f) {}

  //for pqueue (min-heap)
  bool operator>(const AStarNode& other) const {
    return fScore > other.fScore;
  }
};

Path findClearPath(const PhysicalBoard& board, const BoardCoord& from, const BoardCoord& to) {
  //priority queue: min-heap by fScore
  std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;

  std::map<BoardCoord, BoardCoord> cameFrom;
  std::map<BoardCoord, int> gScore;
  std::set<BoardCoord> closedSet;
  
  gScore[from] = 0;
  int hScore = chebyshevDistance(from, to);
  openSet.push(AStarNode(from, hScore));

  while(!openSet.empty()) { 
    AStarNode currentNode = openSet.top();
    openSet.pop();
    BoardCoord current = currentNode.coord;

    //skip if already processed
    if(closedSet.count(current)) {
      continue;
    }

    closedSet.insert(current);

    //reached dest
    if(current == to) {
      //reconstruct path
      Path path;
      BoardCoord step = to;
      std::vector<BoardCoord> reversePath;
      
      while(step != from) {
        reversePath.push_back(step);
        step = cameFrom[step];
      }

      for(int i = static_cast<int>(reversePath.size()) - 1; i >= 0; i--) { 
        path.append(reversePath[i]);
      }

      return path;
    }
    
    //explore neighbors
    std::vector<BoardCoord> neighbors = getNeighbors(current);
    for(const BoardCoord& neighbor : neighbors) { 
      //skip if already processed
      if(closedSet.count(neighbor)) {
        continue;
      }
      
      //skip if occupied (unless dest)
      if(board.isOccupied(neighbor) && neighbor != to) {
        continue;
      }

      int tentativeG = gScore[current] + 1;

      //check if this path to neighbor is better
      if(gScore.count(neighbor) == 0 || tentativeG < gScore[neighbor]) {
        cameFrom[neighbor] = current;
        gScore[neighbor] = tentativeG;
        int h = chebyshevDistance(neighbor, to);
        openSet.push(AStarNode(neighbor, tentativeG + h));
      }
    }
  }
  
  //no path found
  return Path();
}

/************ Move Planning ************/

static const int MAX_RELOCATION_DEPTH = 16;

static bool planBlockerRelocation(PhysicalBoard& board, const BoardCoord& blockerPos, std::set<BoardCoord>& excludeSquares, std::vector<RelocationPlan>& relocations, int depth) {
  if(depth > MAX_RELOCATION_DEPTH) { 
    return false; //prevent infinite recursion
  }
   
  //find parking spot for this blocker
  BoardCoord parkingSpot = findParkingSpot(board, blockerPos, excludeSquares);

  if(!PhysicalBoard::isValidSquare(parkingSpot)) { 
    return false;
  }

  //try to find a clear path to the parking spot
  Path blockerPath = findClearPath(board, blockerPos, parkingSpot);

  if(!blockerPath.empty()) { 
    //path is clear, we can move directly
    RelocationPlan relocation;
    relocation.from = blockerPos;
    relocation.to = parkingSpot;
    relocation.path = blockerPath;
    relocations.push_back(relocation);

    board.movePiece(blockerPos, parkingSpot);
    excludeSquares.insert(parkingSpot);
    return true;
  }

  //path is blocked if it gets here - we need to move blockers out of the way of the blocker
  //find all blockers between us and the parking spot
  std::vector<BoardCoord> pathBlockers;

  //check each square along possible paths to the parking spot
  //use bfs to find occupied squares that are between us and dest
  std::set<BoardCoord> checked;
  std::queue<BoardCoord> toCheck;

  toCheck.push(blockerPos);

  while(!toCheck.empty() && pathBlockers.size() < 8) { 
    BoardCoord current = toCheck.front();
    toCheck.pop();

    if(checked.count(current)) continue;
    checked.insert(current);

    //check neighbors that are closer to or the same distance from parking spot
    int currentDist = chebyshevDistance(current, parkingSpot);
    std::vector<BoardCoord> neighbors = getNeighbors(current);

    for(const BoardCoord& neighbor : neighbors) { 
      if(checked.count(neighbor)) continue;

      int neighborDist = chebyshevDistance(neighbor, parkingSpot);

      //only consider squares that get us closer or maintain distance
      if(neighborDist <= currentDist) { 
        if(neighbor == parkingSpot) { 
          continue; //neighbor is dest
        }

        if(board.isOccupied(neighbor) && excludeSquares.count(neighbor) == 0) {
          bool alreadyFound = false;
          for(const BoardCoord& pb : pathBlockers) { 
            if(pb == neighbor) {
              alreadyFound = true;
              break;
            }
          }
          if(!alreadyFound) {
            pathBlockers.push_back(neighbor);
          }
        }
        
        toCheck.push(neighbor);
      }
    }
  }

  if(pathBlockers.empty()) {
    //no blockers found but path still not clear - try alternative parking spots
    std::set<BoardCoord> triedSpots = excludeSquares;
    triedSpots.insert(parkingSpot);

    for(int attempts = 0; attempts < 5; attempts++) { 
      BoardCoord altSpot = findParkingSpot(board, blockerPos, triedSpots);
      if(!PhysicalBoard::isValidSquare(altSpot)) break;

      Path altPath = findClearPath(board, blockerPos, altSpot);
      if(!altPath.empty()) {
        RelocationPlan relocation;
        relocation.from = blockerPos;
        relocation.to = altSpot;
        relocation.path = altPath;
        relocations.push_back(relocation);

        board.movePiece(blockerPos, altSpot);
        excludeSquares.insert(altSpot);
        return true;
      }

      triedSpots.insert(altSpot);
    }
    
    return false;
  }

  //recursively relocate blockers in our path
  for(const BoardCoord& pathBlocker : pathBlockers) { 
    //skip if blocker was already moved
    if(!board.isOccupied(pathBlocker)) continue;

    if(!planBlockerRelocation(board, pathBlocker, excludeSquares, relocations, depth + 1)) {
      return false;
    }
  }

  //try again to find a clear path
  blockerPath = findClearPath(board, blockerPos, parkingSpot);

  if(blockerPath.empty()) {
    //try alternative parking spots
    std::set<BoardCoord> triedSpots = excludeSquares;
    triedSpots.insert(parkingSpot);

    for(int attempts = 0; attempts < 5; attempts++) { 
      BoardCoord altSpot = findParkingSpot(board, blockerPos, triedSpots);
      if(!PhysicalBoard::isValidSquare(altSpot)) break;

      blockerPath = findClearPath(board, blockerPos, altSpot);
      if(!blockerPath.empty()) {
        parkingSpot = altSpot;
        break;
      }
      
      triedSpots.insert(altSpot);
    }
  }

  if(blockerPath.empty()) {
    return false; //still cant find path :'(
  }

  RelocationPlan relocation;
  relocation.from = blockerPos;
  relocation.to = parkingSpot;
  relocation.path = blockerPath;
  relocations.push_back(relocation);

  board.movePiece(blockerPos, parkingSpot);
  excludeSquares.insert(parkingSpot);
  return true;
}

//main function: generate complete execution plan for a chess move
MovePlan planMove(PhysicalBoard& board, const PhysicalMove& move) {
  MovePlan plan;
  plan.primaryMove = move;

  //step 1: generate possible paths for primary piece
  std::vector<Path> paths = generatePath(move);

  if(paths.empty()) {
    plan.isValid = false;
    plan.errorMessage = "No paths generated for move";
    return plan;
  }

  //step 2: select path with fewest blockers
  plan.primaryPath = selectBestPath(board, paths);

  if(plan.primaryPath.empty()) { 
    plan.isValid = false;
    plan.errorMessage = "No valid path found";
    return plan;
  }

  //step 3: find all blockers on the chosen path
  std::vector<BoardCoord> blockerSquares = findBlockers(board, plan.primaryPath);

  //step 4: build set of excluded squares (primary piece path squares)
  std::set<BoardCoord> excludeSquares;
  for(const BoardCoord& square : plan.primaryPath.squares) { 
    excludeSquares.insert(square);
  }

  //exlcude starting square of primary piece
  excludeSquares.insert(move.from);

  //step 5: plan relocation for each blocker
  // work on a copy of the board to track temporary state
  PhysicalBoard tempBoard = board;
  for(const BoardCoord& blockerPos : blockerSquares) { 
    if(!planBlockerRelocation(tempBoard, blockerPos, excludeSquares, plan.relocations, 0)) {
      plan.isValid = false;
      plan.errorMessage = "Cannot relocate blocker (position too constrained)";
      return plan;
    }
  }

  //step 6: build restorations(reverse order of relocations? (hopefully this approach works))
  for(int i = static_cast<int>(plan.relocations.size()) - 1; i >= 0; i--) { 
    RelocationPlan restoration;
    restoration.from = plan.relocations[i].to; //from parking spot
    restoration.to = plan.relocations[i].from; //to original position

    //path will be computed during execution
    plan.restorations.push_back(restoration);
  }

  plan.isValid = true;
  plan.errorMessage = nullptr;
  return plan;
}

/************ Debug Functions ************/

// Print a path for debugging
void printPath(const Path& path) {
  printf("Path (%d squares): ", path.length());
  for(int i = 0; i < path.length(); i++) {
    char fen[3];
    path.squares[i].toFEN(fen);
    printf("%s ", fen);
  }
  printf("\n");
}

// Print a move plan for debugging
void printMovePlan(const MovePlan& plan) {
  printf("\n=== Move Plan ===\n");
  
  if(!plan.isValid) {
    printf("INVALID: %s\n", plan.errorMessage ? plan.errorMessage : "Unknown error");
    return;
  }
  
  char fromFen[3], toFen[3];
  plan.primaryMove.from.toFEN(fromFen);
  plan.primaryMove.to.toFEN(toFen);
  printf("Primary Move: %s -> %s\n", fromFen, toFen);
  
  printf("Primary Path: ");
  printPath(plan.primaryPath);
  
  printf("\nRelocations (%zu):\n", plan.relocations.size());
  for(size_t i = 0; i < plan.relocations.size(); i++) {
    plan.relocations[i].from.toFEN(fromFen);
    plan.relocations[i].to.toFEN(toFen);
    printf("  %zu. %s -> %s: ", i + 1, fromFen, toFen);
    printPath(plan.relocations[i].path);
  }
  
  printf("\nRestorations (%zu):\n", plan.restorations.size());
  for(size_t i = 0; i < plan.restorations.size(); i++) {
    plan.restorations[i].from.toFEN(fromFen);
    plan.restorations[i].to.toFEN(toFen);
    printf("  %zu. %s -> %s (path computed at execution time)\n", i + 1, fromFen, toFen);
  }
  
  printf("=================\n\n");
}
