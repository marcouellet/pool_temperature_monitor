#include <SPI.h>
#include <TFT_config.h>
#include "User_Setup_Select.h"
#include "User_Setup.h"
#include <TFT_eSPI.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <DHT.h>

#define BUTTON_PIN_BITMASK 0x000000001 // 2^1 in hex

#define DHTPIN 27 
#define DHTTYPE    DHT11

DHT dht(DHTPIN, DHTTYPE);
TFT_eSPI tft = TFT_eSPI(320, 240);
hw_timer_t * timer = NULL;
BLEServer* pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic* pCharacteristic = NULL;
uint8_t timer_id = 0;
uint16_t prescaler = 80; // Between 0 and 65 535
uint32_t cpu_freq_mhz = 80; //reduce to 80 mhz (default is 240mhz)
int threshold = 1000000; // 64 bits value (limited to int size of 32bits)
bool deviceConnected = false;
bool notificationTimeOut = false;
bool notificationDone = false;
float temperature;
float charge;

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  20        /* Time ESP32 will go to sleep (in seconds) */

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

void refreshDisplay() {
  tft.init();
  tft.fillScreen(TFT_DARKCYAN);
  tft.setTextColor(TFT_BLACK, TFT_DARKCYAN); 

  String temperatureString = "Temperature ";
  temperatureString += (int) temperature;
  temperatureString += " `C";
  tft.drawString(temperatureString, tft.width()/6, tft.height() / 3, 4);

  String chargeString = "Charge ";
  chargeString += (int) charge;
  chargeString += " %";
  tft.drawString(chargeString, tft.width()/6, tft.height() / 2, 4);
}

void notificationTimeEnded() {
  notificationTimeOut = true;
}

void setupNotificationTimer(int seconds) {
  timer = timerBegin(timer_id, prescaler, true);
  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &notificationTimeEnded, true);
  timerAlarmWrite(timer, threshold*seconds, true);
  timerAlarmEnable(timer);
  notificationDone = false;
  notificationTimeOut = false;
}

void setupSensors() {
  dht.begin();
  delay(1000);
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
  charge = dht.readHumidity();
}

void deepSleep() {
    Serial.println("Going to deep sleep"); 
    if (pService) {
      pService->stop();
      delay(500);
      pServer->removeService(pService);
      pService = NULL;
    }
    BLEDevice::stopAdvertising();
    pServer = NULL;

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0,0); //1 = High, 0 = Low
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}

void printSensorsValues() {
    Serial.print("Notify values: sleep delay=");
    Serial.print(TIME_TO_SLEEP); 
    Serial.print(" temperature=");
    Serial.print((int) temperature);
    Serial.print(" charge=");
    Serial.println((int) charge);
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

void setup() {
  setCpuFrequencyMhz(cpu_freq_mhz);
  Serial.begin(115200);
  
  setupSensors();
  readSensors();

   esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

  if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Wakeup caused by external signal using RTC_IO"); 
    refreshDisplay();
    delay(5000); // 5 seconds
    deepSleep();
  } else {
    if (wakeup_cause == ESP_SLEEP_WAKEUP_TIMER) {
      Serial.println("Wakeup caused by timer"); 
    }
    setupNotificationTimer(10); //10 seconds
    setupBleService(); 
  } 
}

void loop() {
    if (deviceConnected) {
        Serial.println("Device notified"); 
        notifySensorsValues();
        delay(500); // give the bluetooth stack the chance to get things ready
        //pServer->startAdvertising(); // restart advertising   
        delay(2000); // 5 seconds
        notificationDone = true;
    }

    if (notificationTimeOut) {
      Serial.println("Device notification timeout");
    }

    if (notificationDone || notificationTimeOut) {
        timerEnd(timer);
        deepSleep();
    }
}
