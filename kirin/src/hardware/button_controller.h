/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    button_controller.h - GPIO button input controller
*
*    Three active-low buttons are intended for high-level UI control:
*      Start -> GPIO 20
*      Stop  -> GPIO 21
*      Reset -> GPIO 24
*
*    These pins intentionally avoid the mux select pins, mux signal pins, and
*    the Pi's I2C pins used by the OLED.
*/

#ifndef KIRIN_BUTTON_CONTROLLER_H
#define KIRIN_BUTTON_CONTROLLER_H

#include <chrono>

constexpr int DEFAULT_PIN_BUTTON_START = 20;
constexpr int DEFAULT_PIN_BUTTON_STOP = 21;
constexpr int DEFAULT_PIN_BUTTON_RESET = 24;

enum class ButtonEvent {
    None,
    Start,
    Stop,
    Reset
};

class ButtonController {
private:
    int pins[3];
    bool initialized;
    bool simulationMode;
    bool simulatedState[3];
    bool rawState[3];
    bool stableState[3];
    std::chrono::steady_clock::time_point lastChange[3];

    bool readRaw(int index);

public:
    ButtonController();
    ButtonController(int startPin, int stopPin, int resetPin);
    ~ButtonController();

    bool init();
    void enableSimulation();
    bool isInitialized() const { return initialized; }
    void setSimulatedPressed(ButtonEvent button, bool pressed);

    ButtonEvent poll();
    const char* eventName(ButtonEvent event) const;
};

#endif // KIRIN_BUTTON_CONTROLLER_H
