#include "Assets/AssetCatalog.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace elysium {

namespace {

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

AssetType classifyAssetType(const std::filesystem::path& filePath) {
    const std::string ext = toLowerAscii(filePath.extension().string());
    if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj") {
        return AssetType::Model;
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".dds" || ext == ".ktx" || ext == ".ktx2") {
        return AssetType::Texture;
    }
    if (ext == ".lua" || ext == ".py" || ext == ".js") {
        return AssetType::Script;
    }
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
        return AssetType::Audio;
    }
    if (ext == ".mat" || ext == ".material" || ext == ".json") {
        return AssetType::Material;
    }
    if (ext == ".vert" || ext == ".frag" || ext == ".glsl" || ext == ".comp") {
        return AssetType::Shader;
    }
    return AssetType::Other;
}

bool shouldIndexAsset(const std::filesystem::path& filePath) {
    return classifyAssetType(filePath) != AssetType::Other;
}

bool isAuthoringSourceAsset(const std::filesystem::path& filePath) {
    const std::string ext = toLowerAscii(filePath.extension().string());
    return ext == ".fbx";
}

} // namespace

bool AssetCatalog::setRoot(const std::filesystem::path& newRoot, std::string* errorMessage) {
    std::error_code ec;
    if (!std::filesystem::exists(newRoot, ec) || ec) {
        if (errorMessage != nullptr) {
            *errorMessage = "Path does not exist: " + newRoot.string();
        }
        return false;
    }

    if (!std::filesystem::is_directory(newRoot, ec) || ec) {
        if (errorMessage != nullptr) {
            *errorMessage = "Path is not a directory: " + newRoot.string();
        }
        return false;
    }

    m_root = std::filesystem::weakly_canonical(newRoot, ec);
    if (ec) {
        m_root = newRoot;
    }

    return rescan(errorMessage);
}

bool AssetCatalog::rescan(std::string* errorMessage) {
    m_allEntries.clear();

    if (m_root.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Asset root is empty.";
        }
        rebuildFilteredEntries();
        return false;
    }

    std::error_code ec;
    std::filesystem::recursive_directory_iterator it{m_root, ec};
    if (ec) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to scan assets root: " + m_root.string();
        }
        rebuildFilteredEntries();
        return false;
    }

    // Inspired by xoreos/neveredit resource indexing ideas: keep a pre-scanned cache
    // and filter in-memory to avoid expensive full rescans every frame.
    const std::filesystem::recursive_directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        if (!it->is_regular_file(ec) || ec) {
            continue;
        }

        const auto& path = it->path();
        if (!shouldIndexAsset(path)) {
            continue;
        }

        AssetEntry entry{};
        entry.absolutePath = path;
        entry.relativePath = std::filesystem::relative(path, m_root, ec).string();
        entry.type = classifyAssetType(path);
        if (ec) {
            entry.relativePath = path.filename().string();
            ec.clear();
        }

        m_allEntries.push_back(std::move(entry));
    }

    std::sort(m_allEntries.begin(), m_allEntries.end(), [](const AssetEntry& a, const AssetEntry& b) {
        if (a.type != b.type) {
            return static_cast<int>(a.type) < static_cast<int>(b.type);
        }
        return a.relativePath < b.relativePath;
    });

    rebuildFilteredEntries();
    return true;
}

void AssetCatalog::setFilter(const std::string& filterText) {
    m_filterLower = toLowerAscii(filterText);
    rebuildFilteredEntries();
}

void AssetCatalog::setShowAuthoringSourceFormats(bool show) {
    if (m_showAuthoringSourceFormats == show) {
        return;
    }
    m_showAuthoringSourceFormats = show;
    rebuildFilteredEntries();
}

void AssetCatalog::rebuildFilteredEntries() {
    m_filteredEntries.clear();

    m_filteredEntries.reserve(m_allEntries.size());
    for (const auto& entry : m_allEntries) {
        if (!m_showAuthoringSourceFormats && isAuthoringSourceAsset(entry.absolutePath)) {
            continue;
        }

        if (m_filterLower.empty()) {
            m_filteredEntries.push_back(entry);
            continue;
        }

        const std::string relLower = toLowerAscii(entry.relativePath);
        if (relLower.find(m_filterLower) != std::string::npos) {
            m_filteredEntries.push_back(entry);
        }
    }
}

} // namespace elysium
