#include "gtest/gtest.h"
#include "StarCompositeWorld.hpp"
#include "StarWorldCamera.hpp"

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

TEST(CompositeWorldPlacementTest, PlanetSpaceRemainsInsideOriginalWorld) {
  // The entire planet, including its original upper space layer, stays
  // mapped below the next world; no planet coordinates are cropped.
  const int planetHeight = 3000;
  const int gap = 128;
  CompositeWorldPlacement planet{WorldId(), Vec2I(0, 0)};
  CompositeWorldPlacement ship{WorldId(), Vec2I(0, planetHeight + gap)};
  EXPECT_EQ(planet.toVirtual(Vec2I(0, planetHeight)).y(), planetHeight);
  EXPECT_EQ(ship.toVirtual(Vec2I(0, 0)).y(), planetHeight + gap);
}

TEST(CompositeWorldCameraTest, CompositeViewCanLookOutsideSourceWorld) {
  WorldCamera camera;
  camera.setScreenSize(Vec2U(800, 600));
  camera.setPixelRatio(2.0f);
  camera.setWorldGeometry(WorldGeometry(Vec2U(1000, 500)));
  camera.setCenterWorldPosition(Vec2F(100.0f, 600.0f), true);
  EXPECT_LE(camera.centerWorldPosition().y(), 500.0f);

  camera.setVerticalClamping(false);
  camera.setCenterWorldPosition(Vec2F(100.0f, 600.0f), true);
  EXPECT_GT(camera.centerWorldPosition().y(), 500.0f);

  camera.setVerticalClamping(true);
  EXPECT_LE(camera.centerWorldPosition().y(), 500.0f);
}

TEST(CompositeWorldPlacementTest, RegionContainsOriginalPlanetSpaceAndExcludesAdjacentShip) {
  CompositeWorldPlacement planet{WorldId(), Vec2I(0, 0)};
  CompositeWorldPlacement ship{WorldId(), Vec2I(0, 3128)};

  EXPECT_TRUE(planet.containsVirtualY(2999, 3000));
  EXPECT_FALSE(planet.containsVirtualY(3000, 3000));
  EXPECT_FALSE(ship.containsVirtualY(3127, 250));
  EXPECT_TRUE(ship.containsVirtualY(3128, 250));
  EXPECT_TRUE(ship.containsVirtualY(3377, 250));
  EXPECT_FALSE(ship.containsVirtualY(3378, 250));
}

TEST(CompositeWorldPlacementTest, NegativeVirtualOffsetsAndZeroHeight) {
  CompositeWorldPlacement world{WorldId(), Vec2I(-100, -40)};
  EXPECT_FALSE(world.containsVirtualY(-41, 30));
  EXPECT_TRUE(world.containsVirtualY(-40, 30));
  EXPECT_TRUE(world.containsVirtualY(-11, 30));
  EXPECT_FALSE(world.containsVirtualY(-10, 30));
  EXPECT_FALSE(world.containsVirtualY(-40, 0));
}
