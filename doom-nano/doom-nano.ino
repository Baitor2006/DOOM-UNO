#include "constants.h"
#include "level.h"
#include "sprites.h"
#include "input.h"
#include "entities.h"
#include "types.h"
#include "display.h"
#include "sound.h"
#include "splash.h"
#include "map.h"

// Useful macros
#define swap(a, b)            do { typeof(a) temp = a; a = b; b = temp; } while (0)
#define sign(a, b)            (float) (a > b ? 1 : (b > a ? -1 : 0))

// scenes (also in constants.h: INTRO=0, GAME_PLAY=1)
#define SPLASH                2
#define MAP_SCREEN            3
// WIN_SCREEN = 4 (defined in constants.h)

// general
uint8_t scene = SPLASH;   // start at splash
bool exit_scene = false;
bool invert_screen = false;
uint8_t flash_screen = 0;

// boss tracking
bool boss_spawned = false;
bool boss_dead    = false;
uint8_t boss_burst_left  = 0;  // fireballs still queued in a burst
uint8_t boss_burst_timer = 0;  // ticks between burst shots

// cheat codes — hold combo for CHEAT_HOLD_MS ms to activate
#define CHEAT_HOLD_MS   1000   // ms to hold combo
uint32_t cheat_tp_start   = 0; // "teleport to boss" combo timer  (LEFT+FIRE)
uint32_t cheat_kill_start = 0; // "kill boss" combo timer          (RIGHT+FIRE)

// shotgun reload animation
uint8_t rc1 = 0;      // reload frame selector: 0=idle, 1=frame1, 2=frame2, 3=hidden
uint8_t r = 0;        // reload tick counter
bool reload1 = false; // true while reload animation is in progress

// game
// player and entities
Player player;
Entity entity[MAX_ENTITIES];
StaticEntity static_entity[MAX_STATIC_ENTITIES];
uint8_t num_entities = 0;
uint8_t num_static_entities = 0;

// Unlocked locked-door positions (UID). 0 = empty slot.
uint16_t unlocked_doors[6] = {0, 0, 0, 0, 0, 0};

void setup(void) {
  setupDisplay();
  input_setup();
  sound_init();
}

// Jump to another scene
void jumpTo(uint8_t target_scene) {
  scene = target_scene;
  exit_scene = true;
}

// Finds the player in the map
void initializeLevel(const uint8_t level[]) {
  for (int8_t y = LEVEL_HEIGHT - 1; y >= 0; y--) {
    for (uint8_t x = 0; x < LEVEL_WIDTH; x++) {
      uint8_t block = getBlockAt(level, x, y);

      if (block == E_PLAYER) {
        player = create_player(x, y);
        return;
      }

      // todo create other static entities
    }
  }
}

uint8_t getBlockAt(const uint8_t level[], uint8_t x, uint8_t y) {
  if (x >= LEVEL_WIDTH || y >= LEVEL_HEIGHT) {
    return E_FLOOR;
  }

  // y is read in inverse order
  uint16_t idx = ((uint16_t)(LEVEL_HEIGHT - 1 - y) * LEVEL_WIDTH + x) >> 1;
  uint8_t b = pgm_read_byte(level + idx);
  return (x & 1) ? (b & 0x0F) : (b >> 4);
}

bool isSpawned(UID uid) {
  for (uint8_t i = 0; i < num_entities; i++) {
    if (entity[i].uid == uid) return true;
  }

  return false;
}

bool isStatic(UID uid) {
  for (uint8_t i = 0; i < num_static_entities; i++) {
    if (static_entity[i].uid == uid) return true;
  }

  return false;
}

void spawnEntity(uint8_t type, uint8_t x, uint8_t y) {
  // Limit the number of spawned entities
  if (num_entities >= MAX_ENTITIES) {
    return;
  }

  // todo: read static entity status
  
  switch (type) {
    case E_ENEMY:
      entity[num_entities] = create_enemy(x, y);
      num_entities++;
      break;

    case E_BOSS:
      if (!boss_spawned) {
        entity[num_entities] = create_boss(x, y);
        num_entities++;
        boss_spawned = true;
      }
      break;

    case E_KEY:
      entity[num_entities] = create_key(x, y);
      num_entities++;
      break;

    case E_MEDIKIT:
      entity[num_entities] = create_medikit(x, y);
      num_entities++;
      break;
  }
}

void spawnFireball(float x, float y) {
  // Limit the number of spawned entities
  if (num_entities >= MAX_ENTITIES) {
    return;
  }

  UID uid = create_uid(E_FIREBALL, x, y);
  // Remove if already exists, don't throw anything. Not the best, but shouldn't happen too often
  if (isSpawned(uid)) return;

  // Calculate direction. 32 angles
  int16_t dir = FIREBALL_ANGLES + atan2f(y - player.pos.y, x - player.pos.x) / PI * FIREBALL_ANGLES;
  if (dir < 0) dir += FIREBALL_ANGLES * 2;
  entity[num_entities] = create_fireball(x, y, dir);
  num_entities++;
}

void removeEntity(UID uid, bool makeStatic = false) {
  uint8_t i = 0;
  bool found = false;

  while (i < num_entities) {
    if (!found && entity[i].uid == uid) {
      // todo: doze it
      found = true;
      num_entities--;
    }

    // displace entities
    if (found) {
      entity[i] = entity[i + 1];
    }

    i++;
  }
}

void removeStaticEntity(UID uid) {
  uint8_t i = 0;
  bool found = false;

  while (i < num_static_entities) {
    if (!found && static_entity[i].uid == uid) {
      found = true;
      num_static_entities--;
    }

    // displace entities
    if (found) {
      static_entity[i] = static_entity[i + 1];
    }

    i++;
  }
}

UID detectCollision(const uint8_t level[], Coords *pos, float relative_x, float relative_y, bool only_walls = false) {
  // Wall collision
  uint8_t round_x = int(pos->x + relative_x);
  uint8_t round_y = int(pos->y + relative_y);
  uint8_t block = getBlockAt(level, round_x, round_y);

  if (block == E_WALL) {
    playSound(hit_wall_snd, HIT_WALL_SND_LEN);
    return create_uid(block, round_x, round_y);
  }

  if (block == E_LOCKEDDOOR) {
    UID door_uid = create_uid(block, round_x, round_y);
    // Check if already unlocked
    bool already_open = false;
    for (uint8_t d = 0; d < 6; d++) {
      if (unlocked_doors[d] == door_uid) { already_open = true; break; }
    }
    if (!already_open) {
      if (player.keys > 0) {
        // Use a key to unlock
        player.keys--;
        updateHud();
        playSound(unlock_door_snd, UNLOCK_DOOR_SND_LEN);
        for (uint8_t d = 0; d < 6; d++) {
          if (unlocked_doors[d] == 0) { unlocked_doors[d] = door_uid; break; }
        }
      } else {
        // Blocked — no key
        playSound(locked_door_snd, LOCKED_DOOR_SND_LEN);
        return create_uid(block, round_x, round_y);
      }
    }
  }

  if (only_walls) {
    return UID_null;
  }

  // Entity collision
  for (uint8_t i=0; i < num_entities; i++) {
    // Don't collide with itself
    if (&(entity[i].pos) == pos) {
      continue;
    }

    uint8_t type = uid_get_type(entity[i].uid);

    // Only ALIVE enemy/boss collision
    if ((type != E_ENEMY && type != E_BOSS) || entity[i].state == S_DEAD || entity[i].state == S_HIDDEN) {
      continue;
    }

    Coords new_coords = { entity[i].pos.x - relative_x, entity[i].pos.y - relative_y };
    uint8_t distance = coords_distance(pos, &new_coords);

    // Check distance and if it's getting closer
    if (distance < ENEMY_COLLIDER_DIST && distance < entity[i].distance) {
      return entity[i].uid;
    }
  }

  return UID_null;
}

// Shoot
void fire() {
  playSound(shoot_snd, SHOOT_SND_LEN);

  if (player.shots > 0) player.shots--;
  updateHud();

  for (uint8_t i = 0; i < num_entities; i++) {
    // Shoot only ALIVE enemies and the boss
    uint8_t etype = uid_get_type(entity[i].uid);
    if ((etype != E_ENEMY && etype != E_BOSS) || entity[i].state == S_DEAD || entity[i].state == S_HIDDEN) {
      continue;
    }

    Coords transform = translateIntoView(&(entity[i].pos));
    if (fabsf(transform.x) < 20 && transform.y > 0) {
      uint8_t damage = GUN_MAX_DAMAGE;
      if (etype == E_BOSS) damage = damage / 2; // boss has thick hide
      if (damage > 0) {
        entity[i].health = max(0, entity[i].health - damage);
        entity[i].state = S_HIT;
        entity[i].timer = 4;
      }
    }
  }
}

// Update coords if possible. Return the collided uid, if any
UID updatePosition(const uint8_t level[], Coords *pos, float relative_x, float relative_y, bool only_walls = false) {
  UID collide_x = detectCollision(level, pos, relative_x, 0, only_walls);
  UID collide_y = detectCollision(level, pos, 0, relative_y, only_walls);

  if (!collide_x) pos->x += relative_x;
  if (!collide_y) pos->y += relative_y;

  return collide_x || collide_y || UID_null;
}

void updateEntities(const uint8_t level[]) {
  uint8_t i = 0;
  while (i < num_entities) {
    // update distance
    entity[i].distance = coords_distance(&(player.pos), &(entity[i].pos));

    // Run the timer. Works with actual frames.
    // OPT: delta is now float — this could be re-enabled with minimal cost
    if (entity[i].timer > 0) entity[i].timer--;

    // too far away. put it in doze mode
    if (entity[i].distance > MAX_ENTITY_DISTANCE) {
      removeEntity(entity[i].uid);
      // don't increase 'i', since current one has been removed
      continue;
    }

    // bypass render if hidden
    if (entity[i].state == S_HIDDEN) {
      i++;
      continue;
    }

    uint8_t type = uid_get_type(entity[i].uid);

    switch (type) {
      case E_ENEMY: {
          // Enemy "IA"
          if (entity[i].health == 0) {
            if (entity[i].state != S_DEAD) {
              entity[i].state = S_DEAD;
              entity[i].timer = 6;
            }
          } else  if (entity[i].state == S_HIT) {
            if (entity[i].timer == 0) {
              // Back to alert state
              entity[i].state = S_ALERT;
              entity[i].timer = 40;     // delay next fireball thrown
            }
          } else if (entity[i].state == S_FIRING) {
            if (entity[i].timer == 0) {
              // Back to alert state
              entity[i].state = S_ALERT;
              entity[i].timer = 40;     // delay next fireball throwm
            }
          } else {
            // ALERT STATE
            if (entity[i].distance > ENEMY_MELEE_DIST && entity[i].distance < MAX_ENEMY_VIEW) {
              if (entity[i].state != S_ALERT) {
                entity[i].state = S_ALERT;
                entity[i].timer = 20;   // used to throw fireballs
              } else {
                if (entity[i].timer == 0) {
                  // Throw a fireball
                  spawnFireball(entity[i].pos.x, entity[i].pos.y);
                  entity[i].state = S_FIRING;
                  entity[i].timer = 6;
                } else {
                  // move towards to the player.
                  updatePosition(
                    level,
                    &(entity[i].pos),
                    sign(player.pos.x, entity[i].pos.x) * ENEMY_SPEED * delta,
                    sign(player.pos.y, entity[i].pos.y) * ENEMY_SPEED * delta,
                    true
                  );
                }
              }
            } else if (entity[i].distance <= ENEMY_MELEE_DIST) {
              if (entity[i].state != S_MELEE) {
                // Preparing the melee attack
                entity[i].state = S_MELEE;
                entity[i].timer = 10;
              } else if (entity[i].timer == 0) {
                // Melee attack
                player.health = max(0, player.health - ENEMY_MELEE_DAMAGE);
                entity[i].timer = 14;
                flash_screen = 1;
                updateHud();
              }
            } else {
              // stand
              entity[i].state = S_STAND;
            }
          }
          break;
        }

      case E_FIREBALL: {
          if (entity[i].distance < FIREBALL_COLLIDER_DIST) {
            // Hit the player and disappear
            player.health = max(0, player.health - ENEMY_FIREBALL_DAMAGE);
            flash_screen = 1;
            updateHud();
            removeEntity(entity[i].uid);
            continue; // continue in the loop
          } else {
            // Move. Only collide with walls.
            // Note: using health to store the angle of the movement
            UID collided = updatePosition(
              level,
              &(entity[i].pos),
              cosf((float) entity[i].health / FIREBALL_ANGLES * PI) * FIREBALL_SPEED,
              sinf((float) entity[i].health / FIREBALL_ANGLES * PI) * FIREBALL_SPEED,
              true
            );

            if (collided) {
              removeEntity(entity[i].uid);
              continue; // continue in the entity check loop
            }
          }
          break;
        }

      case E_MEDIKIT: {
          if (entity[i].distance < ITEM_COLLIDER_DIST) {
            // pickup
            playSound(medkit_snd, MEDKIT_SND_LEN);
            entity[i].state = S_HIDDEN;
            player.health = min(100, player.health + 50);
            updateHud();
            flash_screen = 1;
          }
          break;
        }

      case E_KEY: {
          if (entity[i].distance < ITEM_COLLIDER_DIST) {
            // pickup
            playSound(get_key_snd, GET_KEY_SND_LEN);
            entity[i].state = S_HIDDEN;
            player.keys++;
            updateHud();
            flash_screen = 1;
          }
          break;
        }

      case E_BOSS: {
          if (entity[i].health == 0) {
            // Boss death sequence
            if (entity[i].state != S_DEAD) {
              entity[i].state = S_DEAD;
              entity[i].timer = 30;   // linger before disappearing
              flash_screen = 4;
              boss_dead = true;
              playSound(s_snd, S_SND_LEN);
            } else if (entity[i].timer == 0) {
              // All done — go to win screen
              removeEntity(entity[i].uid);
              jumpTo(WIN_SCREEN);
              continue;
            }
          } else if (entity[i].state == S_HIT) {
            if (entity[i].timer == 0) {
              entity[i].state = S_ALERT;
              entity[i].timer = 0;
            }
          } else if (entity[i].state == S_FIRING) {
            // Burst: fire one shot per burst_timer tick
            if (boss_burst_timer == 0 && boss_burst_left > 0) {
              spawnFireball(entity[i].pos.x, entity[i].pos.y);
              boss_burst_left--;
              boss_burst_timer = 8;
            } else if (boss_burst_timer > 0) {
              boss_burst_timer--;
            }
            if (boss_burst_left == 0 && boss_burst_timer == 0) {
              entity[i].state = S_ALERT;
              entity[i].timer = (entity[i].health < BOSS_PHASE2_HP) ? 20 : 35;
            }
          } else if (entity[i].state == S_MELEE) {
            if (entity[i].timer == 0) {
              player.health = max(0, player.health - BOSS_MELEE_DAMAGE);
              entity[i].timer = 12;
              flash_screen = 2;
              updateHud();
            }
          } else {
            // ALERT / STAND logic
            bool phase2 = (entity[i].health < BOSS_PHASE2_HP);
            float spd = phase2 ? BOSS_SPEED * 1.6f : BOSS_SPEED;

            if (entity[i].distance < BOSS_MELEE_DIST) {
              // Melee range
              if (entity[i].state != S_MELEE) {
                entity[i].state = S_MELEE;
                entity[i].timer = 8;
              }
            } else if (entity[i].distance < BOSS_VIEW_DIST) {
              if (entity[i].state != S_ALERT) {
                entity[i].state = S_ALERT;
                entity[i].timer = phase2 ? 15 : 30;
              } else {
                // Move toward player
                updatePosition(
                  level,
                  &(entity[i].pos),
                  sign(player.pos.x, entity[i].pos.x) * spd * delta,
                  sign(player.pos.y, entity[i].pos.y) * spd * delta,
                  true
                );
                if (entity[i].timer == 0) {
                  // Throw fireball burst
                  boss_burst_left = phase2 ? BOSS_BURST_COUNT + 1 : BOSS_BURST_COUNT;
                  boss_burst_timer = 0;
                  entity[i].state = S_FIRING;
                }
              }
            } else {
              entity[i].state = S_STAND;
            }
          }
          break;
        }
    } // end switch(type)

    i++;
  }
}

// The map raycaster. Based on https://lodev.org/cgtutor/raycasting.html
void renderMap(const uint8_t level[], float view_height) {
  UID last_uid;

  for (uint8_t x = 0; x < SCREEN_WIDTH; x += RES_DIVIDER) {
    float camera_x = 2 * (float) x / SCREEN_WIDTH - 1;
    float ray_x = player.dir.x + player.plane.x * camera_x;
    float ray_y = player.dir.y + player.plane.y * camera_x;
    uint8_t map_x = uint8_t(player.pos.x);
    uint8_t map_y = uint8_t(player.pos.y);
    Coords map_coords = { player.pos.x, player.pos.y };
    float delta_x = fabsf(1 / ray_x);
    float delta_y = fabsf(1 / ray_y);

    int8_t step_x; 
    int8_t step_y;
    float side_x;
    float side_y;
    uint8_t block = E_FLOOR;
    bool locked_and_closed = false;

    if (ray_x < 0) {
      step_x = -1;
      side_x = (player.pos.x - map_x) * delta_x;
    } else {
      step_x = 1;
      side_x = (map_x + 1.0f - player.pos.x) * delta_x;
    }

    if (ray_y < 0) {
      step_y = -1;
      side_y = (player.pos.y - map_y) * delta_y;
    } else {
      step_y = 1;
      side_y = (map_y + 1.0f - player.pos.y) * delta_y;
    }

    // Wall detection
    uint8_t depth = 0;
    bool hit = 0;
    bool side; 
    while (!hit && depth < MAX_RENDER_DEPTH) {
      if (side_x < side_y) {
        side_x += delta_x;
        map_x += step_x;
        side = 0;
      } else {
        side_y += delta_y;
        map_y += step_y;
        side = 1;
      }

      block = getBlockAt(level, map_x, map_y);

      locked_and_closed = false;
      if (block == E_LOCKEDDOOR) {
        UID door_uid = create_uid(block, map_x, map_y);
        locked_and_closed = true;
        for (uint8_t d = 0; d < 6; d++) {
          if (unlocked_doors[d] == door_uid) { locked_and_closed = false; break; }
        }
      }
      if (block == E_WALL || locked_and_closed) {
        hit = 1;
      } else {
        // Spawning entities here, as soon they are visible for the
        // player. Not the best place, but would be a very performance
        // cost scan for them in another loop
        if (block == E_ENEMY || block == E_BOSS || (block & 0b00001000) /* all collectable items */) {
          // Check that it's close to the player
          if (coords_distance(&(player.pos), &map_coords) < MAX_ENTITY_DISTANCE) {
            UID uid = create_uid(block, map_x, map_y);
            if (last_uid != uid && !isSpawned(uid)) {
              spawnEntity(block, map_x, map_y);
              last_uid = uid;
            }
          }
        }
      }

      depth++;
    }

    if (hit) {
      float distance;
      
      if (side == 0) {
        distance = max(1, (map_x - player.pos.x + (1 - step_x) / 2) / ray_x);
      } else {
        distance = max(1, (map_y - player.pos.y + (1 - step_y) / 2) / ray_y);
      }

      // store zbuffer value for the column
      zbuffer[x / Z_RES_DIVIDER] = min(distance * DISTANCE_MULTIPLIER, 255);

      // rendered line height
      uint8_t line_height = RENDER_HEIGHT / distance;

      uint8_t intensity = GRADIENT_COUNT - int(distance / MAX_RENDER_DEPTH * GRADIENT_COUNT) - side * 2;
      // Locked doors: flash by toggling between bright and near-dark every ~400 ms
      if (locked_and_closed) {
        bool flash_on = (millis() >> 9) & 1; // bit 9 → toggles every 512 ms
        intensity = flash_on ? (GRADIENT_COUNT - 1) : 1;
      }
      drawVLine(
        x,
        view_height / distance - line_height / 2 + RENDER_HEIGHT / 2,
        view_height / distance + line_height / 2 + RENDER_HEIGHT / 2,
        intensity
      );
    }
  }
}

// Sort entities from far to close
void sortEntities() {
  uint8_t gap = num_entities;
  bool swapped = false;
  while (gap > 1 || swapped) {
    //shrink factor 1.3
    gap = (gap * 10) / 13;
    if (gap == 9 || gap == 10) gap = 11;
    if (gap < 1) gap = 1;
    swapped = false;
    for (uint8_t i = 0; i < num_entities - gap; i++)
    {
      uint8_t j = i + gap;
      if (entity[i].distance < entity[j].distance)
      {
        swap(entity[i], entity[j]);
        swapped = true;
      }
    }
  }
}

Coords translateIntoView(Coords *pos) {
  //translate sprite position to relative to camera
  float sprite_x = pos->x - player.pos.x;
  float sprite_y = pos->y - player.pos.y;

  //required for correct matrix multiplication
  float inv_det = 1.0f / (player.plane.x * player.dir.y - player.dir.x * player.plane.y);
  float transform_x = inv_det * (player.dir.y * sprite_x - player.dir.x * sprite_y);
  float transform_y = inv_det * (- player.plane.y * sprite_x + player.plane.x * sprite_y); // Z in screen

  return { transform_x, transform_y };
}

void renderEntities(float view_height, uint32_t now) {
  sortEntities();

  for (uint8_t i = 0; i < num_entities; i++) {
    if (entity[i].state == S_HIDDEN) continue;

    Coords transform = translateIntoView(&(entity[i].pos));

    // don´t render if behind the player or too far away
    if (transform.y <= 0.1f || transform.y > MAX_SPRITE_DEPTH) {
      continue;
    }

    int16_t sprite_screen_x = HALF_WIDTH * (1.0f + transform.x / transform.y);
    int8_t sprite_screen_y = RENDER_HEIGHT / 2 + view_height / transform.y;
    uint8_t type = uid_get_type(entity[i].uid);

    // don´t try to render if outside of screen
    // doing this pre-shortcut due int16 -> int8 conversion makes out-of-screen
    // values fit into the screen space
    if (sprite_screen_x < - HALF_WIDTH || sprite_screen_x > SCREEN_WIDTH + HALF_WIDTH) {
      continue;
    }

    switch (type) {
      case E_ENEMY: {
          uint8_t sprite;
          if (entity[i].state == S_ALERT) {
            // walking
            sprite = int(now / 500) % 2;
          } else if (entity[i].state == S_FIRING) {
            // fireball
            sprite = 2;
          } else if (entity[i].state == S_HIT) {
            // hit
            sprite = 3;
          } else if (entity[i].state == S_MELEE) {
            // melee atack
            sprite = entity[i].timer > 10 ? 2 : 1;
          } else if (entity[i].state == S_DEAD) {
            // dying
            sprite = entity[i].timer > 0 ? 3 : 4;
          } else {
            // stand
            sprite = 0;
          }

          drawSprite(
            sprite_screen_x - BMP_IMP_WIDTH * .5 / transform.y,
            sprite_screen_y - 8 / transform.y,
            bmp_imp_bits,
            bmp_imp_mask,
            BMP_IMP_WIDTH,
            BMP_IMP_HEIGHT,
            sprite,
            transform.y
          );
          break;
        }

      case E_BOSS: {
          uint8_t sprite;
          if (entity[i].state == S_HIT) {
            sprite = 2;  // hit frame
          } else if (entity[i].state == S_FIRING || entity[i].state == S_MELEE) {
            sprite = (int(now / 200) % 2 == 0) ? 1 : 0;
          } else if (entity[i].state == S_DEAD) {
            sprite = 2;
          } else {
            sprite = (int(now / 600) % 2 == 0) ? 0 : 1;
          }

          // Boss health bar — drawn in HUD area when boss is nearby
          if (entity[i].distance < 80 && entity[i].state != S_DEAD) {
            uint8_t bar_w = map(entity[i].health, 0, BOSS_HEALTH, 0, 50);
            display.clearRect(38, 57, 52, 1);
            for (uint8_t bx = 0; bx < bar_w; bx++)
              drawPixel(38 + bx, 57, true, false);
          }

          drawSprite(
            sprite_screen_x - BMP_BOSS_WIDTH * .5 / transform.y,
            sprite_screen_y - 10 / transform.y,
            bmp_boss_bits,
            bmp_boss_mask,
            BMP_BOSS_WIDTH,
            BMP_BOSS_HEIGHT,
            sprite,
            transform.y
          );
          break;
        }

      case E_FIREBALL: {
          drawSprite(
            sprite_screen_x - BMP_FIREBALL_WIDTH / 2 / transform.y,
            sprite_screen_y - BMP_FIREBALL_HEIGHT / 2 / transform.y,
            bmp_fireball_bits,
            bmp_fireball_mask,
            BMP_FIREBALL_WIDTH,
            BMP_FIREBALL_HEIGHT,
            0,
            transform.y
          );
          break;
        }

      case E_MEDIKIT: {
          drawSprite(
            sprite_screen_x - BMP_ITEMS_WIDTH / 2 / transform.y,
            sprite_screen_y + 5 / transform.y,
            bmp_items_bits,
            bmp_items_mask,
            BMP_ITEMS_WIDTH,
            BMP_ITEMS_HEIGHT,
            0,
            transform.y
          );
          break;
        }

      case E_KEY: {
          drawSprite(
            sprite_screen_x - BMP_ITEMS_WIDTH / 2 / transform.y,
            sprite_screen_y + 5 / transform.y,
            bmp_items_bits,
            bmp_items_mask,
            BMP_ITEMS_WIDTH,
            BMP_ITEMS_HEIGHT,
            1,
            transform.y
          );
          break;
        }
    }
  }
}

void renderGun(uint8_t gun_pos, float amount_jogging, uint32_t now, uint8_t r1) {
  // jogging
  char x = 48 + sinf((float) now * JOGGING_SPEED) * 10 * amount_jogging - 9;
  char y = RENDER_HEIGHT - gun_pos + fabsf(cosf((float) now * JOGGING_SPEED)) * 8 * amount_jogging - 3;
  uint8_t clip_height = max(0, min(y + BMP_GUN_HEIGHT, RENDER_HEIGHT) - y);

  if (gun_pos > GUN_SHOT_POS - 2) {
    // Gun fire flash
    display.drawBitmap(x + 6, y - 11, bmp_fire_bits, BMP_FIRE_WIDTH, BMP_FIRE_HEIGHT, 1);
  }

  if (r1 == 1) {
    // Reload frame 1: pump pulling back
    clip_height = max(0, min(y + BMP_RE1_HEIGHT, RENDER_HEIGHT) - y + 22);
    display.drawBitmap(x - 10, y - 22, bmp_re1_mask, BMP_RE1_WIDTH, clip_height, 0);
    display.drawBitmap(x - 10, y - 22, bmp_re1_bits, BMP_RE1_WIDTH, clip_height, 1);
  } else if (r1 == 2) {
    // Reload frame 2: pump pushing forward
    clip_height = max(0, min(y + BMP_RE2_HEIGHT, RENDER_HEIGHT) - y + 22);
    display.drawBitmap(x - 10, y - 22, bmp_re2_mask, BMP_RE2_WIDTH, clip_height, 0);
    display.drawBitmap(x - 10, y - 22, bmp_re2_bits, BMP_RE2_WIDTH, clip_height, 1);
  } else if (r1 == 0) {
    // Normal idle/firing gun
    display.drawBitmap(x, y, bmp_gun_mask, BMP_GUN_WIDTH, clip_height, 0);
    display.drawBitmap(x, y, bmp_gun_bits, BMP_GUN_WIDTH, clip_height, 1);
  }
  // r1 == 3: gun hidden (player dead), draw nothing
}

// Only needed first time
void renderHud() {
  drawText(2, 58, F("{}"), 0);        // Health symbol
  drawText(40, 58, F("[]"), 0);       // Keys symbol
  drawText(70, 58, F("*"), 0);        // Ammo symbol
  updateHud();
}

// Render values for the HUD
void updateHud() {
  display.clearRect(12, 58, 15, 6);
  display.clearRect(50, 58, 5, 6);
  display.clearRect(80, 58, 20, 6);

  drawText(12, 58, player.health);
  drawText(50, 58, player.keys);
  drawText(80, 58, player.shots);
}

// Debug stats
void renderStats() {
  display.clearRect(58, 58, 70, 6);
  drawText(114, 58, int(getActualFps()));
  drawText(82, 58, num_entities);
  // drawText(94, 58, freeMemory());
}

// ── Story intro ──────────────────────────────────────────────────────────────
// Scrolls a DOOM-style story text upward before showing the logo.
static const char PROGMEM story_0[]  = "YEAR 2145.";
static const char PROGMEM story_1[]  = "UAC OPENED A PORTAL";
static const char PROGMEM story_2[]  = "ON PHOBOS BASE.";
static const char PROGMEM story_3[]  = "SOMETHING CAME";
static const char PROGMEM story_4[]  = "THROUGH.";
static const char PROGMEM story_5[]  = "";
static const char PROGMEM story_6[]  = "MARINES WENT IN.";
static const char PROGMEM story_7[]  = "NONE CAME BACK.";
static const char PROGMEM story_8[]  = "";
static const char PROGMEM story_9[]  = "YOU ARE THE LAST";
static const char PROGMEM story_10[] = "ONE LEFT.";
static const char PROGMEM story_11[] = "";
static const char PROGMEM story_12[] = "KILL EVERYTHING.";
static const char PROGMEM story_13[] = "CLOSE THE PORTAL.";
static const char PROGMEM story_14[] = "";
static const char PROGMEM story_15[] = "GOOD LUCK.";
static const char PROGMEM story_16[] = "YOU WILL NEED IT.";

#define STORY_LINES 17
static const char* const PROGMEM story_lines[STORY_LINES] = {
  story_0,  story_1,  story_2,  story_3,  story_4,
  story_5,  story_6,  story_7,  story_8,  story_9,
  story_10, story_11, story_12, story_13, story_14,
  story_15, story_16
};

static void readProgmemStr(const char* pgm_src, char* buf, uint8_t maxlen) {
  uint8_t i = 0;
  char c;
  while (i < maxlen - 1 && (c = pgm_read_byte(pgm_src + i)) != '\0') {
    buf[i++] = c;
  }
  buf[i] = '\0';
}

// Intro screen
void loopIntro() {
  // Part 1: Scrolling story
  const int8_t LINE_H   = 8;
  const int16_t TOTAL_H = (int16_t)STORY_LINES * LINE_H;
  int16_t scroll = SCREEN_HEIGHT;
  const int16_t scroll_end = -TOTAL_H;
  char buf[28];

  while (scroll > scroll_end && !exit_scene) {
    #ifdef SNES_CONTROLLER
    getControllerData();
    #endif
    if (input_fire()) goto intro_logo;

    display.clearDisplay();

    for (uint8_t ln = 0; ln < STORY_LINES; ln++) {
      int16_t ly = scroll + (int16_t)ln * LINE_H;
      if (ly >= SCREEN_HEIGHT || ly + CHAR_HEIGHT < 0) continue;
      const char* pgm_ptr = (const char*) pgm_read_ptr(story_lines + ln);
      readProgmemStr(pgm_ptr, buf, sizeof(buf));
      if (buf[0] == '\0') continue;
      uint8_t len = strlen(buf);
      int8_t x = (SCREEN_WIDTH - (int8_t)(len * (CHAR_WIDTH + 1) - 1)) / 2;
      drawText(x, (int8_t)ly, buf);
    }

    display.display();
    delay(55);
    scroll--;
  }

intro_logo:
  // Part 2: Logo + PRESS FIRE
  display.clearDisplay();
  display.drawBitmap(
    (SCREEN_WIDTH  - BMP_LOGO_WIDTH)  / 2,
    (SCREEN_HEIGHT - BMP_LOGO_HEIGHT) / 3,
    bmp_logo_bits,
    BMP_LOGO_WIDTH,
    BMP_LOGO_HEIGHT,
    1
  );
  delay(600);
  drawText(SCREEN_WIDTH / 2 - 25, (int8_t)(SCREEN_HEIGHT * .8), F("PRESS FIRE"));
  display.display();

  while (!exit_scene) {
    #ifdef SNES_CONTROLLER
    getControllerData();
    #endif
    if (input_fire()) jumpTo(MAP_SCREEN);
  }
}

// ── Victory screen ───────────────────────────────────────────────────────────
// Cinematic win screen: flash → stats line → scrolling epilogue → PRESS FIRE
static const char PROGMEM win_0[]  = "THE PORTAL IS";
static const char PROGMEM win_1[]  = "CLOSED.";
static const char PROGMEM win_2[]  = "";
static const char PROGMEM win_3[]  = "THE DEMON LORD";
static const char PROGMEM win_4[]  = "IS DEAD.";
static const char PROGMEM win_5[]  = "";
static const char PROGMEM win_6[]  = "PHOBOS BASE";
static const char PROGMEM win_7[]  = "IS SILENT.";
static const char PROGMEM win_8[]  = "";
static const char PROGMEM win_9[]  = "YOU DID IT.";
static const char PROGMEM win_10[] = "";
static const char PROGMEM win_11[] = "BUT EARTH...";
static const char PROGMEM win_12[] = "EARTH IS NEXT.";

#define WIN_LINES 13
static const char* const PROGMEM win_lines[WIN_LINES] = {
  win_0, win_1, win_2, win_3,  win_4,  win_5,  win_6,
  win_7, win_8, win_9, win_10, win_11, win_12
};

// Draw a skull shape centred at (cx, cy) — 9x9 px
static void drawSkull(int8_t cx, int8_t cy) {
  // dome
  for (int8_t dx = -3; dx <= 3; dx++) drawPixel(cx+dx, cy-4, true, false);
  for (int8_t dx = -4; dx <= 4; dx++) drawPixel(cx+dx, cy-3, true, false);
  for (int8_t dx = -4; dx <= 4; dx++) drawPixel(cx+dx, cy-2, true, false);
  // eye sockets
  for (int8_t dx = -4; dx <= 4; dx++) drawPixel(cx+dx, cy-1, true, false);
  drawPixel(cx-2, cy-1, false, false); drawPixel(cx-1, cy-1, false, false); // left eye
  drawPixel(cx+1, cy-1, false, false); drawPixel(cx+2, cy-1, false, false); // right eye
  // cheeks
  for (int8_t dx = -4; dx <= 4; dx++) drawPixel(cx+dx, cy, true, false);
  // jaw
  for (int8_t dx = -3; dx <= 3; dx++) drawPixel(cx+dx, cy+1, true, false);
  // teeth
  drawPixel(cx-2, cy+2, true, false); drawPixel(cx, cy+2, true, false); drawPixel(cx+2, cy+2, true, false);
}

void loopWin() {
  // ── Phase 1: white flash (4 frames) ──────────────────────────────
  for (uint8_t f = 0; f < 4; f++) {
    display.clearDisplay();
    if (f % 2 == 0) {
      // fill screen white via fadeScreen max
      fadeScreen(GRADIENT_COUNT - 1, 1);
    }
    display.display();
    delay(120);
  }

  // ── Phase 2: "DEMON SLAIN" banner ────────────────────────────────
  display.clearDisplay();
  drawSkull(32,  32);
  drawSkull(96,  32);
  // Big centred text
  drawText(37,  20, F("DEMON"));
  drawText(41,  28, F("SLAIN"));
  // decorative lines
  for (uint8_t x = 0; x < SCREEN_WIDTH; x++) {
    drawPixel(x, 15, true, false);
    drawPixel(x, 48, true, false);
  }
  display.display();
  delay(2200);

  // ── Phase 3: Scrolling epilogue ───────────────────────────────────
  const int8_t LINE_H   = 8;
  const int16_t TOTAL_H = (int16_t)WIN_LINES * LINE_H;
  int16_t scroll = SCREEN_HEIGHT;
  const int16_t scroll_end = -TOTAL_H;
  char buf[28];

  while (scroll > scroll_end && !exit_scene) {
    #ifdef SNES_CONTROLLER
    getControllerData();
    #endif
    if (input_fire()) goto win_done;

    display.clearDisplay();
    for (uint8_t ln = 0; ln < WIN_LINES; ln++) {
      int16_t ly = scroll + (int16_t)ln * LINE_H;
      if (ly >= SCREEN_HEIGHT || ly + CHAR_HEIGHT < 0) continue;
      const char* pgm_ptr = (const char*) pgm_read_ptr(win_lines + ln);
      readProgmemStr(pgm_ptr, buf, sizeof(buf));
      if (buf[0] == '\0') continue;
      uint8_t len = strlen(buf);
      int8_t x = (SCREEN_WIDTH - (int8_t)(len * (CHAR_WIDTH + 1) - 1)) / 2;
      drawText(x, (int8_t)ly, buf);
    }
    display.display();
    delay(60);
    scroll--;
  }

  // ── Phase 4: PRESS FIRE ───────────────────────────────────────────
  display.clearDisplay();
  drawText(22, 22, F("MISSION"));
  drawText(26, 30, F("COMPLETE"));
  // blinking PRESS FIRE
  {
    bool blink = true;
    uint32_t last = millis();
    while (!exit_scene) {
      #ifdef SNES_CONTROLLER
      getControllerData();
      #endif
      if (input_fire()) goto win_done;
      if (millis() - last > 500) {
        last = millis();
        blink = !blink;
        display.clearRect(18, 44, 92, 7);
        if (blink) drawText(18, 44, F("PRESS FIRE"));
        display.display();
      }
    }
  }

win_done:
  jumpTo(INTRO);
}


// ── In-game map overlay ────────────────────────────────────────────────────
// Called every frame while up+down are held together.
// Draws the level map into the 3-D viewport area (rows 0..RENDER_HEIGHT-1),
// then overlays the live player position and facing arrow.
// The HUD strip (rows RENDER_HEIGHT..63) is left untouched.
void renderInGameMap() {
  // Clear only the 3-D viewport
  memset(display_buf, 0, SCREEN_WIDTH * (RENDER_HEIGHT / 8));

  // Flash bit: toggles every 256 ms — used for player dot and key markers
  bool flash = (millis() >> 8) & 1;

  // ── draw level cells ─────────────────────────────────────────────
  for (uint8_t ly = 0; ly < LEVEL_HEIGHT; ly++) {
    uint8_t sy = MAP_OY + (LEVEL_HEIGHT - 1 - ly); // top of screen = north
    if (sy >= RENDER_HEIGHT) continue;

    for (uint8_t lx = 0; lx < LEVEL_WIDTH; lx++) {
      uint8_t sx = MAP_OX + lx * MAP_CELL_W;
      if (sx + 1 >= SCREEN_WIDTH) continue;

      uint8_t block = getBlockAt(sto_level_1, lx, ly);

      if (block == E_KEY) {
        // Check if already picked up (spawned as S_HIDDEN entity)
        UID key_uid = create_uid(E_KEY, lx, ly);
        bool picked = false;
        for (uint8_t i = 0; i < num_entities; i++) {
          if (entity[i].uid == key_uid && entity[i].state == S_HIDDEN) {
            picked = true;
            break;
          }
        }
        // Draw flashing single pixel for uncollected keys
        if (!picked && flash) {
          drawPixel(sx,     sy, true, false);
          drawPixel(sx + 1, sy, true, false);
        }
      } else if (block == E_EXIT) {
        // Exit: always draw (important goal), flash for emphasis
        drawPixel(sx,     sy, true, false);
        drawPixel(sx + 1, sy, true, false);
        if (flash && sy > 0) {
          drawPixel(sx,     sy - 1, !flash, false);
          drawPixel(sx + 1, sy - 1, !flash, false);
        }
      } else if (block == E_BOSS) {
        // Boss marker: large pulsing block — 2×3, opposite flash phase
        if (!flash) {
          drawPixel(sx, sy, true, false);
          drawPixel(sx + 1, sy, true, false);
          if (sy > 0) {
            drawPixel(sx, sy - 1, true, false);
            drawPixel(sx + 1, sy - 1, true, false);
          }
          if (sy + 1 < RENDER_HEIGHT) {
            drawPixel(sx, sy + 1, true, false);
            drawPixel(sx + 1, sy + 1, true, false);
          }
          // Draw upward arrow above boss marker (always visible, flashes opposite phase)
          // Arrow tip: 1 px above the boss block top
          // Shape (5px wide, 3px tall):
          //   . . X . .
          //   . X X X .
          //   X X X X X
          int8_t ax = (int8_t)sx - 1;  // arrow left edge (5 wide: sx-1..sx+3)
          int8_t ay = (int8_t)sy - 4;  // top of arrow
          if (ay >= 0 && ay + 2 < RENDER_HEIGHT) {
            drawPixel(ax + 2, ay,     true, false); // tip
            drawPixel(ax + 1, ay + 1, true, false);
            drawPixel(ax + 2, ay + 1, true, false);
            drawPixel(ax + 3, ay + 1, true, false);
            drawPixel(ax,     ay + 2, true, false);
            drawPixel(ax + 1, ay + 2, true, false);
            drawPixel(ax + 2, ay + 2, true, false);
            drawPixel(ax + 3, ay + 2, true, false);
            drawPixel(ax + 4, ay + 2, true, false);
          }
        }
      } else {
        _drawMapCell(block, sx, sy);
      }
    }
  }

  // ── player dot (live position) ───────────────────────────────────
  uint8_t dot_sx = MAP_OX + (uint8_t)player.pos.x * MAP_CELL_W;
  uint8_t dot_sy = MAP_OY + (LEVEL_HEIGHT - 1 - (uint8_t)player.pos.y);

  if (flash) {
    // 3×3 filled square
    for (int8_t dy = -1; dy <= 1; dy++) {
      for (int8_t dx = -1; dx <= 1; dx++) {
        int8_t px = (int8_t)dot_sx + dx;
        int8_t py = (int8_t)dot_sy + dy;
        if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < RENDER_HEIGHT)
          drawPixel(px, py, true, false);
      }
    }

    // ── facing line from the dot ─────────────────────────────────
    // +dir.x → east on screen, -dir.y → north on screen (y flipped)
    float fdx =  player.dir.x * 3.5f;
    float fdy = -player.dir.y * 3.5f;
    for (uint8_t step = 2; step <= 5; step++) {
      int8_t ax = (int8_t)dot_sx + (int8_t)(fdx * step / 5.0f);
      int8_t ay = (int8_t)dot_sy + (int8_t)(fdy * step / 5.0f);
      if (ax >= 0 && ax < SCREEN_WIDTH && ay >= 0 && ay < RENDER_HEIGHT)
        drawPixel(ax, ay, true, false);
    }
  }
}

void loopGamePlay() {
  bool gun_fired = false;
  bool walkSoundToggle = false;
  uint8_t gun_pos = 0;
  float rot_speed;
  float old_dir_x;
  float old_plane_x;
  float view_height;
  float jogging;
  uint8_t fade = GRADIENT_COUNT - 1;

  // reset reload animation state
  rc1 = 0;
  r = 0;
  reload1 = false;
  cheat_tp_start   = 0;
  cheat_kill_start = 0;

  initializeLevel(sto_level_1);

  // Reset boss state
  boss_spawned = false;
  boss_dead    = false;
  boss_burst_left  = 0;
  boss_burst_timer = 0;

  do {
    fps();

    // Cache millis() once per frame to avoid repeated syscalls
    uint32_t now = millis();

    // Clear only the 3d view
    memset(display_buf, 0, SCREEN_WIDTH * (RENDER_HEIGHT / 8));

    #ifdef SNES_CONTROLLER
    getControllerData();
    #endif

    // ── Map overlay: up+down held together ─────────────────────────────
    bool show_map = input_up() && input_down();

    // If the player is alive
    if (player.health > 0) {
      // Player speed (suppressed while map is open)
      if (!show_map && input_up()) {
        player.velocity += (MOV_SPEED - player.velocity) * .4;
        jogging = fabsf(player.velocity) * MOV_SPEED_INV;
      } else if (!show_map && input_down()) {
        player.velocity += (- MOV_SPEED - player.velocity) * .4;
        jogging = fabsf(player.velocity) * MOV_SPEED_INV;
      } else {
        player.velocity *= .5;
        jogging = fabsf(player.velocity) * MOV_SPEED_INV;
      }

      // Player rotation
      if (input_right()) {
        rot_speed = ROT_SPEED * delta;
        float rc = cosf(rot_speed), rs = sinf(rot_speed);
        old_dir_x = player.dir.x;
        player.dir.x   =  old_dir_x * rc + player.dir.y * rs;
        player.dir.y   = -old_dir_x * rs + player.dir.y * rc;
        old_plane_x = player.plane.x;
        player.plane.x =  old_plane_x * rc + player.plane.y * rs;
        player.plane.y = -old_plane_x * rs + player.plane.y * rc;
      } else if (input_left()) {
        rot_speed = ROT_SPEED * delta;
        float rc = cosf(rot_speed), rs = sinf(rot_speed);
        old_dir_x = player.dir.x;
        player.dir.x   =  old_dir_x * rc - player.dir.y * rs;
        player.dir.y   =  old_dir_x * rs + player.dir.y * rc;
        old_plane_x = player.plane.x;
        player.plane.x =  old_plane_x * rc - player.plane.y * rs;
        player.plane.y =  old_plane_x * rs + player.plane.y * rc;
      }

      view_height = fabsf(sinf((float) now * JOGGING_SPEED)) * 6 * jogging;

      if(view_height > 5.9) {
        if(sound == false) {
          if(walkSoundToggle) {
            playSound(walk1_snd, WALK1_SND_LEN);
            walkSoundToggle = false;
          } else {
            playSound(walk2_snd, WALK2_SND_LEN);
            walkSoundToggle = true;
          }
        }
      }
      // Update gun
      if (gun_pos > GUN_TARGET_POS) {
        // Right after fire
        gun_pos -= 1;
      } else if (gun_pos < GUN_TARGET_POS) {
        // Showing up
        gun_pos += 2;
      } else if (!gun_fired && input_fire()) {
        // ready to fire and fire pressed (only when not reloading)
        if (reload1 == false) {
          gun_pos = GUN_SHOT_POS;
          gun_fired = true;
          fire();
        }
      } else if (gun_fired && !input_fire()) {
        // just released fire — start reload animation
        gun_fired = false;
        reload1 = true;
      }
    } else {
      // The player is dead
      if (view_height > -10) view_height--;
      else if (input_fire()) jumpTo(INTRO);

      if (gun_pos > 1) gun_pos -= 2;
      // hide gun, cancel any reload in progress
      reload1 = false;
      r = 0;
      rc1 = 3;
    }

    // Player movement
    if (fabsf(player.velocity) > 0.003f) {
      updatePosition(
        sto_level_1,
        &(player.pos),
        player.dir.x * player.velocity * delta,
        player.dir.y * player.velocity * delta
      );
    } else {
      player.velocity = 0;
    }

    // ── Cheat codes ────────────────────────────────────────────────
    if (player.health > 0) {
      // CHEAT 1: Teleport to boss  — hold LEFT + FIRE for 1 s
      if (!show_map && input_left() && input_fire()) {
        if (cheat_tp_start == 0) cheat_tp_start = now;
        else if (now - cheat_tp_start >= CHEAT_HOLD_MS) {
          cheat_tp_start = 0;
          // Find boss position in level map and warp player there
          bool found_boss = false;
          for (uint8_t by = 0; by < LEVEL_HEIGHT && !found_boss; by++) {
            for (uint8_t bx = 0; bx < LEVEL_WIDTH && !found_boss; bx++) {
              if (getBlockAt(sto_level_1, bx, by) == E_BOSS) {
                // Place player one tile north of boss so we don't clip into it
                player.pos.x = (float)bx + 0.5f;
                player.pos.y = (float)by + 1.5;
                found_boss = true;
                flash_screen = 3;
              }
            }
          }
        }
      } else {
        cheat_tp_start = 0;
      }

      // CHEAT 2: Kill boss — hold RIGHT + FIRE for 1 s
      if (!show_map && input_right() && input_fire()) {
        if (cheat_kill_start == 0) cheat_kill_start = now;
        else if (now - cheat_kill_start >= CHEAT_HOLD_MS) {
          cheat_kill_start = 0;
          // If boss not yet spawned, force-spawn it first
          if (!boss_spawned) {
            for (uint8_t by = 0; by < LEVEL_HEIGHT; by++) {
              for (uint8_t bx = 0; bx < LEVEL_WIDTH; bx++) {
                if (getBlockAt(sto_level_1, bx, by) == E_BOSS) {
                  spawnEntity(E_BOSS, bx, by);
                  break;
                }
              }
              if (boss_spawned) break;
            }
          }
          // Kill it — reset state to STAND so updateEntities FSM picks up health==0
          for (uint8_t i = 0; i < num_entities; i++) {
            if (uid_get_type(entity[i].uid) == E_BOSS) {
              entity[i].health = 0;
              entity[i].state  = S_STAND;
              entity[i].timer  = 0;
              flash_screen = 4;
              break;
            }
          }
        }
      } else {
        cheat_kill_start = 0;
      }
    }

    // Update things
    updateEntities(sto_level_1);

    // Render stuff
    if (show_map) {
      // Map overlay: replaces the 3-D view, keeps the HUD strip
      renderInGameMap();
    } else {
      renderMap(sto_level_1, view_height);
      renderEntities(view_height, now);
    }

    // ── Reload animation tick ──────────────────────────────────────
    if (reload1) r++;

    if (r == 1) {
      rc1 = 1;                              // show pump-back frame
    } else if (r == 3) {
      rc1 = 2;                              // show pump-forward frame
      playSound(r1_snd, R1_SND_LEN);
    } else if (r == 5) {
      rc1 = 1;
      playSound(r2_snd, R2_SND_LEN);
    } else if (r >= 7) {
      r = 0;
      reload1 = false;
      rc1 = 0;                              // back to idle
    }

    if (!show_map) renderGun(gun_pos, jogging, now, rc1);

    // Fade in effect
    if (fade > 0) {
      fadeScreen(fade);
      fade--;

      if (fade == 0) {
        // Only draw the hud after fade in effect
        renderHud();
      }
    }

    // flash screen
    if (flash_screen > 0) {
      invert_screen = !invert_screen;
      flash_screen--;
    } else if (invert_screen) {
      invert_screen = 0;
    }

    // Draw the frame
    display.invertDisplay(invert_screen);
    display.display();

    // Exit routine
    #ifdef SNES_CONTROLLER
    if (input_start()) {
    #else
    if (input_left() && input_right()) {
    #endif
      jumpTo(INTRO);
    }
  } while (!exit_scene);
}

void loop(void) {
  switch (scene) {
    case SPLASH: {
        loopSplash();
        jumpTo(INTRO);
        break;
      }
    case INTRO: {
        loopIntro();
        break;
      }
    case MAP_SCREEN: {
        loopMapScreen();
        jumpTo(GAME_PLAY);
        break;
      }
    case GAME_PLAY: {
        loopGamePlay();
        break;
      }
    case WIN_SCREEN: {
        loopWin();
        break;
      }
  }

  // fade out effect
  for (uint8_t i=0; i<GRADIENT_COUNT; i++) {
    fadeScreen(i, 0);
    display.display();
    delay(40);
  }
  exit_scene = false;
}
