#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace music_player {

class IAtomicFileOps {
public:
    virtual ~IAtomicFileOps() = default;
    virtual bool createParentDirectories(const std::filesystem::path& target,
                                         std::string& error) = 0;
    virtual bool writeUniqueTemporary(const std::filesystem::path& target,
                                      const std::string& contents,
                                      std::filesystem::path& temporary,
                                      std::string& error) = 0;
    virtual bool replaceFile(const std::filesystem::path& temporary,
                             const std::filesystem::path& target,
                             std::string& error) = 0;
    virtual void removeFile(const std::filesystem::path& file) noexcept = 0;
};

std::shared_ptr<IAtomicFileOps> makeDefaultAtomicFileOps();

class AtomicFileWriter {
public:
    explicit AtomicFileWriter(std::shared_ptr<IAtomicFileOps> operations = {});
    bool write(const std::filesystem::path& target,
               const std::string& contents,
               std::string& error) const;

private:
    std::shared_ptr<IAtomicFileOps> operations_;
};

}  // namespace music_player
