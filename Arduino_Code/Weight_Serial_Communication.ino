// This code is for an Arduino project that reads weight data from a load cell using the HX711 ADC and communicates with object detection python code via serial communication. This code also includes calibration and manual calibration factor adjustment features, as well as a simple LCD display for real-time weight monitoring.
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711_ADC.h>
#include <EEPROM.h>

// -------------------- Pins --------------------
#define LSB_PIN   2
#define MSB_PIN   4
#define CHECK_PIN 9

const int HX711_dout = 6; // HX711 DOUT
const int HX711_sck  = 7; // HX711 SCK

// -------------------- LCD ---------------------
LiquidCrystal_I2C lcd(0x3F, 16, 2);

// -------------------- HX711 -------------------
HX711_ADC LoadCell(HX711_dout, HX711_sck);
const int calVal_eepromAdress = 0;

// -------------------- Config ------------------
#define PERIOD_MS   1000
unsigned long lastCycle = 0;


// for printing during loop
unsigned long tPrint = 0;
const int serialPrintInterval = 0; // ms, 0 = print as fast as possible if needed

// -------------------- Forward decls -----------
void handleSerial();
void calibrate();
void changeSavedCalFactor();

void setup() {
  pinMode(LSB_PIN, OUTPUT);
  pinMode(MSB_PIN, OUTPUT);
  pinMode(CHECK_PIN, OUTPUT);
  digitalWrite(LSB_PIN, LOW);
  digitalWrite(MSB_PIN, LOW);
  digitalWrite(CHECK_PIN, LOW);

  Serial.begin(9600);
  Serial.println();
  Serial.println("Starting...");

  // LCD init
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Weight Monitor");

  // HX711 init
  LoadCell.begin();

  unsigned long stabilizingtime = 2000; // let things settle
  bool _tare = true;
  LoadCell.start(stabilizingtime, _tare);

  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  } else {
    float calValue;
    EEPROM.get(calVal_eepromAdress, calValue);
    if (isnan(calValue) || calValue == 0) {
      Serial.println("EEPROM empty or invalid. Using default calFactor = 1.0");
      calValue = 1.0f;
    }
    LoadCell.setCalFactor(calValue);
    Serial.print("Calibration factor in use: ");
    Serial.println(calValue);
    Serial.println("Startup is complete");
  }

  while (!LoadCell.update()); // make sure first data is ready

  Serial.println("Commands: 't' = tare, 'r' = calibrate, 'c' = change cal factor");
  Serial.println("Main loop running...");
}

void loop() {
  // --- HX711 update ---
  static boolean newDataReady = false;
  if (LoadCell.update()) newDataReady = true;

unsigned long now = millis();
  if (now - lastCycle >= PERIOD_MS) {
    lastCycle = now;

    if (newDataReady) {
      float fval = LoadCell.getData();   // calibrated value 
      long current = (long)round(fval);  // keep long-based logic
      newDataReady = false;

      // Print to LCD
      lcd.setCursor(0, 1);
      lcd.print("Val: ");
      lcd.print(current);
      lcd.print("      ");

      if (serialPrintInterval == 0 || (now > tPrint + serialPrintInterval)) {
        Serial.print("Reading: ");
        Serial.println(fval);
        tPrint = now;
      }
    }
  }

  // --- async tare completion message ---
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }

  // --- handle serial commands / fruit mapping ---
  handleSerial();

  delay(50);
}

// ----------------------------------------
// Handle incoming serial commands / fruit
// ----------------------------------------
void handleSerial() {
  if (Serial.available() <= 0) return;

  // Peek so we can distinguish single-char command vs. fruit string
  char peeked = Serial.peek();
  if (peeked == 't' || peeked == 'r' || peeked == 'c') {
    char inByte = Serial.read();
    if (inByte == 't') {
      LoadCell.tareNoDelay(); //tare
    } else if (inByte == 'r') {
      calibrate(); //calibrate
    } else if (inByte == 'c') {
      changeSavedCalFactor(); //edit calibration value manually
    }
    return;
  }

  // Otherwise, treat as fruit string
  String fruit = Serial.readStringUntil('\n');
  fruit.trim();
  if (fruit.length() == 0) {
    Serial.println("Empty or invalid input — ignored.");
    return;
  }

  // Reset pins before setting new state
  digitalWrite(LSB_PIN, LOW);
  digitalWrite(MSB_PIN, LOW);

  if (fruit == "orange") {
    digitalWrite(LSB_PIN, HIGH);
    digitalWrite(MSB_PIN, LOW);
    delay(3000); 
    digitalWrite(CHECK_PIN, HIGH);
    delay(2000);
    digitalWrite(CHECK_PIN, LOW);
  } else if (fruit == "apple") {
    digitalWrite(LSB_PIN, LOW);
    digitalWrite(MSB_PIN, HIGH);
    delay(3000);
    digitalWrite(CHECK_PIN, HIGH);
    delay(2000);
    digitalWrite(CHECK_PIN, LOW);
  } else if (fruit == "banana") {
    digitalWrite(LSB_PIN, HIGH);
    digitalWrite(MSB_PIN, HIGH);
    delay(3000); 
    digitalWrite(CHECK_PIN, HIGH);
    delay(2000);
    digitalWrite(CHECK_PIN, LOW);
  } else {
    digitalWrite(LSB_PIN, LOW);
    digitalWrite(MSB_PIN, LOW);
  }


  delay(3000);
  digitalWrite(LSB_PIN, LOW);
  digitalWrite(MSB_PIN, LOW);

  Serial.println("Object received: " + fruit);  // Console only
}

// -------------------- Calibration --------------------
void calibrate() {
  Serial.println("*");
  Serial.println("Start calibration:");
  Serial.println("Place the load cell on a level stable surface.");
  Serial.println("Remove any load applied to the load cell.");
  Serial.println("Send 't' from serial monitor to set the tare offset.");

  bool _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 't') LoadCell.tareNoDelay();
    }
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complete");
      _resume = true;
    }
  }

  Serial.println("Now, place your known mass on the loadcell.");
  Serial.println("Then send the weight of this mass (i.e. 100.0) from serial monitor.");

  float known_mass = 0;
  _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0) {
      known_mass = Serial.parseFloat();
      if (known_mass != 0) {
        Serial.print("Known mass is: ");
        Serial.println(known_mass);
        _resume = true;
      }
    }
  }

  LoadCell.refreshDataSet(); // ensure correct reading of known mass
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass); //get new cal value

  Serial.print("New calibration value has been set to: ");
  Serial.print(newCalibrationValue);
  Serial.println(", use this as calibration value (calFactor) in your project sketch.");
  Serial.print("Save this value to EEPROM address ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? y/n");

  _resume = false;
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        LoadCell.setCalFactor(newCalibrationValue);
        Serial.print("Value ");
        Serial.print(newCalibrationValue);
        Serial.print(" saved to EEPROM address: ");
        Serial.println(calVal_eepromAdress);
        _resume = true;

      } else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }

  Serial.println("End calibration");
  Serial.println("*");
  Serial.println("To re-calibrate, send 'r' from serial monitor.");
  Serial.println("For manual edit of the calibration value, send 'c' from serial monitor.");
  Serial.println("*");
}

// -------------------- Manual change calFactor --------------------
void changeSavedCalFactor() {
  float oldCalibrationValue = LoadCell.getCalFactor();
  bool _resume = false;
  Serial.println("*");
  Serial.print("Current value is: ");
  Serial.println(oldCalibrationValue);
  Serial.println("Now, send the new value from serial monitor, i.e. 696.0");

  float newCalibrationValue = 0.0f;
  while (_resume == false) {
    if (Serial.available() > 0) {
      newCalibrationValue = Serial.parseFloat();
      if (newCalibrationValue != 0) {
        Serial.print("New calibration value is: ");
        Serial.println(newCalibrationValue);
        LoadCell.setCalFactor(newCalibrationValue);
        _resume = true;
      }
    }
  }

  _resume = false;
  Serial.print("Save this value to EEPROM address ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? y/n");

  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        Serial.print("Value ");
        Serial.print(newCalibrationValue);
        Serial.print(" saved to EEPROM address: ");
        Serial.println(calVal_eepromAdress);
        _resume = true;
      } else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }
  Serial.println("End change calibration value");
  Serial.println("*");
}