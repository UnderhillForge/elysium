#pragma once

#include <filesystem>
#include <functional>
#include <cstdint>
#include <string>
#include <vector>

namespace elysium {

struct PackResourceEntry {
    std::filesystem::path sourcePath;
    std::filesystem::path packagePath;
    std::uintmax_t sizeBytes{0};
};

struct PackManifest {
    std::vector<PackResourceEntry> resources;
};

// Inspired by RavEngine pack_resources: keep packaging deterministic and explicit.
class ResourcePackager {
public:
    using LogFn = std::function<void(const std::string&)>;

    bool collect(const std::filesystem::path& root,
                 const std::vector<std::string>& includeExtensions,
                 PackManifest& outManifest,
                 LogFn logger = {});

    bool writeManifest(const PackManifest& manifest,
                       const std::filesystem::path& manifestPath,
                       LogFn logger = {});
};

} // namespace elysium
