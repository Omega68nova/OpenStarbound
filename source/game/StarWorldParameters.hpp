#pragma once

#include "StarGameTypes.hpp"
#include "StarSkyTypes.hpp"
#include "StarWeatherTypes.hpp"
#include "StarForceRegions.hpp"

namespace Star {

enum class WorldParametersType : uint8_t {
  TerrestrialWorldParameters,
  AsteroidsWorldParameters,
  FloatingDungeonWorldParameters
};
extern EnumMap<WorldParametersType> const WorldParametersTypeNames;

enum class BeamUpRule : uint8_t {
  Nowhere,
  Surface,
  Anywhere,
  AnywhereWithWarning
};
extern EnumMap<BeamUpRule> const BeamUpRuleNames;

enum class WorldEdgeForceRegionType : uint8_t {
  None,
  Top,
  Bottom,
  TopAndBottom
};
extern EnumMap<WorldEdgeForceRegionType> const WorldEdgeForceRegionTypeNames;

STAR_STRUCT(VisitableWorldParameters);
STAR_STRUCT(TerrestrialWorldParameters);
STAR_STRUCT(AsteroidsWorldParameters);
STAR_STRUCT(FloatingDungeonWorldParameters);

struct VisitableWorldParameters {
  VisitableWorldParameters() = default;
  VisitableWorldParameters(VisitableWorldParameters const& visitableWorldParameters) = default;
  explicit VisitableWorldParameters(Json const& store);

  virtual ~VisitableWorldParameters() = default;

  virtual WorldParametersType type() const = 0;

  virtual Json store() const;

  virtual void read(DataStream& ds);
  virtual void write(DataStream& ds) const;

  String typeName;
  float threatLevel{};
  Vec2U worldSize;
  float gravity{};
  bool airless{false};
  WeatherPool weatherPool;
  StringList environmentStatusEffects;
  Maybe<StringList> overrideTech;
  Maybe<List<Directives>> globalDirectives;
  BeamUpRule beamUpRule;
  bool disableDeathDrops{false};
  bool terraformed{false};
  WorldEdgeForceRegionType worldEdgeForceRegions{WorldEdgeForceRegionType::None};
};

struct TerrestrialWorldParameters : VisitableWorldParameters {
  struct TerrestrialRegion {
    String biome;

    String blockSelector;
    String fgCaveSelector;
    String bgCaveSelector;
    String fgOreSelector;
    String bgOreSelector;
    String subBlockSelector;

    LiquidId caveLiquid{};
    float caveLiquidSeedDensity{};

    LiquidId oceanLiquid{};
    int oceanLiquidLevel{};

    bool encloseLiquids{false};
    bool fillMicrodungeons{false};
  };

  struct TerrestrialLayer {
    int layerMinHeight;
    int layerBaseHeight;

    StringList dungeons;
    int dungeonXVariance;

    TerrestrialRegion primaryRegion;
    TerrestrialRegion primarySubRegion;

    List<TerrestrialRegion> secondaryRegions;
    List<TerrestrialRegion> secondarySubRegions;

    Vec2F secondaryRegionSizeRange;
    Vec2F subRegionSizeRange;
  };

  TerrestrialWorldParameters() = default;
  TerrestrialWorldParameters(TerrestrialWorldParameters const& terrestrialWorldParameters) = default;
  explicit TerrestrialWorldParameters(Json const& store);

  TerrestrialWorldParameters &operator=(TerrestrialWorldParameters const& terrestrialWorldParameters);

  WorldParametersType type() const override;

  Json store() const override;

  void read(DataStream& ds) override;
  void write(DataStream& ds) const override;

  String primaryBiome;
  LiquidId primarySurfaceLiquid{};
  String sizeName;
  float hueShift{};

  SkyColoring skyColoring;
  float dayLength{};

  Json blockNoiseConfig;
  Json blendNoiseConfig;
  float blendSize{};

  TerrestrialLayer spaceLayer{};
  TerrestrialLayer atmosphereLayer{};
  TerrestrialLayer surfaceLayer{};
  TerrestrialLayer subsurfaceLayer{};
  List<TerrestrialLayer> undergroundLayers;
  TerrestrialLayer coreLayer{};
};

struct AsteroidsWorldParameters : VisitableWorldParameters {
  AsteroidsWorldParameters();
  explicit AsteroidsWorldParameters(Json const& store);

  WorldParametersType type() const override;

  Json store() const override;

  void read(DataStream& ds) override;
  void write(DataStream& ds) const override;

  int asteroidTopLevel{};
  int asteroidBottomLevel{};
  float blendSize{};
  String asteroidBiome;
  Color ambientLightLevel;
};

struct FloatingDungeonWorldParameters : VisitableWorldParameters {
  FloatingDungeonWorldParameters() = default;
  explicit FloatingDungeonWorldParameters(Json const& store);

  WorldParametersType type() const override;

  Json store() const override;

  void read(DataStream& ds) override;
  void write(DataStream& ds) const override;

  int dungeonBaseHeight;
  int dungeonSurfaceHeight;
  int dungeonUndergroundLevel;
  String primaryDungeon;
  Color ambientLightLevel;
  Maybe<String> biome;
  Maybe<String> dayMusicTrack;
  Maybe<String> nightMusicTrack;
  Maybe<String> dayAmbientNoises;
  Maybe<String> nightAmbientNoises;
};

Json diskStoreVisitableWorldParameters(VisitableWorldParametersConstPtr const& parameters);
VisitableWorldParametersPtr diskLoadVisitableWorldParameters(Json const& store);

ByteArray netStoreVisitableWorldParameters(VisitableWorldParametersConstPtr const& parameters);
VisitableWorldParametersPtr netLoadVisitableWorldParameters(ByteArray data);

// Recursively selects persistent planetary features from the independently
// rolled typed feature and group references configured for a newly generated
// terrestrial celestial body.
StringList selectPlanetaryFeatures(String const& typeName, String const& sizeName, uint64_t seed);
StringList selectPlanetaryFeaturesFromConfig(Json const& selectionConfig, Json const& featureDefinitions,
    uint64_t seed, Json const& featureGroups = JsonObject{});

// Resolves environmental status-effect behavior for every terrestrial layer.
// Each returned object contains a mode (global, layer, or region) and the
// keepPrimaryRegionStatusEffectsAlways flag. More local modes contributed by
// selected features win, while keep-primary flags combine with boolean OR.
Json environmentStatusEffectPolicies(String const& typeName, String const& sizeName,
    StringList const& planetaryFeatures = {});
Json environmentStatusEffectPoliciesFromConfig(Json const& planetConfig,
    Json const& featureDefinitions, StringList const& planetaryFeatures = {});

// Returns a planetary feature definition from terrestrial_worlds.config, or
// nothing when the definition is no longer present.
Maybe<Json> planetaryFeatureConfig(String const& featureName);

// Combines every weather pool asset added to a layer by the selected features.
WeatherPool planetaryFeatureWeatherPool(StringList const& planetaryFeatures, String const& layerName);

// Selects the dungeons contributed independently by one planetary feature layer.
StringList selectPlanetaryFeatureDungeons(Json const& layerConfig, uint64_t seed, String const& featureName, String const& layerName);

// Applies one planetary feature's dungeon configuration to a generated layer.
// replaceDungeons clears all earlier dungeon selections before adding this
// feature's selections, including when the feature selects no dungeons.
void applyPlanetaryFeatureDungeons(StringList& dungeons, Json const& layerConfig,
    uint64_t seed, String const& featureName, String const& layerName);

TerrestrialWorldParametersPtr generateTerrestrialWorldParameters(
    String const& typeName, String const& sizeName, uint64_t seed, StringList const& planetaryFeatures = {});
AsteroidsWorldParametersPtr generateAsteroidsWorldParameters(uint64_t seed);
FloatingDungeonWorldParametersPtr generateFloatingDungeonWorldParameters(String const& dungeonWorldName);

}
