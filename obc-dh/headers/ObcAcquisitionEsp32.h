/**
 * @file ObcAcquisitionEsp32.h
 * @brief Converte resultados do executor para os códigos persistidos em FCP4.
 */
#pragma once

#include "I2cBus.h" // Resultado do executor.
#include "ObcRecordCodec.h" // Códigos estáveis do registro.

namespace obc_acquisition {

    /** @brief Mapeia o domínio I2C sem depender dos ordinais do executor. */
    inline obc_record::ACQUISITION_RESULT MapResult(
        i2c::STATUS result
    ) {
        using FROM = i2c::STATUS;
        using TO = obc_record::ACQUISITION_RESULT;
        switch (result) {
            case FROM::OK: return TO::OK;
            case FROM::NOT_INITIALIZED: return TO::NOT_INITIALIZED;
            case FROM::INVALID_ARGUMENT: return TO::INVALID_ARGUMENT;
            case FROM::INVALID_CONTEXT: return TO::INVALID_CONTEXT;
            case FROM::QUEUE_FULL: return TO::QUEUE_FULL;
            case FROM::QUEUE_TIMEOUT: return TO::QUEUE_TIMEOUT;
            case FROM::OPERATION_FAILED: return TO::OPERATION_FAILED;
            case FROM::DEVICE_NACK: return TO::DEVICE_NACK;
            case FROM::TRANSFER_TIMEOUT: return TO::TRANSFER_TIMEOUT;
            case FROM::BUS_ERROR: return TO::BUS_ERROR;
            case FROM::READ_INCOMPLETE: return TO::READ_INCOMPLETE;
            case FROM::SERVICE_RECOVERING: return TO::SERVICE_RECOVERING;
            case FROM::SERVICE_UNAVAILABLE: return TO::SERVICE_UNAVAILABLE;
        }
        return TO::OPERATION_FAILED;
    }

} // namespace obc_acquisition
