#include <WiFi.h>
#include <WiFiUdp.h>
#include <DHT.h>

const char* SSID = "iPhone de Pablo";
const char* PASS = "Pablo2006";
const int PUERTO = 4210;
const unsigned long INTERVALO = 5000;  // demo: 5 s. Uso real: 1800000 (30 min)

DHT dht(13, DHT11);
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  dht.begin();
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
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    Serial.println("Error leyendo el DHT11");
  } else {
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"tipo\":\"clima\",\"t\":%.1f,\"h\":%.0f}", t, h);
    udp.beginPacket(IPAddress(255, 255, 255, 255), PUERTO);
    udp.print(msg);
    udp.endPacket();
    Serial.println(msg);
  }
  delay(INTERVALO);
}