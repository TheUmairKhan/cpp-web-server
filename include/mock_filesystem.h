#ifndef MOCK_FILESYSTEM_HPP
#define MOCK_FILESYSTEM_HPP

#include "filesystem.h"
#include <string>
#include <vector>
#include <map>
#include <filesystem>

class MockFileSystem : public FileSystemInterface {
public:
    struct FileEntry {
        bool is_dir;
        std::string content;
        FileEntry(bool is_directory = false, const std::string& file_content = "");
    };

    MockFileSystem();

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

    void add_file(const fs::path& path, const std::string& content) const;
    void add_directory(const fs::path& path) const;
    void clear() const;

private:
    std::string normalize_path(const fs::path& path) const;
    mutable std::map<std::string, FileEntry> file_entries;
};

#endif // MOCK_FILESYSTEM_HPP