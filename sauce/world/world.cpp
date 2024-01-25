#include "core.h"

#include <cstring>

#include "world/world.h"

namespace YC {
Result gen_chunk(Chunk &chunk) {
  std::memset(&chunk.cells, (int)Cell_Type::DIRT, CHUNK_CELLS);

  return Result::SUCCESS;
}
} // namespace YC
