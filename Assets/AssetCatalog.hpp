#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace elysium {

enum class AssetType {
    Model,
    Texture,
    Script,
    Audio,
    Material,
    Shader,
    Other
};

struct AssetEntry {
    std::filesystem::path absolutePath;
    std::string relativePath;
    AssetType type{AssetType::Other};
};

class AssetCatalog {
public:
    bool setRoot(const std::filesystem::path& newRoot, std::string* errorMessage = nullptr);
    bool rescan(std::string* errorMessage = nullptr);

    void setFilter(const std::string& filterText);
    void setShowAuthoringSourceFormats(bool show);

    const std::filesystem::path& root() const { return m_root; }
    const std::vector<AssetEntry>& allEntries() const { return m_allEntries; }
    const std::vector<AssetEntry>& filteredEntries() const { return m_filteredEntries; }
    bool showAuthoringSourceFormats() const { return m_showAuthoringSourceFormats; }

private:
    void rebuildFilteredEntries();

private:
    std::filesystem::path m_root;
    std::vector<AssetEntry> m_allEntries;
    std::vector<AssetEntry> m_filteredEntries;
    std::string m_filterLower;
    bool m_showAuthoringSourceFormats{false};
};

} // namespace elysium
