// luna_sensor.ino — Detector de salida de cama para Luna.
// FSR bajo el cojín de la cama -> ESP32 -> mensaje Telegram (+ buzzer opcional).
//
// Cableado FSR (divisor de voltaje):
//   3.3V ----[FSR]----+---- GPIO34 (ADC)
//                     |
//                  [10kΩ]
//                     |
//   GND --------------+
// Más peso => más voltaje => lectura ADC más alta.

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "luna_logic.h"

// ---------- CONFIG: editar antes de subir ----------
const char* WIFI_SSID = "TU_WIFI";
const char* WIFI_PASS = "TU_CLAVE_WIFI";
const char* BOT_TOKEN = "123456:ABC-DEF...";  // token de @BotFather
// chat_id de cada persona que recibe alertas (tú y tu hermana; ver README)
const char* CHAT_IDS[] = { "111111111", "222222222" };
const uint8_t N_CHATS = sizeof(CHAT_IDS) / sizeof(CHAT_IDS[0]);

const uint8_t FSR_PIN    = 34;  // ADC1_CH6 (solo entrada, ideal para esto)
const uint8_t BUZZER_PIN = 26;  // buzzer opcional
#define BUZZER_ON               // comenta esta línea si no usas buzzer

const float    OUT_FACTOR  = 0.5f;    // lectura < 50% del baseline => Luna salió
const uint32_t DEBOUNCE_MS = 5000;    // 5 s sostenidos => confirma cambio
const uint32_t REPEAT_MS   = 120000;  // re-alerta cada 2 min si sigue fuera
const uint32_t CMD_POLL_MS = 3000;    // revisa comandos Telegram cada 3 s
// ----------------------------------------------------

LunaDet det;
uint32_t lastUpdateId = 0;
uint32_t lastAlert = 0;   // último mensaje "salió" entregado (0 = ninguno)
uint32_t lastTry = 0;     // último intento de envío (para no martillar la API)

uint16_t readFsr() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(FSR_PIN);
  return sum / 8;
}

void calibrate() {
  uint32_t sum = 0;
  for (int i = 0; i < 30; i++) { sum += analogRead(FSR_PIN); delay(100); }
  det.baseline = sum / 30;
  det.in_bed = true;
  det.pending = false;
  lastAlert = 0;
}

String urlEncode(const String& s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else { char buf[4]; snprintf(buf, sizeof(buf), "%%%02X", c); out += buf; }
  }
  return out;
}

bool sendTelegramTo(const char* chatId, const String& text) {
  WiFiClientSecure client;
  // ponytail: sin validar certificado; solo enviamos texto a nuestro propio bot.
  // Upgrade: fijar el CA raíz de Telegram (setCACert).
  client.setInsecure();
  HTTPClient http;
  String url = String("https://api.telegram.org/bot") + BOT_TOKEN +
               "/sendMessage?chat_id=" + chatId + "&text=" + urlEncode(text);
  http.begin(client, url);
  int code = http.GET();
  http.end();
  return code == 200;
}

// Envía a todos los chat_id. true si al menos uno recibió.
bool sendTelegram(const String& text) {
  if (WiFi.status() != WL_CONNECTED) return false;
  bool ok = false;
  for (uint8_t i = 0; i < N_CHATS; i++) ok |= sendTelegramTo(CHAT_IDS[i], text);
  return ok;
}

void pollCommands(uint32_t now) {
  static uint32_t lastPoll = 0;
  if (now - lastPoll < CMD_POLL_MS) return;
  lastPoll = now;
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String("https://api.telegram.org/bot") + BOT_TOKEN +
               "/getUpdates?offset=" + String(lastUpdateId + 1) + "&limit=5";
  if (!http.begin(client, url)) return;
  if (http.GET() != 200) { http.end(); return; }
  String body = http.getString();
  http.end();

  int idx = body.lastIndexOf("\"update_id\":");
  if (idx < 0) return;
  lastUpdateId = (uint32_t)body.substring(idx + 12).toInt();

  // Comando "cal" (o /cal): recalibrar con Luna acostada en su cama.
  if (body.indexOf("\"text\":\"cal\"") >= 0 || body.indexOf("\"text\":\"/cal\"") >= 0) {
    calibrate();
    sendTelegram("Calibrado con Luna en la cama. Baseline=" + String(det.baseline));
  }
}

void beep(uint8_t n) {
#ifdef BUZZER_ON
  for (uint8_t i = 0; i < n; i++) { tone(BUZZER_PIN, 2000, 200); delay(400); }
#endif
}

void reconnectWifi() {
  static uint32_t lastAttempt = 0;
  uint32_t now = millis();
  if (now - lastAttempt < 10000) return;
  lastAttempt = now;
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(FSR_PIN, ADC_11db);  // rango útil ~0-3.3V

  det.out_factor = OUT_FACTOR;
  det.debounce_ms = DEBOUNCE_MS;

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print('.'); }
  Serial.println(" OK");

  delay(2000);   // margen para acomodar a Luna en su cama tras encender
  calibrate();

  String msg = "luna_sensor listo. Baseline=" + String(det.baseline);
  if (det.baseline < 300) {
    msg += " (BAJO: Luna esta en la cama? Revisa ubicacion del FSR y cableado)";
  }
  sendTelegram(msg);
}

void loop() {
  uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) reconnectWifi();

  static uint32_t lastSample = 0;
  if (now - lastSample >= 100) {
    lastSample = now;
    LunaEvent ev = luna_update(det, readFsr(), now);
    if (ev == EV_LEFT) {
      lastAlert = 0;  // dispara alerta inmediata abajo
    } else if (ev == EV_RETURNED) {
      lastAlert = 0;
      sendTelegram("Luna volvio a la cama.");
    }
  }

  // Alerta: inmediata al salir, luego cada REPEAT_MS hasta que vuelva.
  // Si el envío falla (sin WiFi), reintenta cada 5 s — la alerta no se pierde.
  if (!det.in_bed && (lastAlert == 0 || now - lastAlert >= REPEAT_MS) && now - lastTry >= 5000) {
    lastTry = now;
    if (sendTelegram("ALERTA: Luna salio de la cama.")) {
      lastAlert = now;
      beep(3);
    }
  }

  pollCommands(now);
}
