#pragma once

#include "core.h"

namespace VV {
inline void lerp(u8& ra, u8& ga, u8& ba, u8& aa, u8 rb, u8 gb, u8 bb, u8 ab,
                 f32 distance) {
  ra += ra > rb ? (ra - rb) / distance : (rb - ra) / distance * -1.0;
  ga += ga > gb ? (ga - gb) / distance : (gb - ga) / distance * -1.0;
  ba += ba > bb ? (ba - bb) / distance : (bb - ba) / distance * -1.0;
  aa += aa > ab ? (aa - ab) / distance : (ab - aa) / distance * -1.0;
}
}  // namespace VV
