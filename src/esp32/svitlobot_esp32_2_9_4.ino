// svitlobot_esp32_2.9.4_Final_Fixed
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <DNSServer.h>

// --- 1. КОНСТАНТИ ---
const int SSID_ADDR = 0;
const int PASSWORD_ADDR = 32;
const int CHANNEL_KEY_ADDR = 64;
const int AP_PASSWORD_ADDR = 128; 
const int BOOT_COUNT_ADDR = 160;
const int TOKEN_ADDR = 192;
const int ADMIN_PASSWORD_ADDR = 256; 
const int EEPROM_SIZE = 512;

const char* DEFAULT_AP_SSID = "svitlobot";
const char* DEFAULT_AP_PASSWORD = "";

// Об'єкти
WebServer server(80);
DNSServer dnsServer;

// Глобальні змінні
unsigned long apStartTime = 0;
unsigned long bootTime = 0;

// Змінні для синхронізації з фоновим процесом
volatile unsigned long lastPingTime = 0;
const unsigned long pingInterval = 90000; // 90 сек

// Ці змінні оновлюються фоновим ядром
String lastSvitlobotStatus = "❓";
String lastUrlStatus = "❓";
bool hasInternetAccess = false;

// Кеш EEPROM
String cachedSSID = "";
String cachedPassword = "";

// --- 2. ДОПОМІЖНІ ФУНКЦІЇ ---

String readStringFromEEPROM(int addr) {
  if (addr == SSID_ADDR && cachedSSID.length() > 0) return cachedSSID;
  if (addr == PASSWORD_ADDR && cachedPassword.length() > 0) return cachedPassword;

  String data = "";
  int maxLen = (addr == TOKEN_ADDR) ? 200 : 64; 
  for (int i = 0; i < maxLen; i++) {
    char ch = EEPROM.read(addr + i);
    if (ch == 0 || (uint8_t)ch == 255) break;
    data += ch;
  }
  
  if (addr == SSID_ADDR) cachedSSID = data;
  if (addr == PASSWORD_ADDR) cachedPassword = data;
  return data;
}

void writeStringToEEPROM(int addr, String data) {
  int maxLen = (addr == TOKEN_ADDR) ? 200 : 64;
  for (int i = 0; i < data.length() && i < maxLen; i++) {
    EEPROM.write(addr + i, data[i]);
  }
  if (data.length() < maxLen) EEPROM.write(addr + data.length(), 0);
  EEPROM.commit();
  
  if (addr == SSID_ADDR) cachedSSID = "";
  if (addr == PASSWORD_ADDR) cachedPassword = "";
}

String formatUptime(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  minutes %= 60; hours %= 24;
  String s = "";
  if (days > 0) s += String(days) + " дн. ";
  if (hours > 0) s += String(hours) + " год. ";
  s += String(minutes) + " хв.";
  return s;
}

// --- 3. ФОНОВИЙ ТАСК (CORE 0) ---
void networkTask(void * pvParameters) {
  for(;;) {
    bool connected = (WiFi.status() == WL_CONNECTED);

    if (connected) {
      // 1. Перевірка інтернету (раз на 30 сек)
      static unsigned long lastNetCheck = 0;
      if (millis() - lastNetCheck > 30000 || lastNetCheck == 0) {
        WiFiClient tClient;
        tClient.setTimeout(3000);
        hasInternetAccess = tClient.connect("www.google.com", 80);
        if (hasInternetAccess) tClient.stop();
        lastNetCheck = millis();
      }

      // 2. Пінги (раз на 90 сек)
      if (millis() - lastPingTime >= pingInterval || lastPingTime == 0) {
        lastPingTime = millis();

        // Svitlobot
        String key = readStringFromEEPROM(CHANNEL_KEY_ADDR);
        if (key.length() > 0) {
          HTTPClient http;
          http.begin("http://api.svitlobot.in.ua/channelPing?channel_key=" + key);
          http.setTimeout(5000);
          int code = http.GET();
          if (code == 200) lastSvitlobotStatus = "✅ OK (200)";
          else if (code > 0) lastSvitlobotStatus = "❌ Код: " + String(code);
          else lastSvitlobotStatus = "❌ Помилка з'єднання";
          http.end();
        } else {
          lastSvitlobotStatus = "⚠️ Немає ключа";
        }

        // Healthchecks
        String token = readStringFromEEPROM(TOKEN_ADDR);
        if (token.startsWith("http")) {
          WiFiClientSecure sClient;
          sClient.setInsecure();
          HTTPClient https;
          https.begin(sClient, token);
          https.setTimeout(8000);
          int code = https.GET();
          if (code == 200) lastUrlStatus = "✅ OK (200)";
          else if (code > 0) lastUrlStatus = "❌ Код: " + String(code);
          else lastUrlStatus = "❌ Помилка з'єднання";
          https.end();
        }
      }
    } else {
      hasInternetAccess = false;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}

// --- 4. ВЕБ-ІНТЕРФЕЙС ---

bool authenticate() {
  String adminPass = readStringFromEEPROM(ADMIN_PASSWORD_ADDR);
  if (adminPass.length() < 1) return true;
  if (server.authenticate("admin", adminPass.c_str())) return true;
  server.requestAuthentication();
  return false;
}

void handleRoot() {
  String html = "<!DOCTYPE html><html lang='uk'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Світлобот</title>";
  html += "<style>body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; color: #fff; }";
  html += ".container { background-color: #333; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(255, 255, 255, 0.1); width: 90%; max-width: 400px; }";
  html += "h1 { text-align: center; color: #fff; margin-bottom: 20px; font-size: 1.5em; position: relative; }";
  html += "h1::before { content: ''; position: absolute; top: -10px; left: -10px; right: -10px; bottom: -10px; border: 2px solid #00fafa; border-radius: 8px; animation: glow 2s infinite; }";
  html += "@keyframes glow { 0% { box-shadow: 0 0 10px #00fafa; } 50% { box-shadow: 0 0 20px #00fafa; } 100% { box-shadow: 0 0 10px #00fafa; } }";
  html += ".button-container { display: flex; flex-direction: column; align-items: center; }";
  html += "button, .button { padding: 10px; background-color: #28a745; color: #fff; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 0.9em; text-align: center; text-decoration: none; margin-bottom: 10px; display: inline-block; }";
  html += "button:hover, .button:hover { background-color: #218838; } .diagnostic { margin-bottom: 20px; } .diagnostic p { margin: 5px 0; font-size: 0.8em; color: #ccc; word-break: break-all; }";
  html += ".loader { display: flex; justify-content: center; margin-top: 20px; } .dot { width: 10px; height: 10px; border-radius: 50%; margin: 0 5px; animation: bounce 1.5s infinite ease-in-out; }";
  html += ".dot:nth-child(1) { background-color: #ff6384; } .dot:nth-child(2) { background-color: #36a2eb; animation-delay: 0.2s; } .dot:nth-child(3) { background-color: #ffce56; animation-delay: 0.4s; }";
  html += "@keyframes bounce { 0%, 80%, 100% { transform: scale(0); } 40% { transform: scale(1); } }</style></head><body>";
  
  html += "<div class='container'><h1>Світлобот</h1><div class='diagnostic'>";
  html += "<p>📡 SSID: " + readStringFromEEPROM(SSID_ADDR) + "</p>";
  html += "<p>🔑 Ключ: " + readStringFromEEPROM(CHANNEL_KEY_ADDR) + "</p>";
  
  String token = readStringFromEEPROM(TOKEN_ADDR);
  if (token.length() > 40) html += "<p>🌐 URL: " + token.substring(0, 40) + "...</p>";
  else html += "<p>🌐 URL: " + token + "</p>";
  
  html += "<p>⏱️ Наступний пінг через: <span id='countdown'>90</span> сек</p>";
  html += "<p>🔵 Svitlobot: <span id='sv-st'>" + lastSvitlobotStatus + "</span></p>";
  html += "<p>🟢 URL ping: <span id='url-st'>" + lastUrlStatus + "</span></p>";
  html += "<p>📶 WiFi: " + String(WiFi.status() == WL_CONNECTED ? "✅ Підключено" : "❌ Не підключено") + "</p>";
  html += "<p>🌍 Інтернет: <span id='net-st'>...</span></p>";
  html += "<p id='uptime'>⏱️ Uptime: " + formatUptime(millis()) + "</p>";
  
  // ОСЬ ТУТ ДОДАНО IP-АДРЕСУ
  html += "<p>📟 IP: " + WiFi.localIP().toString() + "</p>";
  
  int rssi = WiFi.RSSI();
  String signalIcon = "📶";
  if (rssi > -50) signalIcon = "📶📶📶📶📶";
  else if (rssi > -60) signalIcon = "📶📶📶📶";
  else if (rssi > -70) signalIcon = "📶📶📶";
  else if (rssi > -80) signalIcon = "📶📶";
  html += "<p>" + signalIcon + " Сигнал: " + String(rssi) + " dBm</p></div>";
  
  html += "<div class='button-container'><a href='/scan' class='button'>⚙️ Налаштування</a><a href='/help' class='button' style='background-color:#007bff;'>❓ Довідка</a></div>";
  html += "<div class='loader'><div class='dot'></div><div class='dot'></div><div class='dot'></div></div></div>";

  html += "<script>";
  html += "let serverLastPing = " + String(lastPingTime) + ";";
  html += "let PING_INTERVAL_MS = " + String(pingInterval) + ";";
  html += "let localStartTime = Date.now();";
  html += "setInterval(function() {";
  html += " fetch('/ping-status').then(r=>r.json()).then(d=>{";
  html += "  document.getElementById('sv-st').innerHTML=d.svitlobot;";
  html += "  document.getElementById('url-st').innerHTML=d.url;";
  html += "  document.getElementById('net-st').innerText=d.net?'✅':'❌';";
  html += "  document.getElementById('uptime').innerText='⏱️ Uptime: '+d.uptime;";
  html += "  if(d.lastPing != serverLastPing) { serverLastPing = d.lastPing; localStartTime = Date.now(); }";
  html += " });";
  html += "}, 1000);";
  html += "setInterval(function() {";
  html += " let elapsed = Math.floor((Date.now() - localStartTime) / 1000);";
  html += " let remaining = Math.floor((PING_INTERVAL_MS / 1000) - elapsed);";
  html += " if(remaining < 0) remaining = 0;";
  html += " document.getElementById('countdown').textContent = remaining;";
  html += "}, 1000);";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void handleWiFiScan() {
  if (!authenticate()) return;
  
  String html = "<!DOCTYPE html><html lang='uk'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Налаштування WiFi</title>";
  html += "<style>body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; color: #fff; }";
  html += ".container { background-color: #333; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(255, 255, 255, 0.1); width: 90%; max-width: 400px; }";
  html += "h1 { text-align: center; color: #fff; margin-bottom: 20px; font-size: 1.5em; } form { display: flex; flex-direction: column; }";
  html += "label { margin-bottom: 5px; color: #fff; font-size: 0.9em; } input, select { padding: 10px; margin-bottom: 15px; border: 1px solid #555; border-radius: 4px; font-size: 0.9em; background-color: #444; color: #fff; width: 100%; box-sizing: border-box; }";
  html += "button, .button { padding: 10px; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 0.9em; text-align: center; text-decoration: none; margin-bottom: 10px; }";
  html += ".green-button { background-color: #28a745; color: #fff; } .green-button:hover { background-color: #218838; } .red-button { background-color: #dc3545; color: #fff; } .red-button:hover { background-color: #c82333; }";
  html += ".button-row { display: flex; justify-content: space-between; margin-top: 10px; gap: 5px; } .button-row button { flex-grow: 1; }</style></head><body>";
  
  html += "<div class='container'><h1>Налаштування</h1><form action='/save' method='POST'>";
  html += "<label for='ssid'>Оберіть SSID:</label><select id='ssid' name='ssid' required><option>Завантаження...</option></select>";
  html += "<label for='password'>Пароль WiFi:</label><input type='password' name='password' placeholder='Залиште пустим, щоб не змінювати'>";
  html += "<label>Ключ Світлобота:</label><input type='text' name='channelKey' value='" + readStringFromEEPROM(CHANNEL_KEY_ADDR) + "'>";
  html += "<label>URL Healthcheck:</label><input type='url' name='token' value='" + readStringFromEEPROM(TOKEN_ADDR) + "'>";
  html += "<label>Пароль AP:</label><input type='password' name='apPassword' placeholder='Залиште пустим, щоб не змінювати'>";
  html += "<label>Пароль Адміна:</label><input type='password' name='adminPassword' placeholder='Залиште пустим, щоб не змінювати'>";
  html += "<button type='submit' class='green-button'>💾 Зберегти та перезавантажити</button></form>";
  
  html += "<div class='button-row'><button onclick='window.location.href=\"/\"' class='green-button'>🏠 Головна</button>";
  html += "<button onclick='if(confirm(\"Ви впевнені? Це очистить ВСІ налаштування.\")) { fetch(\"/clear\"); }' class='red-button'>❌ Очистити EEPROM</button>";
  html += "<button onclick='if(confirm(\"Перезавантажити пристрій?\")) { fetch(\"/reboot\", {method: \"POST\"}); }' class='green-button'>🔄 Перезавантажити</button></div></div>";
  
  html += "<script>fetch('/scan-networks').then(r=>r.json()).then(d=>{let s=document.getElementById('ssid'); s.innerHTML='';";
  html += "d.forEach(n=>{let o=document.createElement('option'); o.value=n.ssid; o.text=n.ssid+' ('+n.rssi+' dBm)'; s.appendChild(o);});";
  html += "let c='" + readStringFromEEPROM(SSID_ADDR) + "'; if(c){let o=document.createElement('option'); o.value=c; o.text=c+' (поточна)'; o.selected=true; s.prepend(o);}";
  html += "});</script></body></html>";

  server.send(200, "text/html", html);
}

void handlePingStatus() {
  String json = "{";
  json += "\"svitlobot\":\"" + lastSvitlobotStatus + "\",";
  json += "\"url\":\"" + lastUrlStatus + "\",";
  json += "\"net\":" + String(hasInternetAccess ? "true" : "false") + ",";
  json += "\"uptime\":\"" + formatUptime(millis()) + "\",";
  json += "\"lastPing\":" + String(lastPingTime);
  json += "}";
  server.send(200, "application/json", json);
}

void handleScanNetworks() {
  int n = WiFi.scanComplete();
  if (n == -2) {
    WiFi.scanNetworks(true); 
    server.send(200, "application/json", "[]"); 
    return;
  } else if (n == -1) {
    server.send(200, "application/json", "[]"); 
    return;
  }
  
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleSave() {
  if (!authenticate()) return;
  if (server.hasArg("ssid")) writeStringToEEPROM(SSID_ADDR, server.arg("ssid"));
  if (server.hasArg("channelKey")) writeStringToEEPROM(CHANNEL_KEY_ADDR, server.arg("channelKey"));
  if (server.hasArg("token")) writeStringToEEPROM(TOKEN_ADDR, server.arg("token"));
  if (server.hasArg("password") && server.arg("password").length() > 0) writeStringToEEPROM(PASSWORD_ADDR, server.arg("password"));
  if (server.hasArg("apPassword") && server.arg("apPassword").length() > 0) writeStringToEEPROM(AP_PASSWORD_ADDR, server.arg("apPassword"));
  if (server.hasArg("adminPassword") && server.arg("adminPassword").length() > 0) writeStringToEEPROM(ADMIN_PASSWORD_ADDR, server.arg("adminPassword"));

  server.send(200, "text/plain", "Settings saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

void handleClearEEPROM() {
  if (!authenticate()) return;
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
  EEPROM.commit();
  server.send(200, "text/plain", "Cleared. Rebooting...");
  delay(1000);
  ESP.restart();
}

void handleReboot() {
  if (!authenticate()) return;
  server.send(200, "text/plain", "Rebooting...");
  delay(1000);
  ESP.restart();
}

void handleNotFound() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void handleHelp() {
  String html = "<!DOCTYPE html><html lang='uk'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Довідка</title>";
  html += "<style>body{font-family:Arial;background:#000;color:#fff;padding:20px;} .container{background:#333;padding:20px;border-radius:8px;max-width:600px;margin:auto;} h1,h2{color:#00fafa;} a{color:#fff;text-decoration:none;background:#28a745;padding:10px;border-radius:4px;display:inline-block;margin-top:20px;}</style></head><body>";
  html += "<div class='container'><h1>Довідка</h1><h2>Проєкт Світлобот</h2><p>Версія 2.9.4 (Async ESP32)</p>";
  html += "<p>Пристрій пінгує сервери кожні 90 секунд.</p><a href='/'>🏠 На головну</a></div></body></html>";
  server.send(200, "text/html", html);
}

// --- 5. SETUP & LOOP ---

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  bootTime = millis();

  // Boot count loop protection
  int bootCount = EEPROM.read(BOOT_COUNT_ADDR);
  if (bootCount > 10) {
    for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
    EEPROM.commit();
    ESP.restart();
  }
  EEPROM.write(BOOT_COUNT_ADDR, bootCount + 1);
  EEPROM.commit();

  // WiFi Setup
  WiFi.mode(WIFI_AP_STA);
  String s = readStringFromEEPROM(SSID_ADDR);
  String p = readStringFromEEPROM(PASSWORD_ADDR);
  String apPass = readStringFromEEPROM(AP_PASSWORD_ADDR);
  if (apPass.length() < 8) apPass = "";

  if (s.length() > 0) WiFi.begin(s.c_str(), p.c_str());
  WiFi.softAP(DEFAULT_AP_SSID, apPass.c_str());

  dnsServer.start(53, "*", WiFi.softAPIP());
  
  server.on("/", handleRoot);
  server.on("/ping-status", handlePingStatus);
  server.on("/scan", handleWiFiScan); 
  server.on("/scan-networks", handleScanNetworks); 
  server.on("/save", HTTP_POST, handleSave);
  server.on("/clear", handleClearEEPROM);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/help", handleHelp);
  server.onNotFound(handleNotFound);
  server.begin();

  xTaskCreatePinnedToCore(networkTask, "NetTask", 8192, NULL, 1, NULL, 0);
}

void loop() {
  server.handleClient();
  dnsServer.processNextRequest();

  if (apStartTime == 0 && WiFi.softAPgetStationNum() > 0) apStartTime = millis(); 
  if (millis() > 900000 && WiFi.status() == WL_CONNECTED) {
     WiFi.softAPdisconnect(true);
  }

  if (millis() > 30000) {
    if (EEPROM.read(BOOT_COUNT_ADDR) != 0) {
      EEPROM.write(BOOT_COUNT_ADDR, 0);
      EEPROM.commit();
    }
  }
}