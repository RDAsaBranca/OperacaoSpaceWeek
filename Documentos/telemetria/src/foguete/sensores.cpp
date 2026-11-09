#include "sensores.h"
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BNO055.h>
#include <TinyGPSPlus.h>
#include "Protocolo.h"
#include "pinos.h"
#include "log.h"

static Adafruit_BMP280 bmp(&Wire);

// O BNO055 pode estar em 0x28 (Adafruit) ou 0x29 (comum nas placas GY-BNO055)
static Adafruit_BNO055 bnoA(-1, BNO055_ADDRESS_A, &Wire);
static Adafruit_BNO055 bnoB(-1, BNO055_ADDRESS_B, &Wire);
static Adafruit_BNO055* bno = nullptr;
static uint8_t enderecoBno = 0;

static TinyGPSPlus     gps;
static HardwareSerial& gpsSerial = Serial1;

static bool     baroOk = false;
static uint8_t  enderecoBmp = 0, chipIdBmp = 0;
static uint32_t ultimoByteGpsMs = 0;
static uint32_t ultimoAdcMv = 0;   // tensao lida no pino, antes do divisor

static const float G_PADRAO = 9.80665f;

static bool pressaoPlausivel(float p) {
  return p > 10000.0f && p < 120000.0f;  // descarta NaN e lixo de barramento
}

// ---------------- BMP280 ----------------
static bool iniciarBmp() {
  // Modulos de 4 pinos costumam vir em 0x76. Alguns "BMP280" sao na verdade
  // BME280 (chip ID 0x60), que tem os mesmos registros de pressao/temperatura.
  const uint8_t enderecos[] = {0x76, 0x77};
  const uint8_t chipIds[]   = {BMP280_CHIPID, 0x60};
  for (uint8_t e : enderecos) {
    for (uint8_t id : chipIds) {
      if (bmp.begin(e, id)) {
        enderecoBmp = e;
        chipIdBmp = id;
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X2,   // temperatura
                        Adafruit_BMP280::SAMPLING_X8,   // pressao
                        Adafruit_BMP280::FILTER_X4,
                        Adafruit_BMP280::STANDBY_MS_1);
        return true;
      }
    }
  }
  return false;
}

// ---------------- BNO055 ----------------
static void bnoEscrever(uint8_t reg, uint8_t valor) {
  Wire.beginTransmission(enderecoBno);
  Wire.write(reg);
  Wire.write(valor);
  Wire.endTransmission();
  delay(10);
}

static bool iniciarBno() {
#if IMU_MODO_ALTO_G
  const adafruit_bno055_opmode_t modoInicial = OPERATION_MODE_CONFIG;
#else
  const adafruit_bno055_opmode_t modoInicial = OPERATION_MODE_NDOF;
#endif
  if (bnoA.begin(modoInicial)) {
    bno = &bnoA;
    enderecoBno = BNO055_ADDRESS_A;
  } else if (bnoB.begin(modoInicial)) {
    bno = &bnoB;
    enderecoBno = BNO055_ADDRESS_B;
  } else {
    return false;
  }

#if IMU_MODO_ALTO_G
  // Nos modos de fusao o BNO055 fixa o acelerometro em +-4 g. No modo AMG
  // (dados crus) da para escolher o range: aqui +-16 g com banda de 62,5 Hz.
  bnoEscrever(0x07, 1);     // PAGE_ID = 1 (registros de configuracao)
  bnoEscrever(0x08, 0x0F);  // ACC_Config: range 16 g | banda 62,5 Hz | modo normal
  bnoEscrever(0x07, 0);     // PAGE_ID = 0
  bno->setMode(OPERATION_MODE_AMG);
#endif
  return true;
}

// ---------------- Inicializacao ----------------
void sensoresIniciar() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_FREQ_HZ);

  baroOk = iniciarBmp();
  if (baroOk) {
    LOG("[BMP280] ok: encontrado no endereco 0x%02X (chip 0x%02X)\n", enderecoBmp, chipIdBmp);
  } else {
    LOG("[BMP280] FALHA: nao respondeu em 0x76 nem 0x77 (confira SDA=GPIO%d, SCL=GPIO%d e 3V3)\n",
        PIN_I2C_SDA, PIN_I2C_SCL);
  }

  bool imuOk = iniciarBno();
  if (imuOk) {
    LOG("[BNO055] ok: encontrado no endereco 0x%02X, %s\n", enderecoBno,
        IMU_MODO_ALTO_G ? "modo AMG +-16 g" : "modo fusao NDOF");
  } else {
    LOG("[BNO055] FALHA: nao respondeu em 0x28 nem 0x29 (confira SDA=GPIO%d, SCL=GPIO%d e 3V3)\n",
        PIN_I2C_SDA, PIN_I2C_SCL);
  }

  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  LOG("[GPS]    UART1 a %d baud (TX do GPS -> GPIO%d). Se ele esta enviando, aparece no diagnostico\n",
      GPS_BAUD, PIN_GPS_RX);
}

void sensoresAtualizarGps() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
    ultimoByteGpsMs = millis();
  }
}

// ---------------- Leitura ----------------
void sensoresLer(LeituraSensores& l) {
  l.status = 0;

  if (baroOk) {
    l.temperaturaC = bmp.readTemperature();
    l.pressaoPa    = bmp.readPressure();
    if (pressaoPlausivel(l.pressaoPa)) l.status |= STATUS_BARO_OK;
  }

  if (bno) {
    imu::Vector<3> a = bno->getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);  // m/s2
    imu::Vector<3> g = bno->getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);      // graus/s
    l.acelG[0] = a.x() / G_PADRAO;
    l.acelG[1] = a.y() / G_PADRAO;
    l.acelG[2] = a.z() / G_PADRAO;
    l.giroDps[0] = g.x();
    l.giroDps[1] = g.y();
    l.giroDps[2] = g.z();
    // Aceleracao exatamente zero nos 3 eixos indica leitura falha no I2C
    if (a.x() != 0 || a.y() != 0 || a.z() != 0) l.status |= STATUS_IMU_OK;

#if !IMU_MODO_ALTO_G
    imu::Quaternion q = bno->getQuat();
    l.quat[0] = q.w();
    l.quat[1] = q.x();
    l.quat[2] = q.y();
    l.quat[3] = q.z();
    float norma = l.quat[0] * l.quat[0] + l.quat[1] * l.quat[1] +
                  l.quat[2] * l.quat[2] + l.quat[3] * l.quat[3];
    if (norma > 0.5f) l.status |= STATUS_ORIENT_OK;

    uint8_t sis = 0, gir = 0, ace = 0, mag = 0;
    bno->getCalibration(&sis, &gir, &ace, &mag);
    l.calibracao = (uint8_t)((sis << 6) | (gir << 4) | (ace << 2) | mag);
#endif
  }

  if (millis() - ultimoByteGpsMs < 2000) l.status |= STATUS_GPS_OK;
  if (gps.location.isValid()) {
    // Mantem a ultima posicao mesmo sem fix: e o que ajuda no resgate
    l.latitude  = gps.location.lat();
    l.longitude = gps.location.lng();
    if (gps.location.age() < 3000) l.status |= STATUS_GPS_FIX;
  }
  if (gps.altitude.isValid()) l.altitudeGpsM = gps.altitude.meters();
  l.satelites = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;

  ultimoAdcMv = analogReadMilliVolts(PIN_VBAT);
  l.bateriaMv = (uint16_t)(ultimoAdcMv * VBAT_FATOR_DIVISOR);
}

float sensoresPressaoMedia(uint16_t amostras) {
  if (!baroOk) return 0;
  delay(300);  // deixa o filtro interno do BMP280 estabilizar
  double soma = 0;
  uint16_t validas = 0;
  for (uint16_t i = 0; i < amostras; i++) {
    float p = bmp.readPressure();
    if (pressaoPlausivel(p)) { soma += p; validas++; }
    delay(20);
  }
  return validas ? (float)(soma / validas) : 0;
}

void sensoresImprimirResumo(Print& s) {
  if (baroOk) s.printf("# BMP280: ok (endereco 0x%02X, chip 0x%02X)\n", enderecoBmp, chipIdBmp);
  else        s.println("# BMP280: NAO ENCONTRADO");
  if (bno) s.printf("# BNO055: ok (endereco 0x%02X, %s)\n", enderecoBno,
                    IMU_MODO_ALTO_G ? "AMG +-16 g" : "fusao NDOF");
  else     s.println("# BNO055: NAO ENCONTRADO");
  s.printf("# GPS: %s, %lu caracteres NMEA recebidos, %u satelites\n",
           (millis() - ultimoByteGpsMs < 2000) ? "recebendo" : "SEM DADOS",
           (unsigned long)gps.charsProcessed(),
           gps.satellites.isValid() ? (unsigned)gps.satellites.value() : 0u);
}

void sensoresImprimirDiagnostico(const LeituraSensores& l, Print& s) {
  // ---- BMP280 ----
  if (!baroOk) {
    s.println("[BMP280] FALHA: nao encontrado na inicializacao (aperte RESET depois de ligar)");
  } else if (!(l.status & STATUS_BARO_OK)) {
    s.printf("[BMP280] FALHA: leitura invalida (%.0f Pa), I2C com problema?\n", l.pressaoPa);
  } else {
    s.printf("[BMP280] ok    | %.0f Pa | %.2f C\n", l.pressaoPa, l.temperaturaC);
  }

  // ---- BNO055 ----
  if (!bno) {
    s.println("[BNO055] FALHA: nao encontrado na inicializacao (aperte RESET depois de ligar)");
  } else if (!(l.status & STATUS_IMU_OK)) {
    s.println("[BNO055] FALHA: leitura zerada, I2C com problema?");
  } else {
    s.printf("[BNO055] ok    | acel %+.2f %+.2f %+.2f g | giro %+.1f %+.1f %+.1f graus/s",
             l.acelG[0], l.acelG[1], l.acelG[2], l.giroDps[0], l.giroDps[1], l.giroDps[2]);
#if !IMU_MODO_ALTO_G
    const unsigned c = l.calibracao;
    if (l.status & STATUS_ORIENT_OK) {
      s.printf(" | quat %+.3f %+.3f %+.3f %+.3f", l.quat[0], l.quat[1], l.quat[2], l.quat[3]);
    } else {
      s.print(" | sem orientacao");
    }
    s.printf(" | cal S%u G%u A%u M%u", (c >> 6) & 3u, (c >> 4) & 3u, (c >> 2) & 3u, c & 3u);
#endif
    s.println();
  }

  // ---- GPS ----
  const unsigned long frasesOk = gps.passedChecksum(), frasesErro = gps.failedChecksum();
  if (!(l.status & STATUS_GPS_OK)) {
    if (gps.charsProcessed() == 0) {
      s.printf("[GPS]    FALHA: nenhum byte recebido (TX do GPS -> GPIO%d? alimentacao?)\n", PIN_GPS_RX);
    } else {
      s.println("[GPS]    FALHA: parou de enviar dados ha mais de 2 s");
    }
  } else if (frasesOk == 0) {
    s.printf("[GPS]    AVISO: chegam bytes, mas nenhuma frase NMEA valida (%lu com erro). Baud errado?\n",
             frasesErro);
  } else {
    s.printf("[GPS]    ok    | %lu frases validas, %lu com erro | %u satelites", frasesOk, frasesErro,
             (unsigned)l.satelites);
    if (l.status & STATUS_GPS_FIX) {
      s.printf(" | posicao %.7f, %.7f | alt %.0f m", l.latitude, l.longitude, l.altitudeGpsM);
    } else {
      s.print(" | sem posicao ainda (precisa de ceu aberto; leva de 30 s a alguns minutos)");
    }
    s.println();
  }

  // ---- Bateria (ADC) ----
  const float v = l.bateriaMv / 1000.0f;
  s.printf("[BAT]    %.2f V (pino GPIO%d: %lu mV)", v, PIN_VBAT, (unsigned long)ultimoAdcMv);
  if (v < 2.5f || v > 4.5f) s.print(" | AVISO: fora da faixa de uma celula Li-ion (bateria e divisor ligados?)");
  s.println();
}
