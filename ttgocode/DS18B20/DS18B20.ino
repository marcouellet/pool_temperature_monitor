#include <SPI.h>
#include <TFT_config.h>
#include "User_Setup_Select.h"
#include "User_Setup.h"
#include <TFT_eSPI.h>
#include <TFT_Drivers/ST7789_Defines.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <Wire.h>
#include <MAX17043.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#define BUTTON_PIN GPIO_NUM_0
#define BUTTON_PIN_BITMASK 0x000000001 // 2^1 in hex

#define I2C_SDA 21
#define I2C_SCL 15

// GPIO where the DS18B20 is connected to
const int oneWireBus = 2;


TFT_eSPI tft = TFT_eSPI(320, 240);
hw_timer_t * notificationTimer = NULL;
hw_timer_t * delayBeforeSleepTimer = NULL;
hw_timer_t * delayBetweenNotificationsTimer = NULL;
BLEServer* pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic* pCharacteristic = NULL;
uint8_t notificationTimerId = 0;
uint8_t delayBeforeSleepTimerId = 1;
uint8_t delayBetweenNotificationsTimerId = 2;
uint32_t Freq = 0;
const int maxbauds = 115200;
int my_bauds;
int cpufreqs[6] = {240, 160, 80, 40, 20, 10};
//int i = 0;
uint16_t prescaler = 80;                    // Between 0 and 65 535
uint32_t cpu_freq_mhz = 80;                 // Reduce to 80 mhz (default is 240mhz) - TRIED BELOW 80 BUT WIFI NOT WORKING BELOW 80 MHZ
int threshold = 1000000;                    // 64 bits value (limited to int size of 32bits)
//int lastButtonState = HIGH;                 // The previous state from the button input pin
//int currentButtonState;                     // The current reading from the button input pin
//unsigned long lastButtonPress = 0;          // Time since last pressed 
//const int debounceDelay = 50;               // Time between button pushes for it to register 
bool isPowerGaugeAvailable = false;
bool isPowerGaugeActive = false;
bool deviceConnected = false;
bool delayBetweenNotificationsTimeOut = false;
bool delayBetweenNotificationsTimerStarting = false;
bool notificationTimeOut = false;
bool deepSleepRequested = false;
bool deepSleepReady = false;
//bool buttonPressed = false;
int notificationRepeatCount = 0;
float voltage = 0;
float minVoltage = 2.7;
float minSafeVoltage = minVoltage * 1.1;
int charge = 0;
int waterTemperature;
int airTemperature;
bool disableTraceDisplay = false;

MAX17043 powerGauge(40);

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// N.B. All delays are in seconds
#define uS_TO_S_FACTOR 1000000ULL         /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60 * 5             /* Time ESP32 will stay in deep sleep before awakening (in seconds) */
#define TIME_TO_NOTIFY  30                /* Time ESP32 stay awaken to send notifications */
#define TIME_TO_WAIT_BEFORE_SLEEP  5      /* Time ESP32 stay awaken before gooing to deep sleep after notification period */
#define DELAY_BETWEEN_NOTIFICATIONS 5     /* Wait time between each notification send during notification period */
#define DELAY_TO_DISPLAY_SCREEN 5         /* Time to keep display active */
#define NOTIFICATION_REPEAT_COUNT_MAX  2  /* Max notifications to send during notification period */
#define SETUP_SENSORS_DELAY  0.5          /* Delay to read DS18B20 data before initialisation */

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

void trace(char * str) {
  if (!disableTraceDisplay) {
    Serial.print(str);
  }
}

void traceln(char * str) {
   if (!disableTraceDisplay) {
    Serial.println(str);
  } 
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      traceln("Device connected"); 
      deviceConnected = true;
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      traceln("Device disconnected"); 
      deviceConnected = false;
    }
};

int toFarenheit(int celcius) {
  double c = celcius;
  double f = c * 9.0 / 5.0 + 32.0;
  return (int) f;
}

bool isLowVoltage() {
  return voltage < minSafeVoltage;
}

void refreshDisplay() {

  tft.init();
  tft.fillScreen(TFT_DARKCYAN);

  if (isLowVoltage()) {
    tft.setTextColor(TFT_RED, TFT_DARKCYAN); 
    tft.drawString("Recharger!", tft.width()/8, 2 * tft.height() / 8, 4);
  }

  tft.setTextColor(TFT_BLACK, TFT_DARKCYAN); 

  String temperatureString = "Eau   ";
  //temperatureString += waterTemperature;
  //temperatureString += " `C / ";
  temperatureString += toFarenheit(waterTemperature);
  temperatureString += " F";
  tft.drawString(temperatureString, tft.width()/6, 3 * tft.height() / 8, 4);

  temperatureString = "Air   ";
  //temperatureString += airTemperature;
  //temperatureString += " `C / ";
  temperatureString += toFarenheit(airTemperature);
  temperatureString += " F";
  tft.drawString(temperatureString, tft.width()/6, 4 * tft.height() / 8, 4);

  String chargeString = "Charge  ";
  chargeString += charge;
  chargeString += " %";
  tft.drawString(chargeString, tft.width()/6, 5 * tft.height() / 8, 4);
}

bool isPowerGaugeAlert() {
  if (isPowerGaugeAvailable) {
    return powerGauge.isAlerting();
  }
  return false;
}

void checkPowerGaugeAvailable() {
  byte error;
  char str[50];
  traceln("Checking for MAX17043 device address ...");
  Wire.beginTransmission(MAX17043_ADDRESS);
  error = Wire.endTransmission();
    if (error == 0 || error == 5) {
      sprintf(str, "MAX17043 device found at address 0x%02X", MAX17043_ADDRESS);
      traceln(str);
      isPowerGaugeAvailable = true;
    }
    else {
      sprintf(str, "Error %d while accessing MAX17043 device at address 0x%02X", error, MAX17043_ADDRESS);
      traceln(str);
    }
}

void setupPowerGauge() {
  Wire.begin (I2C_SDA, I2C_SCL);
  pinMode (I2C_SDA, INPUT_PULLUP);
  pinMode (I2C_SCL, INPUT_PULLUP);
  checkPowerGaugeAvailable();
  if (isPowerGaugeAvailable) {
    char str[50];
    powerGauge.reset();
    powerGauge.quickStart();
    delay(100);
    sprintf(str, "MAX17043 version %d", powerGauge.getVersion());
    traceln(str);
    sprintf(str, "MAX17043 SOC %d %", powerGauge.getBatteryVoltage());
    traceln(str);
    sprintf(str, "MAX17043 voltage %d", powerGauge.getBatteryVoltage());
    traceln(str);
    // Sets the Alert Threshold to 10% of full capacity
    powerGauge.setAlertThreshold(10);
    sprintf(str, "MAX17043 Power Alert Threshold is set to %d %", powerGauge.getAlertThreshold());
    traceln(str);
    isPowerGaugeActive = true;
  }  
}

void closeDisplay() {
  tft.writecommand(TFT_DISPOFF); //turn off lcd display
}

void notificationTimeEnded() {
  notificationTimeOut = true;
  cancelNotificationTimer();
}

bool isNotificationTimerActive() {
  return notificationTimer != NULL;
}

void cancelNotificationTimer() {
  if (notificationTimer != NULL) {
      timerDetachInterrupt(notificationTimer);
      timerEnd(notificationTimer);
      notificationTimer = NULL;
  }
}

void setupNotification() {
  setupNotificationTimer(TIME_TO_NOTIFY);
  notificationRepeatCount = 0;
}

void setupNotificationTimer(int seconds) {
  cancelNotificationTimer(); // make sure no timer active
  notificationTimer = timerBegin(notificationTimerId, prescaler, true);
  // Attach onTimer function to our timer.
  timerAttachInterrupt(notificationTimer, &notificationTimeEnded, true);
  timerAlarmWrite(notificationTimer, threshold*seconds, true);
  timerAlarmEnable(notificationTimer);
  notificationTimeOut = false;
}

void delayBeforeSleepTimerEnded() {
  cancelDelayBeforeSleepTimer();
  deepSleepReady = true;
}

bool isDelayBeforeSleepTimerActive() {
  return delayBeforeSleepTimer != NULL;
}

void cancelDelayBeforeSleepTimer() {
  if (delayBeforeSleepTimer != NULL) {
      timerEnd(delayBeforeSleepTimer);
      delayBeforeSleepTimer = NULL;
  }
}

void setupDelayBeforeSleepTimer(int seconds) {
  cancelDelayBeforeSleepTimer(); // make sure no timer active
  delayBeforeSleepTimer = timerBegin(delayBeforeSleepTimerId, prescaler, true);
  timerAttachInterrupt(delayBeforeSleepTimer, &delayBeforeSleepTimerEnded, true);
  timerAlarmWrite(delayBeforeSleepTimer, threshold*seconds, true);
  timerAlarmEnable(delayBeforeSleepTimer);
  deepSleepReady = false;
}

void delayBetweenNotificationsTimerEnded() {
  if (!delayBetweenNotificationsTimerStarting) {
    cancelDelayBetweenNotificationsTimer();
    delayBetweenNotificationsTimeOut = true;
  }
}

bool isDelayBetweenNotificationsTimerActive() {
  return delayBetweenNotificationsTimer != NULL;
}

bool isDelayBetweenNotificationsTimerInactive() {
  return !delayBetweenNotificationsTimerStarting && delayBetweenNotificationsTimer == NULL;
}

void cancelDelayBetweenNotificationsTimer() {
  if (delayBetweenNotificationsTimer != NULL) {
    timerDetachInterrupt(delayBetweenNotificationsTimer);
    timerEnd(delayBetweenNotificationsTimer);
    delayBetweenNotificationsTimer = NULL;
  }
}

void setupDelayBetweenNotificationsTimer(int seconds) {
  if (!delayBetweenNotificationsTimerStarting) {
    delayBetweenNotificationsTimeOut = false;
    delayBetweenNotificationsTimerStarting = true;
    cancelDelayBetweenNotificationsTimer();
    delayBetweenNotificationsTimer = timerBegin(delayBetweenNotificationsTimerId, prescaler, true);
    timerAttachInterrupt(delayBetweenNotificationsTimer, &delayBetweenNotificationsTimerEnded, true);
    timerAlarmWrite(delayBetweenNotificationsTimer, threshold*seconds, true);
    timerAlarmEnable(delayBetweenNotificationsTimer);
    delayBetweenNotificationsTimerStarting = false;
  }
}

void displayDS18B20Info() {
  char str[50];
  sprintf(str, "DS18B20 devices count=%d", sensors.getDeviceCount());
  traceln(str);
}

void setupSensors() {
  // Start the DS18B20 sensor
  sensors.begin();

  delay(1000*SETUP_SENSORS_DELAY);
  setupPowerGauge();
}

void setupBleService() {
  // Create the BLE Device
  BLEDevice::init("ESP32TM Pool");
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL0, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL1, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL2, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL3, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL4, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL5, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL6, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL7, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL8, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  traceln("Waiting a client connection to notify...");
}

void readTemperature() {
  sensors.requestTemperatures();
  uint8_t deviceCount = sensors.getDeviceCount();

  switch (deviceCount) {
    case 0:
      waterTemperature = airTemperature = 0;
      break;
    case 1:
      waterTemperature = sensors.getTempCByIndex(0);
      break;
    case 2:
      waterTemperature = sensors.getTempCByIndex(0);
      airTemperature = sensors.getTempCByIndex(1);
      break;
  }
}

void readSensors() {
  readTemperature();
  if (isPowerGaugeActive) {
    charge = powerGauge.getBatteryPercentage();
    voltage = powerGauge.getBatteryPercentage();
  }
}

void deepSleep() {
    char str[50];
    deepSleepRequested = false;
    deepSleepReady = false;
    traceln("Prepare to deep sleep"); 

    BLEDevice::stopAdvertising();
    traceln("Stopped advertising");

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,   ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL,         ESP_PD_OPTION_OFF);
    
    //esp_sleep_enable_ext0_wakeup(GPIO_NUM_0,0); //1 = High, 0 = Low
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

    sprintf(str, "Going to deep sleep for %d seconds", TIME_TO_SLEEP);
    traceln(str);

    esp_deep_sleep_start();
}

void printSensorsValues() {
    char str[150];
    sprintf(str, "Notify values: sleep delay=%d seconds, water temperature=%d, air temperature=%d, charge=%d, alarm low voltage=%d", 
            TIME_TO_SLEEP, waterTemperature, airTemperature, charge, isLowVoltage());
    traceln(str);
}

void notifySensorsValues() {
    String str = "";
    str += TIME_TO_SLEEP;
    str += ",";
    str += waterTemperature;
    str += ",";
    str += airTemperature;
    str += ",";
    str += charge;
    str += ",";
    str += isLowVoltage() ? "1" : "0";
    pCharacteristic->setValue((char*)str.c_str());
    pCharacteristic->notify();
    printSensorsValues();
}

// void setupButton() {
//   pinMode(BUTTON_PIN, INPUT_PULLUP);
//   attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, CHANGE);
// }

// void handleButtonPress() {
//   currentButtonState = digitalRead(BUTTON_PIN);
//   if(lastButtonState == HIGH && currentButtonState == LOW  && millis() - lastButtonPress > debounceDelay) {
//     buttonPressed = true;
//   }
//   lastButtonState = currentButtonState;
// }

void displaySensorsValues() {
  refreshDisplay();
  delay(1000*DELAY_TO_DISPLAY_SCREEN); 
  closeDisplay();
}

void setup() {
  char str[50];

  setCpuFrequencyMhz(cpu_freq_mhz);

  Freq = getCpuFrequencyMhz();
  if (Freq < 80) {
    my_bauds = 80 / Freq * maxbauds;
  }
  else {
    my_bauds = maxbauds;
  }

  disableTraceDisplay = my_bauds > maxbauds;

  Serial.begin(my_bauds);        // Attention dépend de la frequence CPU si elle est <80Mhz 
  delay(500);

  traceln("Setup started");

  sprintf(str, "Baud rate = %d seconds", my_bauds);
  traceln(str);
  sprintf(str, "CPU Freq = %d Mhz", Freq);
  traceln(str);
  sprintf(str, "XTAL Freq = %d Mhz", getXtalFrequencyMhz());
  traceln(str);
  sprintf(str, "APB Freq = %d Hz",  getApbFrequency());
  traceln(str);

  setupSensors();
  readSensors();
    
  displayDS18B20Info();

  // pinMode(BUTTON_PIN, INPUT_PULLUP);

   esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

  // if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT0) {
  //   Serial.println("Wakeup caused by external signal using RTC_IO"); 
  //   refreshDisplay();
  //   printSensorsValues();
  //   delay(1000*DELAY_TO_DISPLAY_SCREEN); 
  //   deepSleep();
  // } else {
    if (wakeup_cause == ESP_SLEEP_WAKEUP_TIMER) {
      traceln("Wakeup caused by timer"); 
    }
    // setupButton();
    setupNotification();
    setupBleService(); 
//  } 
}

void loop() {

  // if (buttonPressed) {
  //   displaySensorsValues();
  //   buttonPressed = false;
  // }

  if (deviceConnected && !notificationTimeOut && notificationRepeatCount < NOTIFICATION_REPEAT_COUNT_MAX) { 
    if (delayBetweenNotificationsTimeOut) {
      notificationRepeatCount++;
      notifySensorsValues();
      traceln("Device notified");  
    }
    if (notificationRepeatCount == 0 && isDelayBetweenNotificationsTimerInactive()) {
      delayBetweenNotificationsTimeOut = true; // No delay for first notification   
    } else if (delayBetweenNotificationsTimeOut && NOTIFICATION_REPEAT_COUNT_MAX > 1) {
      setupDelayBetweenNotificationsTimer(DELAY_BETWEEN_NOTIFICATIONS);
    }
  }

  if (notificationTimeOut || notificationRepeatCount >= NOTIFICATION_REPEAT_COUNT_MAX) {

      cancelDelayBetweenNotificationsTimer();
      cancelNotificationTimer();

      if (!deepSleepRequested) {
        if (notificationTimeOut) {
          traceln("Device notification timeout");     
        } else {
          traceln("Device notification done");
        }
        setupDelayBeforeSleepTimer(TIME_TO_WAIT_BEFORE_SLEEP); 
        deepSleepRequested = true;
        traceln("Deep sleep requested");
      }
  }

  if (deepSleepReady) {
      deepSleep();
  }

  if (isPowerGaugeAlert())
  {
      traceln("Beware, Low Power!");
  }
}
