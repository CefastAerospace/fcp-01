/**
 * @file Pinout.h
 * @brief Mapeamento dos GPIOs do OBC-DH conforme .agents/PINAGEM.md.
 *
 * @details Contém apenas os números dos GPIOs, sem configurar periféricos
 * ou definir níveis ativos.
 */

// Include guard
#pragma once

/**
 * @brief Constantes de pinagem do ESP32, sem estado global mutável.
 */
namespace pinout {

    // ─── DICIONÁRIO ───────────────────────────────────────────────────

    /*
     * Strapping pin: GPIO cujo nível lógico é amostrado durante o reset
     * para selecionar opções de inicialização/configuração do chip.
     * Depois dessa amostragem, pode exercer sua função normal de GPIO.
     * No ESP32 clássico, são GPIOs 0, 2, 5, 12 e 15. Os periféricos
     * conectados não devem impor níveis incompatíveis com o boot esperado.
     */

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    /*
    * "constexpr" é a mesma coisa de "static const".
    */

    // UART0: interface umbilical de bancada pela conexão USB da placa.
    constexpr int uart0_tx = 1; // Transmissão da UART0.
    constexpr int uart0_rx = 3; // Recepção da UART0.

    // UART2: comunicação com o Payload.
    constexpr int uart2_tx = 17; // Transmissão de solicitações ao Payload.
    constexpr int uart2_rx = 16; // Recepção dos dados ADS-B do Payload.

    // I2C: barramento compartilhado.
    constexpr int i2c_sda = 21; // Dados do barramento I2C.
    constexpr int i2c_scl = 22; // Clock do barramento I2C.

    // Rádio LoRa SX1276: VSPI e sinais de controle.
    constexpr int radio_cs   = 5;  // Seleção do rádio (strapping pin).
    constexpr int radio_dio0 = 4;  // Entrada de eventos de recepção e transmissão do rádio.
    constexpr int radio_rst  = 14; // Reset físico do rádio.
    constexpr int radio_sck  = 18; // Clock do VSPI do rádio.
    constexpr int radio_miso = 19; // Recepção de dados do rádio pelo VSPI.
    constexpr int radio_mosi = 23; // Envio de dados ao rádio pelo VSPI.

    // Cartão SD: HSPI independente do barramento do rádio.
    constexpr int sd_cs   = 13; // Seleção do cartão SD.
    constexpr int sd_sck  = 15; // Clock do HSPI do SD (strapping pin).
    constexpr int sd_mosi = 32; // Envio de dados ao cartão SD pelo HSPI.
    constexpr int sd_miso = 35; // Recepção do SD (somente entrada, sem pulls internos).

    // Motor: responsabilidade de atuação ainda a conciliar com ADCS.
    constexpr int motor_en    = 12; // Habilitação do driver do motor (strapping pin).
    constexpr int motor_pwm_u = 25; // PWM da fase U do driver do motor.
    constexpr int motor_pwm_v = 26; // PWM da fase V do driver do motor.
    constexpr int motor_pwm_w = 27; // PWM da fase W do driver do motor.

    // Burn Wire: autorização, temporização e estados em reset ainda pendentes.
    constexpr int burn_wire_trigger = 33; // Gatilho do MOSFET de liberação das antenas.

    /*
     * Free pins:
     * - GPIOs 34, 36 e 39: livres no mapeamento de PINAGEM.md, somente
     *   para entrada e sem pull-up/pull-down interno. Confirmar sua
     *   disponibilidade no módulo e na placa antes de conectar sensores.
     *
     * Alocados; não considerar livres:
     * - UART0: GPIOs 1 e 3; UART2: GPIOs 16 e 17.
     * - I2C compartilhado: GPIOs 21 e 22.
     * - Rádio: GPIOs 4, 5, 14, 18, 19 e 23.
     * - SD: GPIOs 13, 15, 32 e 35.
     * - Motor: GPIOs 12, 25, 26 e 27; Burn Wire: GPIO 33.
     *
     * Reservados ou sujeitos a restrições; não presumir livres:
     * - GPIOs 6 a 11: normalmente conectados à memória flash do módulo.
     * - GPIOs 0 e 2: sem alocação neste mapeamento, mas são strapping pins;
     *   GPIO 0 também participa da seleção do modo de gravação do firmware.
     * - GPIOs 16 e 17: já alocados; disponibilidade depende de flash/PSRAM.
     * - GPIOs 37 e 38: não presumir acesso nos conectores da placa adotada.
     * - 3V3 e VIN/5V, quando presentes: alimentação, não são GPIOs.
     * - GND: referência elétrica/terra, não é GPIO nem entrada disponível.
     * - EN da placa: habilitação/reset do ESP32, não é GPIO. É distinto
     *   de motor_en, que corresponde ao GPIO 12 do driver do motor.
     *
     * A ausência de um pino nesta lista não comprova que esteja livre.
     * Quantidade, posição e ligação dos pinos de alimentação, GND e EN
     * dependem do esquema da variante real do DevKit, ainda a confirmar.
     */

} // namespace pinout
