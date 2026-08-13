#pragma once
#include <stdint.h>

// 3x5 digit font, one byte per row, 3 LSBs used (MSB = left column).
// Scaled x2 when composed into the heightmap, giving the 2-cell-thick
// strokes of the original 42.fdf map.
#define DIGIT_FONT_COLS 3
#define DIGIT_FONT_ROWS 5

static const uint8_t DIGIT_FONT[10][DIGIT_FONT_ROWS] = {
  {0b111, 0b101, 0b101, 0b101, 0b111},  // 0
  {0b010, 0b110, 0b010, 0b010, 0b111},  // 1
  {0b111, 0b001, 0b111, 0b100, 0b111},  // 2
  {0b111, 0b001, 0b111, 0b001, 0b111},  // 3
  {0b101, 0b101, 0b111, 0b001, 0b001},  // 4
  {0b111, 0b100, 0b111, 0b001, 0b111},  // 5
  {0b111, 0b100, 0b111, 0b101, 0b111},  // 6
  {0b111, 0b001, 0b001, 0b010, 0b010},  // 7
  {0b111, 0b101, 0b111, 0b101, 0b111},  // 8
  {0b111, 0b101, 0b111, 0b001, 0b111},  // 9
};
