#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
// #include <FirebaseESP32.h>

#define ARDUINOJSON_USE_LONG_LONG 1
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#define CONFIG_STA "/sta.json"
#define CONFIG_DB "/db.json"
#define CONFIG_MQTT "/mqtt.json"
#define BOOT_BTN 0
#define LED_BUILTIN 2
// #define FORCE_CONFIG false


char ESP_NAME[20] = "";
char DEVICE_ID[11] = "";
unsigned long MyTestTimer = 0;  // variables MUST be of type unsigned long
const char *prefix_ssid_ap = "ESP32_AP";

// Reset Device constants
unsigned long buttonPressTime = 0;
bool buttonPressed = false;
const unsigned long longPressDuration = 3000;  // Long press duration in milliseconds (e.g., 3 seconds)

const int maxWifiConnectAttempts = 10;  // Number of retries before giving up
const int retryWifiConnectDelay = 500;  // Delay between retries in milliseconds

// FireBase Config
String userUID;
String authToken;
String authError;
String firebaseHost;
// const char *API_KEY = "";
// const char *DATABASE_URL = "";
// const char *USER_EMAIL = "";
// const char *USER_PASSWORD = "";

// Define Firebase Data object
// FirebaseData fbData;

// // Authentication credentials
// FirebaseAuth auth;
// FirebaseConfig config;


AsyncWebServer httpServer(80);
HTTPClient http;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  Serial.begin(115200);
  // delay(100);
  if (!SPIFFS.begin(true)) {
    Serial.println(F("An error occurred while mounting SPIFFS"));
    return;
  }
  Serial.println(F("SPIFFS mounted successfully"));
  delay(200);

  if (!SPIFFS.exists(CONFIG_STA)) {
    WiFi.mode(WIFI_AP);
    setDeviceID();

    if (strlen(ESP_NAME) == 0) {
      char esp_ap_name[20];
      snprintf(esp_ap_name, sizeof esp_ap_name, "%s_%S", prefix_ssid_ap, DEVICE_ID);
      strcpy(ESP_NAME, esp_ap_name);
    }
    Serial.println(F(ESP_NAME));
    WiFi.softAP(ESP_NAME);
    Serial.println(F("Entering Configuration mode..."));
    Serial.print(F("AP IP address: "));
    Serial.println(WiFi.softAPIP());
    Serial.println(F("Configuration mode enabled...!!"));
  } else {

    if (connectToWifi()) {
      if (SPIFFS.exists(CONFIG_DB)) {
        Serial.println(F("db config found..."));
        // connectToFireBase();
      }
    } else {
      Serial.println(F("Unable to Connect to Wifi..."));
    }
  }
  startHTTPServer();
  httpServer.begin();
}

//Loop-Methode: Ticker
void loop() {
  int buttonState = digitalRead(BOOT_BTN);

  // Check if the button is pressed
  if (buttonState == LOW && !buttonPressed) {
    // Record the time when the button is pressed
    buttonPressTime = millis();
    buttonPressed = true;
    // Serial.print(F("."));
    // delay(1000);
  }

  // Check if the button is released
  if (buttonState == HIGH && buttonPressed) {
    // Calculate the press duration
    unsigned long pressDuration = millis() - buttonPressTime;

    // Check if the press duration was a long press
    if (pressDuration >= longPressDuration) {
      Serial.print(F(".tr"));
      resetDevice();
    }

    buttonPressed = false;  // Reset the button pressed state
  }

  delay(50);  // Small delay for debounce
  if (WiFi.getMode() == 0) {
    fastBlinkLED(100);
  }
  if (WiFi.getMode() == 1 && WiFi.status() == 3) {
    fastBlinkLED(50);
  }
  if (WiFi.getMode() == 2) {
    pulseBlinkLED(20);
  }
}

void setDeviceID() {
  uint64_t chipid;
  char devId[11];
  chipid = ESP.getEfuseMac();
  //The chip ID is essentially its MAC address(length: 6 bytes).
  // Serial.printf("ESP32 Chip ID = %04X", (uint16_t)(chipid >> 32));  //print High 2 bytes
  // Serial.println();
  // Serial.printf("%08X\n", (uint32_t)chipid);  //print Low 4bytes.
  // if (strlen(ESP_NAME) == 0) {
  snprintf(devId, sizeof devId, "%08X%04X", (uint32_t)chipid, (uint16_t)(chipid >> 32));
  Serial.println(F("Device Name: "));
  Serial.println(F(devId));
  strcpy(DEVICE_ID, devId);
}

void saveToFile(const char *path, JsonDocument data) {
  // delay(100);
  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to open file for writing"));
    return;
  }
  if (serializeJson(data, file) == 0) {
    // Error writing file
    Serial.println(F("Failed to write to file"));
  }
  // Close file
  // file.print(data);
  file.close();
  Serial.print(F("Data written to file : "));
  Serial.println(path);
  // printFileData(path);
}

void printFileData(const char *path) {
  File file = SPIFFS.open(path, FILE_READ);
  if (!file) {
    Serial.println(F("Failed to open file for Printing"));
    return;
  } else {
    JsonDocument json;
    DeserializationError error = deserializeJson(json, file);
    serializeJsonPretty(json, Serial);
  }
  file.close();
}

void appendToJsonFile(const char *path, String key, String value) {
  // Open file for reading
  File file = SPIFFS.open(path, FILE_READ);
  if (!file) {
    Serial.println(F("Failed to open file for reading"));
    return;
  }

  JsonDocument doc;
  // delay(100);
  // Parse the JSON data from the file
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print(F("Failed to read file, using empty JSON: "));
    Serial.println(error.c_str());
  }

  // Close the file as it has been read
  file.close();

  // Append new data
  doc[key] = value;
  saveToFile(CONFIG_STA, doc);
}

JsonDocument retrieveJsonFileData(const char *path) {
  JsonDocument json;

  File configFile = SPIFFS.open(path, FILE_READ);
  // delay(100);
  if (configFile) {
    Serial.println(F("Opened configuration file"));
    DeserializationError error = deserializeJson(json, configFile);
    // serializeJsonPretty(json, Serial);
    if (!error) {
      Serial.println(F("Parsing JSON"));
      delay(100);
    } else {
      // Error loading JSON data
      Serial.println(F("Failed to load json config"));
    }
  }
  configFile.close();
  return json;
}

void pulseBlinkLED(int onOffDelay) {
  // delay(500);
  for (int fadeValue = 0; fadeValue <= 255; fadeValue += 5) {
    // sets the value (range from 0 to 255):
    analogWrite(LED_BUILTIN, fadeValue);
    // wait for 20 milliseconds to see the dimming effect
    delay(onOffDelay);
  }

  // fade out from not-quite-max to min in increments of 5 points:
  for (int fadeValue = 255; fadeValue >= 0; fadeValue -= 5) {
    // sets the value (range from 0 to 200):
    analogWrite(LED_BUILTIN, fadeValue);
    // wait for {onOffDelay} milliseconds to see the dimming effect
    delay(onOffDelay);
  }
}

void fastBlinkLED(int onOffDelay) {

  delay(onOffDelay);
  analogWrite(LED_BUILTIN, 200);

  delay(onOffDelay);
  analogWrite(LED_BUILTIN, 0);
}

void startHTTPServer() {
  endPoints();
  // Start webserver
  httpServer.onNotFound(notFound);
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

bool connectToWifi() {
  pulseBlinkLED(1);
  WiFi.mode(WIFI_STA);
  JsonDocument configJson = retrieveJsonFileData(CONFIG_STA);
  if (configJson.isNull() || configJson.size() == 0) { return false; }
  String ssid = configJson["ssid"];
  String password = configJson["password"];
  String ip = configJson["assignedIP"];

  Serial.print(F("trying to connect to #"));
  Serial.println(ssid);
  if (configJson.containsKey("assignedIP")) {
    IPAddress local_IP;
    if (!local_IP.fromString(ip)) {
      // delay(100);
      Serial.println(F("Invalid IP Address"));
      configJson["assignedIP"] = "";
    } else {
      // delay(100);
      Serial.println(F("valid IP Address"));
      WiFi.config(local_IP);
    }
  }
  int attempts = 0;

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && attempts < maxWifiConnectAttempts) {
    delay(retryWifiConnectDelay);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("Connected to WiFi!"));
    Serial.print(F("# IP address: "));
    Serial.println(WiFi.localIP());
    delay(100);
    if (!configJson.containsKey("assignedIP") || configJson["assignedIP"] == "") {
      appendToJsonFile(CONFIG_STA, "assignedIP", WiFi.localIP().toString());
    }
    return true;
  } else {
    Serial.println("\nFailed to connect to Wi-Fi. Please check your credentials.");
    SPIFFS.remove(CONFIG_STA);
    // ESP.restart();
    // Handle failure to connect (e.g., enter deep sleep, restart, or ask for new credentials)
  }
  return false;
}

void resetDevice() {
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  Serial.println(F("Resetting Device..."));
  while (file) {
    String filePath = file.path();
    Serial.print("Deleting file: ");
    Serial.println(filePath);
    delay(100);

    if (SPIFFS.remove(filePath)) {
      Serial.println(F("File deleted successfully"));
    } else {
      Serial.println(F("Failed to delete file"));
    }

    file = root.openNextFile();
  }
  root.close();
  file.close();
  Serial.println(F("All files deleted."));
  setup();
}

bool connectToFireBase() {
  JsonDocument json = retrieveJsonFileData(CONFIG_DB);
  Serial.print(F("ctfb: "));
  serializeJsonPretty(json, Serial);
  return true;
  /* config.api_key = json["APIKey"].as<const char*>();
  config.database_url = json["firebaseURL"].as<const char*>();

  // Sign in anonymously
  if (Firebase.signUp(&config, &auth, json["fbUserEmail"].as<const char*>(), json["fbUserPassword"].as<const char*>())) {
    Serial.println("Firebase authentication succeeded.");
  } else {
    Serial.printf(F("Firebase authentication failed: "));
    // Serial.printf(F("Firebase authentication failed:  %s\n", config.signer.signupError.message.c_str() );
  }

  // Initialize Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Send test data to Firebase
  if (Firebase.setInt(fbData, "/test/value", 42)) {
    Serial.println(F("Data written to Firebase successfully."));
  } else {
    Serial.print(F("Error writing data: "));
    // Serial.println(fbData.errorReason());
  }  */
}

bool getIdToken(String email, String password, String apiKey) {
  String token = "";

  String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + apiKey;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String requestBody = "{\"email\":\"" + email + "\",\"password\":\"" + password + "\",\"returnSecureToken\":true}";

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Response: " + response);

    // Parse the JSON response to extract the ID token
    JsonDocument doc;
    deserializeJson(doc, response);
    if (doc.containsKey("error")) {
      http.end();
      JsonDocument err = doc["error"];  // = response["error"].as<JsonDocument>;
      // deserializeJson(err, response["error"].as<JsonDocument>);
      authError = err["message"].as<String>();
      return false;
    } else {
      authToken = doc["idToken"].as<String>();
      userUID = doc["localId"].as<String>();

      Serial.println(F("token and uid"));
      Serial.println(authToken);
      Serial.println(userUID);
      http.end();
      return true;
    }
  } else {
    Serial.println("Error on HTTP request: " + String(httpResponseCode));
    http.end();
    return false;
  }
  http.end();
  return true;
}

String postDataToFirebase(String path, String jsonData) {
  // HTTPClient http;
  String payload;

  // Build Firebase URL

  String url = firebaseHost + path;
  if (authToken != "") {
    url += "?auth=" + String(authToken);
  }

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.PUT(jsonData);

  if (httpResponseCode > 0) {
    payload = http.getString();  // Get the response payload
    Serial.println("Response: " + payload);
  } else {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpResponseCode);
  }

  http.end();  // Free resources
  return payload;
}

bool connectMQTT() {
  return true;
}

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    if (client.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("connected");
      
      // Subscribe to a topic
      client.subscribe("esp32/test");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void endPoints() {
  // Wifi Config
  httpServer.on(
    "/config/sta", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!deserializeData(request, data, CONFIG_STA))
        return;

      request->send(200, "text/plain", F("[SUCCESS] The Network details are successfully uplaoded to the device. Please check Device Logs for connection status"));
      // ssid_sta = r.ssid;
      // password = r.password.c_str();
      if (connectToWifi()) {
        Serial.print(F("connected..."));
      } else {
        Serial.print(F("Unable to connect!"));
      }
    });

  // FireBase Config
  httpServer.on(
    "/config/firebase", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!deserializeData(request, data, CONFIG_DB))
        return;

      request->send(200, "text/plain", F("[SUCCESS] The Network details are successfully uplaoded to the device. Please check Device Logs for connection status"));

      // Serial.println(F("FireBase data Saved"));
      if (connectToFireBase()) {
        Serial.println(F("Connected to FireBase"));

        // Example data to post
        JsonDocument dataToPush = retrieveJsonFileData(CONFIG_STA);
        dataToPush["device_id"] = DEVICE_ID;
        String jsonStr;
        serializeJson(dataToPush, jsonStr);
        String jsonData = "{\"devices\":{\"deviceID\":" + String(ESP_NAME) + ",\"age\":30}}";

        // POST data to Firebase
        String response = postDataToFirebase("users/" + userUID + "/devices/" + DEVICE_ID + ".json", jsonStr);
        if (response != "") {
          Serial.println("Post Response: " + response);
        } else {
          Serial.println("Failed to post data.");
        }

      } else {
        Serial.println(F("Unable to Connected to FireBase"));
      }
    });

  // MQTT Config
  httpServer.on(
    "/config/mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!deserializeData(request, data, CONFIG_MQTT))
        return;

      request->send(200, "text/plain", F("[SUCCESS] The MQTT Broker details are successfully uplaoded to the device. Please check Device Logs for connection status"));
      // ssid_sta = r.ssid;
      // password = r.password.c_str();
      /* if (connectToWifi()) {
        Serial.print(F("connected..."));
      } else {
        Serial.print(F("Unable to connect!"));
      } */
    });
  return;
}

bool deserializeData(AsyncWebServerRequest *request, uint8_t *data, const char *path) {
  Serial.print(F("Payload: "));
  Serial.println((const char *)data);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data);
  if (doc.isNull()) {
    request->send(500, "text/plain", "Invalid Payload...");
    doc.clear();
    return false;
  }
  if (error) {
    request->send(500, "text/plain", "Unable to configure the device ");
    Serial.print(F("Error Occured: "));
    Serial.println(error.c_str());
    doc.clear();
    return false;
  }
  if (path == CONFIG_STA) {
    if (!doc.containsKey("espName") || !doc.containsKey("ssid") || !doc.containsKey("password")) {
      request->send(500, "text/plain", "Unable to configure the device [Invalid Payload... Missing some required key]");
      doc.clear();
      return false;
    } else {
      saveToFile(path, doc);
      strcpy(ESP_NAME, doc["espName"].as<const char *>());
      Serial.println(F(ESP_NAME));
      return true;
    }
  } else if (path == CONFIG_DB) {
    if (!doc.containsKey("APIKey") || !doc.containsKey("firebaseRTDBURL") || !doc.containsKey("fbUserEmail") || !doc.containsKey("fbUserPassword")) {
      request->send(500, "text/plain", "Unable to configure the device [Invalid Payload... Missing some required key]");
      doc.clear();
      return false;
    } else {
      if (getIdToken(doc["fbUserEmail"].as<const char *>(), doc["fbUserPassword"].as<const char *>(), doc["APIKey"].as<const char *>())) {
        JsonDocument data;
        data["email"] = doc["fbUserEmail"].as<const char *>();
        data["pass"] = doc["fbUserPassword"].as<const char *>();
        data["key"] = doc["APIKey"].as<const char *>();
        data["rtdb"] = doc["firebaseRTDBURL"].as<const char *>();
        firebaseHost = doc["firebaseRTDBURL"].as<String>();
        if (String(firebaseHost[firebaseHost.length() - 1]) != String("/")) {
          firebaseHost = firebaseHost + "/";
        }
        saveToFile(path, data);
        Serial.print(F("Token : "));
        Serial.println(authToken);
        Serial.print(F("UID : "));
        Serial.println(userUID);
        return true;
      } else {
        request->send(401, "text/plain", "Unable to connect to firebase : " + String(authError));
        doc.clear();
        return false;
      }
    }
  } else if (path == CONFIG_MQTT) {
    if (!doc.containsKey("brokerHost") || !doc.containsKey("brokerPort") || !doc.containsKey("protocol") || !doc.containsKey("brokerUserName") || !doc.containsKey("brokerPassword")) {
      request->send(500, "text/plain", "Unable to configure the device [Invalid Payload... Missing some required key]");
      doc.clear();
      return false;
    } else {
      if (connectMQTT()) {
        JsonDocument data;
        data["host"] = doc["brokerHost"].as<const char *>();
        data["port"] = doc["brokerPort"].as<const char *>();
        data["protocol"] = doc["protocol"].as<const char *>();
        data["user"] = doc["brokerUserName"].as<const char *>();
        data["pass"] = doc["brokerPassword"].as<const char *>();
        
        saveToFile(path, data);
        Serial.print(F("MQTT Config Saved : "));
        
        return true;
      } else {
        request->send(401, "text/plain", "Unable to connect to MQTT Broker : " + String(authError));
        doc.clear();
        return false;
      }
    }
  } else {
    Serial.println(F("file Path not valid"));
    return false;
  }

  return false;
}