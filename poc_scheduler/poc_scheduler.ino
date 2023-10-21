#include <Scheduler.h>
#include <Task.h>
#include <ESP8266WiFi.h>

// WiFi Login
const char* ssid = "wifi-ssid";
// WiFi Password
const char* password = "pass";

class BlinkTask : public Task {
  protected:
    void setup() {
      state = HIGH;

      pinMode(5, OUTPUT);
      digitalWrite(5, state);
    }
  
    void loop() {
      state = state == HIGH ? LOW : HIGH;
      delay(1000);
      digitalWrite(5, state);
    }
  private:
    uint8_t state;
} blink_task;

class BlinkTask2 : public Task {
  protected:
    void setup() {
      state = LOW;

      pinMode(LED_BUILTIN, OUTPUT);
      digitalWrite(LED_BUILTIN, state);
    }
  
    void loop() {
      state = state == HIGH ? LOW : HIGH;
      delay(1000);
      digitalWrite(LED_BUILTIN, state);
    }
  private:
    uint8_t state;
} blink_task_2;

class BlinkTask3 : public Task {
  protected:
    void setup() {
      state = HIGH;

      pinMode(4, OUTPUT);
      digitalWrite(4, state);
    }
  
    void loop() {
      state = state == HIGH ? LOW : HIGH;
      delay(500);
      digitalWrite(4, state);
    }
  private:
    uint8_t state;
} blink_task_3;

class TelegramTask : public Task {
  protected:
    void setup() {
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
} telegram_task;

void loop() {}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Scheduler.start(&blink_task);
  Scheduler.start(&blink_task_2);
  Scheduler.start(&blink_task_3);
  Scheduler.start(&telegram_task);
  Serial.println(F("SETUP"));
  Scheduler.begin();
}
