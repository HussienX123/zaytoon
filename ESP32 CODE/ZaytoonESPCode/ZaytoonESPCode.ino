#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <DHT.h>
#include <FastLED.h>

FASTLED_USING_NAMESPACE

// =========================================
// BLUETOOTH BLE CODE START
// =========================================

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
int aleatorio;
String alea = "2";
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// =========================================
// BLUETOOTH BLE CODE END
// =========================================


//===================
// MULTI TASKING CODE
//===================
TaskHandle_t myTask;
bool pump_startTask = false;
bool led_startTask = false;
int ledTimer = 0;

// Define sensor pins
#define LDR_RIGHT_PIN 18 // Replace with the appropriate GPIO number for A1
#define LDR_LEFT_PIN 22 // Replace with the appropriate GPIO number for A0
#define WATER_LEVEL_SENSOR_PIN 39
#define SOIL_MOISTURE_SENSOR_PIN 36
#define RELAY_PUMP_PIN 17
#define DHT_SENSOR_PIN  21 // ESP32 pin GPIO21 connected to DHT22 sensor
#define DHT_SENSOR_TYPE DHT22

DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

// LED 
#define DATA_PIN    23
//#define CLK_PIN   4
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS    16
CRGB leds[NUM_LEDS];
// Define threshold values
int LIGHT_THRESHOLD = 1;  // Adjust as needed
int MOISTURE_THRESHOLD = 500;  // Adjust as needed
int COLD_THRESHOLD = 20;  // Adjust as needed
int HOT_THRESHOLD = 30;  // Adjust as needed

String BloData = "";



bool isDrinking = false;
bool isDark = false;
bool isTankEmpty = false;
bool isHot = false;
bool isCold = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};



void checkLight() {
    int ldrLeftValue = digitalRead(LDR_LEFT_PIN);
    int ldrRightValue = digitalRead(LDR_RIGHT_PIN);

    if (ldrLeftValue == HIGH && ldrRightValue == HIGH) {
        isDark = true;
        // Both LDRs don't have light, activate LED for 15 min every 1 hours
        LightON();
    } else {
        // At least one LDR is detecting light, do nothing
        isDark = false;
        Serial.println("Its Sunny!!");
        LightOFF();
    }
}

void checkWaterLevel() {
    int waterLevelValue = analogRead(WATER_LEVEL_SENSOR_PIN);


    if (waterLevelValue < 1200) {
        // Water level is low, send message to clients
        isTankEmpty = true;
        Serial.println("\n fill container please");
        Serial.print(waterLevelValue);
    } else {
      isTankEmpty = false;
    }
}

int PumpTimerValue = 10; //seconds
int PumpTimerCurrent = PumpTimerValue; //seconds

void checkSoilMoisture() {
    if(PumpTimerCurrent == 0) {
      digitalWrite(RELAY_PUMP_PIN, LOW);
      int soilMoistureValue = analogRead(SOIL_MOISTURE_SENSOR_PIN);
              Serial.println(soilMoistureValue);


      if(!isTankEmpty){
        if (soilMoistureValue > 2600 && soilMoistureValue < 4000) {
            // Soil moisture is low, activate water pump
            PumpTimerCurrent = PumpTimerValue;
            digitalWrite(RELAY_PUMP_PIN, HIGH); //😂 خلي النبات يشرب
            Serial.println("starting water pump...");
            isDrinking = true;
            // Send message to clients
        } else {
              digitalWrite(RELAY_PUMP_PIN, LOW); 
              isDrinking = false;
        }
      }

    }
}

float humi;
float temperature;

void checkTemperature() {
    // Assuming you have the DHT library properly included
     humi  = dht_sensor.readHumidity();
     temperature = dht_sensor.readTemperature();
    Serial.print("Humidity: ");
    Serial.print(humi);
    Serial.print("%");

    Serial.print("  |  ");

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print("°C  ~  ");

    isCold = false;
    isHot = false;

    if (temperature < COLD_THRESHOLD) {
        // Temperature is below 20°C, send message to clients

        isCold = true;
    } else if (temperature > HOT_THRESHOLD) {
        // Temperature is above 30°C, send message to clients
        isHot = true;
    }

}

void LightON() {
  fill_solid(leds, NUM_LEDS, CRGB::DarkViolet);
  FastLED.show();
}

void LightOFF() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}



void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("ZaytoonPOT");

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

  // https://www.bluetooth.com/specifications/gatt/viewer?
  // attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
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

      // Initialize other sensors and pins
    pinMode(LDR_LEFT_PIN, INPUT);
    pinMode(LDR_RIGHT_PIN, INPUT);
    pinMode(WATER_LEVEL_SENSOR_PIN, INPUT);
    pinMode(SOIL_MOISTURE_SENSOR_PIN, INPUT);
    pinMode(DHT_SENSOR_PIN, INPUT);
    pinMode(RELAY_PUMP_PIN, OUTPUT);
    FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    dht_sensor.begin();
    // Create a new task for checking sensors on Core 1
    xTaskCreatePinnedToCore(
      myTaskFunction,
      "MyTask",
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &myTask,  /* Task handle. */
      0); /* Core where the task should run */
}


void loop() {
    // notify changed value
    if (deviceConnected) {
       // pCharacteristic->setValue((uint8_t*)&value, 4);

        checkWaterLevel();
        checkLight();
        checkSoilMoisture();
        checkTemperature();
        if(PumpTimerCurrent > 0) {
          PumpTimerCurrent -= 1;
        } 

        if(isDark)
          BloData = "Dark"; 
        else if(isDrinking)
          BloData = "DRINK";
        else if(isTankEmpty)
          BloData = "NWIT";
        else if(isCold)
          BloData = "COLD"; 
        else if(isHot)
          BloData = "HOT";
        else
         BloData = "idle";

        BloData = " " + BloData + " " + humi + " " + temperature + " "; 

        Serial.println(BloData);
        pCharacteristic->setValue(BloData.c_str()); //set "Dark" to BLE server
        pCharacteristic->notify(); //send to phone app
        delay(1000);
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        digitalWrite(RELAY_PUMP_PIN, LOW);
        LightOFF();
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}


void myTaskFunction(void *pvParameters) {
  while (1) {
    //disable the function when the setup start
    if (!pump_startTask && !led_startTask){
      vTaskSuspend(myTask);
    }
    
    if(pump_startTask){
      //end the pump use after 10 seconds
      if (xTaskGetTickCount() >= pdMS_TO_TICKS(10000)){
        digitalWrite(RELAY_PUMP_PIN, LOW); //disable the relay water pump
        pump_startTask = false; //change boolen so checking can start again
      }

      // Your task code goes here
      digitalWrite(RELAY_PUMP_PIN, HIGH); //😂 خلي النبات يشرب
    } 
    
    if(led_startTask) {
      //end the led after 15 min
      if (xTaskGetTickCount() >= pdMS_TO_TICKS(900000)){
        ledTimer = 216000; //reset 1 hour timer before led light again
        LightOFF();
        led_startTask = false; //change boolen so checking can start again
      }

      // Your task code goes here
      LightON(); //🫠 خلي النبات يتشمس
    }

    delay(1000);
  }

    // Cleanup and delete the task when it's done
  vTaskDelete(myTask);
}


