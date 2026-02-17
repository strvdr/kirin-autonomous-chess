/*
 *       ,  ,
 *           \\ \\                 
 *           ) \\ \\    _p_        
 *            )^\))\))  /  *\      KIRIN CHESS ENGINE v0.3
 *             \_|| || / /^`-'     
 *    __       -\ \\--/ /          Author:      Strydr Silverberg
 *  <'  \\___/   ___. )'                        Colorado School of Mines, Class of 2026
 *       `====\ )___/\\            
 *            //     `"            Hardware:    Colin Dake
 *            \\    /  \           
 *            `"                   
 * ══════════════════════════════════════════════════════════════════════════════
 * PROJECT OVERVIEW
 * ══════════════════════════════════════════════════════════════════════════════
 * 
 * Kirin is the computational engine powering an autonomous chess system
 * designed to deliver a complete single-player experience. By integrating
 * intelligent move generation with a physical board capable of independent
 * piece manipulation, the system eliminates the need for a human opponent
 * while preserving the tactile satisfaction of over-the-board play.
 * 
 * ══════════════════════════════════════════════════════════════════════════════
 * FUNDING & ACKNOWLEDGMENTS
 * ══════════════════════════════════════════════════════════════════════════════
 * 
 * 2026 Colorado School of Mines ProtoFund Recipient
 * Team: Strydr Silverberg, Colin Dake
 * 
 * This prototype represents the practical application of engineering
 * principles and computer science fundamentals developed through our
 * coursework at the Colorado School of Mines.
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
    MODE_SIMULATION     // Simulate hardware (no actual connection)
};

/************ Forward Declarations ************/
void printHelp();
void runPhysicalMode(const char* port);
void runSimulationMode();

/************ Help Message ************/
void printHelp() {
    printf("\n");
    printf("Kirin Chess Engine v0.3\n");
    printf("══════════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("Usage: kirin [OPTIONS]\n");
    printf("\n");
    printf("Options:\n");
    printf("  (no args)         Run in UCI mode (for chess GUIs)\n");
    printf("  --physical PORT   Run with physical board on serial PORT\n");
    printf("                    Example: kirin --physical /dev/ttyUSB0\n");
    printf("  --simulate        Run in simulation mode (no hardware)\n");
    printf("  --help, -h        Show this help message\n");
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
    printf("  Tests the full pipeline without hardware. Generates G-code\n");
    printf("  output but doesn't send to any device.\n");
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

/************ Main Driver ************/
int main(int argc, char* argv[]) {
    // Initialize engine
    initAll();
    
    // Parse command line arguments
    RunMode mode = MODE_UCI;
    const char* serialPort = nullptr;
    
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
    }
    
    return 0;
}
