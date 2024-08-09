#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#define ARDUINOJSON_USE_LONG_LONG 1
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
// #include "ESPFlashString.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#define CONFIG_STA "/sta.json"
#define CONFIG_MQTT "/mqtt.json"
#define BOOT_BTN 0
#define LED_BUILTIN 2
#define FORCE_CONFIG false
// #define ESP_NAME[20] ""
char ESP_NAME[20] = "";

unsigned long buttonPressTime = 0;
bool buttonPressed = false;
const unsigned long longPressDuration = 3000;  // Long press duration in milliseconds (e.g., 3 seconds)

unsigned long MyTestTimer = 0;  // variables MUST be of type unsigned long

const char *prefix_ssid_ap = "ESP32_AP";
IPAddress myIP = "";

AsyncWebServer httpServer(80);

struct Rule_configSTA {
  int ID;
  const char *espName;
  String ssid;
  String password;
};


bool connectToWifi(String ssid, String password, String ip = "");


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  Serial.begin(115200);
  delay(100);
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }
  Serial.println("SPIFFS mounted successfully");
  delay(2000);
  if (!SPIFFS.exists(CONFIG_STA)) {
    WiFi.mode(WIFI_AP);
    uint64_t chipid;
    char esp_ap_name[20];
    Serial.println(strlen(ESP_NAME));
    chipid = ESP.getEfuseMac();  //The chip ID is essentially its MAC address(length: 6 bytes).
                                 // Serial.printf("ESP32 Chip ID = %04X", (uint16_t)(chipid >> 32));  //print High 2 bytes
                                 // Serial.println();
                                 // Serial.printf("%08X\n", (uint32_t)chipid);  //print Low 4bytes.
    if (strlen(ESP_NAME) == 0) {
      snprintf(esp_ap_name, sizeof esp_ap_name, "%s_%08X%04X", prefix_ssid_ap, (uint32_t)chipid, (uint16_t)(chipid >> 32));
      strcpy(ESP_NAME, esp_ap_name);
    }
    Serial.println(esp_ap_name);
    Serial.println(ESP_NAME);
    WiFi.softAP(ESP_NAME);
    // WiFi.setHostname(ESP_NAME);
    myIP = WiFi.softAPIP();
    Serial.println(F("Configuring access point..."));
    Serial.print(F("AP IP address: "));
    Serial.println(myIP);
    Serial.println(F("ESP32 configured as Access Point"));

  } else {
    File configFile = SPIFFS.open(CONFIG_STA, "r");
    delay(100);
    if (configFile) {
      Serial.println("Opened configuration file");
      JsonDocument json;
      DeserializationError error = deserializeJson(json, configFile);
      serializeJsonPretty(json, Serial);
      if (!error) {
        Serial.println("Parsing JSON");
        delay(100);
        json.containsKey("assignedIP") ? connectToWifi(json["ssid"], json["password"], json["assignedIP"]) : connectToWifi(json["ssid"], json["password"]);
      } else {
        // Error loading JSON data
        Serial.println("Failed to load json config");
      }
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
    Serial.print(F("."));
    delay(1000);
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
  Serial.println(WiFi.getMode());
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


void saveToFile(const char *path, JsonDocument data) {
  Serial.print("data: ");
  serializeJsonPretty(data, Serial);
  delay(100);
  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (serializeJson(data, file) == 0) {
    // Error writing file
    Serial.println(F("Failed to write to file"));
  }
  // Close file
  // file.print(data);
  file.close();
  Serial.println("Data written to file");
  printFileData(path);
}

void printFileData(const char *path) {
  Serial.println(path);
  File file = SPIFFS.open(path, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for Printing");
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
    Serial.println("Failed to open file for reading");
    return;
  }

  JsonDocument doc;
  delay(100);
  // Parse the JSON data from the file
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print("Failed to read file, using empty JSON: ");
    Serial.println(error.c_str());
  }

  // Close the file as it has been read
  file.close();

  // Append new data
  doc[key] = value;
  saveToFile(CONFIG_STA, doc);
}

void pulseBlinkLED(int onOffDelay) {
  delay(500);
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
  Serial.println(F("EndPoints Enable... the device can be configure Now!! "));
  // Start webserver
  httpServer.onNotFound(notFound);
}

void endPoints() {

  httpServer.on(
    "/config/sta", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      Rule_configSTA r = parseRule(request, data);
      if (!r.ID == -2)
        return;

      r.ID = 1;


      request->send(200, "text/plain", F("[SUCCESS] The Network details are successfully uplaoded to the device. Please check Device Logs for connection status"));
      // ssid_sta = r.ssid;
      // password = r.password.c_str();
      if (connectToWifi(r.ssid, r.password)) {
        Serial.print(F("connected..."));
      } else {
        Serial.print(F("Unable to connect!"));
      }
    });
  return;
}

boolean TimePeriodIsOver(unsigned long &periodStartTime, unsigned long TimePeriod) {
  unsigned long currentMillis = millis();
  if (currentMillis - periodStartTime >= TimePeriod) {
    periodStartTime = currentMillis;  // set new expireTime
    return true;                      // more time than TimePeriod) has elapsed since last time if-condition was true
  } else return false;                // not expired
}


Rule_configSTA parseRule(AsyncWebServerRequest *request, uint8_t *data) {
  // JsonObject obj = "{\"espName\":\"test\",\"ssid\":\"ssid\", \"password\"}";
  JsonDocument obj = deserializeData(request, data);
  Rule_configSTA r;
  if (obj.isNull()) {
    r.ID = -2;
    return r;
  }

  saveToFile(CONFIG_STA, obj);
  r.espName = obj["espName"].as<const char *>();
  r.ssid = obj["ssid"].as<String>();
  r.password = obj["password"].as<const char *>();
  strcpy(ESP_NAME, r.espName);
  Serial.println(F(ESP_NAME));
  // printRule(r);

  return r;
}

/* void printRule(Rule_configSTA &r) {
  Serial.print("espName: ");
  Serial.println(r.espName);
  Serial.print("ssid: ");
  Serial.println(r.ssid);
} */

JsonDocument deserializeData(AsyncWebServerRequest *request, uint8_t *data) {
  Serial.print("Payload: ");
  Serial.println((const char *)data);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data);

  if (error || !doc.containsKey("espName") || !doc.containsKey("ssid") || !doc.containsKey("password")) {
    request->send(400, "text/plain", "Unable to configure the device [Invalid JSON Structure]");
    doc.clear();
  }
  return doc;
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

bool connectToWifi(String ssid, String password, String ip) {
  pulseBlinkLED(1);
  WiFi.mode(WIFI_STA);

  int myCount = 0;

  Serial.print(F("trying to connect to #"));
  Serial.println(ssid);
  if (ip != "") {
    IPAddress local_IP;
    if (!local_IP.fromString(ip)) {
      delay(100);
      Serial.println("Invalid IP Address");
    } else {
      delay(100);
      Serial.println("valid IP Address");
      WiFi.config(local_IP);
    }
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && myCount < 31) {
    yield();
    if (TimePeriodIsOver(MyTestTimer, 500)) {  // once every 500 miliseconds
      Serial.print(".");                       // print a dot
      myCount++;

      if (myCount > 30) {  // after 30 dots = 15 seconds restart
        Serial.println();
        Serial.print("not yet connected executing ESP.restart();");
        ESP.restart();
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print(F("Connected to #"));
    Serial.print(ssid);
    Serial.print("# IP address: ");
    myIP = WiFi.localIP();
    Serial.println(myIP);
    delay(100);
    if (ip == "") {
      appendToJsonFile(CONFIG_STA, "assignedIP", myIP.toString());
    }
    return true;
  }
  return false;
}

void checkForReset() {
}

void resetDevice() {
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  Serial.println(F("Resetting Device..."));
  // Serial.println(F(file.path()));
  while (file) {
    Serial.println(F("Resetting Device2..."));
    String filePath = file.path();
    delay(100);
    Serial.print("Deleting file: ");
    Serial.println(filePath);

    if (SPIFFS.remove(filePath)) {
      Serial.println("File deleted successfully");
    } else {
      Serial.println("Failed to delete file");
    }

    file = root.openNextFile();
  }

  Serial.println("All files deleted.");
  setup();
}
