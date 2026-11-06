
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// =====================================
// WIFI CONFIGURATION
// =====================================
const char* ssid = "SmartBro_F0D8A";
const char* password = "Xmystogan.0";

// =====================================
// LARAVEL API URLS
// =====================================
const char* heartbeatURL =
  "http://192.168.1.242:8000/api/device/heartbeat";

const char* liveReadingsURL =
  "http://192.168.1.242:8000/api/device/live-readings";

const char* readingsURL =
  "http://192.168.1.242:8000/api/device/readings";

// =====================================
// SENSOR CONFIGURATION
// =====================================
#define DHTPIN 15
#define DHTTYPE DHT22
#define SOIL_PIN 34

DHT dht(DHTPIN, DHTTYPE);

// =====================================
// TIME INTERVALS
// =====================================
const unsigned long HEARTBEAT_INTERVAL = 5000UL;

const unsigned long SENSOR_READ_INTERVAL = 3000UL;

const unsigned long LIVE_UPLOAD_INTERVAL = 3000UL;

const unsigned long DATABASE_INTERVAL = 900000UL;

const unsigned long RETRY_INTERVAL = 10000UL;

// =====================================
// TIMERS
// =====================================
unsigned long lastHeartbeat = 0;
unsigned long lastSensorRead = 0;
unsigned long lastLiveUpload = 0;

unsigned long lastSuccessfulUpload = 0;
unsigned long lastUploadAttempt = 0;

// =====================================
// SENSOR VALUES
// =====================================
float temperature = 0;
float humidity = 0;
float soilMoisture = 0;

int soilRaw = 0;

bool hasValidReading = false;
bool hasUploadedReading = false;

// =====================================
// SOIL MOISTURE CALIBRATION
// 4095 = 0% (Very dry)
// 1000 = 100% (Very moist)
// =====================================
float calculateSoilMoisture(int rawValue) {

  float moisture =
    (4095.0 - rawValue) * 100.0 / (4095.0 - 1000.0);

  moisture = constrain(moisture, 0.0, 100.0);

  return moisture;
}

// =====================================
// SEND HEARTBEAT TO LARAVEL
// =====================================
void sendHeartbeat() {

  HTTPClient http;

  http.begin(heartbeatURL);
  http.setTimeout(5000);

  http.addHeader("Accept", "application/json");

  int responseCode = http.POST("");

  Serial.print("Heartbeat HTTP: ");
  Serial.println(responseCode);

  if (responseCode > 0) {
    Serial.println(http.getString());
  } else {
    Serial.println(http.errorToString(responseCode));
  }

  http.end();
}

// =====================================
// CREATE SENSOR JSON
// =====================================
String createSensorJSON() {

  String jsonData = "{";

  jsonData += "\"temperature\":" + String(temperature, 2) + ",";
  jsonData += "\"humidity\":" + String(humidity, 2) + ",";
  jsonData += "\"soil_moisture\":" + String(soilMoisture, 2);

  jsonData += "}";

  return jsonData;
}

// =====================================
// SEND LIVE SENSOR READINGS
// Updates temporary Laravel cache
// =====================================
void sendLiveReading() {

  HTTPClient http;

  http.begin(liveReadingsURL);
  http.setTimeout(5000);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  String jsonData = createSensorJSON();

  Serial.println();
  Serial.println("Sending LIVE sensor readings...");
  Serial.println(jsonData);

  int responseCode = http.POST(jsonData);

  Serial.print("Live Upload HTTP: ");
  Serial.println(responseCode);

  if (responseCode > 0) {
    Serial.println(http.getString());
  } else {
    Serial.println(http.errorToString(responseCode));
  }

  http.end();
}

// =====================================
// SAVE SENSOR READINGS TO DATABASE
// =====================================
bool sendSensorReading() {

  HTTPClient http;

  http.begin(readingsURL);
  http.setTimeout(5000);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  String jsonData = createSensorJSON();

  Serial.println();
  Serial.println("Saving readings to DATABASE...");
  Serial.println(jsonData);

  int responseCode = http.POST(jsonData);

  Serial.print("Database Upload HTTP: ");
  Serial.println(responseCode);

  if (responseCode > 0) {
    Serial.println(http.getString());
  } else {
    Serial.println(http.errorToString(responseCode));
  }

  http.end();

  return responseCode == 201 || responseCode == 200;
}

// =====================================
// READ ALL SENSORS
// =====================================
void readSensors() {

  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  soilRaw = analogRead(SOIL_PIN);

  Serial.println();
  Serial.println("===== AGRISENSE LIVE SENSOR DATA =====");

  bool dhtValid =
    !isnan(humidity) && !isnan(temperature);

  bool soilValid = soilRaw > 10;

  if (!dhtValid) {

    Serial.println("DHT22 reading failed!");

  } else {

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
  }

  Serial.print("Soil Raw ADC: ");
  Serial.println(soilRaw);

  if (!soilValid) {

    Serial.println("Invalid soil reading! Check wiring.");

  } else {

    soilMoisture = calculateSoilMoisture(soilRaw);

    Serial.print("Soil Moisture: ");
    Serial.print(soilMoisture, 2);
    Serial.println(" %");

    if (soilMoisture <= 20) {
      Serial.println("Soil Status: Very dry");
    }
    else if (soilMoisture <= 40) {
      Serial.println("Soil Status: Dry");
    }
    else if (soilMoisture <= 60) {
      Serial.println("Soil Status: Moderate");
    }
    else if (soilMoisture <= 80) {
      Serial.println("Soil Status: Moist");
    }
    else {
      Serial.println("Soil Status: Very moist");
    }
  }

  hasValidReading = dhtValid && soilValid;

  if (hasValidReading) {
    Serial.println("Sensor readings are valid.");
  } else {
    Serial.println("Some sensor readings are invalid.");
  }

  Serial.println("======================================");
}

// =====================================
// SETUP
// =====================================
void setup() {

  Serial.begin(115200);

  dht.begin();

  analogReadResolution(12);
  pinMode(SOIL_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  WiFi.begin(ssid, password);

  Serial.println();
  Serial.println("=====================================");
  Serial.println("       AGRISENSE ESP32 SYSTEM");
  Serial.println("=====================================");

  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("System initialized successfully.");
}

// =====================================
// MAIN LOOP
// =====================================
void loop() {

  unsigned long currentTime = millis();

  // ===================================
  // CHECK WIFI CONNECTION
  // ===================================
  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("Wi-Fi disconnected!");

    WiFi.reconnect();

    delay(1000);

    return;
  }

  // ===================================
  // HEARTBEAT EVERY 5 SECONDS
  // ===================================
  if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {

    lastHeartbeat = currentTime;

    sendHeartbeat();
  }

  // ===================================
  // READ SENSORS EVERY 3 SECONDS
  // ===================================
  if (currentTime - lastSensorRead >= SENSOR_READ_INTERVAL) {

    lastSensorRead = currentTime;

    readSensors();
  }

  // ===================================
  // SEND LIVE READINGS EVERY 3 SECONDS
  // ===================================
  if (
    hasValidReading &&
    currentTime - lastLiveUpload >= LIVE_UPLOAD_INTERVAL
  ) {

    lastLiveUpload = currentTime;

    sendLiveReading();
  }

  // ===================================
  // SAVE TO DATABASE EVERY 15 MINUTES
  // ===================================
  bool databaseDue =
    !hasUploadedReading ||
    (currentTime - lastSuccessfulUpload >= DATABASE_INTERVAL);

  bool retryDue =
    lastUploadAttempt == 0 ||
    (currentTime - lastUploadAttempt >= RETRY_INTERVAL);

  if (hasValidReading && databaseDue && retryDue) {

    lastUploadAttempt = currentTime;

    bool saved = sendSensorReading();

    if (saved) {

      hasUploadedReading = true;
      lastSuccessfulUpload = millis();

      Serial.println();
      Serial.println("Database record saved successfully!");
      Serial.println("Next database upload in 15 minutes.");

    } else {

      Serial.println("Database upload failed!");
      Serial.println("Retrying in 10 seconds.");
    }
  }

  delay(10);
}
