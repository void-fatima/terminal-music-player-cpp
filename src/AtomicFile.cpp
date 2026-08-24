#include "AtomicFile.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace music_player {
namespace {

std::atomic<unsigned long long> sequence{0};

unsigned long processId() noexcept {
#ifdef _WIN32
    return static_cast<unsigned long>(GetCurrentProcessId());
#else
    return static_cast<unsigned long>(getpid());
#endif
}

std::string nativeError(unsigned long code) {
#ifdef _WIN32
    return std::system_category().message(static_cast<int>(code));
#else
    (void)code;
    return std::strerror(errno);
#endif
}

class DefaultAtomicFileOps final : public IAtomicFileOps {
public:
    bool createParentDirectories(const std::filesystem::path& target,
                                 std::string& error) override {
        const auto parent = target.parent_path();
        if (parent.empty()) return true;
        std::error_code filesystemError;
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError) {
            error = "Cannot create parent directory '" + parent.string()
                + "': " + filesystemError.message();
            return false;
        }
        return true;
    }

    bool writeUniqueTemporary(const std::filesystem::path& target,
                              const std::string& contents,
                              std::filesystem::path& temporary,
                              std::string& error) override {
        cleanupAbandoned(target);
        for (int attempt = 0; attempt < 64; ++attempt) {
            const auto stamp = static_cast<unsigned long long>(
                std::chrono::steady_clock::now().time_since_epoch().count());
            temporary = target.parent_path()
                / (target.filename().string() + ".tmp." + std::to_string(processId()) + "."
                   + std::to_string(stamp) + "." + std::to_string(sequence.fetch_add(1)));
#ifdef _WIN32
            HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE) {
                const DWORD code = GetLastError();
                if (code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS) continue;
                error = "Cannot open temporary file '" + temporary.string()
                    + "': " + nativeError(code);
                return false;
            }
            std::size_t offset = 0;
            bool success = true;
            while (offset < contents.size()) {
                const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(
                    contents.size() - offset, static_cast<std::size_t>(0x7fffffff)));
                DWORD written = 0;
                if (!WriteFile(file, contents.data() + offset, chunk, &written, nullptr)
                    || written != chunk) {
                    error = "Failed while writing temporary file: "
                        + nativeError(GetLastError());
                    success = false;
                    break;
                }
                offset += written;
            }
            if (success && !FlushFileBuffers(file)) {
                error = "Cannot flush temporary file: " + nativeError(GetLastError());
                success = false;
            }
            if (!CloseHandle(file)) {
                if (success) error = "Cannot close temporary file: " + nativeError(GetLastError());
                success = false;
            }
            if (!success) {
                removeFile(temporary);
                return false;
            }
#else
            const int descriptor = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
            if (descriptor < 0) {
                if (errno == EEXIST) continue;
                error = "Cannot open temporary file '" + temporary.string()
                    + "': " + nativeError(0);
                return false;
            }
            std::size_t offset = 0;
            bool success = true;
            while (offset < contents.size()) {
                const auto written = ::write(descriptor, contents.data() + offset,
                                             contents.size() - offset);
                if (written < 0) {
                    if (errno == EINTR) continue;
                    error = "Failed while writing temporary file: " + nativeError(0);
                    success = false;
                    break;
                }
                offset += static_cast<std::size_t>(written);
            }
            if (success && ::fsync(descriptor) != 0) {
                error = "Cannot flush temporary file: " + nativeError(0);
                success = false;
            }
            if (::close(descriptor) != 0) {
                if (success) error = "Cannot close temporary file: " + nativeError(0);
                success = false;
            }
            if (!success) {
                removeFile(temporary);
                return false;
            }
#endif
            return true;
        }
        error = "Cannot allocate a unique temporary filename.";
        return false;
    }

    bool replaceFile(const std::filesystem::path& temporary,
                     const std::filesystem::path& target,
                     std::string& error) override {
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), target.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            error = "Cannot atomically replace '" + target.string()
                + "': " + nativeError(GetLastError());
            return false;
        }
#else
        if (::rename(temporary.c_str(), target.c_str()) != 0) {
            error = "Cannot atomically replace '" + target.string()
                + "': " + nativeError(0);
            return false;
        }
#endif
        return true;
    }

    void removeFile(const std::filesystem::path& file) noexcept override {
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
    }

private:
    static void cleanupAbandoned(const std::filesystem::path& target) noexcept {
        const auto directory = target.parent_path();
        const auto prefix = target.filename().string() + ".tmp.";
        const auto cutoff = std::filesystem::file_time_type::clock::now()
            - std::chrono::hours(24);
        std::error_code error;
        std::filesystem::directory_iterator iterator(directory, error);
        const std::filesystem::directory_iterator end;
        while (!error && iterator != end) {
            const auto name = iterator->path().filename().string();
            std::error_code entryError;
            const auto modified = iterator->last_write_time(entryError);
            if (!entryError && name.rfind(prefix, 0) == 0 && modified < cutoff) {
                std::filesystem::remove(iterator->path(), entryError);
            }
            iterator.increment(error);
        }
    }
};

}  // namespace

std::shared_ptr<IAtomicFileOps> makeDefaultAtomicFileOps() {
    return std::make_shared<DefaultAtomicFileOps>();
}

AtomicFileWriter::AtomicFileWriter(std::shared_ptr<IAtomicFileOps> operations)
    : operations_(operations ? std::move(operations) : makeDefaultAtomicFileOps()) {}

bool AtomicFileWriter::write(const std::filesystem::path& target,
                             const std::string& contents,
                             std::string& error) const {
    error.clear();
    if (!operations_->createParentDirectories(target, error)) return false;
    std::filesystem::path temporary;
    if (!operations_->writeUniqueTemporary(target, contents, temporary, error)) return false;
    if (!operations_->replaceFile(temporary, target, error)) {
        operations_->removeFile(temporary);
        return false;
    }
    return true;
}

}  // namespace music_player
