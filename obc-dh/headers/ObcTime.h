/**
 * @file ObcTime.h
 * @brief Referências temporais por boot e reserva persistente de identificadores.
 */

#pragma once

#include <stdint.h> // uint32_t, uint64_t, int64_t e limites inteiros.

namespace obc_time {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    /**
     * @brief Compensação inicial do enlace, em ms; ajustar por recompilação.
     * @details Airtime estimado de 162,048 ms, arredondado para 162 ms, para
     * set_time de 63 bytes: SF7, BW 125 kHz, CR 4/6, preâmbulo 32, cabeçalho
     * explícito e CRC. Supõe horário capturado no início efetivo do TX.
     * Não inclui filas, retransmissões, quantização em segundos ou deriva.
     * O comprimento real pode variar. Não representa erro máximo garantido.
     * Uma estimativa dinâmica futura pode substituir este valor na criação
     * da referência, preservando a compensação das referências anteriores.
     * ponytail: atraso fixo; substituir por estimativa por quadro se as
     * medições exigirem maior precisão.
     */
    constexpr uint32_t reference_delay_ms = 162;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Resultado explícito; nenhuma saída numérica é válida fora de OK. */
    enum class STATUS {
        OK,
        INVALID_ARGUMENT,
        NO_REFERENCE,
        BOOT_MISMATCH,
        OUT_OF_RANGE,
        STORAGE_ERROR,
        COUNTER_EXHAUSTED
    };

    /** @brief Instante relativo; boot_id zero significa identificação indisponível. */
    struct EVENT_STAMP {
        uint64_t boot_id;
        uint64_t monotonic_ms;
    };

    /**
     * @brief Relação entre Unix e o instante de recepção no mesmo boot.
     * @details unix_s representa o início do TX; delay_ms aproxima o tempo
     * até a recepção. Reconstrução não altera eventos nem referências.
     * O timestamp próprio do Payload é independente e não é substituído.
     * Cada referência conserva sua compensação para análise posterior.
     * São tipos internos, não formatos de arquivo nem memória serializável.
     */
    struct TIME_REFERENCE {
        EVENT_STAMP reception;
        uint32_t unix_s;
        uint32_t delay_ms;
    };

    /**
     * @brief Acesso mínimo ao contador persistente, substituível no teste nativo.
     * @details Read retorna zero somente quando ainda não existe contador.
     * Write retorna true somente após confirmar a persistência. Erros não
     * podem ser convertidos em contador zero. Não chamar concorrentemente.
     * Context pode ser nullptr quando as operações não precisam dele.
     */
    struct BOOT_STORAGE {
        bool (*read)(
            void* context,
            uint64_t& count
        );
        bool (*write)(
            void* context,
            uint64_t count
        );
        void* context;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Cria uma referência com a compensação configurada nesta compilação.
     * @param reception Instante monotônico da recepção, com boot_id não zero.
     * @param unix_s Segundos Unix recebidos, inclusive zero (época Unix).
     * @param reference Saída; zerada em caso de erro.
     * @return OK ou INVALID_ARGUMENT.
     */
    STATUS CreateReference(
        const EVENT_STAMP& reception,
        uint32_t unix_s,
        TIME_REFERENCE& reference
    );

    /**
     * @brief Reconstrói Unix em ms, inclusive antes da recepção da referência.
     * @details Soma unix_s * 1000 + delay_ms ao intervalo relativo com sinal.
     * Aceita apenas resultados entre zero e INT64_MAX. Aritmética em 64 bits
     * evita a volta de millis() em 32 bits. Não estima deriva ou tempo desligado.
     * A referência é explícita: referências sucessivas não apagam as antigas
     * nem exigem histórico ilimitado em RAM. Ausência de referência deve ser
     * propagada como horário desconhecido aos registros/TM na integração.
     * @param event Evento original com identificação válida do boot.
     * @param reference Referência do mesmo boot; nullptr indica ausência.
     * @param unix_ms Saída; zerada em caso de erro e válida somente com OK.
     * @return Estado da reconstrução.
     */
    STATUS Reconstruct(
        const EVENT_STAMP& event,
        const TIME_REFERENCE* reference,
        int64_t& unix_ms
    );

    /**
     * @brief Reserva o próximo boot sem publicar identificador antes da gravação.
     * @details Primeiro boot confirmado = 1; reinicializações registradas =
     * boot_id - 1. Conta reservas confirmadas, não energizações interrompidas
     * antes da reserva. Identidade é local à placa/NVS, sem garantia após
     * apagamento ou restauração de backup. Não reutilizar IDs já publicados.
     * Chamar uma vez por boot, antes de iniciar os consumidores.
     * @param storage Leitura e escrita durável do contador, sem acesso ao SD.
     * @param boot_id Saída; zero em caso de erro.
     * @return OK, INVALID_ARGUMENT, STORAGE_ERROR ou COUNTER_EXHAUSTED.
     */
    STATUS StartBoot(
        const BOOT_STORAGE& storage,
        uint64_t& boot_id
    );

} // namespace obc_time
