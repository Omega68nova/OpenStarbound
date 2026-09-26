#include "StarCelestialGraphics.hpp"
#include "StarAlgorithm.hpp"
#include "StarJsonExtra.hpp"
#include "StarLexicalCast.hpp"
#include "StarFormat.hpp"
#include "StarImageProcessing.hpp"
#include "StarCelestialDatabase.hpp"
#include "StarParallax.hpp"
#include "StarRoot.hpp"
#include "StarBiomeDatabase.hpp"
#include "StarTerrainDatabase.hpp"
#include "StarLiquidsDatabase.hpp"
#include "StarAssets.hpp"

namespace Star {

namespace {
  List<Json> planetaryFeatureGraphics(CelestialParameters const& parameters) {
    List<Json> graphics;
    for (auto const& featureName : jsonToStringList(parameters.getParameter("planetaryFeatures", JsonArray{}))) {
      if (auto feature = planetaryFeatureConfig(featureName))
        graphics.append(feature->get("graphics", JsonObject{}));
    }
    return graphics;
  }
}

List<pair<String, float>> CelestialGraphics::drawSystemPlanetaryObject(CelestialParameters const& parameters) {
  return {{parameters.getParameter("smallImage").toString(), parameters.getParameter("smallImageScale").toFloat()}};
}

List<pair<String, float>> CelestialGraphics::drawSystemCentralBody(CelestialParameters const& parameters) {
  return {{parameters.getParameter("image").toString(), parameters.getParameter("imageScale").toFloat()}};
}

List<pair<String, float>> CelestialGraphics::drawWorld(
    CelestialParameters const& celestialParameters, Maybe<CelestialParameters> const& overrideShadowParameters) {
  auto& root = Root::singleton();
  auto assets = root.assets();
  auto liquidsDatabase = root.liquidsDatabase();

  CelestialParameters shadowParameters = overrideShadowParameters.value(celestialParameters);

  String type = celestialParameters.getParameter("worldType").toString();

  List<pair<String, float>> layers;

  if (type == "Terrestrial") {
    auto terrestrialParameters = as<TerrestrialWorldParameters>(celestialParameters.visitableParameters());
    if (!terrestrialParameters)
      return {};

    auto terrestrialGraphics = assets->json("/celestial.config:terrestrialGraphics");
    auto gfxConfig = jsonMerge(terrestrialGraphics.get("default"),
        terrestrialGraphics.get(terrestrialParameters->typeName, JsonObject()));
    auto featureGraphics = planetaryFeatureGraphics(celestialParameters);
    float imageScale = celestialParameters.getParameter("imageScale", 1.0f).toFloat();

    List<pair<int, pair<String, float>>> orderedLayers;
    auto appendWorldLayer = [&](int zLevel, String image) {
      orderedLayers.append({zLevel, {std::move(image), imageScale}});
    };

    auto appendGraphicsBody = [&](Json const& graphicsConfig, int zLevel) {
      auto liquidImages = graphicsConfig.getString("liquidImages", "");
      auto baseImages = graphicsConfig.getString("baseImages", "");
      auto baseCount = graphicsConfig.getInt("baseCount", 0);
      auto dynamicsImages = graphicsConfig.getString("dynamicsImages", "");

      // If the planet has water, then draw the corresponding water image as the
      // base layer, otherwise use the bottom most mask image.
      if (graphicsConfig.getBool("baseLayer", true)) {
        if (terrestrialParameters->primarySurfaceLiquid != EmptyLiquidId && !liquidImages.empty()) {
          String liquidBaseImage = liquidImages.replace("<liquid>", liquidsDatabase->liquidName(terrestrialParameters->primarySurfaceLiquid));
          appendWorldLayer(zLevel, std::move(liquidBaseImage));
        } else if (baseCount > 0) {
          String baseLayer = strf("{}?hueshift={}", baseImages.replace("<biome>",
              terrestrialParameters->primaryBiome).replace("<num>", toString(baseCount)), terrestrialParameters->hueShift);
          appendWorldLayer(zLevel, std::move(baseLayer));
        }
      }

      // Then draw all the biome layers.
      for (int i = 0; i < baseCount; ++i) {
        String baseImage = baseImages.replace("<num>", toString(baseCount - i));
        String hueShiftString, dynamicMaskString;
        if (!dynamicsImages.empty())
          dynamicMaskString = "?addmask=" + dynamicsImages.replace("<num>", toString(celestialParameters.randomizeParameterRange(graphicsConfig.getArray("dynamicsRange"), i).toInt()));
        if (terrestrialParameters->hueShift != 0)
          hueShiftString = strf("?hueshift={}", terrestrialParameters->hueShift);
        String layer = baseImage + hueShiftString + dynamicMaskString;
        appendWorldLayer(zLevel, std::move(layer));
      }
    };

    auto appendGraphicsShadow = [&](Json const& graphicsConfig, int zLevel) {
      auto shadowImages = graphicsConfig.getString("shadowImages", "");
      if (!shadowImages.empty()) {
        String shadow = shadowImages.replace("<num>", toString(shadowParameters.randomizeParameterRange(graphicsConfig.getArray("shadowNumber")).toInt()));
        appendWorldLayer(zLevel, std::move(shadow));
      }
    };

    // Canonical planet stages leave room for features behind the planet, over
    // its body but under atmospheric effects, and over the complete planet.
    appendGraphicsBody(gfxConfig, 100);
    appendGraphicsShadow(gfxConfig, 300);

    for (auto const& graphics : featureGraphics) {
      for (auto const& image : graphics.getArray("worldBack", JsonArray{}))
        appendWorldLayer(0, image.toString());

      auto legacyGraphicsName = graphics.optString("terrestrialGraphicsOverlay");
      if (!legacyGraphicsName)
        legacyGraphicsName = graphics.optString("terrestrialGraphics");
      if (legacyGraphicsName) {
        auto featureConfig = jsonMerge(terrestrialGraphics.get("default"), terrestrialGraphics.get(*legacyGraphicsName));
        appendGraphicsBody(featureConfig, 400);
        appendGraphicsShadow(featureConfig, 400);
      }

      for (auto const& layer : graphics.getArray("worldLayers", JsonArray{})) {
        int zLevel = layer.getInt("zLevel", 200);
        for (auto const& image : layer.getArray("images", JsonArray{}))
          appendWorldLayer(zLevel, image.toString());

        if (auto graphicsName = layer.optString("terrestrialGraphics")) {
          auto featureConfig = jsonMerge(terrestrialGraphics.get("default"), terrestrialGraphics.get(*graphicsName));
          // Layer fields may refine a named graphics entry, allowing one image
          // set to be reused with different masks or composition semantics.
          featureConfig = jsonMerge(featureConfig, layer);
          auto parts = jsonToStringList(layer.get("parts", JsonArray{"body", "effects"}));
          if (parts.contains("body"))
            appendGraphicsBody(featureConfig, zLevel);
          if (parts.contains("effects"))
            appendGraphicsShadow(featureConfig, zLevel);
        }
      }

      for (auto const& image : graphics.getArray("worldFront", JsonArray{}))
        appendWorldLayer(400, image.toString());
    }

    stableSort(orderedLayers, [](auto const& left, auto const& right) {
      return left.first < right.first;
    });
    for (auto& layer : orderedLayers)
      layers.append(std::move(layer.second));

  } else if (type == "Asteroids") {
    String maskImages = celestialParameters.getParameter("maskImages").toString();
    int maskCount = celestialParameters.getParameter("masks").toInt();
    String dynamicsImages = celestialParameters.getParameter("dynamicsImages").toString();
    float imageScale = celestialParameters.getParameter("imageScale", 1.0f).toFloat();

    for (int i = 0; i < maskCount; ++i) {
      String biomeMaskBase = maskImages.replace("<num>", toString(maskCount - i));
      String dynamicMask = dynamicsImages.replace("<num>", toString(celestialParameters.randomizeParameterRange("dynamicsRange", i).toInt()));
      String layer = strf("{}?addmask={}", biomeMaskBase, dynamicMask);
      layers.append({std::move(layer), imageScale});
    }

  } else if (type == "FloatingDungeon") {
    String image = celestialParameters.getParameter("image").toString();
    float imageScale = celestialParameters.getParameter("imageScale", 1.0f).toFloat();
    layers.append({std::move(image), imageScale});

    if (!celestialParameters.getParameter("dynamicsImages").toString().empty()) {
      String dynamicsImages = celestialParameters.getParameter("dynamicsImages", "").toString();
      String dynamicsImage = dynamicsImages.replace("<num>", toString(celestialParameters.randomizeParameterRange("dynamicsRange").toInt()));
      layers.append({std::move(dynamicsImage), imageScale});
    }

  } else if (type == "GasGiant") {
    auto gfxConfig = assets->json("/celestial.config:gasGiantGraphics");

    auto baseImage = gfxConfig.getString("baseImage", "");
    auto shadowImages = gfxConfig.getString("shadowImages", "");
    auto dynamicsImages = gfxConfig.getString("dynamicsImages", "");
    auto overlayImages = gfxConfig.getString("overlayImages", "");
    auto overlayCount = gfxConfig.getInt("overlayCount", 0);
    float imageScale = celestialParameters.getParameter("imageScale", 1.0f).toFloat();

    float hueShift = celestialParameters.randomizeParameterRange(gfxConfig.getArray("primaryHueShiftRange")).toFloat();
    if (!baseImage.empty())
      layers.append({strf("{}?hueshift={}", baseImage, hueShift), imageScale});

    if (!overlayImages.empty()) {
      for (int i = 0; i < overlayCount; ++i) {
        hueShift += celestialParameters.randomizeParameterRange(gfxConfig.getArray("hueShiftOffsetRange")).toFloat();
        String maskImage = dynamicsImages.replace("<num>", toString(celestialParameters.randomizeParameterRange(gfxConfig.getArray("dynamicsRange"), i).toInt()));
        String overlayImage = overlayImages.replace("<num>", toString(i));
        layers.append({strf("{}?hueshift={}?addmask={}", overlayImage, hueShift, maskImage), imageScale});
      }
    }

    if (!shadowImages.empty()) {
      String shadow = shadowImages.replace("<num>", toString(shadowParameters.randomizeParameterRange(gfxConfig.getArray("shadowNumber")).toInt()));
      layers.append({std::move(shadow), imageScale});
    }
  }

  return layers;
}

List<pair<String, String>> CelestialGraphics::worldHorizonImages(CelestialParameters const& celestialParameters) {
  auto& root = Root::singleton();
  auto assets = root.assets();
  auto liquidsDatabase = root.liquidsDatabase();

  auto getLR = [](String const& base) -> pair<String, String> {
    return pair<String, String>(base.replace("<selector>", "l"), base.replace("<selector>", "r"));
  };

  String type = celestialParameters.getParameter("worldType").toString();

  List<pair<String, String>> res;

  if (type == "Terrestrial") {
    auto terrestrialParameters = as<TerrestrialWorldParameters>(celestialParameters.visitableParameters());
    if (!terrestrialParameters)
      return {};

    auto terrestrialHorizonGraphics = assets->json("/celestial.config:terrestrialHorizonGraphics");
    auto gfxConfig = jsonMerge(terrestrialHorizonGraphics.get("default"),
        terrestrialHorizonGraphics.get(terrestrialParameters->typeName, JsonObject()));
    auto featureGraphics = planetaryFeatureGraphics(celestialParameters);

    auto biomeHueShift = "?" + imageOperationToString(HueShiftImageOperation::hueShiftDegrees(terrestrialParameters->hueShift));

    List<pair<int, pair<String, String>>> orderedLayers;
    auto appendHorizonLayer = [&](int zLevel, pair<String, String> images) {
      orderedLayers.append({zLevel, std::move(images)});
    };

    auto appendHorizonBody = [&](Json const& graphicsConfig, int zLevel) {
      String baseImages = graphicsConfig.getString("baseImages");
      String maskTextures = graphicsConfig.getString("maskTextures");
      String liquidTextures = graphicsConfig.getString("liquidTextures");
      auto numMasks = jsonToVec2I(graphicsConfig.get("maskRange"));
      auto maskPerPlanetRange = jsonToVec2I(graphicsConfig.get("maskPerPlanetRange"));

      if (terrestrialParameters->primarySurfaceLiquid != EmptyLiquidId) {
        RandomSource rand(celestialParameters.seed());

        int numPlanetMasks = rand.randInt(maskPerPlanetRange[0], maskPerPlanetRange[1]);
        List<int> masks;
        for (int i = 0; i < numPlanetMasks; ++i)
          masks.append(rand.randInt(numMasks[0], numMasks[1]));

        String liquidBase = liquidTextures.replace("<liquid>", liquidsDatabase->liquidName(terrestrialParameters->primarySurfaceLiquid));
        appendHorizonLayer(zLevel, getLR(liquidBase));

        StringList planetMaskListL;
        StringList planetMaskListR;
        for (auto m : masks) {
          String base = maskTextures.replace("<mask>", toString(m));
          auto lr = getLR(base);
          planetMaskListL.append(lr.first);
          planetMaskListR.append(lr.second);
        }

        String leftMask, rightMask;
        if (!planetMaskListL.empty())
          leftMask = "?" + imageOperationToString(AlphaMaskImageOperation{AlphaMaskImageOperation::Additive, planetMaskListL, {0, 0}});
        if (!planetMaskListR.empty())
          rightMask = "?" + imageOperationToString(AlphaMaskImageOperation{AlphaMaskImageOperation::Additive, planetMaskListR, {0, 0}});

        auto toAppend = getLR(baseImages + biomeHueShift);
        appendHorizonLayer(zLevel, {toAppend.first + leftMask, toAppend.second + rightMask});
      } else {
        appendHorizonLayer(zLevel, getLR(baseImages + biomeHueShift));
      }
    };

    auto appendHorizonEffects = [&](Json const& graphicsConfig, int zLevel) {
      if (celestialParameters.getParameter("atmosphere", true).toBool())
        appendHorizonLayer(zLevel, getLR(graphicsConfig.getString("atmosphereTextures")));
      appendHorizonLayer(zLevel, getLR(graphicsConfig.getString("shadowTextures")));
    };

    appendHorizonBody(gfxConfig, 100);
    appendHorizonEffects(gfxConfig, 300);

    for (auto const& graphics : featureGraphics) {
      for (auto const& imagePair : graphics.getArray("horizonBack", JsonArray{})) {
        auto pair = imagePair.toArray();
        appendHorizonLayer(0, {pair.get(0).toString(), pair.get(1).toString()});
      }

      auto legacyGraphicsName = graphics.optString("terrestrialHorizonGraphicsOverlay");
      if (!legacyGraphicsName)
        legacyGraphicsName = graphics.optString("terrestrialHorizonGraphics");
      if (legacyGraphicsName) {
        auto featureConfig = jsonMerge(terrestrialHorizonGraphics.get("default"), terrestrialHorizonGraphics.get(*legacyGraphicsName));
        appendHorizonBody(featureConfig, 400);
        appendHorizonEffects(featureConfig, 400);
      }

      for (auto const& layer : graphics.getArray("horizonLayers", JsonArray{})) {
        int zLevel = layer.getInt("zLevel", 200);
        for (auto const& imagePair : layer.getArray("images", JsonArray{})) {
          auto pair = imagePair.toArray();
          appendHorizonLayer(zLevel, {pair.get(0).toString(), pair.get(1).toString()});
        }

        if (auto graphicsName = layer.optString("terrestrialHorizonGraphics")) {
          auto featureConfig = jsonMerge(terrestrialHorizonGraphics.get("default"), terrestrialHorizonGraphics.get(*graphicsName));
          auto parts = jsonToStringList(layer.get("parts", JsonArray{"body", "effects"}));
          if (parts.contains("body"))
            appendHorizonBody(featureConfig, zLevel);
          if (parts.contains("effects"))
            appendHorizonEffects(featureConfig, zLevel);
        }
      }

      for (auto const& imagePair : graphics.getArray("horizonFront", JsonArray{})) {
        auto pair = imagePair.toArray();
        appendHorizonLayer(400, {pair.get(0).toString(), pair.get(1).toString()});
      }
    }

    stableSort(orderedLayers, [](auto const& left, auto const& right) {
      return left.first < right.first;
    });
    bool frontLayers = false;
    for (auto& layer : orderedLayers) {
      if (!frontLayers && layer.first > 300) {
        // An empty pair is a serialized-compatible separator between the
        // normal horizon pass and layers drawn above orbital clouds.
        res.append({"", ""});
        frontLayers = true;
      }
      res.append(std::move(layer.second));
    }

  } else if (type == "Asteroids") {
    res.append(getLR(assets->json("/celestial.config:asteroidsHorizons").toString()));

  } else if (type == "FloatingDungeon") {
    auto dungeonParameters = as<FloatingDungeonWorldParameters>(celestialParameters.visitableParameters());
    auto dungeonHorizons = assets->json("/celestial.config:floatingDungeonHorizons");
    if (dungeonHorizons.contains(dungeonParameters->primaryDungeon))
      res.append(getLR(dungeonHorizons.get(dungeonParameters->primaryDungeon).toString()));
  }

  return res;
}

int CelestialGraphics::worldRadialPosition(CelestialParameters const& parameters) {
  if (parameters.coordinate().isPlanetaryBody())
    return staticRandomU32(parameters.seed(), "RadialNumber") % planetRadialPositions();
  if (parameters.coordinate().isSatelliteBody())
    return staticRandomU32(parameters.seed(), "RadialNumber") % satelliteRadialPositions();
  return 0;
}

int CelestialGraphics::planetRadialPositions() {
  return Root::singleton().assets()->json("/celestial.config:planetRadialSlots").toInt();
}

int CelestialGraphics::satelliteRadialPositions() {
  return Root::singleton().assets()->json("/celestial.config:satelliteRadialSlots").toInt();
}

List<pair<String, float>> CelestialGraphics::drawSystemTwinkle(CelestialDatabasePtr celestialDatabase, CelestialCoordinate const& system, double time) {
  auto parameters = celestialDatabase->parameters(system);
  if (!parameters)
    return {};

  auto assets = Root::singleton().assets();

  int twinkleFrameCount = assets->json("/celestial.config:twinkleFrames").toInt();
  float twinkleScale = assets->json("/celestial.config:twinkleScale").toFloat();
  String twinkleFrameset = parameters->getParameter("twinkleFrames").toString();
  float twinkleTime = parameters->randomizeParameterRange("twinkleTime").toFloat();
  String twinkleBackground = parameters->getParameter("twinkleBackground").toString();

  String twinkleFrame = strf("{}:{}", twinkleFrameset, (int)(std::fmod<double>(time / twinkleTime, 1.0f) * twinkleFrameCount));

  return {{std::move(twinkleBackground), 1.0f}, {std::move(twinkleFrame), twinkleScale}};
}

List<pair<String, float>> CelestialGraphics::drawSystemPlanetaryObject(CelestialDatabasePtr celestialDatabase, CelestialCoordinate const& coordinate) {
  if (auto params = celestialDatabase->parameters(coordinate))
    return drawSystemPlanetaryObject(params.take());
  return {};
}

List<pair<String, float>> CelestialGraphics::drawSystemCentralBody(CelestialDatabasePtr celestialDatabase, CelestialCoordinate const& coordinate) {
  if (auto params = celestialDatabase->parameters(coordinate))
    return drawSystemCentralBody(params.take());
  return {};
}

List<pair<String, float>> CelestialGraphics::drawWorld(CelestialDatabasePtr celestialDatabase, CelestialCoordinate const& coordinate) {
  auto params = celestialDatabase->parameters(coordinate);
  if (!params)
    return {};

  if (coordinate.isSatelliteBody())
    return drawWorld(params.take(), celestialDatabase->parameters(coordinate.parent()));
  else
    return drawWorld(params.take());
}

List<pair<String, String>> CelestialGraphics::worldHorizonImages(CelestialDatabasePtr celestialDatabase, CelestialCoordinate const& coordinate) {
  if (auto params = celestialDatabase->parameters(coordinate))
    return worldHorizonImages(params.take());
  return {};
}

int CelestialGraphics::worldRadialPosition(CelestialDatabasePtr celestialDatabase, CelestialCoordinate const& coordinate) {
  if (auto params = celestialDatabase->parameters(coordinate))
    return worldRadialPosition(params.take());
  return 0;
}

}
