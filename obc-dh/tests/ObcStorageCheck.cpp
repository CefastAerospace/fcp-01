/**
 * @file ObcStorageCheck.cpp
 * @brief Verificação de arquivos reais e falhas POSIX, sem cartão ou FreeRTOS.
 * @details Executar em obc-dh/:
 * g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcStorage.cpp tests/ObcStorageCheck.cpp -Wl,--wrap=write,--wrap=read,--wrap=lseek,--wrap=fsync,--wrap=close -o /tmp/fcp01-storage-check
 * /tmp/fcp01-storage-check
 */

#include "ObcStorage.h" // Núcleo real e resultados de armazenamento.
#include <assert.h> // assert.
#include <cstdlib> // mkdtemp para diretório exclusivo de teste.
#include <dirent.h> // opendir, readdir e closedir na limpeza do diretório criado.
#include <string>
#include <vector>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcStorage.h".
 */

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

static int write_failures = 0;
static int sync_failures = 0;
static int seek_failures = 0;
static int write_error = EIO;
static bool partial_write = false;
static bool zero_write = false;
static bool partial_read = false;
static bool read_failure = false;
static bool close_failure = false;
static unsigned write_calls = 0;
static std::vector<off_t> write_offsets;

// ─── HELPERS ──────────────────────────────────────────────────────

// GNU ld mantém as chamadas reais; somente este executável injeta falhas.
extern "C" ssize_t __real_write(
    int fd,
    const void* data,
    size_t size
);
extern "C" ssize_t __real_read(
    int fd,
    void* data,
    size_t size
);
extern "C" off_t __real_lseek(
    int fd,
    off_t offset,
    int whence
);
extern "C" int __real_fsync(
    int fd
);
extern "C" int __real_close(
    int fd
);

// Escreve prefixo real ou simula erro; a próxima tentativa usa o mesmo arquivo.
extern "C" ssize_t __wrap_write(
    int fd,
    const void* data,
    size_t size
) {
    ++write_calls;
    write_offsets.push_back(__real_lseek(fd, 0, SEEK_CUR));
    if (write_failures > 0) {
        --write_failures;
        errno = write_error;
        return -1;
    }
    if (zero_write) return 0;
    if (partial_write) {
        partial_write = false;
        return __real_write(fd, data, size / 2);
    }
    return __real_write(fd, data, size);
}

// Exercita erro e leitura incompleta com arquivo cujo tamanho já é conhecido.
extern "C" ssize_t __wrap_read(
    int fd,
    void* data,
    size_t size
) {
    if (read_failure) {
        errno = EIO;
        return -1;
    }
    return __real_read(fd, data, partial_read ? size / 2 : size);
}

// Falha de posicionamento não deve permitir uma escrita no offset errado.
extern "C" off_t __wrap_lseek(
    int fd,
    off_t offset,
    int whence
) {
    if (seek_failures > 0) {
        --seek_failures;
        errno = EIO;
        return -1;
    }
    return __real_lseek(fd, offset, whence);
}

// Falha após escrita completa representa confirmação incerta, não zero bytes.
extern "C" int __wrap_fsync(
    int fd
) {
    if (sync_failures > 0) {
        --sync_failures;
        errno = EIO;
        return -1;
    }
    return __real_fsync(fd);
}

// O descritor real é liberado antes do erro; repetir close seria incorreto.
extern "C" int __wrap_close(
    int fd
) {
    const int result = __real_close(fd);
    if (close_failure) {
        errno = EIO;
        return -1;
    }
    return result;
}

// Restaura injeção sem alterar arquivos ou estado do serviço sob teste.
static void ResetFaults(
) {
    write_failures = sync_failures = seek_failures = 0;
    write_error = EIO;
    partial_write = zero_write = partial_read = read_failure = close_failure = false;
    write_calls = 0;
    write_offsets.clear();
}

// Lê com a API real e compara com conteúdo esperado independente das escritas.
static void CheckBytes(
    obc_storage::Storage& storage,
    const char* name,
    const char* expected
) {
    uint8_t bytes[128] = {};
    uint32_t total = 0;
    const obc_storage::RESULT result = storage.Read(name, 0, bytes, sizeof(bytes), total);
    assert(result.status == obc_storage::STATUS::OK);
    assert(total == std::strlen(expected));
    assert(result.transferred == total);
    assert(std::memcmp(bytes, expected, total) == 0);
}

// Recomeça explicitamente, conservando todos os arquivos anteriores.
static void Restart(
    obc_storage::Storage& storage,
    const std::string& directory
) {
    ResetFaults();
    storage.End();
    assert(storage.Init(directory.c_str()).status == obc_storage::STATUS::OK);
}

// Exercita estado, limites e extração por offset, incluindo EOF e nomes inválidos.
static void CheckFiles(
    obc_storage::Storage& storage,
    const std::string& directory
) {
    using namespace obc_storage;
    const uint8_t block[] = {'A', 'B', 'C', 'D', 'E', 'F'};
    assert(storage.OpenNew("normal.bin").status == STATUS::NOT_INITIALIZED);
    assert(storage.Init(nullptr).status == STATUS::INVALID_ARGUMENT);
    assert(storage.Init("").status == STATUS::INVALID_ARGUMENT);
    const std::string long_directory(directory_chars + 1, 'd');
    assert(storage.Init(long_directory.c_str()).status == STATUS::INVALID_ARGUMENT);
    assert(storage.Init(directory.c_str()).status == STATUS::OK);
    assert(storage.Init(directory.c_str()).status == STATUS::OK);
    assert(storage.Init("/another-root").status == STATUS::BUSY);
    assert(storage.Append(block, sizeof(block)).status == STATUS::NO_FILE);
    for (const char* name : {"", ".", "..", "../bad", "/bad", "bad/name", "bad name", "bad\\name"}) {
        assert(storage.OpenNew(name).status == STATUS::INVALID_ARGUMENT);
    }
    assert(storage.OpenNew(nullptr).status == STATUS::INVALID_ARGUMENT);
    const std::string long_name(filename_chars + 1, 'n');
    assert(storage.OpenNew(long_name.c_str()).status == STATUS::INVALID_ARGUMENT);
    assert(storage.OpenNew("normal.bin").status == STATUS::OK);
    assert(storage.OpenNew("other.bin").status == STATUS::BUSY);
    assert(storage.Append(nullptr, 1).status == STATUS::INVALID_ARGUMENT);
    assert(storage.Append(block, 0).status == STATUS::INVALID_ARGUMENT);
    assert(storage.Append(block, static_cast<size_t>(max_file_bytes) + 1).status == STATUS::FILE_LIMIT);
    RESULT result = storage.Append(block, sizeof(block));
    assert(result.status == STATUS::OK && result.transferred == sizeof(block) && result.attempts == 1);
    assert(storage.Append(block, max_file_bytes).status == STATUS::FILE_LIMIT);
    uint8_t bytes[16] = {};
    uint32_t total = 123;
    assert(storage.Read("normal.bin", 0, bytes, sizeof(bytes), total).status == STATUS::BUSY);
    assert(total == 0);
    assert(storage.Close().status == STATUS::OK);
    assert(storage.Close().status == STATUS::OK);
    assert(storage.OpenNew("normal.bin").status == STATUS::ALREADY_EXISTS);
    CheckBytes(storage, "normal.bin", "ABCDEF");
    result = storage.Read("normal.bin", 2, bytes, 2, total);
    assert(result.status == STATUS::OK && result.transferred == 2 && total == 6);
    assert(bytes[0] == 'C' && bytes[1] == 'D');
    result = storage.Read("normal.bin", 6, bytes, sizeof(bytes), total);
    assert(result.status == STATUS::OK && result.transferred == 0 && total == 6);
    assert(storage.Read("normal.bin", 7, bytes, sizeof(bytes), total).status == STATUS::INVALID_ARGUMENT);
    assert(storage.Read("../normal.bin", 0, bytes, sizeof(bytes), total).status == STATUS::INVALID_ARGUMENT);
    assert(storage.Read("normal.bin", 0, nullptr, 1, total).status == STATUS::INVALID_ARGUMENT);
    assert(storage.Read("normal.bin", 0, bytes, 0, total).status == STATUS::INVALID_ARGUMENT);
    assert(storage.Read("missing.bin", 0, bytes, sizeof(bytes), total).status == STATUS::NOT_FOUND);

    partial_read = true;
    result = storage.Read("normal.bin", 0, bytes, sizeof(bytes), total);
    assert(result.status == STATUS::READ_FAILED && result.transferred == 3);
    ResetFaults();
    read_failure = true;
    result = storage.Read("normal.bin", 0, bytes, sizeof(bytes), total);
    assert(result.status == STATUS::READ_FAILED && result.system_error == EIO);
    ResetFaults();
    close_failure = true;
    result = storage.Read("normal.bin", 0, bytes, sizeof(bytes), total);
    assert(result.status == STATUS::CLOSE_FAILED && result.transferred == 6);
    ResetFaults();

    const std::string max_name(filename_chars, 'n');
    assert(storage.OpenNew(max_name.c_str()).status == STATUS::OK);
    assert(storage.Close().status == STATUS::OK);
    CheckBytes(storage, max_name.c_str(), "");
    Storage invalid_directory;
    assert(invalid_directory.Init((directory + "/normal.bin").c_str()).status == STATUS::INIT_FAILED);
    CheckBytes(storage, "normal.bin", "ABCDEF");
}

// Escrita parcial, erro transitório e fsync incerto nunca duplicam o bloco
// nem alteram o prefixo já confirmado. Verifica os offsets reais das tentativas.
static void CheckRetries(
    obc_storage::Storage& storage
) {
    using namespace obc_storage;
    const uint8_t prefix[] = {'O', 'L', 'D'};
    const uint8_t block[] = {'1', '2', '3', '4', '5', '6'};
    for (const char* name : {"partial.bin", "transient.bin", "sync-once.bin"}) {
        assert(storage.OpenNew(name).status == STATUS::OK);
        assert(storage.Append(prefix, sizeof(prefix)).status == STATUS::OK);
        ResetFaults();
        if (std::strcmp(name, "partial.bin") == 0) partial_write = true;
        if (std::strcmp(name, "transient.bin") == 0) write_failures = 1;
        if (std::strcmp(name, "sync-once.bin") == 0) sync_failures = 1;
        const RESULT result = storage.Append(block, sizeof(block));
        assert(result.status == STATUS::OK && result.transferred == 6 && result.attempts == 2);
        assert(write_calls == 2 && write_offsets.size() == 2);
        assert(write_offsets[0] == 3 && write_offsets[1] == 3);
        ResetFaults();
        assert(storage.Close().status == STATUS::OK);
        CheckBytes(storage, name, "OLD123456");
    }
}

// Falhas permanentes esgotam exatamente três tentativas. Mesmo Init idempotente
// não libera o escritor; a recuperação exige End + Init e nome novo.
static void CheckPermanentFailures(
    obc_storage::Storage& storage,
    const std::string& directory
) {
    using namespace obc_storage;
    const uint8_t block[] = {'D', 'A', 'T', 'A'};
    for (const char* name : {"write-failed.bin", "sync-failed.bin", "nospace.bin", "seek-failed.bin", "zero.bin"}) {
        assert(storage.OpenNew(name).status == STATUS::OK);
        ResetFaults();
        STATUS expected = STATUS::WRITE_FAILED;
        if (std::strcmp(name, "write-failed.bin") == 0) write_failures = 3;
        if (std::strcmp(name, "sync-failed.bin") == 0) {
            sync_failures = 3;
            expected = STATUS::SYNC_FAILED;
        }
        if (std::strcmp(name, "nospace.bin") == 0) {
            write_failures = 3;
            write_error = ENOSPC;
            expected = STATUS::NO_SPACE;
        }
        if (std::strcmp(name, "seek-failed.bin") == 0) seek_failures = 3;
        if (std::strcmp(name, "zero.bin") == 0) zero_write = true;
        const RESULT result = storage.Append(block, sizeof(block));
        assert(result.status == expected && result.attempts == 3);
        if (expected == STATUS::SYNC_FAILED) assert(result.transferred == sizeof(block));
        if (expected == STATUS::NO_SPACE) assert(result.system_error == ENOSPC);
        if (std::strcmp(name, "seek-failed.bin") == 0) assert(write_calls == 0);
        assert(storage.Append(block, sizeof(block)).status == STATUS::WRITER_FAILED);
        assert(storage.Init(directory.c_str()).status == STATUS::OK);
        assert(storage.Append(block, sizeof(block)).status == STATUS::WRITER_FAILED);
        ResetFaults();
        assert(storage.Close().status == STATUS::WRITER_FAILED);
        assert(storage.OpenNew("refused.bin").status == STATUS::WRITER_FAILED);
        CheckBytes(storage, name, expected == STATUS::SYNC_FAILED ? "DATA" : "");
        Restart(storage, directory);
        assert(storage.OpenNew(name).status == STATUS::ALREADY_EXISTS);
    }
}

// Fechamento ou sincronização malsucedidos também impedem novas escritas.
static void CheckCloseFailures(
    obc_storage::Storage& storage,
    const std::string& directory
) {
    using namespace obc_storage;
    assert(storage.OpenNew("close-failed.bin").status == STATUS::OK);
    close_failure = true;
    assert(storage.Close().status == STATUS::CLOSE_FAILED);
    ResetFaults();
    assert(storage.OpenNew("refused.bin").status == STATUS::WRITER_FAILED);
    Restart(storage, directory);
    assert(storage.OpenNew("close-sync-failed.bin").status == STATUS::OK);
    sync_failures = 1;
    assert(storage.End().status == STATUS::SYNC_FAILED);
    ResetFaults();
    assert(storage.OpenNew("refused.bin").status == STATUS::NOT_INITIALIZED);
    assert(storage.Init(directory.c_str()).status == STATUS::OK);
    assert(storage.OpenNew("close-sync-failed.bin").status == STATUS::ALREADY_EXISTS);
}

int main(
) {
    char temporary[] = "/tmp/fcp01-storage-XXXXXX";
    const char* root = ::mkdtemp(temporary);
    assert(root != nullptr);
    const std::string directory = std::string(root) + "/obc";
    obc_storage::Storage storage;
    CheckFiles(storage, directory);
    CheckRetries(storage);
    CheckPermanentFailures(storage, directory);
    CheckCloseFailures(storage, directory);
    assert(storage.End().status == obc_storage::STATUS::OK);

    // Remove somente os arquivos no diretório exclusivo criado por este teste.
    DIR* entries = ::opendir(directory.c_str());
    assert(entries != nullptr);
    while (const struct dirent* entry = ::readdir(entries)) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;
        assert(::unlink((directory + "/" + entry->d_name).c_str()) == 0);
    }
    assert(::closedir(entries) == 0);
    assert(::rmdir(directory.c_str()) == 0);
    assert(::rmdir(root) == 0);
    std::printf("Armazenamento: arquivos, offsets, repetição, sincronização e falhas aprovados.\n");
    return 0;
}
