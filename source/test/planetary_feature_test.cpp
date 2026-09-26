#include "StarCelestialParameters.hpp"
#include "StarJsonExtra.hpp"
#include "StarWorldParameters.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

Json featureSelection(Json groups) {
  return JsonObject{{"planetaryFeatures", std::move(groups)}};
}

}

TEST(PlanetaryFeatureTest, DefaultsSelectNothing) {
  EXPECT_TRUE(selectPlanetaryFeaturesFromConfig(JsonObject{}, JsonObject{}, 1234).empty());
}

TEST(PlanetaryFeatureTest, SelectionIsDeterministicAndUnique) {
  Json definitions = JsonObject{
    {"alpha", JsonObject{}},
    {"beta", JsonObject{}},
    {"gamma", JsonObject{}}};
  Json groups = JsonObject{
    {"groupA", JsonObject{{"alternative", JsonObject{{"members", JsonObject{
      {"feature:alpha", 1.0}, {"feature:beta", 2.0}}}}}}},
    {"groupB", JsonObject{{"guaranteed", JsonObject{{"feature:gamma", true}}}}}};
  Json config = featureSelection(JsonObject{{"group:groupA", 1.0}, {"group:groupB", 1.0}});

  auto first = selectPlanetaryFeaturesFromConfig(config, definitions, 987654, groups);
  auto second = selectPlanetaryFeaturesFromConfig(config, definitions, 987654, groups);
  EXPECT_EQ(first, second);
  EXPECT_EQ(first.size(), 2u);
  EXPECT_EQ(StringSet::from(first).size(), first.size());
  EXPECT_TRUE(first.contains("alpha") || first.contains("beta"));
  EXPECT_TRUE(first.contains("gamma"));
}

TEST(PlanetaryFeatureTest, OceanConflictsPruneWholeGuaranteedBundle) {
  Json definitions = JsonObject{
    {"oceanA", JsonObject{{"layers", JsonObject{{"surface", JsonObject{{"ocean", JsonObject{}}}}}}}},
    {"oceanB", JsonObject{{"layers", JsonObject{{"surface", JsonObject{{"ocean", JsonObject{}}}}}}}},
    {"childA", JsonObject{}}};
  Json groups = JsonObject{
    {"branchA", JsonObject{
      {"guaranteed", JsonObject{{"feature:oceanA", true}}},
      {"random", JsonObject{{"members", JsonObject{{"feature:childA", 1.0}}}}}}},
    {"branchB", JsonObject{{"guaranteed", JsonObject{{"feature:oceanB", true}}}}}};
  Json config = featureSelection(JsonObject{{"group:branchA", 1.0}, {"group:branchB", 1.0}});

  auto selected = selectPlanetaryFeaturesFromConfig(config, definitions, 42, groups);
  EXPECT_NE(selected.contains("oceanA"), selected.contains("oceanB"));
  EXPECT_EQ(selected.contains("childA"), selected.contains("oceanA"));
}

TEST(PlanetaryFeatureTest, GroupChoosesOneWeightedMember) {
  Json definitions = JsonObject{
    {"grey", JsonObject{}},
    {"brown", JsonObject{}},
    {"blue", JsonObject{}}};
  Json groups = JsonObject{{"ringed", JsonObject{{"alternative", JsonObject{{"members", JsonObject{
    {"feature:grey", 1.0}, {"feature:brown", 0.0}, {"feature:blue", 0.0}}}}}}}};
  Json config = featureSelection(JsonObject{{"group:ringed", 1.0}});

  auto selected = selectPlanetaryFeaturesFromConfig(config, definitions, 17, groups);
  EXPECT_EQ(selected, (StringList{"grey"}));
}

TEST(PlanetaryFeatureTest, GroupMemberWeightsAffectSelectionFrequency) {
  Json definitions = JsonObject{{"common", JsonObject{}}, {"rare", JsonObject{}}};
  Json groups = JsonObject{{"variation", JsonObject{{"alternative", JsonObject{{"members", JsonObject{
    {"feature:common", 9.0}, {"feature:rare", 1.0}}}}}}}};
  Json config = featureSelection(JsonObject{{"group:variation", 1.0}});

  unsigned commonCount = 0;
  for (uint64_t seed = 0; seed < 1000; ++seed) {
    auto selected = selectPlanetaryFeaturesFromConfig(config, definitions, seed, groups);
    ASSERT_EQ(selected.size(), 1u);
    if (selected.first() == "common")
      ++commonCount;
  }
  EXPECT_GT(commonCount, 700u);
}

TEST(PlanetaryFeatureTest, UnrelatedRootDoesNotReshuffleExistingGroup) {
  Json definitions = JsonObject{
    {"grey", JsonObject{}}, {"blue", JsonObject{}}, {"volcanic", JsonObject{}}};
  Json groups = JsonObject{
    {"ringed", JsonObject{{"alternative", JsonObject{{"members", JsonObject{
      {"feature:grey", 3.0}, {"feature:blue", 1.0}}}}}}}};

  auto ringOnly = selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:ringed", 1.0}}), definitions, 6789, groups);
  auto withVolcanism = selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:ringed", 1.0}, {"feature:volcanic", 1.0}}), definitions, 6789, groups);
  ASSERT_EQ(ringOnly.size(), 1u);
  ASSERT_EQ(withVolcanism.size(), 2u);
  EXPECT_EQ(ringOnly.contains("grey"), withVolcanism.contains("grey"));
  EXPECT_EQ(ringOnly.contains("blue"), withVolcanism.contains("blue"));
}

TEST(PlanetaryFeatureTest, RootFeaturesAndGroupsHaveIndependentChances) {
  Json definitions = JsonObject{{"ring", JsonObject{}}, {"volcanic", JsonObject{}}};
  Json groups = JsonObject{
    {"ringed", JsonObject{{"guaranteed", JsonObject{{"feature:ring", true}}}}}};

  auto selected = selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:ringed", 1.0}, {"feature:volcanic", 0.0}}), definitions, 17, groups);
  EXPECT_EQ(selected, (StringList{"ring"}));
}

TEST(PlanetaryFeatureTest, RandomMaxCountCountsNestedGroupAsOneChild) {
  Json definitions = JsonObject{
    {"a", JsonObject{}}, {"b", JsonObject{}}, {"c", JsonObject{}}, {"d", JsonObject{}}};
  Json groups = JsonObject{
    {"nested", JsonObject{{"guaranteed", JsonObject{
      {"feature:c", true}, {"feature:d", true}}}}},
    {"parent", JsonObject{{"random", JsonObject{
      {"maxCount", 2},
      {"members", JsonObject{
        {"feature:a", 1.0}, {"feature:b", 1.0}, {"group:nested", 1.0}}}}}}}};

  for (uint64_t seed = 0; seed < 20; ++seed) {
    auto selected = selectPlanetaryFeaturesFromConfig(
        featureSelection(JsonObject{{"group:parent", 1.0}}), definitions, seed, groups);
    EXPECT_EQ(selected.contains("c"), selected.contains("d"));
    unsigned directChildren = (selected.contains("a") ? 1 : 0)
        + (selected.contains("b") ? 1 : 0) + (selected.contains("c") ? 1 : 0);
    EXPECT_EQ(directChildren, 2u);
  }
}

TEST(PlanetaryFeatureTest, AlternativeActivationChanceMaySelectNothing) {
  Json definitions = JsonObject{{"only", JsonObject{}}};
  Json groups = JsonObject{{"optional", JsonObject{{"alternative", JsonObject{
    {"chance", 0.0}, {"members", JsonObject{{"feature:only", 1.0}}}}}}}};
  EXPECT_TRUE(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:optional", 1.0}}), definitions, 1, groups).empty());
}

TEST(PlanetaryFeatureTest, FalseGuaranteedMemberIsDisabled) {
  Json definitions = JsonObject{{"disabled", JsonObject{}}};
  Json groups = JsonObject{{"optional", JsonObject{{"guaranteed", JsonObject{
    {"feature:disabled", false}}}}}};
  EXPECT_TRUE(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:optional", 1.0}}), definitions, 1, groups).empty());
}

TEST(PlanetaryFeatureTest, RepeatedFeatureReferencesAreDeduplicated) {
  Json definitions = JsonObject{{"shared", JsonObject{}}};
  Json groups = JsonObject{
    {"first", JsonObject{{"guaranteed", JsonObject{{"feature:shared", true}}}}},
    {"second", JsonObject{{"guaranteed", JsonObject{{"feature:shared", true}}}}}};

  EXPECT_EQ(selectPlanetaryFeaturesFromConfig(featureSelection(JsonObject{
      {"group:first", 1.0}, {"group:second", 1.0}}), definitions, 1, groups),
      (StringList{"shared"}));
}

TEST(PlanetaryFeatureTest, RecursiveGroupsAreRejected) {
  Json groups = JsonObject{
    {"a", JsonObject{{"guaranteed", JsonObject{{"group:b", true}}}}},
    {"b", JsonObject{{"guaranteed", JsonObject{{"group:a", true}}}}}};
  EXPECT_THROW(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:a", 1.0}}), JsonObject{}, 1, groups), StarException);
}

TEST(PlanetaryFeatureTest, ConflictingGuaranteedSiblingsAreRejected) {
  Json definitions = JsonObject{
    {"oceanA", JsonObject{{"layers", JsonObject{{"surface", JsonObject{{"ocean", JsonObject{}}}}}}}},
    {"oceanB", JsonObject{{"layers", JsonObject{{"surface", JsonObject{{"ocean", JsonObject{}}}}}}}}};
  Json groups = JsonObject{{"invalid", JsonObject{{"guaranteed", JsonObject{
    {"feature:oceanA", true}, {"feature:oceanB", true}}}}}};
  EXPECT_THROW(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:invalid", 1.0}}), definitions, 1, groups), StarException);
  EXPECT_THROW(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:invalid", 0.0}}), definitions, 1, groups), StarException);
}

TEST(PlanetaryFeatureTest, NestedGuaranteedConflictDiscardsBundle) {
  Json definitions = JsonObject{
    {"oceanA", JsonObject{{"layers", JsonObject{{"surface", JsonObject{{"ocean", JsonObject{}}}}}}}},
    {"oceanB", JsonObject{{"layers", JsonObject{{"surface", JsonObject{{"ocean", JsonObject{}}}}}}}},
    {"child", JsonObject{}}};
  Json groups = JsonObject{
    {"nested", JsonObject{{"guaranteed", JsonObject{{"feature:oceanB", true}}}}},
    {"invalid", JsonObject{
      {"guaranteed", JsonObject{{"feature:oceanA", true}, {"group:nested", true}}},
      {"random", JsonObject{{"members", JsonObject{{"feature:child", 1.0}}}}}}}};
  EXPECT_TRUE(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:invalid", 1.0}}), definitions, 1, groups).empty());
}

TEST(PlanetaryFeatureTest, DungeonReplacementOwnsLayerButAllowsDescendantAdditions) {
  Json dungeonLayer = JsonObject{
    {"dungeons", JsonArray{JsonArray{1.0, "test"}}},
    {"dungeonCountRange", JsonArray{1, 1}}};
  Json replacementLayer = dungeonLayer.set("replaceDungeons", true);
  Json definitions = JsonObject{
    {"replacement", JsonObject{{"layers", JsonObject{{"surface", replacementLayer}}}}},
    {"childAddition", JsonObject{{"layers", JsonObject{{"surface", dungeonLayer}}}}},
    {"outsideAddition", JsonObject{{"layers", JsonObject{{"surface", dungeonLayer}}}}}};
  Json groups = JsonObject{{"owner", JsonObject{
    {"guaranteed", JsonObject{{"feature:replacement", true}}},
    {"random", JsonObject{{"members", JsonObject{{"feature:childAddition", 1.0}}}}}}}};

  auto selected = selectPlanetaryFeaturesFromConfig(featureSelection(JsonObject{
      {"group:owner", 1.0}, {"feature:outsideAddition", 1.0}}), definitions, 7, groups);
  if (selected.contains("replacement")) {
    EXPECT_TRUE(selected.contains("childAddition"));
    EXPECT_FALSE(selected.contains("outsideAddition"));
  } else {
    EXPECT_FALSE(selected.contains("childAddition"));
    EXPECT_TRUE(selected.contains("outsideAddition"));
  }
}

TEST(PlanetaryFeatureTest, AdditiveDungeonFeaturesCoexistWithoutReplacement) {
  Json dungeonLayer = JsonObject{
    {"dungeons", JsonArray{JsonArray{1.0, "test"}}},
    {"dungeonCountRange", JsonArray{1, 1}}};
  Json definitions = JsonObject{
    {"additionA", JsonObject{{"layers", JsonObject{{"surface", dungeonLayer}}}}},
    {"additionB", JsonObject{{"layers", JsonObject{{"surface", dungeonLayer}}}}}};

  EXPECT_EQ(selectPlanetaryFeaturesFromConfig(featureSelection(JsonObject{
      {"feature:additionA", 1.0}, {"feature:additionB", 1.0}}), definitions, 1),
      (StringList{"additionA", "additionB"}));
}

TEST(PlanetaryFeatureTest, InvalidConfigurationIsRejected) {
  EXPECT_THROW(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"missing", 1.0}}), JsonObject{}, 1), StarException);
  EXPECT_THROW(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"feature:present", 1.1}}), JsonObject{{"present", JsonObject{}}}, 1), StarException);
  EXPECT_THROW(selectPlanetaryFeaturesFromConfig(
      JsonObject{{"maximumPlanetaryFeatures", 1}}, JsonObject{}, 1), StarException);
  EXPECT_THROW(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:broken", 1.0}}), JsonObject{}, 1,
      JsonObject{{"broken", JsonObject{{"alternative", JsonObject{{"members", JsonObject{{"feature:missing", 1.0}}}}}}}}), StarException);
  EXPECT_THROW(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:broken", 1.0}}), JsonObject{{"present", JsonObject{}}}, 1,
      JsonObject{{"broken", JsonObject{{"alternative", JsonObject{{"members", JsonObject{{"feature:present", -1.0}}}}}}}}), StarException);
  EXPECT_THROW(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:legacy", 1.0}}), JsonObject{{"present", JsonObject{}}}, 1,
      JsonObject{{"legacy", JsonObject{{"present", 1.0}}}}), StarException);
  EXPECT_THROW(selectPlanetaryFeaturesFromConfig(
      featureSelection(JsonObject{{"group:broken", 1.0}}), JsonObject{}, 1,
      JsonObject{{"broken", JsonObject{{"guaranteed", JsonObject{{"feature:missing", false}}}}}}), StarException);
}

TEST(PlanetaryFeatureTest, CelestialParametersPersistFeatureList) {
  Json parameters = JsonObject{
    {"worldType", "Terrestrial"},
    {"worldSize", "small"},
    {"terrestrialType", JsonArray{"garden"}}};
  CelestialParameters generated(CelestialCoordinate({1, 2, 3}, 1), 12345, "Test", parameters);

  EXPECT_TRUE(generated.getParameter("planetaryFeatures").isType(Json::Type::Array));
  EXPECT_EQ(generated.getParameter("planetaryFeatures"),
      jsonFromStringList(selectPlanetaryFeatures(String("garden"), String("small"), 12345)));
  CelestialParameters diskRoundTrip(generated.diskStore());
  CelestialParameters netRoundTrip(generated.netStore());
  EXPECT_EQ(diskRoundTrip.getParameter("planetaryFeatures"), generated.getParameter("planetaryFeatures"));
  EXPECT_EQ(netRoundTrip.getParameter("planetaryFeatures"), generated.getParameter("planetaryFeatures"));
  EXPECT_TRUE(generated.getParameter("environmentStatusEffectPolicies").isType(Json::Type::Object));
  EXPECT_EQ(diskRoundTrip.getParameter("environmentStatusEffectPolicies"),
      generated.getParameter("environmentStatusEffectPolicies"));
  EXPECT_EQ(netRoundTrip.getParameter("environmentStatusEffectPolicies"),
      generated.getParameter("environmentStatusEffectPolicies"));
}

TEST(PlanetaryFeatureTest, EnvironmentStatusEffectPoliciesSupportPlanetAndLayerModes) {
  Json config = JsonObject{
    {"environmentStatusEffectsMode", "layer"},
    {"keepPrimaryRegionStatusEffectsAlways", true},
    {"layers", JsonObject{
      {"surface", JsonObject{
        {"environmentStatusEffectsMode", "region"},
        {"keepPrimaryRegionStatusEffectsAlways", false}}},
      {"space", JsonObject{}}}}};

  Json expected = JsonObject{
      {"space", JsonObject{
        {"mode", "layer"},
        {"keepPrimaryRegionStatusEffectsAlways", true}}},
      {"surface", JsonObject{
        {"mode", "region"},
        {"keepPrimaryRegionStatusEffectsAlways", false}}}};
  EXPECT_EQ(environmentStatusEffectPoliciesFromConfig(config, JsonObject{}), expected);
}

TEST(PlanetaryFeatureTest, EnvironmentStatusEffectPoliciesDefaultToVanillaGlobalBehavior) {
  Json config = JsonObject{{"layers", JsonObject{{"surface", JsonObject{}}}}};
  Json expected = JsonObject{{"surface", JsonObject{
      {"mode", "global"},
      {"keepPrimaryRegionStatusEffectsAlways", false}}}};
  EXPECT_EQ(environmentStatusEffectPoliciesFromConfig(config, JsonObject{}), expected);
}

TEST(PlanetaryFeatureTest, FeatureEnvironmentPoliciesUseMostLocalModeAndBooleanOr) {
  Json config = JsonObject{{"layers", JsonObject{
      {"surface", JsonObject{}},
      {"space", JsonObject{}}}}};
  Json layerFeature = JsonObject{
    {"surface", JsonObject{{"environmentStatusEffectsMode", "layer"}}},
    {"space", JsonObject{{"keepPrimaryRegionStatusEffectsAlways", true}}},
    {"missingLayer", JsonObject{{"environmentStatusEffectsMode", "region"}}}};
  Json regionFeature = JsonObject{
    {"surface", JsonObject{
      {"environmentStatusEffectsMode", "region"},
      {"keepPrimaryRegionStatusEffectsAlways", true}}}};
  Json definitions = JsonObject{
    {"layerFeature", JsonObject{{"layers", layerFeature}}},
    {"regionFeature", JsonObject{{"layers", regionFeature}}},
    {"lowerPriorityFeature", JsonObject{{"layers", JsonObject{
      {"surface", JsonObject{
        {"environmentStatusEffectsMode", "global"},
        {"keepPrimaryRegionStatusEffectsAlways", false}}}}}}}};

  Json expected = JsonObject{
      {"space", JsonObject{
        {"mode", "global"},
        {"keepPrimaryRegionStatusEffectsAlways", true}}},
      {"surface", JsonObject{
        {"mode", "region"},
        {"keepPrimaryRegionStatusEffectsAlways", true}}}};
  EXPECT_EQ(environmentStatusEffectPoliciesFromConfig(
      config, definitions, StringList{"layerFeature", "regionFeature", "lowerPriorityFeature"}), expected);
}

TEST(PlanetaryFeatureTest, FeatureRootEnvironmentPolicyAppliesToAllPlanetLayers) {
  Json config = JsonObject{{"layers", JsonObject{
      {"surface", JsonObject{}}, {"underground1", JsonObject{}}}}};
  Json definitions = JsonObject{{"regional", JsonObject{
      {"environmentStatusEffectsMode", "region"},
      {"keepPrimaryRegionStatusEffectsAlways", true}}}};

  auto policies = environmentStatusEffectPoliciesFromConfig(
      config, definitions, StringList{"regional"});
  EXPECT_EQ(policies.get("surface"), policies.get("underground1"));
  Json expected = JsonObject{
      {"mode", "region"},
      {"keepPrimaryRegionStatusEffectsAlways", true}};
  EXPECT_EQ(policies.get("surface"), expected);
}

TEST(PlanetaryFeatureTest, InvalidEnvironmentStatusEffectModeIsRejected) {
  Json config = JsonObject{
    {"environmentStatusEffectsMode", "localish"},
    {"layers", JsonObject{{"surface", JsonObject{}}}}};
  EXPECT_THROW(environmentStatusEffectPoliciesFromConfig(config, JsonObject{}), StarException);

  Json validConfig = JsonObject{{"layers", JsonObject{{"surface", JsonObject{}}}}};
  Json invalidDefinitions = JsonObject{{"invalid", JsonObject{{"layers", JsonObject{
      {"missingLayer", JsonObject{{"environmentStatusEffectsMode", "localish"}}}}}}}};
  EXPECT_THROW(environmentStatusEffectPoliciesFromConfig(
      validConfig, invalidDefinitions, StringList{"invalid"}), StarException);
}

TEST(PlanetaryFeatureTest, DungeonSelectionIsFeatureOwnedAndDeterministic) {
  Json layerConfig = JsonObject{
    {"dungeons", JsonArray{
      JsonArray{1.0, "featureDungeonA"},
      JsonArray{1.0, "featureDungeonB"},
      JsonArray{1.0, "featureDungeonC"}}},
    {"dungeonCountRange", JsonArray{2, 2}}};

  auto first = selectPlanetaryFeatureDungeons(layerConfig, 12345, "featureA", "surface");
  auto second = selectPlanetaryFeatureDungeons(layerConfig, 12345, "featureA", "surface");
  EXPECT_EQ(first, second);
  EXPECT_EQ(first.size(), 2u);
  EXPECT_EQ(StringSet::from(first).size(), first.size());
}

TEST(PlanetaryFeatureTest, DungeonSelectionDefaultsToNoContribution) {
  EXPECT_TRUE(selectPlanetaryFeatureDungeons(JsonObject{}, 12345, "featureA", "surface").empty());
  EXPECT_TRUE(selectPlanetaryFeatureDungeons(
      JsonObject{{"dungeons", JsonArray{JsonArray{1.0, "featureDungeon"}}}},
      12345, "featureA", "surface").empty());
}

TEST(PlanetaryFeatureTest, DungeonSelectionRejectsInvalidCountRange) {
  EXPECT_THROW(selectPlanetaryFeatureDungeons(
      JsonObject{
        {"dungeons", JsonArray{JsonArray{1.0, "featureDungeon"}}},
        {"dungeonCountRange", JsonArray{2, 1}}},
      12345, "featureA", "surface"), StarException);
}

TEST(PlanetaryFeatureTest, FeatureDungeonsAppendByDefault) {
  StringList dungeons{"baseDungeon"};
  Json layerConfig = JsonObject{
    {"dungeons", JsonArray{JsonArray{1.0, "featureDungeon"}}},
    {"dungeonCountRange", JsonArray{1, 1}}};

  applyPlanetaryFeatureDungeons(dungeons, layerConfig, 12345, "featureA", "surface");
  EXPECT_EQ(dungeons, (StringList{"baseDungeon", "featureDungeon"}));
}

TEST(PlanetaryFeatureTest, ReplaceDungeonsClearsEarlierSelections) {
  StringList dungeons{"baseDungeon", "earlierFeatureDungeon"};
  Json layerConfig = JsonObject{
    {"replaceDungeons", true},
    {"dungeons", JsonArray{JsonArray{1.0, "replacementDungeon"}}},
    {"dungeonCountRange", JsonArray{1, 1}}};

  applyPlanetaryFeatureDungeons(dungeons, layerConfig, 12345, "featureA", "surface");
  EXPECT_EQ(dungeons, (StringList{"replacementDungeon"}));
}

TEST(PlanetaryFeatureTest, ReplaceDungeonsWithoutPoolRemovesAllDungeons) {
  StringList dungeons{"baseDungeon", "earlierFeatureDungeon"};
  applyPlanetaryFeatureDungeons(dungeons, JsonObject{{"replaceDungeons", true}},
      12345, "featureA", "surface");
  EXPECT_TRUE(dungeons.empty());
}
