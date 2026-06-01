#ifndef _constants_h
#define _constants_h

// Key pinout
#define USE_INPUT_PULLUP
#define K_LEFT              6
#define K_RIGHT             7
#define K_UP                8
#define K_DOWN              3
#define K_FIRE              10

// SNES Controller
// uncomment following line to enable snes controller support
// #define SNES_CONTROLLER
constexpr uint8_t DATA_CLOCK   = 11;
constexpr uint8_t DATA_LATCH   = 12;
constexpr uint8_t DATA_SERIAL  = 13;

// Sound
constexpr uint8_t SOUND_PIN   = 9; // do not change, belongs to used timer

// GFX settings
#define OPTIMIZE_SSD1306                // Optimizations for SSD1366 displays

// OPT: FRAME_TIME as uint16_t literal avoids a double constant in ROM
#define FRAME_TIME          67u         // ~15 fps (was 66.666666 double)
#define RES_DIVIDER         2
#define Z_RES_DIVIDER       2
#define DISTANCE_MULTIPLIER 20
#define MAX_RENDER_DEPTH    12
#define MAX_SPRITE_DEPTH    8

#define ZBUFFER_SIZE        SCREEN_WIDTH / Z_RES_DIVIDER

// Level
#define LEVEL_WIDTH_BASE    6
#define LEVEL_WIDTH         (1 << LEVEL_WIDTH_BASE)
#define LEVEL_HEIGHT        57
#define LEVEL_SIZE          LEVEL_WIDTH / 2 * LEVEL_HEIGHT

// scenes
#define INTRO                 0
#define GAME_PLAY             1
#define WIN_SCREEN            4

// Game  — OPT: all movement/speed constants as float (not double).
//         On AVR, every double forces 64-bit soft-float; float uses 32-bit,
//         saving ~300 bytes of ROM per heavy function.
#define GUN_TARGET_POS        18
#define GUN_SHOT_POS          GUN_TARGET_POS + 4

#define ROT_SPEED             .12f
#define MOV_SPEED             .2f
#define MOV_SPEED_INV         5
#define JOGGING_SPEED         .005f
#define ENEMY_SPEED           .02f
#define FIREBALL_SPEED        .2f
#define FIREBALL_ANGLES       45

#define MAX_ENTITIES          10
#define MAX_STATIC_ENTITIES   28

#define MAX_ENTITY_DISTANCE   200
#define MAX_ENEMY_VIEW        80
#define ITEM_COLLIDER_DIST    6
#define ENEMY_COLLIDER_DIST   4
#define FIREBALL_COLLIDER_DIST 2
#define ENEMY_MELEE_DIST      6
#define WALL_COLLIDER_DIST    .2f

#define ENEMY_MELEE_DAMAGE    8
#define ENEMY_FIREBALL_DAMAGE 20
#define GUN_MAX_DAMAGE        34

// Boss
#define BOSS_HEALTH           250
#define BOSS_SPEED            .035f
#define BOSS_MELEE_DIST       5
#define BOSS_MELEE_DAMAGE     15
#define BOSS_FIREBALL_DAMAGE  25
#define BOSS_VIEW_DIST        120
#define BOSS_BURST_COUNT      3
#define BOSS_PHASE2_HP        125

// display
constexpr uint8_t SCREEN_WIDTH     =  128;
constexpr uint8_t SCREEN_HEIGHT    =  64;
constexpr uint8_t HALF_WIDTH       =  SCREEN_WIDTH/2;
constexpr uint8_t RENDER_HEIGHT    =  56;
constexpr uint8_t HALF_HEIGHT      =  SCREEN_HEIGHT/2;

#endif
