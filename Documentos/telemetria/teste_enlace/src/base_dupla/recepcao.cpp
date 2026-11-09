#include "recepcao.h"
#include <math.h>
#include <string.h>
#include "ConfigEnlace.h"
#include "QuadroReyax.h"
#include "config_base.h"
#include "pinos.h"
#include "radio_rx.h"
#include "saida.h"

static const uint32_t PERIODO_LEGIVEL_MS     = 1000;
static const uint32_t PERIODO_ESPERA_MS      = 5000;
static const uint32_t LED_ACESO_MS           = 30;
static const uint32_t RETENTAR_SX1276_MS     = 2000;
static const uint32_t RETENTAR_REYAX_MS      = 10000;  // bloqueia ~3 s: so antes do 1o pacote

static QueueHandle_t  filaSaida = nullptr;
static portMUX_TYPE   trava = portMUX_INITIALIZER_UNLOCKED;
static EstatisticasRx est = {};

static bool         sxOk = false, ryOk = false;
static bool         temSeq = false;
static uint16_t     ultimoSeq = 0;
static ItemRecebido ultimo;
static uint32_t     tLegivel = 0, tEspera = 0, tLed = 0, tRetSx = 0, tRetRy = 0;
static bool         ledAceso = false;

EstatisticasRx recepcaoEstatisticas() {
  portENTER_CRITICAL(&trava);
  EstatisticasRx copia = est;
  portEXIT_CRITICAL(&trava);
  return copia;
}

#define CONTAR(campo) do { portENTER_CRITICAL(&trava); est.campo++; portEXIT_CRITICAL(&trava); } while (0)

// Conta perdas pelos saltos na sequencia (ja fundida). true se o CanSat reiniciou.
static bool contarSequencia(uint16_t seq) {
  bool reiniciou = false;
  portENTER_CRITICAL(&trava);
  if (temSeq) {
    const uint16_t salto = (uint16_t)(seq - ultimoSeq);
    if (salto >= 1 && salto < 1000) est.perdidos += salto - 1;
    else if (salto != 0) reiniciou = true;
  }
  ultimoSeq = seq;
  temSeq = true;
  portEXIT_CRITICAL(&trava);
  return reiniciou;
}

// Um registro por pacote, com o que cada receptor viu
static void processarFundido(const ItemFundido& f) {
  int melhor = -1;            // copia com maior SNR
  uint32_t tPrimeiro = 0;     // chegada da primeira copia
  for (int r = 0; r < N_RECEPTORES; r++) {
    if (!(f.mascara & (1u << r))) continue;
    if (melhor < 0 || f.rx[r].snr > f.rx[melhor].snr) melhor = r;
    if (tPrimeiro == 0 || (int32_t)(f.rx[r].tMs - tPrimeiro) < 0) tPrimeiro = f.rx[r].tMs;
  }

  ItemRecebido item;
  item.pacote     = f.pacote;
  item.rssi       = f.rx[melhor].rssi;
  item.snr        = f.rx[melhor].snr;
  item.tBaseMs    = tPrimeiro;
  item.receptores = f.mascara;
  for (int r = 0; r < N_RECEPTORES; r++) {
    item.rssiRx[r] = f.rx[r].rssi;  // NAN onde o bit da mascara esta desligado
    item.snrRx[r]  = f.rx[r].snr;
  }

  const bool reiniciou = contarSequencia(f.pacote.seq);
  portENTER_CRITICAL(&trava);
  est.recebidos++;
  est.tUltimoMs = millis();
  est.temPacote = true;
  const uint32_t perdidos = est.perdidos;
  portEXIT_CRITICAL(&trava);
  if (reiniciou) Serial.println("# [BASE] sequencia reiniciou (o CanSat reiniciou?)");

  if (xQueueSend(filaSaida, &item, 0) != pdTRUE) CONTAR(descartadosFila);

  ultimo = item;
  saidaCsv(item, perdidos);
  digitalWrite(PIN_LED, HIGH);
  ledAceso = true;
  tLed = millis();
}

// ---- Receptor 1: SX1276. Recebe o quadro INTEIRO: tira o cabecalho Reyax aqui.
static void lerSx1276() {
  uint8_t buf[RYLR_QUADRO_MAX];  // a base de voo usava 64: o quadro Base64 tem 93 bytes
  size_t  len = 0;
  float   rssi = 0, snr = 0;
  switch (radioLer(buf, sizeof(buf), len, rssi, snr)) {
    case RX_OK: {
      CabecalhoRylr    cab;
      const uint8_t*   payload = nullptr;
      PacoteTelemetria p;
      // 1) cabecalho: len >= 5 e byte 4 == len - 5   2) origem = CanSat
      // 3) bytes 5..: Base64 (ou binario) -> PacoteTelemetria -> magic, versao, CRC16
      if (rylrAbrir(buf, len, cab, payload) && cab.src == RYLR_END_CANSAT &&
          tlmDecodificar(payload, cab.len, p)) {
        fusaoEntrada(RECEPTOR_SX1276, p, rssi, snr, millis());
      } else {
        CONTAR(invalidos);
        Serial.printf("# [SX1276] quadro invalido ou de outra rede (%u bytes, RSSI %.0f dBm)\n",
                      (unsigned)len, rssi);
      }
      break;
    }
    case RX_ERRO_CRC: CONTAR(errosCrc);   break;
    case RX_ERRO:     CONTAR(errosRadio); break;
    case RX_NADA:     break;
  }
}

// ---- Receptor 2: RYLR998. O proprio modulo filtrou e tirou o cabecalho.
static void lerReyax() {
  uint16_t origem;
  uint8_t  payload[RYLR_PAYLOAD_MAX];
  size_t   len;
  int      rssi, snr;
  while (reyaxLer(origem, payload, sizeof(payload), len, rssi, snr)) {
    PacoteTelemetria p;
    if (origem == RYLR_END_CANSAT && tlmDecodificar(payload, len, p)) {
      fusaoEntrada(RECEPTOR_RYLR998, p, (float)rssi, (float)snr, millis());
    } else {
      CONTAR(invalidosReyax);
      Serial.printf("# [REYAX] +RCV invalido (origem %u, %u bytes, RSSI %d dBm)\n",
                    (unsigned)origem, (unsigned)len, rssi);
    }
  }
}

// Os dois radios iniciam de forma independente: a base funciona com qualquer um deles.
static void iniciarRadios(uint32_t agora, bool primeiraVez) {
  if (!sxOk && (primeiraVez || agora - tRetSx >= RETENTAR_SX1276_MS)) {
    tRetSx = agora;
    sxOk = radioIniciar();
    if (sxOk) {
      Serial.printf("# [SX1276] escutando em %.1f MHz SF%d BW%.0f kHz sync 0x%02X preambulo %d\n",
                    LORA_FREQ_MHZ, LORA_SF, LORA_BW_KHZ, LORA_SYNC_WORD, LORA_PREAMBULO);
    }
  }
  // O RYLR998 leva ~1-3 s para (re)configurar: so tenta de novo antes do primeiro pacote
  if (!ryOk && (primeiraVez || (!est.temPacote && agora - tRetRy >= RETENTAR_REYAX_MS))) {
    tRetRy = agora;
    ryOk = reyaxIniciar();
  }
  portENTER_CRITICAL(&trava);
  est.radioOk = sxOk;
  est.reyaxOk = ryOk;
  portEXIT_CRITICAL(&trava);
}

static void tarefaRecepcao(void*) {
  fusaoIniciar(JANELA_FUSAO_MS);
  iniciarRadios(millis(), true);

  for (;;) {
    const uint32_t agora = millis();
    if (!sxOk || !ryOk) iniciarRadios(agora, false);

    if (sxOk) lerSx1276();
    if (ryOk) lerReyax();

    // Fusao: cada pacote sai uma vez, quando os dois entregaram ou a janela venceu
    ItemFundido fundido;
    while (fusaoSaida(millis(), fundido)) processarFundido(fundido);

    const EstatisticasFusao ef = fusaoEstatisticas();
    const EstatisticasReyax er = reyaxEstatisticas();
    portENTER_CRITICAL(&trava);
    est.fusao = ef;
    est.reyax = er;
    portEXIT_CRITICAL(&trava);

    if (ledAceso && agora - tLed >= LED_ACESO_MS) {
      digitalWrite(PIN_LED, LOW);
      ledAceso = false;
    }

    // Resumo legivel pela USB 1x por segundo, ou alerta de perda de sinal
    const EstatisticasRx e = recepcaoEstatisticas();
    if (e.temPacote && agora - tLegivel >= PERIODO_LEGIVEL_MS) {
      tLegivel = agora;
      if (agora - e.tUltimoMs < TLM_TIMEOUT_SINAL_MS) {
        saidaLegivel(ultimo.pacote, ultimo.rssi, ultimo.snr, e.recebidos, e.perdidos);
        saidaReceptores(e, ultimo);
      } else {
        Serial.printf("# [BASE] SEM SINAL ha %.1f s | ultima posicao %.7f,%.7f | alt max %.1f m\n",
                      (agora - e.tUltimoMs) / 1000.0f, ultimo.pacote.latE7 / 1e7,
                      ultimo.pacote.lonE7 / 1e7, ultimo.pacote.altMaxCm / 100.0f);
      }
    }
    if (!e.temPacote && agora - tEspera >= PERIODO_ESPERA_MS) {
      tEspera = agora;
      Serial.printf("# [BASE] aguardando o CanSat... SX1276 %s (erros CRC %lu, invalidos %lu) |"
                    " RYLR998 %s (+RCV %lu, invalidos %lu)\n",
                    e.radioOk ? "ok" : "FALHA", (unsigned long)e.errosCrc,
                    (unsigned long)e.invalidos, e.reyaxOk ? "ok" : "FALHA",
                    (unsigned long)e.reyax.linhasRcv, (unsigned long)e.invalidosReyax);
    }

    vTaskDelay(pdMS_TO_TICKS(2));  // libera a CPU; SX1276 e UART guardam os dados
  }
}

bool recepcaoIniciar(QueueHandle_t fila) {
  filaSaida = fila;
  pinMode(PIN_LED, OUTPUT);
  // Nucleo 1, prioridade 3: acima do loop() do Arduino (prioridade 1)
  return xTaskCreatePinnedToCore(tarefaRecepcao, "recepcao", 8192, nullptr, 3, nullptr, 1) == pdPASS;
}
