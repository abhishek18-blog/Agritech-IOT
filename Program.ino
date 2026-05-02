#define BLYNK_TEMPLATE_ID "TMPL3xICshWFk"
#define BLYNK_TEMPLATE_NAME "Smart Agriculture"
#define BLYNK_AUTH_TOKEN "p5oV-dojaWL9mde_ZxXfd-LJjunCfr3Y"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "OPPO A52"; 
char pass[] = "aaaaaaaa"; 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define DHTPIN 2 
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int soilPin = A0;
const int DRY_VALUE = 1024; 
const int WET_VALUE = 400;  
#define RELAY_PIN 14  // Pin D5

#define TRIG_PIN 12   
#define ECHO_PIN 13   
const int BOTTLE_EMPTY = 10; 
const int BOTTLE_FULL = 2;   

int selectedCrop = 0;   
int cropThreshold = 15; 
int manualThreshold = 0;
bool manualPumpOverride = false;
bool isManualMode = false;
String currentCropName = "Wheat";

BlynkTimer timer;

// Function to control physical motor
void writeToRelay(bool shouldRun) {
  if (shouldRun) {
    digitalWrite(RELAY_PIN, HIGH); // ON
  } else {
    digitalWrite(RELAY_PIN, LOW);  // OFF
  }
}

int getWaterLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = (duration * 0.034) / 2;
  return constrain(map(distance, BOTTLE_EMPTY, BOTTLE_FULL, 0, 100), 0, 100);
}

void updateSystem() {
  int rawSoil = analogRead(soilPin);
  int soilPercent = constrain(map(rawSoil, DRY_VALUE, WET_VALUE, 0, 100), 0, 100);
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  int waterPercent = getWaterLevel();

  bool pumpAction = false;
  if (isManualMode) {
    pumpAction = manualPumpOverride;
  } else {
    // AUTO LOGIC: Turn ON if Soil < Threshold AND Water >= 25%
    if (soilPercent < cropThreshold && waterPercent >= 25) {
      pumpAction = true;
    } else {
      pumpAction = false;
    }
  }

  // Final Hardware Safety: Always stop if tank is critically low
  if (waterPercent < 25) pumpAction = false;

  writeToRelay(pumpAction);

  // OLED Display Logic
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Header: Crop and Target
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print(currentCropName);
  display.setCursor(75,0);
  display.print("Trgt:"); display.print(cropThreshold); display.print("%");

  // Main Reading: Soil
  display.setTextSize(2);
  display.setCursor(0, 12);
  display.print("Soil:"); display.print(soilPercent); display.print("%");

  // Environment Data
  display.setTextSize(1);
  display.setCursor(0, 32);
  display.print("Temp:"); display.print(temp, 1); display.print("C");
  display.setCursor(70, 32);
  display.print("Hum:"); display.print(hum, 0); display.print("%");

  // Water Level & Mode
  display.setCursor(0, 45);
  display.print("Wtr:"); display.print(waterPercent); display.print("% ");
  display.print(isManualMode ? "[MANUAL]" : "[AUTO]");

  // Pump Status
  display.setCursor(0, 55);
  display.print("Status: "); display.print(pumpAction ? "WORKING" : "STOPPED");
  
  display.display();

  if (Blynk.connected()) {
    Blynk.virtualWrite(V0, soilPercent);
    Blynk.virtualWrite(V1, temp);
    Blynk.virtualWrite(V2, hum);
    Blynk.virtualWrite(V6, cropThreshold);
    Blynk.virtualWrite(V7, currentCropName);
    Blynk.virtualWrite(V8, waterPercent);
    
    if (!isManualMode) {
      Blynk.virtualWrite(V3, pumpAction);
    }
  }
}

BLYNK_WRITE(V5) {
  int index = param.asInt();
  selectedCrop = index;
  isManualMode = false; // SHIFT TO AUTO

  if (index == 0) { cropThreshold = 15; currentCropName = "Wheat"; } 
  else if (index == 1) { cropThreshold = 10; currentCropName = "Moong"; } 
  else if (index == 2) { cropThreshold = 12; currentCropName = "Brown Cowpea"; } 
  else if (index == 3) { cropThreshold = 14; currentCropName = "White Cowpea"; } 
  else if (index == 4) { cropThreshold = manualThreshold; currentCropName = "Custom"; }
  
  Blynk.virtualWrite(V6, cropThreshold);
  Blynk.virtualWrite(V7, currentCropName);
  updateSystem(); 
}

BLYNK_WRITE(V3) {
  manualPumpOverride = param.asInt();
  isManualMode = true; 
  updateSystem();
}

BLYNK_WRITE(V4) {
  manualThreshold = param.asInt();
  if (selectedCrop == 4) {
    cropThreshold = manualThreshold;
    Blynk.virtualWrite(V6, cropThreshold);
    // Trigger update immediately so motor stops/starts as you slide
    updateSystem(); 
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  writeToRelay(false); 
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  dht.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  Blynk.begin(auth, ssid, pass);
  timer.setInterval(2000L, updateSystem);
}

void loop() {
  Blynk.run();
  timer.run();
}