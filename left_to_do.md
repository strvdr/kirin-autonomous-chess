# Kirin — Hardware Integration Checklist

**Target:** Physical prototype operational within one week  
**Date:** April 2026  
**Authors:** Strydr Silverberg, Colin Dake

---

## Legend

- ✅ Complete and tested
- 🔲 Not started
- ⚠️ Partially done / needs verification on hardware

---

## 1. Software — Complete and Tested

These components are implemented, passing all tests, and ready for integration.

- ✅ Chess engine (search, evaluation, move generation, UCI)
- ✅ Board interpreter (path planning, blocker detection, A* pathfinding)
- ✅ Gantry controller (coordinate translation, G-code generation, GRBL serial)
- ✅ Game controller (engine ↔ physical board bridge, special move handling)
- ✅ Board scanner — hall effect sensor scanning via multiplexed GPIO (6 muxes, 96 sensors)
- ✅ Storage-aware capture detection (`matchLegalMove` with `storageChanged` gate)
- ✅ Human move detection (`waitForLegalMove` — legal-move-matching loop)
- ✅ `GameController::waitForHumanMove()` integration
- ✅ Sensor game loop (`runSensorGameLoop` in `main.cpp`, `play` command)
- ✅ Dry-run mode (full pipeline without hardware)
- ✅ Simulation mode (manual move entry with G-code inspection)
- ✅ Test suite: board scanner (62/62 passing — captures, pondering, castling, en passant, promotion, transients)
- ✅ Test suite: game controller, board interpreter, engine, captured piece detection

---

## 2. Pre-Integration — Software Tasks

These should be done before first hardware power-on.

### 2.1 Board Verification After Engine Moves (Priority: HIGH)

After the gantry executes a move, scan the board and verify the physical
state matches what the engine expects. Catches dropped pieces, failed magnet
engagement, or missed steps.

- ✅ Add `verifyBoardState()` method to `GameController`
  - Scan board with `scanner.scanBoardDebounced()`
  - Compare against `occupancy[both]` (engine's expected state)
  - Return a diff if mismatch, or `true` if matches
- ✅ Call `verifyBoardState()` after every `executeEngineMove()` in `runSensorGameLoop`
- ✅ If mismatch detected: pause game, print diagnostic, alert player
- ✅ Add test: simulate a mismatch and verify detection

### 2.2 Game Start Verification (Priority: HIGH)

After `setupNewGame()` moves all pieces from storage to starting positions,
verify the board matches the expected starting occupancy.

- ✅ Scan board after `setupNewGame()` returns
- ✅ Compare against starting position occupancy (`0xFFFF00000000FFFF`)
- ✅ If mismatch: report which squares are wrong, allow retry

### 2.3 Error Recovery (Priority: MEDIUM)

Ensure engine state only advances after confirmed physical execution.

- ✅ Audit `runSensorGameLoop`: confirm `makeMove()` is called AFTER `executeEngineMove()` succeeds
- ✅ Add rollback path: if gantry fails mid-move, engine state must not advance
- ✅ Currently `executeEngineMove()` updates `physicalBoard` internally — if gantry fails partway through a multi-step move (castling, blocker relocation), the physical board tracking may be partially updated. Consider making updates atomic.

### 2.4 Ambiguous Capture Fallback (Priority: LOW)

In the rare case where multiple legal captures produce identical sensor occupancy
(e.g., queen can capture two different pawns from the same square), the system
hangs waiting for a move that can never resolve.

- 🔲 Add a timeout (e.g., 30 seconds of stable ambiguous state)
- 🔲 On timeout: print available options and fall back to manual input (`move` command)
- 🔲 OR: use storage slot identity to determine which piece was captured (requires knowing which slot maps to which piece type)

### 2.5 Promotion UI for Human Player (Priority: LOW)

Currently defaults to queen promotion. Fine for prototype, but eventually:

- 🔲 Detect underpromotion intent (physical button? specific storage slot?)
- 🔲 For now: document that all promotions are queen

---

## 3. Hardware Integration — First Power-On

Tasks for when the physical prototype is assembled.

### 3.1 Sensor Calibration

- 🔲 Run `kirin --physical /dev/ttyUSB0` → `diag` command
- 🔲 Verify all 64 board sensors read correctly (each square individually)
- 🔲 Verify all 32 storage sensors read correctly (16 per side)
- 🔲 Check for dead/stuck sensors — replace or re-wire
- 🔲 Verify active-low polarity: piece present = LOW, empty = HIGH
- 🔲 Tune `MUX_SETTLE_US` (currently 10μs) — increase if readings are noisy
- 🔲 Tune `DEBOUNCE_SCANS` (currently 3) and `DEBOUNCE_DELAY_MS` (currently 50ms)

### 3.2 Sensor-to-Square Mapping Verification

- 🔲 Place a single piece on a1, run `scan` — verify bit 56 is set (engine square a1)
- 🔲 Repeat for a8 (bit 0), h1 (bit 63), h8 (bit 7)
- 🔲 Verify all 64 squares map correctly — the mux channel-to-square wiring must match `toSquareIndex()`
- 🔲 If wiring differs from expected layout, update `toSquareIndex()` or re-wire

### 3.3 Storage Zone Mapping

- 🔲 Place a piece in black storage slot 0, run `scan` — verify bit 0 of black storage
- 🔲 Verify channel mapping: channels 0–7 = back-rank piece slots, 8–15 = pawn slots
- 🔲 Confirm slot positions physically match `getCapturePosition()` coordinates

### 3.4 Gantry Calibration

- 🔲 Home the gantry (`home` command) — verify limit switches trigger correctly
- 🔲 Run `test` command — verify gantry moves to e4, d5, engages/disengages magnet
- 🔲 Verify `toPhysical()` coordinates match actual board square centers
  - Place piece on a1, command gantry to a1, check alignment
  - Repeat for h8, e4 (center)
- 🔲 Verify capture zone coordinates (`getCapturePosition()`) align with physical slots
- 🔲 Tune `FEED_RATE` (currently 1000 in/min) — too fast may skip steps, too slow wastes time
- 🔲 Verify magnet engagement: piece lifts reliably, doesn't drag adjacent pieces

### 3.5 Gantry + Sensor Interaction

- 🔲 Execute a gantry move and verify the sensors detect the change
- 🔲 Check for electromagnetic interference: does the gantry magnet trigger nearby sensors?
- 🔲 Check for stepper motor noise affecting sensor readings
- 🔲 If interference exists: add shielding or increase `DEBOUNCE_SCANS`

### 3.6 GPIO Pin Conflicts

- 🔲 Verify no pin conflicts between scanner (6 SIG + 4 select = 10 pins) and any other GPIO usage
- 🔲 Current pin assignments:
  - Select lines: GPIO 17, 27, 22, 23
  - Board muxes: GPIO 5, 6, 13, 19
  - Storage muxes: GPIO 26, 16
- 🔲 Confirm libgpiod is installed: `sudo apt install libgpiod-dev`
- 🔲 Confirm build uses `-DHAS_GPIOD` flag
- 🔲 Test GPIO chip detection: Pi 5 = `/dev/gpiochip4`, Pi 4 = `/dev/gpiochip0`

---

## 4. Integration Testing — Full Game

### 4.1 Dry-Run Validation

- 🔲 Run `kirin --dryrun 10 4` — inspect all 20 half-moves of G-code for correctness
- 🔲 Verify capture G-code sends pieces to correct storage slots
- 🔲 Verify castling executes rook then king
- 🔲 Verify en passant removes the correct pawn

### 4.2 First Physical Game (Manual Moves)

- 🔲 `newgame black` — engine plays black, you type moves with `move e2e4`
- 🔲 Verify engine's physical moves execute correctly on the board
- 🔲 Play at least 10 moves, including one capture
- 🔲 Verify physical board state matches engine state after each move (`physical` command)

### 4.3 First Sensor-Based Game

- 🔲 `play black` — engine plays black, you make moves physically
- 🔲 Verify the system detects your moves correctly
- 🔲 Test: pick up a piece, hold it for 10 seconds, put it back — system should not commit a move
- 🔲 Test: make a capture — verify storage sensor detects the captured piece
- 🔲 Test: make an illegal move — verify the system alerts after `ILLEGAL_STATE_ALERT_MS`
- 🔲 Play a full game to completion (checkmate or stalemate)

### 4.4 Edge Cases to Test on Hardware

- 🔲 Castling (both sides, both colors)
- 🔲 En passant
- 🔲 Promotion (pawn reaches 8th rank)
- 🔲 Multiple captures in a row
- 🔲 Engine captures one of your pieces — storage zone updates correctly
- 🔲 New game after a completed game — all pieces return from storage

---

## 5. Known Limitations (Documented, Not Blockers)

These are known issues that don't block the prototype but should be addressed eventually.

| Issue | Impact | Workaround |
|-------|--------|------------|
| Ambiguous captures (same occupancy diff) | Game hangs waiting | Extremely rare; fall back to `move` command |
| Underpromotion not supported | Always promotes to queen | Acceptable for 99% of games |
| No physical feedback (buzzer/LED) for illegal states | Player sees terminal output only | Add piezo buzzer on GPIO in future |
| `physicalBoard` updates not atomic during multi-step moves | Partial state on gantry failure | Restart game if gantry fails mid-move |
| No draw detection (50-move rule, threefold repetition) | Game doesn't auto-detect draws | Player can quit manually |

---

## 6. File Reference

| File | Status | Description |
|------|--------|-------------|
| `board_scanner.h` | ✅ Modified | 96-sensor scanning, `waitForLegalMove`, storage zones |
| `board_scanner.cpp` | ✅ Modified | Storage-aware `matchLegalMove`, capture disambiguation |
| `game_controller.h` | ✅ Modified | `BoardScanner` member, `waitForHumanMove`, `initScanner` |
| `game_controller.cpp` | ✅ Modified | Storage baselines in `syncWithEngine`, `waitForHumanMove` |
| `main.cpp` | ✅ Modified | `play` command, `runSensorGameLoop`, `scan`, `diag` |
| `CMakeLists.txt` | ✅ Modified | Added `board_scanner.cpp/.h`, `board_scanner_test` |
| `board_scanner_test.cpp` | ✅ New | 62 tests covering all move detection logic |
| `gantry_controller.h` | No changes | Gantry namespace declarations |
| `gantry_controller.cpp` | No changes | GRBL serial, G-code generation, coordinate translation |
| `board_interpreter.h/.cpp` | No changes | Path planning, blocker detection |
| Engine files | No changes | Search, evaluation, move generation |
