// Усі піни й числа в одному місці. Розводка: map.svg
#pragma once
#include <stdint.h>

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

    static constexpr int COUNTS_PER_CLICK = 4; // 4 переходи квадратури на один клац

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
