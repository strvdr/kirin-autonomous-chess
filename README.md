# Kirin Autonomous Chess System

```
      ,  ,
      \\ \\                 
      ) \\ \\    _p_        
       )^\))\))  /  *\      KIRIN CHESS ENGINE v0.3
        \_|| || / /^`-'     
 __       -\ \\--/ /          Author:      Strydr Silverberg
<'  \\___/   ___. )'                        Colorado School of Mines, Class of 2026
      `====\ )___/\\            
           //     `"            Hardware:    Colin Dake
           \\    /  \           
```

> **2026 Colorado School of Mines ProtoFund Recipient**  
> Team: Strydr Silverberg · Colin Dake

---

## Overview

Kirin is an autonomous chess system that delivers a complete single-player experience by pairing a custom chess engine with a physically-actuated board. A two-axis gantry equipped with an electromagnet moves pieces across the board independently, eliminating the need for a human opponent while preserving the tactile satisfaction of over-the-board play.

The system is built around three tightly-coupled layers:

- **Chess Engine** — Bitboard-based move generation, Zobrist hashing, and alpha-beta search with iterative deepening
- **Board Interpreter** — Translates engine moves into physical paths, handling blocker detection, piece parking, and A\* pathfinding
- **Gantry Controller** — Generates and dispatches G-code to a GRBL-based motion controller via serial

---

## Hardware

```
         22" rail travel
|<------------------------------->|
|                                 |
|  +-+ +---------------------+ +-+|
|  |B| |                     | |W||
|  |L| |     16" board       | |H||
|  |A| |                     | |I||
|  |C| |      8x8 grid       | |T||
|  |K| |                     | |E||
|  +-+ +---------------------+ +-+|
|  3.0"        16"           3.0" |
|<--->|<------------------->|<--->|
   ^                            ^
   |                            |
Black captures              White captures
X = 1.5" (center)          X = 20.5" (center)
Y = 4" to 18"              Y = 4" to 18"
```

| Component | Specification |
|---|---|
| Rail travel | 22 inches |
| Board size | 16 × 16 inches |
| Square size | 2 × 2 inches |
| Capture zones | 3-inch margins, left (black) and right (white) |
| Motion controller | GRBL 1.1 |
| Serial connection | 115200 baud, 8N1 |
| Piece actuation | Electromagnet (M3/M5 spindle commands) |

---

## Software Architecture

```
main.cpp
  ├── UCI Mode           — Standard UCI protocol for chess GUIs
  ├── Physical Mode      — Live game with gantry hardware
  ├── Simulation Mode    — Interactive, no hardware (manual UCI moves)
  └── Dry Run Mode       — Engine-vs-engine, full G-code audit, no hardware

GameController           — Orchestrates engine ↔ physical board
  ├── PhysicalBoard      — Bitboard-based physical state tracker
  └── GrblController     — Serial comms, G-code dispatch, capture tracking

BoardInterpreter         — Path planning
  ├── Piece-specific paths (sliding, knight L-paths, etc.)
  ├── Blocker detection & temporary piece parking
  └── A* pathfinding for clear routes

Engine
  ├── Bitboard move generation (attacks.cpp, movegen.cpp)
  ├── Zobrist hashing (zobrist.cpp)
  ├── Alpha-beta search with iterative deepening (search.cpp)
  └── Evaluation (evaluation.cpp)
```

### Key Data Flow

```
searchPosition(depth)
    → bestMove (encoded int)
        → engineToPhysicalMove()
            → PhysicalMove { from, to, pieceType, isCapture, ... }
                → planMove()
                    → Path (sequence of BoardCoords)
                        → generateMovePlanGcode()
                            → std::vector<std::string> G-code commands
                                → GrblController::sendCommands()
```

---

## Building

The project uses CMake (3.10+) with a C++17 toolchain. Serial hardware support is conditionally compiled via the `HAS_SERIAL` preprocessor flag (set automatically on Linux).

```
kirin/
├── CMakeLists.txt
├── src/
│   ├── engine/         Chess engine core
│   ├── hardware/       Physical board control
│   └── main.cpp
└── tests/
```

```bash
git clone https://github.com/strvdr/first-move-robotics.git
cd first-move-robotics/kirin
mkdir build && cd build

# Configure (Release by default)
cmake ..

# Configure Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build .

# Run tests
ctest
```

The CMake build produces three targets:

| Target | Description |
|---|---|
| `kirin` | Main executable |
| `kirin_engine` | Static library — chess engine core |
| `kirin_hardware` | Static library — board interpreter + gantry controller (links engine) |

Release builds are compiled with `-O3 -march=native -flto`. Debug builds use `-g -O0 -DDEBUG`.

---

## Usage

```
Usage: kirin [OPTIONS]

Options:
  (no args)                  Run in UCI mode (for chess GUIs)
  --physical PORT            Connect to gantry on serial PORT
                               e.g.  kirin --physical /dev/ttyUSB0
  --simulate                 Interactive simulation (no hardware)
  --dryrun [MOVES [DEPTH]]   Engine-vs-engine dry run, prints all G-code
                               MOVES  = max moves per side (default 40)
                               DEPTH  = search depth     (default 4)
                               e.g.  kirin --dryrun 20 5
  --help, -h                 Show this help message
```

### UCI Mode

Exposes the engine over standard UCI for use with chess GUIs (Arena, Cute Chess, etc.) or for automated testing via the `go` / `position` / `ucinewgame` command set.

```bash
./kirin
```

### Physical Mode

Connects to the gantry over serial, homes the axes, and enters the main game loop. The engine plays one side; the human enters moves in UCI notation.

```bash
./kirin --physical /dev/ttyUSB0
```

### Simulation Mode

Interactive move-by-move walkthrough. Enter moves in UCI format and inspect the generated move plans and G-code — no hardware required.

```bash
./kirin --simulate
```

### Dry Run Mode

Plays a complete engine-vs-engine game automatically and prints every G-code command that would be sent to the gantry, with inline annotations. Use this to audit the full command sequence before powering the hardware for the first time.

```bash
./kirin --dryrun          # 40 moves per side, depth 4
./kirin --dryrun 20 5     # 20 moves per side, depth 5
```

Example dry-run output:

```
══════════════════════════════════════════════════════════════
  Move 1.  e2e4
══════════════════════════════════════════════════════════════
  $H                              ; HOME (seek limit switches)
  G1 X4.00 Y14.00                 ; MOVE
  M3                              ; MAGNET ON
  G4 P0.5                         ; DWELL (settle)
  G1 X4.00 Y10.00                 ; MOVE
  M5                              ; MAGNET OFF
  ...
```

---

## Module Reference

**`src/main.cpp`** — Entry point, run-mode dispatch

**`src/engine/`**

| File | Responsibility |
|---|---|
| `attacks.h/.cpp` | Attack table generation (magic bitboards) |
| `bitboard.h/.cpp` | Bitboard primitives and utilities |
| `movegen.h/.cpp` | Legal move generation |
| `search.h/.cpp` | Alpha-beta search, iterative deepening |
| `evaluation.h/.cpp` | Static position evaluation |
| `position.h/.cpp` | Board state, FEN parsing |
| `zobrist.h/.cpp` | Zobrist hashing for transposition table |
| `uci.h/.cpp` | UCI protocol handler |
| `types.h/.cpp` | Shared type definitions |
| `utils.h/.cpp` | Miscellaneous helpers |
| `engine.h` | Umbrella include for engine internals |

**`src/hardware/`**

| File | Responsibility |
|---|---|
| `game_controller.h/.cpp` | Engine ↔ hardware integration, special move handling |
| `gantry_controller.h/.cpp` | G-code generation, GRBL serial comms, capture zone tracking |
| `board_interpreter.h/.cpp` | Path planning, blocker detection, A\* routing |

**`tests/`**

| File | Coverage |
|---|---|
| `game_controller_test.cpp` | Move encoding/decoding, coordinate conversion, physical board state, special moves |
| `board_interpreter_test.cpp` | Path planning, blocker handling, coordinate math |
| `captured_piece_test.cpp` | Capture zone slot assignment and tracking |

---

## Coordinate Systems

The codebase uses two coordinate systems that are explicitly converted at the engine/hardware boundary.

**Engine squares** (0–63): `a8 = 0`, `b8 = 1`, ..., `h1 = 63`

**BoardCoord** (row, col): `row 0 = rank 8`, `col 0 = file a` — matches the physical board orientation as viewed from white's side.

**Physical position** (inches): origin at the gantry home position (bottom-left); X increases right, Y increases up.

---

## Capture Zone Layout

Captured pieces are stored in two zones flanking the board. Each zone has two columns (pawns / pieces) with 2-inch vertical spacing matching the board squares.

| Zone | X center | Y range | Contents |
|---|---|---|---|
| Black captures (left) | 1.5" | 4" – 18" | White pieces taken by black |
| White captures (right) | 20.5" | 4" – 18" | Black pieces taken by white |

Pawns and back-rank pieces occupy separate columns within each zone. The controller tracks slots independently for white pawns, white pieces, black pawns, and black pieces.

---

## Development Status

| Feature | Status |
|---|---|
| Bitboard engine & UCI | ✅ Complete |
| G-code generation pipeline | ✅ Complete |
| Dry run mode (engine-vs-engine) | ✅ Verified |
| Simulation mode | ✅ Complete |
| Serial / GRBL integration | ✅ Complete |
| Physical board testing | 🔧 In progress |
| En passant path planning | 🔧 In progress |
| Promotion handling | 🔧 In progress |

---

## Acknowledgments

Funded by the **2026 Colorado School of Mines ProtoFund**.  
This prototype applies engineering principles and computer science fundamentals developed through coursework at the Colorado School of Mines.

---

*Kirin v0.3 — Colorado School of Mines, 2026*
