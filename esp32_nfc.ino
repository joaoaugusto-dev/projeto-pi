#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>

#define SS_PIN 5
#define RST_PIN 22
#define LED_PIN 2
#define TEMPO_LIMITE_REGISTRO 30000 // 30 segundos em milissegundos

MFRC522 mfrc522(SS_PIN, RST_PIN);

const char* ssid = "João Augusto";
const char* password = "131103r7";
const char* serverUrl = "https://fantastic-palm-tree-gg4rqqg756939qxg-3000.app.github.dev/esp32/";

bool modoRegistroTag = false;
String matriculaRegistro = "";
unsigned long ultimaVerificacao = 0;
unsigned long inicioRegistro = 0;
const unsigned long intervaloVerificacao = 2000; // 2 segundos

void piscarLED(int vezes, int tempoOn, int tempoOff) {
    for(int i = 0; i < vezes; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(tempoOn);
        digitalWrite(LED_PIN, LOW);
        delay(tempoOff);
    }
}

void verificarSolicitacaoRegistro() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    // Se estiver em modo registro, verifica se excedeu o tempo limite
    if (modoRegistroTag && (millis() - inicioRegistro >= TEMPO_LIMITE_REGISTRO)) {
        modoRegistroTag = false;
        matriculaRegistro = "";
        Serial.println("\nTempo limite de registro excedido");
        piscarLED(3, 100, 100);
        return;
    }
    
    HTTPClient http;
    String url = String(serverUrl) + "tag-status/pendente";
    http.begin(url);
    
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error && doc.containsKey("status")) {
            String status = doc["status"].as<String>();
            if (status == "aguardando" && !modoRegistroTag) {
                matriculaRegistro = doc["matricula"].as<String>();
                modoRegistroTag = true;
                inicioRegistro = millis(); // Inicia o contador de tempo
                Serial.println("\nModo de registro ativado via web");
                Serial.println("Matrícula: " + matriculaRegistro);
                Serial.println("Aproxime a nova tag do leitor...");
                piscarLED(2, 200, 200);
            } else if (status == "nenhum" && modoRegistroTag) {
                modoRegistroTag = false;
                matriculaRegistro = "";
                Serial.println("\nRegistro de tag cancelado");
                piscarLED(3, 100, 100);
            }
        }
    }
    http.end();
}

void setup() {
    Serial.begin(115200);
    SPI.begin();
    mfrc522.PCD_Init();
    pinMode(LED_PIN, OUTPUT);

    WiFi.begin(ssid, password);
    Serial.println("Conectando ao WiFi...");
    
    while (WiFi.status() != WL_CONNECTED) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        delay(500);
        Serial.print(".");
    }
    
    digitalWrite(LED_PIN, HIGH);
    Serial.println("\nWiFi conectado!");
}

void loop() {
    unsigned long agora = millis();
    
    if (!modoRegistroTag && agora - ultimaVerificacao >= intervaloVerificacao) {
        verificarSolicitacaoRegistro();
        ultimaVerificacao = agora;
    }

    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        delay(200);
        return;
    }

    String tagNfc = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) {
            tagNfc += "0";
        }
        tagNfc += String(mfrc522.uid.uidByte[i], HEX);
    }
    tagNfc.toUpperCase();

    Serial.print("\n=== Nova Leitura ===\n");
    Serial.print("Tag NFC detectada: ");
    Serial.println(tagNfc);

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url;
        
        if (modoRegistroTag) {
            url = String(serverUrl) + "registrar-tag/" + matriculaRegistro;
            Serial.println("Registrando nova tag para matrícula: " + matriculaRegistro);
            
            http.begin(url);
            http.addHeader("Content-Type", "application/json");
            
            String jsonData = "{\"tag_nfc\":\"" + tagNfc + "\"}";
            int httpCode = http.POST(jsonData);

            switch (httpCode) {
                case HTTP_CODE_OK:
                    Serial.println("Tag registrada com sucesso!");
                    piscarLED(3, 500, 500); // 3 piscadas lentas = sucesso
                    break;
                case HTTP_CODE_BAD_REQUEST:
                    Serial.println("Erro: Tag já está em uso");
                    piscarLED(4, 200, 200); // 4 piscadas médias = tag em uso
                    break;
                case HTTP_CODE_NOT_FOUND:
                    Serial.println("Erro: Matrícula não encontrada");
                    piscarLED(5, 100, 100); // 5 piscadas rápidas = matrícula não encontrada
                    break;
                default:
                    Serial.print("Erro ao registrar tag. Código: ");
                    Serial.println(httpCode);
                    piscarLED(6, 100, 100); // 6 piscadas rápidas = erro geral
            }

            modoRegistroTag = false;
            matriculaRegistro = "";
        } else {
            url = String(serverUrl) + "tag/" + tagNfc;
            
            http.begin(url);
            int httpCode = http.GET();

            switch (httpCode) {
                case HTTP_CODE_OK:
                    {
                        String payload = http.getString();
                        DynamicJsonDocument doc(1024);
                        DeserializationError error = deserializeJson(doc, payload);

                        if (!error) {
                            const char* nome = doc["nome"];
                            float temperatura = doc["temperatura"];
                            int luminosidade = doc["luminosidade"];

                            Serial.println("\n=== Dados do Usuário ===");
                            Serial.print("Nome: ");
                            Serial.println(nome);
                            Serial.print("Temperatura: ");
                            Serial.println(temperatura);
                            Serial.print("Luminosidade: ");
                            Serial.println(luminosidade);
                            Serial.println("=====================\n");

                            piscarLED(1, 1000, 0); // 1 piscada longa = sucesso
                        } else {
                            Serial.println("Erro ao processar JSON");
                            piscarLED(2, 200, 200); // 2 piscadas = erro no JSON
                        }
                    }
                    break;
                case HTTP_CODE_NOT_FOUND:
                    Serial.println("Tag não cadastrada no sistema");
                    piscarLED(3, 300, 300); // 3 piscadas médias = tag não encontrada
                    break;
                default:
                    Serial.print("Erro na requisição HTTP: ");
                    Serial.println(httpCode);
                    piscarLED(3, 100, 100); // 3 piscadas rápidas = erro HTTP
            }
        }

        http.end();
    } else {
        Serial.println("Erro: WiFi desconectado!");
        piscarLED(7, 50, 50); // 7 piscadas muito rápidas = erro de WiFi
    }

    delay(2000);
}