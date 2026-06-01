/*
 * map.h — Full-screen level map shown before gameplay starts.
 *
 * The level is 64 cols × 57 rows. We render it at 2px wide × 1px tall
 * per cell → 128 × 57 px, which fills the raycaster viewport exactly.
 * The bottom 8 rows (HUD strip) show a legend and "PRESS FIRE".
 *
 * Cell colours (monochrome OLED — white or black):
 * Wall        → white pixel(s)
 * Floor       → black (blank)
 * Door        → alternating white/black checkerboard column
 * Locked door → thin vertical line (distinct from walls and regular doors)
 * Enemy       → white (treated as notable floor marker)
 * Player      → blinking white dot (centre of cell)
 * Key / Medikit / Exit → white pixel(s) — same as floor markers
 */

#ifndef _map_h
#define _map_h

#include <avr/pgmspace.h>

// Forward declaration (defined in doom-nano.ino)
uint8_t getBlockAt(const uint8_t level[], uint8_t x, uint8_t y);

// Width of each cell in screen pixels (height is always 1 px)
#define MAP_CELL_W  2

// Where the map starts on screen
#define MAP_OX      0
#define MAP_OY      0

// ─── internal helpers ────────────────────────────────────────────────────────

// Draw one map cell at level coords (lx, ly)
// sy = screen y, sx = screen x (already pre-computed)
static inline void _drawMapCell(uint8_t block, uint8_t sx, uint8_t sy) {
  switch (block) {
    case E_WALL:
      drawPixel(sx,     sy, true,  false);
      drawPixel(sx + 1, sy, true,  false);
      break;

    case E_DOOR:
      // Checkerboard pattern: alternate left and right pixels depending on the row.
      // This ensures the door is visible on any row and forms a distinct grid pattern.
      if (sy % 2 == 0) {
        drawPixel(sx,     sy, true, false);
      } else {
        drawPixel(sx + 1, sy, true, false);
      }
      break;

    case E_LOCKEDDOOR:
      // Locked door: draw a thin vertical bar (left pixel only) on every row.
      // This makes it visually distinct from both solid walls and checkerboard doors.
      drawPixel(sx,     sy, true, false);
      break;

    case E_EXIT:
      // Exit: bright flashing double-pixel (handled in caller for flash)
      // Draw solid X: both pixels on adjacent rows too
      drawPixel(sx,     sy, true, false);
      drawPixel(sx + 1, sy, true, false);
      break;

    case E_BOSS:
      // Boss: large flashing marker — 2×3 block
      drawPixel(sx,     sy,     true, false);
      drawPixel(sx + 1, sy,     true, false);
      if (sy > 0) {
        drawPixel(sx,     sy - 1, true, false);
        drawPixel(sx + 1, sy - 1, true, false);
      }
      if (sy + 1 < RENDER_HEIGHT) {
        drawPixel(sx,     sy + 1, true, false);
        drawPixel(sx + 1, sy + 1, true, false);
      }
      break;

    // Floor, enemies, items → leave black (blank)
    default:
      break;
  }
}

// ─── public entry point ──────────────────────────────────────────────────────

/*
 * loopMapScreen()
 *
 * Renders the full level map, then waits for the player to press FIRE.
 * Call this from loop() in doom-nano.ino just before GAME_PLAY starts.
 *
 * The player start position is found by scanning the level (same as
 * initializeLevel does) and is drawn as a blinking dot.
 */
void loopMapScreen() {
  // ── find player start ────────────────────────────────────────────
  uint8_t px = 0, py = 0;
  bool found = false;
  for (int8_t y = LEVEL_HEIGHT - 1; y >= 0 && !found; y--) {
    for (uint8_t x = 0; x < LEVEL_WIDTH && !found; x++) {
      if (getBlockAt(sto_level_1, x, y) == E_PLAYER) {
        px = x;
        py = y;
        found = true;
      }
    }
  }

  // ── draw static map once ─────────────────────────────────────────
  display.clearDisplay();

  for (uint8_t ly = 0; ly < LEVEL_HEIGHT; ly++) {
    // Level y=0 is the bottom row in the array (stored top-to-bottom inverted).
    // getBlockAt already handles the inversion, so we draw from
    // level-row 0 (bottom of map) at the bottom of the screen viewport.
    // We want the top of the screen to show the top of the map, so invert:
    uint8_t sy = MAP_OY + (LEVEL_HEIGHT - 1 - ly); // screen y

    if (sy >= RENDER_HEIGHT) continue; // stay within raycaster viewport

    for (uint8_t lx = 0; lx < LEVEL_WIDTH; lx++) {
      uint8_t sx = MAP_OX + lx * MAP_CELL_W;
      if (sx + 1 >= SCREEN_WIDTH) continue;

      uint8_t block = getBlockAt(sto_level_1, lx, ly);
      _drawMapCell(block, sx, sy);
    }
  }

  // ── HUD strip: legend text ────────────────────────────────────────
  // Row 57-63 = HUD area (8 px tall)
  // drawText uses the custom 4×6 font from sprites.h
  drawText(2,  58, F("MAP"));
  drawText(80, 58, F("FIRE"));   // "PRESS" won't fit, abbreviate

  // ── blink loop ───────────────────────────────────────────────────
  uint32_t lastBlink = 0;
  bool     blinkOn   = true;
  bool     waitFire  = true;

  // Convert player level coords → screen coords
  uint8_t dot_sx = MAP_OX + px * MAP_CELL_W;
  uint8_t dot_sy = MAP_OY + (LEVEL_HEIGHT - 1 - py);

  // Find exit position for blinking
  uint8_t ex_sx = 0, ex_sy = 0;
  for (uint8_t y = 0; y < LEVEL_HEIGHT && ex_sx == 0; y++) {
    for (uint8_t x = 0; x < LEVEL_WIDTH && ex_sx == 0; x++) {
      if (getBlockAt(sto_level_1, x, y) == E_EXIT) {
        ex_sx = MAP_OX + x * MAP_CELL_W;
        ex_sy = MAP_OY + (LEVEL_HEIGHT - 1 - y);
      }
    }
  }

  while (waitFire) {
    #ifdef SNES_CONTROLLER
    getControllerData();
    #endif

    if (input_fire()) {
      waitFire = false;
      break;
    }

    // Blink the player dot every 400 ms
    uint32_t now = millis();
    if (now - lastBlink >= 400) {
      lastBlink = now;
      blinkOn = !blinkOn;

      // Draw / erase the two player pixels
      bool c = blinkOn;
      if (dot_sx + 1 < SCREEN_WIDTH && dot_sy < RENDER_HEIGHT) {
        drawPixel(dot_sx,     dot_sy, c, false);
        drawPixel(dot_sx + 1, dot_sy, c, false);
      }

      // Blink the exit marker (opposite phase from player so it always shows one)
      if (ex_sx + 1 < SCREEN_WIDTH && ex_sy < RENDER_HEIGHT) {
        // Draw hollow/filled alternation for exit
        drawPixel(ex_sx,     ex_sy, !c, false);
        drawPixel(ex_sx + 1, ex_sy, !c, false);
        if (ex_sy > 0) {
          drawPixel(ex_sx,     ex_sy - 1, !c, false);
          drawPixel(ex_sx + 1, ex_sy - 1, !c, false);
        }
      }

      // Also blink the "FIRE" prompt so the player knows to act
      display.clearRect(80, 58, 48, 6);
      if (blinkOn) drawText(80, 58, F("FIRE"));

      display.display();
    }
  }
}
#endif // _map_h
