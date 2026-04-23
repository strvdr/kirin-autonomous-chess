/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    display_controller.cpp - I2C OLED status display controller
*/

#include "display_controller.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef __linux__
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#define KIRIN_HAS_I2C 1
#else
#define KIRIN_HAS_I2C 0
#endif

namespace {

constexpr int FONT_WIDTH = 5;
constexpr int FONT_HEIGHT = 7;
constexpr int CHAR_ADVANCE = 6;
constexpr int LINE_HEIGHT = 8;

const uint8_t* glyphFor(char c) {
    static const uint8_t blank[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t unknown[5] = {0x02, 0x01, 0x51, 0x09, 0x06};

    switch (c) {
        case ' ': return blank;
        case '!': { static const uint8_t g[5] = {0x00, 0x00, 0x5f, 0x00, 0x00}; return g; }
        case '.': { static const uint8_t g[5] = {0x00, 0x60, 0x60, 0x00, 0x00}; return g; }
        case ':': { static const uint8_t g[5] = {0x00, 0x36, 0x36, 0x00, 0x00}; return g; }
        case '-': { static const uint8_t g[5] = {0x08, 0x08, 0x08, 0x08, 0x08}; return g; }
        case '/': { static const uint8_t g[5] = {0x20, 0x10, 0x08, 0x04, 0x02}; return g; }
        case '_': { static const uint8_t g[5] = {0x40, 0x40, 0x40, 0x40, 0x40}; return g; }
        case '>': { static const uint8_t g[5] = {0x41, 0x22, 0x14, 0x08, 0x00}; return g; }
        case '<': { static const uint8_t g[5] = {0x08, 0x14, 0x22, 0x41, 0x00}; return g; }
        case '0': { static const uint8_t g[5] = {0x3e, 0x51, 0x49, 0x45, 0x3e}; return g; }
        case '1': { static const uint8_t g[5] = {0x00, 0x42, 0x7f, 0x40, 0x00}; return g; }
        case '2': { static const uint8_t g[5] = {0x42, 0x61, 0x51, 0x49, 0x46}; return g; }
        case '3': { static const uint8_t g[5] = {0x21, 0x41, 0x45, 0x4b, 0x31}; return g; }
        case '4': { static const uint8_t g[5] = {0x18, 0x14, 0x12, 0x7f, 0x10}; return g; }
        case '5': { static const uint8_t g[5] = {0x27, 0x45, 0x45, 0x45, 0x39}; return g; }
        case '6': { static const uint8_t g[5] = {0x3c, 0x4a, 0x49, 0x49, 0x30}; return g; }
        case '7': { static const uint8_t g[5] = {0x01, 0x71, 0x09, 0x05, 0x03}; return g; }
        case '8': { static const uint8_t g[5] = {0x36, 0x49, 0x49, 0x49, 0x36}; return g; }
        case '9': { static const uint8_t g[5] = {0x06, 0x49, 0x49, 0x29, 0x1e}; return g; }
        case 'A': { static const uint8_t g[5] = {0x7e, 0x11, 0x11, 0x11, 0x7e}; return g; }
        case 'B': { static const uint8_t g[5] = {0x7f, 0x49, 0x49, 0x49, 0x36}; return g; }
        case 'C': { static const uint8_t g[5] = {0x3e, 0x41, 0x41, 0x41, 0x22}; return g; }
        case 'D': { static const uint8_t g[5] = {0x7f, 0x41, 0x41, 0x22, 0x1c}; return g; }
        case 'E': { static const uint8_t g[5] = {0x7f, 0x49, 0x49, 0x49, 0x41}; return g; }
        case 'F': { static const uint8_t g[5] = {0x7f, 0x09, 0x09, 0x09, 0x01}; return g; }
        case 'G': { static const uint8_t g[5] = {0x3e, 0x41, 0x49, 0x49, 0x7a}; return g; }
        case 'H': { static const uint8_t g[5] = {0x7f, 0x08, 0x08, 0x08, 0x7f}; return g; }
        case 'I': { static const uint8_t g[5] = {0x00, 0x41, 0x7f, 0x41, 0x00}; return g; }
        case 'J': { static const uint8_t g[5] = {0x20, 0x40, 0x41, 0x3f, 0x01}; return g; }
        case 'K': { static const uint8_t g[5] = {0x7f, 0x08, 0x14, 0x22, 0x41}; return g; }
        case 'L': { static const uint8_t g[5] = {0x7f, 0x40, 0x40, 0x40, 0x40}; return g; }
        case 'M': { static const uint8_t g[5] = {0x7f, 0x02, 0x0c, 0x02, 0x7f}; return g; }
        case 'N': { static const uint8_t g[5] = {0x7f, 0x04, 0x08, 0x10, 0x7f}; return g; }
        case 'O': { static const uint8_t g[5] = {0x3e, 0x41, 0x41, 0x41, 0x3e}; return g; }
        case 'P': { static const uint8_t g[5] = {0x7f, 0x09, 0x09, 0x09, 0x06}; return g; }
        case 'Q': { static const uint8_t g[5] = {0x3e, 0x41, 0x51, 0x21, 0x5e}; return g; }
        case 'R': { static const uint8_t g[5] = {0x7f, 0x09, 0x19, 0x29, 0x46}; return g; }
        case 'S': { static const uint8_t g[5] = {0x46, 0x49, 0x49, 0x49, 0x31}; return g; }
        case 'T': { static const uint8_t g[5] = {0x01, 0x01, 0x7f, 0x01, 0x01}; return g; }
        case 'U': { static const uint8_t g[5] = {0x3f, 0x40, 0x40, 0x40, 0x3f}; return g; }
        case 'V': { static const uint8_t g[5] = {0x1f, 0x20, 0x40, 0x20, 0x1f}; return g; }
        case 'W': { static const uint8_t g[5] = {0x3f, 0x40, 0x38, 0x40, 0x3f}; return g; }
        case 'X': { static const uint8_t g[5] = {0x63, 0x14, 0x08, 0x14, 0x63}; return g; }
        case 'Y': { static const uint8_t g[5] = {0x07, 0x08, 0x70, 0x08, 0x07}; return g; }
        case 'Z': { static const uint8_t g[5] = {0x61, 0x51, 0x49, 0x45, 0x43}; return g; }
        default:
            if (c >= 'a' && c <= 'z') return glyphFor(static_cast<char>(c - 'a' + 'A'));
            return unknown;
    }
}

const char* titleForStatus(DisplayStatus status) {
    switch (status) {
        case DisplayStatus::Booting: return "KIRIN BOOT";
        case DisplayStatus::HardwareReady: return "HARDWARE READY";
        case DisplayStatus::WaitingForHuman: return "WAITING MOVE";
        case DisplayStatus::EngineThinking: return "ENGINE THINKING";
        case DisplayStatus::ExecutingMove: return "EXECUTING MOVE";
        case DisplayStatus::IllegalBoardState: return "BOARD MISMATCH";
        case DisplayStatus::WrongStorageSlot: return "WRONG STORAGE";
        case DisplayStatus::GameOver: return "GAME OVER";
        case DisplayStatus::Idle: return "KIRIN IDLE";
    }
    return "KIRIN";
}

} // namespace

DisplayController::DisplayController(const char* i2cDevice,
                                     uint8_t i2cAddress,
                                     int displayWidth,
                                     int displayHeight)
    : fd(-1), ready(false), devicePath(i2cDevice ? i2cDevice : "/dev/i2c-1"),
      address(i2cAddress), width(displayWidth), height(displayHeight) {
    clearBuffer();
}

DisplayController::~DisplayController() {
    shutdown();
}

bool DisplayController::init() {
    if (ready) return true;

    if (width != DEFAULT_WIDTH || height != DEFAULT_HEIGHT) {
        fprintf(stderr, "[DISPLAY] Error: only 128x64 OLED layout is currently implemented\n");
        return false;
    }

#if KIRIN_HAS_I2C
    fd = open(devicePath.c_str(), O_RDWR);
    if (fd < 0) {
        perror("[DISPLAY] Error opening I2C device");
        return false;
    }

    if (ioctl(fd, I2C_SLAVE, address) < 0) {
        perror("[DISPLAY] Error selecting OLED I2C address");
        close(fd);
        fd = -1;
        return false;
    }

    const uint8_t initSequence[] = {
        0xAE,       // display off
        0xD5, 0x80, // clock divide
        0xA8, 0x3F, // multiplex ratio 1/64
        0xD3, 0x00, // display offset
        0x40,       // display start line
        0x8D, 0x14, // charge pump on
        0x20, 0x00, // horizontal addressing mode
        0xA1,       // segment remap
        0xC8,       // COM scan direction remapped
        0xDA, 0x12, // COM pins
        0x81, 0xCF, // contrast
        0xD9, 0xF1, // pre-charge
        0xDB, 0x40, // VCOM detect
        0xA4,       // resume RAM display
        0xA6,       // normal display
        0x2E,       // deactivate scroll
        0xAF        // display on
    };

    if (!writeCommands(initSequence, static_cast<int>(sizeof(initSequence)))) {
        shutdown();
        return false;
    }

    ready = true;
    clear();
    printf("[DISPLAY] OLED initialized on %s at 0x%02X (%dx%d)\n",
           devicePath.c_str(), address, width, height);
    return true;
#else
    fprintf(stderr, "[DISPLAY] Error: I2C display support requires Linux\n");
    return false;
#endif
}

void DisplayController::shutdown() {
#if KIRIN_HAS_I2C
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
#endif
    ready = false;
}

bool DisplayController::writeCommand(uint8_t command) {
    return writeCommands(&command, 1);
}

bool DisplayController::writeCommands(const uint8_t* commands, int count) {
    if (fd < 0 || !commands || count <= 0) return false;

#if KIRIN_HAS_I2C
    uint8_t packet[17];
    int offset = 0;
    while (offset < count) {
        const int chunk = std::min(16, count - offset);
        packet[0] = 0x00; // command stream
        memcpy(packet + 1, commands + offset, chunk);
        if (write(fd, packet, chunk + 1) != chunk + 1) {
            perror("[DISPLAY] Error writing OLED command");
            return false;
        }
        offset += chunk;
    }
    return true;
#else
    (void)commands;
    (void)count;
    return false;
#endif
}

bool DisplayController::writeData(const uint8_t* data, int count) {
    if (fd < 0 || !data || count <= 0) return false;

#if KIRIN_HAS_I2C
    uint8_t packet[17];
    int offset = 0;
    while (offset < count) {
        const int chunk = std::min(16, count - offset);
        packet[0] = 0x40; // data stream
        memcpy(packet + 1, data + offset, chunk);
        if (write(fd, packet, chunk + 1) != chunk + 1) {
            perror("[DISPLAY] Error writing OLED data");
            return false;
        }
        offset += chunk;
    }
    return true;
#else
    (void)data;
    (void)count;
    return false;
#endif
}

bool DisplayController::flush() {
    if (!ready) return false;

    const uint8_t addressWindow[] = {
        0x21, 0x00, static_cast<uint8_t>(width - 1),
        0x22, 0x00, static_cast<uint8_t>((height / 8) - 1)
    };
    return writeCommands(addressWindow, static_cast<int>(sizeof(addressWindow))) &&
           writeData(framebuffer, width * height / 8);
}

void DisplayController::clearBuffer() {
    memset(framebuffer, 0, sizeof(framebuffer));
}

void DisplayController::setPixel(int x, int y, bool on) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;

    const int index = x + (y / 8) * width;
    const uint8_t mask = static_cast<uint8_t>(1U << (y & 7));
    if (on) {
        framebuffer[index] |= mask;
    } else {
        framebuffer[index] &= static_cast<uint8_t>(~mask);
    }
}

void DisplayController::drawChar(int x, int y, char c) {
    const uint8_t* glyph = glyphFor(c);
    for (int col = 0; col < FONT_WIDTH; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < FONT_HEIGHT; row++) {
            if (bits & (1U << row)) {
                setPixel(x + col, y + row);
            }
        }
    }
}

void DisplayController::drawText(int x, int y, const std::string& text) {
    int cursor = x;
    for (char c : text) {
        if (cursor + FONT_WIDTH >= width) break;
        drawChar(cursor, y, c);
        cursor += CHAR_ADVANCE;
    }
}

std::string DisplayController::fitLine(const std::string& text) const {
    const int maxChars = width / CHAR_ADVANCE;
    if (static_cast<int>(text.size()) <= maxChars) return text;
    if (maxChars <= 3) return text.substr(0, maxChars);
    return text.substr(0, maxChars - 3) + "...";
}

bool DisplayController::clear() {
    clearBuffer();
    return ready ? flush() : true;
}

bool DisplayController::showMessage(const DisplayMessage& message) {
    clearBuffer();
    drawText(0, 0, fitLine(message.title));
    drawText(0, LINE_HEIGHT * 2, fitLine(message.line1));
    drawText(0, LINE_HEIGHT * 4, fitLine(message.line2));
    drawText(0, LINE_HEIGHT * 6, fitLine(message.line3));
    return ready ? flush() : true;
}

bool DisplayController::showStatus(DisplayStatus status,
                                   const std::string& detail,
                                   const std::string& line2,
                                   const std::string& line3) {
    DisplayMessage message;
    message.title = titleForStatus(status);
    message.line1 = detail;
    message.line2 = line2;
    message.line3 = line3;
    return showMessage(message);
}

bool DisplayController::showTestPattern() {
    clearBuffer();
    drawText(0, 0, "KIRIN OLED TEST");
    drawText(0, 16, "I2C 128X64");
    drawText(0, 32, "STATUS: READY");
    drawText(0, 48, "SDA=GPIO2 SCL=3");

    for (int x = 0; x < width; x++) {
        setPixel(x, 63);
    }
    for (int y = 0; y < height; y++) {
        setPixel(127, y);
    }

    return ready ? flush() : true;
}
