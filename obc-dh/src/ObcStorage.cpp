#include "ObcStorage.h" // Núcleo de arquivos e APIs POSIX com resultados verificáveis.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcStorage.h".
 */

namespace obc_storage {

    namespace {

        // ─── HELPERS ──────────────────────────────────────────────────────

        // Conserva errno antes que limpeza ou outra chamada o substitua.
        RESULT IoFailure(
            STATUS status,
            int error,
            size_t transferred = 0,
            uint8_t attempts = 0
        ) {
            if (error == ENOSPC) status = STATUS::NO_SPACE;
            if (error == EFBIG) status = STATUS::FILE_LIMIT;
            return {status, transferred, error, attempts};
        }

    } // namespace

    // ==================================================================
    // === CLASSE =======================================================
    // ==================================================================

    // ─── CONSTRUTORES ─────────────────────────────────────────────────

    // Público:
    Storage::Storage(
    ) : directory_{}, active_name_{}, writer_fd_(-1), confirmed_bytes_(0),
        initialized_(false), failed_(false) {
    }

    // ─── DESTRUTORES ──────────────────────────────────────────────────

    // Público:
    Storage::~Storage(
    ) {
        End();
    }

    // ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

    // Público:
    RESULT Storage::Init(
        const char* directory
    ) {
        if (directory == nullptr || directory[0] == '\0'
                || std::strlen(directory) > directory_chars) {
            return {STATUS::INVALID_ARGUMENT, 0, 0, 0};
        }
        if (initialized_) {
            return {std::strcmp(directory_, directory) == 0 ? STATUS::OK : STATUS::BUSY, 0, 0, 0};
        }

        // Não formata nem recria hierarquias: o pai precisa estar montado.
        if (::mkdir(directory, 0700) != 0 && errno != EEXIST) {
            return IoFailure(STATUS::INIT_FAILED, errno);
        }
        struct stat info;
        if (::stat(directory, &info) != 0) return IoFailure(STATUS::INIT_FAILED, errno);
        if (!S_ISDIR(info.st_mode)) return IoFailure(STATUS::INIT_FAILED, ENOTDIR);
        std::memcpy(directory_, directory, std::strlen(directory) + 1);
        initialized_ = true;
        return {STATUS::OK, 0, 0, 0};
    }

    // Público:
    RESULT Storage::OpenNew(
        const char* name
    ) {
        if (!initialized_) return {STATUS::NOT_INITIALIZED, 0, 0, 0};
        if (failed_) return {STATUS::WRITER_FAILED, 0, 0, 0};
        if (writer_fd_ >= 0) return {STATUS::BUSY, 0, 0, 0};
        char path[directory_chars + filename_chars + 2];
        if (!BuildPath(name, path)) return {STATUS::INVALID_ARGUMENT, 0, 0, 0};

        // O_EXCL impede truncamento ou reutilização de arquivo anterior.
        const int fd = ::open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0) return IoFailure(errno == EEXIST ? STATUS::ALREADY_EXISTS : STATUS::OPEN_FAILED, errno);
        writer_fd_ = fd;
        confirmed_bytes_ = 0;
        std::memcpy(active_name_, name, std::strlen(name) + 1);
        return {STATUS::OK, 0, 0, 0};
    }

    // Privado:
    bool Storage::BuildPath(
        const char* name,
        char* path
    ) const {
        if (name == nullptr || name[0] == '\0' || std::strlen(name) > filename_chars
                || std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) return false;
        for (const char* cursor = name; *cursor != '\0'; ++cursor) {
            const char value = *cursor;
            if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z')
                    || (value >= '0' && value <= '9') || value == '.' || value == '_'
                    || value == '-')) return false;
        }
        std::snprintf(path, directory_chars + filename_chars + 2, "%s/%s", directory_, name);
        return true;
    }

    // Público:
    RESULT Storage::Append(
        const uint8_t* data,
        size_t size
    ) {
        if (!initialized_) return {STATUS::NOT_INITIALIZED, 0, 0, 0};
        if (failed_) return {STATUS::WRITER_FAILED, 0, 0, 0};
        if (writer_fd_ < 0) return {STATUS::NO_FILE, 0, 0, 0};
        if (data == nullptr || size == 0) return {STATUS::INVALID_ARGUMENT, 0, 0, 0};
        if (size > max_file_bytes - confirmed_bytes_) return {STATUS::FILE_LIMIT, 0, 0, 0};

        RESULT result = {STATUS::WRITE_FAILED, 0, 0, 0};
        for (uint8_t attempt = 1; attempt <= write_attempts; ++attempt) {
            // A mesma origem e o mesmo offset tornam a repetição idempotente
            // no arquivo lógico, inclusive quando fsync falhou após escrever tudo.
            if (::lseek(writer_fd_, static_cast<off_t>(confirmed_bytes_), SEEK_SET) < 0) {
                result = IoFailure(STATUS::WRITE_FAILED, errno, 0, attempt);
                continue;
            }
            const ssize_t count = ::write(writer_fd_, data, size);
            if (count < 0 || static_cast<size_t>(count) != size) {
                result = IoFailure(STATUS::WRITE_FAILED, count < 0 ? errno : EIO,
                    count < 0 ? 0 : static_cast<size_t>(count), attempt);
                continue;
            }
            if (::fsync(writer_fd_) != 0) {
                result = IoFailure(STATUS::SYNC_FAILED, errno, size, attempt);
                continue;
            }
            confirmed_bytes_ += static_cast<uint32_t>(size);
            return {STATUS::OK, size, 0, attempt};
        }

        // Não aceita outro bloco sobre uma cauda cuja gravação não foi confirmada.
        failed_ = true;
        return result;
    }

    // Público:
    RESULT Storage::Read(
        const char* name,
        uint32_t offset,
        uint8_t* buffer,
        size_t capacity,
        uint32_t& total_size
    ) {
        total_size = 0;
        if (!initialized_) return {STATUS::NOT_INITIALIZED, 0, 0, 0};
        char path[directory_chars + filename_chars + 2];
        if (!BuildPath(name, path) || buffer == nullptr || capacity == 0
                || capacity > max_file_bytes) return {STATUS::INVALID_ARGUMENT, 0, 0, 0};
        if (writer_fd_ >= 0 && std::strcmp(name, active_name_) == 0) {
            return {STATUS::BUSY, 0, 0, 0};
        }
        const int fd = ::open(path, O_RDONLY);
        if (fd < 0) return IoFailure(errno == ENOENT ? STATUS::NOT_FOUND : STATUS::OPEN_FAILED, errno);

        // Cada leitura usa descritor próprio; nenhum offset do escritor muda.
        RESULT result = {STATUS::OK, 0, 0, 0};
        struct stat info;
        if (::fstat(fd, &info) != 0) {
            result = IoFailure(STATUS::READ_FAILED, errno);
        } else if (!S_ISREG(info.st_mode) || info.st_size < 0
                || static_cast<uint64_t>(info.st_size) > max_file_bytes) {
            result = {STATUS::FILE_LIMIT, 0, 0, 0};
        } else {
            total_size = static_cast<uint32_t>(info.st_size);
            if (offset > total_size) {
                result = {STATUS::INVALID_ARGUMENT, 0, 0, 0};
            } else if (::lseek(fd, static_cast<off_t>(offset), SEEK_SET) < 0) {
                result = IoFailure(STATUS::READ_FAILED, errno);
            } else {
                // EOF limita a quantidade; não varre nem carrega o arquivo inteiro.
                const size_t remaining = total_size - offset;
                const size_t wanted = capacity < remaining ? capacity : remaining;
                if (wanted != 0) {
                    const ssize_t count = ::read(fd, buffer, wanted);
                    if (count < 0 || static_cast<size_t>(count) != wanted) {
                        result = IoFailure(STATUS::READ_FAILED, count < 0 ? errno : EIO,
                            count < 0 ? 0 : static_cast<size_t>(count));
                    } else {
                        result.transferred = static_cast<size_t>(count);
                    }
                }
            }
        }
        if (::close(fd) != 0 && result.status == STATUS::OK) {
            result = IoFailure(STATUS::CLOSE_FAILED, errno, result.transferred);
        }
        return result;
    }

    // Público:
    RESULT Storage::Close(
    ) {
        if (!initialized_) return {STATUS::NOT_INITIALIZED, 0, 0, 0};
        if (writer_fd_ < 0) return {STATUS::OK, 0, 0, 0};
        RESULT result = {failed_ ? STATUS::WRITER_FAILED : STATUS::OK, 0, 0, 0};
        if (!failed_ && ::fsync(writer_fd_) != 0) result = IoFailure(STATUS::SYNC_FAILED, errno);

        // Mesmo em erro de fsync, tenta fechar uma única vez. Não reutiliza fd.
        const int fd = writer_fd_;
        writer_fd_ = -1;
        active_name_[0] = '\0';
        if (::close(fd) != 0 && result.status == STATUS::OK) {
            result = IoFailure(STATUS::CLOSE_FAILED, errno);
        }
        if (result.status != STATUS::OK) failed_ = true;
        return result;
    }

    // Público:
    RESULT Storage::End(
    ) {
        const RESULT result = initialized_ ? Close() : RESULT{STATUS::OK, 0, 0, 0};
        initialized_ = false;
        failed_ = false;
        confirmed_bytes_ = 0;
        directory_[0] = '\0';
        return result;
    }

} // namespace obc_storage
