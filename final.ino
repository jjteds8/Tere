#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <HardwareSerial.h>
#include <math.h>

/* --- GPIO CONFIGURATION --- */
#define SENSOR_SWITCH 25 // BC547 Base (NPN)
#define RTC_SQW_PIN   GPIO_NUM_32
#define MAN_BTN_PIN   GPIO_NUM_33
#define PH_PIN        36
#define TDS_PIN       39
#define TURB_PIN      34
#define SIM_TX        17
#define SIM_RX        16

/* --- PERSISTENT DATA --- */
RTC_DATA_ATTR float phOff = 0.0, tdsOff = 0.0, turbOff = 0.0;
RTC_DATA_ATTR float lastPH, lastTDS, lastTurb;
RTC_DATA_ATTR char phoneNum[20] = "+639519750562";

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;
HardwareSerial sim800(2);

/* --- POWER CONTROL --- */
void setSensorPower(bool state) {
  digitalWrite(SENSOR_SWITCH, state ? HIGH : LOW);
  if(state) {
    delay(2000); // Stabilization
  }
}

/* --- UI: COUNTDOWN WITH INTERRUPT --- */
bool countdownWithInterrupt(int seconds, String label) {
  for (int i = seconds; i >= 0; i--) {
    lcd.setCursor(0, 0);
    lcd.print(label + "          ");
    lcd.setCursor(0, 1);
    lcd.print("Time Left: " + String(i) + "s   ");
    
    if (digitalRead(33) == LOW) return true; 
    delay(1000);
  }
  return false;
}

/* --- MODE: CALIBRATION --- */
void runCalibrationProcedure() {
  setSensorPower(true);
  lcd.clear(); lcd.print("MODE: CALIBRATE");
  delay(2000);

  if(countdownWithInterrupt(30, "PH 7.0 STABILIZE")) return;
  phOff = 7.0 - ((analogRead(PH_PIN) * 3.3 / 4095.0) * 3.5);
  
  if(countdownWithInterrupt(30, "TDS 0PPM STAB")) return;
  tdsOff = 0.0 - (analogRead(TDS_PIN) * 0.5);

  if(countdownWithInterrupt(30, "TURB 0 STAB")) return;
  turbOff = 0.0 - (float)map(constrain(analogRead(TURB_PIN), 2000, 4000), 2000, 4000, 5, 0);

  lcd.clear(); lcd.print("OFFSETS SAVED");
  delay(3000);
  setSensorPower(false); // Immediate pull-down after calibration
}

/* --- MODE: FIELD MONITORING --- */
void runMonitoringCycle(String type) {
  setSensorPower(true);

  if (countdownWithInterrupt(60, "FIELD: TDS READ")) { runCalibrationProcedure(); return; }
  lastTDS = (analogRead(TDS_PIN) * 0.5) + tdsOff;

  if (countdownWithInterrupt(60, "FIELD: PH READ")) { runCalibrationProcedure(); return; }
  lastPH = ((analogRead(PH_PIN) * 3.3 / 4095.0) * 3.5) + phOff;

  if (countdownWithInterrupt(60, "FIELD: TURB READ")) { runCalibrationProcedure(); return; }
  lastTurb = (float)map(constrain(analogRead(TURB_PIN), 2000, 4000), 2000, 4000, 5, 0) + turbOff;

  // SMS Phase - Sensors remain ON during registration for stability
  lcd.clear(); lcd.print("GSM CONNECTING");
  delay(15000);
  
  bool safe = (lastTDS <= 500 && lastPH >= 6.5 && lastPH <= 8.5);
  String msg = type + " REPORT\nTDS: " + String((int)lastTDS) + " ppm\nPH: " + String(lastPH, 1) + "\nSTAT: " + (safe ? "SAFE" : "UNSAFE");
  
  sim800.println("AT+CMGF=1"); delay(500);
  sim800.print("AT+CMGS=\""); sim800.print(phoneNum); sim800.println("\"");
  delay(1000); sim800.print(msg); delay(500); sim800.write(26);
  delay(5000);
}

void setNextAlarm() {
  DateTime now = rtc.now();
  int sched[3][2] = {{5, 55}, {11, 55}, {16, 55}};
  DateTime next;
  bool found = false;
  for(int i=0; i<3; i++) {
    DateTime alarm(now.year(), now.month(), now.day(), sched[i][0], sched[i][1], 0);
    if (alarm.unixtime() > now.unixtime()) { next = alarm; found = true; break; }
  }
  if (!found) {
    DateTime t = now + TimeSpan(1, 0, 0, 0);
    next = DateTime(t.year(), t.month(), t.day(), 5, 55, 0);
  }
  rtc.clearAlarm(1);
  rtc.setAlarm1(next, DS3231_A1_Hour);
}

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_SWITCH, OUTPUT);
  setSensorPower(false); 
  pinMode(33, INPUT_PULLUP);

  Wire.begin(21, 22); lcd.init(); lcd.backlight(); rtc.begin();
  rtc.writeSqwPinMode(DS3231_OFF);
  sim800.begin(9600, SERIAL_8N1, SIM_RX, SIM_TX);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  uint64_t status = esp_sleep_get_ext1_wakeup_status();
  
  String reportType = "MANUAL";
  if (cause == ESP_SLEEP_WAKEUP_EXT1) {
     if (status & (1ULL << RTC_SQW_PIN)) reportType = "SCHEDULED";
  }
  
  runMonitoringCycle(reportType);

  setNextAlarm();
  
  // FINAL DISPLAY BEFORE SHUTDOWN
  lcd.clear(); 
  lcd.print("TASK COMPLETED");
  lcd.setCursor(0, 1);
  lcd.print("SLEEPING NOW...");
  delay(3000); // 3-second display window

  // PULL VBUS DOWN (Disconnect sensors from GND)
  setSensorPower(false); 
  
  uint64_t mask = (1ULL << RTC_SQW_PIN) | (1ULL << MAN_BTN_PIN);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ALL_LOW);
  esp_deep_sleep_start();
}

void loop() {}