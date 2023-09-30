/*
  Guilherme Garcia <https://guilhermegarcia.dev>
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
#include <WiFiClientSecure.h>
#include <optional>
#include <algorithm>
#include <vector>

// Telegram Bot Token
const String BOT_TOKEN = "bot-token";
// WiFi Login
const char* ssid = "wifi-ssid";
// WiFi Password
const char* password = "pass";
// Google Script Deployment ID for logging commands
const char *PostGScriptId = "bot-token";
// Google Script Deployment ID for reading the list of users
const char *ReadUsersGScriptId = "bot-token";

// Base URL for bot requests
String baseUrl = "https://api.telegram.org/bot" + BOT_TOKEN;

// Holds the last receive telegram message id to avoid processing duplicates
int last_received_id = 0;
// Telegram API long polling timeout
int polling_seconds = 30;

/**
 * user_type used to authorize specific actions.
 * In the current implementation, userTypes with higher values
 * are allowed to perform all commands of userTypes lower in
 * the hierarchy, ex: A Jedi can do everything that a Padawan
 * and a Youngling could. A Padawan can do everything that a
 * Youngling could, etc.
 *
 * + `SITH` - Blocked from interacting with the bot, but there is interest in keeping attempts registered.
 * + `YOUNGLING` - User is allowed to interact with the bot, but has no permissions.
 * + `PADAWAN` - ...
 * + `JEDI` - ...
 */
typedef enum {
    SITH = -1,
    YOUNGLING = 0,
    PADAWAN = 1,
    JEDI = 2,
} userType_t;

class User {
  public:
    int id;
    String telegram_id;
    String name;
    String password;
    String salt;
    userType_t user_type;

    bool canRunCommand(const String& command) const {
      // Get the required user type for the command
      userType_t requiredType = getUserTypeRequiredForCommand(command);

      // Compare the user's type with the required type for the command
      return static_cast<int>(user_type) >= static_cast<int>(requiredType);
    }

  private:
    // Defines the command-to-user_type mapping
    userType_t getUserTypeRequiredForCommand(const String& command) const {
        if (command == "/liberar_acesso") {
          return JEDI;
        } 
        if (command == "/abrir") {
          return PADAWAN;
        }
        // Set a default case for unknown commands or commands that don't have any specific user type requirement
        return YOUNGLING;
    }
  

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

    handleResponse(message_id, from_id, from_first_name, chat_id, text);

    last_received_id = update_id;
  }
}


std::optional<User> findUser(int from_id) {
  // Check if a given telegram_id exists in the registered_users array
  Serial.print("Authenticating user with Telegram ID ");
  Serial.print(from_id);
  Serial.println(" ...");

  for (const auto& user : registered_users) {
    if (user.telegram_id == String(from_id)) {
      return std::optional<User>(user);
    }
  }

  return std::nullopt;
}

bool isCommandInList(const String& command, const std::vector<String> commandList) {
  // Use std::find to search for the command in the commandList
  // If the command is found, std::find returns an iterator pointing to the found element
  // If not found, it returns the end iterator of the vector
  // So, we can check if the iterator is not equal to end() to determine if the command is in the list
  
  auto it = std::find(commandList.begin(), commandList.end(), command);
  return it != commandList.end();
}

void handleResponse(int message_id, int from_id, String from_first_name, int chat_id, String text) {
  // TODO: should we log messages from unregistered users?
  std::optional<User> found = findUser(from_id);

  // Telegram id not in list of registered users,
  // send a default message and quit
  if (!found.has_value()) {
    sendResponse(chat_id, "Bem%20vindo%20ao%20Mate%20Hackers.%20Este%20BOT%20%C3%A9%20para%20uso%20exclusivo%20de%20membros%20registrados.%20Para%20mais%20informa%C3%A7%C3%B5es%2C%20entre%20em%20contato%20conosco%20atrav%C3%A9s%20do%20nosso%20canal%20do%20Telegram%3A%20https%3A%2F%2Ft.me%2Fmatehackerspoa");
    return;
  }

  User user = found.value();

  std::vector<String> availableCommands = { "/abrir", "/liberar_acesso" };
  String command = text;

  bool commandInList = isCommandInList(command, availableCommands);

  if (!commandInList) {
    sendResponse(chat_id, "Comando%20inv%C3%A1lido.");
    return;
  }

  if (!user.canRunCommand(command)) {
    sendResponse(chat_id, "Voc%C3%AA%20n%C3%A3o%20tem%20permiss%C3%A3o%20para%20utilizar%20esse%20comando.%20Entre%20em%20contato%20atrav%C3%A9s%20do%20chat.");
    return;
  }

  if (text == "/abrir") {
    sendResponse(chat_id, "Porta%20aberta.");
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
  for (size_t i = 0; i < tokens.size(); i += 6) {
      User user;
      user.id = tokens[i].toInt();
      user.telegram_id = tokens[i + 1];
      user.name = tokens[i + 2];
      user.user_type = getUserType(tokens[i + 3]);
      user.password = tokens[i + 4];
      user.salt = tokens[i + 5];
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
  
  String payload = "";
  String payload_base =  "{\"command\": \"insert_row\", \"sheet_name\": \"logs\", \"values\": ";
  payload = payload_base + "\"" + message_id + "," + from_id + "," + from_first_name + "," + command + "\"}";
  
  httpPostRequest(sheetsURL, payload);
}

void sendResponse(int chat_id, String message) {
  String sendResponseEndpoint = baseUrl + "/sendMessage?chat_id=" + chat_id + "&text=" + message;
  String stringPayload = httpPostRequest(sendResponseEndpoint, "");
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

String httpPostRequest(String requestURL, String payload) {
  WiFiClientSecure client;
  HTTPClient https;

  client.setInsecure();
  if (payload != "") {
    // assume we always send json
    https.addHeader("Content-Type", "application/json");
  }

  https.begin(client, requestURL);

  int httpResponseCode = https.POST(payload);

  String response = "{}";
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    response = https.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    Serial.printf("\n[HTTPS] GET... failed, error: %s\n", https.errorToString(httpResponseCode).c_str());
  }
  // Free resources
  https.end();

  return response;
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


