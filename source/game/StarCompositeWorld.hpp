#pragma once

#include "StarWarping.hpp"
#include "StarVector.hpp"

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
};

}
