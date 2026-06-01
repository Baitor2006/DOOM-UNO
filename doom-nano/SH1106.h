/*!
 * @file SH1106.h
 *
 * Driver léger pour écran OLED 1.3" I2C basé sur SH1106.
 * Remplace Adafruit_SSD1306 + TWI_Master par Wire natif Arduino.
 *
 * Optimisations RAM :
 *  - Buffer framebuffer statique dans la classe (1024 bytes, inchangé)
 *  - Pas de heap allocation
 *  - Envoi par pages (8 rows) pour économiser la pile
 *  - Wire.h standard Arduino (moins overhead que TWI custom)
 */

#ifndef _SH1106_H_
#define _SH1106_H_

#include <Arduino.h>
#include <Wire.h>
#include <avr/pgmspace.h>
#include "constants.h"

// Couleurs (compatibilité avec l'ancien code)
#define SSD1306_BLACK    0
#define SSD1306_WHITE    1
#define SSD1306_INVERSE  2
#ifndef NO_ADAFRUIT_SSD1306_COLOR_COMPATIBILITY
  #define BLACK   SSD1306_BLACK
  #define WHITE   SSD1306_WHITE
  #define INVERSE SSD1306_INVERSE
#endif

// Commandes SH1106
#define SH1106_DISPLAYOFF          0xAE
#define SH1106_DISPLAYON           0xAF
#define SH1106_SETCONTRAST         0x81
#define SH1106_NORMALDISPLAY       0xA6
#define SH1106_INVERTDISPLAY       0xA7
#define SH1106_DISPLAYALLON_RESUME 0xA4
#define SH1106_SEGREMAP            0xA0  // + 0x1 pour flip H
#define SH1106_COMSCANDEC          0xC8  // flip V
#define SH1106_SETMULTIPLEX        0xA8
#define SH1106_SETDISPLAYOFFSET    0xD3
#define SH1106_SETDISPLAYCLOCKDIV  0xD5
#define SH1106_SETPRECHARGE        0xD9
#define SH1106_SETCOMPINS          0xDA
#define SH1106_SETVCOMDETECT       0xDB
#define SH1106_CHARGEPUMP          0x8D  // non standard, utilise 0xAD+0x8B
#define SH1106_SETSTARTLINE        0x40
#define SH1106_SETPAGEADDR         0xB0  // + page (0..7)
#define SH1106_SETLOWCOLUMN        0x00  // + low nibble
#define SH1106_SETHIGHCOLUMN       0x10  // + high nibble
// Le SH1106 a un offset de 2 colonnes par rapport au SSD1306
#define SH1106_COL_OFFSET          2

// SSD1306_SWITCHCAPVCC pour compatibilité avec l'appel begin() existant
#define SSD1306_SWITCHCAPVCC 0x02
#define SSD1306_EXTERNALVCC  0x01

/*
 * Classe template identique à l'ancienne API Adafruit_SSD1306<W,H>
 * pour ne pas toucher display.h ni doom-nano.ino
 */
template <uint8_t WIDTH, uint8_t HEIGHT>
class Adafruit_SSD1306 {
public:
  Adafruit_SSD1306() = default;
  ~Adafruit_SSD1306() = default;

  // Identique à l'ancienne signature
  bool begin(uint8_t switchvcc = SSD1306_SWITCHCAPVCC, uint8_t i2caddr = 0);

  void display();
  void clearDisplay();
  void invertDisplay(bool i);

  void drawPixel(int16_t x, int16_t y, uint16_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  bool getPixel(int16_t x, int16_t y);
  uint8_t* getBuffer();
  void clearRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
  void drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
                  int16_t w, int16_t h, uint16_t color);

private:
  // Envoie une commande sur I2C
  inline void cmd(uint8_t c) {
    Wire.beginTransmission(_addr);
    Wire.write(0x00);   // Co=0, D/C=0 → commande
    Wire.write(c);
    Wire.endTransmission();
  }

  // Envoie deux octets de commande
  inline void cmd2(uint8_t c1, uint8_t c2) {
    Wire.beginTransmission(_addr);
    Wire.write(0x00);
    Wire.write(c1);
    Wire.write(c2);
    Wire.endTransmission();
  }

  void drawFastVLineInternal(int16_t x, int16_t y, int16_t h, uint16_t color);

  // 1024 bytes de framebuffer (128×64/8), stockés en RAM statique
  uint8_t buffer[WIDTH * ((HEIGHT + 7) / 8)];
  uint8_t _addr;
};

// Instanciation explicite pour éviter le linker de supprimer les méthodes
template class Adafruit_SSD1306<SCREEN_WIDTH, SCREEN_HEIGHT>;

// ============================================================
//  IMPLÉMENTATION (inline dans le .h pour template)
// ============================================================

template <uint8_t WIDTH, uint8_t HEIGHT>
bool Adafruit_SSD1306<WIDTH, HEIGHT>::begin(uint8_t vcs, uint8_t i2caddr) {
  _addr = i2caddr ? i2caddr : 0x3C;

  Wire.begin();
  Wire.setClock(400000UL); // Fast mode I2C (400 kHz)

  clearDisplay();

  // Séquence d'init SH1106
  cmd(SH1106_DISPLAYOFF);
  cmd2(SH1106_SETDISPLAYCLOCKDIV, 0x80);
  cmd2(SH1106_SETMULTIPLEX, HEIGHT - 1);
  cmd2(SH1106_SETDISPLAYOFFSET, 0x00);
  cmd(SH1106_SETSTARTLINE | 0x00);

  // Pompe de charge interne (SH1106 : 0xAD, 0x8B)
  cmd(0xAD);
  cmd((vcs == SSD1306_EXTERNALVCC) ? 0x8A : 0x8B);

  cmd(SH1106_SEGREMAP | 0x01);   // flip H
  cmd(SH1106_COMSCANDEC);        // flip V

  // Pins COM : 128×64 → 0x12, 128×32 → 0x02
  cmd2(SH1106_SETCOMPINS, ((WIDTH == 128) && (HEIGHT == 64)) ? 0x12 : 0x02);
  cmd2(SH1106_SETCONTRAST, (vcs == SSD1306_EXTERNALVCC) ? 0x9F : 0xCF);
  cmd2(SH1106_SETPRECHARGE, (vcs == SSD1306_EXTERNALVCC) ? 0x22 : 0xF1);
  cmd2(SH1106_SETVCOMDETECT, 0x40);
  cmd(SH1106_DISPLAYALLON_RESUME);
  cmd(SH1106_NORMALDISPLAY);
  cmd(SH1106_DISPLAYON);

  return true;
}

/*
 * Envoie le framebuffer à l'écran.
 * Le SH1106 utilise un adressage par pages + colonnes (pas streaming).
 * On envoie chaque page (8 lignes) en un seul transfert I2C de 128 octets.
 * Wire.write() avec buffer Arduino peut envoyer 32 octets max par défaut,
 * donc on envoie la page en tranches de WIRE_MAX_WRITE = 30 octets utiles
 * (30 + 2 overhead = 32).
 */
template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::display() {
  constexpr uint8_t PAGES = (HEIGHT + 7) / 8;
  constexpr uint8_t CHUNK = 30; // octets data par transfert I2C (Wire buffer = 32, -1 addr -1 control byte)

  for (uint8_t page = 0; page < PAGES; page++) {
    // Positionne le curseur SH1106 sur la page + colonne 0 (avec offset)
    cmd(SH1106_SETPAGEADDR | page);
    cmd(SH1106_SETHIGHCOLUMN | ((SH1106_COL_OFFSET >> 4) & 0x0F));
    cmd(SH1106_SETLOWCOLUMN  | (SH1106_COL_OFFSET & 0x0F));

    uint8_t *ptr = buffer + (uint16_t)page * WIDTH;
    uint8_t remaining = WIDTH;

    while (remaining) {
      uint8_t n = min(remaining, CHUNK);
      Wire.beginTransmission(_addr);
      Wire.write(0x40); // Co=0, D/C=1 → données
      for (uint8_t i = 0; i < n; i++) {
        Wire.write(ptr[i]);
      }
      Wire.endTransmission();
      ptr += n;
      remaining -= n;
    }
  }
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::clearDisplay() {
  memset(buffer, 0, sizeof(buffer));
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::invertDisplay(bool i) {
  cmd(i ? SH1106_INVERTDISPLAY : SH1106_NORMALDISPLAY);
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (x >= WIDTH) || (y < 0) || (y >= HEIGHT)) return;
  switch (color) {
    case SSD1306_WHITE:   buffer[x + (y / 8) * WIDTH] |=  (1 << (y & 7)); break;
    case SSD1306_BLACK:   buffer[x + (y / 8) * WIDTH] &= ~(1 << (y & 7)); break;
    case SSD1306_INVERSE: buffer[x + (y / 8) * WIDTH] ^=  (1 << (y & 7)); break;
  }
}

template <uint8_t WIDTH, uint8_t HEIGHT>
bool Adafruit_SSD1306<WIDTH, HEIGHT>::getPixel(int16_t x, int16_t y) {
  if ((x < 0) || (x >= WIDTH) || (y < 0) || (y >= HEIGHT)) return false;
  return (buffer[x + (y / 8) * WIDTH] & (1 << (y & 7)));
}

template <uint8_t WIDTH, uint8_t HEIGHT>
uint8_t* Adafruit_SSD1306<WIDTH, HEIGHT>::getBuffer() {
  return buffer;
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  drawFastVLineInternal(x, y, h, color);
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::drawFastVLineInternal(int16_t x, int16_t __y, int16_t __h, uint16_t color) {
  if ((x < 0) || (x >= WIDTH)) return;
  if (__y < 0) { __h += __y; __y = 0; }
  if ((__y + __h) > HEIGHT) { __h = HEIGHT - __y; }
  if (__h <= 0) return;

  uint8_t y = __y, h = __h;
  uint8_t *pBuf = &buffer[(y / 8) * WIDTH + x];

  uint8_t mod = y & 7;
  if (mod) {
    mod = 8 - mod;
    static const uint8_t PROGMEM premask[8] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
    uint8_t mask = pgm_read_byte(&premask[mod]);
    if (h < mod) mask &= (0xFF >> (mod - h));
    switch (color) {
      case SSD1306_WHITE:   *pBuf |=  mask; break;
      case SSD1306_BLACK:   *pBuf &= ~mask; break;
      case SSD1306_INVERSE: *pBuf ^=  mask; break;
    }
    pBuf += WIDTH;
  }

  if (h >= mod) {
    h -= mod;
    if (h >= 8) {
      if (color == SSD1306_INVERSE) {
        do { *pBuf ^= 0xFF; pBuf += WIDTH; h -= 8; } while (h >= 8);
      } else {
        uint8_t val = (color != SSD1306_BLACK) ? 0xFF : 0x00;
        do { *pBuf = val; pBuf += WIDTH; h -= 8; } while (h >= 8);
      }
    }
    if (h) {
      static const uint8_t PROGMEM postmask[8] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };
      uint8_t mask = pgm_read_byte(&postmask[h & 7]);
      switch (color) {
        case SSD1306_WHITE:   *pBuf |=  mask; break;
        case SSD1306_BLACK:   *pBuf &= ~mask; break;
        case SSD1306_INVERSE: *pBuf ^=  mask; break;
      }
    }
  }
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::clearRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  for (uint8_t i = x; i < x + w; i++) {
    drawFastVLineInternal(i, y, h, SSD1306_BLACK);
  }
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::drawBitmap(
  int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color)
{
  int16_t byteWidth = (w + 7) / 8;
  uint8_t byte = 0;
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7) byte <<= 1;
      else       byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      if (byte & 0x80) drawPixel(x + i, y, color);
    }
  }
}

#endif // _SH1106_H_
