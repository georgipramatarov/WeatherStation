#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <string> 
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include "MutichannelGasSensor.h"

//use pin 5 for the temp sensor
#define DS18B20 5
//the default I2C address of the slave is 0x04
//GPIO22(SCL), GPIO21(SDA)
#define SENSOR_ADDR 0X04


float temp;  // stores the temperature as a float
char result[32];
float Ammonia;
float CarbonMonoxide;
float NitrogenDioxide;
int counter = 0;

//WiFi setup settings
const char* ssid = "";
const char* password = "";
  
//MQTT setup settings
const char* mqtt_server = "";
const char* mqtt_username = "";
const char* mqtt_password = "";
const short mqtt_port = 8883;
const char* topic_temperature = "temp";
const char* topic_gas_sensor = "gasSensor";

OneWire ourWire(DS18B20);
DallasTemperature sensor(&ourWire);
WiFiClientSecure espClient;
PubSubClient client(espClient);
TaskHandle_t Task0;
TaskHandle_t Task1;

void reconnect(){

  client.setServer(mqtt_server, mqtt_port);

  while(!client.connected()){

    Serial.println("Attempting MQTT connection...");

    if(client.connect("ESP32Client", mqtt_username, mqtt_password)){

      Serial.println("connected");

      client.publish("temp", result);
    }
  }   
}

//used for callibrating Multichannel Gas Sensor
void callibrate(){
  Serial.println("Begin to calibrate...");
  gas.doCalibrate();
  Serial.println("Calibration ok");
}

void setup(){

  Serial.begin(115200);
  Serial.println("power on! - Multichannel Gas Sensor");
  gas.begin(SENSOR_ADDR); //initializes the Multichannel Gas Sensor, with sensor address 0x04
  gas.powerOn(); //turn on the heating coil
  Serial.print("Firmware Version = ");
  Serial.println(gas.getVersion());
  delay(1000);
  Serial.println("power on! - Temperature Sensor");
  sensor.begin(); //initializes the DS18B20 temp sensor
  delay(100);

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

     if(client.connect("ESP32Client", mqtt_username, mqtt_password)){

       Serial.println("connected");
     }else{

       Serial.println("failed");
       Serial.println(client.state());
       delay(2000);
    }
  }

  //initialize both tasks
    xTaskCreatePinnedToCore(Task0code, "Task0", 10000, NULL, 1, &Task0, 0); //task0 runs on core 0, reading temp sensor
    xTaskCreatePinnedToCore(Task1code, "Task1", 10000, NULL, 1, &Task1, 1); //task1 runs on core 1, reading gas sensor
}

//first task is pinned on core 0 and is responsible for temperature sensor readings
void Task0code(void* param){
  //equivelent to loop() method
  for(;;){
   //checks if the client has disconnected, if so reconnect
   if(!client.connected()){
     reconnect();
   }
    
   client.loop();
     
   sensor.requestTemperatures(); //gets the current reading from the sensor
   temp = sensor.getTempCByIndex(0); //converts the raw reading from the sensor to degrees in centigrade
  
   Serial.print("Temperature is: ");
   Serial.print(temp); //prints temperature to the console
   Serial.print("C");
   Serial.print("\n\n");

   snprintf(result, sizeof(result), "temp/%.2f", temp); 

   client.publish(topic_temperature, result);
   Serial.println(result);
   
   delay(3000);
  }
}

//second task is pinned on core 1 and is responsible for handling the Multichannel Gas Sensor readings
void Task1code(void* param){
  for(;;){
    //runs after 17280 cycles to recallibrate the sensors R0 values
    //to-do use real time clock module for 24hour recalibration
    if(counter == 17280){
      callibrate();
      counter = 0;
    }
    
    client.loop();
    
    Ammonia = gas.measure_NH3();
    Serial.print("The concentration of NH3 is ");
    if(Ammonia>=0) Serial.print(Ammonia);
    else Serial.print("invalid");
    Serial.println(" ppm");

    CarbonMonoxide = gas.measure_CO();
    Serial.print("The concentration of CO is ");
    if(CarbonMonoxide>=0) Serial.print(CarbonMonoxide);
    else Serial.print("invalid");
    Serial.println(" ppm");

    NitrogenDioxide = gas.measure_NO2();
    Serial.print("The concentration of NO2 is ");
    if(NitrogenDioxide>=0) Serial.print(NitrogenDioxide);
    else Serial.print("invalid");
    Serial.println(" ppm");

    delay(1000);
    Serial.println("...");

    //package sensor data into Json Object
    StaticJsonBuffer<300> JSONbuffer;
    JsonObject& JSONencoder = JSONbuffer.createObject();

    JSONencoder["sensorType"] = "GasSensor";
    JsonArray& values = JSONencoder.createNestedArray("values");

    values.add((int)(Ammonia * 100 + 0.5) / 100.0);
    values.add((int)(CarbonMonoxide * 100 + 0.5) / 100.0);
    values.add((int)(NitrogenDioxide * 100 + 0.5) / 100.0);

    char JSONmessageBuffer[100];
    JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
    Serial.println("Sending message to MQTT topic..");
    Serial.println(JSONmessageBuffer);

    client.publish(topic_gas_sensor, JSONmessageBuffer);

    Serial.println("--------------------------------");
    
    delay(4000);
    counter++;
  }
}


void loop(){
    
}
