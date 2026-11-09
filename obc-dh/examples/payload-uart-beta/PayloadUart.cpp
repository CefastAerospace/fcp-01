#include <Arduino.h>
#include <stdint.h>
#include <stdio.h>
#include "Pinout.h"
#include "PayloadUart.h"

namespace {
HardwareSerial uartPayload(2);
uint32_t proximoSeqId = 1;
uint32_t esperadoSeqId = 1;
char bufferRecepcao[128];
size_t tamanhoRecepcao = 0;
bool descartandoMensagem = false;

void processarMensagem(const char* mensagem) {
  char icao[7] = {};
  int altitude = 0;
  int velocidade = 0;
  unsigned long seq = 0;
  int bytesLidos = 0;

  const int campos = sscanf(
      mensagem,
      "{\"icao\":\"%6[0-9A-Fa-f]\",\"alt\":%d,\"vel\":%d,\"seq_id\":%lu}%n",
      icao, &altitude, &velocidade, &seq, &bytesLidos);

  if (campos != 4 || icao[0] == '\0' || mensagem[bytesLidos] != '\0') {
    Serial.println("ERRO: pacote JSON invalido para este prototipo.");
    return;
  }

  const uint32_t recebido = static_cast<uint32_t>(seq);
  if (recebido > esperadoSeqId) {
    Serial.printf("PACOTE(S) PERDIDO(S): esperado %lu, recebido %lu\n",
                  static_cast<unsigned long>(esperadoSeqId), seq);
    Serial.printf("NACK SIMULADO: solicitar pacote %lu\n",
                  static_cast<unsigned long>(esperadoSeqId));
  } else if (recebido < esperadoSeqId) {
    Serial.printf("Pacote antigo ou repetido: %lu\n", seq);
    return;
  }

  esperadoSeqId = recebido + 1;
  Serial.printf("Recebido: ICAO=%s | Altitude=%d pes | Velocidade=%d nos | SeqID=%lu\n",
                icao, altitude, velocidade, seq);
}
}  // namespace

void iniciarUart() {
  uartPayload.begin(115200, SERIAL_8N1, pinout::uart2_rx, pinout::uart2_tx);
  Serial.println("UART2 iniciada: RX=GPIO16, TX=GPIO17, 115200 8N1");
  Serial.println("Teste de bancada: ligar GPIO17 ao GPIO16 (loopback).");
}

void enviarTeste() {
  // O salto do 2 para o 4 testa a deteccao de perda de pacote.
  if (proximoSeqId == 3) ++proximoSeqId;

  char mensagem[100];
  const int n = snprintf(mensagem, sizeof(mensagem),
                         "{\"icao\":\"E4953A\",\"alt\":39000,\"vel\":489,\"seq_id\":%lu}\n",
                         static_cast<unsigned long>(proximoSeqId));
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(mensagem)) {
    Serial.println("ERRO: mensagem maior que o buffer de envio.");
    return;
  }
  uartPayload.print(mensagem);
  Serial.printf("Enviado SeqID=%lu\n", static_cast<unsigned long>(proximoSeqId));
  ++proximoSeqId;
}

void receberDados() {
  while (uartPayload.available() > 0) {
    const char caractere = static_cast<char>(uartPayload.read());
    if (caractere == '\r') continue;
    if (caractere == '\n') {
      if (!descartandoMensagem && tamanhoRecepcao > 0) {
        bufferRecepcao[tamanhoRecepcao] = '\0';
        processarMensagem(bufferRecepcao);
      }
      tamanhoRecepcao = 0;
      descartandoMensagem = false;
      continue;
    }
    if (descartandoMensagem) continue;
    if (tamanhoRecepcao >= sizeof(bufferRecepcao) - 1) {
      Serial.println("ERRO: mensagem maior que o buffer de recepcao. Descartando.");
      tamanhoRecepcao = 0;
      descartandoMensagem = true;
      continue;
    }
    bufferRecepcao[tamanhoRecepcao++] = caractere;
  }
}
