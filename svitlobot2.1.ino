//Інструкція з використання
//Необхідні бібліотеки
//ESP8266WiFi.h
//ESP8266HTTPClient.h
//ESP8266WebServer.h
//EEPROM.h
//NTPClient.h
//WiFiUdp.h

//Підключення
//Підключіть ESP8266 до комп'ютера та завантажте код на пристрій.
//При першому запуску ESP8266 створить точку доступу з ім'ям "svitlobot" та паролем "12345677".
//Підключіться до цієї точки доступу зі свого пристрою.
//Відкрийте веб-браузер та перейдіть за адресою: http://192.168.4.1.
//Заповніть форму налаштування, ввівши SSID вашої Wi-Fi мережі, пароль та ключ каналу.
//Натисніть "Save". Пристрій перезавантажиться та спробує підключитися до вказаної Wi-Fi мережі.

// при не можливості підключитися до вашої Wi-Fi мережі протягом 60 секунд, створить точку доступу з ім'ям "svitlobot" та паролем "12345677".
// яка буде активна 2 хвилини, після чого пристрій перезавантажиться і процесс почнеться заново.

//Очистка пам'яті EEPROM
//Підключіться до точки доступу "svitlobot".
//Відкрийте веб-браузер та перейдіть за адресою: http://192.168.4.1.
//Натисніть кнопку "Clear EEPROM" для очищення пам'яті.
//Пристрій перезавантажиться та скине всі налаштування.

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Адреса для пінгування
String url = "http://api.svitlobot.in.ua/channelPing?channel_key=";

// Веб-сервер на 80 порті
ESP8266WebServer server(80);

// NTP-клієнт для отримання часу
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// Змінні для зберігання параметрів
String ssid;
String password;
String channel_key;

// Адреси в EEPROM для зберігання параметрів
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASSWORD_ADDR 32
#define CHANNEL_KEY_ADDR 64

// Таймер для відстеження бездіяльності
unsigned long inactivityTimer = 0;
#define INACTIVITY_TIMEOUT 120000 // 2 хвилини в мілісекундах

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  // Зчитування параметрів з EEPROM
  ssid = readStringFromEEPROM(SSID_ADDR);
  password = readStringFromEEPROM(PASSWORD_ADDR);
  channel_key = readStringFromEEPROM(CHANNEL_KEY_ADDR);

  if (ssid.length() > 0 && password.length() > 0 && channel_key.length() > 0) {
    // Якщо параметри є, підключаємося до Wi-Fi та починаємо пінгування
    if (!connectToWiFi(ssid, password)) {
      createAccessPoint();
      startWebServer();
      inactivityTimer = millis(); // Початок відліку часу бездіяльності
    } else {
      timeClient.begin();
    }
  } else {
    // Якщо параметрів немає, запускаємо веб-сервер для налаштування
    createAccessPoint();
    startWebServer();
    inactivityTimer = millis(); // Початок відліку часу бездіяльності
  }

  server.on("/clear", HTTP_POST, handleClearEEPROM);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();

    if (timeClient.getMinutes() % 1 == 0 && timeClient.getSeconds() == 0) {
      makeHTTPRequest();
      delay(1000);
    }
  } else {
    server.handleClient();
    checkInactivity();
  }
}

void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("HTTP server started at http://192.168.4.1");
}

void handleRoot() {
  inactivityTimer = millis(); // Скидання таймера при активності
  String html = "<!DOCTYPE html>";
  html += "<html lang='en'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>WiFi Configuration</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f0f0; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }";
  html += ".container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); width: 90%; max-width: 400px; }";
  html += "h1 { text-align: center; color: #333; }";
  html += "form { display: flex; flex-direction: column; }";
  html += "label { margin-bottom: 5px; color: #555; }";
  html += "input { padding: 10px; margin-bottom: 15px; border: 1px solid #ccc; border-radius: 4px; }";
  html += "button { padding: 10px; background-color: #28a745; color: #fff; border: none; border-radius: 4px; cursor: pointer; }";
  html += "button:hover { background-color: #218838; }";
  html += ".clear-button { background-color: #dc3545; }";
  html += ".clear-button:hover { background-color: #c82333; }";
  html += ".diagnostic { margin-bottom: 20px; color: #333; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>WiFi Configuration</h1>";
  html += "<div class='diagnostic'>";
  html += "<p>Saved SSID: " + readStringFromEEPROM(SSID_ADDR) + "</p>";
  html += "<p>Saved Password: " + readStringFromEEPROM(PASSWORD_ADDR) + "</p>";
  html += "<p>Saved Channel Key: " + readStringFromEEPROM(CHANNEL_KEY_ADDR) + "</p>";
  html += "</div>";
  html += "<form action='/save' method='POST'>";
  html += "<label for='ssid'>SSID:</label>";
  html += "<input type='text' id='ssid' name='ssid' required>";
  html += "<label for='password'>Password:</label>";
  html += "<input type='password' id='password' name='password' required>";
  html += "<label for='channel_key'>Channel Key:</label>";
  html += "<input type='text' id='channel_key' name='channel_key' required>";
  html += "<button type='submit'>Save</button>";
  html += "</form>";
  html += "<form action='/clear' method='POST'>";
  html += "<button type='submit' class='clear-button'>Clear EEPROM</button>";
  html += "</form>";
  html += "</div>";
  html += "</body>";
  html += "</html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  inactivityTimer = millis(); // Скидання таймера при активності
  ssid = server.arg("ssid");
  password = server.arg("password");
  channel_key = server.arg("channel_key");

  writeStringToEEPROM(SSID_ADDR, ssid);
  writeStringToEEPROM(PASSWORD_ADDR, password);
  writeStringToEEPROM(CHANNEL_KEY_ADDR, channel_key);

  EEPROM.commit();

  server.send(200, "text/plain", "Settings saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

bool connectToWiFi(String ssid, String password) {
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to Wi-Fi...");
    if (millis() - startTime > 59000) {
      Serial.println("Failed to connect to Wi-Fi within 59 seconds.");
      return false;
    }
  }
  Serial.println("Connected to Wi-Fi");
  WiFi.softAPdisconnect(true); // Закриваємо точку доступу після успішного підключення
  return true;
}

void createAccessPoint() {
  WiFi.softAP("svitlobot", "12345677"); // Додаємо пароль "12345677"
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("Access point created. IP address: ");
  Serial.println(myIP);
}

void makeHTTPRequest() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, url + channel_key);

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("HTTP GET response:");
      Serial.println(payload);
    } else {
      Serial.println("HTTP request error");
    }
    http.end();
  }
}

void checkInactivity() {
  if (millis() - inactivityTimer > INACTIVITY_TIMEOUT) {
    Serial.println("Inactivity timeout. Shutting down AP and restarting.");
    server.stop();
    WiFi.softAPdisconnect(true);
    ESP.restart();
  }
}

void handleClearEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  server.send(200, "text/plain", "EEPROM cleared. Rebooting...");
  delay(1000);
  ESP.restart();
}

String readStringFromEEPROM(int addr) {
  String data = "";
  char ch = EEPROM.read(addr);
  while (ch != '\0' && addr < EEPROM_SIZE) {
    data += ch;
    addr++;
    ch = EEPROM.read(addr);
  }
  return data;
}

void writeStringToEEPROM(int addr, String data) {
  for (int i = 0; i < data.length(); i++) {
    EEPROM.write(addr + i, data[i]);
  }
  EEPROM.write(addr + data.length(), '\0'); // Додаємо нульовий символ в кінець рядка
}
