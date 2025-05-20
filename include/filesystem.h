#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

class FileSystemInterface {
public:
    virtual ~FileSystemInterface() = default;

    // Directory operations
    virtual bool exists(const fs::path& path) const = 0;
    virtual bool is_directory(const fs::path& path) const = 0;
    virtual bool is_regular_file(const fs::path& path) const = 0;
    virtual bool create_directories(const fs::path& path) const = 0;
    virtual bool remove(const fs::path& path) const = 0;
    virtual fs::path canonical(const fs::path& path) const = 0;
    virtual fs::path weakly_canonical(const fs::path& path) const = 0;
    virtual fs::path read_symlink(const fs::path& path) const = 0;

    // Directory entry iterator - returns vector of filenames
    virtual std::vector<fs::path> directory_entries(const fs::path& path) const = 0;

    // File operations
    virtual std::string read_file(const fs::path& path) const = 0;
    virtual bool write_file(const fs::path& path, const std::string& content) const = 0;
};

#endif