#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#define BME_SDA 0x76 //I2C address of the BME680 
#define SEALEVELPRESSURE_HPA (1022) // change according to the sealevel pressure of your region

//WiFi setup settings
const char* ssid = "";
const char* password = "";

//MQTT setup settings
const char* mqtt_server = "";
const char* mqtt_username = "";
const char* mqtt_password = "";
const short mqtt_port = 8883;
const char* mqtt_topic = "indoorAQSensor";


Adafruit_BME680 bme; // I2C
WiFiClient bmeClient;
PubSubClient client(bmeClient);

void reconnect(){

  client.setServer(mqtt_server, mqtt_port);

  while(!client.connected()){

    Serial.println("Attempting MQTT connection...");

    if(client.connect("BME680", mqtt_username, mqtt_password)){

      Serial.println("connected");
      
    }
  }   
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println(F("BME680 test"));

  if (!bme.begin(BME_SDA)) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }

  WiFi.mode(WIFI_STA); //change WiFi module to station mode
  WiFi.begin(ssid, password);

   while(WiFi.status() != WL_CONNECTED){
     delay(500);
     Serial.print(".");
   }

   Serial.println("");
   Serial.println("WiFi connected");
   Serial.println(WiFi.localIP());

   client.setServer(mqtt_server, mqtt_port);

   while(!client.connected()){
     Serial.println("Connecting...");

     if(client.connect("BME680", mqtt_username, mqtt_password)){

       Serial.println("connected");
     }else{

       Serial.println("failed");
       Serial.println(client.state());
       delay(2000);
    }
  }
  
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
}

void loop() {
  if (! bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    return;
  }

  // reconnect if sensor has lost connection with the MQTT broker
  if(!client.connected()){
     reconnect();
   }

  StaticJsonBuffer<300> JSONBuffer;
  JsonObject& JSONEncoder = JSONBuffer.createObject();
  JSONEncoder["sensorType"] = "BmeSensor";
  JsonArray& json_bme = JSONEncoder.createNestedArray("bme");

  json_bme.add(bme.temperature);
  json_bme.add(bme.pressure / 100.0);
  json_bme.add(bme.humidity);
  json_bme.add(bme.readAltitude(SEALEVELPRESSURE_HPA));

  char JSONMessageBuffer[100];
  JSONEncoder.printTo(JSONMessageBuffer, sizeof(JSONMessageBuffer));

  client.publish(mqtt_topic, JSONMessageBuffer);
  
  Serial.print("Temperature = ");
  Serial.print(bme.temperature);
  Serial.println(" *C");

  Serial.print("Pressure = ");
  Serial.print(bme.pressure / 100.0);
  Serial.println(" hPa");

  Serial.print("Humidity = ");
  Serial.print(bme.humidity);
  Serial.println(" %");

  Serial.print("Gas = ");
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(" KOhms");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.println();
  delay(2000);
}
