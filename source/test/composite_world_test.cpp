#include "gtest/gtest.h"
#include "StarCompositeWorld.hpp"

using namespace Star;

TEST(CompositeWorldPlacementTest, RoundTripPreservesLocalCoordinates) {
  CompositeWorldPlacement placement{WorldId(), Vec2I(50, 3000)};
  Vec2I local(12, 34);

  EXPECT_EQ(placement.toVirtual(local), Vec2I(62, 3034));
  EXPECT_EQ(placement.toLocal(placement.toVirtual(local)), local);
}

TEST(CompositeWorldPlacementTest, NegativeOffsetsRemainReversible) {
  CompositeWorldPlacement placement{WorldId(), Vec2I(-150, -40)};
  Vec2I local(450, 99);

  EXPECT_EQ(placement.toVirtual(local), Vec2I(300, 59));
  EXPECT_EQ(placement.toLocal(placement.toVirtual(local)), local);
}
