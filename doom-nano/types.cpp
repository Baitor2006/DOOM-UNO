#include <stdint.h>
#include <math.h>
#include "types.h"
#include "constants.h"

// OPT: inline template avoids a function-call overhead for a trivial squaring op.
//      Using float means sqrtf() (32-bit) instead of sqrt() (64-bit).
template <class T>
static inline T sq(T v) { return v * v; }

Coords create_coords(float x, float y) {
  return { x, y };
}

// OPT: was sqrt(sq(double)…) — now sqrtf(sq(float)…).
//      Saves ~200 bytes of 64-bit soft-float ROM and is faster at runtime.
uint8_t coords_distance(Coords* a, Coords* b) {
  return (uint8_t)(sqrtf(sq(a->x - b->x) + sq(a->y - b->y)) * DISTANCE_MULTIPLIER);
}

UID create_uid(uint8_t type, uint8_t x, uint8_t y) {
  return ((y << LEVEL_WIDTH_BASE) | x) << 4 | type;
}

uint8_t uid_get_type(UID uid) {
  return uid & 0x0F;
}
