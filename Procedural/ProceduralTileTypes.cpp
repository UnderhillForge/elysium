#include "Procedural/ProceduralTileTypes.hpp"

#include <json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>

namespace elysium {

namespace {

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

TileTypeProperties makeType(TileType type,
                            const char* material,
                            const glm::vec4& tint,
                            bool walkable,
                            float rotJitter,
                            float scaleJitter,
                            float detailDensity,
                            const EdgeDisplacementProfile& edge)
{
    TileTypeProperties props{};
    props.type = type;
    props.materialName = material;
    props.baseTint = tint;
    props.walkable = walkable;
    props.maxRotationJitterDeg = rotJitter;
    props.maxUniformScaleJitter = scaleJitter;
    props.detailDensity = detailDensity;
    props.edge = edge;
    return props;
}

} // namespace

const char* tileTypeName(TileType type) {
    switch (type) {
        case TileType::Grass: return "grass";
        case TileType::Cobblestone: return "cobblestone";
        case TileType::Sand: return "sand";
        case TileType::Rocky: return "rocky";
        case TileType::Beach: return "beach";
        case TileType::Snow: return "snow";
        case TileType::Swamp: return "swamp";
        case TileType::Water: return "water";
        case TileType::Unknown: break;
    }
    return "unknown";
}

TileType tileTypeFromString(const std::string& value) {
    const std::string k = lowerCopy(value);
    if (k == "grass") return TileType::Grass;
    if (k == "cobblestone") return TileType::Cobblestone;
    if (k == "sand") return TileType::Sand;
    if (k == "rocky") return TileType::Rocky;
    if (k == "beach") return TileType::Beach;
    if (k == "snow") return TileType::Snow;
    if (k == "swamp") return TileType::Swamp;
    if (k == "water") return TileType::Water;
    return TileType::Unknown;
}

ProceduralTileConfig ProceduralTileConfig::defaults() {
    ProceduralTileConfig cfg{};

    EdgeDisplacementProfile soft{};
    soft.lowFrequency = 0.75f;
    soft.highFrequency = 4.0f;
    soft.lowAmplitude = 0.090f;
    soft.highAmplitude = 0.035f;
    soft.verticalAmplitude = 0.015f;
    soft.edgeFalloff = 1.6f;
    soft.transitionBoost = 0.45f;

    EdgeDisplacementProfile jagged{};
    jagged.lowFrequency = 1.20f;
    jagged.highFrequency = 7.40f;
    jagged.lowAmplitude = 0.070f;
    jagged.highAmplitude = 0.060f;
    jagged.verticalAmplitude = 0.028f;
    jagged.edgeFalloff = 1.2f;
    jagged.transitionBoost = 0.55f;

    EdgeDisplacementProfile watery = soft;
    watery.lowAmplitude = 0.110f;
    watery.highAmplitude = 0.045f;
    watery.verticalAmplitude = 0.010f;

    cfg.typeProperties.emplace(TileType::Grass,
        makeType(TileType::Grass, "mat_grass", glm::vec4(0.43f, 0.68f, 0.38f, 1.0f), true, 5.0f, 0.035f, 0.20f, soft));
    cfg.typeProperties.emplace(TileType::Cobblestone,
        makeType(TileType::Cobblestone, "mat_cobble", glm::vec4(0.48f, 0.48f, 0.50f, 1.0f), true, 2.0f, 0.015f, 0.05f, jagged));
    cfg.typeProperties.emplace(TileType::Sand,
        makeType(TileType::Sand, "mat_sand", glm::vec4(0.80f, 0.74f, 0.52f, 1.0f), true, 6.0f, 0.040f, 0.12f, soft));
    cfg.typeProperties.emplace(TileType::Rocky,
        makeType(TileType::Rocky, "mat_rock", glm::vec4(0.40f, 0.40f, 0.41f, 1.0f), true, 3.0f, 0.020f, 0.10f, jagged));
    cfg.typeProperties.emplace(TileType::Beach,
        makeType(TileType::Beach, "mat_beach", glm::vec4(0.84f, 0.80f, 0.60f, 1.0f), true, 7.0f, 0.050f, 0.08f, soft));
    cfg.typeProperties.emplace(TileType::Snow,
        makeType(TileType::Snow, "mat_snow", glm::vec4(0.93f, 0.94f, 0.98f, 1.0f), true, 4.0f, 0.025f, 0.06f, soft));
    cfg.typeProperties.emplace(TileType::Swamp,
        makeType(TileType::Swamp, "mat_swamp", glm::vec4(0.33f, 0.42f, 0.28f, 1.0f), true, 8.0f, 0.045f, 0.24f, watery));
    cfg.typeProperties.emplace(TileType::Water,
        makeType(TileType::Water, "mat_water", glm::vec4(0.23f, 0.44f, 0.70f, 1.0f), false, 2.0f, 0.010f, 0.00f, watery));

    cfg.typeProperties[TileType::Grass].detailVariants = {
        {"tuft_short", 1.0f, 0.85f, 1.2f},
        {"tuft_wild", 0.7f, 0.8f, 1.25f},
    };
    cfg.typeProperties[TileType::Sand].detailVariants = {
        {"pebble_cluster", 1.0f, 0.8f, 1.15f},
        {"dune_ripple", 0.6f, 0.9f, 1.2f},
    };
    cfg.typeProperties[TileType::Rocky].detailVariants = {
        {"rock_chip", 1.0f, 0.75f, 1.3f},
        {"crack_strip", 0.5f, 0.9f, 1.1f},
    };
    cfg.typeProperties[TileType::Swamp].detailVariants = {
        {"reed_patch", 1.0f, 0.7f, 1.3f},
        {"puddle_dark", 0.8f, 0.9f, 1.2f},
    };

    return cfg;
}

bool ProceduralTileConfig::loadFromJsonFile(const std::string& path, ProceduralTileConfig& outConfig, std::string& outError) {
    std::ifstream in(path);
    if (!in.is_open()) {
        outError = "Unable to open procedural tile config: " + path;
        return false;
    }

    nlohmann::json root;
    try {
        in >> root;
    } catch (const std::exception& ex) {
        outError = std::string("Invalid tile config json: ") + ex.what();
        return false;
    }

    if (!root.contains("types") || !root["types"].is_array()) {
        outError = "Tile config must contain array: types";
        return false;
    }

    ProceduralTileConfig parsed = ProceduralTileConfig::defaults();

    for (const auto& item : root["types"]) {
        if (!item.is_object()) {
            continue;
        }
        const TileType type = tileTypeFromString(item.value("id", std::string{}));
        if (type == TileType::Unknown || !parsed.hasType(type)) {
            continue;
        }

        auto& props = parsed.typeProperties[type];
        props.materialName = item.value("material", props.materialName);
        props.walkable = item.value("walkable", props.walkable);
        props.maxRotationJitterDeg = item.value("rotationJitterDeg", props.maxRotationJitterDeg);
        props.maxUniformScaleJitter = item.value("scaleJitter", props.maxUniformScaleJitter);
        props.detailDensity = item.value("detailDensity", props.detailDensity);

        if (item.contains("tint") && item["tint"].is_array() && item["tint"].size() == 4) {
            props.baseTint = glm::vec4(
                item["tint"][0].get<float>(),
                item["tint"][1].get<float>(),
                item["tint"][2].get<float>(),
                item["tint"][3].get<float>());
        }

        if (item.contains("edge") && item["edge"].is_object()) {
            const auto& edge = item["edge"];
            props.edge.lowFrequency = edge.value("lowFrequency", props.edge.lowFrequency);
            props.edge.highFrequency = edge.value("highFrequency", props.edge.highFrequency);
            props.edge.lowAmplitude = edge.value("lowAmplitude", props.edge.lowAmplitude);
            props.edge.highAmplitude = edge.value("highAmplitude", props.edge.highAmplitude);
            props.edge.verticalAmplitude = edge.value("verticalAmplitude", props.edge.verticalAmplitude);
            props.edge.edgeFalloff = edge.value("edgeFalloff", props.edge.edgeFalloff);
            props.edge.transitionBoost = edge.value("transitionBoost", props.edge.transitionBoost);
        }
    }

    outConfig = std::move(parsed);
    return true;
}

const TileTypeProperties& ProceduralTileConfig::properties(TileType type) const {
    auto it = typeProperties.find(type);
    if (it != typeProperties.end()) {
        return it->second;
    }

    static const TileTypeProperties kFallback = [] {
        TileTypeProperties props{};
        props.type = TileType::Unknown;
        props.materialName = "mat_unknown";
        props.baseTint = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
        props.walkable = true;
        return props;
    }();

    return kFallback;
}

bool ProceduralTileConfig::hasType(TileType type) const {
    return typeProperties.find(type) != typeProperties.end();
}

} // namespace elysium
