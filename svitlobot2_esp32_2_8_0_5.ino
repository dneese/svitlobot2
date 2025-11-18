//svitlobot2_esp33_2_8_0_7 16/11/2025 healthchecks update, LED disabled fix, reverse timer, server response, optimization
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <DNSServer.h>

// Адреси EEPROM
const int SSID_ADDR = 0;
const int PASSWORD_ADDR = 64;
const int CHANNEL_KEY_ADDR = 128;
const int AP_PASSWORD_ADDR = 192;
const int BOOT_COUNT_ADDR = 256;
const int TOKEN_ADDR = 300;
const int EEPROM_SIZE = 512;

// ✅ КОНСТАНТА ДЛЯ LED: Вбудований LED на більшості ESP32
const int LED_PIN = 2; 

const char* DEFAULT_AP_SSID = "svitlobot";
const char* DEFAULT_AP_PASSWORD = "";

WebServer server(80);
DNSServer dnsServer;

unsigned long apStartTime = 0;
unsigned long bootTime = 0;
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 90000;

unsigned long lastWiFiCheckTime = 0;
const unsigned long wifiCheckInterval = 30000;

// ЗМІННІ ДЛЯ КЕШУВАННЯ СТАТУСУ ІНТЕРНЕТУ
unsigned long lastInternetCheckTime = 0;
const unsigned long internetCheckInterval = 30000; // Перевірка раз на 30 секунд
bool hasInternetAccess = false; // Кешований статус

// Кеш
String cachedSSID = "";
String cachedPassword = "";
String cachedChannelKey = "";
String cachedToken = "";
unsigned long lastCacheTime = 0;

// Статуси пінгів
String lastSvitlobotStatus = "❓";
String lastUrlStatus = "❓";
unsigned long lastSvitlobotTime = 0;
unsigned long lastUrlTime = 0;

void handleRoot();
void handleSave();
void handleClearEEPROM();
void handlePingStatus();
void handleUptime();
void handleHelp();
void handleWiFiScan();
void handleScanNetworks();
void handleReboot();
String generateHTML();
String formatUptime(unsigned long milliseconds);
void createAccessPoint();
bool checkInternetConnection();
void updateInternetStatus();

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  bootTime = millis();
  
  // ✅ ВИПРАВЛЕННЯ: Встановлюємо LED в HIGH (це вимикає його на деяких платах)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // ⬅️ ЗМІНЕНО З LOW НА HIGH
  Serial.println("→ Вбудований LED вимкнено (використано LOW).");

  checkBootCount();
  setupWiFi();
  setupWebServer();
  setupDNS();
}

void loop() {
  yield();
  
  server.handleClient();
  dnsServer.processNextRequest();
  
  updateInternetStatus();
  
  handleWiFiConnection();
  disableAPAfterTimeout();
  resetBootCountAfterTimeout();
  checkWiFiConnection();
}

String readStringFromEEPROM(int addr) {
  unsigned long currentTime = millis();
  
  if (currentTime - lastCacheTime < 60000) {
    if (addr == SSID_ADDR && cachedSSID.length() > 0) return cachedSSID;
    if (addr == PASSWORD_ADDR && cachedPassword.length() > 0) return cachedPassword;
    if (addr == CHANNEL_KEY_ADDR && cachedChannelKey.length() > 0) return cachedChannelKey;
    if (addr == TOKEN_ADDR && cachedToken.length() > 0) return cachedToken;
  }
  
  String data = "";
  int maxLen = (addr == TOKEN_ADDR) ? 200 : 64;
  
  char ch = EEPROM.read(addr);
  int i = 0;
  while (ch != '\0' && (addr + i) < EEPROM_SIZE && i < maxLen) {
    data += ch;
    i++;
    ch = EEPROM.read(addr + i);
  }
  
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
  
  lastCacheTime = 0;
  
  Serial.print("Записано в EEPROM (addr ");
  Serial.print(addr);
  Serial.print("): ");
  Serial.println(data);
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

void checkBootCount() {
  int bootCount = EEPROM.read(BOOT_COUNT_ADDR);
  bootCount++;
  EEPROM.write(BOOT_COUNT_ADDR, bootCount);
  EEPROM.commit();

  Serial.print("Кількість завантажень: ");
  Serial.println(bootCount);

  if (bootCount >= 4) {
    Serial.println("Досягнуто 4 завантажень. Очищення EEPROM...");
    handleClearEEPROM();
  }
}

void resetBootCountAfterTimeout() {
  static bool reset = false;
  
  if (!reset && millis() - bootTime > 20000) {
    EEPROM.write(BOOT_COUNT_ADDR, 0);
    EEPROM.commit();
    reset = true;
  }
}

// ФУНКЦІЯ ПЕРЕВІРКИ ІНТЕРНЕТУ (БЛОКУЮЧА, ВИКЛИКАЄМО РІДКО)
bool checkInternetConnection() {
  WiFiClient client;
  client.setTimeout(3000);
  if (client.connect("www.google.com", 80)) {
    client.stop();
    return true;
  }
  return false;
}

// ОНОВЛЕННЯ СТАТУСУ ІНТЕРНЕТУ
void updateInternetStatus() {
  unsigned long currentTime = millis();
  if (WiFi.status() == WL_CONNECTED && currentTime - lastInternetCheckTime >= internetCheckInterval) {
    lastInternetCheckTime = currentTime;
    hasInternetAccess = checkInternetConnection(); 
    Serial.print("→ Перевірка Інтернету: ");
    Serial.println(hasInternetAccess ? "✅" : "❌");
  } else if (WiFi.status() != WL_CONNECTED) {
    hasInternetAccess = false; 
  }
}

void handleRoot() {
  String html = generateHTML();
  server.send(200, "text/html", html);
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
  html += ".green-text { color: green; }";
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
  
  // Зворотній відлік
  html += "<p>⏱️ Наступний пінг через: <span id='countdown'>90</span> сек</p>";
  
  // Статуси пінгів
  html += "<p>🔵 Svitlobot: <span id='svitlobot-status'>" + lastSvitlobotStatus + "</span></p>";
  html += "<p>🟢 URL ping: <span id='url-status'>" + lastUrlStatus + "</span></p>";
  
  html += "<p>📶 WiFi: " + String(WiFi.status() == WL_CONNECTED ? "✅ Підключено" : "❌ Не підключено") + "</p>";
  // ВИКОРИСТАННЯ КЕШУ: Оптимізація
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
  html += "<a href='/help' class='green-text'>Довідка</a>";
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
  html += "let localStartTime = Date.now();"; 
  
  // Оновлення статусів кожну секунду
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
  
  // Відлік кожну секунду
  html += "setInterval(function() {";
  html += "let elapsed = Math.floor((Date.now() - localStartTime) / 1000);";
  html += "let remaining = 90 - elapsed;";
  html += "if(remaining < 0) remaining = 0;";
  html += "if(remaining > 90) remaining = 90;";
  html += "document.getElementById('countdown').textContent = remaining;";
  html += "}, 1000);";
  
  html += "</script>";
  html += "</body>";
  html += "</html>";
  return html;
}


void handleWiFiScan() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='uk'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Налаштування</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; color: #fff; }";
  html += ".container { background-color: #333; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(255, 255, 255, 0.1); width: 90%; max-width: 400px; }";
  html += "h1 { text-align: center; color: #fff; margin-bottom: 20px; font-size: 1.5em; }";
  html += "form { display: flex; flex-direction: column; }";
  html += "label { margin-bottom: 5px; color: #fff; font-size: 0.9em; }";
  html += "input, select { padding: 10px; margin-bottom: 15px; border: 1px solid #555; border-radius: 4px; font-size: 0.9em; background-color: #444; color: #fff; width: 100%; box-sizing: border-box; }";
  html += "button { padding: 10px; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 0.9em; }";
  html += ".green-button { background-color: #28a745; color: #fff; }";
  html += ".green-button:hover { background-color: #218838; }";
  html += ".red-button { background-color: #dc3545; color: #fff; }";
  html += ".red-button:hover { background-color: #c82333; }";
  html += ".button-row { display: flex; justify-content: space-between; margin-top: 10px; }";
  html += ".button-row button { width: 30%; }"; 
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>Налаштування</h1>";
  html += "<form action='/save' method='POST'>";
  html += "<label for='ssid'>Оберіть SSID:</label>";
  html += "<select id='ssid' name='ssid' required>";
  html += "<option value=''>Оберіть мережу</option>";
  html += "</select>";
  html += "<button type='button' onclick='scanNetworks()'>Сканувати мережі</button>";
  html += "<label for='password'>Password:</label>";
  html += "<input type='password' id='password' name='password' placeholder='Пароль WiFi'>";
  html += "<label for='channel_key'>Channel Key:</label>";
  html += "<input type='text' id='channel_key' name='channel_key' placeholder='Ключ світлобота' required>";
  
  html += "<label for='token'>URL для пінгу:</label>";
  html += "<input type='text' id='token' name='token' placeholder='https://hc-ping.com/UUID' style='font-size:0.75em;'>";
  
  html += "<label for='ap_password'>Пароль AP:</label>";
  html += "<input type='password' id='ap_password' name='ap_password' placeholder='Необовʼязково'>";
  html += "<button type='submit' class='green-button'>Зберегти</button>";
  html += "</form>";
  html += "<div class='button-row'>";
  html += "<button onclick='location.href=\"/\"' class='green-button'>Головна</button>";
  html += "<button onclick='confirmClear()' class='red-button'>Скидання</button>";
  html += "<button onclick='rebootDevice()' class='red-button'>Reboot</button>"; 
  html += "</div>";
  html += "</div>";
  html += "<script>";
  html += "function confirmClear(){if(confirm('Очистити EEPROM?'))fetch('/clear',{method:'POST'}).then(r=>r.text()).then(d=>alert(d))}";
  html += "function rebootDevice(){if(confirm('Перезавантажити?'))fetch('/reboot',{method:'POST'}).then(r=>r.text()).then(d=>alert(d))}"; 
  html += "function scanNetworks(){fetch('/scan-networks').then(r=>r.json()).then(d=>{var s=document.getElementById('ssid');s.innerHTML='';d.forEach(n=>{var o=document.createElement('option');o.value=n.SSID;o.text=n.SSID+' ('+n.SignalStrength+' dBm)';s.appendChild(o)})}).catch(e=>alert('Помилка сканування'))}";
  html += "</script></body></html>";
  
  server.send(200, "text/html", html);
}


void handleScanNetworks() {
  int n = WiFi.scanNetworks();
  if (n == 0) {
    server.send(200, "application/json", "[]");
    return;
  }

  int indices[n];
  for (int i = 0; i < n; i++) indices[i] = i;

  for (int i = 0; i < n - 1; i++) {
    for (int j = 0; j < n - i - 1; j++) {
      if (WiFi.RSSI(indices[j]) < WiFi.RSSI(indices[j + 1])) {
        int temp = indices[j];
        indices[j] = indices[j + 1];
        indices[j + 1] = temp;
      }
    }
  }

  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{\"SSID\":\"" + WiFi.SSID(indices[i]) + "\",";
    json += "\"SignalStrength\":" + String(WiFi.RSSI(indices[i])) + "}";
  }
  json += "]";

  server.send(200, "application/json", json);
}

void handleHelp() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='uk'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Довідка</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; color: #fff; }";
  html += ".container { background-color: #333; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(255, 255, 255, 0.1); width: 90%; max-width: 600px; }";
  html += "h1 { text-align: center; color: #fff; margin-bottom: 20px; font-size: 1.5em; }";
  html += "p { margin: 10px 0; font-size: 0.9em; color: #ccc; }";
  html += "a { color: #28a745; text-decoration: none; }";
  html += "a:hover { text-decoration: underline; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>Довідка</h1>";
  html += "<p>Пристрій для пінгування світлобота через WiFi.</p>";
  html += "<p>1. Введіть SSID WiFi-мережі</p>";
  html += "<p>2. Введіть пароль WiFi</p>";
  html += "<p>3. Введіть ключ каналу</p>";
  html += "<p>4. Введіть URL для пінгу (наприклад: https://hc-ping.com/UUID)</p>";
  html += "<p>5. За бажанням - пароль AP</p>";
  html += "<p>6. При 4 перезавантаженнях підряд - очистка памʼяті</p>";
  html += "<p><a href='https://dneese.github.io/svitlobot2/'>dneese.github.io/svitlobot2</a></p>";
  html += "<p><a href='/'>Повернутися</a></p>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String channelKey = server.arg("channel_key");
  String apPassword = server.arg("ap_password");
  String token = server.arg("token");
  
  if (ssid.length() == 0 || channelKey.length() == 0) {
    server.send(400, "text/plain", "SSID and Channel Key required");
    return;
  }

  writeStringToEEPROM(SSID_ADDR, ssid);
  writeStringToEEPROM(PASSWORD_ADDR, password);
  writeStringToEEPROM(CHANNEL_KEY_ADDR, channelKey);
  writeStringToEEPROM(AP_PASSWORD_ADDR, apPassword);
  writeStringToEEPROM(TOKEN_ADDR, token);
  EEPROM.commit();

  server.send(200, "text/plain", "Saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

void handleReboot() {
  server.send(200, "text/plain", "Rebooting...");
  delay(1000);
  ESP.restart();
}

void handleUptime() {
  server.send(200, "text/plain", formatUptime(millis()));
}

void makeHTTPRequest() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi не підключено для svitlobot");
    lastSvitlobotStatus = "❌ No WiFi";
    return;
  }
  
  if (cachedChannelKey.length() == 0) {
    Serial.println("❌ Channel key пустий");
    lastSvitlobotStatus = "❌ No key";
    return;
  }

  Serial.println("→ Пінг svitlobot...");
  
  WiFiClient client;
  client.setTimeout(5000);
  
  if (client.connect("api.svitlobot.in.ua", 80)) {
    client.print("GET /channelPing?channel_key=" + cachedChannelKey + " HTTP/1.1\r\n");
    client.print("Host: api.svitlobot.in.ua\r\n");
    client.print("Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.connected() && !client.available()) {
      if (millis() - timeout > 5000) {
        lastSvitlobotStatus = "❌ Timeout";
        Serial.println("⏱️ Timeout");
        client.stop();
        return;
      }
      delay(10);
    }
    
    // ДОДАНО: Читаємо відповідь сервера
    if (client.available()) {
      String statusLine = client.readStringUntil('\n');
      Serial.print("Відповідь: ");
      Serial.println(statusLine);
      
      if (statusLine.indexOf("200") > 0) {
        lastSvitlobotStatus = "✅ OK (200)";
        Serial.println("✅ Svitlobot пінг OK");
      } else if (statusLine.indexOf("404") > 0) {
        lastSvitlobotStatus = "❌ 404";
        Serial.println("❌ 404 Not Found");
      } else if (statusLine.indexOf("401") > 0) {
        lastSvitlobotStatus = "❌ 401 Unauthorized";
        Serial.println("❌ 401 Unauthorized");
      } else {
        lastSvitlobotStatus = "⚠️ " + statusLine.substring(9, 12);
        Serial.println("⚠️ Отримано: " + statusLine);
      }
    } else {
      lastSvitlobotStatus = "✅ OK";
    }
    
    client.stop();
    lastSvitlobotTime = millis();
  } else {
    lastSvitlobotStatus = "❌ Connection failed";
    Serial.println("❌ Svitlobot пінг FAILED");
  }
}

void makeHcPingRequest() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi не підключено для URL");
    lastUrlStatus = "❌ No WiFi";
    return;
  }
  
  if (cachedToken.length() == 0) {
    Serial.println("❌ URL пустий");
    lastUrlStatus = "❌ No URL";
    return;
  }

  Serial.print("→ Пінг URL: ");
  Serial.println(cachedToken);

  if (cachedToken.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000);
    
    String url = cachedToken.substring(8);
    int slashIndex = url.indexOf('/');
    String host = url.substring(0, slashIndex);
    String path = url.substring(slashIndex);
    
    Serial.print("Host: ");
    Serial.println(host);
    Serial.print("Path: ");
    Serial.println(path);
    
    if (client.connect(host.c_str(), 443)) {
      Serial.println("✅ SSL з'єднання встановлено");
      
      client.print("GET " + path + " HTTP/1.1\r\n");
      client.print("Host: " + host + "\r\n");
      client.print("User-Agent: ESP32\r\n");
      client.print("Connection: close\r\n\r\n");
      
      unsigned long timeout = millis();
      while (client.connected() && !client.available()) {
        if (millis() - timeout > 10000) {
          Serial.println("⏱️ Timeout очікування відповіді");
          client.stop();
          lastUrlStatus = "❌ Timeout";
          return;
        }
        delay(10);
      }
      
      if (client.available()) {
        String statusLine = client.readStringUntil('\n');
        Serial.print("Відповідь: ");
        Serial.println(statusLine);
        
        if (statusLine.indexOf("200") > 0) {
          lastUrlStatus = "✅ OK (200)";
          Serial.println("✅ URL пінг OK");
        } else if (statusLine.indexOf("404") > 0) {
          lastUrlStatus = "❌ 404";
          Serial.println("❌ URL не знайдено (404)");
        } else {
          lastUrlStatus = "⚠️ " + statusLine.substring(9, 12);
          Serial.println("⚠️ Отримано: " + statusLine);
        }
        lastUrlTime = millis();
      }
      
      client.stop();
    } else {
      Serial.println("❌ Не вдалося підключитися до " + host);
      lastUrlStatus = "❌ Connection failed";
    }
    
  } else if (cachedToken.startsWith("http://")) {
    HTTPClient http;
    http.setTimeout(10000);
    http.begin(cachedToken);
    
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      lastUrlStatus = "✅ OK (" + String(httpCode) + ")";
      Serial.println("✅ HTTP пінг OK: " + String(httpCode));
    } else {
      lastUrlStatus = "❌ Failed";
      Serial.println("❌ HTTP помилка: " + String(httpCode));
    }
    
    http.end();
  } else {
    Serial.println("❌ Невірний формат URL");
    lastUrlStatus = "❌ Bad URL";
  }
}

// ЗМІНЕНО: Додано timestamp останнього пінгу
void handlePingStatus() {
  String json = "{";
  json += "\"svitlobot\":\"" + lastSvitlobotStatus + "\",";
  json += "\"url\":\"" + lastUrlStatus + "\",";
  json += "\"lastPing\":" + String(lastPingTime);
  json += "}";
  server.send(200, "application/json", json);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/clear", HTTP_POST, handleClearEEPROM);
  server.on("/ping-status", handlePingStatus);
  server.on("/uptime", handleUptime);
  server.on("/help", handleHelp);
  server.on("/scan", handleWiFiScan);
  server.on("/scan-networks", handleScanNetworks);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1", true);
    server.send(302, "text/plain", "");
  });
  server.begin();
}

void setupDNS() {
  dnsServer.start(53, "*", WiFi.softAPIP());
}

void setupWiFi() {
  String ssid = readStringFromEEPROM(SSID_ADDR);
  String password = readStringFromEEPROM(PASSWORD_ADDR);
  
  Serial.print("SSID з EEPROM: ");
  Serial.println(ssid);
  
  if (ssid.length() > 0) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("\n✅ WiFi підключено, IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\n❌ WiFi не підключено");
      createAccessPoint();
      apStartTime = millis();
    }
  } else {
    Serial.println("SSID пустий, створюємо AP");
    createAccessPoint();
    apStartTime = millis();
  }
}

void checkWiFiConnection() {
  unsigned long currentTime = millis();
  if (currentTime - lastWiFiCheckTime < wifiCheckInterval) return;
  lastWiFiCheckTime = currentTime;
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("→ Перепідключення WiFi...");
    
    // Якщо AP активна, спочатку відключимо її, щоб не конфліктувало
    if (apStartTime > 0) {
      WiFi.softAPdisconnect(true);
      apStartTime = 0;
    }
    
    WiFi.disconnect();
    WiFi.begin(cachedSSID.c_str(), cachedPassword.c_str());
    
    // Якщо WiFi не підключено після спроби, ініціюємо AP одразу.
    if (WiFi.status() != WL_CONNECTED && apStartTime == 0) {
      Serial.println("❌ WiFi не підключено, ініціюємо AP");
      createAccessPoint();
      apStartTime = millis();
    }
  }
}

void handleWiFiConnection() {
  static unsigned long lastCheck = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastCheck < 1000) return;
  lastCheck = currentTime;
  
  if (WiFi.status() == WL_CONNECTED && currentTime - lastPingTime >= pingInterval) {
    Serial.println("\n=== ПОЧАТОК ПІНГІВ ===");
    makeHTTPRequest();
    makeHcPingRequest();
    lastPingTime = currentTime;
    Serial.println("=== КІНЕЦЬ ПІНГІВ ===\n");
  }
}

void createAccessPoint() {
  String apPassword = readStringFromEEPROM(AP_PASSWORD_ADDR);
  if (apPassword.length() < 8) apPassword = DEFAULT_AP_PASSWORD;
  
  WiFi.softAP(DEFAULT_AP_SSID, apPassword.c_str());
  Serial.print("✅ AP створено, IP: ");
  Serial.println(WiFi.softAPIP());
}

void disableAPAfterTimeout() {
  if (apStartTime > 0 && (millis() - apStartTime) > 900000) { // 15 хвилин
    WiFi.softAPdisconnect(true);
    apStartTime = 0;
  }
}