#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

#define PIN_DHT        4
#define PIN_LDR        34
#define PIN_CO2_POT    35
#define PIN_BUTTON     27
#define PIN_LED_GREEN  25
#define PIN_LED_YELLOW 26
#define PIN_LED_RED    33
#define PIN_BUZZER     14

DHT dht(PIN_DHT, DHT22);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- Threshold (bisa diubah via Serial command KEY=VALUE) ----------
float tempWarning=28.0, tempAlarm=35.0;
float humWarningLow=30.0, humWarningHigh=80.0;
float humAlarmLow=20.0, humAlarmHigh=90.0;
float lightWarning=20.0, lightAlarm=10.0;
float co2Warning=800.0, co2Alarm=1200.0;

int mode=1; // 1 suhu/kelembaban, 2 cahaya/CO2, 3 status/uptime
float temperatureC=NAN, humidityPct=NAN, lightPct=0, co2ppm=400;
int systemStatus=0;      // 0 normal, 1 warning, 2 alarm
int previousStatus=0;    // deteksi transisi ke ALARM (Serial alert sekali saja)

unsigned long startedAt=0, lastSensorRead=0, lastDisplay=0, lastJson=0;
unsigned long lastButtonChange=0;
bool buttonStable=HIGH, buttonPrevious=HIGH;
String commandBuffer;

int previousMode=0; // paksa redraw penuh saat mode berganti

bool outside(float value, float low, float high) { return value < low || value > high; }

String statusText(int s) {
  return s==2 ? "ALARM" : (s==1 ? "WARNING" : "NORMAL");
}

// Cetak satu baris LCD dengan padding spasi penuh 16 kolom, tanpa lcd.clear()
// supaya tidak flicker.
void lcdPrintLine(int row, String text) {
  if (text.length() > 16) text = text.substring(0, 16);
  while (text.length() < 16) text += ' ';
  lcd.setCursor(0, row);
  lcd.print(text);
}

void processCommand(String line) {
  line.trim();
  int equalsAt = line.indexOf('=');
  if (equalsAt <= 0) { Serial.println("ERR: format KEY=VALUE"); return; }
  String key = line.substring(0, equalsAt); key.trim();
  float value = line.substring(equalsAt + 1).toFloat();

  if (key=="TEMP_WARN") tempWarning=value;
  else if (key=="TEMP_ALARM") tempAlarm=value;
  else if (key=="HUM_WARN_LOW") humWarningLow=value;
  else if (key=="HUM_WARN_HIGH") humWarningHigh=value;
  else if (key=="HUM_ALARM_LOW") humAlarmLow=value;
  else if (key=="HUM_ALARM_HIGH") humAlarmHigh=value;
  else if (key=="LIGHT_WARN") lightWarning=value;
  else if (key=="LIGHT_ALARM") lightAlarm=value;
  else if (key=="CO2_WARN") co2Warning=value;
  else if (key=="CO2_ALARM") co2Alarm=value;
  else { Serial.println("ERR: unknown threshold key"); return; }

  Serial.print("OK: "); Serial.print(key); Serial.print("="); Serial.println(value);
}

void pollSerial() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch=='\n' || ch=='\r') {
      if (commandBuffer.length()) { processCommand(commandBuffer); commandBuffer=""; }
    } else if (commandBuffer.length() < 80) {
      commandBuffer += ch;
    }
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin();          // default I2C pin ESP32: SDA=21, SCL=22
  lcd.init();
  lcd.backlight();
  dht.begin();

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  startedAt = millis();
  lcdPrintLine(0, "Env Monitor");
  lcdPrintLine(1, "System Ready");
  Serial.println("Monitor ready. Threshold command format: CO2_WARN=900");
}

void loop() {
  unsigned long now = millis();
  pollSerial();

  // --- Ganti mode saat tombol ditekan (debounce, deteksi tepi) ---
  bool buttonNow = digitalRead(PIN_BUTTON);
  if (buttonNow != buttonPrevious) { buttonPrevious = buttonNow; lastButtonChange = now; }
  if (now - lastButtonChange >= 35 && buttonNow != buttonStable) {
    buttonStable = buttonNow;
    if (buttonStable == LOW) {
      mode = (mode % 3) + 1;
      Serial.print("Mode="); Serial.println(mode);
    }
  }

  // --- Baca sensor & tentukan status tiap 2 detik ---
  if (now - lastSensorRead >= 2000 || lastSensorRead == 0) {
    lastSensorRead = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temperatureC = t;
    if (!isnan(h)) humidityPct = h;

    int ldrRaw = analogRead(PIN_LDR);
    int co2Raw = analogRead(PIN_CO2_POT);
    lightPct = 100.0f * (float)ldrRaw / 4095.0f;
    co2ppm   = 400.0f + 1600.0f * (float)co2Raw / 4095.0f;

    // Hitung JUMLAH parameter yang keluar dari rentang normal (warning
    // maupun alarm dianggap sama-sama "melebihi threshold").
    bool tOut = !isnan(temperatureC) && (temperatureC > tempWarning);
    bool hOut = !isnan(humidityPct) && outside(humidityPct, humWarningLow, humWarningHigh);
    bool lOut = lightPct < lightWarning;
    bool cOut = co2ppm > co2Warning;

    int outCount = (int)tOut + (int)hOut + (int)lOut + (int)cOut;
    systemStatus = (outCount >= 2) ? 2 : (outCount == 1 ? 1 : 0);

    digitalWrite(PIN_LED_GREEN,  systemStatus==0 ? HIGH : LOW);
    digitalWrite(PIN_LED_YELLOW, systemStatus==1 ? HIGH : LOW);
    digitalWrite(PIN_LED_RED,    systemStatus==2 ? HIGH : LOW);
    if (systemStatus==2) tone(PIN_BUZZER, 2200); else noTone(PIN_BUZZER);

    // Serial alert khusus, hanya sekali saat BARU masuk ke ALARM
    if (systemStatus==2 && previousStatus!=2) {
      Serial.print("ALERT: system entered ALARM state (");
      Serial.print(outCount); Serial.println(" parameters out of range)");
    }
    previousStatus = systemStatus;

    Serial.print("T="); Serial.print(temperatureC);
    Serial.print("C H="); Serial.print(humidityPct);
    Serial.print("% LDR="); Serial.print(ldrRaw);
    Serial.print(" ("); Serial.print(lightPct,1); Serial.print("%) CO2=");
    Serial.print(co2ppm,0); Serial.print("ppm status=");
    Serial.println(statusText(systemStatus));
  }

  // --- Update LCD tiap 500ms (tanpa clear(), anti-flicker) ---
  if (now - lastDisplay >= 500 || mode != previousMode) {
    lastDisplay = now;
    previousMode = mode;

    if (mode == 1) {
      String line0 = "T:" + (isnan(temperatureC) ? "--" : String(temperatureC,1)) +
                      "C H:" + (isnan(humidityPct) ? "--" : String(humidityPct,0)) + "%";
      lcdPrintLine(0, line0);
      lcdPrintLine(1, statusText(systemStatus));
    } else if (mode == 2) {
      lcdPrintLine(0, "Light:" + String(lightPct,0) + "%");
      lcdPrintLine(1, "CO2:" + String(co2ppm,0) + " " + statusText(systemStatus));
    } else {
      lcdPrintLine(0, "Status:" + statusText(systemStatus));
      lcdPrintLine(1, "Uptime " + String((now-startedAt)/1000) + " sec");
    }
  }

  // --- Kirim JSON tiap 5 detik ---
  if (now - lastJson >= 5000) {
    lastJson = now;
    Serial.printf("{\"temperature_c\":%.1f,\"humidity_pct\":%.1f,\"light_pct\":%.1f,\"co2_ppm\":%.0f,\"status\":\"%s\",\"uptime_s\":%lu}\n",
      temperatureC, humidityPct, lightPct, co2ppm,
      statusText(systemStatus).c_str(),
      (now-startedAt)/1000);
  }

  delay(5);
}
