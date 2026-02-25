# To-Do: Hall Effect Sensor — Human Move Detection
**Kirin Autonomous Chess System v0.4**

This document tracks the next development phase: replacing typed UCI input in physical
mode with real-time detection of human moves via a hall effect sensor matrix.

---

## Overview

```
Human lifts piece     → Arduino #2 detects field drop    → reports LIFTED <square>
Human places piece    → Arduino #2 detects field rise     → reports PLACED <square>
Arduino #2 sends event over serial
Host (Kirin) diffs against known physicalBoard state
Kirin reconstructs UCI move string → parseBoardMove() → engine validates legality
If legal  → makeMove() → engine turn begins
If illegal → signal player to redo
```

---

## Phase 1 — Hardware Design & Procurement

- [ ] **Select hall effect sensor IC**
  - Candidate: AH3144 (latching, digital output) or SS49E (analog, ratiometric)
  - Latching digital is simpler to read; analog gives richer signal for debugging
  - Need one per square × 64 squares

- [ ] **Design multiplexer wiring**
  - 64 sensors → too many direct GPIO pins for a single Arduino
  - Recommended: 4× CD74HC4067 (16-channel analog mux) on an Arduino Mega
  - 4 shared address pins (A0–A3) + 4 analog input pins + 4 enable pins
  - Alternatively: 2× 74HC165 shift registers (serial bitstream on 3 pins)
  - Decision needed before PCB layout

- [ ] **Design piece magnet placement**
  - Each chess piece needs a small permanent magnet in its base
  - Magnet orientation matters for latching sensors — standardize polarity
  - Verify sensor trigger distance matches board thickness + piece base clearance

- [ ] **Decide on Arduino #2 board**
  - Arduino Mega 2560 recommended (54 digital I/O, 16 analog inputs)
  - Needs: one hardware serial port for host comms, one for debug (optional)

- [ ] **Plan PCB or perfboard layout**
  - Sensor grid must align exactly with 2" × 2" square centers
  - Board sits above gantry — sensors face down toward magnet beneath each square
  - Route sensor power/ground bus to minimize noise

---

## Phase 2 — Arduino #2 Firmware

- [ ] **Implement sensor polling loop**
  - Scan all 64 sensors in sequence via multiplexer address cycling
  - Store 64-bit occupancy bitmask (`uint64_t boardState`)
  - Target scan rate: ≥ 20 Hz (50ms full cycle)

- [ ] **Implement debounce logic**
  - A square transition is only valid after N consecutive consistent readings
  - Recommended threshold: 3 readings (~150ms at 20 Hz)
  - Prevents false triggers from piece wobble or gantry vibration

- [ ] **Implement "gantry active" inhibit**
  - While the gantry is moving, the electromagnet will trip sensors
  - Host sends `GANTRY_START` / `GANTRY_DONE` over serial
  - While inhibited: continue scanning but do not report transitions
  - Resume reporting immediately on `GANTRY_DONE`

- [ ] **Implement serial reporting protocol**
  - Propose simple newline-delimited ASCII protocol:
    ```
    LIFTED e2       # piece removed from e2
    PLACED e4       # piece placed on e4
    READY           # sent on boot / after reset
    ```
  - Alternative: send raw 64-bit bitmask after each stable state change
    and let Kirin do all interpretation (simpler firmware, more host logic)
  - **Decision needed:** event-based vs. bitmask-based — pick one before implementing

- [ ] **Implement special move detection helpers (optional on Arduino)**
  - Castling: 4 square changes in one turn (King + Rook both move)
  - En passant: 3 square changes (pawn moves, captured pawn disappears)
  - Could detect these patterns on Arduino and send a single `SPECIAL` tag,
    or let Kirin handle disambiguation entirely (recommended — keep firmware simple)

- [ ] **Write Arduino firmware tests**
  - Bench test: manually trigger sensors, verify correct square labels reported
  - Debounce test: rapid triggering should not produce spurious events
  - Inhibit test: transitions during `GANTRY_START` window must be suppressed

---

## Phase 3 — Kirin Software: `HallSensorReader` Class

New file: `src/hardware/hall_sensor_reader.h/.cpp`

- [ ] **Define `HallSensorReader` class interface**
  ```cpp
  class HallSensorReader {
  public:
      bool connect(const char* port, int baudRate = 115200);
      void disconnect();
      bool isConnected() const;

      void notifyGantryStart();   // sends GANTRY_START to Arduino #2
      void notifyGantryDone();    // sends GANTRY_DONE to Arduino #2

      // Blocks until a complete, valid human move is detected.
      // Returns UCI move string (e.g., "e2e4") or "" on error/timeout.
      std::string waitForHumanMove(const PhysicalBoard& knownState);

  private:
      int serialFd;
      bool connected;
      std::string readLine(int timeoutMs);
      bool send(const std::string& msg);
  };
  ```

- [ ] **Implement `connect()` / `disconnect()`**
  - Mirrors `GrblController` serial setup (8N1, 115200 baud)
  - Can share the serial utility code or factor into a common `SerialPort` base

- [ ] **Implement `waitForHumanMove()`**
  - Read `LIFTED` and `PLACED` events from Arduino #2
  - Diff against `knownState` (the `physicalBoard` after the engine's last move)
  - Reconstruct UCI string from the two squares
  - Handle multi-square patterns (castling = King LIFTED + King PLACED +
    Rook LIFTED + Rook PLACED)
  - Return UCI string; caller (`GameController`) validates legality via
    `parseBoardMove()`

- [ ] **Handle illegal move feedback loop**
  - If `parseBoardMove()` returns 0 (illegal), do NOT update engine state
  - Send an error signal (TBD — serial message to a status LED Arduino, or
    log to console for now)
  - Call `waitForHumanMove()` again (player must reset their piece and retry)

- [ ] **Handle en passant square disappearance**
  - En passant: human's pawn moves diagonally to an empty square; the
    captured pawn's square also clears (that pawn was never physically
    picked up by the human — it was captured "in passing")
  - `waitForHumanMove()` must recognize 3-square patterns and pass the
    full context to `parseBoardMove()` for disambiguation

---

## Phase 4 — Integration into `GameController`

- [ ] **Add `HallSensorReader` as a member of `GameController`**
  ```cpp
  // game_controller.h
  private:
      Gantry::GrblController gantry;
      HallSensorReader hallSensor;   // NEW
      PhysicalBoard physicalBoard;
  ```

- [ ] **Add `connectSensorArray(const char* port)` to `GameController`**
  - Called from `runPhysicalMode()` alongside `connectHardware()`

- [ ] **Add `notifyGantryActive(bool active)` bridge method**
  - Called by `executeEngineMove()` before and after gantry moves
  - Forwards to `hallSensor.notifyGantryStart()` / `notifyGantryDone()`

- [ ] **Replace typed input in `runPhysicalMode()` game loop**

  Current (typed input):
  ```cpp
  // Human types: "move e2e4"
  int move = parseBoardMove(moveStr);
  ```

  Target (physical detection):
  ```cpp
  printf("Your turn. Move a piece.\n");
  std::string uciMove = controller.waitForHumanMove();
  int move = parseBoardMove(uciMove.c_str());
  ```

- [ ] **Update `startGame()` to accept two serial ports**
  - Current: one port for gantry
  - New: `startGame(const char* gantryPort, const char* sensorPort)`

---

## Phase 5 — New Run Mode: `--sensor-test`

Add a new CLI mode for isolated sensor debugging without a full game.

- [ ] **Add `MODE_SENSOR_TEST` to `RunMode` enum in `main.cpp`**
- [ ] **Implement `runSensorTestMode(const char* port)`**
  - Connects to Arduino #2 only
  - Prints every `LIFTED` / `PLACED` event to console in real time
  - Displays a live ASCII board diagram showing current sensor state
  - Useful for verifying wiring and sensor calibration before integration

- [ ] **Add `--sensor-test PORT` to CLI help text and argument parser**

---

## Phase 6 — Testing

- [ ] **Unit tests: `hall_sensor_reader_test.cpp`**
  - Mock serial input (pipe or file-backed fd)
  - Test normal move reconstruction from LIFTED/PLACED pair
  - Test castling 4-event sequence → correct UCI string
  - Test en passant 3-event sequence → correct UCI string
  - Test illegal move → returns empty string, does not modify board state
  - Test gantry inhibit: events during inhibit window are dropped

- [ ] **Integration test: sensor → engine pipeline**
  - Drive mock sensor events through `waitForHumanMove()` →
    `parseBoardMove()` → `makeMove()` for a known game sequence
  - Verify `physicalBoard` matches engine state after each move

- [ ] **Hardware-in-loop test (bench, no full game)**
  - Place and lift pieces on the physical board
  - Verify correct square names reported end-to-end through to Kirin
  - Test edge cases: a1, h1, a8, h8 (corner sensors)

---

## Phase 7 — README & Documentation Updates

- [ ] **Update `README.md`**
  - Add `HallSensorReader` to architecture diagram
  - Add sensor Arduino to hardware section (board, wiring summary, baud rate)
  - Update `--physical` usage to show two-port invocation
  - Add `--sensor-test` mode to usage table
  - Update development status table: mark hall effect detection as in progress

- [ ] **Add wiring reference section to README**
  - Multiplexer pin assignments
  - Arduino #2 serial port used for host comms
  - Signal line between host and Arduino #2 for gantry inhibit

---

## Open Decisions (Must Resolve Before Coding)

| # | Question | Options | Notes |
|---|---|---|---|
| 1 | Sensor IC type | AH3144 (digital latching) vs SS49E (analog) | Digital simpler; analog better for tuning |
| 2 | Multiplexer approach | CD74HC4067 mux vs 74HC165 shift register | Mux easier to wire; shift reg uses fewer pins |
| 3 | Arduino #2 board | Mega 2560 vs Uno + expander | Mega preferred for pin count headroom |
| 4 | Serial protocol | Event-based (LIFTED/PLACED) vs raw bitmask | Event-based cleaner; bitmask simpler firmware |
| 5 | Gantry inhibit signal | Serial message vs dedicated GPIO wire | Serial keeps wiring clean |
| 6 | Illegal move feedback | Console log only vs status LED | LED better UX; needs extra hardware |
| 7 | Two-port CLI syntax | `--physical /dev/ttyUSB0 --sensor /dev/ttyUSB1` vs single arg | Two explicit flags clearest |

---

## Dependency Map

```
Phase 1 (Hardware)
    └── Phase 2 (Arduino Firmware)
            └── Phase 3 (HallSensorReader class)
                    └── Phase 4 (GameController integration)
                            ├── Phase 5 (sensor-test mode)
                            └── Phase 6 (testing)
                                    └── Phase 7 (docs)
```

Phases 1 and 2 can proceed in parallel with Phase 3 if a mock serial source
is used to develop and test `HallSensorReader` before the Arduino firmware is ready.

---

*Kirin v0.4 development plan — last updated February 2026*
