/**
 * @file ObcStorage.h
 * @brief Arquivos persistentes com criação exclusiva e repetição limitada.
 */

#pragma once

#include <stdint.h> // uint8_t, uint32_t e INT32_MAX.
#include <stddef.h> // size_t.
#include <cerrno> // errno e códigos POSIX de falha.
#include <cstdio> // std::snprintf para caminhos limitados.
#include <cstring> // std::strlen, std::strcmp e std::memcpy.
#include <fcntl.h> // open, O_CREAT, O_EXCL e modos de acesso.
#include <sys/stat.h> // stat, fstat, mkdir e S_ISDIR/S_ISREG.
#include <unistd.h> // read, write, lseek, fsync e close.

namespace obc_storage {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr size_t filename_chars = 64;
    constexpr size_t directory_chars = 127;
    constexpr uint8_t write_attempts = 3;
    // off_t do runtime ESP32 é assinado de 32 bits; não confundir com GB do cartão.
    constexpr uint32_t max_file_bytes = INT32_MAX;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Resultado da operação, separado de confirmação física do cartão. */
    enum class STATUS {
        OK,
        INVALID_ARGUMENT,
        INVALID_CONTEXT,
        NOT_INITIALIZED,
        BUSY,
        NO_FILE,
        ALREADY_EXISTS,
        NOT_FOUND,
        FILE_LIMIT,
        NO_SPACE,
        INIT_FAILED,
        MOUNT_FAILED,
        OPEN_FAILED,
        WRITE_FAILED,
        READ_FAILED,
        SYNC_FAILED,
        CLOSE_FAILED,
        WRITER_FAILED
    };

    /**
     * @brief Estado, bytes transferidos, errno original e tentativas realizadas.
     * @details Em Append, transferred é a quantidade da última tentativa,
     * sem somar reescritas. Só OK confirma bloco completo e fsync bem-sucedido.
     * SYNC_FAILED pode ter transferred igual ao tamanho, sem confirmação.
     * Em Read, transferred pode ser parcial em falha. As demais operações
     * retornam zero bytes/tentativas; system_error zero indica erro de contrato.
     */
    struct RESULT {
        STATUS status;
        size_t transferred;
        int system_error;
        uint8_t attempts;
    };

    // ─── CLASSES ──────────────────────────────────────────────────────

    /**
     * @brief Núcleo POSIX, sem montagem, mutex, interpretação ou cópia dos dados.
     * @details Não é copiável nem seguro para chamadas concorrentes. O adaptador
     * ESP32 serializa o acesso. O chamador mantém o buffer de Append íntegro até
     * o retorno. Não há cache próprio, fila ou reparo de FAT32. Usar Close/End
     * explicitamente para observar falhas; o destrutor só libera recursos.
     */
    class Storage {
    public:
        /** @brief Constrói serviço inativo, sem acessar arquivos. */
        Storage(
        );

        /** @brief Libera recursos; resultados de fechamento exigem End explícito. */
        ~Storage(
        );

        /**
         * @brief Prepara um diretório existente ou cria somente seu último nível.
         * @param directory Caminho NUL-terminado; pai deve existir e estar montado.
         * @return OK, INVALID_ARGUMENT, BUSY ou INIT_FAILED. Repetir o mesmo
         * diretório não recupera um escritor falho nem altera arquivos abertos.
         */
        RESULT Init(
            const char* directory
        );

        /**
         * @brief Cria arquivo vazio sem sobrescrever qualquer nome existente.
         * @param name Nome de 1–64 bytes ASCII [A-Za-z0-9._-], exceto . e ...
         * @return Estado; exige serviço inicializado e nenhum escritor ativo.
         */
        RESULT OpenNew(
            const char* name
        );

        /**
         * @brief Escreve bloco e sincroniza, com até três tentativas totais.
         * @details Cada tentativa reposiciona no último offset confirmado e
         * reescreve o bloco inteiro. Não usa O_APPEND. Sucesso avança o offset;
         * falha final bloqueia novas escritas/aberturas até End + Init. O arquivo
         * é preservado, inclusive eventual cauda parcial ou bloco não confirmado.
         * Nenhuma tentativa modifica bytes anteriores ao offset confirmado.
         * Não garante recuperação após reset, queda de energia ou defeito físico.
         * @param data Buffer não nulo, estável até o retorno.
         * @param size Quantidade não zero; bloco e arquivo limitados a INT32_MAX.
         * @return Resultado da última tentativa; erro de argumento/limite não
         * inicia I/O nem invalida o escritor. Não há leitura de verificação.
         */
        RESULT Append(
            const uint8_t* data,
            size_t size
        );

        /**
         * @brief Lê até capacity bytes de arquivo fechado, sem alterar sua origem.
         * @param name Nome com as mesmas restrições de OpenNew.
         * @param offset Offset até o fim; offset maior é inválido.
         * @param buffer Destino não nulo, pertencente ao chamador.
         * @param capacity Capacidade não zero, limitada a INT32_MAX.
         * @param total_size Saída: tamanho do arquivo após fstat; zero antes disso.
         * @return OK com zero bytes no EOF; BUSY para o escritor ainda aberto.
         */
        RESULT Read(
            const char* name,
            uint32_t offset,
            uint8_t* buffer,
            size_t capacity,
            uint32_t& total_size
        );

        /**
         * @brief Sincroniza e fecha o escritor, conferindo ambos os resultados.
         * @details Sem escritor retorna OK. Não repete close em falha, pois o
         * descritor pode já ter sido liberado. Escritor falho não é reabilitado.
         */
        RESULT Close(
        );

        /** @brief Fecha recursos e desativa o núcleo, preservando erro de Close. */
        RESULT End(
        );

    private:
        Storage(const Storage&) = delete;
        Storage& operator=(const Storage&) = delete;

        /** @brief Valida nome e produz caminho sem permitir travessia de diretório. */
        bool BuildPath(
            const char* name,
            char* path
        ) const;

        char directory_[directory_chars + 1];
        char active_name_[filename_chars + 1];
        int writer_fd_;
        uint32_t confirmed_bytes_;
        bool initialized_;
        bool failed_;
    };

} // namespace obc_storage
