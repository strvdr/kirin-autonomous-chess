/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    attacks.cpp - Attack tables and magic bitboard lookups
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
*          ,  ,
*              \\ \\                 
*              ) \\ \\    _p_        
*               )^\))\))  /  *\      KIRIN CHESS ENGINE v0.3
*                \_|| || / /^`-'     
*       __       -\ \\--/ /          Author:      Strydr Silverberg
*     <'  \\___/   ___. )'                        Colorado School of Mines, Class of 2026
*          `====\ )___/\\            
*               //     `"            Hardware:    Colin Dake
*               \\    /  \           
*               `"                   
*    ══════════════════════════════════════════════════════════════════════════════
*    PROJECT OVERVIEW
*    ══════════════════════════════════════════════════════════════════════════════
*    
*    Kirin is the computational engine powering an autonomous chess system
*    designed to deliver a complete single-player experience. By integrating
*    intelligent move generation with a physical board capable of independent
*    piece manipulation, the system eliminates the need for a human opponent
*    while preserving the tactile satisfaction of over-the-board play.
*    
*    ══════════════════════════════════════════════════════════════════════════════
*    FUNDING & ACKNOWLEDGMENTS
*    ══════════════════════════════════════════════════════════════════════════════
*    
*    2026 Colorado School of Mines ProtoFund Recipient
*    Team: Strydr Silverberg, Colin Dake
*    
*    This prototype represents the practical application of engineering
*    principles and computer science fundamentals developed through our
*    coursework at the Colorado School of Mines.
*/

#include <cstdio>
#include <cstring>
#include <string>

// Engine includes
#include "engine.h"

// Hardware includes  
#include "game_controller.h"

/************ Run Mode ************/
enum RunMode {
    MODE_UCI,           // Standard UCI protocol (for GUI or testing)
    MODE_PHYSICAL,      // Physical board with hardware
    MODE_SIMULATION,    // Interactive simulation (no hardware, manual moves)
    MODE_DRYRUN         // Automatic engine-vs-engine dry run (no hardware, no input)
};

/************ Forward Declarations ************/
void printHelp();
void runPhysicalMode(const char* port);
void runSimulationMode();
void runDryRunMode(int maxMoves, int searchDepth);



/************ Help Message ************/
void printHelp() {
    printf("\n");
    printf("Kirin Chess Engine v0.3\n");
    printf("══════════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("Usage: kirin [OPTIONS]\n");
    printf("\n");
    printf("Options:\n");
    printf("  (no args)                  Run in UCI mode (for chess GUIs)\n");
    printf("  --physical PORT            Run with physical board on serial PORT\n");
    printf("                             Example: kirin --physical /dev/ttyUSB0\n");
    printf("  --simulate                 Interactive simulation (no hardware)\n");
    printf("  --dryrun [MOVES [DEPTH]]   Automatic engine-vs-engine dry run\n");
    printf("                             Prints all G-code without hardware\n");
    printf("                             MOVES: max moves per side (default 40)\n");
    printf("                             DEPTH: search depth (default 4)\n");
    printf("                             Example: kirin --dryrun 20 5\n");
    printf("  --help, -h                 Show this help message\n");
    printf("\n");
    printf("UCI Mode:\n");
    printf("  Standard UCI protocol for use with chess GUIs like Arena,\n");
    printf("  Cute Chess, or for automated testing.\n");
    printf("\n");
    printf("Physical Mode:\n");
    printf("  Connects to the gantry controller and operates the physical\n");
    printf("  chess board. Requires serial port connection to GRBL controller.\n");
    printf("\n");
    printf("Simulation Mode:\n");
    printf("  Interactive move-by-move simulation. Enter moves in UCI format\n");
    printf("  and inspect the generated G-code and move plans.\n");
    printf("\n");
    printf("Dry Run Mode:\n");
    printf("  Plays a complete engine-vs-engine game automatically. Every\n");
    printf("  G-code command that would be sent to the gantry is printed with\n");
    printf("  annotations. Use this to audit the full command sequence before\n");
    printf("  powering the hardware for the first time.\n");
    printf("\n");
}

/************ Physical Board Game Loop ************/
void runPhysicalMode(const char* port) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           KIRIN AUTONOMOUS CHESS SYSTEM v0.3                 ║\n");
    printf("║                  Physical Board Mode                         ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    GameController controller;
    
    // Connect to hardware
    printf("Connecting to gantry on %s...\n", port);
    if (!controller.connectHardware(port)) {
        printf("Failed to connect. Check port and try again.\n");
        return;
    }
    
    // Home the gantry
    printf("Homing gantry...\n");
    if (!controller.homeGantry()) {
        printf("Homing failed. Check hardware and try again.\n");
        return;
    }
    
    // Main command loop
    char input[256];
    printf("\nReady. Type 'help' for commands.\n\n");
    
    while (true) {
        printf("kirin> ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == nullptr) {
            break;
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        // Parse command
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            printf("Goodbye!\n");
            break;
        }
        else if (strcmp(input, "help") == 0) {
            printf("\nCommands:\n");
            printf("  newgame [white|black]  - Start new game (engine plays specified color)\n");
            printf("  move <move>            - Make a move (e.g., 'move e2e4')\n");
            printf("  go                     - Engine makes a move\n");
            printf("  board                  - Display current position\n");
            printf("  physical               - Display physical board state\n");
            printf("  fen <fen>              - Set position from FEN string\n");
            printf("  home                   - Home the gantry\n");
            printf("  test                   - Run hardware test sequence\n");
            printf("  quit                   - Exit program\n");
            printf("\n");
        }
        else if (strncmp(input, "newgame", 7) == 0) {
            // Parse color argument
            bool engineWhite = true;  // Default
            if (strstr(input, "black")) {
                engineWhite = false;
            }
            
            printf("Setting up new game (engine plays %s)...\n", 
                   engineWhite ? "white" : "black");
            
            // Reset engine to starting position
            parseFEN(startPosition);
            
            // Set up physical board
            if (!controller.setupNewGame()) {
                printf("Failed to set up board.\n");
                continue;
            }
            
            controller.startGame(engineWhite);
            
            printBoard();
            printf("\nGame started. %s to move.\n", 
                   engineWhite ? "Engine (white)" : "You (white)");
            
            // If engine plays white, make first move
            if (engineWhite) {
                printf("Engine thinking...\n");
                searchPosition(6);  // depth 6
                
                // Get best move from search
                extern int bestMove;
                if (bestMove) {
                    // Make the move in the engine
                    makeMove(bestMove, allMoves);
                    
                    // Execute on physical board
                    if (controller.executeEngineMove(bestMove)) {
                        printf("Engine plays: ");
                        // Print move (simplified)
                        int source = getSource(bestMove);
                        int target = getTarget(bestMove);
                        printf("%c%d%c%d\n", 
                               'a' + (source % 8), 8 - (source / 8),
                               'a' + (target % 8), 8 - (target / 8));
                        printBoard();
                    } else {
                        printf("Hardware error executing move!\n");
                    }
                }
            }
        }
        else if (strncmp(input, "move ", 5) == 0) {
            const char* moveStr = input + 5;
            
            // Skip whitespace
            while (*moveStr == ' ') moveStr++;
            
            // Parse and validate the move
            int move = parseMove(moveStr);
            if (move == 0) {
                printf("Invalid or illegal move: %s\n", moveStr);
                continue;
            }
            
            // Make the move in the engine
            if (!makeMove(move, allMoves)) {
                printf("Illegal move: %s\n", moveStr);
                continue;
            }
            
            // Execute on physical board
            if (!controller.executeEngineMove(move)) {
                printf("Hardware error! Move made in engine but failed on board.\n");
                continue;
            }
            
            printf("Move: %s\n", moveStr);
            printBoard();
            
            // Check for game over
            if (controller.isGameOver()) {
                printf("\nGame over!\n");
                continue;
            }
            
            // Engine responds
            printf("Engine thinking...\n");
            searchPosition(6);
            
            extern int bestMove;
            if (bestMove) {
                makeMove(bestMove, allMoves);
                
                if (controller.executeEngineMove(bestMove)) {
                    printf("Engine plays: ");
                    int source = getSource(bestMove);
                    int target = getTarget(bestMove);
                    printf("%c%d%c%d", 
                           'a' + (source % 8), 8 - (source / 8),
                           'a' + (target % 8), 8 - (target / 8));
                    if (getPromoted(bestMove)) {
                        int promo = getPromoted(bestMove) % 6;
                        const char promoChars[] = "pnbrqk";
                        printf("%c", promoChars[promo]);
                    }
                    printf("\n");
                    printBoard();
                    
                    if (controller.isGameOver()) {
                        printf("\nGame over!\n");
                    }
                } else {
                    printf("Hardware error executing engine move!\n");
                }
            } else {
                printf("No legal moves - game over!\n");
            }
        }
        else if (strcmp(input, "go") == 0) {
            printf("Engine thinking...\n");
            searchPosition(6);
            
            extern int bestMove;
            if (bestMove) {
                makeMove(bestMove, allMoves);
                
                if (controller.executeEngineMove(bestMove)) {
                    printf("Engine plays: ");
                    int source = getSource(bestMove);
                    int target = getTarget(bestMove);
                    printf("%c%d%c%d\n", 
                           'a' + (source % 8), 8 - (source / 8),
                           'a' + (target % 8), 8 - (target / 8));
                    printBoard();
                } else {
                    printf("Hardware error!\n");
                }
            } else {
                printf("No legal moves!\n");
            }
        }
        else if (strcmp(input, "board") == 0) {
            printBoard();
        }
        else if (strcmp(input, "physical") == 0) {
            printPhysicalBoard(controller.getPhysicalBoard());
        }
        else if (strncmp(input, "fen ", 4) == 0) {
            const char* fen = input + 4;
            while (*fen == ' ') fen++;
            parseFEN(fen);
            controller.syncWithEngine();
            printBoard();
        }
        else if (strcmp(input, "home") == 0) {
            printf("Homing gantry...\n");
            if (controller.homeGantry()) {
                printf("Done.\n");
            } else {
                printf("Homing failed.\n");
            }
        }
        else if (strcmp(input, "test") == 0) {
            printf("Running hardware test...\n");
            Gantry::GrblController& gantry = controller.getGantry();
            
            // Move to center of board
            printf("Moving to e4...\n");
            gantry.moveTo(BoardCoord(4, 4));  // e4
            
            printf("Moving to d5...\n");
            gantry.moveTo(BoardCoord(3, 3));  // d5
            
            printf("Testing magnet...\n");
            gantry.setMagnet(true);
            gantry.sendCommand(Gantry::dwell(500));
            gantry.setMagnet(false);
            
            printf("Returning home...\n");
            gantry.home();
            
            printf("Test complete.\n");
        }
        else if (strlen(input) > 0) {
            printf("Unknown command: %s (type 'help' for commands)\n", input);
        }
    }
}

/************ Simulation Mode ************/
void runSimulationMode() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           KIRIN AUTONOMOUS CHESS SYSTEM v0.3                 ║\n");
    printf("║                   Simulation Mode                            ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Running in simulation mode - no hardware connection.\n");
    printf("G-code will be generated but not sent to any device.\n");
    printf("\n");
    
    // Set up starting position
    parseFEN(startPosition);
    
    // Create physical board tracker
    PhysicalBoard physBoard;
    extern unsigned long long occupancy[3];
    physBoard.setOccupancy(occupancy[both]);
    
    char input[256];
    printf("Ready. Enter moves in UCI format (e.g., 'e2e4'), or 'quit' to exit.\n\n");
    
    while (true) {
        printBoard();
        printf("\nphysical> ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == nullptr) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "quit") == 0) {
            break;
        }
        
        if (strlen(input) < 4) {
            printf("Enter a move like 'e2e4' or 'quit'\n");
            continue;
        }
        
        // Parse move
        int move = parseMove(input);
        if (move == 0) {
            printf("Invalid or illegal move: %s\n", input);
            continue;
        }
        
        // Convert to physical move
        PhysicalMove physMove = engineToPhysicalMove(move);
        
        printf("\n=== Move: %s ===\n", input);
        printf("From: (%d, %d) -> To: (%d, %d)\n", 
               physMove.from.row, physMove.from.col,
               physMove.to.row, physMove.to.col);
        printf("Piece type: %d, Capture: %s\n", 
               physMove.pieceType, physMove.isCapture ? "yes" : "no");
        
        // Plan the move
        MovePlan plan = planMove(physBoard, physMove);
        
        if (!plan.isValid) {
            printf("Error planning move: %s\n", 
                   plan.errorMessage ? plan.errorMessage : "unknown");
            continue;
        }
        
        // Print the plan
        printMovePlan(plan);
        
        // Generate G-code
        bool capturedIsWhite = false;
        PieceType capturedType = UNKNOWN;
        
        if (getCapturedPiece(move)) {
            int captured = getCapturedPiece(move);
            capturedType = engineToPhysicalPiece(captured);
            capturedIsWhite = captured < 6;
        }
        
        std::vector<std::string> gcode = Gantry::generateMovePlanGcode(
            plan, capturedIsWhite, capturedType);
        
        printf("\n--- Generated G-code ---\n");
        for (const auto& cmd : gcode) {
            printf("  %s\n", cmd.c_str());
        }
        printf("------------------------\n");
        
        // Make the move in the engine
        makeMove(move, allMoves);
        
        // Update physical board
        if (physMove.isCapture) {
            physBoard.clearSquare(physMove.to);
        }
        physBoard.movePiece(physMove.from, physMove.to);
        
        // Engine response
        printf("\nEngine thinking...\n");
        searchPosition(6);
        
        extern int bestMove;
        if (bestMove) {
            printf("Engine's best move: ");
            int source = getSource(bestMove);
            int target = getTarget(bestMove);
            printf("%c%d%c%d", 
                   'a' + (source % 8), 8 - (source / 8),
                   'a' + (target % 8), 8 - (target / 8));
            if (getPromoted(bestMove)) {
                int promo = getPromoted(bestMove) % 6;
                const char promoChars[] = "pnbrqk";
                printf("%c", promoChars[promo]);
            }
            printf("\n");
            
            // Execute engine move
            PhysicalMove enginePhysMove = engineToPhysicalMove(bestMove);
            MovePlan enginePlan = planMove(physBoard, enginePhysMove);
            
            if (enginePlan.isValid) {
                printMovePlan(enginePlan);
                
                std::vector<std::string> engineGcode = Gantry::generateMovePlanGcode(
                    enginePlan, 
                    getCapturedPiece(bestMove) ? (getCapturedPiece(bestMove) < 6) : false,
                    getCapturedPiece(bestMove) ? engineToPhysicalPiece(getCapturedPiece(bestMove)) : UNKNOWN);
                
                printf("\n--- Engine Move G-code ---\n");
                for (const auto& cmd : engineGcode) {
                    printf("  %s\n", cmd.c_str());
                }
                printf("--------------------------\n");
            }
            
            makeMove(bestMove, allMoves);
            
            if (enginePhysMove.isCapture) {
                physBoard.clearSquare(enginePhysMove.to);
            }
            physBoard.movePiece(enginePhysMove.from, enginePhysMove.to);
        }
    }
    
    printf("Goodbye!\n");
}

/************ Dry Run Mode ************/
// Helper: print a move in UCI notation (e.g., "e2e4", "e7e8q")
static void printUciMove(int engineMove) {
    int source = getSource(engineMove);
    int target = getTarget(engineMove);
    printf("%c%d%c%d",
           'a' + (source % 8), 8 - (source / 8),
           'a' + (target % 8), 8 - (target / 8));
    if (getPromoted(engineMove)) {
        int promo = getPromoted(engineMove) % 6;
        const char promoChars[] = "pnbrqk";
        printf("%c", promoChars[promo]);
    }
}

// Helper: describe a move type for the header banner
static const char* describeMoveType(int engineMove) {
    if (getCastle(engineMove))     return "CASTLE";
    if (getEnpassant(engineMove))  return "EN PASSANT";
    if (getPromoted(engineMove))   return "PROMOTION";
    if (getCapture(engineMove))    return "CAPTURE";
    return "normal";
}

// Helper: piece name from engine piece index
static const char* pieceNameFromEngine(int piece) {
    static const char* names[] = {
        "White Pawn", "White Knight", "White Bishop",
        "White Rook",  "White Queen",  "White King",
        "Black Pawn", "Black Knight", "Black Bishop",
        "Black Rook",  "Black Queen",  "Black King"
    };
    if (piece < 0 || piece > 11) return "Unknown";
    return names[piece];
}

void runDryRunMode(int maxMoves, int searchDepth) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           KIRIN AUTONOMOUS CHESS SYSTEM v0.3                 ║\n");
    printf("║                    Dry Run Mode                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Engine-vs-engine game. Max %d moves per side, search depth %d.\n",
           maxMoves, searchDepth);
    printf("No hardware connection. All G-code printed for inspection.\n");
    printf("\n");

    // --- Set up engine state ---
    parseFEN(startPosition);

    // --- Set up GameController in dry-run mode ---
    GameController controller;
    controller.getGantry().enableDryRun();
    controller.startGame(true);   // Engine plays both sides; white first

    // Dry-run the new-game piece setup (all 32 pieces move from capture zones)
    printf("══════════════════════════════════════════════════════════════\n");
    printf("  INITIAL SETUP — Moving pieces from storage to board\n");
    printf("══════════════════════════════════════════════════════════════\n");
    controller.getGantry().home();
    controller.setupNewGame();   // This calls gantry.setupNewGame() → sendCommands()
    printf("\n");

    // --- Game loop ---
    // moveNumber tracks chess move number (1, 2, 3...) — increments after black plays.
    // halfMoves counts individual plies so we can respect the move limit.
    int moveNumber = 1;
    int halfMoves = 0;
    int maxHalfMoves = maxMoves * 2;

    while (halfMoves < maxHalfMoves) {
        // Check game over before each half-move
        if (controller.isGameOver()) {
            printf("\n══════════════════════════════════════════════════════════════\n");
            bool inCheck = isAttacked(
                getLSBindex(bitboards[side == white ? K : k]),
                side == white ? black : white
            );
            if (inCheck) {
                printf("  CHECKMATE — %s wins\n", side == white ? "Black" : "White");
            } else {
                printf("  STALEMATE — Draw\n");
            }
            printf("══════════════════════════════════════════════════════════════\n");
            break;
        }

        bool isWhiteTurn = (side == white);

        // Search for best move
        extern int bestMove;
        searchPosition(searchDepth);

        if (!bestMove) {
            printf("  No move found (game over not detected?)\n");
            break;
        }

        // Print move header banner
        printf("══════════════════════════════════════════════════════════════\n");
        printf("  Move %d%s  ", moveNumber, isWhiteTurn ? "." : "...");
        printUciMove(bestMove);
        printf("  [%s]  %s\n",
               describeMoveType(bestMove),
               pieceNameFromEngine(getPiece(bestMove)));
        printf("══════════════════════════════════════════════════════════════\n");

        // Execute on the dry-run gantry:
        //   - plans the move using the current physicalBoard state
        //   - prints all G-code to stdout
        //   - updates physicalBoard internally (movePiece / clearSquare)
        if (!controller.executeEngineMove(bestMove)) {
            printf("  [ERROR] executeEngineMove failed — aborting dry run\n");
            break;
        }

        // Advance the engine's bitboard state to match.
        // physicalBoard inside GameController is already updated by executeEngineMove.
        makeMove(bestMove, allMoves);

        halfMoves++;

        // Move number increments after black plays (after each full move pair)
        if (!isWhiteTurn) {
            moveNumber++;
            // Print the board position after each complete move pair
            printf("\n");
            printBoard();
            printf("\n");
        }
    }

    if (halfMoves >= maxHalfMoves) {
        printf("\n══════════════════════════════════════════════════════════════\n");
        printf("  Dry run complete — reached move limit (%d moves per side)\n", maxMoves);
        printf("══════════════════════════════════════════════════════════════\n");
    }

    printf("\nTotal half-moves executed: %d\n", halfMoves);
    printf("Inspect the G-code above for any unexpected commands.\n\n");
}

/************ Main Driver ************/
int main(int argc, char* argv[]) {
    // Initialize engine
    initAll();
    
    // Parse command line arguments
    RunMode mode = MODE_UCI;
    const char* serialPort = nullptr;
    int dryRunMaxMoves = 40;
    int dryRunDepth = 4;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printHelp();
            return 0;
        }
        else if (strcmp(argv[i], "--physical") == 0) {
            mode = MODE_PHYSICAL;
            if (i + 1 < argc) {
                serialPort = argv[++i];
            } else {
                printf("Error: --physical requires a serial port argument\n");
                printf("Example: kirin --physical /dev/ttyUSB0\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "--simulate") == 0) {
            mode = MODE_SIMULATION;
        }
        else if (strcmp(argv[i], "--dryrun") == 0) {
            mode = MODE_DRYRUN;
            // Optional: max moves per side
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                dryRunMaxMoves = atoi(argv[++i]);
                if (dryRunMaxMoves <= 0) dryRunMaxMoves = 40;
            }
            // Optional: search depth
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                dryRunDepth = atoi(argv[++i]);
                if (dryRunDepth <= 0) dryRunDepth = 4;
            }
        }
        else {
            printf("Unknown option: %s\n", argv[i]);
            printf("Use --help for usage information.\n");
            return 1;
        }
    }
    
    // Run in selected mode
    switch (mode) {
        case MODE_UCI:
            uciLoop();
            break;
            
        case MODE_PHYSICAL:
            runPhysicalMode(serialPort);
            break;
            
        case MODE_SIMULATION:
            runSimulationMode();
            break;
            
        case MODE_DRYRUN:
            runDryRunMode(dryRunMaxMoves, dryRunDepth);
            break;
    }
    
    return 0;
}
