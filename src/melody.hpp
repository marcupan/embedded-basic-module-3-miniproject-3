// Мелодії зумера: таблиці нот і неблокуючий плеєр
#pragma once
#include <stddef.h>
#include <stdint.h>

struct Note {
    uint16_t hz; // 0 = пауза
    uint16_t ms;
};

constexpr Note MELODY_OPEN[] = {{1047, 120}, {1319, 120}, {1568, 240}};
constexpr Note MELODY_DENIED[] = {{300, 200}, {0, 80}, {300, 200}};
constexpr Note MELODY_RESET[] = {{800, 80}};
constexpr Note MELODY_ALARM[] = {{1500, 200}, {700, 200}, {1500, 200}, {700, 200},
                                 {1500, 200}, {700, 200}, {1500, 200}, {700, 200}};

void setupBuzzer();
void playNotes(const Note *notes, size_t length);
void updateMelody(uint32_t now); // викликати на кожному оберті loop()

// Шаблон, щоб компілятор сам знав довжину масиву: play(MELODY_OPEN)
template<size_t N>
void play(const Note (&notes)[N]) {
    playNotes(notes, N);
}
