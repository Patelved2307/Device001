#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "model.h"

#define DHTPIN 5             // DHT11 sensor pin
#define DHTTYPE DHT11       // DHT11 type
DHT dht(DHTPIN, DHTTYPE);

#define MQ135_AO_PIN 34   // Analog pin for MQ135
#define MQ135_DO_PIN 25   // Digital pin for MQ135
#define RL 10000.0        // Load resistance in ohms (10kΩ = 10000 ohms)
#define VCC 5.0           // VCC for MQ135 (5V)
float R0 = 146646.48;     // Calibrated R0 from your data (in ohms)

LiquidCrystal_I2C lcd(0x27, 16, 2);

const char *ssid = "Patelved";
const char *password = "ved2307@";
const char *apiUrl = "http://192.168.3.68:8000/update";  // Replace with your FastAPI server IP and port

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(5000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    
    lcd.init();
    lcd.backlight();
    dht.begin();
    pinMode(MQ135_DO_PIN, INPUT);
    analogReadResolution(12); // Set ESP32 to 12-bit ADC
}

float getPPM(int sensorValue) {
    if (sensorValue == 0) sensorValue = 1; // Prevent division by zero
    float voltage = sensorValue * (3.3 / 4095.0);
    float RS = ((VCC * RL) / voltage) - RL;
    float ratio = RS / R0;
    float ppm = 400.0 * pow(ratio, -1.5); // Simplified and tuned for MQ135
    if (ppm < 400.0) ppm = 400.0;          // Scaling for 400ppm
    return ppm;
}

void loop() {
    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();
    int mq135Value = analogRead(MQ135_AO_PIN);
    float co2ppm = getPPM(mq135Value);
    float predicted_co2 = predict_co2(temp, humidity, co2ppm);
    int digitalCO2 = digitalRead(MQ135_DO_PIN);

    Serial.print("Temp: "); 
    Serial.print(temp); 
    Serial.print(" °C | ");
    Serial.print("Humidity: "); 
    Serial.print(humidity);
    Serial.print(" % | ");
    Serial.print("CO2 PPM: "); 
    Serial.print(co2ppm); 
    Serial.print(" | ");
    Serial.print("Predicted CO2: "); 
    Serial.print(predicted_co2); 
    Serial.print(" | ");
    Serial.print("DO: "); 
    Serial.println(digitalCO2 == LOW ? "High CO2!" : "Safe");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:"); 
    lcd.print(temp); 
    lcd.print("C H:"); 
    lcd.print(humidity); 
    lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print("CO2: "); 
    lcd.print(predicted_co2); 
    lcd.print(" ppm");
    
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(apiUrl);
        http.addHeader("Content-Type", "application/json");
        String payload = "{\"temperature\":" + String(temp) +
                         ",\"humidity\":" + String(humidity) +
                         ",\"co2\":" + String(predicted_co2) + "}";
        int httpResponseCode = http.POST(payload);
        if (httpResponseCode > 0) {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
        } else {
            Serial.print("Error sending data: ");
            Serial.println(httpResponseCode);
        }
        http.end();
    } else {
        Serial.println("WiFi not connected, data not sent.");
    }
    
    delay(2500); // Update every 2.5 seconds
}