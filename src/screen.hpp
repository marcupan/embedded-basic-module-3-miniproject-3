// Екран SSD1306 по I2C. Дублює консоль; без екрана сейф теж працює
#pragma once
#include <stdint.h>
#include "safe.hpp"

void setupScreen();
void reportScreen();                  // «є / НЕМАЄ», а коли немає — сканує шину
void markScreenDirty();               // перемалювати на найближчому оберті
void setScreenNote(const char *note); // рядок унизу: «Невірний код», «Скинуто»
void updateScreen(uint32_t now, const Safe &safe, uint8_t codeLength);
