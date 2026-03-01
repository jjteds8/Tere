#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <math.h>
#include <algorithm> 

/* --- GPIO CONFIGURATION --- */
#define SENSOR_SWITCH 25      
#define RTC_SQW_PIN   GPIO_NUM_32  
#define MAN_BTN_PIN   33      
#define PH_PIN        36
#define TDS_PIN       39
#define TURB_PIN      34      
#define SIM_TX        17
#define SIM_RX        16

/* --- GLOBALS & OBJECTS --- */
float phOff = 0.0, tdsOff = 0.0, turbBaseline = 3500.0;
float lastPH, lastTDS, lastTurb;
char phoneNum[20] = "+639519750562";

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;
HardwareSerial sim800(2);
Preferences prefs;

/* --- FUNCTION PROTOTYPES --- */
void setSensorPower(bool state);
float getFilteredReadings(int pin);
float calculateCorrectedNTU(float rawValue);
String getClarityStatus(float raw);
void loadCalibration();
void saveCalibration();
void countdownWithLiveDiagnostics(int seconds, String label, int pinToMonitor);
void runCalibrationProcedure();
void runMonitoringCycle(String type);
void setNextAlarm();

/* --- 1. NON-VOLATILE FLASH STORAGE --- */
void loadCalibration() {
  prefs.begin("daq_prefs", true); // true = Read Only
  phOff = prefs.getFloat("ph_off", 0.0);
  tdsOff = prefs.getFloat("tds_off", 0.0);
  turbBaseline = prefs.getFloat("turb_base", 3500.0);
  prefs.end();
}

void saveCalibration() {
  prefs.begin("daq_prefs", false); // false = Read/Write
  prefs.putFloat("ph_off", phOff);
  prefs.putFloat("tds_off", tdsOff);
  prefs.putFloat("turb_base", turbBaseline);
  prefs.end();
}

/* --- 2. POWER CONTROL & FILTERING --- */
void setSensorPower(bool state) {
  digitalWrite(SENSOR_SWITCH, state ? HIGH : LOW);
  if(state) delay(1500); 
}

float getFilteredReadings(int pin) {
  int samples[15];
  long sum = 0;
  for (int i = 0; i < 15; i++) {
    samples[i] = analogRead(pin);
    delay(20); 
  }
  std::sort(samples, samples + 15);
  for (int i = 3; i < 12; i++) sum += samples[i]; 
  return (float)sum / 9.0;
}

/* --- 3. CLARITY & TURBIDITY LOGIC --- */
String getClarityStatus(float raw) {
  float diff = turbBaseline - raw;
  if (diff < 80)  return "CLEAR"; 
  if (diff < 400) return "MUDDY"; 
  return "DARK ";
}

float calculateCorrectedNTU(float rawValue) {
  float diff = turbBaseline - rawValue;
  // Use the 80-unit bracket for clear water reflection mapping
  if (diff < 80) return 0.0; 
  float ntu = diff * 0.08; 
  return constrain(ntu, 0.0, 20.0); 
}

/* --- 4. LIVE DIAGNOSTIC UI --- */
void countdownWithLiveDiagnostics(int seconds, String label, int pinToMonitor) {
  for (int i = seconds; i >= 0; i--) {
    lcd.setCursor(0, 0);
    lcd.print(label + "          ");
    int liveRaw = analogRead(pinToMonitor);
    lcd.setCursor(0, 1);
    lcd.print(String(i) + "s RAW:" + String(liveRaw) + "    ");
    delay(1000);
  }
}

/* --- 5. CALIBRATION --- */
void runCalibrationProcedure() {
  setSensorPower(true);
  lcd.clear(); lcd.print("SYSTEM CALIBRATE");
  delay(1500);

  countdownWithLiveDiagnostics(5, "PH 7.0 NEUTRAL", PH_PIN);
  phOff = 7.0 - ((getFilteredReadings(PH_PIN) * 3.3 / 4095.0) * 3.5);
  
  countdownWithLiveDiagnostics(5, "TDS 0PPM PURE", TDS_PIN);
  tdsOff = 0.0 - (getFilteredReadings(TDS_PIN) * 0.5);

  countdownWithLiveDiagnostics(5, "TURB ZERO (CASE)", TURB_PIN);
  turbBaseline = getFilteredReadings(TURB_PIN); 

  // SAVE ALL NEW OFFSETS TO FLASH MEMORY
  saveCalibration();

  lcd.clear(); lcd.print("CALIBRATION DONE");
  lcd.setCursor(0,1); lcd.print("FLASH SAVED!");
  delay(3000);
  setSensorPower(false); 
}

/* --- 6. FIELD MONITORING --- */
void runMonitoringCycle(String type) {
  setSensorPower(true);

  lcd.clear(); lcd.print("READING TDS..."); delay(3000);
  lastTDS = constrain((getFilteredReadings(TDS_PIN) * 0.5) + tdsOff, 0.0, 2000.0);

  lcd.clear(); lcd.print("READING PH..."); delay(3000);
  lastPH = constrain(((getFilteredReadings(PH_PIN) * 3.3 / 4095.0) * 3.5) + phOff, 0.0, 14.0);

  lcd.clear(); lcd.print("READING TURB..."); delay(3000);
  float rawTurb = getFilteredReadings(TURB_PIN);
  lastTurb = calculateCorrectedNTU(rawTurb);
  String clarity = getClarityStatus(rawTurb);

  setSensorPower(false); 

  lcd.clear(); lcd.print("GSM CONNECTING..");
  delay(8000); 
  
  bool safe = (lastTDS <= 500 && lastPH >= 6.5 && lastPH <= 8.5 && lastTurb <= 5.0);
  String msg = "DRINKING WELL\n" + type + "\nTDS: " + String((int)lastTDS) + "\nPH: " + String(lastPH, 1) + "\nTURB: " + String(lastTurb, 1) + " [" + clarity + "]\nSTAT: " + (safe ? "POTABLE" : "UNSAFE");
  
  sim800.println("AT+CMGF=1"); delay(500);
  sim800.print("AT+CMGS=\""); sim800.print(phoneNum); sim800.println("\"");
  delay(1000); sim800.print(msg); delay(500); sim800.write(26);
  delay(4000); 
  
  // Flash clarity on screen before sequence ends
  lcd.clear(); lcd.print("TURB:" + String(lastTurb, 1) + " NTU");
  lcd.setCursor(0, 1); lcd.print("WATER IS " + clarity);
  delay(4000);
}

/* --- 7. ALARM LOGIC --- */
void setNextAlarm() {
  DateTime now = rtc.now();
  int sched[3][2] = {{5, 55}, {11, 55}, {16, 55}};
  DateTime next; bool found = false;
  for(int i=0; i<3; i++) {
    DateTime alarm(now.year(), now.month(), now.day(), sched[i][0], sched[i][1], 0);
    if (alarm.unixtime() > now.unixtime()) { next = alarm; found = true; break; }
  }
  if (!found) {
    DateTime t = now + TimeSpan(1, 0, 0, 0);
    next = DateTime(t.year(), t.month(), t.day(), 5, 55, 0);
  }
  rtc.clearAlarm(1); rtc.setAlarm1(next, DS3231_A1_Hour);
}

/* --- 8. MAIN SETUP & SLEEP --- */
void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_SWITCH, OUTPUT);
  setSensorPower(false); 
  pinMode(MAN_BTN_PIN, INPUT_PULLUP);

  Wire.begin(21, 22); lcd.init(); lcd.backlight(); rtc.begin();
  rtc.writeSqwPinMode(DS3231_OFF);
  sim800.begin(9600, SERIAL_8N1, SIM_RX, SIM_TX);

  // LOAD CALIBRATION FROM FLASH ON EVERY BOOT
  loadCalibration();

  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  uint64_t status = esp_sleep_get_ext1_wakeup_status();

  bool isButtonWake = (reason == ESP_SLEEP_WAKEUP_EXT1 && (status & (1ULL << MAN_BTN_PIN)));
  bool isColdBoot = (reason != ESP_SLEEP_WAKEUP_EXT1);

  if (isButtonWake || (isColdBoot && digitalRead(MAN_BTN_PIN) == LOW)) {
    lcd.clear(); lcd.print("HOLD 3S TO CAL");
    unsigned long pressTime = millis();
    bool held = true;
    
    while (millis() - pressTime < 3000) {
      if (digitalRead(MAN_BTN_PIN) == HIGH) { held = false; break; }
      delay(50);
    }
    
    if (held) runCalibrationProcedure();
    else runMonitoringCycle("MANUAL");
    
  } else if (reason == ESP_SLEEP_WAKEUP_EXT1 && (status & (1ULL << RTC_SQW_PIN))) {
    runMonitoringCycle("SCHEDULED");
  } else {
    runMonitoringCycle("INITIAL");
  }

  setNextAlarm();

  // --- FINAL DATA REVIEW ---
  lcd.clear(); lcd.print("---FINAL DATA---"); delay(2000);
  
  lcd.clear(); lcd.print("TDS Reading:");
  lcd.setCursor(0, 1); lcd.print(String((int)lastTDS) + " ppm"); delay(3000);

  lcd.clear(); lcd.print("pH Reading:");
  lcd.setCursor(0, 1); lcd.print(String(lastPH, 1)); delay(3000);

  bool safe = (lastTDS <= 500 && lastPH >= 6.5 && lastPH <= 8.5 && lastTurb <= 5.0);
  lcd.clear(); lcd.print("WELL STATUS:");
  lcd.setCursor(0, 1); lcd.print(safe ? "SAFE TO DRINK" : "UNSAFE TO DRINK");
  delay(4000);
  // -------------------------

  lcd.clear(); lcd.print("TASK COMPLETED");
  lcd.setCursor(0, 1); lcd.print("SLEEPING NOW...");
  delay(3000);

  lcd.noBacklight();
  setSensorPower(false); 
  
  uint64_t mask = (1ULL << RTC_SQW_PIN) | (1ULL << MAN_BTN_PIN);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ALL_LOW);
  esp_deep_sleep_start();
}

void loop() {}