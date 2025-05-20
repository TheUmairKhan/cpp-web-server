#include "mock_filesystem.h"
#include <stdexcept>
#include <algorithm>

MockFileSystem::FileEntry::FileEntry(bool is_directory, const std::string& file_content)
    : is_dir(is_directory), content(file_content) {}

MockFileSystem::MockFileSystem() = default;

bool MockFileSystem::exists(const fs::path& path) const {
    return file_entries.find(normalize_path(path)) != file_entries.end();
}

bool MockFileSystem::is_directory(const fs::path& path) const {
    auto it = file_entries.find(normalize_path(path));
    return it != file_entries.end() && it->second.is_dir;
}

bool MockFileSystem::is_regular_file(const fs::path& path) const {
    auto it = file_entries.find(normalize_path(path));
    return it != file_entries.end() && !it->second.is_dir;
}

bool MockFileSystem::create_directories(const fs::path& path) const {
    std::string norm_path = normalize_path(path);
    if (exists(norm_path) && !is_directory(norm_path)) {
        return false;
    }

    fs::path current;
    for (const auto& part : path) {
        current /= part;
        std::string curr_path = normalize_path(current);
        if (!exists(curr_path)) {
            file_entries[curr_path] = FileEntry(true);
        } else if (!is_directory(curr_path)) {
            return false;
        }
    }
    return true;
}

bool MockFileSystem::remove(const fs::path& path) const {
    std::string norm_path = normalize_path(path);
    auto it = file_entries.find(norm_path);
    if (it != file_entries.end()) {
        file_entries.erase(it);
        return true;
    }
    return false;
}

fs::path MockFileSystem::canonical(const fs::path& path) const {
    return path;
}

fs::path MockFileSystem::weakly_canonical(const fs::path& path) const {
    return path;
}

fs::path MockFileSystem::read_symlink(const fs::path& path) const {
    return path;
}

std::vector<fs::path> MockFileSystem::directory_entries(const fs::path& path) const {
    std::vector<fs::path> entries;
    std::string dir_path = normalize_path(path);

    if (!exists(dir_path) || !is_directory(dir_path)) {
        return entries;
    }

    for (const auto& entry : file_entries) {
        std::string entry_path = entry.first;
        if (entry_path.find(dir_path + "/") == 0) {
            std::string relative_path = entry_path.substr(dir_path.length() + 1);
            if (!relative_path.empty() && relative_path.find('/') == std::string::npos) {
                entries.push_back(relative_path);
            }
        }
    }

    return entries;
}

std::string MockFileSystem::read_file(const fs::path& path) const {
    std::string norm_path = normalize_path(path);
    auto it = file_entries.find(norm_path);
    if (it != file_entries.end() && !it->second.is_dir) {
        return it->second.content;
    }
    throw std::runtime_error("Failed to read file: " + path.string());
}

bool MockFileSystem::write_file(const fs::path& path, const std::string& content) const {
    std::string norm_path = normalize_path(path);
    fs::path parent_path = path.parent_path();
    if (!parent_path.empty()) {
        create_directories(parent_path);
    }

    file_entries[norm_path] = FileEntry(false, content);
    return true;
}

void MockFileSystem::add_file(const fs::path& path, const std::string& content) const {
    std::string norm_path = normalize_path(path);
    fs::path parent_path = path.parent_path();
    if (!parent_path.empty()) {
        create_directories(parent_path);
    }
    file_entries[norm_path] = FileEntry(false, content);
}

void MockFileSystem::add_directory(const fs::path& path) const {
    std::string norm_path = normalize_path(path);
    file_entries[norm_path] = FileEntry(true);
}

void MockFileSystem::clear() const {
    file_entries.clear();
}

std::string MockFileSystem::normalize_path(const fs::path& path) const {
    return path.generic_string();
}