/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
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
#include <thread>
#include <chrono>

#ifdef __linux__
#include <sys/select.h>
#include <unistd.h>
#endif

// Engine includes
#include "engine.h"

// Hardware includes  
#include "game_controller.h"
#include "display_controller.h"
#include "button_controller.h"

/************ Run Mode ************/
enum RunMode {
    MODE_UCI,           // Standard UCI protocol (for GUI or testing)
    MODE_PHYSICAL,      // Physical board with hardware
    MODE_SIMULATION,    // Interactive simulation (no hardware, manual moves)
    MODE_DRYRUN,        // Automatic engine-vs-engine dry run (no hardware, no input)
    MODE_OLED_TEST      // OLED display hardware test
};

/************ Forward Declarations ************/
void printHelp();
void runPhysicalMode(const char* port);
void runSimulationMode();
void runDryRunMode(int maxMoves, int searchDepth);
void runOledTestMode();
static void runSensorGameLoop(GameController& controller, bool engineWhite, DisplayController* display);
static bool promptForBoardRecovery(GameController& controller, DisplayController* display);
static bool finalizePhysicalEngineMove(GameController& controller, int engineMove, DisplayController* display);
static std::string moveToUciString(int move);
static void showDisplay(DisplayController* display, DisplayStatus status,
                        const std::string& detail = "",
                        const std::string& line2 = "",
                        const std::string& line3 = "");
static bool readPhysicalInput(char* input, int size,
                              ButtonController* buttons,
                              DisplayController* display,
                              GameController& controller,
                              bool& shouldQuit);
static bool handleIdleButton(ButtonEvent event,
                             DisplayController* display,
                             GameController& controller,
                             char* command,
                             int commandSize,
                             bool& shouldQuit);



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
    printf("  --oled-test                Initialize OLED and show a test screen\n");
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
    printf("  and inspect the same dry-run G-code path used by hardware mode.\n");
    printf("\n");
    printf("Dry Run Mode:\n");
    printf("  Plays a complete engine-vs-engine game automatically. Every\n");
    printf("  G-code command that would be sent to the gantry is printed with\n");
    printf("  annotations. Use this to audit the full command sequence before\n");
    printf("  powering the hardware for the first time.\n");
    printf("\n");
}

void runOledTestMode() {
    printf("[DISPLAY] Starting OLED test on /dev/i2c-1 address 0x3C\n");
    printf("[DISPLAY] Expected wiring: SDA=GPIO2 pin 3, SCL=GPIO3 pin 5\n");
    fflush(stdout);

    DisplayController display;
    if (!display.init()) {
        printf("[DISPLAY] OLED test failed. Check I2C enablement, wiring, and address.\n");
        printf("[DISPLAY] Try: i2cdetect -y 1\n");
        return;
    }

    display.showTestPattern();
    printf("[DISPLAY] Test pattern displayed.\n");
}

/************ Sensor-Based Game Loop ************/
// This is the core play experience: the human makes moves by physically
// moving pieces on the board, detected via hall effect sensors. The engine
// responds by moving pieces via the gantry.
static std::string moveToUciString(int move) {
    if (!move) return "";

    int source = getSource(move);
    int target = getTarget(move);
    char buf[6];
    buf[0] = 'a' + (source % 8);
    buf[1] = '8' - (source / 8);
    buf[2] = 'a' + (target % 8);
    buf[3] = '8' - (target / 8);
    if (getPromoted(move)) {
        int promo = getPromoted(move) % 6;
        const char promoChars[] = "pnbrqk";
        buf[4] = promoChars[promo];
        buf[5] = '\0';
    } else {
        buf[4] = '\0';
    }
    return std::string(buf);
}

static void showDisplay(DisplayController* display, DisplayStatus status,
                        const std::string& detail,
                        const std::string& line2,
                        const std::string& line3) {
    if (display && display->isReady()) {
        display->showStatus(status, detail, line2, line3);
    }
}

static bool handleIdleButton(ButtonEvent event,
                             DisplayController* display,
                             GameController& controller,
                             char* command,
                             int commandSize,
                             bool& shouldQuit) {
    if (event == ButtonEvent::None) return false;

    switch (event) {
        case ButtonEvent::Start:
            if (controller.isGameInProgress()) {
                snprintf(command, commandSize, "go");
                showDisplay(display, DisplayStatus::EngineThinking, "Start button", "go");
                printf("\n[BUTTONS] Start pressed -> go\n");
            } else {
                snprintf(command, commandSize, "play white");
                showDisplay(display, DisplayStatus::Booting, "Start button", "play white");
                printf("\n[BUTTONS] Start pressed -> play white\n");
            }
            return true;

        case ButtonEvent::Stop:
            if (controller.isGameInProgress()) {
                controller.stopGame();
                showDisplay(display, DisplayStatus::Idle, "Stop pressed", "Game stopped");
                printf("\n[BUTTONS] Stop pressed -> stop game\n");
            } else {
                shouldQuit = true;
                showDisplay(display, DisplayStatus::Idle, "Stop pressed", "Exiting");
                printf("\n[BUTTONS] Stop pressed -> exit physical mode\n");
            }
            return true;

        case ButtonEvent::Reset:
            snprintf(command, commandSize, "home");
            showDisplay(display, DisplayStatus::Booting, "Reset button", "Homing");
            printf("\n[BUTTONS] Reset pressed -> home\n");
            return true;

        case ButtonEvent::None:
            return false;
    }

    return false;
}

static bool readPhysicalInput(char* input, int size,
                              ButtonController* buttons,
                              DisplayController* display,
                              GameController& controller,
                              bool& shouldQuit) {
    while (true) {
        if (buttons && buttons->isInitialized()) {
            ButtonEvent event = buttons->poll();
            if (handleIdleButton(event, display, controller, input, size, shouldQuit)) {
                return true;
            }
        }

#ifdef __linux__
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int ret = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &tv);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(input, size, stdin) == nullptr) {
                return false;
            }
            input[strcspn(input, "\n")] = 0;
            return true;
        }
        if (ret < 0) {
            return false;
        }
#else
        if (fgets(input, size, stdin) == nullptr) {
            return false;
        }
        input[strcspn(input, "\n")] = 0;
        return true;
#endif
    }
}

static bool promptForBoardRecovery(GameController& controller, DisplayController* display) {
    showDisplay(display, DisplayStatus::IllegalBoardState,
                "Fix board", "Type continue", "or abort");
    printf("Board state mismatch after engine move.\n");
    printf("Please fix the board and type 'continue' or 'abort'.\n");

    char response[64];
    while (true) {
        printf("kirin> ");
        fflush(stdout);
        if (fgets(response, sizeof(response), stdin) == nullptr) {
            controller.stopGame();
            return false;
        }

        response[strcspn(response, "\n")] = 0;
        if (strcmp(response, "continue") == 0) {
            controller.syncWithEngine();
            showDisplay(display, DisplayStatus::HardwareReady, "Board resynced");
            return true;
        }
        if (strcmp(response, "abort") == 0) {
            controller.stopGame();
            showDisplay(display, DisplayStatus::Idle, "Game aborted");
            return false;
        }

        printf("Type 'continue' (after fixing the board) or 'abort'.\n");
    }
}

static bool finalizePhysicalEngineMove(GameController& controller, int engineMove, DisplayController* display) {
    makeMove(engineMove, allMoves);
    controller.updateTracker(engineMove);
    controller.syncWithEngine();

    if (controller.isScannerReady() && !controller.verifyBoardState()) {
        return promptForBoardRecovery(controller, display);
    }

    return true;
}

static void runSensorGameLoop(GameController& controller, bool engineWhite, DisplayController* display) {
    printf("\n");
    printf("══════════════════════════════════════════════════════════════\n");
    printf("  GAME IN PROGRESS — %s vs %s\n",
           engineWhite ? "Engine (white)" : "You (white)",
           engineWhite ? "You (black)" : "Engine (black)");
    printf("  Make your moves on the physical board.\n");
    printf("  The system will detect your move automatically.\n");
    printf("  Press Ctrl+C to abort.\n");
    printf("══════════════════════════════════════════════════════════════\n");
    printf("\n");

    int moveNumber = 1;
    showDisplay(display, DisplayStatus::HardwareReady,
                engineWhite ? "Engine white" : "Human white",
                "Game active");

    while (controller.isGameInProgress()) {
        // Check for game over
        if (controller.isGameOver()) {
            printf("\n");
            printf("══════════════════════════════════════════════════════════════\n");
            bool inCheck = isAttacked(
                getLSBindex(bitboards[side == white ? K : k]),
                side == white ? black : white
            );
            if (inCheck) {
                printf("  CHECKMATE — %s wins!\n", side == white ? "Black" : "White");
                showDisplay(display, DisplayStatus::GameOver,
                            side == white ? "Black wins" : "White wins",
                            "Checkmate");
            } else {
                printf("  STALEMATE — Draw!\n");
                showDisplay(display, DisplayStatus::GameOver, "Draw", "Stalemate");
            }
            printf("══════════════════════════════════════════════════════════════\n");
            controller.stopGame();
            break;
        }

        if (controller.isEngineTurn()) {
            // Engine's turn — search and execute
            printf("\nMove %d%s — Engine thinking...\n",
                   moveNumber, (side == white) ? "." : "...");
            showDisplay(display, DisplayStatus::EngineThinking,
                        side == white ? "White to move" : "Black to move",
                        "Searching...");

            searchPosition(6);

            extern int bestMove;
            if (!bestMove) {
                printf("No legal moves — game over!\n");
                showDisplay(display, DisplayStatus::GameOver, "No legal moves");
                controller.stopGame();
                break;
            }

            // Print what the engine chose
            int source = getSource(bestMove);
            int target = getTarget(bestMove);
            printf("Engine plays: %c%d%c%d",
                   'a' + (source % 8), 8 - (source / 8),
                   'a' + (target % 8), 8 - (target / 8));
            if (getPromoted(bestMove)) {
                int promo = getPromoted(bestMove) % 6;
                const char promoChars[] = "pnbrqk";
                printf("%c", promoChars[promo]);
            }
            printf("\n");
            std::string engineMoveStr = moveToUciString(bestMove);
            showDisplay(display, DisplayStatus::ExecutingMove,
                        "Engine: " + engineMoveStr,
                        "Gantry moving");

            // Execute on physical board (gantry moves the piece)
            if (!controller.executeEngineMove(bestMove)) {
                printf("HARDWARE ERROR executing engine move! Aborting game.\n");
                showDisplay(display, DisplayStatus::IllegalBoardState,
                            "Hardware error", "Engine move");
                controller.stopGame();
                break;
            }

            if (!finalizePhysicalEngineMove(controller, bestMove, display)) {
                if (!controller.isGameInProgress()) {
                    break;
                }
                printf("Board recovery failed after engine move.\n");
                showDisplay(display, DisplayStatus::IllegalBoardState,
                            "Recovery failed");
                controller.stopGame();
                break;
            }

            showDisplay(display, DisplayStatus::HardwareReady,
                        "Engine: " + engineMoveStr,
                        side == white ? "White to move" : "Black to move");

            printBoard();

            // Advance move number after black plays
            if (side == white) {  // side already flipped by makeMove, so this was black's move
                moveNumber++;
            }
        } else {
            // Human's turn — wait for sensor input
            printf("\nMove %d%s — Your turn. Make your move on the board.\n",
                   moveNumber, (side == white) ? "." : "...");
            showDisplay(display, DisplayStatus::WaitingForHuman,
                        side == white ? "White to move" : "Black to move",
                        "Move piece");

            int humanMove = 0;
            if (!controller.waitForHumanMove(humanMove)) {
                printf("Move detection failed or timed out.\n");
                showDisplay(display, DisplayStatus::IllegalBoardState,
                            "Detection failed", "Try again");
                continue;
            }
            std::string humanMoveStr = moveToUciString(humanMove);
            showDisplay(display, DisplayStatus::HardwareReady,
                        "Human: " + humanMoveStr,
                        "Move detected");

            // Make the move in the engine
            if (!makeMove(humanMove, allMoves)) {
                // This shouldn't happen since waitForHumanMove validated it,
                // but handle it defensively
                printf("Engine rejected the detected move — this is a bug.\n");
                showDisplay(display, DisplayStatus::IllegalBoardState,
                            "Engine rejected", humanMoveStr);
                continue;
            }

            // Keep PieceTracker in sync
            controller.updateTracker(humanMove);

            // Sync controller after engine state change
            controller.syncWithEngine();

            printBoard();

            // Advance move number after black plays
            if (side == white) {
                moveNumber++;
            }
        }
    }

    printf("\nGame ended.\n");
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
    DisplayController display;
    DisplayController* displayPtr = nullptr;
    ButtonController buttons;
    ButtonController* buttonsPtr = nullptr;

    if (display.init()) {
        displayPtr = &display;
        showDisplay(displayPtr, DisplayStatus::Booting, "Physical mode");
    } else {
        printf("[DISPLAY] OLED unavailable; continuing without display.\n");
    }

    if (buttons.init()) {
        buttonsPtr = &buttons;
        printf("[BUTTONS] Start=GPIO%d Stop=GPIO%d Reset=GPIO%d\n",
               DEFAULT_PIN_BUTTON_START, DEFAULT_PIN_BUTTON_STOP, DEFAULT_PIN_BUTTON_RESET);
    } else {
        printf("[BUTTONS] Buttons unavailable; continuing with terminal commands only.\n");
    }
    
    // Connect to hardware
    printf("Connecting to gantry on %s...\n", port);
    showDisplay(displayPtr, DisplayStatus::Booting, "Connecting gantry");
    if (!controller.connectHardware(port)) {
        printf("Failed to connect. Check port and try again.\n");
        showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                    "Gantry connect", "failed");
        return;
    }
    
    // Initialize the board scanner
    printf("Initializing board scanner...\n");
    showDisplay(displayPtr, DisplayStatus::Booting, "Init scanner");
    if (!controller.initScanner()) {
        printf("Warning: Scanner initialization failed.\n");
        printf("  Sensor-based move detection will not be available.\n");
        printf("  You can still use 'move' to type moves manually.\n");
        showDisplay(displayPtr, DisplayStatus::HardwareReady,
                    "Scanner failed", "Manual moves ok");
    } else {
        showDisplay(displayPtr, DisplayStatus::HardwareReady, "Scanner OK");
    }
    
    // Home the gantry
    printf("Homing gantry...\n");
    showDisplay(displayPtr, DisplayStatus::Booting, "Homing gantry");
    if (!controller.homeGantry()) {
        printf("Homing failed. Check hardware and try again.\n");
        showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                    "Homing failed");
        return;
    }
    showDisplay(displayPtr, DisplayStatus::Idle, "Ready", "Type command");
    
    // Main command loop
    char input[256];
    bool shouldQuit = false;
    printf("\nReady. Type 'help' for commands.\n\n");
    
    while (!shouldQuit) {
        printf("kirin> ");
        fflush(stdout);
        
        if (!readPhysicalInput(input, sizeof(input), buttonsPtr, displayPtr, controller, shouldQuit)) {
            break;
        }
        if (shouldQuit) {
            break;
        }
        
        // Parse command
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            printf("Goodbye!\n");
            showDisplay(displayPtr, DisplayStatus::Idle, "Goodbye");
            break;
        }
        else if (strcmp(input, "help") == 0) {
            printf("\nCommands:\n");
            printf("  play [white|black]     - Start a game using board sensors\n");
            printf("                           (engine plays specified color, default white)\n");
            printf("  newgame [white|black]  - Start new game with manual move entry\n");
            printf("  move <move>            - Make a move manually (e.g., 'move e2e4')\n");
            printf("  go                     - Engine makes a move\n");
            printf("  board                  - Display current position\n");
            printf("  physical               - Display physical board state\n");
            printf("  scan                   - Scan and display sensor readings\n");
            printf("  diag                   - Run full sensor diagnostic\n");
            printf("  fen <fen>              - Set position from FEN string\n");
            printf("  home                   - Home the gantry\n");
            printf("  test                   - Run hardware test sequence\n");
            printf("  quit                   - Exit program\n");
            printf("\nButtons at safe command prompts:\n");
            printf("  Start: play white if idle, go if game active\n");
            printf("  Stop:  stop active game, exit if idle\n");
            printf("  Reset: home gantry\n");
            printf("\n");
        }
        else if (strncmp(input, "play", 4) == 0) {
            // Sensor-based game — the main experience
            if (!controller.isScannerReady()) {
                printf("Error: Board scanner not initialized.\n");
                printf("  Sensor-based play requires working hall effect sensors.\n");
                printf("  Use 'newgame' for manual move entry instead.\n");
                showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                            "Scanner needed", "Use newgame");
                continue;
            }

            bool engineWhite = true;
            if (strstr(input, "black")) {
                engineWhite = false;
            }
            
            printf("Setting up new game (engine plays %s)...\n",
                   engineWhite ? "white" : "black");
            showDisplay(displayPtr, DisplayStatus::Booting,
                        "Setting up", engineWhite ? "Engine white" : "Engine black");
            
            // Reset engine
            parseFEN(startPosition);
            
            // Set up physical board
            if (!controller.setupNewGame()) {
                printf("Failed to set up board.\n");
                showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                            "Setup failed");
                continue;
            }
            
            controller.startGame(engineWhite);
            
            // Verify the physical board matches the starting position
            if (!controller.verifyBoardState()) {
                printf("Board does not match starting position after setup.\n");
                printf("Please fix piece placement and try 'play' again.\n");
                showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                            "Setup mismatch", "Fix pieces");
                controller.stopGame();
                continue;
            }
            
            printBoard();
            
            // Run the sensor-based game loop
            runSensorGameLoop(controller, engineWhite, displayPtr);
            showDisplay(displayPtr, DisplayStatus::Idle, "Game ended");
        }
        else if (strncmp(input, "newgame", 7) == 0) {
            // Manual-entry game (legacy / fallback)
            bool engineWhite = true;
            if (strstr(input, "black")) {
                engineWhite = false;
            }
            
            printf("Setting up new game (engine plays %s)...\n", 
                   engineWhite ? "white" : "black");
            showDisplay(displayPtr, DisplayStatus::Booting,
                        "Manual game", engineWhite ? "Engine white" : "Engine black");
            
            // Reset engine to starting position
            parseFEN(startPosition);
            
            // Set up physical board
            if (!controller.setupNewGame()) {
                printf("Failed to set up board.\n");
                showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                            "Setup failed");
                continue;
            }
            
            controller.startGame(engineWhite);
            
            // Verify setup if scanner is available
            if (controller.isScannerReady() && !controller.verifyBoardState()) {
                printf("Warning: Board does not match starting position.\n");
                showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                            "Board mismatch", "Manual mode");
            } else {
                showDisplay(displayPtr, DisplayStatus::HardwareReady,
                            "Manual game", engineWhite ? "Engine white" : "Human white");
            }
            
            printBoard();
            printf("\nGame started. %s to move.\n", 
                   engineWhite ? "Engine (white)" : "You (white)");
            printf("Use 'move <uci>' to enter moves manually.\n");
            
            // If engine plays white, make first move
            if (engineWhite) {
                printf("Engine thinking...\n");
                showDisplay(displayPtr, DisplayStatus::EngineThinking,
                            "Opening move");
                searchPosition(6);  // depth 6
                
                // Get best move from search
                extern int bestMove;
                if (bestMove) {
                    // Execute on physical board FIRST
                    if (controller.executeEngineMove(bestMove)) {
                        showDisplay(displayPtr, DisplayStatus::ExecutingMove,
                                    "Engine: " + moveToUciString(bestMove),
                                    "Gantry moving");
                        if (!finalizePhysicalEngineMove(controller, bestMove, displayPtr)) {
                            printf("Board recovery failed after engine move.\n");
                            continue;
                        }
                        
                        printf("Engine plays: ");
                        int source = getSource(bestMove);
                        int target = getTarget(bestMove);
                        printf("%c%d%c%d\n", 
                               'a' + (source % 8), 8 - (source / 8),
                               'a' + (target % 8), 8 - (target / 8));
                        printBoard();
                        showDisplay(displayPtr, DisplayStatus::WaitingForHuman,
                                    "Human to move", "Type move");
                    } else {
                        printf("Hardware error executing move!\n");
                        showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                                    "Hardware error");
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
                showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                            "Invalid move", moveStr);
                continue;
            }
            
            // Execute on physical board FIRST (before advancing engine state)
            showDisplay(displayPtr, DisplayStatus::ExecutingMove,
                        "Manual: " + moveToUciString(move),
                        "Gantry moving");
            if (!controller.executeEngineMove(move)) {
                printf("Hardware error executing move! Board state unchanged.\n");
                showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                            "Hardware error", "Manual move");
                continue;
            }
            
            // Hardware succeeded — now advance the engine state
            if (!makeMove(move, allMoves)) {
                printf("Illegal move: %s\n", moveStr);
                showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                            "Illegal move", moveStr);
                continue;
            }
            
            controller.updateTracker(move);
            controller.syncWithEngine();
            
            printf("Move: %s\n", moveStr);
            printBoard();
            
            // Check for game over
            if (controller.isGameOver()) {
                printf("\nGame over!\n");
                showDisplay(displayPtr, DisplayStatus::GameOver, "Game over");
                continue;
            }
            
            // Engine responds
            printf("Engine thinking...\n");
            showDisplay(displayPtr, DisplayStatus::EngineThinking, "Engine turn");
            searchPosition(6);
            
            extern int bestMove;
            if (bestMove) {
                // Execute on hardware FIRST
                if (controller.executeEngineMove(bestMove)) {
                    showDisplay(displayPtr, DisplayStatus::ExecutingMove,
                                "Engine: " + moveToUciString(bestMove),
                                "Gantry moving");
                    if (!finalizePhysicalEngineMove(controller, bestMove, displayPtr)) {
                        printf("Board recovery failed after engine move.\n");
                        continue;
                    }
                    
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
                        showDisplay(displayPtr, DisplayStatus::GameOver, "Game over");
                    } else {
                        showDisplay(displayPtr, DisplayStatus::WaitingForHuman,
                                    "Human to move", "Type move");
                    }
                } else {
                    printf("Hardware error executing engine move!\n");
                    showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                                "Hardware error", "Engine move");
                }
            } else {
                printf("No legal moves - game over!\n");
                showDisplay(displayPtr, DisplayStatus::GameOver, "No legal moves");
            }
        }
        else if (strcmp(input, "go") == 0) {
            printf("Engine thinking...\n");
            showDisplay(displayPtr, DisplayStatus::EngineThinking, "Engine turn");
            searchPosition(6);
            
            extern int bestMove;
            if (bestMove) {
                // Execute on hardware FIRST
                if (controller.executeEngineMove(bestMove)) {
                    showDisplay(displayPtr, DisplayStatus::ExecutingMove,
                                "Engine: " + moveToUciString(bestMove),
                                "Gantry moving");
                    if (!finalizePhysicalEngineMove(controller, bestMove, displayPtr)) {
                        printf("Board recovery failed after engine move.\n");
                        continue;
                    }
                    
                    printf("Engine plays: ");
                    int source = getSource(bestMove);
                    int target = getTarget(bestMove);
                    printf("%c%d%c%d\n", 
                           'a' + (source % 8), 8 - (source / 8),
                           'a' + (target % 8), 8 - (target / 8));
                    printBoard();
                    showDisplay(displayPtr, DisplayStatus::HardwareReady,
                                "Engine: " + moveToUciString(bestMove));
                } else {
                    printf("Hardware error! Engine state unchanged.\n");
                    showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                                "Hardware error");
                }
            } else {
                printf("No legal moves!\n");
                showDisplay(displayPtr, DisplayStatus::GameOver, "No legal moves");
            }
        }
        else if (strcmp(input, "board") == 0) {
            printBoard();
        }
        else if (strcmp(input, "physical") == 0) {
            printPhysicalBoard(controller.getPhysicalBoard());
        }
        else if (strcmp(input, "scan") == 0) {
            if (!controller.isScannerReady()) {
                printf("Scanner not initialized.\n");
                showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                            "Scanner offline");
                continue;
            }
            showDisplay(displayPtr, DisplayStatus::HardwareReady,
                        "Scanning board");
            BoardScanner& scanner = controller.getScanner();
            Bitboard boardScan = scanner.scanBoardDebounced();
            scanner.printScan(boardScan);
            
            uint16_t blackStorage = scanner.scanStorageDebounced(false);
            uint16_t whiteStorage = scanner.scanStorageDebounced(true);
            scanner.printStorageScan(blackStorage, false);
            scanner.printStorageScan(whiteStorage, true);
        }
        else if (strcmp(input, "diag") == 0) {
            if (!controller.isScannerReady()) {
                printf("Scanner not initialized.\n");
                showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                            "Scanner offline");
                continue;
            }
            showDisplay(displayPtr, DisplayStatus::HardwareReady,
                        "Sensor diag", "Running...");
            controller.getScanner().diagnosticScan();
            showDisplay(displayPtr, DisplayStatus::Idle, "Diag done");
        }
        else if (strncmp(input, "fen ", 4) == 0) {
            const char* fen = input + 4;
            while (*fen == ' ') fen++;
            parseFEN(fen);
            controller.invalidateTracker();
            controller.syncWithEngine();
            showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                        "Tracker unknown", "New game needed");
            printf("Tracker state marked unknown for arbitrary FEN.\n");
            printf("  Sensor-based play is disabled until a new game is started.\n");
            printBoard();
        }
        else if (strcmp(input, "home") == 0) {
            printf("Homing gantry...\n");
            showDisplay(displayPtr, DisplayStatus::Booting, "Homing gantry");
            if (controller.homeGantry()) {
                printf("Done.\n");
                showDisplay(displayPtr, DisplayStatus::Idle, "Homing done");
            } else {
                printf("Homing failed.\n");
                showDisplay(displayPtr, DisplayStatus::IllegalBoardState,
                            "Homing failed");
            }
        }
        else if (strcmp(input, "test") == 0) {
            printf("Running hardware test...\n");
            showDisplay(displayPtr, DisplayStatus::ExecutingMove,
                        "Hardware test");
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

    // Use the same GameController -> GrblController dry-run path as real
    // hardware so simulation cannot drift into a placeholder-only pipeline.
    GameController controller;
    controller.getGantry().enableDryRun();
    controller.initTrackerForStartingPosition();
    controller.syncWithEngine();
    
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
        
        printf("\n=== Move: %s ===\n", input);

        if (!controller.executeEngineMove(move)) {
            printf("Error executing simulated hardware move.\n");
            continue;
        }

        if (!makeMove(move, allMoves)) {
            printf("Engine rejected move after simulated hardware execution. Aborting simulation.\n");
            break;
        }
        controller.updateTracker(move);
        controller.syncWithEngine();
        
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
            
            if (!controller.executeEngineMove(bestMove)) {
                printf("Error executing simulated engine hardware move.\n");
                continue;
            }

            if (!makeMove(bestMove, allMoves)) {
                printf("Engine rejected its own selected move. Aborting simulation.\n");
                break;
            }
            controller.updateTracker(bestMove);
            controller.syncWithEngine();
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
        controller.updateTracker(bestMove);

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
        else if (strcmp(argv[i], "--oled-test") == 0) {
            mode = MODE_OLED_TEST;
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
        case MODE_OLED_TEST:
            runOledTestMode();
            break;
    }
    
    return 0;
}
