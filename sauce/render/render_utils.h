#pragma once

#include "core.h"

namespace VV {

inline void lerp(u8& ra, u8& ga, u8& ba, u8& aa, u8 rb, u8 gb, u8 bb, u8 ab,
                 f32 t) {
  ra = static_cast<u8>(ra + t * (rb - ra));
  ga = static_cast<u8>(ga + t * (gb - ga));
  ba = static_cast<u8>(ba + t * (bb - ba));
  aa = static_cast<u8>(aa + t * (ab - aa));
}

}  // namespace VV
