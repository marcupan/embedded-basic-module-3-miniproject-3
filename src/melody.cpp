#include "melody.hpp"

#include <Arduino.h>
#include "config.hpp"

namespace {
    const Note *melody = nullptr;
    size_t melodyLength = 0;
    size_t noteIndex = 0;
    uint32_t noteStartedAt = 0;
}

void setupBuzzer() {
    ledcSetup(Config::BUZZER_CH, 2000, 10);
    ledcAttachPin(Config::BUZZER_PIN, Config::BUZZER_CH);
    ledcWriteTone(Config::BUZZER_CH, 0);
}

void playNotes(const Note *notes, const size_t length) {
    melody = notes;
    melodyLength = length;
    noteIndex = 0;
    noteStartedAt = millis();
    ledcWriteTone(Config::BUZZER_CH, melody[0].hz); // сам ставить duty 50 %
}

void updateMelody(const uint32_t now) {
    if (melody == nullptr || now - noteStartedAt < melody[noteIndex].ms) return;

    noteIndex++;
    if (noteIndex == melodyLength) {
        ledcWriteTone(Config::BUZZER_CH, 0);
        melody = nullptr;
        return;
    }
    noteStartedAt = now;
    ledcWriteTone(Config::BUZZER_CH, melody[noteIndex].hz);
}
