#ifndef _splash_h
#define _splash_h

/*
 * Splash sequence:
 *   Screen 1 → skull  "BAITOR"  skull   (centred on 128×64 OLED)
 *   Screen 2 → "PRESENTS" centred
 *
 * Layout:
 *   Two 13×15 skulls at scale ×2 (26×30 px) flank BAITOR at scale ×2.
 *   Word uses 1 px inter-char gap (tight) → 70 px wide.
 *   Total: 26+2+70+2+26 = 126 px, centred with 1 px margin each side.
 *   Skulls vertically centred (y=17); BAITOR vertically centred inside skull height.
 *
 * All glyph data in PROGMEM.
 */

#include <avr/pgmspace.h>
#include "constants.h"

// ─────────────────────────────────────────────────────────────────
//  LETTER GLYPHS  5 px wide × 7 px tall
// ─────────────────────────────────────────────────────────────────
#define GL_W        5
#define GL_H        7
#define GL_G_WORD   1   // inter-char gap (unscaled px) — tighter than default 2

static const uint8_t PROGMEM gl_A[] = {
  0b01110000,
  0b10001000,
  0b10001000,
  0b11111000,
  0b10001000,
  0b10001000,
  0b10001000,
};
static const uint8_t PROGMEM gl_B[] = {
  0b11110000,
  0b10001000,
  0b10001000,
  0b11110000,
  0b10001000,
  0b10001000,
  0b11110000,
};
static const uint8_t PROGMEM gl_E[] = {
  0b11111000,
  0b10000000,
  0b10000000,
  0b11110000,
  0b10000000,
  0b10000000,
  0b11111000,
};
static const uint8_t PROGMEM gl_I[] = {
  0b11111000,
  0b00100000,
  0b00100000,
  0b00100000,
  0b00100000,
  0b00100000,
  0b11111000,
};
static const uint8_t PROGMEM gl_N[] = {
  0b10001000,
  0b11001000,
  0b10101000,
  0b10011000,
  0b10001000,
  0b10001000,
  0b10001000,
};
static const uint8_t PROGMEM gl_O[] = {
  0b01110000,
  0b10001000,
  0b10001000,
  0b10001000,
  0b10001000,
  0b10001000,
  0b01110000,
};
static const uint8_t PROGMEM gl_P[] = {
  0b11110000,
  0b10001000,
  0b10001000,
  0b11110000,
  0b10000000,
  0b10000000,
  0b10000000,
};
static const uint8_t PROGMEM gl_R[] = {
  0b11110000,
  0b10001000,
  0b10001000,
  0b11110000,
  0b10100000,
  0b10010000,
  0b10001000,
};
static const uint8_t PROGMEM gl_S[] = {
  0b01111000,
  0b10000000,
  0b10000000,
  0b01110000,
  0b00001000,
  0b00001000,
  0b11110000,
};
static const uint8_t PROGMEM gl_T[] = {
  0b11111000,
  0b00100000,
  0b00100000,
  0b00100000,
  0b00100000,
  0b00100000,
  0b00100000,
};

// ─────────────────────────────────────────────────────────────────
//  HIGH-QUALITY SKULL  13 px wide × 15 px tall
//
//  Stored as uint16_t per row, MSB = leftmost pixel, bits 15..3 used.
//  At render scale ×2 → 26 px × 30 px on screen.
//
//  Pixel art (# = lit, . = dark):
//    row 0:  . . # # # # # # # # # . .   dome top
//    row 1:  . # # # # # # # # # # # .   dome
//    row 2:  # # # # # # # # # # # # #   dome wide
//    row 3:  # # # # # # # # # # # # #   dome wide
//    row 4:  # . # # # # # # # # # . #   brow shadow
//    row 5:  # . . # # . # . # # . . #   eye sockets
//    row 6:  # . . # # . # . # # . . #   eye sockets
//    row 7:  # # # # # # # # # # # # #   cheekbones
//    row 8:  # # # . # . . . # . # # #   nose cavity
//    row 9:  # # # # # # # # # # # # #   upper jaw
//    row 10: . # # # # # # # # # # # .   jaw
//    row 11: . . # # # # # # # # # . .   chin
//    row 12: . # . # . . . . . # . # .   upper teeth border
//    row 13: . # # . # # # # # . # # .   teeth
//    row 14: . . . . . . . . . . . . .   padding
// ─────────────────────────────────────────────────────────────────
#define SKULL2_W   13
#define SKULL2_H   15

static const uint16_t PROGMEM gl_SKULL2[SKULL2_H] = {
  0b0011111111100000,  // row  0
  0b0111111111110000,  // row  1
  0b1111111111111000,  // row  2
  0b1111111111111000,  // row  3
  0b1011111111101000,  // row  4
  0b1001100101001000,  // row  5
  0b1001100101001000,  // row  6
  0b1111111111111000,  // row  7
  0b1110100010111000,  // row  8
  0b1111111111111000,  // row  9
  0b0111111111110000,  // row 10
  0b0011111111100000,  // row 11
  0b0101000000101000,  // row 12
  0b0110111110110000,  // row 13
  0b0000000000000000,  // row 14
};

// ─────────────────────────────────────────────────────────────────
//  Word arrays
// ─────────────────────────────────────────────────────────────────

#define W_BAITOR_N 6
static const uint8_t* const PROGMEM w_baitor[W_BAITOR_N] = {
  gl_B, gl_A, gl_I, gl_T, gl_O, gl_R
};

#define W_PRES_N 8
static const uint8_t* const PROGMEM w_presents[W_PRES_N] = {
  gl_P, gl_R, gl_E, gl_S, gl_E, gl_N, gl_T, gl_S
};

// ─────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────

// Word pixel width using a custom gap (unscaled)
static int8_t wordPxWidthG(uint8_t n, uint8_t sc, uint8_t gap) {
  return (int8_t)(n * GL_W * sc + (n - 1) * gap * sc);
}

// Draw a single letter glyph at (ox,oy) scaled by sc
static void splashGlyph(const uint8_t* pgmG, int8_t ox, int8_t oy, uint8_t sc) {
  for (uint8_t row = 0; row < GL_H; row++) {
    uint8_t rd = pgm_read_byte(pgmG + row);
    for (uint8_t col = 0; col < GL_W; col++) {
      if ((rd >> (7 - col)) & 1) {
        for (uint8_t sy = 0; sy < sc; sy++)
          for (uint8_t sx = 0; sx < sc; sx++)
            drawPixel(ox + col * sc + sx, oy + row * sc + sy, true, false);
      }
    }
  }
}

// Draw a word (array of glyph pointers) with custom gap
static void splashWordG(const uint8_t* const* arr, uint8_t n,
                        int8_t x0, int8_t y0, uint8_t sc, uint8_t gap) {
  uint8_t stride = GL_W * sc + gap * sc;
  for (uint8_t i = 0; i < n; i++) {
    const uint8_t* glyph = (const uint8_t*) pgm_read_ptr(arr + i);
    splashGlyph(glyph, x0 + (int8_t)(i * stride), y0, sc);
  }
}

// Draw the 13-wide skull using uint16_t row data
static void skull2Glyph(int8_t ox, int8_t oy, uint8_t sc) {
  for (uint8_t row = 0; row < SKULL2_H; row++) {
    uint16_t rd = pgm_read_word(gl_SKULL2 + row);
    for (uint8_t col = 0; col < SKULL2_W; col++) {
      if ((rd >> (15 - col)) & 1) {
        for (uint8_t sy = 0; sy < sc; sy++)
          for (uint8_t sx = 0; sx < sc; sx++)
            drawPixel(ox + col * sc + sx, oy + row * sc + sy, true, false);
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────────
//  Screen 1 — skull  BAITOR  skull   (centred on 128×64)
//
//  Skull at scale ×2  → 26 px wide, 30 px tall
//  BAITOR at scale ×2, gap=1 → 70 px wide, 14 px tall
//  Side gap between skull and word: 2 px
//  Total width: 26+2+70+2+26 = 126 px  (1 px margin each side)
//  Vertical: skulls centred → y=17; word centred inside skull → y=25
// ─────────────────────────────────────────────────────────────────
static void showSplash1() {
  display.clearDisplay();

  const uint8_t skull_sc  = 2;
  const uint8_t word_sc   = 2;
  const uint8_t word_gap  = GL_G_WORD;  // 1 px (unscaled)
  const uint8_t side_gap  = 2;          // px between skull and word

  const uint8_t skull_rw  = SKULL2_W * skull_sc;                         // 26
  const uint8_t skull_rh  = SKULL2_H * skull_sc;                         // 30
  const int8_t  word_w    = wordPxWidthG(W_BAITOR_N, word_sc, word_gap); // 70
  const uint8_t word_h    = GL_H * word_sc;                              // 14

  const uint8_t total_w   = skull_rw + side_gap + (uint8_t)word_w + side_gap + skull_rw;
  const int8_t  x_left    = (int8_t)((SCREEN_WIDTH - (int8_t)total_w) / 2);

  const int8_t  x_skull_l = x_left;
  const int8_t  x_word    = x_left  + (int8_t)skull_rw + (int8_t)side_gap;
  const int8_t  x_skull_r = x_word  + word_w           + (int8_t)side_gap;

  const int8_t  y_skull   = (int8_t)((SCREEN_HEIGHT - (int8_t)skull_rh) / 2);
  const int8_t  y_word    = y_skull + (int8_t)((skull_rh - word_h) / 2);

  skull2Glyph(x_skull_l, y_skull, skull_sc);
  splashWordG(w_baitor, W_BAITOR_N, x_word, y_word, word_sc, word_gap);
  skull2Glyph(x_skull_r, y_skull, skull_sc);

  display.display();
}

// ─────────────────────────────────────────────────────────────────
//  Screen 2 — PRESENTS  (centered)
// ─────────────────────────────────────────────────────────────────
static void showSplash2() {
  display.clearDisplay();

  const uint8_t sc  = 2;
  const uint8_t gap = 2;
  int8_t w = wordPxWidthG(W_PRES_N, sc, gap);
  int8_t x = (SCREEN_WIDTH  - w) / 2;
  int8_t y = (SCREEN_HEIGHT - GL_H * sc) / 2;
  splashWordG(w_presents, W_PRES_N, x, y, sc, gap);

  display.display();
}

// ─────────────────────────────────────────────────────────────────
//  Public entry — call once from loop()
// ─────────────────────────────────────────────────────────────────
#define SPLASH_HOLD_MS 2800

void loopSplash() {
  showSplash1();
  uint32_t t0 = millis();
  while (millis() - t0 < SPLASH_HOLD_MS) {
    #ifdef SNES_CONTROLLER
    getControllerData();
    #endif
    if (input_fire()) goto splash_done;
    delay(16);
  }

  showSplash2();
  t0 = millis();
  while (millis() - t0 < SPLASH_HOLD_MS) {
    #ifdef SNES_CONTROLLER
    getControllerData();
    #endif
    if (input_fire()) goto splash_done;
    delay(16);
  }

splash_done:;
}

#endif // _splash_h