/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    button_controller.cpp - GPIO button input controller
*/

#include "button_controller.h"
#include <cstdio>

// Use libgpiod v2.x when GPIO support is enabled at build time.
#ifdef HAS_GPIOD
#include <gpiod.h>
#define HAS_BUTTON_GPIO 1
#else
#define HAS_BUTTON_GPIO 0
#endif

namespace {

constexpr int BUTTON_COUNT = 3;
constexpr int DEBOUNCE_MS = 50;

#if HAS_BUTTON_GPIO
static struct gpiod_chip* buttonChip = nullptr;
static struct gpiod_line_request* buttonRequest = nullptr;
static unsigned int buttonOffsets[BUTTON_COUNT] = {};
#endif

ButtonEvent eventForIndex(int index) {
    switch (index) {
        case 0: return ButtonEvent::Start;
        case 1: return ButtonEvent::Stop;
        case 2: return ButtonEvent::Reset;
        default: return ButtonEvent::None;
    }
}

} // namespace

ButtonController::ButtonController()
    : pins{DEFAULT_PIN_BUTTON_START, DEFAULT_PIN_BUTTON_STOP, DEFAULT_PIN_BUTTON_RESET},
      initialized(false), simulationMode(false),
      simulatedState{false, false, false},
      rawState{false, false, false},
      stableState{false, false, false} {
    auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < BUTTON_COUNT; i++) {
        lastChange[i] = now;
    }
}

ButtonController::ButtonController(int startPin, int stopPin, int resetPin)
    : pins{startPin, stopPin, resetPin},
      initialized(false), simulationMode(false),
      simulatedState{false, false, false},
      rawState{false, false, false},
      stableState{false, false, false} {
    auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < BUTTON_COUNT; i++) {
        lastChange[i] = now;
    }
}

ButtonController::~ButtonController() {
#if HAS_BUTTON_GPIO
    if (buttonRequest) {
        gpiod_line_request_release(buttonRequest);
        buttonRequest = nullptr;
    }
    if (buttonChip) {
        gpiod_chip_close(buttonChip);
        buttonChip = nullptr;
    }
#endif
}

bool ButtonController::init() {
    if (simulationMode) {
        initialized = true;
        return true;
    }

#if HAS_BUTTON_GPIO
    const char* chipPaths[] = { "/dev/gpiochip4", "/dev/gpiochip0", nullptr };
    for (int i = 0; chipPaths[i] != nullptr; i++) {
        buttonChip = gpiod_chip_open(chipPaths[i]);
        if (buttonChip) {
            printf("[BUTTONS] Opened GPIO chip: %s\n", chipPaths[i]);
            break;
        }
    }

    if (!buttonChip) {
        fprintf(stderr, "[BUTTONS] Error: could not open GPIO chip\n");
        return false;
    }

    struct gpiod_line_settings* settings = gpiod_line_settings_new();
    if (!settings) {
        fprintf(stderr, "[BUTTONS] Error: could not create line settings\n");
        return false;
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);

    struct gpiod_line_config* lineConfig = gpiod_line_config_new();
    if (!lineConfig) {
        gpiod_line_settings_free(settings);
        fprintf(stderr, "[BUTTONS] Error: could not create line config\n");
        return false;
    }

    for (int i = 0; i < BUTTON_COUNT; i++) {
        buttonOffsets[i] = static_cast<unsigned int>(pins[i]);
    }

    if (gpiod_line_config_add_line_settings(lineConfig, buttonOffsets, BUTTON_COUNT, settings) < 0) {
        fprintf(stderr, "[BUTTONS] Error: could not add line settings\n");
        gpiod_line_config_free(lineConfig);
        gpiod_line_settings_free(settings);
        return false;
    }

    struct gpiod_request_config* reqConfig = gpiod_request_config_new();
    if (reqConfig) {
        gpiod_request_config_set_consumer(reqConfig, "kirin-buttons");
    }

    buttonRequest = gpiod_chip_request_lines(buttonChip, reqConfig, lineConfig);

    if (reqConfig) gpiod_request_config_free(reqConfig);
    gpiod_line_config_free(lineConfig);
    gpiod_line_settings_free(settings);

    if (!buttonRequest) {
        fprintf(stderr, "[BUTTONS] Error: could not request button lines (GPIO %d,%d,%d)\n",
                pins[0], pins[1], pins[2]);
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < BUTTON_COUNT; i++) {
        rawState[i] = readRaw(i);
        stableState[i] = rawState[i];
        lastChange[i] = now;
    }

    printf("[BUTTONS] Initialized active-low buttons: start GPIO %d, stop GPIO %d, reset GPIO %d\n",
           pins[0], pins[1], pins[2]);
    initialized = true;
    return true;
#else
    fprintf(stderr, "[BUTTONS] Error: GPIO not supported in this build\n");
    return false;
#endif
}

void ButtonController::enableSimulation() {
    simulationMode = true;
    initialized = true;
    auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < BUTTON_COUNT; i++) {
        simulatedState[i] = false;
        rawState[i] = false;
        stableState[i] = false;
        lastChange[i] = now;
    }
}

void ButtonController::setSimulatedPressed(ButtonEvent button, bool pressed) {
    int index = -1;
    switch (button) {
        case ButtonEvent::Start: index = 0; break;
        case ButtonEvent::Stop: index = 1; break;
        case ButtonEvent::Reset: index = 2; break;
        case ButtonEvent::None: return;
    }
    simulatedState[index] = pressed;
}

bool ButtonController::readRaw(int index) {
    if (simulationMode) {
        return (index >= 0 && index < BUTTON_COUNT) ? simulatedState[index] : false;
    }

#if HAS_BUTTON_GPIO
    if (!buttonRequest || index < 0 || index >= BUTTON_COUNT) {
        return false;
    }

    enum gpiod_line_value val = gpiod_line_request_get_value(buttonRequest, buttonOffsets[index]);
    return val == GPIOD_LINE_VALUE_INACTIVE; // active-low: pressed = low
#else
    (void)index;
    return false;
#endif
}

ButtonEvent ButtonController::poll() {
    if (!initialized) return ButtonEvent::None;

    const auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < BUTTON_COUNT; i++) {
        bool raw = readRaw(i);
        if (raw != rawState[i]) {
            rawState[i] = raw;
            lastChange[i] = now;
            continue;
        }

        const auto stableFor = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastChange[i]).count();
        if (stableFor >= DEBOUNCE_MS && stableState[i] != rawState[i]) {
            stableState[i] = rawState[i];
            if (stableState[i]) {
                return eventForIndex(i);
            }
        }
    }

    return ButtonEvent::None;
}

const char* ButtonController::eventName(ButtonEvent event) const {
    switch (event) {
        case ButtonEvent::Start: return "start";
        case ButtonEvent::Stop: return "stop";
        case ButtonEvent::Reset: return "reset";
        case ButtonEvent::None: return "none";
    }
    return "unknown";
}
