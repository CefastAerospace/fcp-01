/** @file DeliveryLogs.h
 * @brief CLI local de listagem, extração binária e retomada do diário INT-006.
 */
#pragma once
#include "ObcDelivery.h" // Scanner, cache e Storage de produção.
#include <cstdlib> // strtoul e limites de argumentos numéricos.

/** @brief Executa list, read ou recover sobre diretório de arquivos fechados. */
int main(
    int argc,
    char** argv
);
