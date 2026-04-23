/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    display_controller.h - I2C OLED status display controller
*
*    This module drives a small SSD1306-compatible OLED display over the
*    Raspberry Pi's I2C bus. It is intentionally isolated from game logic:
*    callers send high-level status text, and this controller owns the
*    hardware details, text layout, and future scrolling behavior.
*/

#ifndef KIRIN_DISPLAY_CONTROLLER_H
#define KIRIN_DISPLAY_CONTROLLER_H

#include <cstdint>
#include <string>

enum class DisplayStatus {
    Booting,
    HardwareReady,
    WaitingForHuman,
    EngineThinking,
    ExecutingMove,
    IllegalBoardState,
    WrongStorageSlot,
    GameOver,
    Idle
};

struct DisplayMessage {
    std::string title;
    std::string line1;
    std::string line2;
    std::string line3;
};

class DisplayController {
private:
    int fd;
    bool ready;
    std::string devicePath;
    uint8_t address;
    int width;
    int height;

    static constexpr int DEFAULT_WIDTH = 128;
    static constexpr int DEFAULT_HEIGHT = 64;
    static constexpr uint8_t DEFAULT_ADDRESS = 0x3C;

    bool writeCommand(uint8_t command);
    bool writeCommands(const uint8_t* commands, int count);
    bool writeData(const uint8_t* data, int count);
    bool flush();

    void clearBuffer();
    void setPixel(int x, int y, bool on = true);
    void drawChar(int x, int y, char c);
    void drawText(int x, int y, const std::string& text);
    std::string fitLine(const std::string& text) const;

    uint8_t framebuffer[DEFAULT_WIDTH * DEFAULT_HEIGHT / 8];

public:
    DisplayController(const char* i2cDevice = "/dev/i2c-1",
                      uint8_t i2cAddress = DEFAULT_ADDRESS,
                      int displayWidth = DEFAULT_WIDTH,
                      int displayHeight = DEFAULT_HEIGHT);
    ~DisplayController();

    bool init();
    void shutdown();
    bool isReady() const { return ready; }

    bool clear();
    bool showMessage(const DisplayMessage& message);
    bool showStatus(DisplayStatus status,
                    const std::string& detail = "",
                    const std::string& line2 = "",
                    const std::string& line3 = "");
    bool showTestPattern();
};

#endif // KIRIN_DISPLAY_CONTROLLER_H
