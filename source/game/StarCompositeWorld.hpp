#pragma once

#include "StarWarping.hpp"
#include "StarVector.hpp"
#include <cstdint>

namespace Star {

// A client-side placement of an independently simulated world in a shared
// virtual coordinate space. This does not alter world geometry or persistence.
// The primary world is conventionally placed at the virtual origin.
struct CompositeWorldPlacement {
  WorldId worldId;
  Vec2I offset;

  Vec2I toVirtual(Vec2I const& localPosition) const {
    return localPosition + offset;
  }

  Vec2I toLocal(Vec2I const& virtualPosition) const {
    return virtualPosition - offset;
  }

  // Vertical worlds retain their full native Y range, including their
  // existing atmosphere/space biomes. Bounds are inclusive at the bottom
  // and exclusive at the top, matching regular world tile coordinates.
  bool containsVirtualY(int virtualY, unsigned worldHeight) const {
    int64_t localY = (int64_t)virtualY - (int64_t)offset.y();
    return localY >= 0 && localY < (int64_t)worldHeight;
  }
};

}
