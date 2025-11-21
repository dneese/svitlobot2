//svitlobot2.9.ino  21.11.2025 wemos d1 mini
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h> 
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <Ticker.h> 

// --- 1. КОНСТАНТИ EEPROM ТА ГЛОБАЛЬНІ ЗМІННІ ---

// Адреси для зберігання даних в EEPROM
const int SSID_ADDR = 0;
const int PASSWORD_ADDR = 32;
const int CHANNEL_KEY_ADDR = 64;
const int AP_PASSWORD_ADDR = 128; 
const int BOOT_COUNT_ADDR = 160; 
const int TOKEN_ADDR = 192;
const int ADMIN_PASSWORD_ADDR = 256; // НОВЕ: Адреса для зберігання паролю Web-адміна
const int EEPROM_SIZE = 512;

const char* DEFAULT_AP_SSID = "svitlobot";
const char* DEFAULT_AP_PASSWORD = "";

WiFiClient wifiClient;
ESP8266WebServer server(80); 
DNSServer dnsServer;
Ticker watchdog; 

unsigned long apStartTime = 0;
unsigned long bootTime = 0;
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 90000; // 90 секунд 

unsigned long lastWiFiCheckTime = 0;
const unsigned long wifiCheckInterval = 30000; 

unsigned long lastInternetCheckTime = 0;
const unsigned long internetCheckInterval = 30000;
bool hasInternetAccess = false; 

String cachedSSID = "";
String cachedPassword = "";
String cachedChannelKey = "";
String cachedToken = "";
unsigned long lastCacheTime = 0;

String lastSvitlobotStatus = "❓";
String lastUrlStatus = "❓";
unsigned long lastSvitlobotTime = 0;
unsigned long lastUrlTime = 0;


// --- 2. ПРОТОТИПИ ФУНКЦІЙ ---

// Обробники
void handleRoot(); void handleSave(); void handleClearEEPROM(); void handlePingStatus(); void handleUptime(); 
void handleHelp(); void handleWiFiScan(); void handleScanNetworks(); void handleReboot(); void handleNotFound();
// Утиліти
String generateHTML(); String formatUptime(unsigned long milliseconds); 
String readStringFromEEPROM(int addr); void writeStringToEEPROM(int addr, String data);
bool authenticate(); // НОВЕ: Функція для аутентифікації
// Логіка
void checkBootCount(); void resetBootCountAfterTimeout(); void setupWiFi(); 
void checkWiFiConnection(); void reconnectWiFi(); void handleWiFiConnection(); 
void createAccessPoint(); bool checkInternetConnection(); void updateInternetStatus(); 
void makeHTTPRequest(); void makeHcPingRequest(); void disableAPAfterTimeout();
void setupWebServer(); void setupDNS();


// --- 3. ФУНКЦІЇ EEPROM ---

String readStringFromEEPROM(int addr) {
  unsigned long currentTime = millis();
  
  // Додаємо кешування
  if (currentTime - lastCacheTime < 60000) {
    if (addr == SSID_ADDR && cachedSSID.length() > 0) return cachedSSID;
    if (addr == PASSWORD_ADDR && cachedPassword.length() > 0) return cachedPassword;
    if (addr == CHANNEL_KEY_ADDR && cachedChannelKey.length() > 0) return cachedChannelKey;
    if (addr == TOKEN_ADDR && cachedToken.length() > 0) return cachedToken;
  }
  
  String data = "";
  int maxLen = (addr == TOKEN_ADDR) ? 200 : 64; // Довжина для токену
  
  char ch = EEPROM.read(addr);
  int i = 0;
  while (ch != '\0' && (addr + i) < EEPROM_SIZE && i < maxLen) {
    data += ch;
    i++;
    ch = EEPROM.read(addr + i);
  }
  
  // Оновлення кешу
  if (addr == SSID_ADDR) cachedSSID = data;
  if (addr == PASSWORD_ADDR) cachedPassword = data;
  if (addr == CHANNEL_KEY_ADDR) cachedChannelKey = data;
  if (addr == TOKEN_ADDR) cachedToken = data;
  lastCacheTime = currentTime;
  
  return data;
}

void writeStringToEEPROM(int addr, String data) {
  int maxLen = (addr == TOKEN_ADDR) ? 200 : 64;
  
  for (int i = 0; i < data.length() && i < maxLen; i++) {
    EEPROM.write(addr + i, data[i]);
  }
  EEPROM.write(addr + data.length(), '\0');
  EEPROM.commit();
  
  lastCacheTime = 0; // Скидаємо кеш
}

void handleClearEEPROM() {
  Serial.println(">>> Очищення EEPROM..."); 
  
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  server.send(200, "text/plain", "EEPROM cleared. Rebooting...");
  delay(1000);
  ESP.restart();
}


// --- 4. ФУНКЦІЇ ЗАВАНТАЖЕННЯ/WDT ---

void checkBootCount() {
  int bootCount = EEPROM.read(BOOT_COUNT_ADDR);
  bootCount++;
  EEPROM.write(BOOT_COUNT_ADDR, bootCount);
  EEPROM.commit();

  Serial.print("Кількість завантажень: ");
  Serial.println(bootCount); 

  if (bootCount >= 10) {
    Serial.println("Досягнуто 10 завантажень. Очищення EEPROM та перезавантаження...");
    handleClearEEPROM();
  }
}

void resetBootCountAfterTimeout() {
  static bool reset = false;
  
  if (!reset && millis() - bootTime > 20000) { 
    EEPROM.write(BOOT_COUNT_ADDR, 0);
    EEPROM.commit();
    reset = true;
    Serial.println("Лічильник завантажень скинуто."); 
  }
}


// --- 5. ФУНКЦІЇ МЕРЕЖІ ТА WIFI ---

bool checkInternetConnection() { 
  WiFiClient client;
  client.setTimeout(3000);
  if (client.connect("www.google.com", 80)) {
    client.stop();
    return true;
  } else {
    return false;
  }
}

void updateInternetStatus() {
  unsigned long currentTime = millis();
  if (WiFi.status() == WL_CONNECTED && currentTime - lastInternetCheckTime >= internetCheckInterval) {
    lastInternetCheckTime = currentTime;
    hasInternetAccess = checkInternetConnection(); 
    Serial.print("Перевірка Інтернету: ");
    Serial.println(hasInternetAccess ? "✅ OK" : "❌ Failed");
  } else if (WiFi.status() != WL_CONNECTED) {
    hasInternetAccess = false; 
  }
}

void setupWiFi() {
  String ssid = readStringFromEEPROM(SSID_ADDR);
  String pass = readStringFromEEPROM(PASSWORD_ADDR);
  
  Serial.print("Спроба підключення до WiFi: ");
  Serial.println(ssid.length() > 0 ? ssid : "N/A (Буде запущено AP)"); 
  
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  if (WiFi.status() != WL_CONNECTED) {
    createAccessPoint();
    apStartTime = millis();
    Serial.println("WiFi не підключено. Запуск режиму Точки доступу (AP)."); 
  } else {
    Serial.print("✅ WiFi підключено. IP: http://");
    Serial.println(WiFi.localIP());
  }
}

void createAccessPoint() {
  String apPassword = readStringFromEEPROM(AP_PASSWORD_ADDR);
  if (apPassword.length() < 8) {
    apPassword = DEFAULT_AP_PASSWORD;
  }
  WiFi.softAP(DEFAULT_AP_SSID, apPassword.c_str());
  Serial.print("AP SSID: ");
  Serial.println(DEFAULT_AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void reconnectWiFi() {
  Serial.println("WiFi відключено. Перепідключення..."); 
  WiFi.disconnect();
  
  String ssid = readStringFromEEPROM(SSID_ADDR);
  String pass = readStringFromEEPROM(PASSWORD_ADDR);
  
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long startTime = millis();
  
  Serial.print("Спроба підключення: ");
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) { 
    delay(500);
    Serial.print("."); 
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✅ WiFi перепідключено. IP: ");
    Serial.println(WiFi.localIP());
    if (apStartTime > 0) {
      WiFi.softAPdisconnect(true);
      apStartTime = 0;
      Serial.println("Точка доступу вимкнена.");
    }
  } else {
    Serial.println("❌ Не вдалося перепідключитися до WiFi.");
    if (apStartTime == 0) {
      createAccessPoint();
      apStartTime = millis();
    }
  }
}

void checkWiFiConnection() {
  unsigned long currentTime = millis();
  if (currentTime - lastWiFiCheckTime >= wifiCheckInterval) {
    if (WiFi.status() != WL_CONNECTED) {
      reconnectWiFi();
    }
    lastWiFiCheckTime = currentTime;
  }
}

void handleWiFiConnection() { 
  if (WiFi.status() == WL_CONNECTED) {
    unsigned long currentTime = millis();
    // Використовуємо ">=" для спрацювання при першому loop, якщо lastPingTime "обмануто" у setup()
    if (currentTime - lastPingTime >= pingInterval) {
      Serial.println("⏱️ Інтервал пінгу минув. Запуск HTTP-запитів..."); 
      makeHTTPRequest();
      makeHcPingRequest(); 
      lastPingTime = currentTime;
    }
  }
}

void disableAPAfterTimeout() {
  // 15 хвилин
  if (apStartTime > 0 && (millis() - apStartTime) > 900000) { 
    WiFi.softAPdisconnect(true);
    apStartTime = 0;
    Serial.println("Точка доступу вимкнена за тайм-аутом.");
  }
}


// --- 6. ФУНКЦІЇ HTTP-ЗАПИТІВ ---

void makeHTTPRequest() {
  String key = readStringFromEEPROM(CHANNEL_KEY_ADDR);

  if (WiFi.status() != WL_CONNECTED) {
    lastSvitlobotStatus = "❌ No WiFi";
    Serial.println("❌ WiFi не підключено для svitlobot.");
    return;
  }
  
  if (key.length() == 0) {
    lastSvitlobotStatus = "❌ No key";
    Serial.println("❌ Channel key пустий.");
    return;
  }

  Serial.println("→ Пінг svitlobot...");
  
  WiFiClient client;
  client.setTimeout(5000);
  
  if (client.connect("api.svitlobot.in.ua", 80)) {
    client.print("GET /channelPing?channel_key=" + key + " HTTP/1.1\r\n");
    client.print("Host: api.svitlobot.in.ua\r\n");
    client.print("Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.connected() && !client.available()) {
      if (millis() - timeout > 5000) {
        lastSvitlobotStatus = "❌ Timeout";
        Serial.println("⏱️ Svitlobot Timeout");
        client.stop();
        return;
      }
      delay(10);
    }
    
    if (client.available()) {
      String statusLine = client.readStringUntil('\n');
      if (statusLine.indexOf("200") > 0) {
        lastSvitlobotStatus = "✅ OK (200)";
        Serial.println("✅ Svitlobot: OK (200)");
      } else if (statusLine.indexOf("404") > 0) {
        lastSvitlobotStatus = "❌ 404";
        Serial.println("❌ Svitlobot: 404 Not Found");
      } else if (statusLine.indexOf("401") > 0) {
        lastSvitlobotStatus = "❌ 401";
        Serial.println("❌ Svitlobot: 401 Unauthorized (Check key)");
      } else {
        lastSvitlobotStatus = "⚠️ " + statusLine.substring(9, 12);
        Serial.println("⚠️ Svitlobot: Інший статус: " + statusLine.substring(9));
      }
    } else {
      lastSvitlobotStatus = "✅ OK";
      Serial.println("✅ Svitlobot: Успішне підключення, але порожня відповідь.");
    }
    client.stop();
    lastSvitlobotTime = millis();
  } else {
    lastSvitlobotStatus = "❌ Connection failed";
    Serial.println("❌ Не вдалося підключитися до api.svitlobot.in.ua");
  }
}

void makeHcPingRequest() {
  String token = readStringFromEEPROM(TOKEN_ADDR);

  if (WiFi.status() != WL_CONNECTED) {
    lastUrlStatus = "❌ No WiFi";
    return;
  }
  
  if (token.length() == 0) {
    lastUrlStatus = "❌ No URL";
    return;
  }

  Serial.println("→ Пінг Healthcheck URL...");
  
  if (token.startsWith("https://")) {
    // HTTPS ЗАПИТ
    WiFiClientSecure client;
    client.setInsecure(); 
    client.setTimeout(10000);
    
    String url = token.substring(8);
    int slashIndex = url.indexOf('/');
    String host = url.substring(0, slashIndex);
    String path = url.substring(slashIndex);
    
    if (client.connect(host.c_str(), 443)) {
      client.print("GET " + path + " HTTP/1.1\r\n");
      client.print("Host: " + host + "\r\n");
      client.print("User-Agent: ESP8266\r\n");
      client.print("Connection: close\r\n\r\n");
      
      unsigned long timeout = millis();
      while (client.connected() && !client.available()) {
        if (millis() - timeout > 10000) {
          lastUrlStatus = "❌ Timeout";
          Serial.println("⏱️ Healthcheck Timeout");
          client.stop();
          return;
        }
        delay(10);
      }
      
      if (client.available()) {
        String statusLine = client.readStringUntil('\n');
        if (statusLine.indexOf("200") > 0) {
          lastUrlStatus = "✅ OK (200)";
          Serial.println("✅ Healthcheck: OK (200)");
        } else {
          lastUrlStatus = "⚠️ " + statusLine.substring(9, 12);
          Serial.println("⚠️ Healthcheck: Інший статус: " + statusLine.substring(9));
        }
      }
      client.stop();
      lastUrlTime = millis();
    } else {
      lastUrlStatus = "❌ Connection failed";
      Serial.println("❌ Не вдалося підключитися до Healthcheck URL (HTTPS)");
    }
    
  } else if (token.startsWith("http://")) {
    // HTTP ЗАПИТ: ВИКОРИСТАННЯ HTTPClient
    WiFiClient client; 
    HTTPClient http;
    http.setTimeout(10000);
    
    http.begin(client, token); 
    
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      lastUrlStatus = "✅ OK (" + String(httpCode) + ")";
      Serial.println("✅ Healthcheck: OK (" + String(httpCode) + ")");
    } else {
      lastUrlStatus = "❌ Failed";
      Serial.println("❌ Healthcheck: Помилка HTTP GET. Код: " + String(httpCode));
    }
    http.end();
    lastUrlTime = millis();
  } else {
    lastUrlStatus = "❌ Bad URL";
    Serial.println("❌ Healthcheck: Невірний формат URL");
  }
}


// --- 7. ФУНКЦІЇ ВЕБ-СЕРВЕРА/HTML ТА АУТЕНТИФІКАЦІЯ ---

// НОВА ФУНКЦІЯ АУТЕНТИФІКАЦІЇ
bool authenticate() {
  String adminPass = readStringFromEEPROM(ADMIN_PASSWORD_ADDR);

  // Якщо пароль не встановлено, дозволяємо доступ
  if (adminPass.length() < 1) { 
    return true;
  }
  
  // Використовуємо "admin" як логін
  if (server.authenticate("admin", adminPass.c_str())) {
    return true;
  }
  
  // Аутентифікація не пройдена
  server.requestAuthentication();
  return false;
}

String formatUptime(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;

  minutes %= 60;
  hours %= 24;

  String uptimeString = "";
  if (days > 0) uptimeString += String(days) + " дн. ";
  if (hours > 0) uptimeString += String(hours) + " год. ";
  uptimeString += String(minutes) + " хв.";

  return uptimeString;
}

void handleRoot() {
  String html = generateHTML();
  server.send(200, "text/html", html);
}

// Генерація HTML для головної сторінки (Dashboard)
String generateHTML() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='uk'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Світлобот</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; color: #fff; }";
  html += ".container { background-color: #333; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(255, 255, 255, 0.1); width: 90%; max-width: 400px; }";
  html += "h1 { text-align: center; color: #fff; margin-bottom: 20px; font-size: 1.5em; position: relative; }";
  html += "h1::before { content: ''; position: absolute; top: -10px; left: -10px; right: -10px; bottom: -10px; border: 2px solid #00fafa; border-radius: 8px; animation: glow 2s infinite; }";
  html += "@keyframes glow { 0% { box-shadow: 0 0 10px #00fafa; } 50% { box-shadow: 0 0 20px #00fafa; } 100% { box-shadow: 0 0 10px #00fafa; } }";
  html += ".button-container { display: flex; flex-direction: column; align-items: center; }";
  html += "button, .button { padding: 10px; background-color: #28a745; color: #fff; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 0.9em; text-align: center; text-decoration: none; margin-bottom: 10px; display: inline-block; }";
  html += "button:hover, .button:hover { background-color: #218838; }";
  html += ".diagnostic { margin-bottom: 20px; }";
  html += ".diagnostic p { margin: 5px 0; font-size: 0.8em; color: #ccc; word-break: break-all; }";
  html += ".loader { display: flex; justify-content: center; margin-top: 20px; }";
  html += ".dot { width: 10px; height: 10px; border-radius: 50%; margin: 0 5px; animation: bounce 1.5s infinite ease-in-out; }";
  html += ".dot:nth-child(1) { background-color: #ff6384; }";
  html += ".dot:nth-child(2) { background-color: #36a2eb; animation-delay: 0.2s; }";
  html += ".dot:nth-child(3) { background-color: #ffce56; animation-delay: 0.4s; }";
  html += "@keyframes bounce { 0%, 80%, 100% { transform: scale(0); } 40% { transform: scale(1); } }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>Світлобот</h1>";
  html += "<div class='diagnostic'>";
  html += "<p>📡 SSID: " + readStringFromEEPROM(SSID_ADDR) + "</p>";
  html += "<p>🔑 Ключ: " + readStringFromEEPROM(CHANNEL_KEY_ADDR) + "</p>";
  
  String token = readStringFromEEPROM(TOKEN_ADDR);
  if (token.length() > 40) {
    html += "<p>🌐 URL: " + token.substring(0, 40) + "...</p>";
  } else {
    html += "<p>🌐 URL: " + token + "</p>";
  }
  
  html += "<p>⏱️ Наступний пінг через: <span id='countdown'>90</span> сек</p>";
  
  html += "<p>🔵 Svitlobot: <span id='svitlobot-status'>" + lastSvitlobotStatus + "</span></p>";
  html += "<p>🟢 URL ping: <span id='url-status'>" + lastUrlStatus + "</span></p>";
  
  html += "<p>📶 WiFi: " + String(WiFi.status() == WL_CONNECTED ? "✅ Підключено" : "❌ Не підключено") + "</p>";
  html += "<p>🌍 Інтернет: " + String(WiFi.status() == WL_CONNECTED && hasInternetAccess ? "✅" : "❌") + "</p>";
  html += "<p id='uptime'>⏱️ Uptime: " + formatUptime(millis()) + "</p>";
  html += "<p>📟 IP: " + WiFi.localIP().toString() + "</p>";
  
  int rssi = WiFi.RSSI();
  String signalIcon = "📶";
  if (rssi > -50) signalIcon = "📶📶📶📶📶";
  else if (rssi > -60) signalIcon = "📶📶📶📶";
  else if (rssi > -70) signalIcon = "📶📶📶";
  else if (rssi > -80) signalIcon = "📶📶";
  html += "<p>" + signalIcon + " Сигнал: " + String(rssi) + " dBm</p>";

  html += "</div>";
  html += "<div class='button-container'>";
  html += "<a href='/scan' class='button'>⚙️ Налаштування</a>";
  html += "<a href='/help' class='button' style='background-color:#007bff;'>❓ Довідка</a>";
  html += "</div>";
  html += "<div class='loader'>";
  html += "<div class='dot'></div>";
  html += "<div class='dot'></div>";
  html += "<div class='dot'></div>";
  html += "</div>";
  html += "</div>";
  
  // JavaScript
  html += "<script>";
  html += "let serverLastPing = " + String(lastPingTime) + ";"; 
  html += "let PING_INTERVAL_MS = " + String(pingInterval) + ";";
  html += "let localStartTime = Date.now();"; 
  
  html += "setInterval(function() {";
  html += "fetch('/uptime').then(r=>r.text()).then(u=>document.getElementById('uptime').innerHTML='⏱️ Uptime: '+u);";
  html += "fetch('/ping-status').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('svitlobot-status').innerHTML=d.svitlobot;";
  html += "document.getElementById('url-status').innerHTML=d.url;";
  html += "if(d.lastPing != serverLastPing) {";
  html += "serverLastPing = d.lastPing;";
  html += "localStartTime = Date.now();";
  html += "}";
  html += "});";
  html += "}, 1000);";
  
  html += "setInterval(function() {";
  html += "let elapsed = Math.floor((Date.now() - localStartTime) / 1000);";
  html += "let remaining = Math.floor((PING_INTERVAL_MS / 1000) - elapsed);";
  html += "if(remaining < 0) remaining = 0;";
  html += "if(remaining > (PING_INTERVAL_MS / 1000)) remaining = (PING_INTERVAL_MS / 1000);";
  html += "document.getElementById('countdown').textContent = remaining;";
  html += "}, 1000);";
  
  html += "</script>";
  html += "</body>";
  html += "</html>";
  return html;
}

// НОВА ЛОГІКА ЗБЕРЕЖЕННЯ: ЗБЕРЕЖЕННЯ ТІЛЬКИ НЕПУСТИХ ПОЛІВ
void handleSave() { 
  // Перевіряємо, чи всі необхідні аргументи присутні (навіть якщо вони пусті)
  if (!server.hasArg("ssid") || !server.hasArg("password") || !server.hasArg("channelKey") || !server.hasArg("token") || !server.hasArg("apPassword") || !server.hasArg("adminPassword")) {
    server.send(400, "text/plain", "Missing arguments in POST request.");
    return;
  }
  
  // 1. Читаємо поточні (старі) значення
  String oldSSID = readStringFromEEPROM(SSID_ADDR);
  String oldPassword = readStringFromEEPROM(PASSWORD_ADDR);
  String oldChannelKey = readStringFromEEPROM(CHANNEL_KEY_ADDR);
  String oldAPPassword = readStringFromEEPROM(AP_PASSWORD_ADDR);
  String oldToken = readStringFromEEPROM(TOKEN_ADDR);
  String oldAdminPassword = readStringFromEEPROM(ADMIN_PASSWORD_ADDR);

  // 2. Визначаємо нові значення: якщо нове поле пусте, залишаємо старе
  String newSSID = server.arg("ssid").length() > 0 ? server.arg("ssid") : oldSSID;
  String newPassword = server.arg("password").length() > 0 ? server.arg("password") : oldPassword;
  String newChannelKey = server.arg("channelKey").length() > 0 ? server.arg("channelKey") : oldChannelKey;
  String newAPPassword = server.arg("apPassword").length() > 0 ? server.arg("apPassword") : oldAPPassword;
  String newToken = server.arg("token").length() > 0 ? server.arg("token") : oldToken;
  String newAdminPassword = server.arg("adminPassword").length() > 0 ? server.arg("adminPassword") : oldAdminPassword;

  // 3. Валідація: SSID та Channel Key не можуть бути пустими в кінцевому результаті
  if (newSSID.length() == 0) {
    server.send(400, "text/plain", "SSID cannot be empty.");
    return;
  }
  if (newChannelKey.length() == 0) {
    server.send(400, "text/plain", "Channel Key cannot be empty.");
    return;
  }

  // 4. Перевірка довжини (тільки для тих, що були введені)
  if (server.arg("ssid").length() > 32 || server.arg("password").length() > 32 || server.arg("channelKey").length() > 32 || server.arg("apPassword").length() > 32 || server.arg("adminPassword").length() > 32 || server.arg("token").length() > 200) {
    server.send(400, "text/plain", "One of the input fields is too long.");
    return;
  }

  // 5. Зберігаємо фінальні значення
  Serial.println(">>> Збереження налаштувань. SSID: " + newSSID + ", Admin Pass Len: " + String(newAdminPassword.length()));
  
  writeStringToEEPROM(SSID_ADDR, newSSID);
  writeStringToEEPROM(PASSWORD_ADDR, newPassword);
  writeStringToEEPROM(CHANNEL_KEY_ADDR, newChannelKey);
  writeStringToEEPROM(AP_PASSWORD_ADDR, newAPPassword);
  writeStringToEEPROM(TOKEN_ADDR, newToken);
  writeStringToEEPROM(ADMIN_PASSWORD_ADDR, newAdminPassword); // НОВЕ: Зберігаємо пароль адміна

  server.send(200, "text/plain", "Settings saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

void handleWiFiScan() { // Сторінка налаштувань.
  String html = "<!DOCTYPE html>";
  html += "<html lang='uk'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Налаштування WiFi</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; color: #fff; }";
  html += ".container { background-color: #333; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(255, 255, 255, 0.1); width: 90%; max-width: 400px; }";
  html += "h1 { text-align: center; color: #fff; margin-bottom: 20px; font-size: 1.5em; }";
  html += "form { display: flex; flex-direction: column; }";
  html += "label { margin-bottom: 5px; color: #fff; font-size: 0.9em; }";
  html += "input, select { padding: 10px; margin-bottom: 15px; border: 1px solid #555; border-radius: 4px; font-size: 0.9em; background-color: #444; color: #fff; width: 100%; box-sizing: border-box; }";
  html += "button, .button { padding: 10px; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 0.9em; text-align: center; text-decoration: none; margin-bottom: 10px; }";
  html += ".green-button { background-color: #28a745; color: #fff; }";
  html += ".green-button:hover { background-color: #218838; }";
  html += ".red-button { background-color: #dc3545; color: #fff; }";
  html += ".red-button:hover { background-color: #c82333; }";
  html += ".button-row { display: flex; justify-content: space-between; margin-top: 10px; gap: 5px; }";
  html += ".button-row button { flex-grow: 1; }"; 
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>Налаштування</h1>";
  html += "<form action='/save' method='POST'>";
  html += "<label for='ssid'>Оберіть SSID:</label>";
  html += "<select id='ssid' name='ssid' required>";
  html += "<option value='" + readStringFromEEPROM(SSID_ADDR) + "'>" + readStringFromEEPROM(SSID_ADDR) + " (поточна)</option>";
  html += "<option value=''>[ОБЕРІТЬ МЕРЕЖУ]</option>";
  html += "</select>";
  
  html += "<label for='password'>Пароль WiFi:</label>";
  html += "<input type='password' id='password' name='password' value='' placeholder='Залиште пустим, щоб не змінювати'>";
  
  // АВТОЗАПОВНЕННЯ: Channel Key
  html += "<label for='channelKey'>Ключ Світлобота (32 символи):</label>";
  html += "<input type='text' id='channelKey' name='channelKey' value='" + readStringFromEEPROM(CHANNEL_KEY_ADDR) + "' placeholder='Ваш Channel Key'>";

  // АВТОЗАПОВНЕННЯ: Healthcheck URL
  html += "<label for='token'>URL Healthcheck (опціонально):</label>";
  html += "<input type='url' id='token' name='token' value='" + readStringFromEEPROM(TOKEN_ADDR) + "' placeholder='https://hc-ping.com/...' maxlength='200'>";

  html += "<label for='apPassword'>Пароль AP (мін. 8 символів):</label>";
  html += "<input type='password' id='apPassword' name='apPassword' value='' placeholder='Залиште пустим, щоб не змінювати'>";
  
  // НОВЕ ПОЛЕ: Пароль Web-адміна
  html += "<label for='adminPassword'>Пароль Web-адміна:</label>";
  html += "<input type='password' id='adminPassword' name='adminPassword' value='' placeholder='Залиште пустим, щоб не змінювати/вимкнути. Логін: admin'>";
  
  html += "<button type='submit' class='green-button'>💾 Зберегти та перезавантажити</button>";
  html += "</form>";
  
  html += "<div class='button-row'>";
  html += "<button onclick='window.location.href=\"/\"' class='green-button'>🏠 Головна</button>";
  html += "<button onclick='if(confirm(\"Ви впевнені? Це очистить ВСІ налаштування.\")) { fetch(\"/clear\"); }' class='red-button'>❌ Очистити EEPROM</button>";
  html += "<button onclick='if(confirm(\"Перезавантажити пристрій?\")) { fetch(\"/reboot\", {method: \"POST\"}); }' class='green-button'>🔄 Перезавантажити</button>";
  html += "</div>";
  
  html += "</div>";
  
  html += "<script>";
  html += "function updateNetworks() {";
  html += "  fetch('/scan-networks').then(r => r.json()).then(data => {";
  html += "    const select = document.getElementById('ssid');";
  html += "    const currentSsid = '" + readStringFromEEPROM(SSID_ADDR) + "';";
  html += "    select.innerHTML = '';";
  html += "    let currentSsidFound = false;";
  html += "    data.forEach(net => {";
  html += "      if (net.ssid === currentSsid) currentSsidFound = true;";
  html += "      const option = document.createElement('option');";
  html += "      option.value = net.ssid;";
  html += "      option.textContent = net.ssid + ' (' + net.rssi + ' dBm)';";
  html += "      if (net.ssid === currentSsid) { option.selected = true; }";
  html += "      select.appendChild(option);";
  html += "    });";
  html += "    if (!currentSsidFound && currentSsid.length > 0) {\r\n";
  html += "      const option = document.createElement('option');\r\n";
  html += "      option.value = currentSsid;\r\n";
  html += "      option.textContent = currentSsid + ' (поточна)';\r\n";
  html += "      option.selected = true;\r\n";
  html += "      select.prepend(option);\r\n";
  html += "    }\r\n";
  html += "  }).catch(error => console.error('Error fetching networks:', error));";
  html += "}";
  html += "setTimeout(updateNetworks, 500); ";
  html += "</script>";
  
  html += "</body>";
  html += "</html>";
  server.send(200, "text/html", html);
}

void handleScanNetworks() {
  Serial.println("→ Сканування WiFi мереж...");
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]";
  WiFi.scanDelete(); 
  Serial.printf("← Знайдено %d мереж.\n", n);
  server.send(200, "application/json", json);
}

void handleHelp() { 
  // (Код довідки без змін)
  String html = "<!DOCTYPE html>";
  html += "<html lang='uk'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Довідка - Світлобот</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 20px; color: #fff; }";
  html += ".container { background-color: #333; padding: 20px; border-radius: 8px; max-width: 600px; margin: 0 auto; }";
  html += "h1, h2 { color: #00fafa; }";
  html += "p, ul { font-size: 0.9em; line-height: 1.5; }";
  html += "li { margin-bottom: 10px; }";
  html += ".button { display: inline-block; padding: 10px 15px; background-color: #28a745; color: #fff; text-decoration: none; border-radius: 4px; margin-top: 20px; }";
  html += ".button:hover { background-color: #218838; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>Довідка</h1>";
  html += "<h2>Проєкт Світлобот</h2>";
  html += "<p>Цей пристрій на базі ESP8266 (Wemos D1 mini) svitlobot2.9  21.11.2025 wemos d1 mini,  призначений для відстеження наявності світла (або інтернету) та надсилання регулярних сигналів (пінгів) на сервіси:</p>";
  html += "<ul>";
  html += "<li><strong>Svitlobot:</strong> Повідомляє про статус вашої локації.</li>";
  html += "<li><strong>Healthchecks (опціонально):</strong> Сторонній сервіс для моніторингу.</li>";
  html += "</ul>";
  
  html += "<h2>Режими роботи:</h2>";
  html += "<ul>";
  html += "<li><strong>Режим AP (Точка доступу):</strong> Пристрій створює мережу <code>svitlobot</code>.</li>";
  html += "<li><strong>Режим Клієнта (WiFi):</strong> Пристрій підключений до вашого роутера.</li>";
  html += "</ul>";
  
  html += "<a href='/' class='button'>🏠 На головну</a>";
  html += "</div>";
  html += "</body>";
  html += "</html>";
  server.send(200, "text/html", html);
}

void handleReboot() {
  Serial.println(">>> Перезавантаження за запитом...");
  server.send(200, "text/plain", "Rebooting...");
  delay(1000);
  ESP.restart();
}

void handleUptime() {
  server.send(200, "text/plain", formatUptime(millis()));
}

void handlePingStatus() {
  String json = "{";
  json += "\"svitlobot\":\"" + lastSvitlobotStatus + "\",";
  json += "\"url\":\"" + lastUrlStatus + "\",";
  json += "\"lastPing\":" + String(lastPingTime);
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() { 
  server.sendHeader("Location", "http://" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "192.168.4.1"), true);
  server.send(302, "text/plain", "");
}

void setupWebServer() { 
  server.on("/", handleRoot);
  server.on("/ping-status", handlePingStatus);
  server.on("/uptime", handleUptime);
  server.on("/help", handleHelp);

  // ЗАХИЩЕНІ ОБРОБНИКИ (Protected handlers)
  // Використовуємо функцію-обгортку (lambda), щоб спочатку викликати authenticate()
  server.on("/scan", [](){ if (authenticate()) handleWiFiScan(); });
  server.on("/save", HTTP_POST, [](){ if (authenticate()) handleSave(); }); 
  server.on("/clear", [](){ if (authenticate()) handleClearEEPROM(); });
  server.on("/scan-networks", [](){ if (authenticate()) handleScanNetworks(); });
  server.on("/reboot", HTTP_POST, [](){ if (authenticate()) handleReboot(); });
  
  server.onNotFound(handleNotFound); 
  server.begin();
}

void setupDNS() {
  const byte DNS_PORT = 53;
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
}


// --- 8. SETUP & LOOP ---

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- СТАРТ СВІТЛОБОТ ESP8266 ---"); 
  
  EEPROM.begin(EEPROM_SIZE);
  bootTime = millis();
  
  checkBootCount(); 
  setupWiFi();      
  
  // НОВЕ: Налаштовуємо час останнього пінгу для МИТТЄВОГО спрацювання.
  // Встановлюємо, що останній пінг був pingInterval мс тому.
  lastPingTime = millis() - pingInterval; 

  setupWebServer(); 
  setupDNS();       

  ESP.wdtEnable(30000); 
  watchdog.attach(1, []() { ESP.wdtFeed(); }); 
  
  // Кешуємо налаштування для швидкого доступу
  readStringFromEEPROM(SSID_ADDR);
  readStringFromEEPROM(PASSWORD_ADDR);
  readStringFromEEPROM(CHANNEL_KEY_ADDR);
  readStringFromEEPROM(TOKEN_ADDR);
  readStringFromEEPROM(ADMIN_PASSWORD_ADDR); // Кешуємо пароль адміна

  Serial.println("--------------------------------");
}

void loop() {
  yield(); 

  server.handleClient();
  dnsServer.processNextRequest();
  
  updateInternetStatus();
  handleWiFiConnection(); // Тепер спрацює одразу, якщо є WiFi
  disableAPAfterTimeout();
  resetBootCountAfterTimeout();
  checkWiFiConnection(); 
}