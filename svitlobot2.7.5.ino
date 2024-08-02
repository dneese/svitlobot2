
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
const int TOKEN_ADDR = 192; // Адреса для зберігання токену
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

unsigned long lastWiFiCheckTime = 0;
const unsigned long wifiCheckInterval = 30000; // Інтервал перевірки WiFi в мілісекундах (30 секунд)

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
  checkWiFiConnection(); // Додаємо перевірку WiFi
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
  server.on("/help", handleHelp); // Додаємо обробник для сторінки довідки
  server.on("/scan", handleWiFiScan); // Додаємо обробник для сканування WiFi
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
      makeHcPingRequest(); // Виконуємо запит на hc-ping.com
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
  Serial.print("Створено точку доступу. IP-адреса: ");
  Serial.println(myIP);
}

// Обробка кореневого шляху
void handleRoot() {
  String html = generateHTML();
  server.send(200, "text/html", html);
}

// HTML-сторінка
String generateHTML() {
  String token = readStringFromEEPROM(TOKEN_ADDR);
  String html = "<!DOCTYPE html>";
  html += "<html lang='uk'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Світлобот</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; color: #fff; }";
  html += ".container { background-color: #333; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(255, 255, 255, 0.1); width: 90%; max-width: 400px; }";
  html += "h1 { text-align: center; color: #fff; margin-bottom: 20px; font-size: 1.5em; }";
  html += ".button-container { display: flex; flex-direction: column; align-items: center; }";
  html += "button, .button { padding: 10px; background-color: #28a745; color: #fff; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 0.9em; text-align: center; text-decoration: none; margin-bottom: 10px; }";
  html += "button:hover, .button:hover { background-color: #218838; }";
  html += ".clear-button { background-color: #dc3545; margin-top: 10px; font-size: 0.9em; }";
  html += ".clear-button:hover { background-color: #c82333; }";
  html += ".diagnostic { margin-bottom: 20px; }";
  html += ".diagnostic p { margin: 5px 0; font-size: 0.8em; color: #ccc; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>Світлобот</h1>";
  html += "<div class='diagnostic'>";
  html += "<p>Збережений SSID: " + readStringFromEEPROM(SSID_ADDR) + "</p>";
  html += "<p>Збережений ключ svitlobot: " + readStringFromEEPROM(CHANNEL_KEY_ADDR) + "</p>";
  html += "<p>Збережений токен hc-ping.com: " + readStringFromEEPROM(TOKEN_ADDR) + "</p>";
  html += "<p id='last-ping-time'></p>"; // додаємо ID до абзацу
  html += "<p>Статус WiFi: " + String(WiFi.status() == WL_CONNECTED ? "✅ Підключено" : "❌ Не підключено") + "</p>";
  html += "<p>Статус Інтернету: " + String(WiFi.status() == WL_CONNECTED && checkInternetConnection() ? "✅ Доступний" : "❌ Недоступний") + "</p>";
  //html += "<p><a href='https://hc-ping.com/" + token + "'>hc-ping.com</a></p>"; // Додаємо посилання з токеном
  html += "</div>";
  html += "<div class='button-container'>";
  html += "<a href='/scan' class='button'>Налаштування WiFi</a>"; // Кнопка для налаштування WiFi
  html += "<a href='/help' class='button'>Довідка та інструкція</a>"; // Кнопка для довідки та інструкції
  html += "<button onclick='confirmClear()' class='clear-button'>Скидання налаштувань і перезавантаження</button>";
  html += "</div>";
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
  String token = server.arg("token"); // Отримуємо токен з форми

  if (ssid.length() == 0 || password.length() == 0 || channelKey.length() == 0 || token.length() == 0) {
    server.send(400, "text/plain", "SSID, Password, Channel Key, and Token are required.");
    return;
  }

  if (ssid.length() > 32 || password.length() > 32 || channelKey.length() > 32 || apPassword.length() > 32 || token.length() > 64) {
    server.send(400, "text/plain", "Input too long.");
    return;
  }

  writeStringToEEPROM(SSID_ADDR, ssid);
  writeStringToEEPROM(PASSWORD_ADDR, password);
  writeStringToEEPROM(CHANNEL_KEY_ADDR, channelKey);
  writeStringToEEPROM(AP_PASSWORD_ADDR, apPassword);
  writeStringToEEPROM(TOKEN_ADDR, token); // Зберігаємо токен

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
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi не підключено, неможливо виконати HTTP-запит.");
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

    lastPingTime = millis();
  } else {
    Serial.println("Помилка HTTP-запиту");
  }
}

// Виконання запиту на hc-ping.com
void makeHcPingRequest() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi не підключено, неможливо виконати запит на hc-ping.com.");
    return;
  }

  String token = readStringFromEEPROM(TOKEN_ADDR);
  if (token.length() == 0) {
    Serial.println("Токен не встановлено.");
    return;
  }

  WiFiClient client;
  if (client.connect("hc-ping.com", 80)) {
    client.print("GET /" + token + " HTTP/1.1\r\n");
    client.print("Host: hc-ping.com\r\n");
    client.print("Connection: close\r\n\r\n");

    while (client.connected() && !client.available()) delay(1);
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    client.stop();
  } else {
    Serial.println("Помилка запиту на hc-ping.com");
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
  if (apStartTime > 0 && (millis() - apStartTime) > 900000) { // 600000 мс = 10 хвилин
    server.stop();
    WiFi.softAPdisconnect(true);
    apStartTime = 0;
  }
}

// Перевірка лічильника завантажень
void checkBootCount() {
  int bootCount = EEPROM.read(BOOT_COUNT_ADDR);
  bootCount++;
  EEPROM.write(BOOT_COUNT_ADDR, bootCount);
  EEPROM.commit();

  Serial.print("Кількість завантажень: ");
  Serial.println(bootCount);

  if (bootCount >= 4) {
    Serial.println("Досягнуто 4 завантажень. Очищення EEPROM та перезавантаження...");
    handleClearEEPROM();
  }
}

// Обнулення лічильника завантажень після 20 секунд
void resetBootCountAfterTimeout() {
  if (millis() - bootTime > 20000) { // 20000 мс = 20 секунд
    EEPROM.write(BOOT_COUNT_ADDR, 0);
    EEPROM.commit();
  }
}

// Перевірка підключення до WiFi
void checkWiFiConnection() {
  unsigned long currentTime = millis();
  if (currentTime - lastWiFiCheckTime >= wifiCheckInterval) {
    if (WiFi.status() != WL_CONNECTED) {
      reconnectWiFi();
    }
    lastWiFiCheckTime = currentTime;
  }
}

// Перепідключення до WiFi
void reconnectWiFi() {
  Serial.println("WiFi відключено. Перепідключення...");
  WiFi.disconnect();
  WiFi.begin(readStringFromEEPROM(SSID_ADDR).c_str(), readStringFromEEPROM(PASSWORD_ADDR).c_str());
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) { // 20 секунд на спробу підключення
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi перепідключено");
  } else {
    Serial.println("\nНе вдалося перепідключитися до WiFi");
    createAccessPoint();
    apStartTime = millis();
  }
}

// Обробка сторінки довідки
void handleHelp() {
  String html = generateHelpHTML();
  server.send(200, "text/html", html);
}

// HTML-сторінка довідки
String generateHelpHTML() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='uk'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Довідка та інструкція</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; color: #fff; }";
  html += ".container { background-color: #333; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(255, 255, 255, 0.1); width: 90%; max-width: 600px; }";
  html += "h1 { text-align: center; color: #fff; margin-bottom: 20px; font-size: 1.5em; }";
  html += "p { margin: 10px 0; font-size: 0.9em; color: #ccc; }";
  html += "a { color: #28a745; text-decoration: none; }";
  html += "a:hover { text-decoration: underline; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>Довідка та інструкція</h1>";
  html += "<p>Цей пристрій дозволяє пінгувати через ваше WiFi-підключення вашого світлобота.</p>";
  html += "<p>1. Введіть SSID вашої WiFi-мережі.</p>";
  html += "<p>2. Введіть пароль від вашої WiFi-мережі.</p>";
  html += "<p>3. Введіть ключ каналу для пінгу.</p>";
  html += "<p>4. Введіть токен для hc-ping.com.</p>";
  html += "<p>5. За бажанням, введіть пароль для точки доступу.</p>";
  html += "<p>6. Натисніть кнопку 'Зберегти' для збереження налаштувань.</p>";
  html += "<p>7. Ви можете скинути всі налаштування та перезавантажити пристрій, натиснувши кнопку 'Скидання налаштувань і перезавантаження'.</p>";
  html += "<p>8. При перезавантаженні 4 рази підряд, також відбувається очистка пам'яті!</p>"; // Додаємо інформацію про очистку пам'яті
  html += "<p><a href='/'>Повернутися до головної сторінки</a></p>";
  html += "</div>";
  html += "</body>";
  html += "</html>";
  return html;
}

// Сканування WiFi мереж
void handleWiFiScan() {
  int n = WiFi.scanNetworks();
  String html = "<!DOCTYPE html>";
  html += "<html lang='uk'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Налаштування WiFi</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; color: #fff; }";
  html += ".container { background-color: #333; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(255, 255, 255, 0.1); width: 90%; max-width: 400px; }";
  html += "h1 { text-align: center; color: #fff; margin-bottom: 20px; font-size: 1.5em; }";
  html += "form { display: flex; flex-direction: column; }";
  html += "label { margin-bottom: 5px; color: #fff; font-size: 0.9em; }";
  html += "input, select { padding: 10px; margin-bottom: 15px; border: 1px solid #555; border-radius: 4px; font-size: 0.9em; background-color: #444; color: #fff; width: 100%; box-sizing: border-box; }";
  html += "button { padding: 10px; background-color: #28a745; color: #fff; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 0.9em; }";
  html += "button:hover { background-color: #218838; }";
  html += ".clear-button { background-color: #dc3545; margin-top: 10px; font-size: 0.9em; }";
  html += ".clear-button:hover { background-color: #c82333; }";
  html += ".diagnostic { margin-bottom: 20px; }";
  html += ".diagnostic p { margin: 5px 0; font-size: 0.8em; color: #ccc; }";
  html += "a { color: #28a745; text-decoration: none; }";
  html += "a:hover { text-decoration: underline; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>Налаштування WiFi</h1>";
  html += "<form action='/save' method='POST'>";
  html += "<label for='ssid'>Оберіть SSID:</label>";
  html += "<select id='ssid' name='ssid' required>";
  for (int i = 0; i < n; ++i) {
    html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
  }
  html += "</select>";
  html += "<label for='password'>Password:</label>";
  html += "<input type='password' id='password' name='password'placeholder='Пароль вашої мережі wifi' required>";
  html += "<label for='channel_key'>Channel Key:</label>";
  html += "<input type='text' id='channel_key' name='channel_key'placeholder='Ключ світлобота' required>";
  html += "<label for='token'>Token для hc-ping.com:</label>";
  html += "<input type='text' id='token' name='token' placeholder='Введіть токен' required>";
  html += "<label for='ap_password'>Пароль цієї точки доступу:</label>";
  html += "<input type='password' id='ap_password' name='ap_password' placeholder='Необовязково'>";
  html += "<button type='submit'>Зберегти</button>";
  html += "</form>";
  html += "<button onclick='window.location.href=\"/\"' class='clear-button'>Повернутися до головної сторінки</button>";
  html += "</div>";
  html += "</body>";
  html += "</html>";
  server.send(200, "text/html", html);
}

// Перевірка наявності інтернету
bool checkInternetConnection() {
  WiFiClient client;
  if (client.connect("www.google.com", 80)) {
    client.stop();
    return true;
  } else {
    return false;
  }
}
