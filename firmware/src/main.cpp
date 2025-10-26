#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <UUID.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>//used for uart communication with GPS module
#include <Adafruit_BMP280.h>

// put function declarations here:
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void sendVerification();
void sendTelemetry();
void handleCommand(JsonObjectConst payload);


//NETWORK CREDENTIALS
const char* ssid = "SpectrumSetup-CD";
const char* password = "leastpoet407";
const char* serverAddress = "192.168.1.26";
const int serverPort = 3000;
const char* serverID = "server-main";
String MAC_ADDRESS = "";
String assignedID = "placeholder-value";

//Server/WS variables and objects
WebSocketsClient webSocket;

//Pin assignments
const int TEST_LED_PIN = 2;
const int GPS_TX_PIN = 17;
const int GPS_RX_PIN = 16;

//timing variables
unsigned long latestGPSRead = 0;
const unsigned long GPS_READ_INTERVAL = 1000;

unsigned long latestBMPRead = 0;
const long BMP_READ_INTERVAL = 250;

unsigned long latestLedToggle = 0;
unsigned long LED_BLINK_INTERVAL = 1000; // Default to a slow blink (1 second)

unsigned long latestTelemetrySent = 0;
const long TELEMETRY_SEND_INTERVAL = 500; //MS

//variables for non-blocking LED blink
bool ledState = false;

//Objects and variables
UUID uuidGen;
TinyGPSPlus gps;
const int GPS_BAUD_RATE = 9600;
HardwareSerial hws(2);

const int JSON_DOC_SIZE = 512;

Adafruit_BMP280 bmp;


//sensor values
double latestLat = 0;
double latestLng = 0;
uint32_t latestSatellites = 0;
float latestHDOP = -99;
float latestGPSAlt = 0;
float latestGPSHeading = 0;
float latestGPSSpeed = 0;

float latestTemp = 0;
float latestPressure = 0;
float latestBMPAlt = 0;

void setup() {
  Serial.begin(115200);

  hws.begin(GPS_BAUD_RATE);

  Wire.begin();
  Wire.setClock(400000);

  if(!bmp.begin()){
    Serial.println("Cannot find bmp280, check wiring");
    while(1);
  }
  Serial.println("BMP 280 initialized");

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
				Adafruit_BMP280::SAMPLING_X2,
				Adafruit_BMP280::SAMPLING_X8,
				Adafruit_BMP280::FILTER_X4,
				Adafruit_BMP280::STANDBY_MS_63);
 Serial.println("BMP parameters set");

 WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
  }//wait for wifi to connect before proceeding
  Serial.println("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());

  MAC_ADDRESS = WiFi.macAddress();
  Serial.print("ESP32 MAC Address: ");
  Serial.println(MAC_ADDRESS);

  webSocket.begin(serverAddress, serverPort, "/");//set routing for webSocket server
  webSocket.onEvent(webSocketEvent);//set event listener for webSocket events
  webSocket.setReconnectInterval(5000);

  pinMode(TEST_LED_PIN, OUTPUT);
  digitalWrite(TEST_LED_PIN, LOW);//turn off test LED
}//end setup()


void loop() {
    webSocket.loop();

    unsigned long currentMillis = millis();

    // This runs constantly, feeding the GPS parser.
    while (hws.available() > 0) {
        gps.encode(hws.read());
    }

    // --- This block runs once per second to print the GPS status ---
    if (currentMillis - latestGPSRead > GPS_READ_INTERVAL) {
        latestGPSRead = currentMillis;

        if (gps.location.isValid() && gps.location.isUpdated()) {
          latestLat = gps.location.lat();
          latestLng = gps.location.lng();
          Serial.printf("Location Updated: Lat=%.6f, Lng=%.6f\n", latestLat, latestLng);
        }
        if (gps.satellites.isValid() && gps.satellites.isUpdated()) {
          latestSatellites = gps.satellites.value();
          Serial.printf("Satellites Updated: %u\n", latestSatellites);
        }
        if (gps.hdop.isValid() && gps.hdop.isUpdated()) {
          latestHDOP = gps.hdop.hdop();
          Serial.printf("HDOP Updated: %.2f\n", latestHDOP);
        }
        if (gps.altitude.isValid() && gps.altitude.isUpdated()){
          latestGPSAlt = gps.altitude.meters();
          Serial.printf("GPS Altitude Updated: %.2f m\n", latestGPSAlt);
        }
        if (gps.course.isValid() && gps.course.isUpdated()){
          latestGPSHeading = gps.course.deg();
          Serial.printf("GPS Course Updated: %.2f deg\n", latestGPSHeading);
        }
        if(gps.speed.isValid() && gps.speed.isUpdated()){
          latestGPSSpeed = gps.speed.kmph();
          Serial.printf("GPS Speed updated: %.2f kmph\n", latestGPSSpeed);
        }

        if (gps.satellites.value() > 3) {
            LED_BLINK_INTERVAL = 100;
            Serial.println("GPS lock acquired");
        } else {
            // No lock yet. Set the blink interval to be slow.
            LED_BLINK_INTERVAL = 1000; // A slow 1-second blink
            Serial.println("No GPS lock acquired");
        }
    }

    if(currentMillis - latestBMPRead >= BMP_READ_INTERVAL){
      latestBMPRead = currentMillis;
      latestTemp = bmp.readTemperature();
      latestPressure = bmp.readPressure();

      Serial.println("--- Reading BMP280 Data ---");
      Serial.printf("Temperature: %.2f *C\n", latestTemp);
      Serial.printf("Pressure: %.2f Pa\n", latestPressure);
    }

    if (currentMillis - latestTelemetrySent >= TELEMETRY_SEND_INTERVAL) {
        latestTelemetrySent = currentMillis; // Reset the telemetry send timer
        if (webSocket.isConnected()) { // Only send if connected
             sendTelemetry();
        }
    }

    /*
    if (currentMillis - latestLedToggle > LED_BLINK_INTERVAL) {
        latestLedToggle = millis(); // Reset the blink timer

        // Toggle the LED state
        ledState = !ledState;
        digitalWrite(TEST_LED_PIN, ledState);
    }
    */
}//end loop()

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("Websocket Disconnected");
      break;
    case WStype_CONNECTED:
        Serial.println("Websocket Connected");
        sendVerification();
        break;
    case WStype_TEXT:
    {
      Serial.printf("Websocket Message Received: %s\n", payload);
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload, length);

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return; // Exit if JSON is invalid
      }

      // --- Safely extract msgType ---
      const char* msgType = doc["msgType"];
      if (msgType == nullptr) {
        Serial.println("Error: Received message without msgType field.");
        return; // Exit if msgType is missing
      }

      // --- Dispatch based on msgType ---
      if (strcmp(msgType, "assignedID") == 0) {
        Serial.println("Processing assignedID message...");
        // Safely access the nested ID value using JsonVariantConst for read-only access
        JsonVariantConst idVariant = doc["msgPayload"]["assignedID"];
                
        if (idVariant.is<const char*>()) { // Check if the field exists and is a string
          assignedID = idVariant.as<String>(); // Store the ID in the global variable
          Serial.println("Successfully assigned ID: " + assignedID);
        } else {
          Serial.println("Error: assignedID message payload is invalid or missing 'assignedID' field.");
        }

        } else if (strcmp(msgType, "COMMAND") == 0) {
          Serial.println("Processing COMMAND message...");
          // Get the nested msgPayload object (use Const for safety)
          JsonObjectConst cmdPayload = doc["msgPayload"].as<JsonObjectConst>(); 
                
          if (cmdPayload) { // Check if the payload object exists
            handleCommand(cmdPayload); // Pass the payload to your command handler
          } else {
            Serial.println("Error: COMMAND message is missing msgPayload object.");
          }
                
          } else {
            // --- Handle Unknown Message Types ---
            Serial.printf("Received unhandled message type: %s\n", msgType);
          }
        break;
      }
    case WStype_BIN:
      Serial.printf("Websocket Binary Message Received of length: %u\n", length);
      //handle binary message
      break;
    }
}

void sendVerification(){
  uuidGen.generate();
  JsonDocument verificationDoc;
  String verificationString;

  verificationDoc["msgID"] = uuidGen.toCharArray();
  verificationDoc["msgType"] = "verification";
  verificationDoc["msgSource"] = "rover-placeholder-source-id";
  verificationDoc["msgTarget"] = "server-main";
  verificationDoc["msgTimestamp"] = "";//can be established using GPS module, without internet connection
  verificationDoc["msgPayload"]["clientType"] = "rover";

  serializeJson(verificationDoc, verificationString);
  Serial.println(verificationString);
  webSocket.sendTXT(verificationString);
}

void sendTelemetry(){
  JsonDocument telemetryDoc;

  uuidGen.generate();

  telemetryDoc["msgID"] = uuidGen.toCharArray();
  telemetryDoc["msgType"] = "TELEMETRY";
  telemetryDoc["msgSource"] = assignedID;
  telemetryDoc["msgTarget"] = serverID;
  telemetryDoc["msgTimestand"] = "placeholder-timestamp";

  telemetryDoc["msgPayload"]["lat"] = latestLat;
  telemetryDoc["msgPayload"]["lng"] = latestLng;
  telemetryDoc["msgPayload"]["satellites"] = latestSatellites;
  telemetryDoc["msgPayload"]["hdop"] = latestHDOP;
  telemetryDoc["msgPayload"]["gpsAlt"] = latestGPSAlt;
  telemetryDoc["msgPayload"]["gpsHeading"] = latestGPSHeading;
  telemetryDoc["msgPayload"]["gpsSpeed"] = latestGPSSpeed;

  telemetryDoc["msgPayload"]["temp"] = latestTemp;
  telemetryDoc["msgPayload"]["pressure"] = latestPressure;
  

  String telemetryString;
  serializeJson(telemetryDoc, telemetryString);

  if (telemetryString.length() > 0) {
    Serial.println("Sending Telemetry: " + telemetryString);
    webSocket.sendTXT(telemetryString); // Send the message
  } else {
    Serial.println("Error: Failed to serialize telemetry JSON!");
  }
}


//used for parsing incoming commands into actionable calls within the firmware
//Motor commands, led toggles etc.
void handleCommand(JsonObjectConst payload){
}