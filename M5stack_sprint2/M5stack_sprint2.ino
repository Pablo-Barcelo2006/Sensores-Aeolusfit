#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiUdp.h>

const char* SSID = "iPhone de Pablo";
const char* PASS = "Pablo2006";
const int PUERTO = 4210;

// Valores ajustables
const int UMBRAL_HUMEDAD = 80;                // % de humedad medida para avisar de lluvia
const int UMBRAL_LLUVIA = 50;                 // % de probabilidad de lluvia de AEMET para avisar
const unsigned long PUERTA_OLVIDADA = 30000;  // ms abierta sin nadie antes del pitido
const unsigned long DATOS_VIEJOS = 60000;     // ms sin datos del exterior = aviso

WiFiUDP udp;
float temp = NAN, hum = NAN;
float aemetMax = NAN, aemetMin = NAN, aemetLluvia = NAN;
char nombrePerfil[24] = "Sin perfil";
float ajustePerfil = 0;  // lo envía la Raspberry Pi: friolero -3, normal 0, caluroso +3
unsigned long ultimoClima = 0, ultimaPresencia = 0, ultimoDibujo = 0;
bool puerta = false, presencia = false;
bool pitadoLluvia = false, pitadoPuerta = false;
bool redibujar = true;

// Lee el número que va detrás de una clave en un JSON sencillo
bool leer(const char* msg, const char* clave, float& valor) {
  const char* p = strstr(msg, clave);
  if (!p) return false;
  valor = atof(p + strlen(clave));
  return true;
}

// Lee el texto entre comillas que va detrás de una clave
void leerTexto(const char* msg, const char* clave, char* destino, size_t tam) {
  const char* p = strstr(msg, clave);
  if (!p) return;
  p += strlen(clave);
  size_t i = 0;
  while (*p && *p != '"' && i < tam - 1) destino[i++] = *p++;
  destino[i] = 0;
}

void pedirSiguientePerfil() {
  udp.beginPacket(IPAddress(255, 255, 255, 255), PUERTO);
  udp.print("{\"tipo\":\"perfil_siguiente\"}");
  udp.endPacket();
}

void recibir() {
  if (!udp.parsePacket()) return;
  char msg[160];
  int len = udp.read(msg, sizeof(msg) - 1);
  if (len <= 0) return;
  msg[len] = 0;
  float v;

  if (strstr(msg, "\"clima\"")) {
    if (leer(msg, "\"t\":", v)) temp = v;
    if (leer(msg, "\"h\":", v)) hum = v;
    ultimoClima = millis();
    redibujar = true;
  } else if (strstr(msg, "\"armario\"")) {
    if (leer(msg, "\"presencia\":", v)) presencia = v;
    if (presencia) ultimaPresencia = millis();
    if (leer(msg, "\"puerta\":", v) && (bool)v != puerta) {
      puerta = v;
      if (puerta) ultimaPresencia = millis();  // la cuenta empieza al abrir
      redibujar = true;
    }
  } else if (strstr(msg, "\"aemet\"")) {
    if (leer(msg, "\"max\":", v)) aemetMax = v;
    if (leer(msg, "\"min\":", v)) aemetMin = v;
    if (leer(msg, "\"lluvia\":", v)) aemetLluvia = v;
    redibujar = true;
  } else if (strstr(msg, "\"tipo\":\"perfil\"")) {  // perfil activo, enviado por la Raspberry Pi
    char nuevo[24] = "";
    leerTexto(msg, "\"nombre\":\"", nuevo, sizeof(nuevo));
    if (leer(msg, "\"ajuste\":", v)) ajustePerfil = v;
    if (strcmp(nuevo, nombrePerfil) != 0) {
      strcpy(nombrePerfil, nuevo);
      redibujar = true;
    }
  }
}

bool hayLluvia() {
  return (!isnan(hum) && hum >= UMBRAL_HUMEDAD) ||
         (!isnan(aemetLluvia) && aemetLluvia >= UMBRAL_LLUVIA);
}

const char* ropa(float sensacion) {
  if (sensacion < 10) return "Abrigo, jersey,\npantalon largo y botas";
  if (sensacion < 17) return "Chaqueta, sudadera\ny pantalon largo";
  if (sensacion < 24) return "Manga larga\ny pantalon";
  return "Camiseta y\npantalon corto";
}

void dibujar() {
  auto& d = M5.Display;
  if (!puerta) {  // puerta cerrada: pantalla apagada
    d.fillScreen(TFT_BLACK);
    d.setBrightness(0);
    return;
  }
  d.setBrightness(128);
  d.fillScreen(TFT_WHITE);
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  d.setTextSize(2);
  d.setCursor(10, 10);
  d.print(nombrePerfil);

  if (isnan(temp)) {
    d.setCursor(10, 60);
    d.print("Esperando datos...");
    return;
  }
  d.setTextSize(3);
  d.setCursor(10, 50);
  d.printf("%.1f C  %.0f%%", temp, hum);
  d.setTextSize(2);
  d.setCursor(10, 100);
  d.print(ropa(temp + ajustePerfil));

  if (!isnan(aemetMax)) {
    d.setCursor(10, 140);
    d.printf("AEMET: %.0f-%.0f C  Lluvia %.0f%%", aemetMin, aemetMax, aemetLluvia);
  }
  int y = 168;
  if (hayLluvia()) {
    d.setTextColor(TFT_BLUE, TFT_WHITE);
    d.setCursor(10, y); d.print("Llevate paraguas"); y += 20;
  }
  if (!isnan(aemetMin) && temp - aemetMin >= 5) {
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    d.setCursor(10, y); d.print("Lleva una capa extra"); y += 20;
  }
  if (millis() - ultimoClima > DATOS_VIEJOS) {
    d.setTextColor(TFT_RED, TFT_WHITE);
    d.setCursor(10, y); d.print("Datos sin actualizar");
  }
  d.setTextColor(TFT_DARKGREY, TFT_WHITE);
  d.setTextSize(1);
  d.setCursor(110, 228);
  d.print("B: siguiente perfil");
}

void avisos() {
  if (!puerta) {  // al cerrar se rearman los avisos
    pitadoLluvia = pitadoPuerta = false;
    return;
  }
  if (!pitadoLluvia && hayLluvia()) {
    M5.Speaker.tone(2000, 200);  // un pitido de lluvia por apertura
    pitadoLluvia = true;
  }
  if (!pitadoPuerta && millis() - ultimaPresencia > PUERTA_OLVIDADA) {
    M5.Speaker.tone(1000, 400);  // un pitido de puerta olvidada
    pitadoPuerta = true;
  }
}

void setup() {
  M5.begin(M5.config());
  M5.Speaker.setVolume(128);
  M5.Display.setTextSize(2);
  M5.Display.print("Conectando WiFi...");
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  udp.begin(PUERTO);
  M5.Display.println();
  M5.Display.println(WiFi.localIP());
  delay(1500);
}

void loop() {
  M5.update();
  recibir();

  if (M5.BtnB.wasPressed()) pedirSiguientePerfil();  // la Raspberry Pi responde con el nuevo perfil

  avisos();
  if (puerta && millis() - ultimoDibujo > 5000) redibujar = true;  // refresca el aviso de datos viejos
  if (redibujar) {
    dibujar();
    redibujar = false;
    ultimoDibujo = millis();
  }
  delay(10);
}