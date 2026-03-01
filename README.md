
---

## 📘 System Documentation: IoT Drinking Well Monitor

### 1. System Overview

This system is a low-power, IoT-enabled water quality monitor designed for drinking wells. It operates primarily in a Deep Sleep state, waking at scheduled intervals to sample water parameters, evaluate potability based on strict standards, and transmit the results via SMS.

**Safety Parameters (Potability Logic):**
| Parameter | Safe Range | Diagnostic Indication |
| :--- | :--- | :--- |
| **Turbidity** | $\le 1.0$ NTU | Measures water clarity and suspended solids. |
| **TDS (Total Dissolved Solids)** | $300 - 500$ mg/L | Ensures water is neither demineralized nor overly saturated with salts. |
| **pH Level** | $6.5 - 8.5$ | Neutral range to prevent acidity or excessive alkalinity. |

---

### 2. Setup & Calibration Instructions

The system uses Non-Volatile Flash Memory to store calibration offsets. Calibration must be performed whenever probes are replaced or the physical enclosure changes.

**Preparation:**
Gather a pH 7.0 buffer solution, pure distilled water (0 ppm), and ensure the Turbidity sensor is firmly mounted inside its final protective casing with clear water.

**Execution Steps:**

1. **Wake the Device:** Press the manual trigger button (Pin 33) once to wake the system from sleep.
2. **Enter Calibration Mode:** Immediately upon seeing `HOLD 3S TO CAL` on the LCD, press and hold the button for 3 full seconds.
3. **Follow On-Screen Prompts:**
* `PH 7.0 NEUTRAL`: Submerge the pH probe in the 7.0 buffer solution. Wait 5 seconds.
* `TDS 0PPM PURE`: Submerge the TDS probe in pure distilled water. Wait 5 seconds.
* `TURB ZERO (CASE)`: Submerge the cased Turbidity sensor in clear water. Wait 5 seconds.


4. **Confirmation:** The screen will display `FLASH SAVED!`. The device can now be powered down or deployed; it will retain these settings permanently.

---

### 3. Standard Operating Procedures

The device operates in two primary modes:

* **Scheduled Monitoring (Automatic):** The DS3231 RTC hardware wakes the ESP32 at predefined intervals (e.g., 05:55, 11:55, 16:55). The system automatically powers the sensors, filters 15 data samples per parameter, sends the SMS, displays a final LCD review, and returns to sleep.
* **Manual Monitoring (On-Demand):** Press the Pin 33 button once and release it immediately (do not hold). The system will bypass the RTC schedule, perform an immediate water quality test, transmit an SMS labeled "MANUAL", and go back to sleep.

---

### 4. Troubleshooting Guide

| Issue / Symptom | Probable Cause | Corrective Action |
| --- | --- | --- |
| **System skips `HOLD 3S TO CAL` entirely** | Button circuit is floating or not reaching 0V. | Verify the 10kΩ pull-up resistor is connected to 3.3V. Ensure the button correctly connects Pin 33 to GND when pressed. |
| **Turbidity reads > 0 NTU in clean water** | Internal IR reflection from the sensor enclosure. | The enclosure is reflecting light back to the receiver. Re-run the Calibration Mode (Step 2) *while the sensor is inside the casing* to zero out the reflection. |
| **TDS reads 0 ppm in tap water** | Probe is disconnected or unpowered. | Check the BC547 transistor switching circuit. Ensure VBus is supplying power to the sensor boards. |
| **LCD displays `UNSAFE` in bottled water** | System requires a minimum of 300 ppm TDS. | Bottled water often has very low TDS (<100 ppm). To test the "SAFE" logic, dissolve a tiny pinch of salt into the water until TDS reaches ~400 ppm. |
| **Device resets during `GSM CONNECTING**` | Voltage drop / Brownout from the SIM800L module. | The SIM800L draws up to 2A during transmission bursts. Ensure your power supply (e.g., buck converter) can handle high-current spikes. |

---
