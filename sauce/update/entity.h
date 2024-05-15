#pragma once

#include "core.h"
#include "render/texture.h"
#include "update/ai.h"

namespace VV {
constexpr u32 MAX_TOTAL_ENTITIES = 100000;

struct Entity_Coord {
  f64 x, y;
};

typedef u32 Entity_ID;
typedef s8 Entity_Z;

// bool flags
enum class Entity_Status : u8 {
  ON_GROUND = 1,
  IN_WATER = 2,
  ANIMATED = 4,
  DEAD = 8,
  DEATHLESS = 16,  // Will respawn when dead
};

// For which entity to create
enum class Entity_Factory_Type : u16 {
  GUYPLAYER,
  TREE,
  BUSH,
  GRASS,
  NIETZSCHE,
  AKNIETZSCHE,
  ECNIETZSCHE,
  SDNIETZSCHE,
  JELLYFISH,
  SEAWEED,
  FISH,
};

// Monolithic Entity struct. Every entity possess every possible
// attribute to simplify the data. I'm thinking we'll probably
// never have more than at max hundreds of thousands of these
// so even at hundreds of bytes this struct should be totally fine
// to fit into memory. (Like maybe 100MB max)
// AOS, so a little OOP, but I think this is very flexible way
// to hold all the data. Plus no mapping like a SOA would require.

// Entity functionality: whenever components of an entity need to be acted on,
// they should be registered in the dimensions component list. For example, if
// an entity should be rendered, it should be added to the dimension's render
// map with it's z depth. Generally this is handled in the create factories, but
// with anything custom, you can also add functionality after the entity is
// created.

struct Entity {
  Entity_Coord coord;  // For bounding box and rendering, this is top left
  f32 vx, vy;
  f32 ax, ay;

  f32 camx, camy;  // This is relative to coord

  u16 status;
  f32 bouyancy;

  // The physics bounding box starting from coord as top left
  f32 boundingw, boundingh;

  // Head bounding box starting from coord as top left
  f32 head_boundingw, head_boundingh;

  Texture_Id texture;  // a number that is mapped to a file in res/textures
  u8 texture_index;    // an index into a texture atlas
  Entity_Z zdepth;     // Only applies to entity rendering
  bool flipped;

  u8 anim_width;
  u8 anim_frames;
  u8 anim_current_frame;
  u16 anim_delay;
  u16 anim_delay_variety;

  u16 anim_timer;
  u16 anim_delay_current_spice;

  s64 health;
  s64 max_health;
  Entity_Coord respawn_point;

  AI_ID ai_id;

  Entity_Coord wander_target;
  u64 wander_target_frame;
};

struct Entity_Factory {
  Entity e;
  bool register_kinetic;
  bool register_render;
  bool register_health;
  bool register_ai;
};

Entity default_entity();

inline Entity_Coord get_cam_coord(const Entity &e);

}  // namespace VV
