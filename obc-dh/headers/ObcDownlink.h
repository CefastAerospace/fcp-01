/**
 * @file ObcDownlink.h
 * @brief Adaptação das entregas e extração limitada para o contrato TT&C.
 */
#pragma once

#include "ObcDelivery.h" // Entregas e callbacks existentes de listagem/leitura.
#include "TtcProtocol.h" // Modelos e geradores do contrato compartilhado.
#include <limits> // Limite representável na altitude float do TT&C.

namespace obc_downlink {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    // Provisório para desenvolvimento; o valor operacional exige medição conjunta.
    constexpr uint64_t confirmation_timeout_ms = 10000;
    constexpr uint64_t log_timeout_ms = 1000;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Resposta por cópia; length zero representa prazo local esgotado. */
    struct LOG_RESPONSE {
        uint16_t sequence;
        ttc::LOG_REQUEST request;
        size_t length;
        char json[ttc::max_body_len + 1];
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Mapeia somente campos disponíveis; rejeita altitude não representável. */
    bool MapAircraft(
        const obc_delivery::DELIVERY& delivery,
        ttc::AIRCRAFT_DATA& out,
        uint32_t& available
    );

    /** @brief Produz lg de erro preservando operação, identificação e destino. */
    void BuildLogFailure(
        uint16_t sequence,
        const ttc::LOG_REQUEST& request,
        ttc::LOG_ERROR error,
        LOG_RESPONSE& out
    );

    /**
     * @brief Executa um pedido validado usando o proprietário dos callbacks.
     * @details Lista até quatro candidatos e deixa o gerador limitar o quadro.
     * Não envia pacotes nem aceita bytes de uma leitura malsucedida.
     */
    void BuildLogResponse(
        uint16_t sequence,
        const ttc::LOG_REQUEST& request,
        const obc_delivery::READER_IO& io,
        LOG_RESPONSE& out
    );

    // ─── CLASSES ──────────────────────────────────────────────────────

    /**
     * @brief Caixa de um pedido entre atendimento TC e proprietário do SD.
     * @details O adaptador protege cada chamada, sem executar I/O sob lock.
     * Após expirar em execução, só Complete libera a caixa; resultado tardio
     * nunca pode virar resposta de um pedido novo. Não contém alocação dinâmica.
     */
    class LogMailbox {
    public:
        /** @brief Reserva uma caixa livre; conserva pedido e instante por cópia. */
        bool Submit(
            uint16_t sequence,
            const ttc::LOG_REQUEST& request,
            uint64_t now_ms
        );
        /** @brief Somente o proprietário do SD retira um pedido pendente. */
        bool Begin(
            uint16_t& sequence,
            ttc::LOG_REQUEST& request
        );
        /** @brief Publica resultado ou descarta trabalho que expirou em execução. */
        void Complete(
            const LOG_RESPONSE& response
        );
        /** @brief Retira resultado uma vez; prazo esgotado entrega length zero. */
        bool Take(
            uint64_t now_ms,
            LOG_RESPONSE& out
        );

    private:
        enum class STATE { FREE, PENDING, RUNNING, READY, ABANDONED };
        STATE state_ = STATE::FREE;
        uint64_t submitted_ms_ = 0;
        LOG_RESPONSE response_ = {};
    };

} // namespace obc_downlink
