// Машина станів сейфа. Без заліза: приймає клаци й натискання, повертає подію.
// main.cpp вирішує, що з подією робити (консоль, серво, зумер, екран).
#pragma once
#include <stdint.h>

class Safe {
public:
    static constexpr uint8_t MAX_DIGITS = 8;
    static constexpr uint8_t MAX_ATTEMPTS = 3;

    enum class Phase : uint8_t { Entering, Open, Locked };

    enum class Event : uint8_t { None, DigitStarted, DigitChanged, Granted, Denied, Reset, Lockout, Relocked };

    // Код довший за MAX_DIGITS обрізаємо, інакше entered_ переповниться
    Safe(const uint8_t *code, const uint8_t length)
        : code_(code), length_(length <= MAX_DIGITS ? length : MAX_DIGITS) {
    }

    // direction: +1 або −1, один клац енкодера
    Event onClick(const int direction) {
        if (phase_ != Phase::Entering) return Event::None;

        // Перший клац цифри, в будь-який бік, дає їй значення 0
        if (lastDirection_ == 0) {
            lastDirection_ = direction;
            current_ = 0;
            return Event::DigitStarted;
        }

        // Той самий напрямок: +1, після 9 по колу на 0
        if (direction == lastDirection_) {
            current_ = (current_ + 1) % 10;
            return Event::DigitChanged;
        }

        // Зміна напрямку: цифра підтверджена
        entered_[done_++] = current_;
        if (done_ == length_) return check();

        // Цей самий клац уже дає наступній цифрі значення 0
        lastDirection_ = direction;
        current_ = 0;
        return Event::DigitStarted;
    }

    Event onButton() {
        if (phase_ == Phase::Open) {
            attemptsLeft_ = MAX_ATTEMPTS;
            phase_ = Phase::Entering;
            clearEntry();
            return Event::Relocked;
        }
        if (phase_ == Phase::Locked) return Event::None;

        // Нічого не набрано: скидати нічого, спробу не витрачаємо
        if (done_ == 0 && lastDirection_ == 0) return Event::None;

        // Почав набирати і натиснув: це скидання, і воно витрачає спробу,
        // інакше кнопкою можна було б перебирати код без обмежень
        clearEntry();
        return spendAttempt(Event::Reset);
    }

    // Гетери для екрана й консолі
    Phase phase() const { return phase_; }              // вводимо / відкрито / заблоковано
    uint8_t currentDigit() const { return current_; }   // цифра, яку зараз крутимо
    uint8_t digitsDone() const { return done_; }        // скільки цифр уже підтверджено
    bool digitInProgress() const { return lastDirection_ != 0; } // чи почали крутити поточну цифру
    uint8_t attemptsLeft() const { return attemptsLeft_; }

    // Підтверджена цифра за номером. Номер поза набраними → 0, без читання за межами масиву
    uint8_t digitAt(const uint8_t index) const { return index < done_ ? entered_[index] : 0; }

private:
    Event check() {
        bool match = true;
        for (uint8_t i = 0; i < length_; i++) {
            if (entered_[i] != code_[i]) match = false;
        }
        clearEntry();

        if (match) {
            phase_ = Phase::Open;
            return Event::Granted;
        }
        return spendAttempt(Event::Denied);
    }

    Event spendAttempt(const Event ifSomeLeft) {
        attemptsLeft_--;
        if (attemptsLeft_ == 0) {
            phase_ = Phase::Locked;
            return Event::Lockout;
        }
        return ifSomeLeft;
    }

    void clearEntry() {
        done_ = 0;
        current_ = 0;
        lastDirection_ = 0; // 0 = поточну цифру ще не почато
    }

    const uint8_t *code_;
    uint8_t length_;
    uint8_t entered_[MAX_DIGITS] = {};
    uint8_t done_ = 0;
    uint8_t current_ = 0;
    int lastDirection_ = 0;
    uint8_t attemptsLeft_ = MAX_ATTEMPTS;
    Phase phase_ = Phase::Entering;
};
