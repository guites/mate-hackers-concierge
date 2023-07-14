/*
  Guilherme Garcia <https://guilhermegarcia.dev>
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
// #include <WiFiClient.h>
#include <Arduino_JSON.h>
// #include <WiFiClientSecureBearSSL.h>
#include <WiFiClientSecure.h>

#define CHAT_ID "123445"

const String BOT_TOKEN = "bottoken";
const char* ssid = "wifi-username";
const char* password = "wifi-pass";

// Base URL for bot requests
String baseUrl = "https://api.telegram.org/bot" + BOT_TOKEN;

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 5000;

int last_received_id = 0;
int polling_seconds = 30;

String stringPayload;

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
 
  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");
}

void loop() {
  // Send an HTTP GET request depending on timerDelay
  if ((millis() - lastTime) > timerDelay) {
    lastTime = millis();
    //Check WiFi connection status
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("WiFi Disconnected");
      return;
    }
    // String getMeEndpoint = baseUrl + "/getMe";
    int updatesOffset = last_received_id + 1;
    String getUpdatesEndpoint = baseUrl + "/getUpdates?offset=" + updatesOffset + "&timeout=" + polling_seconds;
    stringPayload = httpGETRequest(getUpdatesEndpoint);
    JSONVar objPayload = JSON.parse(stringPayload);

    // JSON.typeof(jsonVar) can be used to get the type of the var
    if (JSON.typeof(objPayload) == "undefined") {
      Serial.println("Parsing input failed!");
      return;
    }

    for (int i = 0; i < objPayload["result"].length(); i++) {
      JSONVar currentResult = objPayload["result"][i];

      // myObject.keys() can be used to get an array of all the keys in the object
      JSONVar resultKeys = currentResult.keys();

      for (int j = 0; j < resultKeys.length(); j++) {
        JSONVar value = currentResult[resultKeys[j]];
        Serial.print(resultKeys[j]);
        Serial.print(" = ");
        Serial.println(value);  
      }

      int update_id = currentResult["update_id"];
      int from_id = currentResult["from"]["id"];
      int from_username = currentResult["from"]["username"];
      String text = currentResult["text"];

      Serial.print(update_id);
      Serial.print(" from ");
      Serial.print(from_username);
      Serial.print(": ");
      Serial.println(text);

      last_received_id = update_id;
    } 
  }
}

String httpGETRequest(String requestURL) {
  // std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  WiFiClientSecure client;
  HTTPClient https;

  client.setInsecure();

  Serial.print("Setting timeout to ");
  Serial.print(polling_seconds * 1000);
  Serial.println(" millisseconds.");
  https.setTimeout(polling_seconds * 1000);

  Serial.print("Requesting ");
  Serial.print(requestURL);
  Serial.println(" ...");
  // Your IP address with path or Domain name with URL path 
  https.begin(client, requestURL);
  
  // Send HTTP GET request
  int httpResponseCode = https.GET();
  
  String payload = "{}";
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = https.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    Serial.printf("\n[HTTPS] GET... failed, error: %s\n", https.errorToString(httpResponseCode).c_str());
  }
  // Free resources
  https.end();

  return payload;
}
