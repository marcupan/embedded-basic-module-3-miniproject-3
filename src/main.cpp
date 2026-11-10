// ═══════════════════════════════════════════════════════════════════════════
// Міні-проєкт 3 · сейф: пін-код енкодером
// Плата: YD-ESP32-S3 (ESP32-S3-WROOM-1-N16R8) · PlatformIO · Arduino framework
//
//   оберт в один бік — цифра +1, після 9 знову 0
//   зміна напрямку   — цифра підтверджена, цей клац дає наступній цифрі 0
//   кнопка           — скинути набране, це теж спроба; коли відкрито — закрити
//   ?                — показати стан
//
// Файли: safe.hpp — логіка без заліза · input — енкодер і кнопка ·
//        melody — зумер · screen — екран · config.hpp — піни й числа
// Тут: серво і реакція на події сейфа.
// ═══════════════════════════════════════════════════════════════════════════
#include <Arduino.h>
#include "config.hpp"
#include "input.hpp"
#include "melody.hpp"
#include "safe.hpp"
#include "screen.hpp"

namespace {
    constexpr uint8_t CODE[] = {3, 1, 4, 2};
    constexpr uint8_t CODE_LENGTH = sizeof(CODE);
    static_assert(CODE_LENGTH <= Safe::MAX_DIGITS, "у коді забагато цифр");

    Safe safe(CODE, CODE_LENGTH);

    // ── Серво: кут → мкс → duty ──────────────────────────────────────────────────
    void setupServo() {
        ledcSetup(Config::SERVO_CH, Config::SERVO_FREQ_HZ, Config::SERVO_BITS);
        ledcAttachPin(Config::SERVO_PIN, Config::SERVO_CH);
    }

    void writeServo(const int deg) {
        const uint32_t pulseUs = Config::PULSE_MIN_US +
                                 (Config::PULSE_MAX_US - Config::PULSE_MIN_US) * deg / Config::ANGLE_MAX;
        const uint32_t duty = pulseUs * (1UL << Config::SERVO_BITS) / Config::PERIOD_US;
        ledcWrite(Config::SERVO_CH, duty);
    }

    // ── Подія сейфа → консоль, серво, зумер, екран ───────────────────────────────
    void printPrompt() {
        Serial.printf("Спроба %u з %u. Код: ", Safe::MAX_ATTEMPTS - safe.attemptsLeft() + 1, Safe::MAX_ATTEMPTS);
    }

    void handle(const Safe::Event event) {
        if (event == Safe::Event::None) return;
        markScreenDirty();

        switch (event) {
            case Safe::Event::None:
                break;
            case Safe::Event::DigitStarted:
                setScreenNote("");
                if (safe.digitsDone() > 0) Serial.print(' ');
                Serial.print(safe.currentDigit());
                break;
            case Safe::Event::DigitChanged:
                Serial.print('\b'); // курсор на символ назад, нова цифра стає поверх старої
                Serial.print(safe.currentDigit());
                break;
            case Safe::Event::Granted:
                Serial.println("\nВІДКРИТО. Кнопка закриє сейф.");
                writeServo(Config::ANGLE_OPEN);
                play(MELODY_OPEN);
                break;
            case Safe::Event::Denied:
                Serial.println("\nНевірний код.");
                setScreenNote("Невірний код");
                play(MELODY_DENIED);
                printPrompt();
                break;
            case Safe::Event::Reset:
                Serial.println("\nСкинуто, спроба витрачена.");
                setScreenNote("Скинуто");
                play(MELODY_RESET);
                printPrompt();
                break;
            case Safe::Event::Lockout:
                Serial.println("\nБЛОКУВАННЯ: спроби вичерпано. Перезавантаж плату.");
                play(MELODY_ALARM);
                break;
            case Safe::Event::Relocked:
                Serial.println("Закрито.");
                setScreenNote("");
                writeServo(Config::ANGLE_CLOSED);
                play(MELODY_RESET);
                printPrompt();
                break;
        }
    }

    // ── Клавіша «?»: рядки зі старту можуть загубитись, поки монітор підключається ──
    bool readHelpKey() {
        return Serial.available() && Serial.read() == '?';
    }

    void printState() {
        Serial.printf("\nЦифр у коді %u · набрано %u · лишилось спроб %u\n", CODE_LENGTH, safe.digitsDone(),
                      safe.attemptsLeft());
        reportScreen();
        if (safe.phase() == Safe::Phase::Entering) {
            printPrompt();
        } else {
            Serial.println(safe.phase() == Safe::Phase::Open ? "Відкрито." : "Заблоковано.");
        }
    }
} // namespace

void setup() {
    Serial.begin(115200);
    delay(1500); // час на підключення монітора (рідний USB)

    setupInput();
    setupBuzzer();
    setupServo();
    writeServo(Config::ANGLE_CLOSED);
    setupScreen();

    Serial.printf("Сейф. Цифр у коді: %u\n", CODE_LENGTH);
    reportScreen();
    printPrompt();
}

void loop() {
    const uint32_t now = millis();

    // 1. Спершу читаємо всі входи
    const bool helpKey = readHelpKey();
    const int click = pollEncoder();
    const bool pressed = pollButton(now);

    // 2. Потім реагуємо
    if (helpKey) printState();
    if (click != 0) handle(safe.onClick(click));
    if (pressed) handle(safe.onButton());

    // 3. І оновлюємо виходи, що тривають у часі
    updateMelody(now);
    updateScreen(now, safe, CODE_LENGTH);
}
