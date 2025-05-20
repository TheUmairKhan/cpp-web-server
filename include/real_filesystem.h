#ifndef REAL_FILESYSTEM_HPP
#define REAL_FILESYSTEM_HPP

#include "filesystem.h"
#include <string>
#include <vector>
#include <filesystem>

class RealFileSystem : public FileSystemInterface {
public:
    bool exists(const fs::path& path) const override;
    bool is_directory(const fs::path& path) const override;
    bool is_regular_file(const fs::path& path) const override;
    bool create_directories(const fs::path& path) const override;
    bool remove(const fs::path& path) const override;
    fs::path canonical(const fs::path& path) const override;
    fs::path weakly_canonical(const fs::path& path) const override;
    fs::path read_symlink(const fs::path& path) const override;
    std::vector<fs::path> directory_entries(const fs::path& path) const override;
    std::string read_file(const fs::path& path) const override;
    bool write_file(const fs::path& path, const std::string& content) const override;
};

#endif // REAL_FILESYSTEM_HPP