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
#define IR_PROTOCOLO 8
#define IR_ENDERECO 0xFC00
#define IR_POWER 0x85
#define IR_UMIDIFICAR 0x87
#define IR_VELOCIDADE 0x84
#define IR_TIMER 0x86
#define IR_ALETA_VERTICAL 0x83
#define IR_ALETA_HORIZONTAL 0x82

// === DEBUG ATIVADO PARA DEPURAÇÃO ===
#define DEBUG_SERIAL 1      // Ativar depuração serial

// === OBJETOS GLOBAIS ===
MFRC522 mfrc522(SS_PIN, RST_PIN);
DHT dht(DHT_PIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === ESTRUTURAS OTIMIZADAS PARA MEMÓRIA ===
struct DadosSensores {
  float temperatura = 22.0;
  float humidade = 45.0;
  int luminosidade = 0;
  int valorLDR = 0;
  bool dadosValidos = false;
} sensores;

// Climatizador ultra compacto (apenas 4 bytes + timestamp)
struct {
  bool ligado:1;
  bool umidificando:1;
  bool aletaV:1;
  bool aletaH:1;
  uint8_t velocidade:2;
  uint8_t ultimaVel:2; // Para guardar a última velocidade manual ou automática
  uint8_t timer:3;
  uint8_t reservado:5; // Para futuras expansões
  unsigned long ultimaAtualizacao;
} clima;

// Controle de pessoas ultra otimizado
struct {
  int total = 0;
  String tags[4]; // Reduzido para máximo 4 pessoas simultâneas
  bool estado[4]; // true = presente, false = saiu mas a tag ainda está no histórico
  int count = 0;  // Número de tags distintas no histórico
  float tempPref = 25.0;
  int lumPref = 50;
  bool prefsAtualizadas = false; // Indica se as preferências já foram consultadas para o grupo atual
} pessoas;

// Flags de controle compactas
struct {
  bool modoManualIlum:1;
  bool modoManualClima:1;
  bool ilumAtiva:1;
  bool monitorandoLDR:1;
  bool irPausa:1;
  bool comandoIR:1;
  bool comandoApp:1;
  bool wifiOk:1;
  bool erroSensor:1;      // Flag para monitorar erro de sensores
  bool erroConexao:1;     // Flag para monitorar erro de conexão
  bool debug:1;           // Flag para modo de debug
  bool atualizandoPref:1; // Flag para atualização de preferências (para evitar múltiplas chamadas)
} flags = {false, false, false, true, false, false, false, false, false, false, false, false};

// === INTERVALOS OTIMIZADOS ===
const unsigned long INTERVALO_DHT = 10000;        // 10s
const unsigned long INTERVALO_DADOS = 25000;      // 25s
const unsigned long INTERVALO_MANUAL = 10000;     // 10s (para comandos app)
const unsigned long INTERVALO_LDR = 3000;         // 3s
const unsigned long INTERVALO_CLIMA_AUTO = 10000; // 10s
const unsigned long INTERVALO_COMANDOS = 3000;    // 3s (para verificar comandos do app)
const unsigned long INTERVALO_DEBUG = 5000;       // 5s para exibir informações de debug
const unsigned long INTERVALO_PREF_CHECK = 30000; // 30s para verificar preferências periodicamente

// Timestamps otimizados
unsigned long tempos[8] = {0}; // Array único para todos os timestamps

// Adicione esta constante para o limiar do LDR
// IMPORTANTE: Se o seu LDR retorna valores ALTO para ESCURO e BAIXO para CLARO,
// então a condição no código (LDR < LIMIAR) significa que LDR baixo liga as luzes.
// Se o seu LDR retorna valores BAIXO para ESCURO e ALTO para CLARO,
// então a condição seria (LDR > LIMIAR). Ajuste conforme seu hardware.
#define LIMIAR_LDR_ESCURIDAO 200 // Valor LDR abaixo do qual é considerado "muito escuro" para ligar as luzes

// === CONFIGURAÇÃO DE REDE ===
const char* ssid = "João Augusto";
const char* password = "131103r7";
// ATENÇÃO: Confirme se a URL do servidor termina com /esp32/
// Se o seu servidor for `https://qlpq8xws-3000.brs.devtunnels.ms/`
// e as rotas começarem com `/esp32/`, então `serverUrl` deve ser `https://qlpq8xws-3000.brs.devtunnels.ms/esp32/`
const char* serverUrl = "https://qlpq8xws-3000.brs.devtunnels.ms/esp32/"; 

// === CARACTERES PERSONALIZADOS PARA LCD ===
uint8_t SIMBOLO_PESSOA[8] = {0x0E, 0x0E, 0x04, 0x1F, 0x04, 0x0A, 0x0A, 0x00}; // Pessoa
uint8_t SIMBOLO_TEMP[8]   = {0x04, 0x0A, 0x0A, 0x0A, 0x0A, 0x11, 0x1F, 0x0E}; // Termômetro
uint8_t SIMBOLO_HUM[8]    = {0x04, 0x04, 0x0A, 0x0A, 0x11, 0x11, 0x11, 0x0E}; // Gota
uint8_t SIMBOLO_LUZ[8]    = {0x00, 0x0A, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00}; // Lâmpada
uint8_t SIMBOLO_WIFI[8]   = {0x00, 0x0F, 0x11, 0x0E, 0x04, 0x00, 0x04, 0x00}; // WiFi
uint8_t SIMBOLO_AR[8]     = {0x00, 0x0E, 0x15, 0x15, 0x15, 0x0E, 0x00, 0x00}; // Climatizador
uint8_t SIMBOLO_OK[8]     = {0x00, 0x01, 0x02, 0x14, 0x08, 0x04, 0x02, 0x00}; // √ de OK
uint8_t SIMBOLO_ERRO[8]   = {0x00, 0x11, 0x0A, 0x04, 0x04, 0x0A, 0x11, 0x00}; // X de erro

// Enum para os sons do buzzer - economiza memória usando constantes
enum SomBuzzer : uint8_t {
  SOM_NENHUM = 0,
  SOM_INICIAR,     // Sistema iniciando
  SOM_PESSOA_ENTROU,    // Pessoa detectada entrando
  SOM_PESSOA_SAIU,      // Pessoa detectada saindo
  SOM_COMANDO,     // Comando recebido
  SOM_ALERTA,      // Alerta geral
  SOM_ERRO,        // Erro
  SOM_CONECTADO,   // WiFi conectado
  SOM_DESCONECTADO, // WiFi desconectado
  SOM_OK           // Recuperação de erro
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
  
  // Limitar a frequência das mensagens de debug
  if (agora - ultimoDebug < INTERVALO_DEBUG) return;
  ultimoDebug = agora;
  
  debugPrint("\n--- STATUS DO SISTEMA ---");
  debugPrint("Temperatura: " + String(sensores.temperatura) + "°C");
  debugPrint("Umidade: " + String(sensores.humidade) + "%");
  debugPrint("LDR valor: " + String(sensores.valorLDR));
  debugPrint("Luminosidade: " + String(sensores.luminosidade) + "%");
  debugPrint("Pessoas: " + String(pessoas.total) + " (Tags Hist: " + String(pessoas.count) + ")");
  debugPrint("Temp. Preferida: " + String(pessoas.tempPref));
  debugPrint("Lum. Preferida: " + String(pessoas.lumPref));
  debugPrint("Diff Temp: " + String(sensores.temperatura - pessoas.tempPref));
  
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

// Função de buzzer otimizada usando técnica de PWM manual
void tocarSom(SomBuzzer tipo) {
  // Estrutura compacta para os padrões sonoros
  // Formato: {frequência em x100Hz, duração em x10ms, pausa em x10ms, repetições}
  static const uint8_t SONS[][4] = {
    {0, 0, 0, 0},                // SOM_NENHUM
    {20, 10, 5, 3},              // SOM_INICIAR (3 bips ascendentes)
    {25, 5, 0, 1},               // SOM_PESSOA_ENTROU (1 bip curto alto)
    {15, 5, 0, 1},               // SOM_PESSOA_SAIU (1 bip curto baixo)
    {30, 2, 2, 1},               // SOM_COMANDO (1 bip muito curto)
    {40, 3, 3, 3},               // SOM_ALERTA (3 bips rápidos)
    {10, 15, 5, 2},              // SOM_ERRO (2 bips longos graves)
    {35, 3, 2, 2},               // SOM_CONECTADO (2 bips médios)
    {15, 10, 5, 1},              // SOM_DESCONECTADO (1 bip longo grave)
    {45, 2, 1, 2}                // SOM_OK (2 bips agudos rápidos)
  };
  
  if (tipo == SOM_NENHUM) return;
  
  const uint8_t* som = SONS[tipo];
  int freq = som[0] * 100;   // Converter para Hz
  int duracao = som[1] * 10; // Converter para ms
  int pausa = som[2] * 10;   // Converter para ms
  int repeticoes = som[3];
  
  // Implementação ultra-otimizada de PWM para economizar CPU
  for (int r = 0; r < repeticoes; r++) {
    // Frequência ascendente para SOM_INICIAR
    int freqAtual = freq;
    if (tipo == SOM_INICIAR) {
      freqAtual += r * 200; // Aumenta 200Hz a cada repetição
    }
    
    if (freqAtual > 0) {
      int periodo = 1000000 / freqAtual; // Período em microssegundos
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

// Tela de erro LCD otimizada
void mostrarErroLCD(const char* erro, bool critico = false) {
  // Uso da ROM em vez da RAM para strings
  static const char MSG_ERRO[] PROGMEM = "ERRO:";
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(7); // Símbolo de erro
  lcd.print(' ');
  lcd.print(MSG_ERRO);
  lcd.setCursor(0, 1);
  lcd.print(erro);
  
  if (critico) {
    tocarSom(SOM_ERRO);
  } else {
    tocarSom(SOM_ALERTA);
  }
}

// Animação de transição otimizada para o LCD
void animacaoTransicao() {
  // Padrão pré-definido - usa menos memória que cálculos em tempo real
  static const uint8_t PADRAO[] = {0, 1, 3, 7, 15, 14, 12, 8, 0};
  
  for (int i = 0; i < 8; i++) {
    lcd.clear();
    lcd.setCursor(i, 0);
    lcd.print(">");
    lcd.setCursor(15-i, 1);
    lcd.print("<");
    delay(30); // Delay mínimo para ser perceptível
  }
  lcd.clear();
}

// Atualização otimizada do LCD com ícones
void atualizarLCD() {
  static uint32_t hashAnterior = 0;
  
  // NOVO: Hash inclui flags de modo manual para forçar atualização do LCD
  uint32_t hash = (uint32_t)(sensores.temperatura * 10) +
                  (uint32_t)(sensores.humidade) * 1000 +
                  sensores.luminosidade * 10000 +
                  pessoas.total * 100000 +
                  (clima.ligado ? 1000000 : 0) +
                  (flags.wifiOk ? 2000000 : 0) +
                  (flags.erroSensor ? 4000000 : 0) +
                  (flags.modoManualIlum ? 8000000 : 0) + // Adiciona flag de modo manual ilum
                  (flags.modoManualClima ? 16000000 : 0); // Adiciona flag de modo manual clima
  
  if (hash == hashAnterior) return;
  hashAnterior = hash;
  
  lcd.clear();
  
  // NOVO: Lógica para modo manual no display
  if (flags.modoManualIlum || flags.modoManualClima) {
    // Linha 1: Temperatura e Umidade com ícones
    lcd.setCursor(0, 0);
    lcd.write(1); // Ícone Temperatura
    lcd.print(sensores.temperatura, 1);
    lcd.write(0xDF); // Símbolo de grau (built-in)
    lcd.print("C ");
    
    lcd.write(2); // Ícone Umidade
    lcd.print(sensores.humidade, 0);
    lcd.print("%");
    
    // Linha 2: Mensagem de Modo Manual e Pessoas
    lcd.setCursor(0, 1);
    lcd.print("Manual Mode ");
    lcd.write(0); // Ícone Pessoa
    lcd.print(pessoas.total);
    
    // Indicador WiFi no canto direito
    lcd.setCursor(15, 1);
    lcd.write(flags.wifiOk ? 4 : 7); // Ícone WiFi ou Erro
  } else {
    // Lógica original para modo automático/normal
    // Linha 1: Temperatura e Umidade com ícones
    lcd.setCursor(0, 0);
    lcd.write(1); // Ícone Temperatura
    lcd.print(sensores.temperatura, 1);
    lcd.write(0xDF); // Símbolo de grau (built-in)
    lcd.print("C ");
    
    lcd.write(2); // Ícone Umidade
    lcd.print(sensores.humidade, 0);
    lcd.print("%");
    
    // Linha 2: Informações de status e ícones de estado
    lcd.setCursor(0, 1);
    
    // Símbolo de Iluminação e valor
    lcd.write(3); // Ícone Luz
    lcd.print(sensores.luminosidade);
    lcd.print("% ");
    
    // Símbolo Pessoa e contagem
    lcd.write(0); // Ícone Pessoa
    lcd.print(pessoas.total);
    lcd.print(" ");
    
    // Status do Climatizador
    lcd.write(5); // Ícone Climatizador
    lcd.print(clima.ligado ? "L" : "-");
    
    // Indicador WiFi
    lcd.setCursor(15, 1);
    lcd.write(flags.wifiOk ? 4 : 7); // Ícone WiFi ou Erro
  }
}

// Tela de status otimizada para o Climatizador
void atualizarTelaClimatizador() {
  static uint8_t estadoAnterior = 0;
  
  // Compactar o estado em um único byte para comparação eficiente
  uint8_t estadoAtual = (clima.ligado << 0) | 
                        (clima.umidificando << 1) | 
                        (clima.velocidade << 2) | 
                        (clima.timer << 4) |
                        (clima.aletaV << 7); // Usando bit alto para uma das aletas
  
  if (estadoAtual == estadoAnterior) return;
  estadoAnterior = estadoAtual;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(5); // Ícone climatizador
  lcd.print(" Climatizador");
  
  lcd.setCursor(0, 1);
  if (!clima.ligado) {
    lcd.print("Desligado");
    return;
  }
  
  // Mostrar status de forma compacta com ícones
  char buffer[17]; // Buffer otimizado para evitar sprintf
  uint8_t pos = 0;
  
  // Velocidade
  buffer[pos++] = 'V';
  buffer[pos++] = '0' + clima.velocidade;
  buffer[pos++] = ' ';
  
  // Timer (se ativo)
  if (clima.timer > 0) {
    buffer[pos++] = 'T';
    buffer[pos++] = '0' + clima.timer;
    buffer[pos++] = ' ';
  }
  
  // Status Umidificação
  if (clima.umidificando) {
    buffer[pos++] = 'U';
    buffer[pos++] = 'M';
    buffer[pos++] = ' ';
  }
  
  // Status Aletas
  if (clima.aletaV || clima.aletaH) {
    buffer[pos++] = 'A';
    if (clima.aletaV) buffer[pos++] = 'V';
    if (clima.aletaH) buffer[pos++] = 'H';
  }
  
  buffer[pos] = '\0'; // Terminar string
  lcd.print(buffer);
}

// === FUNÇÃO DE CONSULTA DE PREFERÊNCIAS ===
bool consultarPreferencias() {
  if (pessoas.total == 0 || !flags.wifiOk || flags.atualizandoPref) {
    debugPrint("consultarPreferencias: Condições não atendidas. Total pessoas: " + String(pessoas.total) + ", WiFiOK: " + String(flags.wifiOk) + ", AtualizandoPref: " + String(flags.atualizandoPref));
    return false;
  }
  
  flags.atualizandoPref = true; // Define a flag para evitar chamadas duplicadas
  
  // Construir array de tags para enviar ao servidor
  StaticJsonDocument<256> doc; // Tamanho suficiente para as tags e estrutura JSON
  JsonArray tagsArray = doc.createNestedArray("tags");
  
  int tagsAtivas = 0;
  for (int i = 0; i < pessoas.count; i++) {
    if (pessoas.estado[i]) { // Apenas tags de pessoas presentes
      tagsArray.add(pessoas.tags[i]);
      tagsAtivas++;
    }
  }
  
  // Se não houver tags ativas (ex: todas saíram mas o total ainda é > 0 por algum motivo), não há necessidade de consultar
  if (tagsAtivas == 0) {
    flags.atualizandoPref = false;
    debugPrint("consultarPreferencias: Nenhuma tag ativa para consultar.");
    return false;
  }
  
  String jsonTags;
  serializeJson(doc, jsonTags);
  
  debugPrint("Consultando preferências para " + String(tagsAtivas) + " tags");
  debugPrint("JSON enviado: " + jsonTags);
  
  // Enviar requisição para o servidor
  HTTPClient http;
  String fullUrl = String(serverUrl) + "preferencias";
  debugPrint("URL da requisição: " + fullUrl);
  
  http.begin(fullUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000); // Timeout ligeiramente maior para preferenciais
  
  int httpCode = http.POST(jsonTags);
  bool sucesso = false;
  
  debugPrint("HTTP Code para preferencias: " + String(httpCode));
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    debugPrint("Resposta do servidor para preferencias: " + payload);
    
    // Parsear resposta JSON
    StaticJsonDocument<256> respDoc; // Pode ser menor, mas 256 é seguro
    DeserializationError error = deserializeJson(respDoc, payload);
    
    if (!error) {
      // Extrair e salvar valores com verificação explícita
      if (respDoc.containsKey("temperatura")) {
        float tempPref = respDoc["temperatura"];
        debugPrint("Temperatura preferida recebida: " + String(tempPref));
        
        // Ajuste a validação para o seu limite aceitável (ex: 16.0)
        if (!isnan(tempPref) && tempPref >= 16.0 && tempPref <= 32.0) { 
          pessoas.tempPref = tempPref;
          debugPrint("Temperatura preferida atualizada para: " + String(pessoas.tempPref));
        } else {
          debugPrint("Temperatura inválida recebida, usando padrão: " + String(tempPref));
          pessoas.tempPref = 25.0; // Usa padrão se inválido
        }
      } else {
        debugPrint("Chave 'temperatura' não encontrada na resposta, usando padrão.");
        pessoas.tempPref = 25.0; // Usa padrão se chave não existe
      }
      
      if (respDoc.containsKey("luminosidade")) {
        int lumPref = respDoc["luminosidade"];
        debugPrint("Luminosidade preferida recebida: " + String(lumPref));
        
        // Luminosidade em múltiplos de 25
        if (lumPref >= 0 && lumPref <= 100 && (lumPref % 25 == 0)) {
          pessoas.lumPref = lumPref;
          debugPrint("Luminosidade preferida atualizada para: " + String(pessoas.lumPref));
        } else {
          debugPrint("Luminosidade inválida recebida, usando padrão: " + String(lumPref));
          pessoas.lumPref = 50; // Usa padrão se inválido
        }
      } else {
        debugPrint("Chave 'luminosidade' não encontrada na resposta, usando padrão.");
        pessoas.lumPref = 50; // Usa padrão se chave não existe
      }
      
      sucesso = true;
      pessoas.prefsAtualizadas = true; // Marca como atualizado para o estado atual
    } else {
      debugPrint("Erro ao parsear JSON da resposta de preferencias: " + String(error.c_str()));
      mostrarErroLCD("Erro JSON Pref", false); // Mensagem mais específica
      delay(800);
    }
  } else {
    debugPrint("Erro na requisição HTTP para preferencias: " + String(httpCode));
    mostrarErroLCD("Erro API Pref", false); // Mensagem mais específica
    delay(800);
  }
  
  http.end();
  flags.atualizandoPref = false; // Libera a flag
  atualizarLCD(); // Mantém esta linha para garantir que o LCD principal é restaurado/atualizado
  return sucesso;
}

// === FUNÇÕES PRINCIPAIS (OTIMIZADAS) ===

void configurarRele(int nivel) {
  static int nivelAnterior = -1;
  if (nivel == nivelAnterior) return; // Evita reconfiguração desnecessária
  
  // Garantir que o nível seja um múltiplo de 25
  nivel = (nivel / 25) * 25;
  if (nivel > 100) nivel = 100;
  
  const bool estados[5][4] = {
    {1,1,1,1}, // 0% (todos HIGH para relé NC)
    {1,0,1,1}, // 25%
    {0,1,0,1}, // 50%
    {0,0,0,1}, // 75%
    {0,0,0,0}  // 100% (todos LOW para relé NC)
  };
  
  int indice = nivel / 25;
  if (indice >= 0 && indice <= 4) {
    digitalWrite(RELE_1, estados[indice][0]);
    digitalWrite(RELE_2, estados[indice][1]);
    digitalWrite(RELE_3, estados[indice][2]);
    digitalWrite(RELE_4, estados[indice][3]);
    sensores.luminosidade = nivel;
    
    debugPrint("Rele configurado: " + String(nivel) + "% (indice " + String(indice) + ")");
    
    // Feedback sonoro e visual para mudanças de iluminação
    if (nivel > 0 && nivelAnterior == 0) {
      // Luz ligando
      tocarSom(SOM_COMANDO);
    } else if (nivel == 0 && nivelAnterior > 0) {
      // Luz desligando
      tocarSom(SOM_COMANDO);
    } else if (abs(nivel - nivelAnterior) >= 50) {
      // Mudança significativa
      tocarSom(SOM_COMANDO);
    }
    
    nivelAnterior = nivel;
    atualizarLCD();
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
      // Recuperou de um erro anterior
      flags.erroSensor = false;
      tocarSom(SOM_OK);
      debugPrint("Sensores DHT recuperados.");
    }
    sensores.temperatura = temp;
    sensores.humidade = hum;
    sensores.dadosValidos = true;
  } else {
    // Usar valores padrão se a leitura falhar (para manter a funcionalidade)
    if (!sensores.dadosValidos) {
      sensores.temperatura = 25.0;  // Valor padrão seguro
      sensores.humidade = 50.0;     // Valor padrão seguro
      sensores.dadosValidos = true; // Marcar como válido para uso
    }
    
    // Falha de leitura - adicionar feedback
    if (!flags.erroSensor) {
      flags.erroSensor = true;
      debugPrint("ERRO: Falha na leitura do DHT");
      mostrarErroLCD("Sensor DHT", false);
      delay(1000); // Mostra erro brevemente
    }
  }
  
  // LDR com filtro - A cada chamada de lerSensores, uma nova leitura LDR é feita e a média é atualizada.
  static int ldrBuffer[3] = {0};
  static int bufferIndex = 0;
  
  ldrBuffer[bufferIndex] = analogRead(LDR_PIN);
  bufferIndex = (bufferIndex + 1) % 3;
  
  // Média móvel simples
  sensores.valorLDR = (ldrBuffer[0] + ldrBuffer[1] + ldrBuffer[2]) / 3;
  
  // Atualizar o LCD com os novos valores
  atualizarLCD();
}

// Função auxiliar para enviar dados HTTP de forma genérica
bool enviarHTTP(const String& endpoint, const String& dados = "", bool isPost = false) {
  if (!flags.wifiOk) {
    // debugPrint("enviarHTTP: WiFi não conectado, não pode enviar para " + endpoint);
    return false;
  }
  
  HTTPClient http;
  String fullUrl = String(serverUrl) + endpoint;
  http.begin(fullUrl);
  http.setTimeout(5000); // Timeout otimizado
  
  bool sucesso = false;
  int httpCode = 0;
  
  if (isPost) {
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(dados);
    sucesso = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT); // 200 OK ou 204 No Content
  } else {
    httpCode = http.GET();
    sucesso = (httpCode == HTTP_CODE_OK);
    if (sucesso) {
      // String resposta = http.getString();
      // debugPrint("Resposta GET de " + endpoint + ": " + resposta.substring(0, min(resposta.length(), 100))); // Limita para debug
    }
  }
  
  if (!sucesso) {
    // CORREÇÃO: Cast para unsigned int na chamada de min()
    debugPrint("enviarHTTP: Erro (" + String(httpCode) + ") ao enviar para " + endpoint + " Dados: " + dados.substring(0, min(dados.length(), (unsigned int)100)));
    // Feedback para erros de conexão (apenas se for a primeira vez)
    if (!flags.erroConexao) {
      flags.erroConexao = true;
      mostrarErroLCD("Falha Conexao", false);
      delay(800); // Mostra erro brevemente
      atualizarLCD();
    }
  } else {
    // debugPrint("enviarHTTP: Sucesso para " + endpoint);
    if (flags.erroConexao) { // Se estava com erro e recuperou
      flags.erroConexao = false;
      tocarSom(SOM_OK);
    }
  }
  
  http.end();
  return sucesso;
}

void enviarDados() {
  unsigned long agora = millis();
  if (agora - tempos[1] < INTERVALO_DADOS) return;
  tempos[1] = agora;
  
  // JSON ultra compacto
  StaticJsonDocument<300> doc; // Aumentado um pouco para acomodar mais dados do clima
  doc["t"] = round(sensores.temperatura * 10) / 10.0;
  doc["h"] = round(sensores.humidade);
  doc["l"] = sensores.luminosidade; // Use sensores.luminosidade diretamente
  doc["p"] = pessoas.total;
  
  JsonObject c = doc.createNestedObject("c");
  c["l"] = clima.ligado;
  c["u"] = clima.umidificando;
  c["v"] = clima.velocidade;
  c["uv"] = clima.ultimaVel; // Adicionado para enviar a última velocidade
  c["t"] = clima.timer;
  c["av"] = clima.aletaV; // Adicionado para enviar estado das aletas
  c["ah"] = clima.aletaH; // Adicionado para enviar estado das aletas
  c["mmc"] = flags.modoManualClima; // Adicionado para enviar o modo manual do climatizador
  
  // Adicionar tags ativas ao envio
  if (pessoas.count > 0) { // Usa pessoas.count para iterar pelo histórico de tags
    JsonArray tagsArray = doc.createNestedArray("tags");
    for (int i = 0; i < pessoas.count; i++) {
      if (pessoas.estado[i]) { // Apenas tags de pessoas ativamente presentes
        tagsArray.add(pessoas.tags[i]);
      }
    }
  }
  
  String dados;
  serializeJson(doc, dados);
  
  debugPrint("Enviando dados ambiente: " + dados);
  
  enviarHTTP("ambiente", dados, true);
}

void processarNFC() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;
  
  String tag = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) tag += "0";
    tag += String(mfrc522.uid.uidByte[i], HEX);
  }
  tag.toUpperCase();
  
  debugPrint("Tag NFC lida: " + tag);
  gerenciarPresenca(tag);
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1(); // Importante para liberar o PCD para nova leitura
}

void gerenciarPresenca(const String& tag) {
  int indice = -1;
  bool entrando = false;
  int totalAnterior = pessoas.total;
  
  // Buscar tag existente
  for (int i = 0; i < pessoas.count; i++) {
    if (pessoas.tags[i] == tag) {
      indice = i;
      break;
    }
  }
  
  if (indice == -1) {
    // Nova pessoa - Adicionar à lista de tags históricas
    if (pessoas.count < 4) { // Limite de 4 tags distintas
      pessoas.tags[pessoas.count] = tag;
      pessoas.estado[pessoas.count] = true; // Define como presente
      pessoas.total++; // Incrementa total de presentes
      pessoas.count++; // Incrementa número de tags distintas
      entrando = true;
      
      // Primeira pessoa - inicia monitoramento e reseta flags de automação
      if (pessoas.total == 1) {
        flags.monitorandoLDR = true;
        flags.ilumAtiva = false;
        flags.modoManualIlum = false;  // Reset para modo automático
        flags.modoManualClima = false; // Reset para modo automático
        debugPrint("Primeira pessoa detectada. Automação reativada.");
      }
      
      debugPrint("Nova pessoa detectada - Tag: " + tag + ", Total: " + String(pessoas.total));
      
      // Feedback de entrada
      tocarSom(SOM_PESSOA_ENTROU);
      
      // Tela de boas-vindas rápida
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.write(0); // Ícone pessoa
      lcd.print(" Bem-vindo!");
      lcd.setCursor(0, 1);
      lcd.print("Pessoas: ");
      lcd.print(pessoas.total);
      delay(800);
    } else {
      debugPrint("Limite de pessoas atingido (" + String(pessoas.count) + "). Ignorando nova tag: " + tag);
      // Opcional: som de alerta ou feedback visual de lotação
      tocarSom(SOM_ERRO);
      delay(500);
    }
  } else if (pessoas.estado[indice]) {
    // Pessoa saindo (tag já presente e em estado 'presente')
    pessoas.estado[indice] = false; // Marca como 'ausente'
    pessoas.total--; // Decrementa total de presentes
    
    debugPrint("Pessoa saindo - Tag: " + tag + ", Total: " + String(pessoas.total));
    
    // Feedback de saída
    tocarSom(SOM_PESSOA_SAIU);
    
    // Tela de despedida rápida
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(0); // Ícone pessoa
    lcd.print(" Ate logo!");
    lcd.setCursor(0, 1);
    lcd.print("Pessoas: ");
    lcd.print(pessoas.total);
    delay(800);
    
    if (pessoas.total == 0) {
      resetarSistema(); // Se não há mais ninguém, reseta
    }
  } else {
    // Pessoa voltando (tag já presente mas em estado 'ausente')
    pessoas.estado[indice] = true; // Marca como 'presente' novamente
    pessoas.total++; // Incrementa total de presentes
    entrando = true;
    
    debugPrint("Pessoa voltando - Tag: " + tag + ", Total: " + String(pessoas.total));
    
    // Feedback de entrada
    tocarSom(SOM_PESSOA_ENTROU);
    
    // Tela de boas-vindas rápida
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(0); // Ícone pessoa
    lcd.print(" Bem-vindo!");
    lcd.setCursor(0, 1);
    lcd.print("Pessoas: ");
    lcd.print(pessoas.total);
    delay(800);
  }
  
  // Se o total de pessoas mudou ou se uma pessoa entrou (independentemente se total mudou por reentrada)
  // e se as preferências não foram atualizadas para o grupo atual
  if ((totalAnterior != pessoas.total || entrando) && pessoas.total > 0 && !flags.atualizandoPref) {
    debugPrint("Total de pessoas mudou ou nova entrada - Atualizando preferências.");
    pessoas.prefsAtualizadas = false; // Força a reconsulta de preferências
    consultarPreferencias();
  }
  
  // Retorna à tela principal
  atualizarLCD();
  
  // Reporta mudança de presença ao servidor
  enviarDados();
}

void resetarSistema() {
  // Feedback para shutdown do sistema
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Desativando...");
  lcd.setCursor(0, 1);
  lcd.print("Sistema");
  tocarSom(SOM_ALERTA);
  
  // Desligar equipamentos
  flags.modoManualIlum = false;
  flags.modoManualClima = false;
  flags.ilumAtiva = false;
  flags.monitorandoLDR = true; // Sempre monitora LDR em modo automático
  configurarRele(0); // Desliga luzes
  
  // Desligar climatizador se ligado
  if (clima.ligado) {
    enviarComandoIR(IR_POWER);
  }
  
  // Reset das preferências e estado do climatizador
  pessoas.prefsAtualizadas = false;
  // O estado do clima é atualizado pelo enviarComandoIR
  
  debugPrint("Sistema resetado - Modo de espera");
  
  delay(1000);
  animacaoTransicao();
  atualizarLCD();
}

// Função atualizada para considerar preferências do usuário
void gerenciarIluminacao() {
  unsigned long agora = millis();
  // Este intervalo de 3s para o LDR é bom, mantém a resposta ágil.
  if (agora - tempos[2] < INTERVALO_LDR) return;
  tempos[2] = agora;
  
  // Condição para DESLIGAR as luzes:
  // 1. Nenhuma pessoa na sala (total = 0)
  // 2. Ou o modo manual de iluminação foi ativado (app ou comando direto)
  if (pessoas.total == 0 || flags.modoManualIlum) {
    if (flags.ilumAtiva) { // Se as luzes estão ligadas e precisamos desligar
      debugPrint("Desligando luzes: Pessoas = " + String(pessoas.total) + ", ModoManual = " + String(flags.modoManualIlum ? "SIM" : "NAO"));
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.write(3); // Ícone luz
      lcd.print(" Luz Auto Off");
      lcd.setCursor(0, 1);
      if (pessoas.total == 0) {
        lcd.print("Nenhuma pessoa");
      } else {
        lcd.print("Modo Manual ON");
      }
      tocarSom(SOM_COMANDO);
      delay(800);
      
      configurarRele(0); // Desliga todas as luzes
      flags.ilumAtiva = false;
      atualizarLCD();
    }
    // Não precisa fazer mais nada se não há pessoas ou está em modo manual.
    return; 
  }
  
  // Se chegamos aqui, significa:
  // - Há pessoas na sala (pessoas.total > 0)
  // - O modo de iluminação está no automático (!flags.modoManualIlum)

  // Lógica para LIGAR as luzes:
  // As luzes só ligam se já não estiverem ativas E o LDR indicar muita escuridão.
  if (!flags.ilumAtiva) { 
    // Assumindo que LDR < LIMIAR_LDR_ESCURIDAO (ex: < 200) significa "MUITO ESCURO"
    if (sensores.valorLDR < LIMIAR_LDR_ESCURIDAO) { 
      // Usar nível de luminosidade preferido (garante um mínimo de 25% se for 0)
      int nivel = (pessoas.lumPref > 0) ? pessoas.lumPref : 50;
      if (nivel == 0) nivel = 25; // Garante que a luz ligue se a preferência for 0
      
      debugPrint("LIGANDO luzes automaticamente: LDR=" + String(sensores.valorLDR) + " < " + String(LIMIAR_LDR_ESCURIDAO) + ". Nível: " + String(nivel) + "%");
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.write(3); // Ícone luz
      lcd.print(" Luz Auto ON ");
      lcd.print(nivel);
      lcd.print("%");
      lcd.setCursor(0, 1);
      lcd.print("LDR: ");
      lcd.print(sensores.valorLDR);
      tocarSom(SOM_COMANDO);
      delay(800);
      
      configurarRele(nivel); // Liga as luzes no nível desejado
      flags.ilumAtiva = true; // Marca que as luzes estão ativas por automação
      atualizarLCD();
    } else {
        // Se a luz não está ativa e não está escuro o suficiente, apenas debug
        debugPrint("Luzes OFF: LDR=" + String(sensores.valorLDR) + " >= " + String(LIMIAR_LDR_ESCURIDAO) + ". Aguardando escuridão.");
    }
  } else {
    // Lógica para MANTER as luzes LIGADAS:
    // Se as luzes já estão ligadas (flags.ilumAtiva é true) e há pessoas na sala
    // e não estamos em modo manual, as luzes permanecem ligadas, independentemente do LDR.
    debugPrint("Luzes ON: Mantendo ligadas. Pessoas presentes, modo auto ativo.");
  }
}

// Envia um comando IR e atualiza o estado local do climatizador
bool enviarComandoIR(uint8_t comando) {
  static unsigned long ultimoComandoIR = 0;
  unsigned long agora = millis();
  
  // Debounce para evitar comandos IR muito rápidos (para o próprio climatizador)
  if (agora - ultimoComandoIR < 300) {
    debugPrint("Comando IR ignorado (debounce): 0x" + String(comando, HEX));
    return false;
  }
  
  debugPrint("Enviando comando IR: 0x" + String(comando, HEX));
  
  // Pause IrReceiver temporarily to avoid self-reception during send
  IrReceiver.stop(); 
  
  // Três tentativas para garantir recepção
  for (int i = 0; i < 3; i++) {
    IrSender.sendNEC(IR_ENDERECO, comando, 32);
    delay(50); // Pequeno delay entre as repetições
  }
  
  // Resume IrReceiver
  IrReceiver.start();
  
  ultimoComandoIR = agora; // Atualiza o timestamp do último comando IR enviado
  
  // Atualiza o estado interno do climatizador com base no comando enviado
  atualizarEstadoClima(comando);
  
  // Feedback sonoro para comandos IR
  tocarSom(SOM_COMANDO);
  
  // Mostrar tela de status do climatizador
  atualizarTelaClimatizador(); // Mostra o estado detalhado do clima (ex: V1, UM, A...)
  delay(800);
  // Não precisamos de atualizarLCD() aqui pois o fluxo de comando abaixo fará isso
  // ou a tela vai para o estado padrão de modo manual (se for o caso)
  
  return true;
}

// Atualiza o estado interno do climatizador (chamado após enviar IR ou receber comando)
void atualizarEstadoClima(uint8_t comando) {
  bool estadoAnteriorLigado = clima.ligado; // Para checar se o estado de ligado mudou
  
  switch (comando) {
    case IR_POWER:
      clima.ligado = !clima.ligado;
      if (clima.ligado) {
        clima.velocidade = clima.ultimaVel > 0 ? clima.ultimaVel : 1; // Ao ligar, usa a última velocidade ou 1
      } else {
        clima.velocidade = 0; // Desliga, velocidade 0
        clima.umidificando = false; // Desliga umidificação
        clima.timer = 0; // Reseta timer
        clima.aletaV = false; // Desliga aletas
        clima.aletaH = false; // Desliga aletas
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
        clima.velocidade = (clima.velocidade % 3) + 1; // 1 -> 2 -> 3 -> 1...
        clima.ultimaVel = clima.velocidade; // Guarda a última velocidade ativa
        debugPrint("Velocidade: " + String(clima.velocidade));
      }
      break;
    case IR_TIMER:
      if (clima.ligado) {
        clima.timer = (clima.timer + 1) % 6; // 0 (off), 1h, 2h, 3h, 4h, 5h
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
  
  // Se o estado de ligado mudou, ou se um comando foi processado
  // (e não estamos em um comando de debounce), atualiza o timestamp.
  // This helps tracking last known state change for UI.
  if (estadoAnteriorLigado != clima.ligado || comando != 0) { // Comando != 0 para comandos reais
    clima.ultimaAtualizacao = millis();
  }
}

// Função corrigida para o controle automático de clima
void controleAutomaticoClima() {
  // Se não há pessoas ou está em modo manual, não faz nada
  if (pessoas.total <= 0 || flags.modoManualClima) {
    // Se climatizador estiver ligado e não há pessoas, desligar
    if (pessoas.total == 0 && clima.ligado) {
      debugPrint("Desligando climatizador - Nenhuma pessoa presente.");
      enviarComandoIR(IR_POWER);
    }
    return;
  }
  
  // Se as preferências não foram atualizadas, não realizar controle automático
  if (!pessoas.prefsAtualizadas) {
    debugPrint("Preferências não atualizadas, aguardando para controle de clima...");
    return;
  }
  
  unsigned long agora = millis();
  if (agora - tempos[3] < INTERVALO_CLIMA_AUTO) return;
  tempos[3] = agora;
  
  float diff = sensores.temperatura - pessoas.tempPref;
  
  debugPrint("Diferença de temperatura: " + String(diff) + " (Atual: " + 
             String(sensores.temperatura) + ", Desejada: " + String(pessoas.tempPref) + ")");
  
  // Lógica simplificada do climatizador
  // Ligar apenas se estiver pelo menos 2.0 graus acima do preferido (mais econômico)
  if (diff >= 2.0 && !clima.ligado) { 
    // Feedback para ativação automática
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(5); // Ícone clima
    lcd.print(" Auto Ligar");
    lcd.setCursor(0, 1);
    lcd.print("Temp: +");
    lcd.print(diff, 1);
    lcd.print(" graus");
    tocarSom(SOM_COMANDO);
    
    debugPrint("LIGANDO climatizador automaticamente!");
    
    delay(800);
    
    enviarComandoIR(IR_POWER); // Ligar
    // Ao ligar, tenta ajustar para velocidade 1 (a mais econômica)
    if (clima.velocidade != 1) { 
      debugPrint("Ajustando velocidade para 1 (default auto)");
      int toquesNecessarios = 0;
      if (clima.velocidade == 2) toquesNecessarios = 2; // de 2 para 1
      else if (clima.velocidade == 3) toquesNecessarios = 1; // de 3 para 1

      for(int i=0; i<toquesNecessarios; ++i) {
          enviarComandoIR(IR_VELOCIDADE);
          delay(300); 
      }
    }
  } 
  // Limiar para desligar permanece -0.5 para evitar liga/desliga constante
  else if (diff <= -0.5 && clima.ligado) {
    // Feedback para desativação automática
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(5); // Ícone clima
    lcd.print(" Auto Deslig.");
    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.print(diff, 1);
    lcd.print(" graus");
    tocarSom(SOM_COMANDO);
    
    debugPrint("DESLIGANDO climatizador automaticamente!");
    
    delay(800);
    
    enviarComandoIR(IR_POWER); // Desligar
  } 
  else if (clima.ligado) {
    // Ajustar velocidade baseado na diferença (novos limiares mais distantes para economia)
    int velDesejada = 1; // Padrão mais econômico
    if (diff >= 4.5) { // Para ativar a velocidade 3, diferença deve ser de 4.5 graus ou mais
        velDesejada = 3;
    } else if (diff >= 3.0) { // Para ativar a velocidade 2, diferença deve ser de 3.0 graus ou mais
        velDesejada = 2;
    } else { // Se diff está entre 2.0 (ligar) e 3.0, mantém velocidade 1
        velDesejada = 1;
    }
    
    if (clima.velocidade != velDesejada) {
      // Feedback para ajuste automático de velocidade
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.write(5); // Ícone clima
      lcd.print(" Ajuste Auto");
      lcd.setCursor(0, 1);
      lcd.print("Vel ");
      lcd.print(clima.velocidade);
      lcd.print(" -> ");
      lcd.print(velDesejada);
      tocarSom(SOM_COMANDO);
      
      debugPrint("Ajustando velocidade: " + String(clima.velocidade) + 
                 " -> " + String(velDesejada));
      
      delay(800);
      
      // Enviar comando de velocidade quantas vezes forem necessárias
      int tentativas = 0;
      while (clima.velocidade != velDesejada && tentativas < 3) { // Limita tentativas para evitar loop infinito
          enviarComandoIR(IR_VELOCIDADE);
          tentativas++;
          delay(300); // Pequeno delay entre envios
      }
    }
  }

  // === LÓGICA: Controle de Aletas baseado no número de pessoas ===
  if (clima.ligado && !flags.modoManualClima) { // Apenas se climatizador ligado e em modo automático
    if (pessoas.total == 1) {
      // Se há 1 pessoa: Aleta Vertical ATIVA, Aleta Horizontal INATIVA
      if (!clima.aletaV) { // Se a aleta vertical está desligada, ligue-a
        debugPrint("Aleta vertical: Ativando (1 pessoa)");
        enviarComandoIR(IR_ALETA_VERTICAL);
        delay(300); // Pequeno delay para o climatizador processar o comando
      }
      if (clima.aletaH) { // Se a aleta horizontal está ligada, desligue-a
        debugPrint("Aleta horizontal: Desativando (1 pessoa)");
        enviarComandoIR(IR_ALETA_HORIZONTAL);
        delay(300);
      }
    } else if (pessoas.total > 1) {
      // Se há mais de 1 pessoa: Aleta Vertical ATIVA, Aleta Horizontal ATIVA
      if (!clima.aletaV) { // Se a aleta vertical está desligada, ligue-a
        debugPrint("Aleta vertical: Ativando (>1 pessoa)");
        enviarComandoIR(IR_ALETA_VERTICAL);
        delay(300);
      }
      if (!clima.aletaH) { // Se a aleta horizontal está desligada, ligue-a
        debugPrint("Aleta horizontal: Ativando (>1 pessoa)");
        enviarComandoIR(IR_ALETA_HORIZONTAL);
        delay(300);
      }
    }
    // Se pessoas.total é 0, a lógica no início da função já desliga o climatizador,
    // o que implícitamente reseta o estado das aletas para o padrão de desligado.
  }
}

void verificarComandos() {
  unsigned long agora = millis();
  if (agora - tempos[4] < INTERVALO_COMANDOS || !flags.wifiOk) return;
  tempos[4] = agora;
  
  // --- Comandos de Iluminação ---
  HTTPClient http;
  String urlIlum = String(serverUrl) + "comando";
  http.begin(urlIlum);
  int httpCodeIlum = http.GET();
  
  if (httpCodeIlum == HTTP_CODE_OK) {
    String cmd = http.getString();
    cmd.trim();
    
    if (cmd != "auto" && cmd.length() > 0 && pessoas.total > 0) {
      int nivel = cmd.toInt();
      // Verifica se o nível é válido e um múltiplo de 25
      if (nivel >= 0 && nivel <= 100 && (nivel % 25 == 0)) {
        // Apenas muda o modo se ele não estiver já em manual com o mesmo nível ou se o nível é diferente
        if (!flags.modoManualIlum || sensores.luminosidade != nivel) {
            bool enteringManualMode = !flags.modoManualIlum; // Captura se estamos *entrando* no modo manual
            flags.modoManualIlum = true;
            flags.ilumAtiva = true;
            
            if (enteringManualMode) { // Avisa apenas na transição para manual
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.write(3); // Ícone luz
                lcd.print(" Luz Manual ON"); 
                lcd.setCursor(0, 1);
                lcd.print("Nivel: ");
                lcd.print(nivel);
                lcd.print("%");
                tocarSom(SOM_COMANDO); // Buzzer uma vez
                delay(1500); // Exibe mensagem temporariamente
            }
            debugPrint("Comando recebido do app: luz manual " + String(nivel) + "%");
            configurarRele(nivel); // Isso já atualiza o LCD para a tela principal (ou manual)
        }
      }
    } else if (cmd == "auto" && flags.modoManualIlum) {
      // Transicionando PARA FORA do modo manual
      flags.modoManualIlum = false;
      // No retorno ao auto, pode haver um feedback breve no LCD (opcional)
      debugPrint("Comando recebido do app: luz automática");
      // As luzes desligarão se ninguém estiver presente, ou se o LDR estiver muito claro
      if (pessoas.total > 0) { // Se ainda há pessoas, o auto mode reassume
        flags.monitorandoLDR = true; 
        flags.ilumAtiva = false; 
      }
      configurarRele(0); // Garante que a luz desligue ao sair do manual para o auto
      atualizarLCD(); // Atualiza LCD imediatamente para refletir o modo automático
    }
  } else {
    debugPrint("Erro ao verificar comando de iluminação: " + String(httpCodeIlum));
  }
  http.end();
  
  // --- Comandos do Climatizador ---
  String urlClimaCmd = String(serverUrl) + "climatizador/comando";
  http.begin(urlClimaCmd);
  int httpCodeClimaCmd = http.GET();
  
  if (httpCodeClimaCmd == HTTP_CODE_OK) {
    String cmd = http.getString();
    cmd.trim();
    
    if (cmd != "none" && cmd.length() > 0) {
      // Força o modo manual do climatizador se um comando específico for enviado pelo app
      if (!flags.modoManualClima) {
          flags.modoManualClima = true;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.write(5); // Ícone clima
          lcd.print(" Clima: Manual");
          lcd.setCursor(0, 1);
          lcd.print("Control: APP"); // Origem do controle manual
          tocarSom(SOM_COMANDO); // Buzzer uma vez
          delay(1500); // Exibe mensagem temporariamente
      }
      
      // Agora, mostra a ação específica do comando brevemente
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.write(5);
      lcd.print(" Comando App");
      lcd.setCursor(0,1);
      if (cmd == "power") lcd.print("Power Toggle");
      else if (cmd == "velocidade") lcd.print("Velocidade");
      else if (cmd == "timer") lcd.print("Timer");
      else if (cmd == "umidificar") lcd.print("Umidificar");
      else if (cmd == "aleta_vertical") lcd.print("Aleta Vertical");
      else if (cmd == "aleta_horizontal") lcd.print("Aleta Horizontal");
      tocarSom(SOM_COMANDO); // Um beep rápido para a execução do comando
      delay(800); // Exibe por um momento
      
      debugPrint("Comando do app para climatizador: " + cmd);
      
      // Envia o comando IR correspondente
      if (cmd == "power") enviarComandoIR(IR_POWER);
      else if (cmd == "velocidade") enviarComandoIR(IR_VELOCIDADE);
      else if (cmd == "timer") enviarComandoIR(IR_TIMER);
      else if (cmd == "umidificar") enviarComandoIR(IR_UMIDIFICAR);
      else if (cmd == "aleta_vertical") enviarComandoIR(IR_ALETA_VERTICAL);
      else if (cmd == "aleta_horizontal") enviarComandoIR(IR_ALETA_HORIZONTAL);
      
      atualizarLCD(); // Volta para a tela principal (que agora será a de Modo Manual)
    }
  } else {
    debugPrint("Erro ao verificar comando do climatizador: " + String(httpCodeClimaCmd));
  }
  http.end();
  
  // --- Sincronizar estado do modo manual do climatizador (do servidor) ---
  String urlClimaManual = String(serverUrl) + "climatizador/manual";
  http.begin(urlClimaManual);
  int httpCodeClimaManual = http.GET();
  
  if (httpCodeClimaManual == HTTP_CODE_OK) {
    String resposta = http.getString();
    StaticJsonDocument<64> doc; // Pequeno o suficiente para {"modoManualClimatizador":true/false}
    DeserializationError error = deserializeJson(doc, resposta);
    
    if (!error) {
      if (doc.containsKey("modoManualClimatizador")) {
        bool modoManual = doc["modoManualClimatizador"];
        
        // Atualizar o modo manual se houver mudança
        if (modoManual != flags.modoManualClima) {
          flags.modoManualClima = modoManual;
          
          if (flags.modoManualClima) { // Transicionando PARA o modo manual (via sync)
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.write(5); // Ícone clima
            lcd.print(" Clima: Manual");
            lcd.setCursor(0, 1);
            lcd.print("Control: Sync"); // Origem da mudança: sincronização
            tocarSom(SOM_COMANDO); // Buzzer uma vez
            delay(1500); // Exibe mensagem temporariamente
          } else { // Transicionando PARA FORA do modo manual (via sync)
            debugPrint("Modo do climatizador alterado para: Automatico (via sync)");
          }
          atualizarLCD(); // Atualiza LCD imediatamente para refletir o novo modo
        }
      } else {
         debugPrint("Chave 'modoManualClimatizador' não encontrada na resposta.");
      }
    } else {
      debugPrint("Erro ao parsear JSON para modo manual do climatizador: " + String(error.c_str()));
    }
  } else {
    debugPrint("Erro ao verificar modo manual do climatizador: " + String(httpCodeClimaManual));
  }
  http.end();
}

void processarIRRecebido() {
  if (!IrReceiver.decode()) return;
  
  uint8_t protocolo = IrReceiver.decodedIRData.protocol;
  uint16_t endereco = IrReceiver.decodedIRData.address;
  uint8_t comando = IrReceiver.decodedIRData.command;
  IrReceiver.resume(); // Importante para continuar a receber comandos
  
  if (protocolo == IR_PROTOCOLO && endereco == IR_ENDERECO) {
    static unsigned long ultimoIRRecebido = 0;
    unsigned long agora = millis();
    
    if (agora - ultimoIRRecebido > 500) { // Debounce para comandos IR recebidos
      // Força o modo manual do climatizador se um comando físico for recebido
      if (!flags.modoManualClima) {
          flags.modoManualClima = true;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.write(5); // Ícone clima
          lcd.print(" Clima: Manual");
          lcd.setCursor(0, 1);
          lcd.print("Control: IR"); // Origem do controle manual
          tocarSom(SOM_COMANDO); // Buzzer uma vez
          delay(1500); // Exibe mensagem temporariamente
      }
      
      debugPrint("Comando IR recebido: 0x" + String(comando, HEX));
      
      // Agora, mostra a ação específica do comando brevemente
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.write(5);
      lcd.print(" Comando IR");
      lcd.setCursor(0,1);
      if (comando == IR_POWER) lcd.print("Power Toggle");
      else if (comando == IR_VELOCIDADE) lcd.print("Velocidade");
      else if (comando == IR_TIMER) lcd.print("Timer");
      else if (comando == IR_UMIDIFICAR) lcd.print("Umidificar");
      else if (comando == IR_ALETA_VERTICAL) lcd.print("Aleta Vertical");
      else if (comando == IR_ALETA_HORIZONTAL) lcd.print("Aleta Horizontal");
      tocarSom(SOM_COMANDO); // Um beep rápido para a execução do comando
      delay(800); // Exibe por um momento
      
      atualizarEstadoClima(comando); // Atualiza estado interno do climatizador
      ultimoIRRecebido = agora;
      atualizarLCD(); // Volta para a tela principal (que agora será a de Modo Manual)
    } else {
        debugPrint("Comando IR ignorado (debounce): 0x" + String(comando, HEX));
    }
  }
}

void monitorarWiFi() {
  static unsigned long ultimaVerif = 0;
  unsigned long agora = millis();
  
  if (agora - ultimaVerif > 30000) { // Verificar a cada 30s
    ultimaVerif = agora;
    bool estadoAtual = (WiFi.status() == WL_CONNECTED);
    
    if (estadoAtual != flags.wifiOk) {
      flags.wifiOk = estadoAtual;
      
      // Feedback para mudança de estado do WiFi
      if (flags.wifiOk) {
        // Conectado
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.write(4); // Ícone WiFi
        lcd.print(" WiFi Conectado!");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP().toString());
        tocarSom(SOM_CONECTADO);
        
        debugPrint("WiFi conectado: " + WiFi.localIP().toString());
        
        // Se há pessoas, atualizar preferências ao reconectar
        if (pessoas.total > 0 && !flags.atualizandoPref) {
          pessoas.prefsAtualizadas = false; // Força a reconsulta
          consultarPreferencias();
        }
      } else {
        // Desconectado
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.write(7); // Ícone Erro
        lcd.print(" WiFi Perdido");
        lcd.setCursor(0, 1);
        lcd.print("Reconectando...");
        tocarSom(SOM_DESCONECTADO);
        
        debugPrint("WiFi desconectado - tentando reconectar");
      }
      
      delay(1000);
      atualizarLCD();
    }
    // CORREÇÃO: Removido o uso de WiFi.reconnectMillis() que não existe.
    // O stack do ESP32 já tenta reconectar automaticamente.
    // } else if (!flags.wifiOk && agora - WiFi.reconnectMillis() > 10000) {
    //   debugPrint("Aguardando reconexão WiFi automática...");
    // }
  }
}

// === SETUP OTIMIZADO ===
void setup() {
  Serial.begin(115200);
  debugPrint("\n\n=== SISTEMA INICIANDO ===");
  
  // Configurar pinos
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELE_1, OUTPUT);
  pinMode(RELE_2, OUTPUT);
  pinMode(RELE_3, OUTPUT);
  pinMode(RELE_4, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  
  debugPrint("Pinos configurados.");
  
  // Inicializar o LCD e criar caracteres personalizados
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
  
  // Tela de inicialização com animação simples
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Home v3.0");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando");
  
  // Sequência de pontos animados
  for (int i = 0; i < 3; i++) {
    delay(200);
    lcd.print(".");
  }
  
  // Som de inicialização
  tocarSom(SOM_INICIAR);
  
  // Desligar todos os relés ao iniciar (para relés NC)
  configurarRele(0);
  
  SPI.begin();
  mfrc522.PCD_Init();
  dht.begin();
  
  debugPrint("Módulos SPI e DHT inicializados.");
  
  IrSender.begin(IR_SEND_PIN);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Com feedback de LED
  
  debugPrint("Módulos IR inicializados.");
  
  // WiFi com timeout e feedback
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Conectando WiFi");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  
  debugPrint("Conectando ao WiFi: " + String(ssid));
  
  WiFi.begin(ssid, password);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) { // Aumenta tentativas
    delay(500);
    lcd.setCursor(15, 1); // Ponto de animação no canto inferior direito
    if (tentativas % 2 == 0) lcd.print("."); else lcd.print(" "); // Piscando
    tentativas++;
  }
  
  flags.wifiOk = (WiFi.status() == WL_CONNECTED);
  
  if (flags.wifiOk) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(4); // Ícone WiFi
    lcd.print(" Conectado!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());
    tocarSom(SOM_CONECTADO);
    
    debugPrint("WiFi conectado: " + WiFi.localIP().toString());
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(7); // Ícone Erro
    lcd.print(" Sem WiFi");
    lcd.setCursor(0, 1);
    lcd.print("Modo Offline");
    tocarSom(SOM_ERRO);
    
    debugPrint("Falha na conexão WiFi");
  }
  
  // Inicializar todos os timestamps para que as primeiras verificações ocorram logo
  unsigned long agora = millis();
  for (int i = 0; i < 8; i++) {
    tempos[i] = agora - (INTERVALO_DADOS + 1000); // Força a primeira execução logo
  }
  
  // Primeira leitura de sensores e atualização do LCD
  lerSensores();
  atualizarLCD();
  
  delay(1000);
  animacaoTransicao();
  atualizarLCD();
  
  debugPrint("=== SISTEMA INICIALIZADO ===");
}

// === LOOP PRINCIPAL ULTRA OTIMIZADO ===
void loop() {
  // 1. Prioridade mais alta: IR (tempo real, interação física)
  processarIRRecebido();
  
  // 2. Segunda prioridade: NFC (detecção de presença)
  processarNFC();
  
  // 3. Leitura de Sensores (fundamental para automação)
  lerSensores();
  
  // 4. Automação (reage aos sensores e presença)
  gerenciarIluminacao();
  controleAutomaticoClima();
  
  // 5. Comunicação com o Servidor (pode ser menos frequente, usa timers)
  // Agrupa as chamadas de comunicação para não bloquear o loop.
  static unsigned long ultimoCicloComunicacao = 0;
  unsigned long agora = millis(); // Define agora para usar em todas as verificações neste ciclo
  
  if (agora - ultimoCicloComunicacao > 1000) { // Executar comunicação aprox. 1x por segundo
    ultimoCicloComunicacao = agora;
    
    // Monitora a conexão WiFi e tenta reconectar se necessário
    monitorarWiFi(); 
    
    // Verifica se há comandos pendentes do aplicativo
    verificarComandos();

    // 6. Verificar e atualizar preferências periodicamente (se houver pessoas)
    // Usamos tempos[5] para este timer.
    if (agora - tempos[5] >= INTERVALO_PREF_CHECK) {
      tempos[5] = agora; // Reseta o timer para a próxima verificação
      if (pessoas.total > 0) { // Só consulta se houver pessoas na sala
        debugPrint("Verificando atualizações de preferências periodicamente...");
        // A função consultarPreferencias já lida com:
        // - flags.wifiOk (só tenta se WiFi está OK)
        // - flags.atualizandoPref (evita múltiplas chamadas simultâneas)
        // - Atualização de pessoas.prefsAtualizadas após sucesso/falha
        consultarPreferencias(); 
      } else {
        // Se não há pessoas, podemos opcionalmente "resetar" a flag de prefsAtualizadas
        // para garantir que na próxima entrada de pessoa a consulta seja feita imediatamente.
        pessoas.prefsAtualizadas = false; 
      }
    }
    
    // Envia o estado atual do ESP32 para o servidor
    enviarDados();
    
    // Exibe informações de depuração periodicamente
    mostrarTelaDebug();
  }
  
  // Pequeno delay no final para estabilidade e evitar loop muito rápido
  delay(10); 
}