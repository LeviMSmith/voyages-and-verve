#include "update/entity.h"

namespace VV {
Entity default_entity() {
  Entity return_entity;

  std::memset(&return_entity, 0, sizeof(Entity));
  /*
  return_entity.coord.x = 0;
  return_entity.coord.y = 0;
  return_entity.vx = 0;
  return_entity.vy = 0;
  return_entity.ax = 0;
  return_entity.ay = 0;
  return_entity.status = 0;
  return_entity.camx = 0;
  return_entity.camy = 0;
  return_entity.boundingw = 0;
  return_entity.boundingh = 0;
  return_entity.texture = Texture_Id::NONE;  // Also 0
  return_entity.texture_index = 0;
  */

  return return_entity;
}

Entity_Coord get_cam_coord(const Entity &e) {
  return {
      e.coord.x + e.camx,  // x
      e.coord.y + e.camy   // y
  };
}
}  // namespace VV
