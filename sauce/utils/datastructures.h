#include <mutex>
#include <set>
#include <stack>

#include "update/world.h"

namespace VV {

class ThreadSafeProcessingSet {
 private:
  std::stack<Chunk_Coord> stack;
  std::set<Chunk_Coord> currentlyProcessing;
  mutable std::mutex mutex;

 public:
  ThreadSafeProcessingSet() = default;

  ThreadSafeProcessingSet(const ThreadSafeProcessingSet&) = delete;
  ThreadSafeProcessingSet& operator=(const ThreadSafeProcessingSet&) = delete;

  void push(const Chunk_Coord& value) {
    std::lock_guard<std::mutex> lock(mutex);
    stack.push(std::move(value));
    currentlyProcessing.erase(value);
  }

  bool try_pop(Chunk_Coord& t) {
    std::lock_guard<std::mutex> lock(mutex);
    while (!stack.empty()) {
      Chunk_Coord topValue = std::move(stack.top());
      stack.pop();
      // Attempt to insert into currentlyProcessing. If the insert is
      // successful, it means this chunk is not currently being processed by
      // another thread.
      if (currentlyProcessing.insert(topValue).second) {
        t = topValue;
        return true;
      }
    }
    return false;
  }

  void mark_done(Chunk_Coord const& value) {
    std::lock_guard<std::mutex> lock(mutex);
    currentlyProcessing.erase(value);
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex);
    return stack.size();
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(mutex);
    return stack.empty();
  }

  bool is_adjacent(const Chunk_Coord& coord) const {
    std::lock_guard<std::mutex> lock(mutex);
    // Check all possible adjacent positions
    std::vector<Chunk_Coord> adjacentPositions = {{coord.x - 1, coord.y},
                                                  {coord.x + 1, coord.y},
                                                  {coord.x, coord.y - 1},
                                                  {coord.x, coord.y + 1}};

    for (const auto& adjCoord : adjacentPositions) {
      if (currentlyProcessing.find(adjCoord) != currentlyProcessing.end()) {
        return true;  // Found an adjacent chunk that's currently being
                      // processed
      }
    }

    return false;
  }
};

}  // namespace VV
