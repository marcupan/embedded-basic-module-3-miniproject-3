// Енкодер і його кнопка
#pragma once
#include <stdint.h>

void setupInput();
int pollEncoder();               // +1 / −1 на кожен клац, інакше 0
bool pollButton(uint32_t now);   // true один раз на натискання
