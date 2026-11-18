#include <WiFi.h>
#include <WiFiUdp.h>

const char* SSID = "iPhone de Pablo";
const char* PASS = "Pablo2006";
const int PUERTO = 4210;

#define PIR_PIN 27
#define PUERTA_PIN 26
const unsigned long VENTANA = 5000;  // "hay alguien" si hubo movimiento en los últimos 5 s

WiFiUDP udp;
bool puertaAnt = false, presenciaAnt = false;
unsigned long ultimoMov = 0, ultimoEnvio = 0;

void enviar(bool puerta, bool presencia) {
  char msg[64];
  snprintf(msg, sizeof(msg), "{\"tipo\":\"armario\",\"puerta\":%d,\"presencia\":%d}",
           puerta, presencia);
  udp.beginPacket(IPAddress(255, 255, 255, 255), PUERTO);
  udp.print(msg);
  udp.endPacket();

  Serial.printf("Puerta: %-8s | Presencia: %-3s | Enviado: %s\n",
                puerta ? "ABIERTA" : "cerrada", presencia ? "SI" : "no", msg);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(PUERTA_PIN, INPUT_PULLUP);
  Serial.print("Conectando a ");
  Serial.println(SSID);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConectado, IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  bool puerta = digitalRead(PUERTA_PIN);  // 1 = abierta. Si sale al revés: !digitalRead(...)
  if (puerta && digitalRead(PIR_PIN)) ultimoMov = millis();  // con la puerta cerrada se ignora el PIR
  bool presencia = puerta && millis() - ultimoMov < VENTANA;

  // Envía al cambiar algo y, además, cada 10 s por si se pierde algún paquete UDP
  if (puerta != puertaAnt || presencia != presenciaAnt || millis() - ultimoEnvio > 10000) {
    enviar(puerta, presencia);
    puertaAnt = puerta;
    presenciaAnt = presencia;
    ultimoEnvio = millis();
  }
  delay(50);
}