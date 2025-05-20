#include "real_filesystem.h"
#include <fstream>
#include <stdexcept>


bool RealFileSystem::exists(const fs::path& path) const {
    return fs::exists(path);
}

bool RealFileSystem::is_directory(const fs::path& path) const {
    return fs::is_directory(path);
}

bool RealFileSystem::is_regular_file(const fs::path& path) const {
    return fs::is_regular_file(path);
}

bool RealFileSystem::create_directories(const fs::path& path) const {
    return fs::create_directories(path);
}

bool RealFileSystem::remove(const fs::path& path) const {
    return fs::remove(path);
}

fs::path RealFileSystem::canonical(const fs::path& path) const {
    return fs::canonical(path);
}

fs::path RealFileSystem::weakly_canonical(const fs::path& path) const {
    return fs::weakly_canonical(path);
}

fs::path RealFileSystem::read_symlink(const fs::path& path) const {
    return fs::read_symlink(path);
}

std::vector<fs::path> RealFileSystem::directory_entries(const fs::path& path) const {
    std::vector<fs::path> entries;
    for (const auto& entry : fs::directory_iterator(path)) {
        entries.push_back(entry.path().filename());
    }
    return entries;
}

std::string RealFileSystem::read_file(const fs::path& path) const {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open file for reading: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool RealFileSystem::write_file(const fs::path& path, const std::string& content) const {
    std::ofstream file(path);
    if (!file) {
        return false;
    }
    file << content;
    return file.good();
}