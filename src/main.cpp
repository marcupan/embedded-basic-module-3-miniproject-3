// ═══════════════════════════════════════════════════════════════════════════
// Міні-проєкт 3 · сейф: пін-код енкодером
// Плата: YD-ESP32-S3 (ESP32-S3-WROOM-1-N16R8) · PlatformIO · Arduino framework
//
//   оберт в один бік — цифра +1, після 9 знову 0
//   зміна напрямку   — цифра підтверджена, цей клац дає наступній цифрі 0
//   кнопка           — скинути набране, це теж спроба; коли відкрито — закрити
//   ?                — показати стан
//
// Екран дублює консоль. Без екрана сейф теж працює, лише консоль.
// Залізо: map.svg
// ═══════════════════════════════════════════════════════════════════════════
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "safe.h"

namespace {
    // ── 1. Конфігурація ──────────────────────────────────────────────────────────
    struct Config {
        static constexpr uint8_t SERVO_PIN = 14;
        static constexpr uint8_t ENC_CLK_PIN = 7;
        static constexpr uint8_t ENC_DT_PIN = 8;
        static constexpr uint8_t ENC_SW_PIN = 9;
        static constexpr uint8_t BUZZER_PIN = 15;

        // Серво: обережні межі, щоб SG90 не впирався в упор. Канал 2 на іншому таймері, ніж зумер (канал 0)
        static constexpr uint8_t SERVO_CH = 2;
        static constexpr uint32_t SERVO_FREQ_HZ = 50;
        static constexpr uint8_t SERVO_BITS = 14; // duty 0…16383
        static constexpr uint32_t PERIOD_US = 20000;
        static constexpr uint32_t PULSE_MIN_US = 600;
        static constexpr uint32_t PULSE_MAX_US = 2300;
        static constexpr int ANGLE_MAX = 180;
        static constexpr int ANGLE_CLOSED = 0;
        static constexpr int ANGLE_OPEN = 90;

        static constexpr int COUNTS_PER_CLICK = 4;

        static constexpr uint32_t BTN_POLL_MS = 5;
        static constexpr uint8_t BTN_STABLE_READS = 4;

        static constexpr uint8_t BUZZER_CH = 0;

        // Екран K-18: пін SCK на модулі = SCL у I2C
        static constexpr uint8_t OLED_SDA_PIN = 10;
        static constexpr uint8_t OLED_SCL_PIN = 3;
        static constexpr uint8_t OLED_ADDRESS = 0x3C;
        static constexpr uint32_t I2C_HZ = 400000;
        static constexpr uint32_t SCREEN_MIN_MS = 50; // не частіше, ніж раз на 50 мс
    };

    constexpr uint8_t CODE[] = {3, 1, 4, 2};
    constexpr uint8_t CODE_LENGTH = sizeof(CODE);
    static_assert(CODE_LENGTH <= Safe::MAX_DIGITS, "у коді забагато цифр");

    Safe safe(CODE, CODE_LENGTH);

    // ── 2. Серво: кут → мкс → duty ───────────────────────────────────────────────
    void writeServo(const int deg) {
        const uint32_t pulseUs = Config::PULSE_MIN_US +
                                 (Config::PULSE_MAX_US - Config::PULSE_MIN_US) * deg / Config::ANGLE_MAX;
        const uint32_t duty = pulseUs * (1UL << Config::SERVO_BITS) / Config::PERIOD_US;
        ledcWrite(Config::SERVO_CH, duty);
    }

    // ── 3. Мелодії: неблокуючий плеєр, таблиця нот ───────────────────────────────
    struct Note {
        uint16_t hz; // 0 = пауза
        uint16_t ms;
    };

    constexpr Note MELODY_OPEN[] = {{1047, 120}, {1319, 120}, {1568, 240}};
    constexpr Note MELODY_DENIED[] = {{300, 200}, {0, 80}, {300, 200}};
    constexpr Note MELODY_RESET[] = {{800, 80}};
    constexpr Note MELODY_ALARM[] = {{1500, 200}, {700, 200}, {1500, 200}, {700, 200},
                                     {1500, 200}, {700, 200}, {1500, 200}, {700, 200}};

    const Note *melody = nullptr;
    size_t melodyLength = 0;
    size_t noteIndex = 0;
    uint32_t noteStartedAt = 0;

    template<size_t N>
    void play(const Note (&notes)[N]) {
        melody = notes;
        melodyLength = N;
        noteIndex = 0;
        noteStartedAt = millis();
        ledcWriteTone(Config::BUZZER_CH, melody[0].hz);
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

    // ── 4. Енкодер: таблиця квадратури в перериванні ─────────────────────────────
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

    // Повертає +1 / −1 на кожен клац, інакше 0. Клаци, що накопичились, віддає по одному
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

    // Повертає true один раз на натискання: рівень стабільний кілька опитувань поспіль
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

    // ── 5. Екран ─────────────────────────────────────────────────────────────────
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C screen(U8G2_R0, U8X8_PIN_NONE); // без пінів: шину запускає setup(), бібліотека її не чіпає
    bool screenFound = false;
    bool screenDirty = true;
    uint32_t lastDrawAt = 0;
    const char *screenNote = "";

    void drawCentered(const char *text, const int y) {
        screen.drawUTF8((screen.getDisplayWidth() - screen.getUTF8Width(text)) / 2, y, text);
    }

    void drawEntering() {
        char line[20];
        snprintf(line, sizeof(line), "Спроба %u з %u", Safe::MAX_ATTEMPTS - safe.attemptsLeft() + 1,
                 Safe::MAX_ATTEMPTS);
        screen.setFont(u8g2_font_unifont_t_cyrillic);
        drawCentered(line, 14);
        drawCentered(screenNote, 62);

        // Цифри: підтверджені, поточна, решта прочерки
        char digits[Safe::MAX_DIGITS * 2] = {};
        for (uint8_t i = 0; i < CODE_LENGTH; i++) {
            char symbol = '-';
            if (i < safe.digitsDone()) {
                symbol = static_cast<char>('0' + safe.digitAt(i));
            } else if (i == safe.digitsDone() && safe.digitInProgress()) {
                symbol = static_cast<char>('0' + safe.currentDigit());
            }
            digits[i * 2] = symbol;
            digits[i * 2 + 1] = (i + 1 < CODE_LENGTH) ? ' ' : '\0';
        }
        screen.setFont(u8g2_font_logisoso20_tr);
        drawCentered(digits, 44);
    }

    void drawScreen() {
        screen.clearBuffer();
        if (safe.phase() == Safe::Phase::Entering) {
            drawEntering();
        } else {
            const bool open = safe.phase() == Safe::Phase::Open;
            screen.setFont(u8g2_font_unifont_t_cyrillic);
            drawCentered(open ? "ВІДКРИТО" : "БЛОКУВАННЯ", 30);
            drawCentered(open ? "кнопка закриє" : "перезавантаж", 50);
        }
        screen.sendBuffer();
    }

    void updateScreen(const uint32_t now) {
        if (!screenFound || !screenDirty || now - lastDrawAt < Config::SCREEN_MIN_MS) return;
        screenDirty = false;
        lastDrawAt = now;
        drawScreen();
    }

    // ── 6. Подія сейфа → консоль, серво, зумер, екран ────────────────────────────
    void printPrompt() {
        Serial.printf("Спроба %u з %u. Код: ", Safe::MAX_ATTEMPTS - safe.attemptsLeft() + 1, Safe::MAX_ATTEMPTS);
    }

    void handle(const Safe::Event event) {
        screenDirty = true;

        switch (event) {
            case Safe::Event::None:
                break;
            case Safe::Event::DigitStarted:
                screenNote = "";
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
                screenNote = "Невірний код";
                play(MELODY_DENIED);
                printPrompt();
                break;
            case Safe::Event::Reset:
                Serial.println("\nСкинуто, спроба витрачена.");
                screenNote = "Скинуто";
                play(MELODY_RESET);
                printPrompt();
                break;
            case Safe::Event::Lockout:
                Serial.println("\nБЛОКУВАННЯ: спроби вичерпано. Перезавантаж плату.");
                play(MELODY_ALARM);
                break;
            case Safe::Event::Relocked:
                Serial.println("Закрито.");
                screenNote = "";
                writeServo(Config::ANGLE_CLOSED);
                play(MELODY_RESET);
                printPrompt();
                break;
        }
    }

    // Діагностика екрана: хто відповідає на шині, у звичайному і переставленому порядку дротів
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

    bool findScreen() {
        Wire.setClock(Config::I2C_HZ);
        Wire.beginTransmission(Config::OLED_ADDRESS);
        return Wire.endTransmission() == 0;
    }

    void reportScreen() {
        Serial.printf("Екран: %s\n", screenFound ? "є" : "НЕМАЄ");
        if (screenFound) return;

        scanI2C(Config::OLED_SCL_PIN, Config::OLED_SDA_PIN);
        scanI2C(Config::OLED_SDA_PIN, Config::OLED_SCL_PIN);
    }

    // ── 7. Клавіша: рядки зі старту можуть загубитись, поки монітор підключається ──
    void handleKeys() {
        if (!Serial.available() || Serial.read() != '?') return;

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

    pinMode(Config::ENC_CLK_PIN, INPUT_PULLUP);
    pinMode(Config::ENC_DT_PIN, INPUT_PULLUP);
    pinMode(Config::ENC_SW_PIN, INPUT_PULLUP);
    encoderPrev = readEncoderAB();
    attachInterrupt(digitalPinToInterrupt(Config::ENC_CLK_PIN), onEncoderEdge, CHANGE);
    attachInterrupt(digitalPinToInterrupt(Config::ENC_DT_PIN), onEncoderEdge, CHANGE);

    ledcSetup(Config::BUZZER_CH, 2000, 10);
    ledcAttachPin(Config::BUZZER_PIN, Config::BUZZER_CH);
    ledcWriteTone(Config::BUZZER_CH, 0);

    ledcSetup(Config::SERVO_CH, Config::SERVO_FREQ_HZ, Config::SERVO_BITS);
    ledcAttachPin(Config::SERVO_PIN, Config::SERVO_CH);
    writeServo(Config::ANGLE_CLOSED);

    Wire.begin(Config::OLED_SDA_PIN, Config::OLED_SCL_PIN);
    screenFound = findScreen();
    if (screenFound) {
        screen.setI2CAddress(Config::OLED_ADDRESS << 1);
        screen.setBusClock(Config::I2C_HZ);
        screen.begin();
        screen.enableUTF8Print();
    }

    Serial.printf("Сейф. Цифр у коді: %u\n", CODE_LENGTH);
    reportScreen();
    printPrompt();
}

void loop() {
    const uint32_t now = millis();

    handleKeys();
    updateMelody(now);
    updateScreen(now);

    const int click = pollEncoder();
    if (click != 0) handle(safe.onClick(click));

    if (pollButton(now)) handle(safe.onButton());
}
