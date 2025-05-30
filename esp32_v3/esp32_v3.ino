#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>

// === PINOS E CONSTANTES ===
#define BUZZER_PIN 12
#define RELE_1 14
#define RELE_2 27
#define RELE_3 26
#define RELE_4 25
#define SS_PIN 5
#define RST_PIN 15
#define DHT_PIN 4
#define DHTTYPE DHT22
#define LDR_PIN 35
#define IR_SEND_PIN 33
#define IR_RECEIVE_PIN 32

// === COMANDOS IR OTIMIZADOS ===
#define IR_PROTOCOLO NEC          // Protocolo NEC (usar constante da biblioteca)
#define IR_ENDERECO 0xFC00        // Endereço do climatizador
#define IR_POWER 0x85             // Comando Power
#define IR_UMIDIFICAR 0x87        // Comando Umidificar
#define IR_VELOCIDADE 0x84        // Comando Velocidade
#define IR_TIMER 0x86             // Comando Timer
#define IR_ALETA_VERTICAL 0x83    // Comando Aleta Vertical
#define IR_ALETA_HORIZONTAL 0x82  // Comando Aleta Horizontal

// === CONSTANTES DE CONTROLE IR ===
#define DEBOUNCE_ENVIAR 300      // Debounce para envio (ms)
#define DEBOUNCE_RECEBER 500     // Debounce para recebimento (ms)
#define JANELA_ECO 100           // Janela para ignorar ecos após envio (ms)
#define TIMEOUT_CONFIRMACAO 500  // Timeout para confirmação (ms)

// === DEBUG ATIVADO PARA DEPURAÇÃO ===
#define DEBUG_SERIAL 1  // Ativar depuração serial

// === OBJETOS GLOBAIS ===
MFRC522 mfrc522(SS_PIN, RST_PIN);
DHT dht(DHT_PIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === ESTRUTURAS OTIMIZADAS PARA MEMÓRIA ===
struct DadosSensores {
  float temperatura = 0;
  float humidade = 0;
  int luminosidade = 0;
  int valorLDR = 0;
  bool dadosValidos = false;
} sensores;

// Climatizador ultra compacto (apenas 4 bytes + timestamp)
struct {
  bool ligado : 1;
  bool umidificando : 1;
  bool aletaV : 1;
  bool aletaH : 1;
  uint8_t velocidade : 2;
  uint8_t ultimaVel : 2;  // Para guardar a última velocidade manual ou automática
  uint8_t timer : 3;
  uint8_t reservado : 5;  // Para futuras expansões
  unsigned long ultimaAtualizacao;
} clima;

// Controle de pessoas ultra otimizado
struct {
  int total = 0;
  String tags[10];  // Máximo 10 pessoas simultâneas
  bool estado[10];  // true = presente, false = saiu mas a tag ainda está no histórico
  int count = 0;   // Número de tags distintas no histórico
  float tempPref = 25.0;
  int lumPref = 50;
  bool prefsAtualizadas = false;  // Indica se as preferências já foram consultadas para o grupo atual
} pessoas;

// Flags de controle compactas
struct {
  bool modoManualIlum : 1;
  bool modoManualClima : 1;
  bool ilumAtiva : 1;
  bool monitorandoLDR : 1;
  bool irPausa : 1;
  bool comandoIR : 1;
  bool comandoApp : 1;
  bool wifiOk : 1;
  bool erroSensor : 1;       // Flag para monitorar erro de sensores
  bool erroConexao : 1;      // Flag para monitorar erro de conexão
  bool debug : 1;            // Flag para modo de debug
  bool atualizandoPref : 1;  // Flag para atualização de preferências (para evitar múltiplas chamadas)
} flags = { false, false, false, true, false, false, false, false, false, false, false, false };

// === MÁQUINA DE ESTADOS IR ===
enum EstadoIR {
  IR_OCIOSO,                 // Aguardando comando para enviar
  IR_ENVIANDO,               // Enviando comando
  IR_AGUARDANDO_CONFIRMACAO  // Aguardando confirmação
};

// === ESTRUTURA DE CONTROLE IR ===
struct {
  EstadoIR estado = IR_OCIOSO;     // Estado atual
  uint8_t comandoPendente = 0;     // Comando sendo enviado/aguardado
  unsigned long inicioEnvio = 0;   // Timestamp do início do envio
  bool comandoConfirmado = false;  // Flag para confirmação
} controleIR;

// === INTERVALOS OTIMIZADOS ===
const unsigned long INTERVALO_DHT = 5000;
const unsigned long INTERVALO_DADOS = 5000;
const unsigned long INTERVALO_MANUAL = 1500;
const unsigned long INTERVALO_LDR = 5000;
const unsigned long INTERVALO_CLIMA_AUTO = 5000;
const unsigned long INTERVALO_COMANDOS = 3000;
const unsigned long INTERVALO_DEBUG = 5000;
const unsigned long INTERVALO_PREF_CHECK = 30000;

// Timestamps otimizados
unsigned long tempos[8] = { 0 };  // Array único para todos os timestamps

// Adicione esta constante para o limiar do LDR
#define LIMIAR_LDR_ESCURIDAO 400  // Valor LDR abaixo do qual é considerado "muito escuro" para ligar as luzes

// === CONFIGURAÇÃO DE REDE ===
const char* ssid = "João Augusto";
const char* password = "131103r7";
const char* serverUrl = "http://192.168.31.102:3000/esp32/";

// === CARACTERES PERSONALIZADOS PARA LCD ===
uint8_t SIMBOLO_PESSOA[8] = { 0x0E, 0x0E, 0x04, 0x1F, 0x04, 0x0A, 0x0A, 0x00 };  // Pessoa
uint8_t SIMBOLO_TEMP[8] = { 0x04, 0x0A, 0x0A, 0x0A, 0x0A, 0x11, 0x1F, 0x0E };    // Termômetro
uint8_t SIMBOLO_HUM[8] = { 0x04, 0x04, 0x0A, 0x0A, 0x11, 0x11, 0x11, 0x0E };     // Gota
uint8_t SIMBOLO_LUZ[8] = { 0x00, 0x0A, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00 };     // Lâmpada
uint8_t SIMBOLO_WIFI[8] = { 0x00, 0x0F, 0x11, 0x0E, 0x04, 0x00, 0x04, 0x00 };    // WiFi
uint8_t SIMBOLO_AR[8] = { 0x00, 0x0E, 0x15, 0x15, 0x15, 0x0E, 0x00, 0x00 };      // Climatizador
uint8_t SIMBOLO_OK[8] = { 0x00, 0x01, 0x02, 0x14, 0x08, 0x04, 0x02, 0x00 };      // √ de OK
uint8_t SIMBOLO_ERRO[8] = { 0x00, 0x11, 0x0A, 0x04, 0x04, 0x0A, 0x11, 0x00 };    // X de erro

// Enum para os sons do buzzer - economiza memória usando constantes
enum SomBuzzer : uint8_t {
  SOM_NENHUM = 0,
  SOM_INICIAR,        // Sistema iniciando
  SOM_PESSOA_ENTROU,  // Pessoa detectada entrando
  SOM_PESSOA_SAIU,    // Pessoa detectada saindo
  SOM_COMANDO,        // Comando recebido
  SOM_ALERTA,         // Alerta geral
  SOM_ERRO,           // Erro
  SOM_CONECTADO,      // WiFi conectado
  SOM_DESCONECTADO,   // WiFi desconectado
  SOM_OK              // Recuperação de erro
};

// === FUNÇÕES PARA DEBUG ===
void debugPrint(const String& msg) {
#if DEBUG_SERIAL
  Serial.println(msg);
#endif
}

void mostrarTelaDebug() {
#if DEBUG_SERIAL
  unsigned long agora = millis();
  static unsigned long ultimoDebug = 0;

  if (agora - ultimoDebug < INTERVALO_DEBUG) return;
  ultimoDebug = agora;

  debugPrint("\n--- STATUS DO SISTEMA ---");
  debugPrint("Temperatura: " + String(sensores.temperatura) + "°C");
  debugPrint("Umidade: " + String(sensores.humidade) + "%");
  debugPrint("LDR valor: " + String(sensores.valorLDR));
  debugPrint("Luminosidade: " + String(sensores.luminosidade) + "%");  debugPrint("Pessoas: " + String(pessoas.total) + " (Tags Hist: " + String(pessoas.count) + ")");
  debugPrint("Temp. Preferida: " + String(pessoas.tempPref) + "°C");
  debugPrint("Lum. Preferida: " + String(pessoas.lumPref) + "%");
  debugPrint("Prefs Atualizadas: " + String(pessoas.prefsAtualizadas ? "SIM" : "NAO"));
  debugPrint("Diff Temp: " + String(sensores.temperatura - pessoas.tempPref) + "°C");
  
  // Debug específico das tags presentes
  if (pessoas.total > 0) {
    debugPrint("--- TAGS PRESENTES ---");
    for (int i = 0; i < pessoas.count; i++) {
      if (pessoas.estado[i]) {
        debugPrint("Tag " + String(i) + ": " + pessoas.tags[i] + " (PRESENTE)");
      }
    }
    debugPrint("Última consulta: " + String(millis() - tempos[5]) + "ms atrás");
  }

  debugPrint("\n--- FLAGS ---");
  debugPrint("modoManualIlum: " + String(flags.modoManualIlum ? "SIM" : "NAO"));
  debugPrint("modoManualClima: " + String(flags.modoManualClima ? "SIM" : "NAO"));
  debugPrint("ilumAtiva: " + String(flags.ilumAtiva ? "SIM" : "NAO"));
  debugPrint("monitorandoLDR: " + String(flags.monitorandoLDR ? "SIM" : "NAO"));
  debugPrint("atualizandoPref: " + String(flags.atualizandoPref ? "SIM" : "NAO"));
  debugPrint("prefsAtualizadas: " + String(pessoas.prefsAtualizadas ? "SIM" : "NAO"));
  debugPrint("erroSensor: " + String(flags.erroSensor ? "SIM" : "NAO"));
  debugPrint("erroConexao: " + String(flags.erroConexao ? "SIM" : "NAO"));

  debugPrint("\n--- CLIMA ---");
  debugPrint("Ligado: " + String(clima.ligado ? "SIM" : "NAO"));
  debugPrint("Velocidade: " + String(clima.velocidade));
  debugPrint("Umidificando: " + String(clima.umidificando ? "SIM" : "NAO"));
  debugPrint("Aleta V: " + String(clima.aletaV ? "SIM" : "NAO"));
  debugPrint("Aleta H: " + String(clima.aletaH ? "SIM" : "NAO"));
  debugPrint("Timer: " + String(clima.timer));

  debugPrint("\n--- SISTEMA ---");
  debugPrint("WiFi: " + String(flags.wifiOk ? "Conectado" : "Desconectado"));
  debugPrint("-------------------------\n");
#endif
}

// === FUNÇÕES PARA FEEDBACK ===
void tocarSom(SomBuzzer tipo) {
  static const uint8_t SONS[][4] = {
    { 0, 0, 0, 0 },    // SOM_NENHUM
    { 20, 10, 5, 3 },  // SOM_INICIAR
    { 25, 5, 0, 1 },   // SOM_PESSOA_ENTROU
    { 15, 5, 0, 1 },   // SOM_PESSOA_SAIU
    { 30, 2, 2, 1 },   // SOM_COMANDO
    { 40, 3, 3, 3 },   // SOM_ALERTA
    { 10, 15, 5, 2 },  // SOM_ERRO
    { 35, 3, 2, 2 },   // SOM_CONECTADO
    { 15, 10, 5, 1 },  // SOM_DESCONECTADO
    { 45, 2, 1, 2 }    // SOM_OK
  };

  if (tipo == SOM_NENHUM) return;

  const uint8_t* som = SONS[tipo];
  int freq = som[0] * 100;
  int duracao = som[1] * 10;
  int pausa = som[2] * 10;
  int repeticoes = som[3];

  for (int r = 0; r < repeticoes; r++) {
    int freqAtual = freq;
    if (tipo == SOM_INICIAR) {
      freqAtual += r * 200;
    }

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

    delay(pausa);
  }
}

void mostrarErroLCD(const char* erro, bool critico = false) {
  static const char MSG_ERRO[] PROGMEM = "ERRO:";

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(7);
  lcd.print(' ');
  lcd.print(MSG_ERRO);
  lcd.setCursor(0, 1);
  lcd.print(erro);

  if (critico) {
    tocarSom(SOM_ERRO);
  } else {
    tocarSom(SOM_ALERTA);
  }
    // Não bloquear por muito tempo - retornar à tela principal após breve exibição
  delay(600);
  
  // Verificar se há pessoas antes de atualizar a tela
  if (pessoas.total > 0) {
    atualizarLCD();
  }
}

void animacaoTransicao() {
  static const uint8_t PADRAO[] = { 0, 1, 3, 7, 15, 14, 12, 8, 0 };

  for (int i = 0; i < 8; i++) {
    lcd.clear();
    lcd.setCursor(i, 0);
    lcd.print(">");
    lcd.setCursor(15 - i, 1);
    lcd.print("<");
    delay(30);
  }
  lcd.clear();
}

void atualizarLCD() {
  // Sempre manter o backlight ligado quando há pessoas
  if (pessoas.total > 0) {
    lcd.backlight();
  } else {
    // Modo standby - tela específica quando não há pessoas
    lcd.setCursor(0, 0);
    lcd.print("Sistema Inativo");
    lcd.setCursor(0, 1);
    lcd.print("Passe o cartao");
    lcd.noBacklight(); // Apagar backlight apenas no standby
    return;
  }

  static uint32_t hashAnterior = 0;
  static unsigned long ultimaAtualizacao = 0;
  unsigned long agora = millis();

  // Limitar frequência de atualização para evitar flickering
  if (agora - ultimaAtualizacao < 200) return;

  uint32_t hash = (uint32_t)(sensores.temperatura * 10) + (uint32_t)(sensores.humidade) * 1000 + sensores.luminosidade * 10000 + pessoas.total * 100000 + (clima.ligado ? 1000000 : 0) + (flags.wifiOk ? 2000000 : 0) + (flags.erroSensor ? 4000000 : 0) + (flags.modoManualIlum ? 8000000 : 0) + (flags.modoManualClima ? 16000000 : 0);

  if (hash == hashAnterior) return;
  hashAnterior = hash;
  ultimaAtualizacao = agora;

  // Só fazer clear se realmente necessário
  lcd.clear();

  // Modos operacionais
  if (flags.modoManualIlum || flags.modoManualClima) {
    lcd.setCursor(0, 0);
    lcd.write(1);
    lcd.print(sensores.temperatura, 1);
    lcd.write(0xDF);
    lcd.print("C ");

    lcd.write(2);
    lcd.print(sensores.humidade, 0);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("Manual Mode ");
    lcd.write(0);
    lcd.print(pessoas.total);

    lcd.setCursor(15, 1);
    lcd.write(flags.wifiOk ? 4 : 7);
  } else {
    lcd.setCursor(0, 0);
    lcd.write(1);
    lcd.print(sensores.temperatura, 1);
    lcd.write(0xDF);
    lcd.print("C ");

    lcd.write(2);
    lcd.print(sensores.humidade, 0);
    lcd.print("%");

    lcd.setCursor(0, 1);

    lcd.write(3);
    lcd.print(sensores.luminosidade);
    lcd.print("% ");

    lcd.write(0);
    lcd.print(pessoas.total);
    lcd.print(" ");

    lcd.write(5);
    lcd.print(clima.ligado ? "L" : "-");

    lcd.setCursor(15, 1);
    lcd.write(flags.wifiOk ? 4 : 7);
  }
}

void atualizarTelaClimatizador() {
  static uint8_t estadoAnterior = 0;
  static unsigned long tempoExibicao = 0;

  uint8_t estadoAtual = (clima.ligado << 0) | (clima.umidificando << 1) | (clima.velocidade << 2) | (clima.timer << 4) | (clima.aletaV << 7);

  if (estadoAtual != estadoAnterior) {
    estadoAnterior = estadoAtual;
    tempoExibicao = millis();
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(5);
    lcd.print(" Climatizador");

    lcd.setCursor(0, 1);
    if (!clima.ligado) {
      lcd.print("Desligado");
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
        buffer[pos++] = 'U';
        buffer[pos++] = 'M';
        buffer[pos++] = ' ';
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
  
  // Retornar automaticamente após 1.5 segundos SEM BLOQUEAR
  if (millis() - tempoExibicao > 1500) {
    atualizarLCD();
  }
}

// === FUNÇÃO DE CONSULTA DE PREFERÊNCIAS OTIMIZADA ===
bool consultarPreferencias() {
  if (pessoas.total == 0 || !flags.wifiOk || flags.atualizandoPref) {
    debugPrint("consultarPreferencias: Condições não atendidas. Total pessoas: " + String(pessoas.total) + ", WiFiOK: " + String(flags.wifiOk) + ", AtualizandoPref: " + String(flags.atualizandoPref));
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
    debugPrint("consultarPreferencias: Nenhuma tag ativa para consultar.");
    return false;
  }

  String jsonTags;
  serializeJson(doc, jsonTags);
  debugPrint("Consultando preferências para " + String(tagsAtivas) + " tags");
  debugPrint("JSON enviado: " + jsonTags);

  HTTPClient http;
  String fullUrl = String(serverUrl) + "preferencias";
  http.begin(fullUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000); // Reduzido de 8000 para 5000
  
  int httpCode = http.POST(jsonTags);
  bool sucesso = false;

  debugPrint("HTTP Code para preferencias: " + String(httpCode));

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    debugPrint("Resposta do servidor para preferencias: " + payload);

    StaticJsonDocument<256> respDoc;
    DeserializationError error = deserializeJson(respDoc, payload);

    if (!error) {
      // Sempre aplicar as preferências recebidas do servidor
      if (respDoc.containsKey("temperatura")) {
        float tempPref = respDoc["temperatura"];
        debugPrint("Temperatura preferida calculada pelo servidor: " + String(tempPref));

        if (!isnan(tempPref) && tempPref >= 16.0 && tempPref <= 32.0) {
          float tempAnterior = pessoas.tempPref;
          pessoas.tempPref = tempPref;
          debugPrint("✓ Temperatura preferida aplicada: " + String(tempAnterior) + "°C -> " + String(pessoas.tempPref) + "°C");
        } else {
          debugPrint("⚠ Temperatura inválida, usando padrão 25°C");
          pessoas.tempPref = 25.0;
        }
      } else {
        debugPrint("⚠ Temperatura não encontrada, usando padrão 25°C");
        pessoas.tempPref = 25.0;
      }

      if (respDoc.containsKey("luminosidade")) {
        int lumPref = respDoc["luminosidade"];
        debugPrint("Luminosidade preferida calculada pelo servidor: " + String(lumPref));

        if (lumPref >= 0 && lumPref <= 100 && (lumPref % 25 == 0)) {
          int lumAnterior = pessoas.lumPref;
          pessoas.lumPref = lumPref;
          debugPrint("✓ Luminosidade preferida aplicada: " + String(lumAnterior) + "% -> " + String(pessoas.lumPref) + "%");
        } else {
          debugPrint("⚠ Luminosidade inválida (" + String(lumPref) + "), usando padrão 50%");
          pessoas.lumPref = 50;
        }
      } else {
        debugPrint("⚠ Luminosidade não encontrada, usando padrão 50%");
        pessoas.lumPref = 50;
      }

      sucesso = true;
      pessoas.prefsAtualizadas = true;
      
      // Log final das preferências aplicadas
      debugPrint("=== PREFERÊNCIAS FINAIS APLICADAS ===");
      debugPrint("Temperatura: " + String(pessoas.tempPref) + "°C");
      debugPrint("Luminosidade: " + String(pessoas.lumPref) + "%");
      debugPrint("Flag prefsAtualizadas: " + String(pessoas.prefsAtualizadas ? "TRUE" : "FALSE"));
      debugPrint("===================================");
    } else {
      debugPrint("Erro ao parsear JSON da resposta de preferencias: " + String(error.c_str()));
      mostrarErroLCD("Erro JSON Pref", false);
    }
  } else {
    debugPrint("Erro na requisição HTTP para preferencias: " + String(httpCode));
    mostrarErroLCD("Erro API Pref", false);
  }

  http.end();
  flags.atualizandoPref = false;
  
  // Não forçar atualização do LCD aqui para evitar travamentos
  return sucesso;
}

// === FUNÇÕES PRINCIPAIS ===
void configurarRele(int nivel) {
  static int nivelAnterior = -1;
  if (nivel == nivelAnterior) return;

  nivel = (nivel / 25) * 25;
  if (nivel > 100) nivel = 100;

  // Corrigindo a lógica dos relés - invertendo para LOW = ligado
  const bool estados[5][4] = {
    { HIGH, HIGH, HIGH, HIGH },  // 0% - todos desligados
    { LOW, HIGH, HIGH, HIGH },   // 25% - só R1 ligado
    { LOW, LOW, HIGH, HIGH },    // 50% - R1 e R2 ligados
    { LOW, LOW, LOW, HIGH },     // 75% - R1, R2 e R3 ligados
    { LOW, LOW, LOW, LOW }       // 100% - todos ligados
  };

  int indice = nivel / 25;
  if (indice >= 0 && indice <= 4) {
    digitalWrite(RELE_1, estados[indice][0]);
    digitalWrite(RELE_2, estados[indice][1]);
    digitalWrite(RELE_3, estados[indice][2]);
    digitalWrite(RELE_4, estados[indice][3]);
    sensores.luminosidade = nivel;

    debugPrint("Rele configurado: " + String(nivel) + "% (indice " + String(indice) + ")");
    debugPrint("Estados dos relés: R1=" + String(estados[indice][0] == LOW ? "ON" : "OFF") + 
               ", R2=" + String(estados[indice][1] == LOW ? "ON" : "OFF") + 
               ", R3=" + String(estados[indice][2] == LOW ? "ON" : "OFF") + 
               ", R4=" + String(estados[indice][3] == LOW ? "ON" : "OFF"));

    // Só tocar som se não estivermos em processo de reset do sistema
    if (pessoas.total > 0) {
      if (nivel > 0 && nivelAnterior == 0) {
        tocarSom(SOM_COMANDO);
      } else if (nivel == 0 && nivelAnterior > 0) {
        tocarSom(SOM_COMANDO);
      } else if (abs(nivel - nivelAnterior) >= 50) {
        tocarSom(SOM_COMANDO);
      }
    }

    nivelAnterior = nivel;
    
    // Só atualizar LCD se há pessoas presentes (não durante reset)
    if (pessoas.total > 0) {
      atualizarLCD();
    }
  }
}

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
      debugPrint("Sensores DHT recuperados.");
    }
    sensores.temperatura = temp;
    sensores.humidade = hum;
    sensores.dadosValidos = true;
  } else {
    if (!sensores.dadosValidos) {
      sensores.temperatura = 25.0;
      sensores.humidade = 50.0;
      sensores.dadosValidos = true;
    }    if (!flags.erroSensor) {
      flags.erroSensor = true;
      debugPrint("ERRO: Falha na leitura do DHT");
      mostrarErroLCD("Sensor DHT", false);
    }
  }

  static int ldrBuffer[3] = { 0 };
  static int bufferIndex = 0;

  ldrBuffer[bufferIndex] = analogRead(LDR_PIN);
  bufferIndex = (bufferIndex + 1) % 3;
  sensores.valorLDR = (ldrBuffer[0] + ldrBuffer[1] + ldrBuffer[2]) / 3;

  // Só atualizar LCD se há pessoas presentes
  if (pessoas.total > 0) {
    atualizarLCD();
  }
}

bool enviarHTTP(const String& endpoint, const String& dados = "", bool isPost = false) {
  if (!flags.wifiOk) {
    return false;
  }

  HTTPClient http;
  String fullUrl = String(serverUrl) + endpoint;
  http.begin(fullUrl);
  http.setTimeout(5000);

  bool sucesso = false;
  int httpCode = 0;

  if (isPost) {
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(dados);
    sucesso = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT);
  } else {
    httpCode = http.GET();
    sucesso = (httpCode == HTTP_CODE_OK);
  }

  if (!sucesso) {
    debugPrint("enviarHTTP: Erro (" + String(httpCode) + ") ao enviar para " + endpoint + " Dados: " + dados.substring(0, min(dados.length(), (unsigned int)100)));    if (!flags.erroConexao) {
      flags.erroConexao = true;
      mostrarErroLCD("Falha Conexao", false);
    }
  } else {
    if (flags.erroConexao) {
      flags.erroConexao = false;
      tocarSom(SOM_OK);
    }
  }

  http.end();
  return sucesso;
}

void enviarDados() {
  static bool forcarEnvio = false;
  
  unsigned long agora = millis();
  if (!forcarEnvio && agora - tempos[1] < INTERVALO_DADOS) return;
  tempos[1] = agora;

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
      if (pessoas.estado[i]) {
        tagsArray.add(pessoas.tags[i]);
      }
    }
  }

  String dados;
  serializeJson(doc, dados);

  debugPrint("Enviando dados ambiente: " + dados);

  bool sucesso = enviarHTTP("ambiente", dados, true);
  
  // Se for o último envio antes do reset, mostrar feedback
  if (pessoas.total == 0 && forcarEnvio) {
    debugPrint("Envio final " + String(sucesso ? "BEM-SUCEDIDO" : "FALHOU"));
  }
  
  forcarEnvio = false;
}

// Função auxiliar para forçar envio imediato
void enviarDadosImediato() {
  static bool forcarEnvio = true;
  
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
      if (pessoas.estado[i]) {
        tagsArray.add(pessoas.tags[i]);
      }
    }
  }

  String dados;
  serializeJson(doc, dados);

  debugPrint("ENVIO FORÇADO - dados ambiente: " + dados);

  bool sucesso = enviarHTTP("ambiente", dados, true);
  debugPrint("Resultado envio forçado: " + String(sucesso ? "SUCESSO" : "FALHA"));
}

void processarNFC() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  String tag = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) tag += "0";
    tag += String(mfrc522.uid.uidByte[i], HEX);
  }  tag.toUpperCase();

  debugPrint("Tag NFC lida: " + tag);
  gerenciarPresenca(tag);
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void gerenciarPresenca(const String& tag) {
  int indice = -1;
  bool entrando = false;
  int totalAnterior = pessoas.total;

  for (int i = 0; i < pessoas.count; i++) {
    if (pessoas.tags[i] == tag) {
      indice = i;
      break;
    }
  }
  if (indice == -1) {
    if (pessoas.count < 10) {
      pessoas.tags[pessoas.count] = tag;
      pessoas.estado[pessoas.count] = true;
      pessoas.total++;
      pessoas.count++;
      entrando = true;      if (pessoas.total == 1) {
        flags.monitorandoLDR = true;
        flags.ilumAtiva = false;
        flags.modoManualIlum = false;
        flags.modoManualClima = false;
        lcd.backlight(); // Religar o backlight quando a primeira pessoa entra
        debugPrint("Primeira pessoa detectada. Automação reativada e backlight ligado.");
      }

      debugPrint("Nova pessoa detectada - Tag: " + tag + ", Total: " + String(pessoas.total));      tocarSom(SOM_PESSOA_ENTROU);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.write(0);
      lcd.print(" Bem-vindo!");
      lcd.setCursor(0, 1);
      lcd.print("Pessoas: ");
      lcd.print(pessoas.total);
      delay(400);

    } else {
      debugPrint("Limite de pessoas atingido (10). Ignorando nova tag: " + tag);
      tocarSom(SOM_ERRO);
      delay(200);
    }} else if (pessoas.estado[indice]) {
    pessoas.estado[indice] = false;
    pessoas.total--;

    debugPrint("Pessoa saindo - Tag: " + tag + ", Total: " + String(pessoas.total));    tocarSom(SOM_PESSOA_SAIU);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(0);
    lcd.print(" Ate logo!");
    lcd.setCursor(0, 1);
    lcd.print("Pessoas: ");
    lcd.print(pessoas.total);
    delay(400);

    // Enviar dados IMEDIATAMENTE para registrar a saída
    enviarDadosImediato();
    
    if (pessoas.total == 0) {
      // Garantir que a saída da última pessoa seja registrada
      debugPrint("ÚLTIMA PESSOA SAINDO - Enviando dados críticos múltiplas vezes");
      
      delay(1000);
      enviarDadosImediato();
      
      delay(1500); 
      enviarDadosImediato();
      
      delay(2000);
      resetarSistema();
      return; // Importante: sair da função aqui para evitar chamadas desnecessárias
    }
  } else {
    pessoas.estado[indice] = true;
    pessoas.total++;
    entrando = true;

    debugPrint("Pessoa voltando - Tag: " + tag + ", Total: " + String(pessoas.total));

    tocarSom(SOM_PESSOA_ENTROU);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(0);
    lcd.print(" Bem-vindo!");
    lcd.setCursor(0, 1);
    lcd.print("Pessoas: ");
    lcd.print(pessoas.total);
    delay(500);
  }  if ((totalAnterior != pessoas.total || entrando) && pessoas.total > 0 && !flags.atualizandoPref) {
    debugPrint("=== MUDANÇA DETECTADA ===");
    debugPrint("Total anterior: " + String(totalAnterior) + " -> Atual: " + String(pessoas.total));
    debugPrint("Entrando: " + String(entrando ? "SIM" : "NAO"));
    debugPrint("Atualizando preferências...");
    pessoas.prefsAtualizadas = false;
    
    // Consultar preferências e forçar atualização da iluminação
    if (consultarPreferencias()) {
      debugPrint("Preferências atualizadas com sucesso. Forçando reavaliação da iluminação...");
      // Forçar reavaliação da iluminação após receber as preferências
      if (flags.ilumAtiva && !flags.modoManualIlum) {
        int nivelDesejado = pessoas.lumPref;
        if (nivelDesejado == 0) nivelDesejado = 25; // Mínimo de segurança
        
        if (sensores.luminosidade != nivelDesejado) {
          debugPrint("FORÇANDO ajuste imediato de " + String(sensores.luminosidade) + "% para " + String(nivelDesejado) + "%");
          configurarRele(nivelDesejado);
        }
      }
    }
  }

  atualizarLCD();

  enviarDados();
}

void resetarSistema() {
  // Mostrar tela de desativação e manter até o final
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Desativando...");
  lcd.setCursor(0, 1);
  lcd.print("Sistema");
  tocarSom(SOM_ALERTA);
  delay(800); // Dar tempo para ver a mensagem

  enviarDados();

  // Desligar climatizador se estiver ligado (sem mostrar outras telas)
  if (clima.ligado) {
    lcd.setCursor(0, 1);
    lcd.print("Desl. Clima...  "); // Atualizar apenas a segunda linha
    enviarComandoIR(IR_POWER);
    delay(500); // Dar tempo para o comando IR
  }

  // Desligar iluminação
  lcd.setCursor(0, 1);
  lcd.print("Desl. Luzes... "); // Atualizar apenas a segunda linha
  flags.modoManualIlum = false;
  flags.modoManualClima = false;
  flags.ilumAtiva = false;
  flags.monitorandoLDR = true;
  configurarRele(0);
  delay(300);

  // Limpar completamente a estrutura de pessoas para evitar estados inconsistentes
  for (int i = 0; i < pessoas.count; i++) {
    pessoas.tags[i] = "";
    pessoas.estado[i] = false;
  }
  pessoas.total = 0;
  pessoas.count = 0;
  pessoas.prefsAtualizadas = false;

  // Mostrar conclusão da desativação
  lcd.setCursor(0, 1);
  lcd.print("Concluido!     "); // Atualizar apenas a segunda linha
  delay(1000);

  debugPrint("Sistema resetado completamente - Modo de espera");
  
  // Apagar o backlight do LCD quando não há ninguém
  lcd.noBacklight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistema Inativo");
  lcd.setCursor(0, 1);
  lcd.print("Passe o cartao");
  
  debugPrint("Backlight do LCD desligado - Sistema em standby");
}

void gerenciarIluminacao() {
  unsigned long agora = millis();
  if (agora - tempos[2] < INTERVALO_LDR) return;
  tempos[2] = agora;

  // Se não há pessoas, desligar luzes automaticamente
  if (pessoas.total == 0) {
    if (flags.ilumAtiva) {
      debugPrint("Desligando luzes: Nenhuma pessoa presente");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.write(3);
      lcd.print(" Luz Auto Off");
      lcd.setCursor(0, 1);
      lcd.print("Nenhuma pessoa");
      tocarSom(SOM_COMANDO);
      delay(800);

      configurarRele(0);
      flags.ilumAtiva = false;
      flags.modoManualIlum = false; // Reset do modo manual quando não há pessoas
      atualizarLCD();
    }
    return;
  }

  // Se está em modo manual, NÃO fazer nada automaticamente
  if (flags.modoManualIlum) {
    debugPrint("Modo manual ativo - ignorando controle automático de iluminação");
    return;
  }

  // MUDANÇA CRÍTICA: Verificar se as preferências foram atualizadas antes de ligar
  if (!pessoas.prefsAtualizadas && pessoas.total > 0) {
    debugPrint("Aguardando atualização das preferências antes de gerenciar iluminação...");
    return;
  }
  
  if (!flags.ilumAtiva) {
    if (sensores.valorLDR < LIMIAR_LDR_ESCURIDAO) {
      // CORREÇÃO: Usar a preferência correta mesmo se for 0
      int nivel = pessoas.lumPref;
      
      // Se a preferência for 0%, forçar pelo menos 25% para segurança
      if (nivel == 0) {
        nivel = 25;
        debugPrint("⚠ Preferência é 0% - aplicando 25% mínimo para segurança");
      }

      debugPrint("=== LIGANDO LUZES AUTOMÁTICO ===");
      debugPrint("LDR: " + String(sensores.valorLDR) + " < " + String(LIMIAR_LDR_ESCURIDAO));
      debugPrint("Preferência recebida: " + String(pessoas.lumPref) + "%");
      debugPrint("Nível aplicado: " + String(nivel) + "%");
      debugPrint("Prefs atualizadas: " + String(pessoas.prefsAtualizadas ? "SIM" : "NAO"));
      debugPrint("================================");      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.write(3);
      lcd.print(" Luz Auto ON ");
      lcd.print(nivel);
      lcd.print("%");
      lcd.setCursor(0, 1);
      lcd.print("Pref: ");
      lcd.print(pessoas.lumPref);
      lcd.print("% LDR:");
      lcd.print(sensores.valorLDR);
      tocarSom(SOM_COMANDO);
      delay(800);

      configurarRele(nivel);
      flags.ilumAtiva = true;
      atualizarLCD();
    } else {
      debugPrint("Luzes OFF: LDR=" + String(sensores.valorLDR) + " >= " + String(LIMIAR_LDR_ESCURIDAO) + ". Aguardando escuridão. Pref: " + String(pessoas.lumPref) + "%");
    }
  } else {
    // Verificar se o nível atual corresponde à preferência
    int nivelDesejado = pessoas.lumPref;
    if (nivelDesejado == 0) nivelDesejado = 25; // Mínimo de segurança
    
    if (sensores.luminosidade != nivelDesejado && pessoas.prefsAtualizadas) {
      debugPrint("AJUSTANDO nível de " + String(sensores.luminosidade) + "% para " + String(nivelDesejado) + "% (nova preferência)");
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.write(3);
      lcd.print(" Ajuste Auto");
      lcd.setCursor(0, 1);
      lcd.print(sensores.luminosidade);
      lcd.print("% -> ");
      lcd.print(nivelDesejado);
      lcd.print("%");
      tocarSom(SOM_COMANDO);
      delay(800);
      
      configurarRele(nivelDesejado);
      atualizarLCD();
    } else {
      debugPrint("Luzes ON: Mantendo " + String(sensores.luminosidade) + "%. Pessoas presentes, modo auto ativo.");
    }
  }
}

bool enviarComandoIR(uint8_t comando) {
  unsigned long agora = millis();

  static unsigned long ultimoComandoIR = 0;
  if (agora - ultimoComandoIR < DEBOUNCE_ENVIAR) {
    debugPrint("Comando IR ignorado (debounce): 0x" + String(comando, HEX));
    return false;
  }

  if (controleIR.estado != IR_OCIOSO) {
    debugPrint("Comando IR ignorado (estado ocupado): 0x" + String(comando, HEX));
    return false;
  }

  debugPrint("Enviando comando: 0x" + String(comando, HEX));

  controleIR.estado = IR_ENVIANDO;
  controleIR.comandoPendente = comando;
  controleIR.inicioEnvio = agora;
  controleIR.comandoConfirmado = false;

  for (int i = 0; i < 3; i++) {
    IrSender.sendNEC(IR_ENDERECO, comando, 0);
    debugPrint("Enviado (tentativa " + String(i + 1) + "): 0x" + String(comando, HEX));
    delay(5);
  }

  controleIR.estado = IR_AGUARDANDO_CONFIRMACAO;
  ultimoComandoIR = agora;

  return true;
}

void atualizarEstadoClima(uint8_t comando) {
  bool estadoAnteriorLigado = clima.ligado;

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
      debugPrint("Climatizador: " + String(clima.ligado ? "LIGADO" : "DESLIGADO"));
      break;
    case IR_UMIDIFICAR:
      if (clima.ligado) {
        clima.umidificando = !clima.umidificando;
        debugPrint("Umidificação: " + String(clima.umidificando ? "LIGADA" : "DESLIGADA"));
      }
      break;
    case IR_VELOCIDADE:
      if (clima.ligado) {
        clima.velocidade = (clima.velocidade % 3) + 1;
        clima.ultimaVel = clima.velocidade;
        debugPrint("Velocidade: " + String(clima.velocidade));
      }
      break;
    case IR_TIMER:
      if (clima.ligado) {
        clima.timer = (clima.timer + 1) % 6;
        debugPrint("Timer: " + String(clima.timer));
      }
      break;
    case IR_ALETA_VERTICAL:
      if (clima.ligado) {
        clima.aletaV = !clima.aletaV;
        debugPrint("Aleta vertical: " + String(clima.aletaV ? "ATIVA" : "INATIVA"));
      }
      break;
    case IR_ALETA_HORIZONTAL:
      if (clima.ligado) {
        clima.aletaH = !clima.aletaH;
        debugPrint("Aleta horizontal: " + String(clima.aletaH ? "ATIVA" : "INATIVA"));
      }
      break;
  }

  if (estadoAnteriorLigado != clima.ligado || comando != 0) {
    clima.ultimaAtualizacao = millis();
  }
}

void controleAutomaticoClima() {
  if (pessoas.total <= 0 || flags.modoManualClima) {
    if (pessoas.total == 0 && clima.ligado) {
      debugPrint("Desligando climatizador - Nenhuma pessoa presente.");
      enviarComandoIR(IR_POWER);
    }
    return;
  }
  if (!pessoas.prefsAtualizadas) {
    debugPrint("Preferências não atualizadas, aguardando para controle de clima...");
    return;
  }

  unsigned long agora = millis();
  if (agora - tempos[3] < INTERVALO_CLIMA_AUTO) return;
  tempos[3] = agora;

  float diff = sensores.temperatura - pessoas.tempPref;

  debugPrint("=== CONTROLE AUTOMÁTICO CLIMA ===");
  debugPrint("Temperatura atual: " + String(sensores.temperatura) + "°C");
  debugPrint("Temperatura preferida: " + String(pessoas.tempPref) + "°C");
  debugPrint("Diferença: " + String(diff) + "°C");
  debugPrint("Climatizador ligado: " + String(clima.ligado ? "SIM" : "NAO"));
  debugPrint("================================");

  if (diff >= 2.0 && !clima.ligado) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(5);
    lcd.print(" Auto Ligar");
    lcd.setCursor(0, 1);
    lcd.print("Temp: +");
    lcd.print(diff, 1);
    lcd.print(" graus");
    tocarSom(SOM_COMANDO);    debugPrint("LIGANDO climatizador automaticamente!");

    delay(800);

    enviarComandoIR(IR_POWER);
    atualizarLCD();
    
    if (clima.velocidade != 1) {
      debugPrint("Ajustando velocidade para 1 (default auto)");
      int toquesNecessarios = (clima.velocidade == 2) ? 2 : (clima.velocidade == 3) ? 1 : 0;

      for (int i = 0; i < toquesNecessarios; i++) {
        if (enviarComandoIR(IR_VELOCIDADE)) {
          delay(300);
        }
      }
    }
  } else if (diff <= -0.5 && clima.ligado) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(5);
    lcd.print(" Auto Deslig.");
    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.print(diff, 1);
    lcd.print(" graus");
    tocarSom(SOM_COMANDO);

    debugPrint("DESLIGANDO climatizador automaticamente!");

    delay(800);

    enviarComandoIR(IR_POWER);
    atualizarLCD();
  } else if (clima.ligado) {
    int velDesejada = 1;
    if (diff >= 4.5) {
      velDesejada = 3;
    } else if (diff >= 3.0) {
      velDesejada = 2;
    }

    if (clima.velocidade != velDesejada) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.write(5);
      lcd.print(" Ajuste Auto");
      lcd.setCursor(0, 1);
      lcd.print("Vel ");
      lcd.print(clima.velocidade);
      lcd.print(" -> ");
      lcd.print(velDesejada);
      tocarSom(SOM_COMANDO);

      debugPrint("Ajustando velocidade: " + String(clima.velocidade) + " -> " + String(velDesejada));      delay(800);

      int tentativas = 0;
      while (clima.velocidade != velDesejada && tentativas < 3) {
        if (enviarComandoIR(IR_VELOCIDADE)) {
          tentativas++;
          unsigned long t0 = millis();
          while (controleIR.estado != IR_OCIOSO && millis() - t0 < TIMEOUT_CONFIRMACAO + 200) {
            processarIRRecebido();  // Processa possíveis confirmações
            delay(10);
          }
        }
      }
      atualizarLCD();
    }
  }

  if (clima.ligado && !flags.modoManualClima) {
    if (pessoas.total == 1) {
      if (!clima.aletaV) {
        debugPrint("Aleta vertical: Ativando (1 pessoa)");
        enviarComandoIR(IR_ALETA_VERTICAL);
        delay(300);
      }
      if (clima.aletaH) {
        debugPrint("Aleta horizontal: Desativando (1 pessoa)");
        enviarComandoIR(IR_ALETA_HORIZONTAL);
        delay(300);
      }
    } else if (pessoas.total > 1) {
      if (!clima.aletaV) {
        debugPrint("Aleta vertical: Ativando (>1 pessoa)");
        enviarComandoIR(IR_ALETA_VERTICAL);
        delay(300);
      }
      if (!clima.aletaH) {
        debugPrint("Aleta horizontal: Ativando (>1 pessoa)");
        enviarComandoIR(IR_ALETA_HORIZONTAL);
        delay(300);
      }
    }
  }
}

void verificarComandos() {
  // === BLOCO 1: VALIDAÇÃO INICIAL ===
  unsigned long agora = millis();
  if (agora - tempos[4] < INTERVALO_COMANDOS || !flags.wifiOk) return;
  tempos[4] = agora;

  // === BLOCO 2: COMANDOS DE ILUMINAÇÃO ===
  processarComandosIluminacao();
  
  // === BLOCO 3: COMANDOS DO CLIMATIZADOR ===
  processarComandosClimatizador();
  
  // === BLOCO 4: SINCRONIZAÇÃO DE MODO MANUAL ===
  sincronizarModoManualClimatizador();
}

// === FUNÇÃO AUXILIAR: PROCESSAR COMANDOS DE ILUMINAÇÃO ===
void processarComandosIluminacao() {
  HTTPClient http;
  String urlIlum = String(serverUrl) + "comando";
  http.begin(urlIlum);
  int httpCodeIlum = http.GET();

  if (httpCodeIlum == HTTP_CODE_OK) {
    String cmd = http.getString();
    cmd.trim();
    
    // AÇÃO 1: COMANDO MANUAL DE NÍVEL (0, 25, 50, 75, 100)
    if (cmd != "auto" && cmd.length() > 0 && pessoas.total > 0) {
      processarComandoNivelLuz(cmd);
    } 
    // AÇÃO 2: COMANDO AUTOMÁTICO
    else if (cmd == "auto" && flags.modoManualIlum) {
      processarComandoAutoLuz();
    }
  } else {
    debugPrint("Erro ao verificar comando de iluminação: " + String(httpCodeIlum));
  }
  http.end();
}

// === AÇÃO 1: PROCESSAR COMANDO DE NÍVEL DE LUZ ===
void processarComandoNivelLuz(String cmd) {
  int nivel = cmd.toInt();
  
  // Validar nível (0, 25, 50, 75, 100)
  if (nivel >= 0 && nivel <= 100 && (nivel % 25 == 0)) {
    bool enteringManualMode = !flags.modoManualIlum;
    bool nivelDiferente = (sensores.luminosidade != nivel);
    
    // Ativar modo manual
    flags.modoManualIlum = true;
    flags.ilumAtiva = true;
    
    // Mostrar feedback visual na primeira entrada no modo manual
    if (enteringManualMode) {
      mostrarFeedbackModoManualLuz(nivel);
    }
    
    // Configurar relé apenas se o nível for diferente
    if (nivelDiferente) {
      debugPrint("Comando APP: luz manual " + String(nivel) + "% (anterior: " + String(sensores.luminosidade) + "%)");
      configurarRele(nivel);
    } else {
      debugPrint("Comando APP: mantendo " + String(nivel) + "% (já no nível correto)");
    }
  }
}

// === AÇÃO 2: PROCESSAR COMANDO AUTOMÁTICO DE LUZ ===
void processarComandoAutoLuz() {
  flags.modoManualIlum = false;
  debugPrint("Comando APP: saindo do modo manual para automático");
  
  if (pessoas.total > 0) {
    flags.monitorandoLDR = true;
    flags.ilumAtiva = false; // Reset para reavaliação automática
  } else {
    configurarRele(0); // Desligar se não há pessoas
  }
  atualizarLCD();
}

// === FUNÇÃO AUXILIAR: PROCESSAR COMANDOS DO CLIMATIZADOR ===
void processarComandosClimatizador() {
  HTTPClient http;
  String urlClimaCmd = String(serverUrl) + "climatizador/comando";
  http.begin(urlClimaCmd);
  int httpCodeClimaCmd = http.GET();

  if (httpCodeClimaCmd == HTTP_CODE_OK) {
    String cmd = http.getString();
    cmd.trim();

    // AÇÃO 3: COMANDO DO CLIMATIZADOR
    if (cmd != "none" && cmd.length() > 0) {
      processarComandoClimatizador(cmd);
    }
  } else {
    debugPrint("Erro ao verificar comando do climatizador: " + String(httpCodeClimaCmd));
  }
  http.end();
}

// === AÇÃO 3: PROCESSAR COMANDO ESPECÍFICO DO CLIMATIZADOR ===
void processarComandoClimatizador(String cmd) {
  // Ativar modo manual na primeira execução
  if (!flags.modoManualClima) {
    ativarModoManualClimatizador("APP");
  }

  // Mostrar feedback do comando
  mostrarFeedbackComandoClimatizador(cmd);
  
  // Executar comando IR correspondente
  executarComandoIR(cmd);
  
  atualizarLCD();
}

// === FUNÇÃO AUXILIAR: SINCRONIZAR MODO MANUAL ===
void sincronizarModoManualClimatizador() {
  HTTPClient http;
  String urlClimaManual = String(serverUrl) + "climatizador/manual";
  http.begin(urlClimaManual);
  int httpCodeClimaManual = http.GET();

  if (httpCodeClimaManual == HTTP_CODE_OK) {
    String resposta = http.getString();
    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, resposta);

    if (!error && doc.containsKey("modoManualClimatizador")) {
      bool modoManual = doc["modoManualClimatizador"];

      // AÇÃO 4: SINCRONIZAR MODO MANUAL
      if (modoManual != flags.modoManualClima) {
        sincronizarModoClima(modoManual);
      }
    }
  } else {
    debugPrint("Erro ao verificar modo manual do climatizador: " + String(httpCodeClimaManual));
  }
  http.end();
}

// === FUNÇÕES DE APOIO PARA FEEDBACK VISUAL ===
void mostrarFeedbackModoManualLuz(int nivel) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(3);
  lcd.print(" Luz Manual ON");
  lcd.setCursor(0, 1);
  lcd.print("Nivel: ");
  lcd.print(nivel);
  lcd.print("%");
  tocarSom(SOM_COMANDO);
  delay(800);
  debugPrint("Entrando no modo manual de iluminação");
}

void ativarModoManualClimatizador(String origem) {
  flags.modoManualClima = true;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(5);
  lcd.print(" Clima: Manual");
  lcd.setCursor(0, 1);
  lcd.print("Control: ");
  lcd.print(origem);
  tocarSom(SOM_COMANDO);
  delay(800);
}

void mostrarFeedbackComandoClimatizador(String cmd) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(5);
  lcd.print(" Comando App");
  lcd.setCursor(0, 1);
  
  // Mapear comandos para descrições
  if (cmd == "power") lcd.print("Power Toggle");
  else if (cmd == "velocidade") lcd.print("Velocidade");
  else if (cmd == "timer") lcd.print("Timer");
  else if (cmd == "umidificar") lcd.print("Umidificar");
  else if (cmd == "aleta_vertical") lcd.print("Aleta Vertical");
  else if (cmd == "aleta_horizontal") lcd.print("Aleta Horizontal");
  
  tocarSom(SOM_COMANDO);
  delay(800);
  debugPrint("Comando do app para climatizador: " + cmd);
}

void executarComandoIR(String cmd) {
  if (cmd == "power") enviarComandoIR(IR_POWER);
  else if (cmd == "velocidade") enviarComandoIR(IR_VELOCIDADE);
  else if (cmd == "timer") enviarComandoIR(IR_TIMER);
  else if (cmd == "umidificar") enviarComandoIR(IR_UMIDIFICAR);
  else if (cmd == "aleta_vertical") enviarComandoIR(IR_ALETA_VERTICAL);
  else if (cmd == "aleta_horizontal") enviarComandoIR(IR_ALETA_HORIZONTAL);
}

void sincronizarModoClima(bool modoManual) {
  flags.modoManualClima = modoManual;

  if (flags.modoManualClima) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(5);
    lcd.print(" Clima: Manual");
    lcd.setCursor(0, 1);
    lcd.print("Control: Sync");
    tocarSom(SOM_COMANDO);
    delay(800);
  } else {
    debugPrint("Modo do climatizador alterado para: Automático (via sync)");
  }
  atualizarLCD();
}
void setup() {
  Serial.begin(115200);
  debugPrint("\n=== SISTEMA INICIANDO ===");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELE_1, OUTPUT);
  pinMode(RELE_2, OUTPUT);
  pinMode(RELE_3, OUTPUT);
  pinMode(RELE_4, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  // Inicializar todos os relés como desligados (HIGH = desligado)
  digitalWrite(RELE_1, HIGH);
  digitalWrite(RELE_2, HIGH);
  digitalWrite(RELE_3, HIGH);
  digitalWrite(RELE_4, HIGH);

  debugPrint("Pinos configurados e relés inicializados como desligados.");

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

  debugPrint("LCD inicializado.");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Climate v3.0");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando");

  for (int i = 0; i < 3; i++) {
    delay(200);
    lcd.print(".");
  }
  tocarSom(SOM_INICIAR);

  // Testar todos os relés na inicialização
  testarReles();

  configurarRele(0);

  SPI.begin();
  mfrc522.PCD_Init();
  dht.begin();

  debugPrint("Módulos SPI e DHT inicializados.");

  // Initialize IR sender and receiver
  IrSender.begin(IR_SEND_PIN, true, IR_SEND_PIN);  // Use same pin for feedback LED
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  debugPrint("Módulos IR inicializados.");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Conectando WiFi");
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  debugPrint("Conectando ao WiFi: " + String(ssid));

  WiFi.begin(ssid, password);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) {
    delay(500);
    lcd.setCursor(15, 1);
    if (tentativas % 2 == 0) lcd.print(".");
    else lcd.print(" ");
    tentativas++;
  }

  flags.wifiOk = (WiFi.status() == WL_CONNECTED);

  if (flags.wifiOk) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(4);
    lcd.print(" Conectado!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    tocarSom(SOM_CONECTADO);

    debugPrint("WiFi conectado: " + WiFi.localIP().toString());
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(7);
    lcd.print(" Sem WiFi");
    lcd.setCursor(0, 1);
    lcd.print("Modo Offline");
    tocarSom(SOM_ERRO);
    debugPrint("Falha na conexão WiFi");
  }

  unsigned long agora = millis();
  for (int i = 0; i < 8; i++) {
    tempos[i] = agora - (INTERVALO_DADOS + 1000);
  }
  lerSensores();
  delay(1000);
  
  // Sistema inicia em modo standby sem ninguém presente
  lcd.noBacklight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistema Inativo");
  lcd.setCursor(0, 1);
  lcd.print("Passe o cartao");
  
  debugPrint("=== SISTEMA INICIALIZADO - STANDBY ===");
}

void loop() {
  // Processamento prioritário - sempre executado
  processarIRRecebido();
  processarNFC();

  static unsigned long ultimoCicloRapido = 0;
  static unsigned long ultimoCicloComunicacao = 0;
  unsigned long agora = millis();

  // Ciclo rápido (100ms) para sensores e controles críticos
  if (agora - ultimoCicloRapido > 100) {
    ultimoCicloRapido = agora;
    
    lerSensores();
    gerenciarIluminacao();
    controleAutomaticoClima();
  }

  // Ciclo de comunicação (1000ms) para APIs e comandos
  if (agora - ultimoCicloComunicacao > 1000) {
    ultimoCicloComunicacao = agora;

    monitorarWiFi();
    verificarComandos();

    // Verificação periódica de preferências
    if (agora - tempos[5] >= INTERVALO_PREF_CHECK) {
      tempos[5] = agora;
      if (pessoas.total > 0) {
        debugPrint("Verificando atualizações de preferências...");
        consultarPreferencias();
      } else {
        pessoas.prefsAtualizadas = false;
      }
    }

    // Comandos de teste (apenas se não há pessoas presentes)
    if (pessoas.total == 0 && Serial.available() > 0) {
      String comando = Serial.readString();
      comando.trim();
      comando.toLowerCase();
      
      if (comando == "testar_reles") {
        debugPrint("=== COMANDO MANUAL: TESTE DE RELÉS ===");
        testarReles();
      } else if (comando == "testar_niveis") {
        debugPrint("=== COMANDO MANUAL: TESTE DE NÍVEIS ===");
        testarNiveisLuminosidade();
      } else if (comando == "debug_preferencias") {
        debugPrint("=== COMANDO MANUAL: DEBUG PREFERÊNCIAS ===");
        debugPrint("pessoas.total: " + String(pessoas.total));
        debugPrint("pessoas.tempPref: " + String(pessoas.tempPref));
        debugPrint("pessoas.lumPref: " + String(pessoas.lumPref));
        debugPrint("pessoas.prefsAtualizadas: " + String(pessoas.prefsAtualizadas ? "TRUE" : "FALSE"));
        debugPrint("flags.atualizandoPref: " + String(flags.atualizandoPref ? "TRUE" : "FALSE"));
        debugPrint("sensores.luminosidade: " + String(sensores.luminosidade));
        debugPrint("flags.ilumAtiva: " + String(flags.ilumAtiva ? "TRUE" : "FALSE"));
        debugPrint("flags.modoManualIlum: " + String(flags.modoManualIlum ? "TRUE" : "FALSE"));
      } else if (comando.startsWith("rele_")) {
        int nivel = comando.substring(5).toInt();
        if (nivel >= 0 && nivel <= 100 && (nivel % 25 == 0)) {
          debugPrint("=== COMANDO MANUAL: CONFIGURAR RELÉ " + String(nivel) + "% ===");
          configurarRele(nivel);
        }
      }
    }

    enviarDados();
    mostrarTelaDebug();
  }

  // Delay mínimo para não sobrecarregar o processador
  delay(5);
}

// === FUNÇÃO DE TESTE DOS RELÉS ===
void testarReles() {
  debugPrint("=== TESTE DOS RELÉS ===");
  
  // Testar cada relé individualmente
  for (int i = 1; i <= 4; i++) {
    int pino = 0;
    switch(i) {
      case 1: pino = RELE_1; break;
      case 2: pino = RELE_2; break;
      case 3: pino = RELE_3; break;
      case 4: pino = RELE_4; break;
    }
    
    debugPrint("Testando Relé " + String(i) + " (Pino " + String(pino) + ")");
    
    // Ligar relé
    digitalWrite(pino, LOW);
    debugPrint("  Relé " + String(i) + " LIGADO (LOW)");
    delay(200);
    
    // Desligar relé
    digitalWrite(pino, HIGH);
    debugPrint("  Relé " + String(i) + " DESLIGADO (HIGH)");
    delay(200);
  }
  
  debugPrint("=== TESTE CONCLUÍDO ===");
}

// === FUNÇÃO DE TESTE COMPLETO DOS NÍVEIS ===
void testarNiveisLuminosidade() {
  debugPrint("=== TESTE DOS NÍVEIS DE LUMINOSIDADE ===");
  
  const int niveis[] = {0, 25, 50, 75, 100};
  const char* descricoes[] = {"Desligado", "25%", "50%", "75%", "100%"};
  
  for (int i = 0; i < 5; i++) {
    debugPrint("Testando nível: " + String(niveis[i]) + "% (" + String(descricoes[i]) + ")");
    configurarRele(niveis[i]);
    delay(1000);
  }
  
  // Finalizar desligado
  configurarRele(0);
  debugPrint("=== TESTE DE NÍVEIS CONCLUÍDO ===");
}