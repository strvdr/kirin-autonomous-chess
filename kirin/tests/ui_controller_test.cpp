/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    ui_controller_test.cpp - Tests for OLED display and button controllers
*/

#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

#include "../src/hardware/button_controller.h"
#include "../src/hardware/display_controller.h"

#define ANSI_RED    "\x1b[31m"
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_CYAN   "\x1b[36m"
#define ANSI_BOLD   "\x1b[1m"
#define ANSI_RESET  "\x1b[0m"

static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST_ASSERT(cond, msg)                                              \
    do {                                                                    \
        testsRun++;                                                         \
        if (cond) {                                                         \
            testsPassed++;                                                  \
            printf("  " ANSI_GREEN "✓ %s" ANSI_RESET "\n", msg);           \
        } else {                                                            \
            testsFailed++;                                                  \
            printf("  " ANSI_RED "✗ %s" ANSI_RESET " [line %d]\n", msg, __LINE__); \
        }                                                                   \
    } while (0)

static void printHeader(const char* name) {
    printf("\n" ANSI_BOLD ANSI_CYAN "── %s ──" ANSI_RESET "\n", name);
}

static bool anyPixelSet(const DisplayController& display) {
    const uint8_t* fb = display.getFramebuffer();
    for (int i = 0; i < display.framebufferSize(); i++) {
        if (fb[i] != 0) return true;
    }
    return false;
}

static void waitDebounce() {
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
}

static ButtonEvent pressAndDebounce(ButtonController& buttons, ButtonEvent button) {
    buttons.setSimulatedPressed(button, true);
    ButtonEvent immediate = buttons.poll();
    if (immediate != ButtonEvent::None) return immediate;
    waitDebounce();
    return buttons.poll();
}

static ButtonEvent releaseAndDebounce(ButtonController& buttons, ButtonEvent button) {
    buttons.setSimulatedPressed(button, false);
    ButtonEvent immediate = buttons.poll();
    if (immediate != ButtonEvent::None) return immediate;
    waitDebounce();
    return buttons.poll();
}

static void testButtonSimulationLifecycle() {
    printHeader("Button simulation lifecycle");

    ButtonController buttons;
    TEST_ASSERT(!buttons.isInitialized(), "buttons start uninitialized");
    TEST_ASSERT(buttons.poll() == ButtonEvent::None, "uninitialized poll returns none");

    buttons.enableSimulation();
    TEST_ASSERT(buttons.isInitialized(), "simulation initializes buttons");
    TEST_ASSERT(buttons.eventName(ButtonEvent::Start) == std::string("start"), "start event name");
    TEST_ASSERT(buttons.eventName(ButtonEvent::Stop) == std::string("stop"), "stop event name");
    TEST_ASSERT(buttons.eventName(ButtonEvent::Reset) == std::string("reset"), "reset event name");
}

static void testButtonDebounceAndEdges() {
    printHeader("Button debounce and edges");

    ButtonController buttons;
    buttons.enableSimulation();

    buttons.setSimulatedPressed(ButtonEvent::Start, true);
    TEST_ASSERT(buttons.poll() == ButtonEvent::None, "start press does not fire before debounce");
    waitDebounce();
    TEST_ASSERT(buttons.poll() == ButtonEvent::Start, "start fires after debounce");
    TEST_ASSERT(buttons.poll() == ButtonEvent::None, "held start does not repeat");

    TEST_ASSERT(releaseAndDebounce(buttons, ButtonEvent::Start) == ButtonEvent::None,
                "start release does not emit event");

    TEST_ASSERT(pressAndDebounce(buttons, ButtonEvent::Stop) == ButtonEvent::Stop,
                "stop fires after debounce");
    TEST_ASSERT(releaseAndDebounce(buttons, ButtonEvent::Stop) == ButtonEvent::None,
                "stop release does not emit event");

    TEST_ASSERT(pressAndDebounce(buttons, ButtonEvent::Reset) == ButtonEvent::Reset,
                "reset fires after debounce");
}

static void testDisplaySimulationLifecycle() {
    printHeader("Display simulation lifecycle");

    DisplayController display;
    TEST_ASSERT(!display.isReady(), "display starts not ready");
    display.enableSimulation();
    TEST_ASSERT(display.isReady(), "simulation display is ready");
    TEST_ASSERT(display.framebufferSize() == 1024, "128x64 framebuffer is 1024 bytes");
    TEST_ASSERT(!anyPixelSet(display), "simulation starts with clear framebuffer");
}

static void testDisplayRendering() {
    printHeader("Display rendering");

    DisplayController display;
    display.enableSimulation();

    TEST_ASSERT(display.showMessage({"KIRIN", "White to move", "Last e7e5", "Ready"}),
                "showMessage succeeds in simulation");
    TEST_ASSERT(anyPixelSet(display), "showMessage draws pixels");
    TEST_ASSERT(display.isPixelSet(0, 0) || display.isPixelSet(1, 0) || display.isPixelSet(2, 0),
                "title draws near origin");

    TEST_ASSERT(display.clear(), "clear succeeds");
    TEST_ASSERT(!anyPixelSet(display), "clear resets framebuffer");

    TEST_ASSERT(display.showStatus(DisplayStatus::WrongStorageSlot,
                                   "Wrong slot", "Move piece", "Try again"),
                "showStatus succeeds");
    TEST_ASSERT(anyPixelSet(display), "showStatus draws pixels");

    TEST_ASSERT(display.showTestPattern(), "test pattern succeeds");
    TEST_ASSERT(display.isPixelSet(127, 63), "test pattern draws bottom-right border");
}

static void testDisplayUnsupportedSize() {
    printHeader("Display unsupported size");

    DisplayController display("/dev/i2c-1", 0x3C, 128, 32);
    TEST_ASSERT(!display.init(), "unsupported 128x32 init fails cleanly");
}

int main() {
    printf(ANSI_BOLD "\n═══ Kirin UI Controller Test Suite ═══\n" ANSI_RESET);

    testButtonSimulationLifecycle();
    testButtonDebounceAndEdges();
    testDisplaySimulationLifecycle();
    testDisplayRendering();
    testDisplayUnsupportedSize();

    printf("\n" ANSI_BOLD "═══ Results: %d/%d passed" ANSI_RESET, testsPassed, testsRun);
    if (testsFailed > 0) {
        printf(ANSI_RED " (%d failed)" ANSI_RESET, testsFailed);
    }
    printf("\n\n");

    return testsFailed == 0 ? 0 : 1;
}
