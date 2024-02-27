#include "app.h"
#include "gtest/gtest.h"

namespace VV {
TEST(GetChunkCoordNegatives, CorrectNegativeCoord) {
  Chunk_Coord res = get_chunk_coord(-5.0, 5.0);

  EXPECT_EQ(res.x, -1);
  EXPECT_EQ(res.y, 0);

  res = get_chunk_coord(-5.0, -5.0);

  EXPECT_EQ(res.x, -1);
  EXPECT_EQ(res.y, -1);
}

TEST(SurfaceGen, ReasonableLessThanZero) {
  s32 chunk_x = -1;
  u16 last_height;
  u16 x = 0;
  for (; x < CHUNK_CELL_WIDTH; x++) {
    u16 height = surface_height(x + CHUNK_CELL_WIDTH * chunk_x, 32, 0);
    if (x > 0 && last_height != height) {
      break;
    }

    last_height = height;
    // LOG_INFO("{}: {}", x + CHUNK_CELL_WIDTH * chunk_x,
    //          surface_height(x + CHUNK_CELL_WIDTH * chunk_x));
  }

  EXPECT_NE(x, CHUNK_CELL_WIDTH) << "All heights at chunk_x " << chunk_x
                                 << " were the same height: " << last_height;
}
}  // namespace VV
