#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FirebaseESP32.h>

// Network credentials
const char* ssid = "esp";
const char* password = "12345678";
const char* serverUrl = "https://abdulrahman0700-flaskapi.hf.space/verify";  // URL of the JSON file from "adallrahaman"

// Firebase project details
#define FIREBASE_HOST "https://esp32-e9004-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "N0y1Y61rFSqI5cGlfYRM1SjP0dHSohU4Ka7EL1Xc"

// Define (output) pins
const int irSensorPin = 34; // IR sensor pin
const int servoPin = 23;    // Servo motor pin

// Create Servo objects
Servo outputServo;  // Servo object for output
Servo inputServo;    // Servo object for input

// Firebase and WiFi objects
FirebaseData firebaseData;
FirebaseJson json;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// IR sensor pins
const int IRsensorPins[] = {13, 12, 14, 27}; 
int IRdata[4] = {0};

void setup() {
  Serial.begin(115200);

  // Set IR sensor pins as input
  for (int i = 0; i < 4; i++) {
    pinMode(IRsensorPins[i], INPUT);
  }

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");

  inputServo.attach(18);  // Attach the servo to pin 18

  // Set Firebase configuration
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;

  // Initialize Firebase
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);
  Serial.println("Connected to Firebase...");

  // Set output IR sensor pin as input
  pinMode(irSensorPin, INPUT);

  // Attach the servo to the pin and set to neutral position
  outputServo.attach(servoPin);     
  outputServo.write(90);            
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println(payload);

      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }

      int verification_value = doc["verification_value"];  // Adjust the key according to your JSON structure

      if (verification_value == 1) {
        inputServo.write(90);  // Move the servo to 90 degrees
      } else {
        inputServo.write(0);  // Move the servo to 0 degrees
      }
    } else {
      Serial.print("Error on HTTP request: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }

  delay(10000);  // Wait for 10 seconds before reading the JSON again

  // Read IR sensor data
  for (int i = 0; i < 4; i++) {
    IRdata[i] = digitalRead(IRsensorPins[i]); 
    Serial.print("IR Sensor ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(IRdata[i]);
  }
  delay(100); 
  
  // Find all empty sensors
  int emptySensors[4];
  int emptyCount = 0;
  for (int i = 0; i < 4; i++) {
    if (IRdata[i] == 1) {
      emptySensors[emptyCount] = i + 1; // Sensor numbers are 1, 2, 3, 4
      emptyCount++;
    }
  }

  // Determine the next sensor in priority
  int nextSensor = (emptyCount > 0) ? emptySensors[0] : -1;

  // Update Firebase with IR sensor data, empty sensors info, and next sensor
  json.clear();
  json.set("emptyCount", emptyCount);
  for (int i = 0; i < emptyCount; i++) {
    json.set(String("emptySensor") + String(i + 1), emptySensors[i]);
  }
  for (int i = 0; i < 4; i++) {
    json.set(String("IRsensor") + String(i + 1), IRdata[i]);
  }
  json.set("nextSensor", nextSensor);

  if (Firebase.updateNode(firebaseData, "/SensorData", json)) {
    Serial.println("Data updated successfully.");
  } else {
    Serial.print("Failed to update data. Error: ");
    Serial.println(firebaseData.errorReason());
  }

  delay(1000); // Delay to avoid spamming the server

  int irValue = digitalRead(irSensorPin);  // Read the IR sensor value

  if (irValue == HIGH) {
    // If IR sensor detects a signal
    outputServo.write(0);  // Rotate the servo to 0 degrees
    Serial.println("IR signal detected: Servo at 0 degrees");
  } else {
    // If no signal from IR sensor
    outputServo.write(90); // Move the servo back to neutral position
    Serial.println("No IR signal: Servo at 90 degrees");
  }

  delay(50); // Wait for 50ms a second before repeating
}
