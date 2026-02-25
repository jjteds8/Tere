## 📄 Project Documentation: README.md

### **Project Title:** **IoT-Based Automated Water Quality Monitoring with Deep Sleep & Low-Side Power Management**

### **1. Executive Summary**

This system is designed for high-efficiency, long-term field deployment. By leveraging the **ESP32’s Deep Sleep** capabilities and a **BC547 NPN transistor** for low-side power switching, the device minimizes power consumption and prevents sensor probe electrolysis. It provides real-time data for TDS, pH, and Turbidity via SMS reports sent through a SIM800L GSM module.

### **2. Hardware Pin Map**

| Component | ESP32 GPIO | Logic/Function |
| --- | --- | --- |
| **BC547 Base** | **25** | HIGH = Sensors ON |
| **Manual Button** | **33** | Manual Wake / Calibration Hold |
| **RTC SQW** | **32** | Scheduled Alarm Wakeup |
| **pH Probe** | **36 (VP)** | Analog Input |
| **TDS Meter** | **39 (VN)** | Analog Input |
| **Turbidity** | **34** | Analog Input |
| **GSM TX/RX** | **17 / 16** | Serial Communication |

---

## 🔄 Program Logic Flow (The Design)

### **Step 1: The Wakeup Event**

The ESP32 stays in a "comatose" state (Deep Sleep) consuming only microamps. It wakes up only when:

* The **DS3231 RTC** triggers an alarm (5:55 AM, 11:55 AM, 4:55 PM).
* A user presses the **Manual Button**.

### **Step 2: VBus Activation & Stabilization**

Immediately upon waking, the ESP32 pulls **GPIO 25 HIGH**. This saturates the **BC547 NPN transistor**, completing the circuit for the sensors to Ground.

### **Step 3: Sequential Sensing (3-Minute Window)**

To ensure maximum accuracy and zero electrical interference between the probes, the sensors are read one by one with a live LCD countdown:

1. **TDS (60s):** The sensor stabilizes and takes a reading.
2. **pH (60s):** The voltage is converted to a pH scale using calibrated offsets.
3. **Turbidity (60s):** The light-scattering value is converted to NTU.

### **Step 4: Calibration Interrupt**

During any of these countdowns, if the system detects the **Manual Button** is being held, it halts the field measurement and opens the **Calibration Menu** to allow for sensor zeroing.

### **Step 5: Data Transmission**

The **SIM800L** powers up (it remains on the 5V rail directly). The system waits 15 seconds for network registration, formats the SMS with the "Safe/Unsafe" status, and sends the report.

### **Step 6: Total Shut-off (The Pull-Down)**

After the "TASK COMPLETED" message is displayed:

1. **GPIO 25 is pulled LOW.** The BC547 disconnects the sensor Ground.
2. The sensors are now **physically off**, preventing battery drain and probe corrosion.
3. The ESP32 calculates the next alarm time and re-enters **Deep Sleep**.

---

### Final Thesis Tips:

* **The 10k Pull-down:** Don't forget that 10k resistor from GPIO 25 to GND. Without it, the transistor might "leak" electricity during sleep.
* **The SIM800L Current:** Ensure your buck converter can handle the **2A burst** when the SMS is sending, as that’s usually when systems reset if the power is weak.


Since you are deploying this in the field (likely at a pond or research site in Talisay), things can get tricky with signal interference and power drops. This guide covers the most common "fail points" for your TUPV thesis prototype.

---

## 🛠️ Hardware Troubleshooting & Pin Verification

### 1. Power & Boot Issues

| Symptom | Potential Cause | Fix |
| --- | --- | --- |
| **LCD is blank or dim** | I2C Address or Contrast | Check if address is `0x27` or `0x3F`. Adjust the blue potentiometer on the back of the LCD. |
| **ESP32 resets when SMS sends** | Voltage Drop | SIM800L draws **2A** bursts. Add a **1000µF capacitor** across the SIM800L VCC and GND. |
| **Sensors won't turn ON** | BC547 Wiring | Ensure the **Collector** is to Sensor GND and **Emitter** is to Battery GND. Check if the 1kΩ base resistor is connected to **GPIO 25**. |

### 2. Signal & SMS Issues

| Symptom | Potential Cause | Fix |
| --- | --- | --- |
| **GSM stuck on "Connecting"** | Poor Signal / Power | Ensure antenna is connected. Check if SIM has load/promo. SIM800L LED should blink **once every 3 seconds** when registered. |
| **SMS not received** | Number Format | Ensure `phoneNum` in code starts with `+63`. |

---

## 📍 Final Pin Connectivity Map (Checklist)

Before soldering or final housing, use a multimeter to verify these connections:

### **Power Rail**

* [ ] **Buck Converter OUT (+)** → ESP32 **VIN**, LCD **VCC**, RTC **VCC**, SIM800L **VCC**, and all Sensor **VCC**.
* [ ] **Buck Converter OUT (-)** → ESP32 **GND**, LCD **GND**, RTC **GND**, SIM800L **GND**, and BC547 **Emitter**.

### **The "Switch" (BC547 NPN)**

* [ ] **Base (Middle Pin)** → 1kΩ Resistor → **GPIO 25**.
* [ ] **Collector (Pin 1)** → All Sensor **GND** pins.
* [ ] **Emitter (Pin 3)** → Main **GND**.
* [ ] **Pull-down** → 10kΩ Resistor between **GPIO 25 and GND** (To keep it shut off during sleep).

### **Communication & Sensors**

* [ ] **I2C Bus:** SDA → **GPIO 21**, SCL → **GPIO 22** (Shared by LCD and RTC).
* [ ] **GSM Serial:** SIM TX → **GPIO 16 (RX2)**, SIM RX → **GPIO 17 (TX2)**.
* [ ] **pH Probe:** Signal → **GPIO 36**.
* [ ] **TDS Meter:** Signal → **GPIO 39**.
* [ ] **Turbidity:** Signal → **GPIO 34**.

---

## 📋 Software Debugging Logic

If the system isn't behaving, check these three states:

### **The "Always-ON" Test**

If the BC547 isn't switching, temporarily bypass it by connecting Sensor GND directly to Main GND. If the sensors start working, your transistor wiring or GPIO 25 logic is the issue.

### **The "Serial Monitor" Check**

Open the Arduino Serial Monitor at **115200 baud**.

* Look for "PNP ON" or "VBus Active" messages.
* If you see "Brownout detector was triggered," your power supply/battery cannot handle the GSM module's current.

### **Deep Sleep Verification**

* If the ESP32 wakes up immediately after sleeping, check **GPIO 32 (SQW)**. It must have a **10kΩ Pull-up resistor** to 3.3V. If it's "floating," it will trigger random wakeups.

---

### 🎓 Justen’s Final Pro-Tip:

When you present this to your panel, bring a **Power Bank** or a **Li-ion battery pack** (like 18650 cells) with a high discharge rate. Typical 9V alkaline batteries will die almost instantly when the SIM800L tries to transmit.
