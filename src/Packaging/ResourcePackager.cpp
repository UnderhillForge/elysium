#include "Packaging/ResourcePackager.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace elysium {

namespace {

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

} // namespace

bool ResourcePackager::collect(const std::filesystem::path& root,
                               const std::vector<std::string>& includeExtensions,
                               PackManifest& outManifest,
                               LogFn logger) {
    outManifest.resources.clear();

    std::vector<std::string> loweredExt;
    loweredExt.reserve(includeExtensions.size());
    for (const std::string& ext : includeExtensions) {
        loweredExt.push_back(toLowerAscii(ext));
    }

    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it{root, ec}, end; it != end; it.increment(ec)) {
        if (ec || !it->is_regular_file(ec)) {
            continue;
        }

        const std::filesystem::path sourcePath = it->path();
        const std::string ext = toLowerAscii(sourcePath.extension().string());
        if (std::find(loweredExt.begin(), loweredExt.end(), ext) == loweredExt.end()) {
            continue;
        }

        PackResourceEntry entry{};
        entry.sourcePath = sourcePath;
        entry.packagePath = std::filesystem::relative(sourcePath, root, ec);
        entry.sizeBytes = std::filesystem::file_size(sourcePath, ec);
        if (ec) {
            entry.sizeBytes = 0;
            ec.clear();
        }

        outManifest.resources.push_back(std::move(entry));
    }

    if (logger) {
        logger("ResourcePackager collected " + std::to_string(outManifest.resources.size()) + " resources.");
    }
    return true;
}

bool ResourcePackager::writeManifest(const PackManifest& manifest,
                                     const std::filesystem::path& manifestPath,
                                     LogFn logger) {
    std::error_code ec;
    std::filesystem::create_directories(manifestPath.parent_path(), ec);

    std::ofstream out(manifestPath);
    if (!out.is_open()) {
        return false;
    }

    out << "# Elysium Pack Manifest\n";
    out << "# source_path|package_path|bytes\n";
    for (const auto& resource : manifest.resources) {
        out << resource.sourcePath.string() << '|'
            << resource.packagePath.string() << '|'
            << resource.sizeBytes << '\n';
    }

    if (logger) {
        logger("ResourcePackager wrote manifest: " + manifestPath.string());
    }
    return true;
}

} // namespace elysium
