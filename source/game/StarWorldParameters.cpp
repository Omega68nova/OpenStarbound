#include "StarWorldParameters.hpp"
#include "StarJsonExtra.hpp"
#include "StarDataStreamExtra.hpp"
#include "StarRoot.hpp"
#include "StarAssets.hpp"
#include "StarBiomeDatabase.hpp"
#include "StarLiquidsDatabase.hpp"
#include "StarLogging.hpp"

namespace Star {

EnumMap<WorldParametersType> const WorldParametersTypeNames{
    {WorldParametersType::TerrestrialWorldParameters, "TerrestrialWorldParameters"},
    {WorldParametersType::AsteroidsWorldParameters, "AsteroidsWorldParameters"},
    {WorldParametersType::FloatingDungeonWorldParameters, "FloatingDungeonWorldParameters"}};

EnumMap<BeamUpRule> const BeamUpRuleNames{
  {BeamUpRule::Nowhere, "Nowhere"},
  {BeamUpRule::Surface, "Surface"},
  {BeamUpRule::Anywhere, "Anywhere"},
  {BeamUpRule::AnywhereWithWarning, "AnywhereWithWarning"}};

EnumMap<WorldEdgeForceRegionType> const WorldEdgeForceRegionTypeNames{
  {WorldEdgeForceRegionType::None, "None"},
  {WorldEdgeForceRegionType::Top, "Top"},
  {WorldEdgeForceRegionType::Bottom, "Bottom"},
  {WorldEdgeForceRegionType::TopAndBottom, "TopAndBottom"}};

VisitableWorldParameters::VisitableWorldParameters(Json const& store) {
  typeName = store.getString("typeName", "");
  threatLevel = store.getFloat("threatLevel");
  worldSize = jsonToVec2U(store.get("worldSize"));
  gravity = store.getFloat("gravity", 1.0f);
  airless = store.getBool("airless", false);
  weatherPool = jsonToWeightedPool<String>(store.getArray("weatherPool", JsonArray()));
  environmentStatusEffects = store.opt("environmentStatusEffects").apply(jsonToStringList).value();
  overrideTech = store.opt("overrideTech").apply(jsonToStringList);
  globalDirectives = store.opt("globalDirectives").apply(jsonToDirectivesList);
  beamUpRule = BeamUpRuleNames.getLeft(store.getString("beamUpRule", "Surface"));
  disableDeathDrops = store.getBool("disableDeathDrops", false);
  terraformed = store.getBool("terraformed", false);
  worldEdgeForceRegions = WorldEdgeForceRegionTypeNames.getLeft(store.getString("worldEdgeForceRegions", "None"));
}

Json VisitableWorldParameters::store() const {
  return JsonObject{{"typeName", typeName},
      {"threatLevel", threatLevel},
      {"worldSize", jsonFromVec2U(worldSize)},
      {"gravity", gravity},
      {"airless", airless},
      {"weatherPool", jsonFromWeightedPool<String>(weatherPool)},
      {"environmentStatusEffects", jsonFromStringList(environmentStatusEffects)},
      {"overrideTech", jsonFromMaybe(overrideTech.apply(&jsonFromStringList))},
      {"globalDirectives", jsonFromMaybe(globalDirectives.apply(&jsonFromDirectivesList))},
      {"beamUpRule", BeamUpRuleNames.getRight(beamUpRule)},
      {"disableDeathDrops", disableDeathDrops},
      {"terraformed", terraformed},
      {"worldEdgeForceRegions", WorldEdgeForceRegionTypeNames.getRight(worldEdgeForceRegions)}};
}

void VisitableWorldParameters::read(DataStream& ds) {
  ds >> typeName;
  ds >> threatLevel;
  ds >> worldSize;
  ds >> gravity;
  ds >> airless;
  weatherPool = WeatherPool(ds.read<WeatherPool::ItemsList>());
  ds >> environmentStatusEffects;
  ds >> overrideTech;
  ds >> globalDirectives;
  ds >> beamUpRule;
  ds >> disableDeathDrops;
  ds >> terraformed;
  ds >> worldEdgeForceRegions;
}

void VisitableWorldParameters::write(DataStream& ds) const {
  ds << typeName;
  ds << threatLevel;
  ds << worldSize;
  ds << gravity;
  ds << airless;
  ds.writeContainer<WeatherPool::ItemsList>(weatherPool.items());
  ds << environmentStatusEffects;
  ds << overrideTech;
  ds << globalDirectives;
  ds << beamUpRule;
  ds << disableDeathDrops;
  ds << terraformed;
  ds << worldEdgeForceRegions;
}

TerrestrialWorldParameters::TerrestrialWorldParameters(Json const& store) : VisitableWorldParameters(store) {
  auto loadTerrestrialRegion = [](Json const& config) {
    return TerrestrialRegion{config.getString("biome"),
        config.getString("blockSelector"),
        config.getString("fgCaveSelector"),
        config.getString("bgCaveSelector"),
        config.getString("fgOreSelector"),
        config.getString("bgOreSelector"),
        config.getString("subBlockSelector"),
        static_cast<LiquidId>(config.getUInt("caveLiquid")),
        config.getFloat("caveLiquidSeedDensity"),
        static_cast<LiquidId>(config.getUInt("oceanLiquid")),
        static_cast<int>(config.getInt("oceanLiquidLevel")),
        config.getBool("encloseLiquids"),
        config.getBool("fillMicrodungeons")};
  };

  auto loadTerrestrialLayer = [loadTerrestrialRegion](Json const& config) {
    return TerrestrialLayer{static_cast<int>(config.getInt("layerMinHeight")),
        static_cast<int>(config.getInt("layerBaseHeight")),
        jsonToStringList(config.get("dungeons")),
        static_cast<int>(config.getInt("dungeonXVariance")),
        loadTerrestrialRegion(config.get("primaryRegion")),
        loadTerrestrialRegion(config.get("primarySubRegion")),
        config.getArray("secondaryRegions").transformed(loadTerrestrialRegion),
        config.getArray("secondarySubRegions").transformed(loadTerrestrialRegion),
        jsonToVec2F(config.get("secondaryRegionSizeRange")),
        jsonToVec2F(config.get("subRegionSizeRange"))};
  };

  primaryBiome = store.getString("primaryBiome");
  primarySurfaceLiquid = store.getUInt("surfaceLiquid");
  sizeName = store.getString("sizeName");
  hueShift = store.getFloat("hueShift");
  skyColoring = SkyColoring(store.get("skyColoring"));
  dayLength = store.getFloat("dayLength");
  blockNoiseConfig = store.get("blockNoise");
  blendNoiseConfig = store.get("blendNoise");
  blendSize = store.getFloat("blendSize");

  spaceLayer = loadTerrestrialLayer(store.get("spaceLayer"));
  atmosphereLayer = loadTerrestrialLayer(store.get("atmosphereLayer"));
  surfaceLayer = loadTerrestrialLayer(store.get("surfaceLayer"));
  subsurfaceLayer = loadTerrestrialLayer(store.get("subsurfaceLayer"));
  undergroundLayers = store.getArray("undergroundLayers").transformed(loadTerrestrialLayer);
  coreLayer = loadTerrestrialLayer(store.get("coreLayer"));
}

TerrestrialWorldParameters &TerrestrialWorldParameters::operator=(TerrestrialWorldParameters const& terrestrialWorldParameters) {
  this->primaryBiome = terrestrialWorldParameters.primaryBiome;
  this->primarySurfaceLiquid = terrestrialWorldParameters.primarySurfaceLiquid;
  this->sizeName = terrestrialWorldParameters.sizeName;
  this->hueShift = terrestrialWorldParameters.hueShift;

  this->skyColoring = terrestrialWorldParameters.skyColoring;
  this->dayLength = terrestrialWorldParameters.dayLength;

  this->blockNoiseConfig = terrestrialWorldParameters.blockNoiseConfig;
  this->blendNoiseConfig = terrestrialWorldParameters.blendNoiseConfig;
  this->blendSize = terrestrialWorldParameters.blendSize;

  this->spaceLayer = terrestrialWorldParameters.spaceLayer;
  this->atmosphereLayer = terrestrialWorldParameters.atmosphereLayer;
  this->surfaceLayer = terrestrialWorldParameters.surfaceLayer;
  this->subsurfaceLayer = terrestrialWorldParameters.subsurfaceLayer;
  this->undergroundLayers = terrestrialWorldParameters.undergroundLayers;
  this->coreLayer = terrestrialWorldParameters.coreLayer;
  return *this;
}

WorldParametersType TerrestrialWorldParameters::type() const {
  return WorldParametersType::TerrestrialWorldParameters;
}

Json TerrestrialWorldParameters::store() const {
  auto storeTerrestrialRegion = [](TerrestrialRegion const& region) -> Json {
    return JsonObject{{"biome", region.biome},
        {"blockSelector", region.blockSelector},
        {"fgCaveSelector", region.fgCaveSelector},
        {"bgCaveSelector", region.bgCaveSelector},
        {"fgOreSelector", region.fgOreSelector},
        {"bgOreSelector", region.bgOreSelector},
        {"subBlockSelector", region.subBlockSelector},
        {"caveLiquid", region.caveLiquid},
        {"caveLiquidSeedDensity", region.caveLiquidSeedDensity},
        {"oceanLiquid", region.oceanLiquid},
        {"oceanLiquidLevel", region.oceanLiquidLevel},
        {"encloseLiquids", region.encloseLiquids},
        {"fillMicrodungeons", region.fillMicrodungeons}};
  };
  auto storeTerrestrialLayer = [storeTerrestrialRegion](TerrestrialLayer const& layer) -> Json {
    return JsonObject{{"layerMinHeight", layer.layerMinHeight},
        {"layerBaseHeight", layer.layerBaseHeight},
        {"dungeons", jsonFromStringList(layer.dungeons)},
        {"dungeonXVariance", layer.dungeonXVariance},
        {"primaryRegion", storeTerrestrialRegion(layer.primaryRegion)},
        {"primarySubRegion", storeTerrestrialRegion(layer.primarySubRegion)},
        {"secondaryRegions", layer.secondaryRegions.transformed(storeTerrestrialRegion)},
        {"secondarySubRegions", layer.secondarySubRegions.transformed(storeTerrestrialRegion)},
        {"secondaryRegionSizeRange", jsonFromVec2F(layer.secondaryRegionSizeRange)},
        {"subRegionSizeRange", jsonFromVec2F(layer.subRegionSizeRange)}};
  };

  return VisitableWorldParameters::store().setAll(JsonObject{{"primaryBiome", primaryBiome},
      {"sizeName", sizeName},
      {"hueShift", hueShift},
      {"surfaceLiquid", primarySurfaceLiquid},
      {"skyColoring", skyColoring.toJson()},
      {"dayLength", dayLength},
      {"blockNoise", blockNoiseConfig},
      {"blendNoise", blendNoiseConfig},
      {"blendSize", blendSize},
      {"spaceLayer", storeTerrestrialLayer(spaceLayer)},
      {"atmosphereLayer", storeTerrestrialLayer(atmosphereLayer)},
      {"surfaceLayer", storeTerrestrialLayer(surfaceLayer)},
      {"subsurfaceLayer", storeTerrestrialLayer(subsurfaceLayer)},
      {"undergroundLayers", undergroundLayers.transformed(storeTerrestrialLayer)},
      {"coreLayer", storeTerrestrialLayer(coreLayer)}});
}

DataStream& operator>>(DataStream& ds, TerrestrialWorldParameters::TerrestrialRegion& region) {
  ds >> region.biome;
  ds >> region.blockSelector;
  ds >> region.fgCaveSelector;
  ds >> region.bgCaveSelector;
  ds >> region.fgOreSelector;
  ds >> region.bgOreSelector;
  ds >> region.subBlockSelector;
  ds >> region.caveLiquid;
  ds >> region.caveLiquidSeedDensity;
  ds >> region.oceanLiquid;
  ds >> region.oceanLiquidLevel;
  ds >> region.encloseLiquids;
  ds >> region.fillMicrodungeons;
  return ds;
}

void TerrestrialWorldParameters::read(DataStream& ds) {
  auto readTerrestrialLayer = [](DataStream& ds, TerrestrialLayer& layer) {
    ds >> layer.layerMinHeight;
    ds >> layer.layerBaseHeight;
    ds >> layer.dungeons;
    ds >> layer.dungeonXVariance;
    ds >> layer.primaryRegion;
    ds >> layer.primarySubRegion;
    ds >> layer.secondaryRegions;
    ds >> layer.secondarySubRegions;
    ds >> layer.secondaryRegionSizeRange;
    ds >> layer.subRegionSizeRange;
  };

  VisitableWorldParameters::read(ds);
  ds >> primaryBiome;
  ds >> primarySurfaceLiquid;
  ds >> sizeName;
  ds >> hueShift;
  ds >> skyColoring;
  ds >> dayLength;
  ds >> blendSize;
  ds >> blockNoiseConfig;
  ds >> blendNoiseConfig;
  readTerrestrialLayer(ds, spaceLayer);
  readTerrestrialLayer(ds, atmosphereLayer);
  readTerrestrialLayer(ds, surfaceLayer);
  readTerrestrialLayer(ds, subsurfaceLayer);
  ds.readContainer(undergroundLayers, readTerrestrialLayer);
  readTerrestrialLayer(ds, coreLayer);
}

DataStream& operator<<(DataStream& ds, TerrestrialWorldParameters::TerrestrialRegion const& region) {
  ds << region.biome;
  ds << region.blockSelector;
  ds << region.fgCaveSelector;
  ds << region.bgCaveSelector;
  ds << region.fgOreSelector;
  ds << region.bgOreSelector;
  ds << region.subBlockSelector;
  ds << region.caveLiquid;
  ds << region.caveLiquidSeedDensity;
  ds << region.oceanLiquid;
  ds << region.oceanLiquidLevel;
  ds << region.encloseLiquids;
  ds << region.fillMicrodungeons;
  return ds;
}

void TerrestrialWorldParameters::write(DataStream& ds) const {
  auto writeTerrestrialLayer = [](DataStream& ds, TerrestrialLayer const& layer) {
    ds << layer.layerMinHeight;
    ds << layer.layerBaseHeight;
    ds << layer.dungeons;
    ds << layer.dungeonXVariance;
    ds << layer.primaryRegion;
    ds << layer.primarySubRegion;
    ds << layer.secondaryRegions;
    ds << layer.secondarySubRegions;
    ds << layer.secondaryRegionSizeRange;
    ds << layer.subRegionSizeRange;
  };

  VisitableWorldParameters::write(ds);
  ds << primaryBiome;
  ds << primarySurfaceLiquid;
  ds << sizeName;
  ds << hueShift;
  ds << skyColoring;
  ds << dayLength;
  ds << blendSize;
  ds << blockNoiseConfig;
  ds << blendNoiseConfig;
  writeTerrestrialLayer(ds, spaceLayer);
  writeTerrestrialLayer(ds, atmosphereLayer);
  writeTerrestrialLayer(ds, surfaceLayer);
  writeTerrestrialLayer(ds, subsurfaceLayer);
  ds.writeContainer(undergroundLayers, writeTerrestrialLayer);
  writeTerrestrialLayer(ds, coreLayer);
}

AsteroidsWorldParameters::AsteroidsWorldParameters() {
  airless = true;
}

AsteroidsWorldParameters::AsteroidsWorldParameters(Json const& store) : VisitableWorldParameters(store) {
  asteroidTopLevel = static_cast<int>(store.getInt("asteroidTopLevel"));
  asteroidBottomLevel = static_cast<int>(store.getInt("asteroidBottomLevel"));
  blendSize = store.getFloat("blendSize");
  asteroidBiome = store.getString("asteroidBiome");
  ambientLightLevel = jsonToColor(store.get("ambientLightLevel"));
}

WorldParametersType AsteroidsWorldParameters::type() const {
  return WorldParametersType::AsteroidsWorldParameters;
}

Json AsteroidsWorldParameters::store() const {
  return VisitableWorldParameters::store().setAll(JsonObject{{"asteroidTopLevel", asteroidTopLevel},
      {"asteroidBottomLevel", asteroidBottomLevel},
      {"blendSize", blendSize},
      {"asteroidBiome", asteroidBiome},
      {"ambientLightLevel", jsonFromColor(ambientLightLevel)}});
}

void AsteroidsWorldParameters::read(DataStream& ds) {
  VisitableWorldParameters::read(ds);
  ds >> asteroidTopLevel;
  ds >> asteroidBottomLevel;
  ds >> blendSize;
  ds >> asteroidBiome;
  ds >> ambientLightLevel;
}

void AsteroidsWorldParameters::write(DataStream& ds) const {
  VisitableWorldParameters::write(ds);
  ds << asteroidTopLevel;
  ds << asteroidBottomLevel;
  ds << blendSize;
  ds << asteroidBiome;
  ds << ambientLightLevel;
}

FloatingDungeonWorldParameters::FloatingDungeonWorldParameters(Json const& store) : VisitableWorldParameters(store) {
  dungeonBaseHeight = static_cast<int>(store.getInt("dungeonBaseHeight"));
  dungeonSurfaceHeight = static_cast<int>(store.getInt("dungeonSurfaceHeight"));
  dungeonUndergroundLevel = static_cast<int>(store.getInt("dungeonUndergroundLevel"));
  primaryDungeon = store.getString("primaryDungeon");
  biome = store.optString("biome");
  ambientLightLevel = jsonToColor(store.get("ambientLightLevel"));
  dayMusicTrack = store.optString("dayMusicTrack");
  nightMusicTrack = store.optString("nightMusicTrack");
  dayAmbientNoises = store.optString("dayAmbientNoises");
  nightAmbientNoises = store.optString("nightAmbientNoises");
}

WorldParametersType FloatingDungeonWorldParameters::type() const {
  return WorldParametersType::FloatingDungeonWorldParameters;
}

Json FloatingDungeonWorldParameters::store() const {
  return VisitableWorldParameters::store().setAll(JsonObject{{"dungeonBaseHeight", dungeonBaseHeight},
      {"dungeonSurfaceHeight", dungeonSurfaceHeight},
      {"dungeonUndergroundLevel", dungeonUndergroundLevel},
      {"primaryDungeon", primaryDungeon},
      {"biome", jsonFromMaybe(biome)},
      {"ambientLightLevel", jsonFromColor(ambientLightLevel)},
      {"dayMusicTrack", jsonFromMaybe(dayMusicTrack)},
      {"nightMusicTrack", jsonFromMaybe(nightMusicTrack)},
      {"dayAmbientNoises", jsonFromMaybe(dayAmbientNoises)},
      {"nightAmbientNoises", jsonFromMaybe(nightAmbientNoises)}});
}

void FloatingDungeonWorldParameters::read(DataStream& ds) {
  VisitableWorldParameters::read(ds);
  ds >> dungeonBaseHeight;
  ds >> dungeonSurfaceHeight;
  ds >> dungeonUndergroundLevel;
  ds >> primaryDungeon;
  ds >> biome;
  ds >> ambientLightLevel;
  ds >> dayMusicTrack;
  ds >> nightMusicTrack;
  ds >> dayAmbientNoises;
  ds >> nightAmbientNoises;
}

void FloatingDungeonWorldParameters::write(DataStream& ds) const {
  VisitableWorldParameters::write(ds);
  ds << dungeonBaseHeight;
  ds << dungeonSurfaceHeight;
  ds << dungeonUndergroundLevel;
  ds << primaryDungeon;
  ds << biome;
  ds << ambientLightLevel;
  ds << dayMusicTrack;
  ds << nightMusicTrack;
  ds << dayAmbientNoises;
  ds << nightAmbientNoises;
}

Json diskStoreVisitableWorldParameters(VisitableWorldParametersConstPtr const& parameters) {
  if (!parameters)
    return {};

  return parameters->store().setAll({{"type", WorldParametersTypeNames.getRight(parameters->type())}});
}

VisitableWorldParametersPtr diskLoadVisitableWorldParameters(Json const& store) {
  if (store.isNull())
    return {};

  auto type = WorldParametersTypeNames.getLeft(store.getString("type"));
  if (type == WorldParametersType::TerrestrialWorldParameters)
    return make_shared<TerrestrialWorldParameters>(store);
  else if (type == WorldParametersType::AsteroidsWorldParameters)
    return make_shared<AsteroidsWorldParameters>(store);
  else if (type == WorldParametersType::FloatingDungeonWorldParameters)
    return make_shared<FloatingDungeonWorldParameters>(store);
  throw StarException("No such WorldParametersType");
}

ByteArray netStoreVisitableWorldParameters(VisitableWorldParametersConstPtr const& parameters) {
  if (!parameters)
    return {};

  DataStreamBuffer ds;
  ds.write(parameters->type());
  parameters->write(ds);
  return ds.takeData();
}

VisitableWorldParametersPtr netLoadVisitableWorldParameters(ByteArray data) {
  if (data.empty())
    return {};

  DataStreamBuffer ds(std::move(data));
  auto type = ds.read<WorldParametersType>();

  VisitableWorldParametersPtr parameters;
  if (type == WorldParametersType::TerrestrialWorldParameters)
    parameters = make_shared<TerrestrialWorldParameters>();
  else if (type == WorldParametersType::AsteroidsWorldParameters)
    parameters = make_shared<AsteroidsWorldParameters>();
  else if (type == WorldParametersType::FloatingDungeonWorldParameters)
    parameters = make_shared<FloatingDungeonWorldParameters>();
  else
    throw StarException("No such WorldParametersType");

  parameters->read(ds);

  return parameters;
}

Maybe<Json> planetaryFeatureConfig(String const& featureName) {
  auto features = Root::singleton().assets()->json("/terrestrial_worlds.config")
      .get("planetaryFeatures", JsonObject{});
  if (features.contains(featureName))
    return features.get(featureName);
  return {};
}

WeatherPool planetaryFeatureWeatherPool(StringList const& planetaryFeatures, String const& layerName) {
  WeatherPool result;
  auto assets = Root::singleton().assets();
  for (auto const& featureName : planetaryFeatures) {
    auto feature = planetaryFeatureConfig(featureName);
    if (!feature) {
      Logger::warn("Planetary feature '{}' is no longer defined; ignoring its weather additions", featureName);
      continue;
    }

    auto layers = feature->getObject("layers", JsonObject{});
    if (!layers.contains(layerName))
      continue;

    for (auto const& weatherPoolPath : layers.get(layerName).getArray("weatherPools", JsonArray{})) {
      auto pool = jsonToWeightedPool<String>(assets->fetchJson(weatherPoolPath.toString()));
      for (auto const& entry : pool.items())
        result.add(entry.first, entry.second);
    }
  }
  return result;
}

StringList selectPlanetaryFeatureDungeons(Json const& layerConfig, uint64_t seed, String const& featureName, String const& layerName) {
  WeightedPool<String> dungeonPool = jsonToWeightedPool<String>(layerConfig.get("dungeons", JsonArray{}));
  Vec2U dungeonCountRange = layerConfig.opt("dungeonCountRange").apply(jsonToVec2U).value(Vec2U());
  if (dungeonCountRange[0] > dungeonCountRange[1])
    throw StarException(strf("Planetary feature '{}' has an invalid dungeonCountRange for layer '{}'", featureName, layerName));

  unsigned dungeonCount = staticRandomU32Range(dungeonCountRange[0], dungeonCountRange[1],
      seed, layerName, featureName, "PlanetaryFeatureDungeonCount");
  return dungeonPool.selectUniques(dungeonCount,
      staticRandomHash64(seed, layerName, featureName, "PlanetaryFeatureDungeonChoice"));
}

void applyPlanetaryFeatureDungeons(StringList& dungeons, Json const& layerConfig,
    uint64_t seed, String const& featureName, String const& layerName) {
  if (layerConfig.getBool("replaceDungeons", false))
    dungeons.clear();

  for (auto const& dungeon : selectPlanetaryFeatureDungeons(layerConfig, seed, featureName, layerName)) {
    if (!dungeons.contains(dungeon))
      dungeons.append(dungeon);
  }
}

namespace {

enum class PlanetaryFeatureSelectionMode {
  Root,
  Guaranteed,
  Random,
  Alternative
};

struct PlanetaryFeatureSelectionNode;
using PlanetaryFeatureSelectionNodePtr = shared_ptr<PlanetaryFeatureSelectionNode>;

struct PlanetaryFeatureSelectionNode {
  String reference;
  String id;
  String path;
  bool group = false;
  bool active = true;
  PlanetaryFeatureSelectionMode mode = PlanetaryFeatureSelectionMode::Root;
  PlanetaryFeatureSelectionNode* parent = nullptr;
  List<PlanetaryFeatureSelectionNodePtr> children;
};

struct PlanetaryFeatureClaim {
  String resource;
  String featureId;
  PlanetaryFeatureSelectionNode* feature;
};

pair<bool, String> parsePlanetaryFeatureReference(String const& reference) {
  if (reference.beginsWith("feature:") && reference.size() > 8)
    return {false, reference.substr(8)};
  if (reference.beginsWith("group:") && reference.size() > 6)
    return {true, reference.substr(6)};
  throw StarException(strf("Invalid planetary feature reference '{}'; expected feature:<id> or group:<id>", reference));
}

double planetaryFeatureChance(Json const& value, String const& context) {
  double chance = value.toDouble();
  if (chance < 0.0 || chance > 1.0)
    throw StarException(strf("Planetary feature chance at '{}' must be between 0 and 1", context));
  return chance;
}

StringList sortedObjectKeys(JsonObject const& object) {
  StringList keys;
  for (auto const& entry : object)
    keys.append(entry.first);
  keys.sort();
  return keys;
}

class PlanetaryFeatureSelector {
public:
  PlanetaryFeatureSelector(Json featureDefinitions, Json featureGroups, uint64_t seed)
    : m_featureDefinitions(std::move(featureDefinitions)), m_featureGroups(std::move(featureGroups)), m_seed(seed) {}

  StringList select(Json const& selectionConfig) {
    if (selectionConfig.contains("maximumPlanetaryFeatures")
        || selectionConfig.contains("planetaryFeatureChance")
        || selectionConfig.contains("planetaryFeaturePool")) {
      throw StarException("Legacy planetary feature slot configuration is not supported; configure typed references in planetaryFeatures");
    }

    auto roots = selectionConfig.getObject("planetaryFeatures", JsonObject{});
    for (auto const& reference : sortedObjectKeys(roots))
      validate(reference, strf("planetaryFeatures.{}", reference), {});
    for (auto const& reference : sortedObjectKeys(roots)) {
      double chance = planetaryFeatureChance(roots.get(reference), strf("planetaryFeatures.{}", reference));
      String path = strf("root/{}", reference);
      if (staticRandomDouble(m_seed, "PlanetaryFeatureChance", path) < chance)
        m_roots.append(expand(reference, PlanetaryFeatureSelectionMode::Root, path, nullptr, {}));
    }

    validateGuaranteedSiblings();
    resolveConflicts();

    StringList selected;
    StringSet present;
    for (auto const& root : m_roots)
      flatten(root.get(), selected, present);
    return selected;
  }

private:
  StringList exclusiveResources(String const& featureId) const {
    StringList resources;
    for (auto const& layer : m_featureDefinitions.get(featureId).getObject("layers", JsonObject{})) {
      if (layer.second.contains("ocean"))
        resources.append(strf("ocean:{}", layer.first));
      if (layer.second.getBool("replaceDungeons", false))
        resources.append(strf("replaceDungeons:{}", layer.first));
    }
    return resources;
  }

  void validate(String const& reference, String const& path, StringList groupStack) const {
    auto parsed = parsePlanetaryFeatureReference(reference);
    if (!parsed.first) {
      if (!m_featureDefinitions.contains(parsed.second))
        throw StarException(strf("Unknown planetary feature '{}' referenced at '{}'", parsed.second, path));
      return;
    }

    if (!m_featureGroups.contains(parsed.second))
      throw StarException(strf("Unknown planetary feature group '{}' referenced at '{}'", parsed.second, path));
    if (groupStack.contains(parsed.second)) {
      groupStack.append(parsed.second);
      throw StarException(strf("Recursive planetary feature group cycle at '{}': {}", path, groupStack.join(" -> ")));
    }
    groupStack.append(parsed.second);

    auto groupConfig = m_featureGroups.get(parsed.second);
    for (auto const& key : sortedObjectKeys(groupConfig.toObject())) {
      if (key != "guaranteed" && key != "random" && key != "alternative")
        throw StarException(strf("Unknown planetary feature group property '{}' at '{}'", key, path));
    }

    auto guaranteed = groupConfig.getObject("guaranteed", JsonObject{});
    StringMap<String> guaranteedResources;
    for (auto const& childReference : sortedObjectKeys(guaranteed)) {
      bool enabled = guaranteed.get(childReference).toBool();
      auto child = parsePlanetaryFeatureReference(childReference);
      if (enabled) {
        if (!child.first && m_featureDefinitions.contains(child.second)) {
          for (auto const& resource : exclusiveResources(child.second)) {
            if (auto previous = guaranteedResources.maybe(resource)) {
              throw StarException(strf("Planetary feature group '{}' has conflicting guaranteed siblings '{}' and '{}' for '{}'",
                  path, *previous, child.second, resource));
            }
            guaranteedResources[resource] = child.second;
          }
        }
      }
      validate(childReference, strf("{}/guaranteed/{}", path, childReference), groupStack);
    }

    if (groupConfig.contains("random")) {
      auto randomConfig = groupConfig.get("random");
      for (auto const& key : sortedObjectKeys(randomConfig.toObject())) {
        if (key != "maxCount" && key != "members")
          throw StarException(strf("Unknown planetary feature random property '{}' at '{}'", key, path));
      }
      randomConfig.getUInt("maxCount", 0);
      auto members = randomConfig.getObject("members", JsonObject{});
      for (auto const& childReference : sortedObjectKeys(members)) {
        planetaryFeatureChance(members.get(childReference), strf("{}/random/{}", path, childReference));
        validate(childReference, strf("{}/random/{}", path, childReference), groupStack);
      }
    }

    if (groupConfig.contains("alternative")) {
      auto alternative = groupConfig.get("alternative");
      for (auto const& key : sortedObjectKeys(alternative.toObject())) {
        if (key != "chance" && key != "members")
          throw StarException(strf("Unknown planetary feature alternative property '{}' at '{}'", key, path));
      }
      double chance = planetaryFeatureChance(alternative.get("chance", 1.0), strf("{}/alternative", path));
      auto members = alternative.getObject("members", JsonObject{});
      double totalWeight = 0.0;
      for (auto const& childReference : sortedObjectKeys(members)) {
        double weight = members.get(childReference).toDouble();
        if (weight < 0.0)
          throw StarException(strf("Planetary feature alternative '{}' has negative weight for '{}'", path, childReference));
        totalWeight += weight;
        validate(childReference, strf("{}/alternative/{}", path, childReference), groupStack);
      }
      if (chance > 0.0 && totalWeight <= 0.0)
        throw StarException(strf("Planetary feature alternative '{}' has no positive-weight members", path));
    }
  }

  PlanetaryFeatureSelectionNodePtr expand(String const& reference, PlanetaryFeatureSelectionMode mode,
      String const& path, PlanetaryFeatureSelectionNode* parent, StringList groupStack) {
    auto parsed = parsePlanetaryFeatureReference(reference);
    auto node = make_shared<PlanetaryFeatureSelectionNode>();
    node->reference = reference;
    node->id = parsed.second;
    node->path = path;
    node->group = parsed.first;
    node->mode = mode;
    node->parent = parent;

    if (!node->group) {
      if (!m_featureDefinitions.contains(node->id))
        throw StarException(strf("Unknown planetary feature '{}' referenced at '{}'", node->id, path));
      return node;
    }

    if (!m_featureGroups.contains(node->id))
      throw StarException(strf("Unknown planetary feature group '{}' referenced at '{}'", node->id, path));
    if (groupStack.contains(node->id)) {
      groupStack.append(node->id);
      throw StarException(strf("Recursive planetary feature group cycle at '{}': {}", path, groupStack.join(" -> ")));
    }
    groupStack.append(node->id);

    auto groupConfig = m_featureGroups.get(node->id);
    auto guaranteed = groupConfig.getObject("guaranteed", JsonObject{});
    for (auto const& childReference : sortedObjectKeys(guaranteed)) {
      bool enabled = guaranteed.get(childReference).toBool();
      parsePlanetaryFeatureReference(childReference);
      if (enabled) {
        String childPath = strf("{}/guaranteed/{}", path, childReference);
        node->children.append(expand(childReference, PlanetaryFeatureSelectionMode::Guaranteed,
            childPath, node.get(), groupStack));
      }
    }

    if (groupConfig.contains("random")) {
      auto randomConfig = groupConfig.get("random");
      auto members = randomConfig.getObject("members", JsonObject{});
      struct SuccessfulChild {
        String reference;
        uint64_t rank;
      };
      List<SuccessfulChild> successful;
      for (auto const& childReference : sortedObjectKeys(members)) {
        parsePlanetaryFeatureReference(childReference);
        String childPath = strf("{}/random/{}", path, childReference);
        double chance = planetaryFeatureChance(members.get(childReference), childPath);
        if (staticRandomDouble(m_seed, "PlanetaryFeatureRandomChance", childPath) < chance)
          successful.append({childReference, staticRandomHash64(m_seed, "PlanetaryFeatureRandomRank", childPath)});
      }

      size_t maxCount = randomConfig.getUInt("maxCount", successful.size());
      std::sort(successful.begin(), successful.end(), [](auto const& a, auto const& b) {
        if (a.rank != b.rank)
          return a.rank < b.rank;
        return a.reference < b.reference;
      });
      if (successful.size() > maxCount)
        successful.resize(maxCount);
      std::sort(successful.begin(), successful.end(), [](auto const& a, auto const& b) {
        return a.reference < b.reference;
      });
      for (auto const& child : successful) {
        String childPath = strf("{}/random/{}", path, child.reference);
        node->children.append(expand(child.reference, PlanetaryFeatureSelectionMode::Random,
            childPath, node.get(), groupStack));
      }
    }

    if (groupConfig.contains("alternative")) {
      auto alternative = groupConfig.get("alternative");
      double chance = planetaryFeatureChance(alternative.get("chance", 1.0), strf("{}/alternative", path));
      if (staticRandomDouble(m_seed, "PlanetaryFeatureAlternativeChance", path) < chance) {
        auto members = alternative.getObject("members", JsonObject{});
        WeightedPool<String> pool;
        for (auto const& childReference : sortedObjectKeys(members)) {
          parsePlanetaryFeatureReference(childReference);
          double weight = members.get(childReference).toDouble();
          if (weight < 0.0)
            throw StarException(strf("Planetary feature alternative '{}' has negative weight for '{}'", path, childReference));
          pool.add(weight, childReference);
        }
        if (pool.empty())
          throw StarException(strf("Planetary feature alternative '{}' has no positive-weight members", path));
        String childReference = pool.select(staticRandomHash64(m_seed, "PlanetaryFeatureAlternativeSelection", path));
        String childPath = strf("{}/alternative/{}", path, childReference);
        node->children.append(expand(childReference, PlanetaryFeatureSelectionMode::Alternative,
            childPath, node.get(), groupStack));
      }
    }

    return node;
  }

  bool active(PlanetaryFeatureSelectionNode const* node) const {
    for (; node; node = node->parent) {
      if (!node->active)
        return false;
    }
    return true;
  }

  bool descendantOf(PlanetaryFeatureSelectionNode const* node, PlanetaryFeatureSelectionNode const* ancestor) const {
    for (; node; node = node->parent) {
      if (node == ancestor)
        return true;
    }
    return false;
  }

  PlanetaryFeatureSelectionNode* pruningRoot(PlanetaryFeatureSelectionNode* feature) const {
    auto node = feature;
    while (node->mode == PlanetaryFeatureSelectionMode::Guaranteed && node->parent)
      node = node->parent;
    return node;
  }

  List<PlanetaryFeatureClaim> featureClaims(PlanetaryFeatureSelectionNode* feature) const {
    List<PlanetaryFeatureClaim> claims;
    for (auto const& resource : exclusiveResources(feature->id))
      claims.append({resource, feature->id, feature});
    return claims;
  }

  void collectFeatures(PlanetaryFeatureSelectionNode* node, List<PlanetaryFeatureSelectionNode*>& features) const {
    if (!active(node))
      return;
    if (!node->group)
      features.append(node);
    for (auto const& child : node->children)
      collectFeatures(child.get(), features);
  }

  List<PlanetaryFeatureSelectionNode*> currentFeatures() const {
    List<PlanetaryFeatureSelectionNode*> features;
    for (auto const& root : m_roots)
      collectFeatures(root.get(), features);
    return features;
  }

  void collectGroups(PlanetaryFeatureSelectionNode* node, List<PlanetaryFeatureSelectionNode*>& groups) const {
    if (node->group)
      groups.append(node);
    for (auto const& child : node->children)
      collectGroups(child.get(), groups);
  }

  void validateGuaranteedSiblings() const {
    List<PlanetaryFeatureSelectionNode*> groups;
    for (auto const& root : m_roots)
      collectGroups(root.get(), groups);
    for (auto const& group : groups) {
      StringMap<String> resources;
      for (auto const& child : group->children) {
        if (child->mode != PlanetaryFeatureSelectionMode::Guaranteed || child->group)
          continue;
        for (auto const& claim : featureClaims(child.get())) {
          if (auto previous = resources.maybe(claim.resource)) {
            if (*previous != claim.featureId)
              throw StarException(strf("Planetary feature group '{}' has conflicting guaranteed siblings '{}' and '{}' for '{}'",
                  group->path, *previous, claim.featureId, claim.resource));
          } else {
            resources[claim.resource] = claim.featureId;
          }
        }
      }
    }
  }

  uint64_t conflictRank(String const& resource, PlanetaryFeatureSelectionNode const* branch) const {
    return staticRandomHash64(m_seed, "PlanetaryFeatureConflictRank", resource, branch->path);
  }

  void discard(PlanetaryFeatureSelectionNode* branch, String const& resource, String const& winner = {}) {
    if (!branch->active)
      return;
    branch->active = false;
    if (winner.empty())
      Logger::warn("Discarding planetary feature bundle '{}' due to internal conflict for '{}'", branch->path, resource);
    else
      Logger::warn("Discarding planetary feature bundle '{}' due to conflict for '{}'; '{}' won", branch->path, resource, winner);
  }

  bool resolveOceanConflict(String const& resource, List<PlanetaryFeatureClaim> const& claims) {
    StringMap<List<PlanetaryFeatureClaim>> byFeature;
    for (auto const& claim : claims)
      byFeature[claim.featureId].append(claim);
    if (byFeature.size() <= 1)
      return false;

    StringMap<StringSet> featuresByBranchPath;
    StringMap<PlanetaryFeatureSelectionNode*> branches;
    for (auto const& claim : claims) {
      auto branch = pruningRoot(claim.feature);
      branches[branch->path] = branch;
      featuresByBranchPath[branch->path].add(claim.featureId);
    }
    for (auto const& entry : featuresByBranchPath) {
      if (entry.second.size() > 1) {
        discard(branches.get(entry.first), resource);
        return true;
      }
    }

    PlanetaryFeatureSelectionNode* winner = nullptr;
    uint64_t winnerRank = 0;
    for (auto const& entry : branches) {
      uint64_t rank = conflictRank(resource, entry.second);
      if (!winner || rank < winnerRank) {
        winner = entry.second;
        winnerRank = rank;
      }
    }
    String winnerFeature;
    for (auto const& claim : claims) {
      if (pruningRoot(claim.feature) == winner) {
        winnerFeature = claim.featureId;
        break;
      }
    }
    for (auto const& entry : branches) {
      // Repeated references to the same feature produce the same ocean
      // configuration and are deduplicated when flattened.  Preserve all of
      // those bundles once that feature wins, rather than discarding an
      // otherwise non-conflicting occurrence solely because its path differs.
      if (!featuresByBranchPath.get(entry.first).contains(winnerFeature))
        discard(entry.second, resource, winner->path);
    }
    return branches.size() > 1;
  }

  bool resolveDungeonConflict(String const& layerName, List<PlanetaryFeatureSelectionNode*> const& features) {
    List<PlanetaryFeatureSelectionNode*> replacements;
    List<PlanetaryFeatureSelectionNode*> additions;
    for (auto feature : features) {
      auto layers = m_featureDefinitions.get(feature->id).getObject("layers", JsonObject{});
      if (!layers.contains(layerName))
        continue;
      auto layer = layers.get(layerName);
      if (layer.getBool("replaceDungeons", false))
        replacements.append(feature);
      if (!layer.getArray("dungeons", JsonArray{}).empty())
        additions.append(feature);
    }
    if (replacements.empty())
      return false;

    String resource = strf("dungeons:{}", layerName);
    StringMap<PlanetaryFeatureSelectionNode*> replacementBranches;
    StringMap<StringSet> replacementIdsByBranch;
    for (auto replacement : replacements) {
      auto branch = pruningRoot(replacement);
      replacementBranches[branch->path] = branch;
      replacementIdsByBranch[branch->path].add(replacement->id);
    }
    for (auto const& entry : replacementIdsByBranch) {
      if (entry.second.size() > 1) {
        discard(replacementBranches.get(entry.first), resource);
        return true;
      }
    }

    StringMap<PlanetaryFeatureSelectionNode*> candidates = replacementBranches;
    for (auto addition : additions) {
      bool insideReplacement = false;
      for (auto const& replacement : replacementBranches) {
        if (descendantOf(addition, replacement.second)) {
          insideReplacement = true;
          break;
        }
      }
      if (!insideReplacement) {
        auto branch = pruningRoot(addition);
        candidates[branch->path] = branch;
      }
    }
    if (candidates.size() <= 1)
      return false;

    PlanetaryFeatureSelectionNode* winner = nullptr;
    uint64_t winnerRank = 0;
    for (auto const& candidate : candidates) {
      uint64_t rank = conflictRank(resource, candidate.second);
      if (!winner || rank < winnerRank) {
        winner = candidate.second;
        winnerRank = rank;
      }
    }

    bool replacementWon = replacementBranches.contains(winner->path);
    if (replacementWon) {
      for (auto const& candidate : candidates) {
        if (candidate.second != winner && !descendantOf(candidate.second, winner))
          discard(candidate.second, resource, winner->path);
      }
    } else {
      for (auto const& replacement : replacementBranches)
        discard(replacement.second, resource, winner->path);
    }
    return true;
  }

  void resolveConflicts() {
    while (true) {
      auto features = currentFeatures();
      StringMap<List<PlanetaryFeatureClaim>> oceanClaims;
      StringSet dungeonLayers;
      for (auto feature : features) {
        auto layers = m_featureDefinitions.get(feature->id).getObject("layers", JsonObject{});
        for (auto const& layer : layers) {
          if (layer.second.contains("ocean"))
            oceanClaims[strf("ocean:{}", layer.first)].append({strf("ocean:{}", layer.first), feature->id, feature});
          if (layer.second.getBool("replaceDungeons", false))
            dungeonLayers.add(layer.first);
        }
      }

      bool changed = false;
      for (auto const& resource : oceanClaims.keys().sorted()) {
        if (resolveOceanConflict(resource, oceanClaims.get(resource))) {
          changed = true;
          break;
        }
      }
      if (changed)
        continue;

      for (auto const& layerName : dungeonLayers.values().sorted()) {
        if (resolveDungeonConflict(layerName, features)) {
          changed = true;
          break;
        }
      }
      if (!changed)
        break;
    }
  }

  void flatten(PlanetaryFeatureSelectionNode const* node, StringList& selected, StringSet& present) const {
    if (!active(node))
      return;
    if (!node->group && present.add(node->id))
      selected.append(node->id);
    for (auto const& child : node->children)
      flatten(child.get(), selected, present);
  }

  Json m_featureDefinitions;
  Json m_featureGroups;
  uint64_t m_seed;
  List<PlanetaryFeatureSelectionNodePtr> m_roots;
};

}

StringList selectPlanetaryFeaturesFromConfig(Json const& selectionConfig, Json const& featureDefinitions,
    uint64_t seed, Json const& featureGroups) {
  return PlanetaryFeatureSelector(featureDefinitions, featureGroups, seed).select(selectionConfig);
}

StringList selectPlanetaryFeatures(String const& typeName, String const& sizeName, uint64_t seed) {
  auto terrestrialConfig = Root::singleton().assets()->json("/terrestrial_worlds.config");
  auto config = jsonMerge(terrestrialConfig.get("planetDefaults"),
      terrestrialConfig.get("planetSizes").get(sizeName),
      terrestrialConfig.get("planetTypes").get(typeName));
  return selectPlanetaryFeaturesFromConfig(config, terrestrialConfig.get("planetaryFeatures", JsonObject{}), seed,
      terrestrialConfig.get("planetaryFeatureGroups", JsonObject{}));
}

Json environmentStatusEffectPoliciesFromConfig(Json const& planetConfig,
    Json const& featureDefinitions, StringList const& planetaryFeatures) {
  auto modeRank = [](String const& mode, String const& context) -> unsigned {
    if (mode == "global")
      return 0;
    if (mode == "layer")
      return 1;
    if (mode == "region")
      return 2;
    throw StarException(strf("Unknown environmentStatusEffectsMode '{}' at '{}'", mode, context));
  };

  struct ResolvedPolicy {
    String mode;
    unsigned rank;
    bool keepPrimary;
  };

  auto layers = planetConfig.getObject("layers", JsonObject{});
  auto layerDefaults = planetConfig.get("layerDefaults", JsonObject{});
  String planetMode = planetConfig.getString("environmentStatusEffectsMode", "global");
  modeRank(planetMode, "planet");
  bool planetKeepPrimary = planetConfig.getBool("keepPrimaryRegionStatusEffectsAlways", false);

  StringMap<ResolvedPolicy> resolved;
  for (auto const& layer : layers) {
    auto layerConfig = jsonMerge(layerDefaults, layer.second);
    String mode = layerConfig.getString("environmentStatusEffectsMode", planetMode);
    resolved[layer.first] = ResolvedPolicy{
        mode,
        modeRank(mode, strf("layers.{}", layer.first)),
        layerConfig.getBool("keepPrimaryRegionStatusEffectsAlways", planetKeepPrimary)};
  }

  for (auto const& featureName : planetaryFeatures) {
    if (!featureDefinitions.contains(featureName))
      continue;

    auto feature = featureDefinitions.get(featureName);
    auto featureMode = feature.optString("environmentStatusEffectsMode");
    Maybe<unsigned> featureModeRank;
    if (featureMode)
      featureModeRank = modeRank(*featureMode, strf("planetaryFeatures.{}", featureName));
    auto featureKeepPrimary = feature.optBool("keepPrimaryRegionStatusEffectsAlways");
    auto featureLayers = feature.getObject("layers", JsonObject{});
    for (auto const& featureLayer : featureLayers) {
      if (auto layerMode = featureLayer.second.optString("environmentStatusEffectsMode"))
        modeRank(*layerMode,
            strf("planetaryFeatures.{}.layers.{}", featureName, featureLayer.first));
      (void)featureLayer.second.optBool("keepPrimaryRegionStatusEffectsAlways");
    }

    for (auto const& layer : layers) {
      auto& policy = resolved[layer.first];
      Maybe<String> mode = featureMode;
      Maybe<unsigned> rank = featureModeRank;
      Maybe<bool> keepPrimary = featureKeepPrimary;

      if (featureLayers.contains(layer.first)) {
        auto featureLayer = featureLayers.get(layer.first);
        if (auto layerMode = featureLayer.optString("environmentStatusEffectsMode")) {
          mode = *layerMode;
          rank = modeRank(*layerMode,
              strf("planetaryFeatures.{}.layers.{}", featureName, layer.first));
        }
        if (auto layerKeepPrimary = featureLayer.optBool("keepPrimaryRegionStatusEffectsAlways"))
          keepPrimary = *layerKeepPrimary;
      }

      if (mode && *rank > policy.rank) {
        policy.mode = *mode;
        policy.rank = *rank;
      }
      if (keepPrimary && *keepPrimary)
        policy.keepPrimary = true;
    }
  }

  JsonObject result;
  for (auto const& layerName : resolved.keys().sorted()) {
    auto const& policy = resolved.get(layerName);
    result[layerName] = JsonObject{
        {"mode", policy.mode},
        {"keepPrimaryRegionStatusEffectsAlways", policy.keepPrimary}};
  }
  return result;
}

Json environmentStatusEffectPolicies(String const& typeName, String const& sizeName,
    StringList const& planetaryFeatures) {
  auto terrestrialConfig = Root::singleton().assets()->json("/terrestrial_worlds.config");
  auto config = jsonMerge(terrestrialConfig.get("planetDefaults"),
      terrestrialConfig.get("planetSizes").get(sizeName),
      terrestrialConfig.get("planetTypes").get(typeName));
  return environmentStatusEffectPoliciesFromConfig(config,
      terrestrialConfig.get("planetaryFeatures", JsonObject{}), planetaryFeatures);
}

TerrestrialWorldParametersPtr generateTerrestrialWorldParameters(
    String const& typeName, String const& sizeName, uint64_t seed, StringList const& planetaryFeatures) {
  auto& root = Root::singleton();
  auto assets = root.assets();
  auto liquidsDatabase = root.liquidsDatabase();
  auto biomeDatabase = root.biomeDatabase();

  auto terrestrialConfig = assets->json("/terrestrial_worlds.config");

  auto regionDefaults = terrestrialConfig.get("regionDefaults");
  auto regionTypes = terrestrialConfig.get("regionTypes");

  auto baseConfig = terrestrialConfig.get("planetDefaults");
  auto sizeConfig = terrestrialConfig.get("planetSizes").get(sizeName);
  auto typeConfig = terrestrialConfig.get("planetTypes").get(typeName);
  auto config = jsonMerge(baseConfig, sizeConfig, typeConfig);

  List<pair<String, Json>> featureConfigs;
  for (auto const& featureName : planetaryFeatures) {
    if (auto feature = planetaryFeatureConfig(featureName))
      featureConfigs.append({featureName, feature.take()});
    else
      Logger::warn("Planetary feature '{}' is no longer defined; ignoring its runtime generation effects", featureName);
  }

  auto appendUniqueStrings = [](JsonArray base, JsonArray const& additions) {
    StringSet present;
    for (auto const& value : base)
      present.add(value.toString());
    for (auto const& value : additions) {
      if (present.add(value.toString()))
        base.append(value);
    }
    return base;
  };

  auto featureLayerConfigs = [&featureConfigs](String const& layerName) {
    List<pair<String, Json>> layers;
    for (auto const& feature : featureConfigs) {
      auto featureLayers = feature.second.getObject("layers", JsonObject{});
      if (featureLayers.contains(layerName))
        layers.append({feature.first, featureLayers.get(layerName)});
    }
    return layers;
  };

  auto gravityRange = jsonToVec2F(config.get("gravityRange"));
  auto dayLengthRange = jsonToVec2F(config.get("dayLengthRange"));
  auto threatLevelRange = jsonToVec2F(config.query("threatRange"));

  auto threatLevel = static_cast<float>(staticRandomDouble(seed, "ThreatLevel") * (threatLevelRange[1] - threatLevelRange[0]) + threatLevelRange[0]);
  auto surfaceBiomeSeed = staticRandomU64(seed, "SurfaceBiomeSeed");

  auto readRegion = [liquidsDatabase, threatLevel, seed](Json const& regionConfig, String const& layerName, int layerBaseHeight) {
    TerrestrialWorldParameters::TerrestrialRegion region;
    auto biomeChoices = jsonToStringList(binnedChoiceFromJson(regionConfig.get("biome"), threatLevel));
    region.biome = staticRandomValueFrom(biomeChoices, seed, layerName.utf8Ptr());

    region.blockSelector = staticRandomFrom(regionConfig.getArray("blockSelector"), seed, "blockSelector", layerName.utf8Ptr()).toString();
    region.fgCaveSelector = staticRandomFrom(regionConfig.getArray("fgCaveSelector"), seed, "fgCaveSelector", layerName.utf8Ptr()).toString();
    region.bgCaveSelector = staticRandomFrom(regionConfig.getArray("bgCaveSelector"), seed, "bgCaveSelector", layerName.utf8Ptr()).toString();
    region.fgOreSelector = staticRandomFrom(regionConfig.getArray("fgOreSelector"), seed, "fgOreSelector", layerName.utf8Ptr()).toString();
    region.bgOreSelector = staticRandomFrom(regionConfig.getArray("bgOreSelector"), seed, "bgOreSelector", layerName.utf8Ptr()).toString();
    region.subBlockSelector = staticRandomFrom(regionConfig.getArray("subBlockSelector"), seed, "subBlockSelector", layerName.utf8Ptr()).toString();

    if (auto caveLiquid = staticRandomValueFrom(regionConfig.getArray("caveLiquid", {}), seed, "caveLiquid").optString()) {
      auto caveLiquidSeedDensityRange = jsonToVec2F(regionConfig.get("caveLiquidSeedDensityRange"));
      region.caveLiquid = liquidsDatabase->liquidId(*caveLiquid);
      region.caveLiquidSeedDensity = staticRandomFloatRange(caveLiquidSeedDensityRange[0],
          caveLiquidSeedDensityRange[1],
          seed,
          "caveLiquidSeedDensity",
          layerName.utf8Ptr());
    } else {
      region.caveLiquid = EmptyLiquidId;
      region.caveLiquidSeedDensity = 0.0f;
    }

    if (auto oceanLiquid = staticRandomValueFrom(regionConfig.getArray("oceanLiquid", {}), seed, "oceanLiquid", layerName.utf8Ptr()).optString()) {
      region.oceanLiquid = liquidsDatabase->liquidId(*oceanLiquid);
      region.oceanLiquidLevel = static_cast<int>(regionConfig.getInt("oceanLevelOffset", 0) + layerBaseHeight);
    } else {
      region.oceanLiquid = EmptyLiquidId;
      region.oceanLiquidLevel = 0;
    }
    region.encloseLiquids = regionConfig.getBool("encloseLiquids", false);
    region.fillMicrodungeons = regionConfig.getBool("fillMicrodungeons", false);

    return region;
  };

  auto readLayer = [readRegion, regionDefaults, regionTypes, seed, config, appendUniqueStrings, featureLayerConfigs](String const& layerName) -> Maybe<TerrestrialWorldParameters::TerrestrialLayer> {
    if (!config.get("layers").contains(layerName))
      return {};

    auto layerConfig = jsonMerge(config.get("layerDefaults"), config.get("layers").get(layerName));

    JsonArray primaryRegionList = layerConfig.getArray("primaryRegion", JsonArray{});
    JsonArray secondaryRegionList = layerConfig.getArray("secondaryRegions", JsonArray{});
    int secondaryRegionCountAdd = 0;
    JsonArray addedSubRegions;
    Json oceanConfig = JsonObject{};
    auto featureLayers = featureLayerConfigs(layerName);
    for (auto const& namedFeatureLayer : featureLayers) {
      auto const& featureLayer = namedFeatureLayer.second;
      primaryRegionList = appendUniqueStrings(std::move(primaryRegionList), featureLayer.getArray("primaryRegions", JsonArray{}));
      secondaryRegionList = appendUniqueStrings(std::move(secondaryRegionList), featureLayer.getArray("secondaryRegions", JsonArray{}));
      addedSubRegions = appendUniqueStrings(std::move(addedSubRegions), featureLayer.getArray("subRegions", JsonArray{}));
      int countAdd = featureLayer.getInt("secondaryRegionCountAdd", 0);
      if (countAdd < 0)
        throw StarException("secondaryRegionCountAdd must be nonnegative");
      secondaryRegionCountAdd += countAdd;
      if (featureLayer.contains("ocean"))
        oceanConfig = jsonMerge(oceanConfig, featureLayer.get("ocean"));
    }

    if (!layerConfig || !layerConfig.getBool("enabled"))
      return {};

    TerrestrialWorldParameters::TerrestrialLayer layer;

    layer.layerMinHeight = static_cast<int>(layerConfig.getFloat("layerLevel"));
    layer.layerBaseHeight = static_cast<int>(layerConfig.getFloat("baseHeight"));

    auto primaryRegionConfigName = staticRandomFrom(primaryRegionList, seed, layerName.utf8Ptr(), "PrimaryRegionSelection").toString();
    Json primaryRegionConfig = jsonMerge(regionDefaults, regionTypes.get(primaryRegionConfigName));
    primaryRegionConfig = primaryRegionConfig.set("subRegion",
        appendUniqueStrings(primaryRegionConfig.getArray("subRegion", JsonArray{}), addedSubRegions));
    primaryRegionConfig = jsonMerge(primaryRegionConfig, oceanConfig);
    layer.primaryRegion = readRegion(primaryRegionConfig, layerName, layer.layerBaseHeight);

    {
      auto subRegionList = primaryRegionConfig.getArray("subRegion");
      Json subRegionConfig;
      if (!subRegionList.empty()) {
        String subRegionName = staticRandomFrom(subRegionList, seed, layerName, primaryRegionConfigName).toString();
        subRegionConfig = jsonMerge(regionDefaults, regionTypes.get(subRegionName));
      } else {
        subRegionConfig = primaryRegionConfig;
      }
      subRegionConfig = jsonMerge(subRegionConfig, oceanConfig);
      layer.primarySubRegion = readRegion(subRegionConfig, layerName, layer.layerBaseHeight);
    }

    Vec2U secondaryRegionCountRange = jsonToVec2U(layerConfig.get("secondaryRegionCount"));
    secondaryRegionCountRange[0] += secondaryRegionCountAdd;
    secondaryRegionCountRange[1] += secondaryRegionCountAdd;
    int secondaryRegionCount = staticRandomI32Range(static_cast<int>(secondaryRegionCountRange[0]), static_cast<int>(secondaryRegionCountRange[1]), seed, layerName, "SecondaryRegionCount");
    if (!secondaryRegionList.empty()) {
      staticRandomShuffle(secondaryRegionList, seed, layerName, "SecondaryRegionShuffle");
      for (const auto& regionName : secondaryRegionList) {
        if (secondaryRegionCount <= 0)
          break;
        Json secondaryRegionConfig = jsonMerge(regionDefaults, regionTypes.get(regionName.toString()));
        secondaryRegionConfig = secondaryRegionConfig.set("subRegion",
            appendUniqueStrings(secondaryRegionConfig.getArray("subRegion", JsonArray{}), addedSubRegions));
        secondaryRegionConfig = jsonMerge(secondaryRegionConfig, oceanConfig);
        layer.secondaryRegions.append(readRegion(secondaryRegionConfig, layerName, layer.layerBaseHeight));

        auto subRegionList = secondaryRegionConfig.getArray("subRegion");
        Json subRegionConfig;
        if (!subRegionList.empty()) {
          String subRegionName = staticRandomFrom(subRegionList, seed, layerName, regionName.toString()).toString();
          subRegionConfig = jsonMerge(regionDefaults, regionTypes.get(subRegionName));
        } else {
          subRegionConfig = secondaryRegionConfig;
        }
        subRegionConfig = jsonMerge(subRegionConfig, oceanConfig);
        layer.secondarySubRegions.append(readRegion(subRegionConfig, layerName, layer.layerBaseHeight));

        --secondaryRegionCount;
      }
    }

    layer.secondaryRegionSizeRange = jsonToVec2F(layerConfig.get("secondaryRegionSize"));
    layer.subRegionSizeRange = jsonToVec2F(layerConfig.get("subRegionSize"));

    WeightedPool<String> dungeonPool = jsonToWeightedPool<String>(layerConfig.get("dungeons"));
    Vec2U dungeonCountRange = layerConfig.opt("dungeonCountRange").apply(jsonToVec2U).value();
    unsigned dungeonCount = staticRandomU32Range(dungeonCountRange[0], dungeonCountRange[1], seed, layerName, "DungeonCount");
    layer.dungeons.appendAll(dungeonPool.selectUniques(dungeonCount, staticRandomHash64(seed, layerName, "DungeonChoice")));
    bool replacementApplied = false;
    for (auto const& namedFeatureLayer : featureLayers) {
      if (namedFeatureLayer.second.getBool("replaceDungeons", false)) {
        if (!replacementApplied) {
          applyPlanetaryFeatureDungeons(layer.dungeons, namedFeatureLayer.second, seed, namedFeatureLayer.first, layerName);
          replacementApplied = true;
        } else {
          Logger::warn("Ignoring additional dungeon replacement from planetary feature '{}' in layer '{}'",
              namedFeatureLayer.first, layerName);
        }
      }
    }
    for (auto const& namedFeatureLayer : featureLayers) {
      if (!namedFeatureLayer.second.getBool("replaceDungeons", false))
        applyPlanetaryFeatureDungeons(layer.dungeons, namedFeatureLayer.second, seed, namedFeatureLayer.first, layerName);
    }
    layer.dungeonXVariance = static_cast<int>(layerConfig.getInt("dungeonXVariance", 0));

    return layer;
  };

  auto surfaceLayer = readLayer("surface").take();
  String primaryBiome = surfaceLayer.primaryRegion.biome;

  auto parameters = make_shared<TerrestrialWorldParameters>();

  parameters->threatLevel = threatLevel;
  parameters->typeName = typeName;
  parameters->worldSize = jsonToVec2U(config.get("size"));
  parameters->gravity = staticRandomFloatRange(gravityRange[0], gravityRange[1], seed, "WorldGravity");
  parameters->airless = biomeDatabase->biomeIsAirless(primaryBiome);
  parameters->environmentStatusEffects = biomeDatabase->biomeStatusEffects(primaryBiome);
  parameters->overrideTech = config.opt("overrideTech").apply(jsonToStringList);
  parameters->globalDirectives = config.opt("globalDirectives").apply(jsonToDirectivesList);
  parameters->beamUpRule = BeamUpRuleNames.getLeft(config.getString("beamUpRule", "Surface"));
  parameters->disableDeathDrops = config.getBool("disableDeathDrops", false);
  parameters->worldEdgeForceRegions = WorldEdgeForceRegionTypeNames.getLeft(config.getString("worldEdgeForceRegions", "Top"));

  parameters->weatherPool = biomeDatabase->biomeWeathers(primaryBiome, seed, threatLevel);

  parameters->primaryBiome = primaryBiome;
  parameters->sizeName = sizeName;
  parameters->hueShift = biomeDatabase->biomeHueShift(parameters->primaryBiome, surfaceBiomeSeed);

  parameters->primarySurfaceLiquid = surfaceLayer.primaryRegion.oceanLiquid != EmptyLiquidId
      ? surfaceLayer.primaryRegion.oceanLiquid
      : surfaceLayer.primaryRegion.caveLiquid;

  parameters->skyColoring = biomeDatabase->biomeSkyColoring(parameters->primaryBiome, seed);
  parameters->dayLength = staticRandomFloatRange(dayLengthRange[0], dayLengthRange[1], seed, "DayLength");

  parameters->blockNoiseConfig = config.get("blockNoise");
  parameters->blendNoiseConfig = config.get("blendNoise");
  parameters->blendSize = config.getFloat("blendSize");

  parameters->spaceLayer = readLayer("space").take();
  parameters->atmosphereLayer = readLayer("atmosphere").take();
  parameters->surfaceLayer = surfaceLayer;
  parameters->subsurfaceLayer = readLayer("subsurface").take();

  while (auto undergroundLayer = readLayer(strf("underground{}", parameters->undergroundLayers.size() + 1)))
    parameters->undergroundLayers.append(undergroundLayer.take());

  parameters->coreLayer = readLayer("core").take();

  return parameters;
}

AsteroidsWorldParametersPtr generateAsteroidsWorldParameters(uint64_t seed) {
  auto& root = Root::singleton();
  auto assets = root.assets();

  auto parameters = make_shared<AsteroidsWorldParameters>();

  auto asteroidsConfig = assets->json("/asteroids_worlds.config");
  String biome = asteroidsConfig.getString("biome");
  auto gravityRange = jsonToVec2F(asteroidsConfig.get("gravityRange"));

  auto threatLevelRange = jsonToVec2F(asteroidsConfig.get("threatRange"));
  parameters->threatLevel = static_cast<float>(staticRandomDouble(seed, "ThreatLevel") * (threatLevelRange[1] - threatLevelRange[0]) + threatLevelRange[0]);
  parameters->typeName = "asteroids";
  parameters->worldSize = jsonToVec2U(asteroidsConfig.get("worldSize"));
  parameters->gravity = staticRandomFloatRange(gravityRange[0], gravityRange[1], seed, "WorldGravity");
  parameters->environmentStatusEffects = jsonToStringList(asteroidsConfig.getArray("environmentStatusEffects", JsonArray()));
  parameters->overrideTech = asteroidsConfig.opt("overrideTech").apply(jsonToStringList);
  parameters->globalDirectives = asteroidsConfig.opt("globalDirectives").apply(jsonToDirectivesList);
  parameters->beamUpRule = BeamUpRuleNames.getLeft(asteroidsConfig.getString("beamUpRule", "Surface"));
  parameters->disableDeathDrops = asteroidsConfig.getBool("disableDeathDrops", false);
  parameters->worldEdgeForceRegions = WorldEdgeForceRegionTypeNames.getLeft(asteroidsConfig.getString("worldEdgeForceRegions", "TopAndBottom"));

  parameters->asteroidTopLevel = static_cast<int>(asteroidsConfig.getInt("asteroidsTop"));
  parameters->asteroidBottomLevel = static_cast<int>(asteroidsConfig.getInt("asteroidsBottom"));
  parameters->blendSize = asteroidsConfig.getFloat("blendSize");
  parameters->asteroidBiome = biome;
  parameters->ambientLightLevel = jsonToColor(asteroidsConfig.get("ambientLightLevel"));

  return parameters;
}

FloatingDungeonWorldParametersPtr generateFloatingDungeonWorldParameters(String const& dungeonWorldName) {
  auto& root = Root::singleton();
  auto assets = root.assets();

  auto worldConfig = assets->json("/dungeon_worlds.config:" + dungeonWorldName);

  auto parameters = make_shared<FloatingDungeonWorldParameters>();

  parameters->threatLevel = worldConfig.getFloat("threatLevel", 0);
  parameters->typeName = dungeonWorldName;
  parameters->worldSize = jsonToVec2U(worldConfig.get("worldSize"));
  parameters->gravity = worldConfig.getFloat("gravity");
  parameters->airless = worldConfig.getBool("airless", false);
  parameters->environmentStatusEffects = jsonToStringList(worldConfig.getArray("environmentStatusEffects", JsonArray()));
  parameters->overrideTech = worldConfig.opt("overrideTech").apply(jsonToStringList);
  parameters->globalDirectives = worldConfig.opt("globalDirectives").apply(jsonToDirectivesList);
  if (auto weatherPoolConfig = worldConfig.optArray("weatherPool"))
    parameters->weatherPool = jsonToWeightedPool<String>(*weatherPoolConfig);
  parameters->beamUpRule = BeamUpRuleNames.getLeft(worldConfig.getString("beamUpRule", "Surface"));
  parameters->disableDeathDrops = worldConfig.getBool("disableDeathDrops", false);
  parameters->worldEdgeForceRegions = WorldEdgeForceRegionTypeNames.getLeft(worldConfig.getString("worldEdgeForceRegions", "Top"));

  parameters->dungeonBaseHeight = static_cast<int>(worldConfig.getInt("dungeonBaseHeight"));
  parameters->dungeonSurfaceHeight = static_cast<int>(worldConfig.getInt("dungeonSurfaceHeight", parameters->dungeonBaseHeight));
  parameters->dungeonUndergroundLevel = static_cast<int>(worldConfig.getInt("dungeonUndergroundLevel", 0));
  parameters->primaryDungeon = worldConfig.getString("primaryDungeon");
  parameters->biome = worldConfig.optString("biome");
  parameters->ambientLightLevel = jsonToColor(worldConfig.get("ambientLightLevel"));
  if (worldConfig.contains("musicTrack")) {
    parameters->dayMusicTrack = worldConfig.optString("musicTrack");
    parameters->nightMusicTrack = worldConfig.optString("musicTrack");
  } else {
    parameters->dayMusicTrack = worldConfig.optString("dayMusicTrack");
    parameters->nightMusicTrack = worldConfig.optString("nightMusicTrack");
  }
  if (worldConfig.contains("ambientNoises")) {
    parameters->dayAmbientNoises = worldConfig.optString("ambientNoises");
    parameters->nightAmbientNoises = worldConfig.optString("ambientNoises");
  } else {
    parameters->dayAmbientNoises = worldConfig.optString("dayAmbientNoises");
    parameters->nightAmbientNoises = worldConfig.optString("nightAmbientNoises");
  }

  return parameters;
}

}
