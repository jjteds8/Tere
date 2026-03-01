
---

# 📘 System Documentation: IoT Drinking Well Water Quality Monitor

## 1. System Overview

This system is an ultra-low-power, IoT-enabled water quality monitoring node designed for drinking wells. It operates predominantly in a Deep Sleep state to conserve battery, waking only at hardware-triggered intervals (via an RTC) or manual user interrupts. It samples critical water parameters, evaluates potability against strict threshold standards, and transmits the results via SMS.

**Potability Standards Evaluated:**
| Parameter | Safe Threshold | Functional Purpose |
| :--- | :--- | :--- |
| **Turbidity** | $\le$ 1.0 NTU | Assesses water clarity; detects suspended solids that can harbor bacteria. |
| **TDS (Total Dissolved Solids)** | 300 - 500 mg/L | Ensures the water is adequately mineralized but not overly saturated with salts. |
| **pH Level** | 6.5 - 8.5 | Verifies neutral acidity to prevent pipe corrosion or alkaline scaling. |

---

## 2. Hardware & GPIO Configuration

The system relies on an ESP32 microcontroller interfacing with analog sensors, an I2C bus, and a UART GSM module. Power to the analog sensors is physically gated using an NPN transistor to prevent corrosion and battery drain during sleep cycles.

### ESP32 Pin Mapping Table

| Component | ESP32 Pin | I/O Type | Description / Circuit Notes |
| --- | --- | --- | --- |
| **BC547 Transistor Base** | GPIO 25 | Output | Switches the VBus (Power) for all analog sensors. |
| **DS3231 RTC Alarm (SQW)** | GPIO 32 | Input | Triggers scheduled wakeups (Active Low). |
| **Manual Trigger Button** | GPIO 33 | Input | Triggers manual readings or calibration. Requires an external 10kΩ pull-up resistor to 3.3V. |
| **pH Sensor** | GPIO 36 (VP) | Analog In | Reads raw pH voltage. |
| **TDS Sensor** | GPIO 39 (VN) | Analog In | Reads raw conductivity voltage. |
| **Turbidity Sensor** | GPIO 34 | Analog In | Reads raw clarity voltage via a 10k/20k voltage divider. |
| **SIM800L TX** | GPIO 17 (RX2) | UART RX | Receives serial data from the GSM module. |
| **SIM800L RX** | GPIO 16 (TX2) | UART TX | Transmits AT commands to the GSM module. |
| **I2C SDA (RTC & LCD)** | GPIO 21 | I2C Data | Data line for the DS3231 RTC and 16x2 I2C LCD. |
| **I2C SCL (RTC & LCD)** | GPIO 22 | I2C Clock | Clock line for the DS3231 RTC and 16x2 I2C LCD. |

---

## 3. Functional Software Blocks

The firmware is highly modular. Below are the core functional blocks that drive the system's precision and power efficiency.

### Block A: Non-Volatile Flash Storage

Instead of relying on volatile RAM, the system uses the ESP32's `Preferences.h` library to write calibration baselines directly to the internal flash memory. This ensures the device never "forgets" its zero-points, even if completely unplugged.

```cpp
void loadCalibration() {
  prefs.begin("daq_prefs", true); 
  phOff = prefs.getFloat("ph_off", 0.0);
  tdsOff = prefs.getFloat("tds_off", 0.0);
  turbBaseline = prefs.getFloat("turb_base", 3500.0);
  prefs.end();
}

```

### Block B: Truncated Mean Filtering (Noise Reduction)

Analog sensors are highly susceptible to electrical noise and water bubbles. This block captures 15 rapid samples, sorts them from lowest to highest, discards the top 3 and bottom 3 extreme outliers, and averages the remaining 9 stable readings.

```cpp
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

```

### Block C: Enclosure-Aware Turbidity Algorithm

Turbidity enclosures often reflect IR light, causing false "dirty" readings. This logic creates an 80-unit "deadzone" bracket around the saved baseline. If the raw reading falls inside this bracket, the enclosure noise is ignored, and the water is classified as perfectly clear (0.0 NTU).

```cpp
float calculateCorrectedNTU(float rawValue) {
  float diff = turbBaseline - rawValue;
  if (diff < 80) return 0.0; // Enclosure noise bracket
  float ntu = diff * 0.08;   // High-sensitivity slope for drinking water
  return constrain(ntu, 0.0, 20.0); 
}

```

### Block D: Power Control & Deep Sleep Routing

The system physically disconnects power to the sensors when not actively reading. Upon waking, it evaluates the `EXT1` wakeup register to determine if it was woken by the RTC clock (Scheduled) or the user button (Manual/Calibration).

```cpp
void setSensorPower(bool state) {
  digitalWrite(SENSOR_SWITCH, state ? HIGH : LOW);
  if(state) delay(1500); // Wake-up stabilization delay
}

```

---

## 4. Operating Instructions

### Calibration Mode (Baseline Setup)

Calibration must be performed upon initial deployment or if the physical sensor housing is altered. You will need a pH 7.0 buffer solution, pure distilled water, and the final turbidity enclosure filled with clear water.

1. **Wake the System:** Press the physical button on GPIO 33 once.
2. **Trigger the Interrupt:** Immediately upon seeing `HOLD 3S TO CAL` on the LCD, press and hold the button for 3 full seconds until the screen changes.
3. **Execute the 5-Second Routines:**
* **pH:** Place the pH probe in the 7.0 buffer. The screen displays the live raw ADC value. Wait 5 seconds.
* **TDS:** Place the TDS probe in pure distilled water (0 ppm). Wait 5 seconds.
* **Turbidity:** Ensure the turbidity sensor is in clear water *inside its casing*. Wait 5 seconds.


4. **Confirmation:** The LCD will display `FLASH SAVED!`. The baselines are now permanently stored.

### Manual Water Testing

To take an on-demand reading outside of the scheduled alarm times:

1. Press the GPIO 33 button once and release it immediately.
2. The system will skip the calibration hold, power on the sensors, evaluate the water, transmit a "MANUAL" SMS, and display the final parameters on the LCD before returning to sleep.

---

## 5. Troubleshooting Guide

| Symptom | Probable Cause | Corrective Action |
| --- | --- | --- |
| **System skips `HOLD 3S TO CAL` entirely** | Floating pin or unstable ground connection. | Verify the external 10kΩ pull-up resistor on GPIO 33 is securely connected to 3.3V, and the button completes the circuit to GND. |
| **Turbidity reads > 0 NTU in clean water** | Enclosure reflection / Baseline mismatch. | The casing is reflecting IR light. Run the Calibration Mode *while the sensor is mounted inside the casing* to zero out this optical noise. |
| **TDS reads 0 ppm continuously** | Sensor power failure. | Check the BC547 transistor circuit. Ensure GPIO 25 is outputting 3.3V and the transistor is properly switching the VBus line. |
| **LCD displays `UNSAFE` in clean bottled water** | Water lacks required mineral threshold. | The system strictly requires TDS to be $\ge$ 300 ppm. Pure bottled water is usually < 100 ppm. Dissolve a tiny pinch of salt to manually raise the TDS for demonstration purposes. |
| **Device resets during "GSM CONNECTING"** | Power supply voltage drop / Brownout. | The SIM800L module draws up to 2A in short bursts. Ensure the power supply (e.g., LM2596 buck converter) is tuned to 4.0V and a 1000µF capacitor is placed across the GSM power pins. |

---

### Appendix: Full System Source Code

As established, here is the complete, integrated code block for this system architecture.

```cpp
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
  prefs.begin("daq_prefs", true); 
  phOff = prefs.getFloat("ph_off", 0.0);
  tdsOff = prefs.getFloat("tds_off", 0.0);
  turbBaseline = prefs.getFloat("turb_base", 3500.0);
  prefs.end();
}

void saveCalibration() {
  prefs.begin("daq_prefs", false); 
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
  
  bool safe = (lastTDS >= 300 && lastTDS <= 500 && lastPH >= 6.5 && lastPH <= 8.5 && lastTurb <= 1.0);
  String msg = "DRINKING WELL\n" + type + "\nTDS: " + String((int)lastTDS) + "\nPH: " + String(lastPH, 1) + "\nTURB: " + String(lastTurb, 1) + " [" + clarity + "]\nSTAT: " + (safe ? "POTABLE" : "UNSAFE");
  
  sim800.println("AT+CMGF=1"); delay(500);
  sim800.print("AT+CMGS=\""); sim800.print(phoneNum); sim800.println("\"");
  delay(1000); sim800.print(msg); delay(500); sim800.write(26);
  delay(4000); 
  
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
  
  lcd.clear(); lcd.print("Turbidity:");
  lcd.setCursor(0, 1); lcd.print(String(lastTurb, 1) + " NTU"); delay(3000);

  bool safe = (lastTDS >= 300 && lastTDS <= 500 && lastPH >= 6.5 && lastPH <= 8.5 && lastTurb <= 1.0);
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

```
