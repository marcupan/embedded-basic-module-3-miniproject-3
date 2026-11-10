#include "screen.hpp"

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "config.hpp"

namespace {
    // Без пінів: шину запускає setupScreen(), бібліотека її не чіпає
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C screen(U8G2_R0, U8X8_PIN_NONE);
    bool screenFound = false;
    bool screenDirty = true;
    uint32_t lastDrawAt = 0;
    const char *screenNote = "";

    void drawCentered(const char *text, const int y) {
        screen.drawUTF8((screen.getDisplayWidth() - screen.getUTF8Width(text)) / 2, y, text);
    }

    void drawEntering(const Safe &safe, const uint8_t codeLength) {
        char line[20];
        snprintf(line, sizeof(line), "Спроба %u з %u", Safe::MAX_ATTEMPTS - safe.attemptsLeft() + 1,
                 Safe::MAX_ATTEMPTS);
        screen.setFont(u8g2_font_unifont_t_cyrillic);
        drawCentered(line, 14);
        drawCentered(screenNote, 62);

        // Цифри: підтверджені, поточна, решта прочерки
        char digits[Safe::MAX_DIGITS * 2] = {};
        for (uint8_t i = 0; i < codeLength; i++) {
            char symbol = '-';
            if (i < safe.digitsDone()) {
                symbol = static_cast<char>('0' + safe.digitAt(i));
            } else if (i == safe.digitsDone() && safe.digitInProgress()) {
                symbol = static_cast<char>('0' + safe.currentDigit());
            }
            digits[i * 2] = symbol;
            digits[i * 2 + 1] = (i + 1 < codeLength) ? ' ' : '\0';
        }
        screen.setFont(u8g2_font_logisoso20_tr);
        drawCentered(digits, 44);
    }

    void drawScreen(const Safe &safe, const uint8_t codeLength) {
        screen.clearBuffer();
        if (safe.phase() == Safe::Phase::Entering) {
            drawEntering(safe, codeLength);
        } else {
            const bool open = safe.phase() == Safe::Phase::Open;
            screen.setFont(u8g2_font_unifont_t_cyrillic);
            drawCentered(open ? "ВІДКРИТО" : "БЛОКУВАННЯ", 30);
            drawCentered(open ? "кнопка закриє" : "перезавантаж", 50);
        }
        screen.sendBuffer();
    }

    // Діагностика: хто відповідає на шині, у звичайному і переставленому порядку дротів
    void scanI2C(const uint8_t sda, const uint8_t scl) {
        Wire.end();
        Wire.begin(sda, scl);
        Wire.setClock(100000);

        Serial.printf("I2C SDA=%u SCL=%u:", sda, scl);
        int found = 0;
        for (uint8_t address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            if (Wire.endTransmission() == 0) {
                Serial.printf(" 0x%02X", address);
                found++;
            }
        }
        Serial.println(found == 0 ? " нікого" : "");
    }
}

void setupScreen() {
    Wire.begin(Config::OLED_SDA_PIN, Config::OLED_SCL_PIN);
    Wire.setClock(Config::I2C_HZ);
    Wire.beginTransmission(Config::OLED_ADDRESS);
    screenFound = Wire.endTransmission() == 0;
    if (!screenFound) return;

    screen.setI2CAddress(Config::OLED_ADDRESS << 1);
    screen.setBusClock(Config::I2C_HZ);
    screen.begin();
    screen.enableUTF8Print();
}

void reportScreen() {
    Serial.printf("Екран: %s\n", screenFound ? "є" : "НЕМАЄ");
    if (screenFound) return;

    scanI2C(Config::OLED_SCL_PIN, Config::OLED_SDA_PIN);
    scanI2C(Config::OLED_SDA_PIN, Config::OLED_SCL_PIN);
}

void markScreenDirty() {
    screenDirty = true;
}

void setScreenNote(const char *note) {
    screenNote = note;
}

void updateScreen(const uint32_t now, const Safe &safe, const uint8_t codeLength) {
    if (!screenFound || !screenDirty || now - lastDrawAt < Config::SCREEN_MIN_MS) return;
    screenDirty = false;
    lastDrawAt = now;
    drawScreen(safe, codeLength);
}
