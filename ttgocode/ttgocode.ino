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
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool deviceSleeping = false;
bool sensorsValuesNotified = false;
uint32_t value = 0;
float temperature;
float charge;

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
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

void setupMonitors() {
  dht.begin();
}

void setupBleService() {
  // Create the BLE Device
  BLEDevice::init("ESP32TM Pool");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

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
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0,0); //1 = High, 0 = Low
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}

void notifySensorsValues() {
  if (deviceConnected && !sensorsValuesNotified) {
    String str = "";
    str += temperature;
    str += ",";
    str += charge;
    pCharacteristic->setValue((char*)str.c_str());
    pCharacteristic->notify();
    sensorsValuesNotified = true;
  }
}

void setup() {
  Serial.begin(115200);
  
  setupMonitors();
  readSensors();

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Wakeup caused by external signal using RTC_IO"); 
    refreshDisplay();
    sleep(5); // 5 seconds
  }

  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Wakeup caused by timer"); 
    setupBleService();
    notifySensorsValues();
    sleep(30); // 30 seconds
  }

  deepSleep();
}

void loop() {
}
