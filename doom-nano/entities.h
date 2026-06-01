#ifndef _entities_h
#define _entities_h

#include "types.h"

// Shortcuts
#define create_player(x, y)   { \
    create_coords((float)(x) + 0.5f, (float)(y) + 0.5f), \
    create_coords(1.0f, 0.0f), \
    create_coords(0.0f, -0.66f), \
    0.0f, \
    100,  \
    0,    \
    50,   \
  }

#define create_enemy(x, y)            create_entity(E_ENEMY,    x, y, S_STAND, 100)
#define create_boss(x, y)             create_entity(E_BOSS,     x, y, S_STAND, BOSS_HEALTH)
#define create_medikit(x, y)          create_entity(E_MEDIKIT,  x, y, S_STAND, 0)
#define create_key(x, y)              create_entity(E_KEY,      x, y, S_STAND, 0)
#define create_fireball(x, y, dir)    create_entity(E_FIREBALL, x, y, S_STAND, dir)

// entity statuses
#define S_STAND               0
#define S_ALERT               1
#define S_FIRING              2
#define S_MELEE               3
#define S_HIT                 4
#define S_DEAD                5
#define S_HIDDEN              6
#define S_OPEN                7
#define S_CLOSE               8

// OPT: Player uses float (not double) for pos/dir/plane/velocity.
//      Each float is 4 bytes vs 8 bytes for double — saves 28 bytes of RAM here.
struct Player {
  Coords  pos;
  Coords  dir;
  Coords  plane;
  float   velocity;
  uint8_t health;
  uint8_t keys;
  uint8_t shots;
};

// OPT: Entity struct unchanged in layout (already uint8_t for state/health/distance/timer).
//      Coords uses float (4 bytes each) — already optimal for this target.
struct Entity {
  UID     uid;
  Coords  pos;
  uint8_t state;
  uint8_t health;   // angle for fireballs
  uint8_t distance;
  uint8_t timer;
};

struct StaticEntity {
  UID     uid;
  uint8_t x;
  uint8_t y;
  bool    active;
};

Entity       create_entity(uint8_t type, uint8_t x, uint8_t y,
                           uint8_t initialState, uint8_t initialHealth);
StaticEntity create_static_entity(UID uid, uint8_t x, uint8_t y, bool active);

#endif
