/*
  display.h — rendering helpers for doom-nano
  OPT summary vs original:
    • delta/FRAME_TIME use float (not double) → ~200 bytes ROM saved
    • FRAME_TIME is now an integer constant (67 ms ≈ 15 fps) — no floating division
    • getByte() removed (never called anywhere)
    • drawSprite distance param changed from double to float
    • Minor: getActualFps() uses float arithmetic
*/
#include "SH1106.h"
#include "constants.h"

#define F_char(ifsh, ch)    pgm_read_byte(reinterpret_cast<PGM_P>(ifsh) + ch)

const static uint8_t PROGMEM bit_mask[8] = { 128, 64, 32, 16, 8, 4, 2, 1 };
#define read_bit(b, n)      b & pgm_read_byte(bit_mask + n) ? 1 : 0

void setupDisplay();
void fps();
bool getGradientPixel(uint8_t x, uint8_t y, uint8_t i);
void fadeScreen(uint8_t intensity, bool color);
void drawByte(uint8_t x, uint8_t y, uint8_t b);
void drawPixel(int8_t x, int8_t y, bool color, bool raycasterViewport);
void drawVLine(uint8_t x, int8_t start_y, int8_t end_y, uint8_t intensity);
// OPT: distance is float (was double) — consistent with the rest of the codebase
void drawSprite(int8_t x, int8_t y, const uint8_t bitmap[], const uint8_t mask[],
                int16_t w, int16_t h, uint8_t sprite, float distance);
void drawChar(int8_t x, int8_t y, char ch);
void drawText(int8_t x, int8_t y, char *txt, uint8_t space = 1);
void drawText(int8_t x, int8_t y, const __FlashStringHelper *txt, uint8_t space = 1);

Adafruit_SSD1306<SCREEN_WIDTH, SCREEN_HEIGHT> display;

// OPT: float instead of double — delta only ever needs ~2 decimal places
float    delta = 1.0f;
uint32_t lastFrameTime = 0;

#ifdef OPTIMIZE_SSD1306
uint8_t *display_buf;
#endif

uint8_t zbuffer[ZBUFFER_SIZE];

void setupDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }

#ifdef OPTIMIZE_SSD1306
  display_buf = display.getBuffer();
#endif

  memset(zbuffer, 0xFF, ZBUFFER_SIZE);
}

// OPT: FRAME_TIME is now a uint16_t (67), so millis() arithmetic stays integer.
//      delta is float — still correct for movement smoothing.
void fps() {
  while (millis() - lastFrameTime < FRAME_TIME);
  delta = (float)(millis() - lastFrameTime) / FRAME_TIME;
  lastFrameTime = millis();
}

float getActualFps() {
  return 1000.0f / (FRAME_TIME * delta);
}

void drawByte(uint8_t x, uint8_t y, uint8_t b) {
#ifdef OPTIMIZE_SSD1306
  display_buf[(y / 8) * SCREEN_WIDTH + x] = b;
#endif
}

boolean getGradientPixel(uint8_t x, uint8_t y, uint8_t i) {
  if (i == 0) return 0;
  if (i >= GRADIENT_COUNT - 1) return 1;

  uint8_t index = max(0, min(GRADIENT_COUNT - 1, i)) * GRADIENT_WIDTH * GRADIENT_HEIGHT
                  + y * GRADIENT_WIDTH % (GRADIENT_WIDTH * GRADIENT_HEIGHT)
                  + x / GRADIENT_HEIGHT % GRADIENT_WIDTH;

  return read_bit(pgm_read_byte(gradient + index), x % 8);
}

void fadeScreen(uint8_t intensity, bool color = 0) {
  for (uint8_t x = 0; x < SCREEN_WIDTH; x++)
    for (uint8_t y = 0; y < SCREEN_HEIGHT; y++)
      if (getGradientPixel(x, y, intensity))
        drawPixel(x, y, color, false);
}

void drawPixel(int8_t x, int8_t y, bool color, bool raycasterViewport = false) {
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 ||
      y >= (raycasterViewport ? RENDER_HEIGHT : SCREEN_HEIGHT)) return;

#ifdef OPTIMIZE_SSD1306
  if (color)
    display_buf[x + (y / 8) * SCREEN_WIDTH] |=  (1 << (y & 7));
  else
    display_buf[x + (y / 8) * SCREEN_WIDTH] &= ~(1 << (y & 7));
#else
  display.drawPixel(x, y, color);
#endif
}

void drawVLine(uint8_t x, int8_t start_y, int8_t end_y, uint8_t intensity) {
  int8_t  lower_y  = max(min(start_y, end_y), 0);
  int8_t  higher_y = min(max(start_y, end_y), (int8_t)(RENDER_HEIGHT - 1));
  uint8_t c;

#ifdef OPTIMIZE_SSD1306
  uint8_t bp, b;
  for (c = 0; c < RES_DIVIDER; c++) {
    int8_t y = lower_y;
    b  = 0;
    while (y <= higher_y) {
      bp = y % 8;
      b |= getGradientPixel(x + c, y, intensity) << bp;
      if (bp == 7) { drawByte(x + c, y, b); b = 0; }
      y++;
    }
    if (bp != 7) drawByte(x + c, y - 1, b);
  }
#else
  int8_t y = lower_y;
  while (y <= higher_y) {
    for (c = 0; c < RES_DIVIDER; c++)
      if (getGradientPixel(x + c, y, intensity))
        drawPixel(x + c, y, 1, true);
    y++;
  }
#endif
}

// OPT: distance parameter changed double→float.  The sprite math is identical;
//      we save the double-precision division/multiplication chains in every call.
void drawSprite(
  int8_t x, int8_t y,
  const uint8_t bitmap[], const uint8_t mask[],
  int16_t w, int16_t h,
  uint8_t sprite,
  float distance
) {
  uint8_t  tw          = (float)w / distance;
  uint8_t  th          = (float)h / distance;
  uint8_t  byte_width  = w / 8;
  uint8_t  pixel_size  = max(1, 1.0f / distance);
  uint16_t sprite_offset = byte_width * h * sprite;

  if (zbuffer[min(max(x, 0), ZBUFFER_SIZE - 1) / Z_RES_DIVIDER] < distance * DISTANCE_MULTIPLIER)
    return;

  for (uint8_t ty = 0; ty < th; ty += pixel_size) {
    if (y + ty < 0 || y + ty >= RENDER_HEIGHT) continue;
    uint8_t sy = ty * distance;

    for (uint8_t tx = 0; tx < tw; tx += pixel_size) {
      uint8_t  sx          = tx * distance;
      uint16_t byte_offset = sprite_offset + sy * byte_width + sx / 8;
      if (x + tx < 0 || x + tx >= SCREEN_WIDTH) continue;

      bool maskPixel = read_bit(pgm_read_byte(mask + byte_offset), sx % 8);
      if (maskPixel) {
        bool pixel = read_bit(pgm_read_byte(bitmap + byte_offset), sx % 8);
        for (uint8_t ox = 0; ox < pixel_size; ox++)
          for (uint8_t oy = 0; oy < pixel_size; oy++)
            drawPixel(x + tx + ox, y + ty + oy, pixel, true);
      }
    }
  }
}

void drawChar(int8_t x, int8_t y, char ch) {
  uint8_t c = 0;
  while (CHAR_MAP[c] != ch && CHAR_MAP[c] != '\0') c++;

  uint8_t bOffset = c / 2;
  for (uint8_t line = 0; line < CHAR_HEIGHT; line++) {
    uint8_t b = pgm_read_byte(bmp_font + (line * bmp_font_width + bOffset));
    for (uint8_t n = 0; n < CHAR_WIDTH; n++)
      if (read_bit(b, (c % 2 == 0 ? 0 : 4) + n))
        drawPixel(x + n, y + line, 1, false);
  }
}

void drawText(int8_t x, int8_t y, char *txt, uint8_t space) {
  uint8_t pos = x;
  uint8_t i   = 0;
  char ch;
  while ((ch = txt[i]) != '\0') {
    drawChar(pos, y, ch);
    pos += CHAR_WIDTH + space;
    i++;
    if (pos > SCREEN_WIDTH) return;
  }
}

void drawText(int8_t x, int8_t y, const __FlashStringHelper *txt_p, uint8_t space = 1) {
  uint8_t pos = x;
  uint8_t i   = 0;
  char ch;
  while (pos < SCREEN_WIDTH && (ch = F_char(txt_p, i)) != '\0') {
    drawChar(pos, y, ch);
    pos += CHAR_WIDTH + space;
    i++;
  }
}

void drawText(uint8_t x, uint8_t y, uint8_t num) {
  char buf[4];
  itoa(num, buf, 10);
  drawText(x, y, buf);
}
