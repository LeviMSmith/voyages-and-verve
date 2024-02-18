#include "app.h"
#include "gtest/gtest.h"

namespace VV {
TEST(GetChunkCoordNegatives, CorrectNegativeCoord) {
  Chunk_Coord res = get_chunk_coord(-5.0, 5.0);

  // Assert that the function returns the expected string
  EXPECT_EQ(res.x, -1);
  EXPECT_EQ(res.y, 0);
}
}  // namespace VV
