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
// Google Script Deployment ID for logging commands
const char *PostGScriptId = "your-google-script-id";
// Google Script Deployment ID for reading the list of users
const char *ReadUsersGScriptId = "your-google-script-id";

// Base URL for bot requests
String baseUrl = "https://api.telegram.org/bot" + BOT_TOKEN;

// Holds the last receive telegram message id to avoid processing duplicates
int last_received_id = 0;
// Telegram API long polling timeout
int polling_seconds = 30;

/**
 * user_type used to authorize specific actions.
 * + `JEDI` - ...
 * + `PADAWAN` - ...
 * + `SITH` - Blocked from interacting with the bot, but there is interest in keeping attempts registered.
 * + `YOUNGLING` - User is allowed to interact with the bot, but has no permissions.
 */
typedef enum {
    JEDI,
    PADAWAN,
    SITH,
    YOUNGLING
} userType_t;

class User {
  public:
    int id;
    String telegram_id;
    String name;
    userType_t user_type;
};

userType_t getUserType(const String& userTypeString) {
    if (userTypeString == "Jedi") {
      return JEDI;
    }
    if (userTypeString == "Padawan") {
        return PADAWAN;
    }
    if (userTypeString == "Sith") {
        return SITH;
    }
    // Set a default case for unknown values
    return YOUNGLING;   
}

// Holds list of users fetched from the database
std::vector<User> registered_users;


void setup() {
  Serial.begin(115200);
  // Allow some time for the serial port to initialize
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Loading users from Google Sheets...");
  readUsersFromSheets();
}

void loop() {
  //Check WiFi connection status
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("WiFi Disconnected");
    return;
  }
  
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

bool authenticateUser(int from_id) {
  // Check if a given telegram_id exists in the userArray
  bool found = false;
  Serial.print("Authenticating user with Telegram ID ");
  Serial.print(from_id);
  Serial.println(" ...");
  for (const auto& user : registered_users) {
    if (user.telegram_id == String(from_id)) {
      found = true;
      break;
    }
  }

  return found;
}

void handleResponse(int message_id, int from_id, String from_first_name, int chat_id, String text) {
  // TODO: should we log messages from unregistered users?
  if (!authenticateUser(from_id)) {
    sendResponse(chat_id, "Bem%20vindo%20ao%20Mate%20Hackers.%20Este%20BOT%20%C3%A9%20para%20uso%20exclusivo%20de%20membros%20registrados.%20Para%20mais%20informa%C3%A7%C3%B5es%2C%20entre%20em%20contato%20conosco%20atrav%C3%A9s%20do%20nosso%20canal%20do%20Telegram%3A%20https%3A%2F%2Ft.me%2Fmatehackerspoa");
    return;
  }
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
  // TODO: Add a default response listing available commands
}

String readUsersFromSheets() {
  String sheetsURL = "https://script.google.com" +  String("/macros/s/") + ReadUsersGScriptId + "/exec";
  String users = httpGETRequest(sheetsURL, 5, true);
  String token;
  int start = 0;
  int end = users.indexOf(",");
  std::vector<String> tokens;
  while (end != -1) {
    token = users.substring(start, end);
    tokens.push_back(token);
    start = end + 1;
    end = users.indexOf(',', start);
  }

  // Add the last token (after the last comma) to the vector
  if (start < users.length()) {
      token = users.substring(start);
      tokens.push_back(token);
  }

  // Create an array of User objects from the tokens
  std::vector<User> userArray;
  for (size_t i = 0; i < tokens.size(); i += 4) {
      User user;
      user.id = tokens[i].toInt();
      user.telegram_id = tokens[i + 1];
      user.name = tokens[i + 2];
      user.user_type = getUserType(tokens[i + 3]);
      userArray.push_back(user);
  }
  registered_users = userArray;
  printFoundUsers(userArray);
  return users;
}

void printFoundUsers(std::vector<User> userArray) {
  // Output the user information through the serial port
  for (const auto& user : userArray) {
    Serial.print("ID: ");
    Serial.print(user.id);
    Serial.print(", Telegram ID: ");
    Serial.print(user.telegram_id);
    Serial.print(", Name: ");
    Serial.print(user.name);
    Serial.print(", User Type: ");
    switch (user.user_type) {
      case JEDI:
        Serial.print("Jedi");
        break;
      case PADAWAN:
        Serial.print("Padawan");
        break;
      case SITH:
        Serial.print("Sith");
        break;
      default:
        Serial.print("Youngling");
        break;
    }
    Serial.println();
  }
}

void logCommandToSheets(int message_id, int from_id, String from_first_name, String command) {
  String sheetsURL = "https://script.google.com" +  String("/macros/s/") + PostGScriptId + "/exec";
  
  httpPOSTRequestJson(sheetsURL, message_id, from_id, from_first_name, command);
}

void sendResponse(int chat_id, String message) {
  String sendResponseEndpoint = baseUrl + "/sendMessage?chat_id=" + chat_id + "&text=" + message;
  String stringPayload = httpPOSTRequest(sendResponseEndpoint);
  Serial.println(stringPayload);
  JSONVar objPayload = JSON.parse(stringPayload);
}

JSONVar getUpdates(int last_update_id) {
  int updatesOffset = last_update_id + 1;
  String getUpdatesEndpoint = baseUrl + "/getUpdates?offset=" + updatesOffset + "&timeout=" + polling_seconds;
  String stringPayload = httpGETRequest(getUpdatesEndpoint, polling_seconds, false);
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

String httpGETRequest(String requestURL, int timeoutSeconds, bool followRedirects) {
  WiFiClientSecure client;
  HTTPClient https;

  client.setInsecure();

  Serial.print("Setting timeout to ");
  Serial.print(timeoutSeconds * 1000);
  Serial.println(" millisseconds.");
  https.setTimeout(timeoutSeconds * 1000);

  if (followRedirects) {
    https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  }

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


