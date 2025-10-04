#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <MFRC522.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>
#include <SPI.h>

// ==================== DEFINIÇÕES DE PINOS ====================
#define BUZZER_PIN 12
#define RELE_1 14
#define RELE_2 26
#define RELE_3 27
#define RELE_4 25
#define SS_PIN 5
#define RST_PIN 15
#define DHT_PIN 4
#define DHTTYPE DHT22
#define LDR_PIN 35
#define IR_SEND_PIN 33
#define IR_RECEIVE_PIN 32

// ==================== CONSTANTES IR ====================
#define IR_PROTOCOLO NEC
#define IR_ENDERECO 0xFC00
#define IR_POWER 0x85
#define IR_UMIDIFICAR 0x87
#define IR_VELOCIDADE 0x84
#define IR_TIMER 0x86
#define IR_ALETA_VERTICAL 0x83
#define IR_ALETA_HORIZONTAL 0x82

// ==================== CONSTANTES DE TIMING ====================
#define DEBOUNCE_ENVIAR 300
#define DEBOUNCE_RECEBER 500
#define JANELA_ECO 100
#define TIMEOUT_CONFIRMACAO 500
#define DEBUG_SERIAL 1
#define LIMIAR_LDR_ESCURIDAO 400
#define HISTERESE_LDR 100

// ==================== INTERVALOS ====================
const unsigned long INTERVALO_DHT = 5000;
const unsigned long INTERVALO_DADOS = 5000;
const unsigned long INTERVALO_LDR = 5000;
const unsigned long INTERVALO_CLIMA_AUTO = 5000;
const unsigned long INTERVALO_COMANDOS = 3000;
const unsigned long INTERVALO_DEBUG = 5000;
const unsigned long INTERVALO_PREF_CHECK = 30000;
unsigned long tempos[8] = {0};

// ==================== OBJETOS GLOBAIS ====================
MFRC522 mfrc522(SS_PIN, RST_PIN);
DHT dht(DHT_PIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==================== CONFIGURAÇÃO DE REDE ====================
const char *ssid = "esp32";
const char *password = "123654123";
const char *serverUrl = "http://192.168.137.1:3000/esp32/";

// ==================== ESTRUTURAS DE DADOS ====================
struct DadosSensores {
  float temperatura = 0;
  float humidade = 0;
  int luminosidade = 0;
  int valorLDR = 0;
  bool dadosValidos = false;
} sensores;

struct {
  bool ligado : 1;
  bool umidificando : 1;
  bool aletaV : 1;
  bool aletaH : 1;
  uint8_t velocidade : 2;
  uint8_t ultimaVel : 2;
  uint8_t timer : 3;
  uint8_t reservado : 5;
  unsigned long ultimaAtualizacao;
} clima;

struct {
  int total = 0;
  String tags[10];
  bool estado[10];
  int count = 0;
  float tempPref = 25.0;
  int lumPref = 50;
  bool prefsAtualizadas = false;
} pessoas;

struct {
  bool modoManualIlum : 1;
  bool modoManualClima : 1;
  bool ilumAtiva : 1;
  bool irPausa : 1;
  bool comandoIR : 1;
  bool comandoApp : 1;
  bool wifiOk : 1;
  bool erroSensor : 1;
  bool erroConexao : 1;
  bool debug : 1;
  bool atualizandoPref : 1;
  bool reservado : 5;
} flags = {false, false, false, false, false, false, false, false, false, false, false, false};

enum EstadoIR {
  IR_OCIOSO,
  IR_ENVIANDO,
  IR_AGUARDANDO_CONFIRMACAO
};

struct {
  EstadoIR estado = IR_OCIOSO;
  uint8_t comandoPendente = 0;
  unsigned long inicioEnvio = 0;
  bool comandoConfirmado = false;
} controleIR;

// ==================== SÍMBOLOS LCD ====================
uint8_t SIMBOLO_PESSOA[8] = {0x0E, 0x0E, 0x04, 0x1F, 0x04, 0x0A, 0x0A, 0x00};
uint8_t SIMBOLO_TEMP[8] = {0x04, 0x0A, 0x0A, 0x0A, 0x0A, 0x11, 0x1F, 0x0E};
uint8_t SIMBOLO_HUM[8] = {0x04, 0x04, 0x0A, 0x0A, 0x11, 0x11, 0x11, 0x0E};
uint8_t SIMBOLO_LUZ[8] = {0x00, 0x0A, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00};
uint8_t SIMBOLO_WIFI[8] = {0x00, 0x0F, 0x11, 0x0E, 0x04, 0x00, 0x04, 0x00};
uint8_t SIMBOLO_AR[8] = {0x00, 0x0E, 0x15, 0x15, 0x15, 0x0E, 0x00, 0x00};
uint8_t SIMBOLO_OK[8] = {0x00, 0x01, 0x02, 0x14, 0x08, 0x04, 0x02, 0x00};
uint8_t SIMBOLO_ERRO[8] = {0x00, 0x11, 0x0A, 0x04, 0x04, 0x0A, 0x11, 0x00};

// ==================== SONS DO BUZZER ====================
enum SomBuzzer : uint8_t {
  SOM_NENHUM = 0,
  SOM_INICIAR,
  SOM_PESSOA_ENTROU,
  SOM_PESSOA_SAIU,
  SOM_COMANDO,
  SOM_ALERTA,
  SOM_ERRO,
  SOM_CONECTADO,
  SOM_DESCONECTADO,
  SOM_OK
};

// ==================== FUNÇÕES DE DEBUG ====================
inline void debugPrint(const String &msg) {
#if DEBUG_SERIAL
  Serial.println(msg);
#endif
}

void mostrarTelaDebug() {
#if DEBUG_SERIAL
  static unsigned long ultimoDebug = 0;
  unsigned long agora = millis();
  if (agora - ultimoDebug < INTERVALO_DEBUG) return;
  ultimoDebug = agora;

  debugPrint(F("\n--- STATUS DO SISTEMA ---"));
  debugPrint("Temperatura: " + String(sensores.temperatura) + "°C");
  debugPrint("Umidade: " + String(sensores.humidade) + "%");
  debugPrint("LDR: " + String(sensores.valorLDR) + " | Lum: " + String(sensores.luminosidade) + "%");
  debugPrint("Pessoas: " + String(pessoas.total) + " | Temp Pref: " + String(pessoas.tempPref) + "°C");
  debugPrint("Lum Pref: " + String(pessoas.lumPref) + "% | Prefs OK: " + String(pessoas.prefsAtualizadas));
  
  if (pessoas.total > 0) {
    debugPrint("--- TAGS PRESENTES ---");
    for (int i = 0; i < pessoas.count; i++) {
      if (pessoas.estado[i]) {
        debugPrint("Tag " + String(i) + ": " + pessoas.tags[i]);
      }
    }
  }

  debugPrint("\n--- FLAGS ---");
  debugPrint("Manual Ilum/Clima: " + String(flags.modoManualIlum) + "/" + String(flags.modoManualClima));
  debugPrint("Ilum Ativa: " + String(flags.ilumAtiva));
  debugPrint("Erros: Sensor=" + String(flags.erroSensor) + " Conexao=" + String(flags.erroConexao));

  debugPrint("\n--- CLIMA ---");
  debugPrint("Ligado: " + String(clima.ligado) + " | Vel: " + String(clima.velocidade));
  debugPrint("Umidif: " + String(clima.umidificando) + " | Timer: " + String(clima.timer));
  debugPrint("WiFi: " + String(flags.wifiOk ? "OK" : "OFF"));
  debugPrint("-------------------------\n");
#endif
}

// ==================== FUNÇÃO DE SOM ====================
void tocarSom(SomBuzzer tipo) {
  static const uint8_t SONS[][4] PROGMEM = {
    {0, 0, 0, 0},      // SOM_NENHUM
    {20, 10, 5, 3},    // SOM_INICIAR
    {25, 5, 0, 1},     // SOM_PESSOA_ENTROU
    {15, 5, 0, 1},     // SOM_PESSOA_SAIU
    {30, 2, 2, 1},     // SOM_COMANDO
    {40, 3, 3, 3},     // SOM_ALERTA
    {10, 15, 5, 2},    // SOM_ERRO
    {35, 3, 2, 2},     // SOM_CONECTADO
    {15, 10, 5, 1},    // SOM_DESCONECTADO
    {45, 2, 1, 2}      // SOM_OK
  };

  if (tipo == SOM_NENHUM) return;

  uint8_t som[4];
  memcpy_P(som, SONS[tipo], 4);
  
  int freq = som[0] * 100;
  int duracao = som[1] * 10;
  int pausa = som[2] * 10;
  int repeticoes = som[3];

  for (int r = 0; r < repeticoes; r++) {
    int freqAtual = freq + (tipo == SOM_INICIAR ? r * 200 : 0);
    if (freqAtual > 0) {
      int periodo = 1000000 / freqAtual;
      unsigned long inicio = millis();
      while (millis() - inicio < duracao) {
        digitalWrite(BUZZER_PIN, HIGH);
        delayMicroseconds(periodo / 2);
        digitalWrite(BUZZER_PIN, LOW);
        delayMicroseconds(periodo / 2);
      }
    }
    if (pausa > 0) delay(pausa);
  }
}

// ==================== FUNÇÕES LCD ====================
void mostrarErroLCD(const char *erro, bool critico = false) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(7);
  lcd.print(F(" ERRO:"));
  lcd.setCursor(0, 1);
  lcd.print(erro);
  tocarSom(critico ? SOM_ERRO : SOM_ALERTA);
  delay(600);
  if (pessoas.total > 0) atualizarLCD();
}

void mostrarMensagemLCD(uint8_t icone, const char* linha1, const char* linha2, int delayMs = 400) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(icone);
  lcd.print(" ");
  lcd.print(linha1);
  if (linha2) {
    lcd.setCursor(0, 1);
    lcd.print(linha2);
  }
  if (delayMs > 0) delay(delayMs);
}

void atualizarLCD() {
  if (pessoas.total == 0) {
    lcd.setCursor(0, 0);
    lcd.print(F("Sistema Inativo"));
    lcd.setCursor(0, 1);
    lcd.print(F("Passe o cartao"));
    lcd.noBacklight();
    return;
  }

  lcd.backlight();
  static uint32_t hashAnterior = 0;
  static unsigned long ultimaAtualizacao = 0;
  
  unsigned long agora = millis();
  if (agora - ultimaAtualizacao < 200) return;

  uint32_t hash = (uint32_t)(sensores.temperatura * 10) +
                  (uint32_t)(sensores.humidade) * 1000 +
                  sensores.luminosidade * 10000 +
                  pessoas.total * 100000 +
                  (clima.ligado ? 1UL << 20 : 0) +
                  (flags.wifiOk ? 1UL << 21 : 0) +
                  (flags.erroSensor ? 1UL << 22 : 0) +
                  (flags.modoManualIlum ? 1UL << 23 : 0) +
                  (flags.modoManualClima ? 1UL << 24 : 0);

  if (hash == hashAnterior) return;
  
  hashAnterior = hash;
  ultimaAtualizacao = agora;
  lcd.clear();

  if (flags.modoManualIlum || flags.modoManualClima) {
    lcd.setCursor(0, 0);
    lcd.write(1); lcd.print(sensores.temperatura, 1); lcd.write(0xDF);
    lcd.print("C "); lcd.write(2); lcd.print(sensores.humidade, 0); lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print(F("Manual Mode "));
    lcd.write(0); lcd.print(pessoas.total);
    lcd.setCursor(15, 1);
    lcd.write(flags.wifiOk ? 4 : 7);
  } else {
    lcd.setCursor(0, 0);
    lcd.write(1); lcd.print(sensores.temperatura, 1); lcd.write(0xDF);
    lcd.print("C "); lcd.write(2); lcd.print(sensores.humidade, 0); lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.write(3); lcd.print(sensores.luminosidade); lcd.print("% ");
    lcd.write(0); lcd.print(pessoas.total); lcd.print(" ");
    lcd.write(5); lcd.print(clima.ligado ? "L" : "-");
    lcd.setCursor(15, 1);
    lcd.write(flags.wifiOk ? 4 : 7);
  }
}

void atualizarTelaClimatizador() {
  static uint8_t estadoAnterior = 0;
  static unsigned long tempoExibicao = 0;

  uint8_t estadoAtual = (clima.ligado) | (clima.umidificando << 1) | 
                        (clima.velocidade << 2) | (clima.timer << 4) | 
                        (clima.aletaV << 7);

  if (estadoAtual != estadoAnterior) {
    estadoAnterior = estadoAtual;
    tempoExibicao = millis();
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(5);
    lcd.print(F(" Climatizador"));
    lcd.setCursor(0, 1);

    if (!clima.ligado) {
      lcd.print(F("Desligado"));
    } else {
      char buffer[17];
      uint8_t pos = 0;
      buffer[pos++] = 'V';
      buffer[pos++] = '0' + clima.velocidade;
      buffer[pos++] = ' ';
      
      if (clima.timer > 0) {
        buffer[pos++] = 'T';
        buffer[pos++] = '0' + clima.timer;
        buffer[pos++] = ' ';
      }
      if (clima.umidificando) {
        buffer[pos++] = 'U'; buffer[pos++] = 'M'; buffer[pos++] = ' ';
      }
      if (clima.aletaV || clima.aletaH) {
        buffer[pos++] = 'A';
        if (clima.aletaV) buffer[pos++] = 'V';
        if (clima.aletaH) buffer[pos++] = 'H';
      }
      buffer[pos] = '\0';
      lcd.print(buffer);
    }
  }

  if (millis() - tempoExibicao > 1500) {
    atualizarLCD();
  }
}

// ==================== CONSULTAR PREFERÊNCIAS ====================
bool consultarPreferencias() {
  if (pessoas.total == 0 || !flags.wifiOk || flags.atualizandoPref) {
    debugPrint("consultarPreferencias: Condições não atendidas");
    return false;
  }

  flags.atualizandoPref = true;
  StaticJsonDocument<256> doc;
  JsonArray tagsArray = doc.createNestedArray("tags");
  
  int tagsAtivas = 0;
  for (int i = 0; i < pessoas.count; i++) {
    if (pessoas.estado[i]) {
      tagsArray.add(pessoas.tags[i]);
      tagsAtivas++;
    }
  }

  if (tagsAtivas == 0) {
    flags.atualizandoPref = false;
    return false;
  }

  String jsonTags;
  serializeJson(doc, jsonTags);
  debugPrint("Consultando preferências: " + jsonTags);

  HTTPClient http;
  http.begin(String(serverUrl) + "preferencias");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  
  int httpCode = http.POST(jsonTags);
  bool sucesso = false;

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    debugPrint("Resposta: " + payload);
    
    StaticJsonDocument<256> respDoc;
    DeserializationError error = deserializeJson(respDoc, payload);
    
    if (!error) {
      if (respDoc.containsKey("temperatura")) {
        float tempPref = respDoc["temperatura"];
        if (!isnan(tempPref) && tempPref >= 16.0 && tempPref <= 32.0) {
          pessoas.tempPref = tempPref;
          debugPrint("✓ Temp preferida: " + String(pessoas.tempPref) + "°C");
        } else {
          pessoas.tempPref = 25.0;
        }
      }
      
      if (respDoc.containsKey("luminosidade")) {
        int lumPref = respDoc["luminosidade"];
        if (lumPref >= 0 && lumPref <= 100 && (lumPref % 25 == 0)) {
          pessoas.lumPref = lumPref;
          debugPrint("✓ Lum preferida: " + String(pessoas.lumPref) + "%");
        } else {
          pessoas.lumPref = 50;
        }
      }
      
      sucesso = true;
      pessoas.prefsAtualizadas = true;
      debugPrint("=== PREFERÊNCIAS APLICADAS ===");
    } else {
      mostrarErroLCD("Erro JSON Pref", false);
    }
  } else {
    mostrarErroLCD("Erro API Pref", false);
  }

  http.end();
  flags.atualizandoPref = false;
  return sucesso;
}

// ==================== CONTROLE DE RELÉS ====================
void configurarRele(int nivel) {
  static int nivelAnterior = -1;
  if (nivel == nivelAnterior) return;

  nivel = (nivel / 25) * 25;
  nivel = constrain(nivel, 0, 100);

  const bool estados[5][4] = {
    {HIGH, HIGH, HIGH, HIGH},  // 0%
    {LOW, HIGH, HIGH, HIGH},   // 25%
    {LOW, LOW, HIGH, HIGH},    // 50%
    {LOW, LOW, LOW, HIGH},     // 75%
    {LOW, LOW, LOW, LOW}       // 100%
  };

  int indice = nivel / 25;
  digitalWrite(RELE_1, estados[indice][0]);
  digitalWrite(RELE_2, estados[indice][1]);
  digitalWrite(RELE_3, estados[indice][2]);
  digitalWrite(RELE_4, estados[indice][3]);
  
  sensores.luminosidade = nivel;
  debugPrint("Relé: " + String(nivel) + "% (índice " + String(indice) + ")");

  if (pessoas.total > 0 && abs(nivel - nivelAnterior) >= 25) {
    tocarSom(SOM_COMANDO);
  }
  
  nivelAnterior = nivel;
  if (pessoas.total > 0) atualizarLCD();
}

// ==================== LEITURA DE SENSORES ====================
void lerSensores() {
  unsigned long agora = millis();
  if (agora - tempos[0] < INTERVALO_DHT) return;
  tempos[0] = agora;

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (!isnan(temp) && !isnan(hum) && temp >= -40 && temp <= 80 && hum >= 0 && hum <= 100) {
    if (flags.erroSensor) {
      flags.erroSensor = false;
      tocarSom(SOM_OK);
      debugPrint("Sensores DHT recuperados");
    }
    sensores.temperatura = temp;
    sensores.humidade = hum;
    sensores.dadosValidos = true;
  } else {
    if (!sensores.dadosValidos) {
      sensores.temperatura = 25.0;
      sensores.humidade = 50.0;
      sensores.dadosValidos = true;
    }
    if (!flags.erroSensor) {
      flags.erroSensor = true;
      mostrarErroLCD("Sensor DHT", false);
    }
  }

  // LDR com buffer de 3 amostras
  static int ldrBuffer[3] = {0};
  static int bufferIndex = 0;
  ldrBuffer[bufferIndex] = analogRead(LDR_PIN);
  bufferIndex = (bufferIndex + 1) % 3;
  sensores.valorLDR = (ldrBuffer[0] + ldrBuffer[1] + ldrBuffer[2]) / 3;

  if (pessoas.total > 0) atualizarLCD();
}

// ==================== COMUNICAÇÃO HTTP ====================
bool enviarHTTP(const String &endpoint, const String &dados = "", bool isPost = false) {
  if (!flags.wifiOk) return false;

  HTTPClient http;
  http.begin(String(serverUrl) + endpoint);
  http.setTimeout(5000);

  int httpCode = isPost ? 
    (http.addHeader("Content-Type", "application/json"), http.POST(dados)) : 
    http.GET();

  bool sucesso = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT);

  if (!sucesso) {
    if (!flags.erroConexao) {
      flags.erroConexao = true;
      mostrarErroLCD("Falha Conexao", false);
    }
  } else if (flags.erroConexao) {
    flags.erroConexao = false;
    tocarSom(SOM_OK);
  }

  http.end();
  return sucesso;
}

// ==================== ENVIAR DADOS (UNIFICADO) ====================
void enviarDados(bool forcado = false) {
  static unsigned long ultimoEnvio = 0;
  unsigned long agora = millis();
  
  if (!forcado && agora - ultimoEnvio < INTERVALO_DADOS) return;
  ultimoEnvio = agora;

  StaticJsonDocument<300> doc;
  doc["t"] = round(sensores.temperatura * 10) / 10.0;
  doc["h"] = round(sensores.humidade);
  doc["l"] = sensores.luminosidade;
  doc["p"] = pessoas.total;

  JsonObject c = doc.createNestedObject("c");
  c["l"] = clima.ligado;
  c["u"] = clima.umidificando;
  c["v"] = clima.velocidade;
  c["uv"] = clima.ultimaVel;
  c["t"] = clima.timer;
  c["av"] = clima.aletaV;
  c["ah"] = clima.aletaH;
  c["mmc"] = flags.modoManualClima;

  if (pessoas.count > 0) {
    JsonArray tagsArray = doc.createNestedArray("tags");
    for (int i = 0; i < pessoas.count; i++) {
      if (pessoas.estado[i]) tagsArray.add(pessoas.tags[i]);
    }
  }

  String dados;
  serializeJson(doc, dados);
  debugPrint((forcado ? "ENVIO FORÇADO: " : "Enviando: ") + dados);
  enviarHTTP("ambiente", dados, true);
}

// ==================== PROCESSAMENTO NFC ====================
void processarNFC() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  String tag = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) tag += "0";
    tag += String(mfrc522.uid.uidByte[i], HEX);
  }
  tag.toUpperCase();

  debugPrint("Tag NFC: " + tag);
  gerenciarPresenca(tag);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

// ==================== GERENCIAMENTO DE PRESENÇA ====================
void gerenciarPresenca(const String &tag) {
  int indice = -1;
  bool entrando = false;
  int totalAnterior = pessoas.total;

  // Busca tag existente
  for (int i = 0; i < pessoas.count; i++) {
    if (pessoas.tags[i] == tag) {
      indice = i;
      break;
    }
  }

  if (indice == -1) {
    // Nova pessoa
    if (pessoas.count < 10) {
      pessoas.tags[pessoas.count] = tag;
      pessoas.estado[pessoas.count] = true;
      pessoas.total++;
      pessoas.count++;
      entrando = true;

      if (pessoas.total == 1) {
        flags.ilumAtiva = false;
        flags.modoManualIlum = false;
        flags.modoManualClima = false;
        lcd.backlight();
        debugPrint("Primeira pessoa - Automação ativada");
      }

      mostrarMensagemLCD(0, "Bem-vindo!", ("Pessoas: " + String(pessoas.total)).c_str());
      tocarSom(SOM_PESSOA_ENTROU);
    } else {
      tocarSom(SOM_ERRO);
      return;
    }
  } else if (pessoas.estado[indice]) {
    // Pessoa saindo
    pessoas.estado[indice] = false;
    pessoas.total--;
    
    mostrarMensagemLCD(0, "Ate logo!", ("Pessoas: " + String(pessoas.total)).c_str());
    tocarSom(SOM_PESSOA_SAIU);
    enviarDados(true);

    if (pessoas.total == 0) {
      debugPrint("Última pessoa saindo - Reset do sistema");
      delay(1000);
      enviarDados(true);
      delay(1500);
      enviarDados(true);
      delay(2000);
      resetarSistema();
      return;
    }
  } else {
    // Pessoa retornando
    pessoas.estado[indice] = true;
    pessoas.total++;
    entrando = true;
    
    mostrarMensagemLCD(0, "Bem-vindo!", ("Pessoas: " + String(pessoas.total)).c_str(), 500);
    tocarSom(SOM_PESSOA_ENTROU);
  }

  // Atualiza preferências se houve mudança
  if ((totalAnterior != pessoas.total || entrando) && pessoas.total > 0) {
    pessoas.prefsAtualizadas = false;
    if (consultarPreferencias()) {
      // Não liga luzes automaticamente aqui - deixa o gerenciarIluminacao decidir
      atualizarLCD();
      enviarDados();
    }
  }
}

// ==================== RESET DO SISTEMA ====================
void resetarSistema() {
  mostrarMensagemLCD(7, "Desativando...", "Sistema", 800);
  tocarSom(SOM_ALERTA);

  if (clima.ligado) {
    lcd.setCursor(0, 1);
    lcd.print(F("Desl. Clima... "));
    enviarComandoIR(IR_POWER);
    delay(500);
  }

  lcd.setCursor(0, 1);
  lcd.print(F("Desl. Luzes... "));
  flags.modoManualIlum = false;
  flags.modoManualClima = false;
  flags.ilumAtiva = false;
  configurarRele(0);
  delay(300);

  for (int i = 0; i < pessoas.count; i++) {
    pessoas.tags[i] = "";
    pessoas.estado[i] = false;
  }

  pessoas.total = 0;
  pessoas.count = 0;
  pessoas.prefsAtualizadas = false;

  lcd.setCursor(0, 1);
  lcd.print(F("Concluido! "));
  delay(1000);

  lcd.noBacklight();
  lcd.clear();
  lcd.print(F("Sistema Inativo"));
  lcd.setCursor(0, 1);
  lcd.print(F("Passe o cartao"));
  
  debugPrint("Sistema resetado - Standby");
}

// ==================== GERENCIAMENTO DE ILUMINAÇÃO (CORRIGIDO) ====================
void gerenciarIluminacao() {
  unsigned long agora = millis();
  if (agora - tempos[2] < INTERVALO_LDR) return;
  tempos[2] = agora;

  // Desliga tudo se não há pessoas
  if (pessoas.total == 0) {
    if (flags.ilumAtiva || sensores.luminosidade != 0) {
      configurarRele(0);
      flags.ilumAtiva = false;
      flags.modoManualIlum = false;
    }
    return;
  }

  // Não faz nada se está em modo manual ou preferências não estão carregadas
  if (flags.modoManualIlum || !pessoas.prefsAtualizadas) return;

  // NOVA LÓGICA: Monitora continuamente o LDR
  if (!flags.ilumAtiva) {
    // Luz está desligada - verifica se deve ligar (escuro)
    if (sensores.valorLDR < LIMIAR_LDR_ESCURIDAO) {
      int nivel = pessoas.lumPref == 0 ? 25 : pessoas.lumPref;
      
      debugPrint("=== LIGANDO LUZES AUTO ===");
      debugPrint("LDR: " + String(sensores.valorLDR) + " | Nível: " + String(nivel) + "%");
      
      mostrarMensagemLCD(3, ("Luz Auto ON " + String(nivel) + "%").c_str(), 
                        ("Pref:" + String(pessoas.lumPref) + "% LDR:" + String(sensores.valorLDR)).c_str(), 800);
      tocarSom(SOM_COMANDO);
      
      configurarRele(nivel);
      flags.ilumAtiva = true;
      atualizarLCD();
    }
  } else {
    // Luz está ligada - verifica se deve desligar ou ajustar
    
    // CORREÇÃO: Verifica se a luz ambiente está suficiente para desligar
    if (sensores.valorLDR > LIMIAR_LDR_ESCURIDAO + HISTERESE_LDR) {
      debugPrint("=== DESLIGANDO LUZES AUTO ===");
      debugPrint("LDR: " + String(sensores.valorLDR) + " (luz suficiente)");
      
      mostrarMensagemLCD(3, "Luz Auto OFF", 
                        ("LDR:" + String(sensores.valorLDR)).c_str(), 800);
      tocarSom(SOM_COMANDO);
      
      configurarRele(0);
      flags.ilumAtiva = false;
      atualizarLCD();
    } else {
      // Luz ainda necessária - verifica ajuste de nível baseado em preferências
      int nivelDesejado = pessoas.lumPref == 0 ? 25 : pessoas.lumPref;
      if (sensores.luminosidade != nivelDesejado && pessoas.prefsAtualizadas) {
        debugPrint("AJUSTANDO: " + String(sensores.luminosidade) + "% -> " + String(nivelDesejado) + "%");
        mostrarMensagemLCD(3, "Ajuste Auto", 
                          (String(sensores.luminosidade) + "% -> " + String(nivelDesejado) + "%").c_str(), 800);
        tocarSom(SOM_COMANDO);
        configurarRele(nivelDesejado);
        atualizarLCD();
      }
    }
  }
}

// ==================== CONTROLE IR ====================
bool enviarComandoIR(uint8_t comando) {
  static unsigned long ultimoComandoIR = 0;
  unsigned long agora = millis();

  if (agora - ultimoComandoIR < DEBOUNCE_ENVIAR || controleIR.estado != IR_OCIOSO) {
    return false;
  }

  debugPrint("Enviando IR: 0x" + String(comando, HEX));
  controleIR.estado = IR_ENVIANDO;
  controleIR.comandoPendente = comando;
  controleIR.inicioEnvio = agora;
  controleIR.comandoConfirmado = false;

  for (int i = 0; i < 3; i++) {
    IrSender.sendNEC(IR_ENDERECO, comando, 0);
    delay(5);
  }

  controleIR.estado = IR_AGUARDANDO_CONFIRMACAO;
  ultimoComandoIR = agora;
  return true;
}

void atualizarEstadoClima(uint8_t comando) {
  bool estadoAnterior = clima.ligado;

  switch (comando) {
    case IR_POWER:
      clima.ligado = !clima.ligado;
      if (clima.ligado) {
        clima.velocidade = clima.ultimaVel > 0 ? clima.ultimaVel : 1;
      } else {
        clima.velocidade = 0;
        clima.umidificando = false;
        clima.timer = 0;
        clima.aletaV = false;
        clima.aletaH = false;
      }
      break;
      
    case IR_UMIDIFICAR:
      if (clima.ligado) clima.umidificando = !clima.umidificando;
      break;
      
    case IR_VELOCIDADE:
      if (clima.ligado) {
        clima.velocidade = (clima.velocidade % 3) + 1;
        clima.ultimaVel = clima.velocidade;
      }
      break;
      
    case IR_TIMER:
      if (clima.ligado) clima.timer = (clima.timer + 1) % 6;
      break;
      
    case IR_ALETA_VERTICAL:
      if (clima.ligado) clima.aletaV = !clima.aletaV;
      break;
      
    case IR_ALETA_HORIZONTAL:
      if (clima.ligado) clima.aletaH = !clima.aletaH;
      break;
  }

  if (estadoAnterior != clima.ligado || comando != 0) {
    clima.ultimaAtualizacao = millis();
  }
}

void controleAutomaticoClima() {
  if (pessoas.total <= 0 || flags.modoManualClima) {
    if (pessoas.total == 0 && clima.ligado) {
      enviarComandoIR(IR_POWER);
    }
    return;
  }

  unsigned long agora = millis();
  if (agora - tempos[3] < INTERVALO_CLIMA_AUTO) return;
  tempos[3] = agora;

  float tempAlvo = pessoas.prefsAtualizadas ? pessoas.tempPref : 25.0;
  float diff = sensores.temperatura - tempAlvo;

  debugPrint("=== CLIMA AUTO === Temp:" + String(sensores.temperatura) + 
             " Alvo:" + String(tempAlvo) + " Diff:" + String(diff));

  if (diff >= 2.0 && !clima.ligado) {
    mostrarMensagemLCD(5, "Auto Ligar", ("Temp: +" + String(diff, 1) + " graus").c_str(), 800);
    tocarSom(SOM_COMANDO);
    enviarComandoIR(IR_POWER);
    atualizarLCD();
  } else if (diff <= -0.5 && clima.ligado) {
    mostrarMensagemLCD(5, "Auto Deslig.", ("Temp: " + String(diff, 1) + " graus").c_str(), 800);
    tocarSom(SOM_COMANDO);
    enviarComandoIR(IR_POWER);
    atualizarLCD();
  } else if (clima.ligado) {
    int velDesejada = (diff >= 4.5) ? 3 : (diff >= 3.0) ? 2 : 1;
    
    if (clima.velocidade != velDesejada) {
      mostrarMensagemLCD(5, "Ajuste Auto", ("Vel " + String(clima.velocidade) + " -> " + String(velDesejada)).c_str(), 800);
      tocarSom(SOM_COMANDO);
      
      int tentativas = 0;
      while (clima.velocidade != velDesejada && tentativas < 3) {
        if (enviarComandoIR(IR_VELOCIDADE)) {
          tentativas++;
          unsigned long t0 = millis();
          while (controleIR.estado != IR_OCIOSO && millis() - t0 < TIMEOUT_CONFIRMACAO + 200) {
            processarIRRecebido();
            delay(10);
          }
        } else break;
      }
      atualizarLCD();
    }
  }

  // Controle de aletas
  if (clima.ligado && !flags.modoManualClima) {
    if (pessoas.total == 1) {
      if (!clima.aletaV) enviarComandoIR(IR_ALETA_VERTICAL);
      if (clima.aletaH) enviarComandoIR(IR_ALETA_HORIZONTAL);
    } else if (pessoas.total > 1) {
      if (!clima.aletaV) enviarComandoIR(IR_ALETA_VERTICAL);
      if (!clima.aletaH) enviarComandoIR(IR_ALETA_HORIZONTAL);
    }
  }
}

// ==================== VERIFICAR COMANDOS DO SERVIDOR ====================
void verificarComandos() {
  unsigned long agora = millis();
  if (agora - tempos[4] < INTERVALO_COMANDOS || !flags.wifiOk) return;
  tempos[4] = agora;

  HTTPClient http;

  // Comando de iluminação
  http.begin(String(serverUrl) + "comando");
  if (http.GET() == HTTP_CODE_OK) {
    String cmd = http.getString();
    cmd.trim();

    if (cmd != "auto" && cmd.length() > 0 && pessoas.total > 0) {
      int nivel = cmd.toInt();
      if (nivel >= 0 && nivel <= 100 && (nivel % 25 == 0)) {
        bool entrandoManual = !flags.modoManualIlum;
        flags.modoManualIlum = true;
        flags.ilumAtiva = true;

        if (entrandoManual) {
          mostrarMensagemLCD(3, "Luz Manual ON", ("Nivel: " + String(nivel) + "%").c_str(), 800);
          tocarSom(SOM_COMANDO);
        }

        if (sensores.luminosidade != nivel) {
          configurarRele(nivel);
        }
      }
    } else if (cmd == "auto" && flags.modoManualIlum) {
      flags.modoManualIlum = false;
      flags.ilumAtiva = false;
      atualizarLCD();
    }
  }
  http.end();

  // Comando direto do climatizador
  http.begin(String(serverUrl) + "climatizador/comando");
  if (http.GET() == HTTP_CODE_OK) {
    String cmd = http.getString();
    cmd.trim();

    if (cmd != "none" && cmd.length() > 0) {
      if (!flags.modoManualClima) {
        flags.modoManualClima = true;
        mostrarMensagemLCD(5, "Clima: Manual", "Control: APP", 800);
        tocarSom(SOM_COMANDO);
      }

      mostrarMensagemLCD(5, "Comando App", cmd.c_str(), 800);
      tocarSom(SOM_COMANDO);

      if (cmd == "power") enviarComandoIR(IR_POWER);
      else if (cmd == "velocidade") enviarComandoIR(IR_VELOCIDADE);
      else if (cmd == "timer") enviarComandoIR(IR_TIMER);
      else if (cmd == "umidificar") enviarComandoIR(IR_UMIDIFICAR);
      else if (cmd == "aleta_vertical") enviarComandoIR(IR_ALETA_VERTICAL);
      else if (cmd == "aleta_horizontal") enviarComandoIR(IR_ALETA_HORIZONTAL);

      atualizarLCD();
    }
  }
  http.end();

  // Modo manual do climatizador
  http.begin(String(serverUrl) + "climatizador/manual");
  if (http.GET() == HTTP_CODE_OK) {
    StaticJsonDocument<64> doc;
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      if (doc.containsKey("modoManualClimatizador")) {
        bool modoManual = doc["modoManualClimatizador"];
        if (modoManual != flags.modoManualClima) {
          flags.modoManualClima = modoManual;
          if (modoManual) {
            mostrarMensagemLCD(5, "Clima: Manual", "Control: Sync", 800);
            tocarSom(SOM_COMANDO);
          }
          atualizarLCD();
        }
      }
    }
  }
  http.end();
}

// ==================== PROCESSAMENTO IR RECEBIDO ====================
void processarIRRecebido() {
  if (!IrReceiver.decode()) {
    // Timeout de confirmação
    if (controleIR.estado == IR_AGUARDANDO_CONFIRMACAO && 
        millis() - controleIR.inicioEnvio > TIMEOUT_CONFIRMACAO) {
      atualizarEstadoClima(controleIR.comandoPendente);
      controleIR.estado = IR_OCIOSO;
      atualizarTelaClimatizador();
    }
    return;
  }

  unsigned long agora = millis();
  uint8_t protocolo = IrReceiver.decodedIRData.protocol;
  uint16_t endereco = IrReceiver.decodedIRData.address;
  uint8_t comando = IrReceiver.decodedIRData.command;

  if (protocolo != IR_PROTOCOLO || endereco != IR_ENDERECO) {
    IrReceiver.resume();
    return;
  }

  // Ignora eco
  if (controleIR.estado == IR_ENVIANDO || 
      (controleIR.estado == IR_AGUARDANDO_CONFIRMACAO && 
       agora - controleIR.inicioEnvio < JANELA_ECO)) {
    IrReceiver.resume();
    return;
  }

  static unsigned long ultimoIRRecebido = 0;
  if (agora - ultimoIRRecebido < DEBOUNCE_RECEBER) {
    IrReceiver.resume();
    return;
  }

  if (controleIR.estado == IR_AGUARDANDO_CONFIRMACAO && 
      comando == controleIR.comandoPendente) {
    debugPrint("Comando IR confirmado: 0x" + String(comando, HEX));
    controleIR.comandoConfirmado = true;
    atualizarEstadoClima(comando);
    controleIR.estado = IR_OCIOSO;
    atualizarTelaClimatizador();
  } else {
    // Comando externo
    if (!flags.modoManualClima) {
      flags.modoManualClima = true;
      mostrarMensagemLCD(5, "Clima: Manual", "Control: IR", 600);
      tocarSom(SOM_COMANDO);
    }

    debugPrint("Comando IR externo: 0x" + String(comando, HEX));
    atualizarEstadoClima(comando);
    if (pessoas.total > 0) atualizarLCD();
  }

  ultimoIRRecebido = agora;
  IrReceiver.resume();
}

// ==================== MONITORAMENTO WiFi ====================
void monitorarWiFi() {
  static unsigned long ultimaVerif = 0;
  unsigned long agora = millis();
  
  if (agora - ultimaVerif < 30000) return;
  ultimaVerif = agora;

  bool estadoAtual = (WiFi.status() == WL_CONNECTED);
  
  if (estadoAtual != flags.wifiOk) {
    flags.wifiOk = estadoAtual;
    
    if (flags.wifiOk) {
      mostrarMensagemLCD(4, "WiFi Conectado!", WiFi.localIP().toString().c_str(), 1000);
      tocarSom(SOM_CONECTADO);
      
      if (pessoas.total > 0 && !flags.atualizandoPref) {
        pessoas.prefsAtualizadas = false;
        consultarPreferencias();
      }
    } else {
      mostrarMensagemLCD(7, "WiFi Perdido", "Reconectando...", 1000);
      tocarSom(SOM_DESCONECTADO);
    }
    
    atualizarLCD();
  }
}

// ==================== FUNÇÕES DE TESTE ====================
void testarReles() {
  debugPrint("=== TESTE DE RELÉS ===");
  int pinos[] = {RELE_1, RELE_2, RELE_3, RELE_4};
  
  for (int i = 0; i < 4; i++) {
    digitalWrite(pinos[i], LOW);
    debugPrint("Relé " + String(i + 1) + " ON");
    delay(200);
    digitalWrite(pinos[i], HIGH);
    debugPrint("Relé " + String(i + 1) + " OFF");
    delay(200);
  }
}

void testarNiveisLuminosidade() {
  debugPrint("=== TESTE DE NÍVEIS ===");
  const int niveis[] = {0, 25, 50, 75, 100, 75, 50, 25, 0};
  
  for (int nivel : niveis) {
    configurarRele(nivel);
    delay(1000);
  }
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  debugPrint("\n=== SISTEMA INICIANDO ===");

  // Configuração de pinos
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELE_1, OUTPUT);
  pinMode(RELE_2, OUTPUT);
  pinMode(RELE_3, OUTPUT);
  pinMode(RELE_4, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  digitalWrite(RELE_1, HIGH);
  digitalWrite(RELE_2, HIGH);
  digitalWrite(RELE_3, HIGH);
  digitalWrite(RELE_4, HIGH);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, SIMBOLO_PESSOA);
  lcd.createChar(1, SIMBOLO_TEMP);
  lcd.createChar(2, SIMBOLO_HUM);
  lcd.createChar(3, SIMBOLO_LUZ);
  lcd.createChar(4, SIMBOLO_WIFI);
  lcd.createChar(5, SIMBOLO_AR);
  lcd.createChar(6, SIMBOLO_OK);
  lcd.createChar(7, SIMBOLO_ERRO);

  lcd.clear();
  lcd.print(F("Smart Climate v3.0"));
  lcd.setCursor(0, 1);
  lcd.print(F("Iniciando"));
  for (int i = 0; i < 3; i++) {
    delay(200);
    lcd.print(".");
  }

  tocarSom(SOM_INICIAR);
  testarReles();
  configurarRele(0);

  // Módulos
  SPI.begin();
  mfrc522.PCD_Init();
  dht.begin();
  IrSender.begin(IR_SEND_PIN, true, IR_SEND_PIN);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  // WiFi
  lcd.clear();
  lcd.print(F("Conectando WiFi"));
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  WiFi.begin(ssid, password);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) {
    delay(500);
    lcd.setCursor(15, 1);
    lcd.print(tentativas % 2 == 0 ? "." : " ");
    tentativas++;
  }

  flags.wifiOk = (WiFi.status() == WL_CONNECTED);

  if (flags.wifiOk) {
    mostrarMensagemLCD(4, "Conectado!", WiFi.localIP().toString().c_str(), 1000);
    tocarSom(SOM_CONECTADO);
  } else {
    mostrarMensagemLCD(7, "Sem WiFi", "Modo Offline", 1000);
    tocarSom(SOM_ERRO);
  }

  // Inicializa tempos
  unsigned long agora = millis();
  for (int i = 0; i < 8; i++) {
    tempos[i] = agora - (INTERVALO_DADOS + 1000);
  }

  lerSensores();
  delay(1000);

  lcd.noBacklight();
  lcd.clear();
  lcd.print(F("Sistema Inativo"));
  lcd.setCursor(0, 1);
  lcd.print(F("Passe o cartao"));

  debugPrint("=== SISTEMA PRONTO - STANDBY ===");
}

// ==================== LOOP PRINCIPAL ====================
void loop() {
  processarIRRecebido();
  processarNFC();

  static unsigned long ultimoCicloRapido = 0;
  static unsigned long ultimoCicloComunicacao = 0;
  unsigned long agora = millis();

  // Ciclo rápido (100ms)
  if (agora - ultimoCicloRapido > 100) {
    ultimoCicloRapido = agora;
    lerSensores();
    gerenciarIluminacao();
    controleAutomaticoClima();
  }

  // Ciclo de comunicação (1s)
  if (agora - ultimoCicloComunicacao > 1000) {
    ultimoCicloComunicacao = agora;
    monitorarWiFi();
    verificarComandos();

    if (agora - tempos[5] >= INTERVALO_PREF_CHECK) {
      tempos[5] = agora;
      if (pessoas.total > 0) {
        consultarPreferencias();
      } else {
        pessoas.prefsAtualizadas = false;
      }
    }

    // Comandos seriais (debug)
    if (pessoas.total == 0 && Serial.available() > 0) {
      String comando = Serial.readString();
      comando.trim();
      comando.toLowerCase();

      if (comando == "testar_reles") testarReles();
      else if (comando == "testar_niveis") testarNiveisLuminosidade();
      else if (comando.startsWith("rele_")) {
        int nivel = comando.substring(5).toInt();
        if (nivel >= 0 && nivel <= 100 && (nivel % 25 == 0)) {
          configurarRele(nivel);
        }
      }
    }

    enviarDados();
    mostrarTelaDebug();
  }

  delay(5);
}