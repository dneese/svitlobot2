#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

// Адреси для зберігання даних в EEPROM
const int SSID_ADDR = 0;
const int PASSWORD_ADDR = 32;
const int CHANNEL_KEY_ADDR = 64;
const int AP_PASSWORD_ADDR = 128; // Нова адреса для зберігання паролю точки доступу
const int BOOT_COUNT_ADDR = 160; // Адреса для зберігання лічильника завантажень
const int EEPROM_SIZE = 512;

// Стандартні SSID та пароль для точки доступу
const char* DEFAULT_AP_SSID = "svitlobot";
const char* DEFAULT_AP_PASSWORD = "12345677";

WiFiClient wifiClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
ESP8266WebServer server(80);

unsigned long apStartTime = 0;
unsigned long bootTime = 0;
unsigned long lastPingTime = 0; // Змінна для зберігання часу останнього пінгу
const unsigned long pingInterval = 90000; // Інтервал пінгу в мілісекундах (1 хвилина 30 секунд)

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  bootTime = millis();
  checkBootCount();
  setupWiFi();
  setupTimeClient();
  setupWebServer();
  setupLED();
}

void loop() {
  server.handleClient();
  handleWiFiConnection();
  updateLEDStatus();
  disableAPAfterTimeout();
  resetBootCountAfterTimeout();
}

// Налаштування WiFi
void setupWiFi() {
  WiFi.begin(readStringFromEEPROM(SSID_ADDR).c_str(), readStringFromEEPROM(PASSWORD_ADDR).c_str());
  if (WiFi.status() != WL_CONNECTED) {
    createAccessPoint();
    apStartTime = millis();
  }
}

// Налаштування клієнта часу
void setupTimeClient() {
  timeClient.begin();
}

// Налаштування веб-сервера
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/clear", handleClearEEPROM);
  server.on("/last-ping-time", handleLastPingTime);
  server.begin();
}

// Налаштування LED
void setupLED() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

// Обробка підключення до WiFi
void handleWiFiConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    unsigned long currentTime = millis();
    if (currentTime - lastPingTime >= pingInterval) {  // Перевірка інтервалу пінгу
      makeHTTPRequest();
      lastPingTime = currentTime; // Оновлення часу останнього пінгу
    }
  }
}

// Оновлення статусу LED
void updateLEDStatus() {
  static unsigned long lastTime = 0;
  unsigned long currentTime = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (currentTime - lastTime >= 3000) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      lastTime = currentTime;
    }
  } else {
    if (currentTime - lastTime >= 100) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      lastTime = currentTime;
    }
  }
}

// Створення точки доступу
void createAccessPoint() {
  String apPassword = readStringFromEEPROM(AP_PASSWORD_ADDR);
  if (apPassword.length() < 8) {
    apPassword = DEFAULT_AP_PASSWORD;
  }
  WiFi.softAP(DEFAULT_AP_SSID, apPassword.c_str());
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("Access point created. IP address: ");
  Serial.println(myIP);
}

// Обробка кореневого шляху
void handleRoot() {
  String html = generateHTML();
  server.send(200, "text/html", html);
}

// HTML-сторінка
String generateHTML() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='uk'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Конфігурація WiFi</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f0f0; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }";
  html += ".container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); width: 90%; max-width: 400px; }";
  html += "h1 { text-align: center; color: #333; margin-bottom: 20px; }";
  html += "form { display: flex; flex-direction: column; }";
  html += "label { margin-bottom: 5px; color: #555; }";
  html += "input { padding: 10px; margin-bottom: 15px; border: 1px solid #ccc; border-radius: 4px; }";
  html += "button { padding: 10px; background-color: #28a745; color: #fff; border: none; border-radius: 4px; cursor: pointer; width: 100%; }";
  html += "button:hover { background-color: #218838; }";
  html += ".clear-button { background-color: #dc3545; margin-top: 10px; }";
  html += ".clear-button:hover { background-color: #c82333; }";
  html += ".diagnostic { margin-bottom: 20px; }";
  html += ".diagnostic p { margin: 5px 0; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>Конфігурація WiFi</h1>";
  html += "<div class='diagnostic'>";
  html += "<p>Збережений SSID: " + readStringFromEEPROM(SSID_ADDR) + "</p>";
  //html += "<p>Збережений пароль: " + readStringFromEEPROM(PASSWORD_ADDR) + "</p>";
  html += "<p>Збережений ключ каналу: " + readStringFromEEPROM(CHANNEL_KEY_ADDR) + "</p>";
 // html += "<p>Збережений пароль точки доступу: " + readStringFromEEPROM(AP_PASSWORD_ADDR) + "</p>";
  html += "<p id='last-ping-time'></p>"; // додаємо ID до абзацу
  html += "</div>";
  html += "<form action='/save' method='POST'>";
  html += "<label for='ssid'>SSID:</label>";
  html += "<input type='text' id='ssid' name='ssid' required>";
  html += "<label for='password'>Password:</label>";
  html += "<input type='password' id='password' name='password' required>";
  html += "<label for='channel_key'>Channel Key:</label>";
  html += "<input type='text' id='channel_key' name='channel_key' required>";
  html += "<label for='ap_password'>Access Point Password:</label>";
  html += "<input type='password' id='ap_password' name='ap_password' required>";
  html += "<button type='submit'>Зберегти</button>";
  html += "</form>";
  html += "<button onclick='confirmClear()' class='clear-button'>Скидання налаштувань і перезавантаження</button>";
  html += "</div>";
  html += "<script>";
  html += "function confirmClear() {";
  html += "if (confirm('Ви впевнені, що хочете очистити EEPROM?')) {";
  html += "fetch('/clear', {method: 'POST'})";
  html += ".then(response => response.text())";
  html += ".then(data => alert(data));";
  html += "}";
  html += "}";
  html += "setInterval(function() {";
  html += "fetch('/last-ping-time')";
  html += ".then(response => response.text())";
  html += ".then(time => document.getElementById('last-ping-time').innerHTML = 'Час з останнього пінгу: ' + time + ' сек');";
  html += "}, 1000);";
  html += "</script>";
  html += "</html>";
  return html;
}

// Обробка збереження налаштувань
void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String channelKey = server.arg("channel_key");
  String apPassword = server.arg("ap_password");

  if (ssid.length() == 0 || password.length() == 0 || channelKey.length() == 0 || apPassword.length() == 0) {
    server.send(400, "text/plain", "All fields are required.");
    return;
  }

  if (ssid.length() > 32 || password.length() > 32 || channelKey.length() > 32 || apPassword.length() > 32) {
    server.send(400, "text/plain", "Input too long.");
    return;
  }

  writeStringToEEPROM(SSID_ADDR, ssid);
  writeStringToEEPROM(PASSWORD_ADDR, password);
  writeStringToEEPROM(CHANNEL_KEY_ADDR, channelKey);
  writeStringToEEPROM(AP_PASSWORD_ADDR, apPassword);

  EEPROM.commit();

  server.send(200, "text/plain", "Settings saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

// Обробка очищення EEPROM
void handleClearEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  server.send(200, "text/plain", "EEPROM cleared. Rebooting...");
  delay(1000);
  ESP.restart();
}

// Виконання HTTP-запиту
void makeHTTPRequest() {
  // Перевірка статусу підключення до Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot make HTTP request.");
    return;
  }

  WiFiClient client;
  if (client.connect("api.svitlobot.in.ua", 80)) {
    client.print("GET /channelPing?channel_key=" + readStringFromEEPROM(CHANNEL_KEY_ADDR) + " HTTP/1.1\r\n");
    client.print("Host: api.svitlobot.in.ua\r\n");
    client.print("Connection: close\r\n\r\n");

    while (client.connected() && !client.available()) delay(1);
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    client.stop();

    // Оновлюємо час останнього пінгу
    lastPingTime = millis();
  } else {
    Serial.println("HTTP request error");
  }
}

// Читання рядка з EEPROM
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

// Запис рядка в EEPROM
void writeStringToEEPROM(int addr, String data) {
  for (int i = 0; i < data.length(); i++) {
    EEPROM.write(addr + i, data[i]);
  }
  EEPROM.write(addr + data.length(), '\0');
}

// Обробка запиту на час останнього пінгу
void handleLastPingTime() {
  unsigned long currentTime = millis();
  unsigned long difference = (currentTime - lastPingTime) / 1000; // переводимо в секунди
  server.send(200, "text/plain", String(difference));
}

// Вимкнення точки доступу після таймауту
void disableAPAfterTimeout() {
  if (apStartTime > 0 && (millis() - apStartTime) > 300000) { // 600000 мс = 10 хвилин (можете змінити цей час або взагалі вимкнути цю функцію закоментувавши наступних два рядки
    server.stop(); // Зупиняємо веб-сервер                          .... якщо не потрібно закоментуйте
    WiFi.softAPdisconnect(true); // Вимикаємо точку доступу          .... якщо не потрібно закоментуйте
    apStartTime = 0;
  }
}

// Перевірка лічильника завантажень
void checkBootCount() {
  int bootCount = EEPROM.read(BOOT_COUNT_ADDR);
  bootCount++;
  EEPROM.write(BOOT_COUNT_ADDR, bootCount);
  EEPROM.commit();

  Serial.print("Boot count: ");
  Serial.println(bootCount);

  if (bootCount >= 4) {
    Serial.println("Boot count reached 4. Clearing EEPROM and rebooting...");
    handleClearEEPROM();
  }
}

// Обнулення лічильника завантажень після 20 секунд
void resetBootCountAfterTimeout() {
  if (millis() - bootTime > 20000) { // 20000 мс = 20 секунд
    EEPROM.write(BOOT_COUNT_ADDR, 0);
    EEPROM.commit();
    //Serial.println("Boot count reset after 20 seconds.");
  }
}

//
//Тепер пінг буде виконуватися кожні 1 хвилину 30 секунд (90000 мілісекунд).
