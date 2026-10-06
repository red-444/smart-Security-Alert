// CampusEye + WiFi dashboard API (your original logic, plus WebServer)
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WebServer.h>

const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASS = "YOUR_PASSWORD";
const char* CONTROL_KEY = "1234";          // change this PIN

#define RFID_SS_PIN 5
#define RFID_RST_PIN 4
#define PIR_PIN 27
#define LED_PIN 26
#define BUZZER_PIN 16                       // LOW = ON

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
RTC_DS3231 rtc;
WebServer server(80);

byte authorizedUID[] = {0xC7, 0x8D, 0xB7, 0x31};   // replace before publishing code
const byte UID_SIZE = 4;
const int START_MIN = 18 * 60, END_MIN = 7 * 60;
const unsigned long RFID_WAIT = 5000, ALARM_TIME = 5000;

bool systemArmed = true, alarmActive = false, motionHandled = false;
unsigned long alarmStart = 0;

String events[10];
int evCount = 0;

String timeStr() {
  DateTime n = rtc.now();
  char b[12];
  snprintf(b, sizeof(b), "%02d:%02d:%02d", n.hour(), n.minute(), n.second());
  return String(b);
}

void addEvent(String s) {
  s = timeStr() + "  " + s;
  if (evCount < 10) events[evCount++] = s;
  else { for (int i = 1; i < 10; i++) events[i - 1] = events[i]; events[9] = s; }
  Serial.println(s);
}

bool isMonitoring() {
  DateTime n = rtc.now();
  int m = n.hour() * 60 + n.minute();
  return (START_MIN > END_MIN) ? (m >= START_MIN || m < END_MIN) : (m >= START_MIN && m < END_MIN);
}

void startAlarm() {
  alarmActive = true; alarmStart = millis();
  digitalWrite(LED_PIN, HIGH); digitalWrite(BUZZER_PIN, LOW);
  addEvent("ALARM ON");
}

void stopAlarm() {
  alarmActive = false;
  digitalWrite(LED_PIN, LOW); digitalWrite(BUZZER_PIN, HIGH);
  addEvent("ALARM OFF");
}

bool readRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return false;
  bool ok = (rfid.uid.size == UID_SIZE);
  for (byte i = 0; ok && i < UID_SIZE; i++) ok = (rfid.uid.uidByte[i] == authorizedUID[i]);
  rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
  addEvent(ok ? "RFID: authorized card" : "RFID: unauthorized card");
  return ok;
}

// ---------- web API ----------
void cors() { server.sendHeader("Access-Control-Allow-Origin", "*"); }

void handleStatus() {
  String j = "{\"time\":\"" + timeStr() + "\",\"monitoring\":" + (isMonitoring() ? "true" : "false") +
             ",\"armed\":" + (systemArmed ? "true" : "false") +
             ",\"pir\":" + (digitalRead(PIR_PIN) ? "true" : "false") +
             ",\"alarm\":" + (alarmActive ? "true" : "false") + ",\"events\":[";
  for (int i = 0; i < evCount; i++) { if (i) j += ","; j += "\"" + events[i] + "\""; }
  j += "]}";
  cors(); server.send(200, "application/json", j);
}

void handleControl() {
  cors();
  if (server.arg("key") != CONTROL_KEY) { server.send(403, "application/json", "{\"ok\":false}"); return; }
  String c = server.arg("cmd");
  if (c == "arm") { systemArmed = true; addEvent("System ARMED (web)"); }
  else if (c == "disarm") { systemArmed = false; if (alarmActive) stopAlarm(); addEvent("System DISARMED (web)"); }
  else if (c == "stop") { if (alarmActive) stopAlarm(); }
  else if (c == "test") { startAlarm(); }
  server.send(200, "application/json", "{\"ok\":true}");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT); pinMode(LED_PIN, OUTPUT); pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); digitalWrite(BUZZER_PIN, HIGH);
  SPI.begin(18, 19, 23, RFID_SS_PIN);
  rfid.PCD_Init();
  Wire.begin(21, 22);
  if (!rtc.begin()) Serial.println("ERROR: DS3231 not detected");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println();
  Serial.print("Dashboard IP: "); Serial.println(WiFi.localIP());

  server.on("/status", handleStatus);
  server.on("/control", handleControl);
  server.begin();
  addEvent("System ready");
}

void loop() {
  server.handleClient();

  if (alarmActive) {
    unsigned long e = millis() - alarmStart;
    digitalWrite(BUZZER_PIN, (e % 600) < 300 ? LOW : HIGH);
    digitalWrite(LED_PIN, HIGH);
    if (e >= ALARM_TIME) { stopAlarm(); motionHandled = true; }
    delay(20);
    return;
  }

  bool motion = digitalRead(PIR_PIN) == HIGH;

  if (!(isMonitoring() && systemArmed)) {
    digitalWrite(LED_PIN, LOW); digitalWrite(BUZZER_PIN, HIGH);
    motionHandled = false;
    delay(50);
    return;
  }

  if (!motion) motionHandled = false;

  if (motion && !motionHandled) {
    motionHandled = true;
    addEvent("MOTION detected - waiting for RFID");
    unsigned long t0 = millis();
    bool ok = false;
    while (millis() - t0 < RFID_WAIT) {
      server.handleClient();               // keep dashboard responsive
      if (readRFID()) { ok = true; break; }
      delay(20);
    }
    if (ok) { addEvent("AUTHORIZED - alarm suppressed"); }
    else { addEvent("UNAUTHORIZED ACCESS"); startAlarm(); }
  }
  delay(50);
}
