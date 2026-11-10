#include "input.hpp"

#include <Arduino.h>
#include "config.hpp"

namespace {
    // Енкодер: таблиця квадратури в перериванні.
    // Оновлення екрана займає ~25 мс. Опитування в loop() пропускало б переходи,
    // тому кожен фронт CLK і DT ловить ISR.
    constexpr int8_t QUAD[4][4] = {
        {0, 1, -1, 0},
        {-1, 0, 0, 1},
        {1, 0, 0, -1},
        {0, -1, 1, 0},
    };

    volatile int32_t encoderCounts = 0; // пише лише ISR, читає loop()
    volatile uint8_t encoderPrev = 0;

    uint8_t readEncoderAB() {
        return static_cast<uint8_t>((digitalRead(Config::ENC_CLK_PIN) << 1) | digitalRead(Config::ENC_DT_PIN));
    }

    void IRAM_ATTR onEncoderEdge() {
        const uint8_t now = readEncoderAB();
        encoderCounts = encoderCounts + QUAD[encoderPrev][now];
        encoderPrev = now;
    }
}

void setupInput() {
    pinMode(Config::ENC_CLK_PIN, INPUT_PULLUP);
    pinMode(Config::ENC_DT_PIN, INPUT_PULLUP);
    pinMode(Config::ENC_SW_PIN, INPUT_PULLUP);
    encoderPrev = readEncoderAB();
    attachInterrupt(digitalPinToInterrupt(Config::ENC_CLK_PIN), onEncoderEdge, CHANGE);
    attachInterrupt(digitalPinToInterrupt(Config::ENC_DT_PIN), onEncoderEdge, CHANGE);
}

// Клаци, що накопичились, віддає по одному
int pollEncoder() {
    static int32_t handled = 0;

    const int32_t diff = encoderCounts - handled;
    if (diff >= Config::COUNTS_PER_CLICK) {
        handled += Config::COUNTS_PER_CLICK;
        return 1;
    }
    if (diff <= -Config::COUNTS_PER_CLICK) {
        handled -= Config::COUNTS_PER_CLICK;
        return -1;
    }

    return 0;
}

// Рівень має протриматись кілька опитувань поспіль
bool pollButton(const uint32_t now) {
    static uint32_t lastPoll = 0;
    static uint8_t same = 0;
    static bool stable = HIGH;

    if (now - lastPoll < Config::BTN_POLL_MS) return false;
    lastPoll = now;

    const bool level = digitalRead(Config::ENC_SW_PIN);
    same = (level == stable) ? 0 : same + 1;
    if (same < Config::BTN_STABLE_READS) return false;

    stable = level;
    same = 0;
    return stable == LOW;
}
