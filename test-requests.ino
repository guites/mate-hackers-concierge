/*
  Guilherme Garcia <https://guilhermegarcia.dev>
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
#include <WiFiClientSecure.h>

// Telegram Bot Token
const String BOT_TOKEN = "bottoken";
// WiFi Login
const char* ssid = "wifi-username";
// WiFi Password
const char* password = "wifi-pass";
// Google Script Deployment ID:
const char *GScriptId = "your-google-script-id";

// Base URL for bot requests
String baseUrl = "https://api.telegram.org/bot" + BOT_TOKEN;

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
}

void loop() {
  //Check WiFi connection status
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("WiFi Disconnected");
    return;
  }
  // String getMeEndpoint = baseUrl + "/getMe";
  
  JSONVar updates = getUpdates(last_received_id);

  for (int i = 0; i < updates["result"].length(); i++) {
    JSONVar update = updates["result"][i];

    int update_id = update["update_id"];

    int message_id = update["message"]["message_id"];
    int from_id = update["message"]["from"]["id"];
    bool is_bot = update["message"]["from"]["is_bot"];
    String from_first_name = update["message"]["from"]["first_name"];
    String language_code = update["message"]["from"]["language_code"];
    
    int chat_id = update["message"]["chat"]["id"];
    String chat_first_name = update["message"]["chat"]["first_name"];
    String chat_type = update["message"]["chat"]["type"];

    String text = update["message"]["text"];

    Serial.println("--------");
    Serial.print(update_id);
    Serial.print(" from ");
    Serial.print(from_first_name);
    Serial.print(": ");
    Serial.println(text);


    handleResponse(message_id, from_id, from_first_name, chat_id, text);

    Serial.println("--------");

    last_received_id = update_id;
  }
}

void dumpObject(JSONVar obj) {
  JSONVar resultKeys = obj.keys();

  for (int j = 0; j < resultKeys.length(); j++) {
    JSONVar value = obj[resultKeys[j]];
    Serial.print(resultKeys[j]);
    Serial.print(" = ");
    Serial.println(value);  
  }
}

void handleResponse(int message_id, int from_id, String from_first_name, int chat_id, String text) {
  if (text == "/abrir") {
    sendResponse(chat_id, "Porta%20aberta.");
    logCommandToSheets(message_id, from_id, from_first_name, text);
  }
  if (text == "/fechar") {
    sendResponse(chat_id, "Porta%20fechada.");
    logCommandToSheets(message_id, from_id, from_first_name, text);
  }
  if (text == "/liberar_acesso") {
    sendResponse(chat_id, "Acesso%20liberado.");
    logCommandToSheets(message_id, from_id, from_first_name, text);
  }
}

void logCommandToSheets(int message_id, int from_id, String from_first_name, String command) {
  String sheetsURL = "https://script.google.com" +  String("/macros/s/") + GScriptId + "/exec";
  
  httpPOSTRequestJson(sheetsURL, message_id, from_id, from_first_name, command);
}

void sendResponse(int chat_id, String message) {
  String sendResponseEndpoint = baseUrl + "/sendMessage?chat_id=" + chat_id + "&text=" + message;
  stringPayload = httpPOSTRequest(sendResponseEndpoint);
  Serial.println(stringPayload);
  JSONVar objPayload = JSON.parse(stringPayload);
}

JSONVar getUpdates(int last_update_id) {
  int updatesOffset = last_update_id + 1;
  String getUpdatesEndpoint = baseUrl + "/getUpdates?offset=" + updatesOffset + "&timeout=" + polling_seconds;
  stringPayload = httpGETRequest(getUpdatesEndpoint);
  Serial.println(stringPayload);
  JSONVar objPayload = JSON.parse(stringPayload);
  return objPayload;
}

String httpPOSTRequestJson(String requestURL, int message_id, int from_id, String from_first_name, String command) {
  WiFiClientSecure client;
  HTTPClient https;

  client.setInsecure();
  https.addHeader("Content-Type", "application/json");
  https.begin(client, requestURL);

  String payload = "";
  String payload_base =  "{\"command\": \"insert_row\", \"sheet_name\": \"logs\", \"values\": ";

  payload = payload_base + "\"" + message_id + "," + from_id + "," + from_first_name + "," + command + "\"}";

  int httpResponseCode = https.POST(payload);

  String response = "{}";
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    response = https.getString();
    Serial.println(response);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    Serial.printf("\n[HTTPS] POST... failed, error: %s\n", https.errorToString(httpResponseCode).c_str());
  }
  // Free resources
  https.end();
  return response;
}

String httpPOSTRequest(String requestURL) {
  WiFiClientSecure client;
  HTTPClient https;

  client.setInsecure();

  https.begin(client, requestURL);

  int httpResponseCode = https.POST("");

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

String httpGETRequest(String requestURL) {
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


