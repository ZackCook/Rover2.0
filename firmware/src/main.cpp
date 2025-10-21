#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <UUID.h>

// put function declarations here:
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void sendVerification();


//NETWORK CREDENTIALS
const char* ssid = "SpectrumSetup-CD";
const char* password = "leastpoet407";
const char* serverAddress = "192.168.1.26";
const int serverPort = 3000;
String MAC_ADDRESS = "";

//Server/WS variables and objects
WebSocketsClient webSocket;//web socket instance that will connect to wqensocket server and handle RTC
String serverSideID;// will be assigned when sent back from the server after a successful client verification (server side)

//Pin assignments
const int TEST_LED_PIN = 2;
//timing variables

//
UUID uuidGen;

const int JSON_DOC_SIZE = 512;

void setup() {
  Serial.begin(115200);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }//wait for wifi to connect before proceeding

    Serial.println("Connected to WiFi. IP address: ");
    Serial.println(WiFi.localIP());

    MAC_ADDRESS = WiFi.macAddress();
    Serial.println(MAC_ADDRESS);

    webSocket.begin(serverAddress, serverPort, "/");//set routing for webSocket server
    webSocket.onEvent(webSocketEvent);//set event listener for webSocket events

    pinMode(TEST_LED_PIN, OUTPUT);
    digitalWrite(TEST_LED_PIN, LOW);//turn off test LED
}//end setup()


void loop() {
  webSocket.loop();//keep webSocket alive and processing events
}

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
      Serial.printf("Websocket Message Received: %s\n", payload);
      //handle message
      break;
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
  //JsonDocument payload = verificationDoc.createNestedObject("msgPayload");
  verificationDoc["msgPayload"]["clientType"] = "rover";
  //payload["clientType"] = "rover";

  //StaticJsonDocument <JSON_DOC_SIZE> doc;

  serializeJson(verificationDoc, verificationString);
  Serial.println(verificationString);
  webSocket.sendTXT(verificationString);
}
