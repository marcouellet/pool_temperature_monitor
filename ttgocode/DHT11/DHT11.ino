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
#include <LiFuelGauge.h>

#include <DHT.h>

#define BUTTON_PIN GPIO_NUM_0
#define BUTTON_PIN_BITMASK 0x000000001 // 2^1 in hex

#define DHTPIN 27 
#define DHTTYPE    DHT11

DHT dht(DHTPIN, DHTTYPE);
LiFuelGauge* powerGauge;
TFT_eSPI tft = TFT_eSPI(320, 240);
hw_timer_t * notificationTimer = NULL;
hw_timer_t * delayBeforeSleepTimer = NULL;
hw_timer_t * delayBetweenNotificationsTimer = NULL;
BLEServer* pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic* pCharacteristic = NULL;
uint8_t notificationTimerId = 0;
uint8_t delayBetweenNotificationsTimerId = 2;
uint8_t delayBeforeSleepTimerId = 1;
uint16_t prescaler = 80;                    // Between 0 and 65 535
uint32_t cpu_freq_mhz = 80;                 // Reduce to 80 mhz (default is 240mhz)
int threshold = 1000000;                    // 64 bits value (limited to int size of 32bits)
int lastButtonState = HIGH;                 // The previous state from the button input pin
int currentButtonState;                     // The current reading from the button input pin
unsigned long lastButtonPress = 0;          // Time since last pressed 
const int debounceDelay = 50;               // Time between button pushes for it to register 
bool isPowerGaugeAvailable = false;
bool isPowerGaugeActive = false;
bool alertLowPower = false;
bool deviceConnected = false;
bool delayBetweenNotificationsTimeOut = false;
bool delayBetweenNotificationsTimerStarting = false;
bool notificationTimeOut = false;
bool deepSleepRequested = false;
bool deepSleepReady = false;
bool buttonPressed = false;
int notificationRepeatCount = 0;
int charge = 0;
int temperature;

// N.B. All delays are in seconds

#define uS_TO_S_FACTOR 1000000ULL         /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60                 /* Time ESP32 will stay in deep sleep before awakening (in seconds) */
#define TIME_TO_NOTIFY  30                /* Time ESP32 stay awaken to send notifications */
#define TIME_TO_WAIT_BEFORE_SLEEP      5  /* Time ESP32 stay awaken before gooing to deep sleep after notification period */
#define DELAY_BETWEEN_NOTIFICATIONS    5  /* Wait time between each notification send during notification period */
#define DELAY_TO_DISPLAY_SCREEN        5  /* Time to keep display active */
#define DELAY_TO_LOOP                  1  /* Time to wait before executing next loop - to save power consumption */
#define NOTIFICATION_REPEAT_COUNT_MAX 2   /* Max notifications to send during notification period */
#define SETUP_SENSORS_DELAY  0.5          /* Delay to make sure the sensors get time to initialize */

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("Device connected"); 
      deviceConnected = true;
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("Device disconnected"); 
      deviceConnected = false;
    }
};

void lowPower() { alertLowPower = true; }

void refreshDisplay() {
  tft.init();
  tft.fillScreen(TFT_DARKCYAN);
  tft.setTextColor(TFT_BLACK, TFT_DARKCYAN); 

  String temperatureString = "Temperature ";
  temperatureString += temperature;
  temperatureString += " `C";
  tft.drawString(temperatureString, tft.width()/6, tft.height() / 3, 4);

  String chargeString = "Charge ";
  chargeString += charge;
  chargeString += " %";
  tft.drawString(chargeString, tft.width()/6, tft.height() / 2, 4);
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

void setupPowerGauge() {
  if (isPowerGaugeAvailable) {
    powerGauge = new LiFuelGauge(MAX17043, 0, lowPower);
    powerGauge->reset();  // Resets MAX17043
    delay(200);  // Waits for the initial measurements to be made

    // Sets the Alert Threshold to 10% of full capacity
    powerGauge->setAlertThreshold(10);
    Serial.println(String("Power Alert Threshold is set to ") + 
                   powerGauge->getAlertThreshold() + '%');
    isPowerGaugeActive = true;
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
    timerDetachInterrupt(delayBeforeSleepTimer);
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

void setupSensors() {
  dht.begin();
  delay(1000*SETUP_SENSORS_DELAY);
  setupPowerGauge();
}

void setupBleService() {
  // Create the BLE Device
  BLEDevice::init("ESP32TM Pool");

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
  Serial.println("Waiting a client connection to notify...");
}

void readSensors() {
  temperature = dht.readTemperature();
  if (isPowerGaugeActive) {
    charge = powerGauge->getSOC();
  }
}

void deepSleep() {
    deepSleepRequested = false;
    deepSleepReady = false;
    Serial.println("Prepare to deep sleep"); 

    BLEDevice::stopAdvertising();
    Serial.println("Stopped advertising");

    esp_sleep_enable_ext0_wakeup(BUTTON_PIN,0); //1 = High, 0 = Low
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Going to deep sleep");
    esp_deep_sleep_start();
}

void printSensorsValues() {
    Serial.print("Notify values: sleep delay=");
    Serial.print(TIME_TO_SLEEP); 
    Serial.print(" temperature=");
    Serial.print(temperature);
    Serial.print(" charge=");
    Serial.println(charge);
}

void notifySensorsValues() {
    String str = "";
    str += TIME_TO_SLEEP;
    str += ",";
    str += temperature;
    str += ",";
    str += charge;
    pCharacteristic->setValue((char*)str.c_str());
    pCharacteristic->notify();
    printSensorsValues();
}

void setupButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, CHANGE);
}

void handleButtonPress() {
  currentButtonState = digitalRead(BUTTON_PIN);
  if(lastButtonState == HIGH && currentButtonState == LOW  && millis() - lastButtonPress > debounceDelay) {
    buttonPressed = true;
  }
  lastButtonState = currentButtonState;
}

void displaySensorsValues() {
  refreshDisplay();
  delay(1000*DELAY_TO_DISPLAY_SCREEN); 
  closeDisplay();
}

void setup() {
  setCpuFrequencyMhz(cpu_freq_mhz);
  Serial.begin(115200);
  
  setupSensors();
  readSensors();

  esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

  if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Wakeup caused by external signal using RTC_IO"); 
    refreshDisplay();
    delay(1000*DELAY_TO_DISPLAY_SCREEN); 
    deepSleep();
  } else {
    if (wakeup_cause == ESP_SLEEP_WAKEUP_TIMER) {
      Serial.println("Wakeup caused by timer"); 
    }
    setupNotification();
    setupBleService(); 
    setupButton();
  } 
}

void loop() {

  delay(1000*DELAY_TO_LOOP); // Slow down the time spent in loop - to save power consumption

  if (buttonPressed) {
    displaySensorsValues();
    buttonPressed = false;
  }

  if (deviceConnected && !notificationTimeOut && notificationRepeatCount < NOTIFICATION_REPEAT_COUNT_MAX) { 
    if (delayBetweenNotificationsTimeOut) {
      notificationRepeatCount++;
      notifySensorsValues();
      Serial.println("Device notified");  
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
          Serial.println("Device notification timeout");     
        } else {
          Serial.println("Device notification done");
        }

        setupDelayBeforeSleepTimer(TIME_TO_WAIT_BEFORE_SLEEP); 
        deepSleepRequested = true;
        Serial.println("Deep sleep requested");
      }
  }

  if (deepSleepReady) {
      deepSleep();
  }

  if (alertLowPower)
  {
      Serial.println("Beware, Low Power!");
      powerGauge->clearAlertInterrupt();  // Resets the ALRT pin
      alertLowPower = false;
  }
}
