#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ESP8266Ping.h>

// ========================================
// SVITLOBOT v3.2.9 ESP8266 + UPS MODE
// WEMOS D1 MINI - https://github.com/dneese/svitlobot2
// ========================================

const char* AP_SSID = "Svitlobot-Setup";
const char* HOSTNAME = "svitlobot";
const int DNS_PORT = 53;

const int SSID_ADDR = 0;           // 0-31   (32 bytes)
const int PASSWORD_ADDR = 32;      // 32-63  (32 bytes)
const int CHANNEL_KEY_ADDR = 64;   // 64-95  (32 bytes)
const int HEALTH_URL_ADDR = 96;    // 96-191 (96 bytes) ⚡ ЗБІЛЬШЕНО!
const int AP_PASSWORD_ADDR = 192;  // 192-223 (32 bytes)
const int ADMIN_PASSWORD_ADDR = 224; // 224-255 (32 bytes)
const int BOOT_COUNT_ADDR = 256;   // 256
const int UPS_ENABLED_ADDR = 257;  // 257
const int UPS_PROBE_IP_ADDR = 258; // 258-272 (15 bytes)
const int EEPROM_SIZE = 512;


const unsigned long PING_INTERVAL = 90000;
const unsigned long WIFI_CHECK_INTERVAL = 30000;
const unsigned long AP_TIMEOUT = 900000;
const unsigned long INTERNET_CHECK_INTERVAL = 30000;

ESP8266WebServer server(80);
DNSServer dnsServer;
Ticker watchdog;

struct SystemState {
  String svitlobotStatus = "⏳ Очікування";
  String urlStatus       = "⏳ Очікування";
  bool internetAccess    = false;
  unsigned long lastPingTime  = 0;
  unsigned long lastWiFiCheck = 0;
  unsigned long apStartTime   = 0;
  unsigned long lastInternetCheck = 0;
  unsigned long bootTime      = 0;
  int wifiFailCount = 0;
  int bootCount     = 0;
  bool apActive     = false;
} state;

struct Config {
  String ssid;
  String password;
  String channelKey;
  String healthUrl;
  String adminPassword;
  String apPassword;
  bool   upsEnabled = false;
  String upsProbeIp = "";
} config;

// ========================================
// EEPROM
// ========================================

String readStringFromEEPROM(int addr, int maxLen = 64) {
  String data = "";
  char ch = EEPROM.read(addr);
  int i = 0;
  while (ch != '\0' && (addr + i) < EEPROM_SIZE && i < maxLen) {
    data += ch;
    i++;
    ch = EEPROM.read(addr + i);
  }
  return data;
}

void writeStringToEEPROM(int addr, String data, int maxLen = 64) {
  for (int i = 0; i < data.length() && i < maxLen; i++) {
    EEPROM.write(addr + i, data[i]);
  }
  EEPROM.write(addr + data.length(), '\0');
  EEPROM.commit();
}

void loadConfig() {
  config.ssid          = readStringFromEEPROM(SSID_ADDR, 32);
  config.password      = readStringFromEEPROM(PASSWORD_ADDR, 32);
  config.channelKey    = readStringFromEEPROM(CHANNEL_KEY_ADDR, 32);
  config.healthUrl     = readStringFromEEPROM(HEALTH_URL_ADDR, 96); // ⚡ 96 байт
  config.adminPassword = readStringFromEEPROM(ADMIN_PASSWORD_ADDR, 32);
  config.apPassword    = readStringFromEEPROM(AP_PASSWORD_ADDR, 32);
  config.upsEnabled    = EEPROM.read(UPS_ENABLED_ADDR) == 1;
  config.upsProbeIp    = readStringFromEEPROM(UPS_PROBE_IP_ADDR, 15);
  state.bootCount      = EEPROM.read(BOOT_COUNT_ADDR);
  
  Serial.printf("[CONFIG] SSID: %s | AP Pass: %s | UPS: %s\n",
                config.ssid.c_str(),
                config.apPassword.length() > 0 ? "SET" : "OPEN",
                config.upsEnabled ? "ON" : "OFF");
  Serial.printf("[CONFIG] Health URL: '%s' (len=%d)\n", config.healthUrl.c_str(), config.healthUrl.length());
}

void saveConfig() {
  writeStringToEEPROM(SSID_ADDR, config.ssid, 32);
  writeStringToEEPROM(PASSWORD_ADDR, config.password, 32);
  writeStringToEEPROM(CHANNEL_KEY_ADDR, config.channelKey, 32);
  writeStringToEEPROM(HEALTH_URL_ADDR, config.healthUrl, 96); // ⚡ 96 байт
  writeStringToEEPROM(ADMIN_PASSWORD_ADDR, config.adminPassword, 32);
  writeStringToEEPROM(AP_PASSWORD_ADDR, config.apPassword, 32);
  EEPROM.write(UPS_ENABLED_ADDR, config.upsEnabled ? 1 : 0);
  writeStringToEEPROM(UPS_PROBE_IP_ADDR, config.upsProbeIp, 15);
  EEPROM.commit();
  Serial.println("[CONFIG] ✓ Збережено");
  Serial.printf("[SAVE] Health URL: '%s' (len=%d)\n", config.healthUrl.c_str(), config.healthUrl.length());
}


void factoryReset() {
  Serial.println("[!!!] FACTORY RESET");
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  delay(500);
  ESP.restart();
}

// ========================================
// UPS ЛОГІКА (4 ПІНГИ)
// ========================================

bool upsProbeOk() {
  if (!config.upsEnabled) return true;
  if (config.upsProbeIp.length() < 7) return true;

  IPAddress ip;
  if (!ip.fromString(config.upsProbeIp)) {
    Serial.printf("[UPS] Невірний IP: %s\n", config.upsProbeIp.c_str());
    return true;
  }

  bool ok = Ping.ping(ip, 4); // 4 ПІНГИ
  Serial.printf("[UPS] Ping %s (x4) -> %s\n",
                config.upsProbeIp.c_str(), ok ? "✅ OK (є світло)" : "❌ FAIL (немає світла)");
  return ok;
}

// ========================================
// БЕЗПЕКА
// ========================================

bool checkAuth() {
  if (config.adminPassword.length() == 0) {
    return true;
  }
  if (server.authenticate("admin", config.adminPassword.c_str())) {
    return true;
  }
  server.requestAuthentication();
  return false;
}

// ========================================
// МЕРЕЖА
// ========================================

void checkInternetAccess() {
  if (millis() - state.lastInternetCheck < INTERNET_CHECK_INTERVAL) return;
  state.lastInternetCheck = millis();
  
  if (WiFi.status() != WL_CONNECTED) {
    state.internetAccess = false;
    return;
  }
  
  WiFiClient client;
  client.setTimeout(3000);
  if (client.connect("www.google.com", 80)) {
    state.internetAccess = true;
    Serial.println("[INTERNET] ✅ OK");
    client.stop();
  } else {
    state.internetAccess = false;
    Serial.println("[INTERNET] ❌ Offline");
  }
}

String httpErrText(int code) {
  if (code == 200) return "✅ OK";
  if (code == -1)  return "❌ Таймаут";
  if (code <= 0)   return "❌ Помилка";
  if (code >= 500) return "❌ Сервер " + String(code);
  if (code >= 400) return "❌ Код " + String(code);
  return "✅ " + String(code);
}

void performPings() {
  if (WiFi.status() != WL_CONNECTED) {
    state.svitlobotStatus = "❌ WiFi";
    state.urlStatus       = "❌ WiFi";
    return;
  }

  // ПЕРЕВІРКА UPS
  if (!upsProbeOk()) {
    state.svitlobotStatus = "⚪ Пропущено (UPS)";
    state.urlStatus       = "⚪ Пропущено (UPS)";
    state.lastPingTime = millis();
    Serial.println("[PING] Пропущено через UPS");
    return;
  }

  Serial.println("\n[PING] Старт...");

  // ==================== SVITLOBOT ====================
  if (config.channelKey.length() > 5) {
    WiFiClient client;
    HTTPClient http;
    
    http.begin(client, "http://api.svitlobot.in.ua/channelPing?channel_key=" + config.channelKey);
    http.setTimeout(5000);
    http.addHeader("User-Agent", "ESP8266-Svitlobot/3.2.9");
    
    int code = http.GET();
    state.svitlobotStatus = httpErrText(code);
    http.end();
    
    Serial.printf("[PING] Svitlobot: %s (%d)\n", state.svitlobotStatus.c_str(), code);
  } else {
    state.svitlobotStatus = "⚪ Не налаштовано";
  }

  delay(100);

  // ==================== HEALTH URL ====================
  // ОЧИЩАЄМО URL ВІД ЗАЙВИХ ПРОБІЛІВ ТА ЕНТЕРІВ
  String cleanUrl = config.healthUrl;
  cleanUrl.trim();
  
  // Видаляємо зайві слеші в кінці
  while (cleanUrl.endsWith("/")) {
    cleanUrl.remove(cleanUrl.length() - 1);
  }

  if (cleanUrl.length() > 5 && cleanUrl.startsWith("http")) {
    
    // ВІДОБРАЖАЄМО URL ДЛЯ ДІАГНОСТИКИ
    Serial.printf("[DEBUG] Health URL: '%s' (len=%d)\n", cleanUrl.c_str(), cleanUrl.length());

    bool isHTTPS = cleanUrl.startsWith("https");
    
    WiFiClientSecure secureClient;
    WiFiClient normalClient;
    HTTPClient http;
    
    bool beginResult = false;
    
    if (isHTTPS) {
      secureClient.setInsecure(); // Відключаємо перевірку SSL сертифікату
      secureClient.setBufferSizes(512, 512); // Економія RAM
      beginResult = http.begin(secureClient, cleanUrl);
    } else {
      beginResult = http.begin(normalClient, cleanUrl);
    }

    if (beginResult) {
      http.setTimeout(10000);
      
      // КРИТИЧНІ ЗАГОЛОВКИ (БЕЗ НИХ = 400)
      http.addHeader("User-Agent", "ESP8266-Svitlobot/3.2.9");
      http.addHeader("Accept", "*/*");
      http.addHeader("Connection", "close");
      
      // Для healthchecks.io потрібен Host header
      if (cleanUrl.indexOf("hc-ping.com") > 0 || cleanUrl.indexOf("healthchecks.io") > 0) {
        http.addHeader("Host", "hc-ping.com");
      }

      int code = http.GET();
      state.urlStatus = httpErrText(code);
      
      // ДІАГНОСТИКА: Якщо помилка - виводимо відповідь сервера
      if (code == 400 || code == 401 || code == 404) {
        String response = http.getString();
        Serial.printf("[DEBUG] Server Response (code %d): %s\n", code, response.substring(0, 200).c_str());
      }
      
      http.end();
      
      Serial.printf("[PING] Health %s: %s (%d)\n", isHTTPS ? "HTTPS" : "HTTP", state.urlStatus.c_str(), code);
      
    } else {
      state.urlStatus = "❌ Init Failed";
      Serial.println("[PING] ❌ Unable to begin HTTP connection");
    }
    
  } else if (cleanUrl.length() > 0) {
    state.urlStatus = "❌ Bad URL";
    Serial.printf("[PING] ❌ Invalid URL format: '%s'\n", cleanUrl.c_str());
  } else {
    state.urlStatus = "⚪ Не налаштовано";
  }

  state.lastPingTime = millis();
  Serial.printf("[PING] Завершено. RAM: %d bytes\n", ESP.getFreeHeap());
}

void checkAndReconnectWiFi() {
  if (millis() - state.lastWiFiCheck < WIFI_CHECK_INTERVAL) return;
  state.lastWiFiCheck = millis();
  if (WiFi.status() != WL_CONNECTED && config.ssid.length() > 0) {
    state.wifiFailCount++;
    Serial.printf("[WiFi] Втрачено (%d/3)\n", state.wifiFailCount);
    if (state.wifiFailCount >= 3) {
      Serial.println("[WiFi] Перепідключення...");
      WiFi.disconnect();
      delay(100);
      WiFi.begin(config.ssid.c_str(), config.password.c_str());
      state.wifiFailCount = 0;
    }
  } else if (WiFi.status() == WL_CONNECTED) {
    state.wifiFailCount = 0;
  }
}

void checkAPTimeout() {
  if (state.apActive && WiFi.status() == WL_CONNECTED &&
      millis() - state.apStartTime > AP_TIMEOUT) {
    Serial.println("[AP] 15 хв + WiFi OK - вимикаємо AP");
    WiFi.softAPdisconnect(true);
    state.apActive = false;
  }
}

// ========================================
// WEB UI
// ========================================

String formatUptime(unsigned long ms) {
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours   = minutes / 60;
  unsigned long days    = hours / 24;
  if (days > 0)  return String(days) + "д " + String(hours % 24) + "г";
  if (hours > 0) return String(hours) + "г " + String(minutes % 60) + "хв";
  return String(minutes) + "хв " + String(seconds % 60) + "с";
}

const char HTML_CSS[] PROGMEM = R"rawliteral(
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:linear-gradient(135deg,#0a0a1a 0%,#1a1a2e 100%);color:#e8e8e8;
font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
min-height:100vh;display:flex;justify-content:center;align-items:center;padding:15px}
.container{background:rgba(25,25,40,0.95);backdrop-filter:blur(15px);padding:30px;
border-radius:24px;width:100%;max-width:440px;box-shadow:0 10px 40px rgba(0,250,250,0.2),
0 0 2px rgba(0,250,250,0.4);border:1px solid rgba(0,250,250,0.15)}
h1{text-align:center;background:linear-gradient(135deg,#00fafa 0%,#00d4ff 100%);
-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text;
font-size:32px;font-weight:800;margin-bottom:30px;letter-spacing:1.5px;position:relative;
padding-bottom:15px}
.stats{background:linear-gradient(135deg,rgba(15,15,30,0.9),rgba(20,20,35,0.9));padding:20px;
border-radius:16px;margin-bottom:25px;border:1px solid rgba(0,250,250,0.1)}
.stat{display:flex;justify-content:space-between;align-items:center;padding:12px 14px;margin:8px 0;
border-radius:10px;transition:all 0.3s cubic-bezier(0.4,0,0.2,1)}
.stat:hover{background:rgba(0,250,250,0.08);transform:translateX(5px)}
.stat-label{color:#a0a0b0;font-size:14px;font-weight:500}
.stat-value{color:#fff;font-weight:700;text-align:right;font-size:14px}
input,select{width:100%;padding:15px 18px;margin:12px 0;background:rgba(35,35,55,0.7);color:#fff;
border:2px solid rgba(0,250,250,0.25);border-radius:12px;font-size:14px;transition:all 0.3s ease;
outline:none}
input:focus,select:focus{border-color:#00fafa;box-shadow:0 0 0 4px rgba(0,250,250,0.12);
background:rgba(40,40,65,0.95)}
input[type=checkbox]{width:auto;margin-right:8px}
label{display:block;margin:18px 0 8px;color:#b5b5c0;font-size:13px;font-weight:600;
text-transform:uppercase;letter-spacing:0.5px}
.btn{display:block;width:100%;padding:16px;margin:10px 0;border:none;border-radius:12px;cursor:pointer;
font-weight:700;text-align:center;text-decoration:none;color:#fff;font-size:15px;transition:all 0.3s ease;
position:relative;overflow:hidden;text-transform:uppercase;letter-spacing:0.5px}
.btn-primary{background:linear-gradient(135deg,#28a745 0%,#20c997 100%);
box-shadow:0 5px 20px rgba(40,167,69,0.35)}
.btn-danger{background:linear-gradient(135deg,#dc3545 0%,#c82333 100%);
box-shadow:0 5px 20px rgba(220,53,69,0.35)}
.btn-info{background:linear-gradient(135deg,#007bff 0%,#0056b3 100%);
box-shadow:0 5px 20px rgba(0,123,255,0.35)}
.btn-cyan{background:linear-gradient(135deg,#00fafa 0%,#00c3ff 100%);
box-shadow:0 5px 20px rgba(0,250,250,0.35);color:#000}
.btn-warning{background:linear-gradient(135deg,#ffc107 0%,#ff9800 100%);
box-shadow:0 5px 20px rgba(255,193,7,0.35);color:#000}
.pulse{animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.6}}
.section{margin:25px 0;padding:20px;background:rgba(15,15,30,0.6);border-radius:12px;
border:1px solid rgba(0,250,250,0.1)}
.section-title{color:#00fafa;font-size:16px;font-weight:700;margin-bottom:15px;
text-transform:uppercase;letter-spacing:1px}
#ups-test-result{color:#aaa;font-size:13px;margin-top:8px;min-height:20px}
small{color:#888;font-size:12px;line-height:1.5}
</style>
)rawliteral";

String processTemplate(const String& html) {
  String result = html;
  result.replace("%CSS%",           String(FPSTR(HTML_CSS)));
  result.replace("%SSID%",          config.ssid.length() > 0 ? config.ssid : "Не підключено");
  result.replace("%IP%",            WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "—");
  result.replace("%SVITLOBOT%",     state.svitlobotStatus);
  result.replace("%URL%",           state.urlStatus);
  result.replace("%INTERNET%",      state.internetAccess ? "✅ Активний" : "❌ Недоступний");
  result.replace("%RSSI%",          WiFi.status() == WL_CONNECTED ? String(WiFi.RSSI()) + " dBm" : "—");
  result.replace("%UPTIME%",        formatUptime(millis()));
  result.replace("%HEAP%",          String(ESP.getFreeHeap()));
  result.replace("%KEY%",           config.channelKey);
  result.replace("%HEALTHURL%",     config.healthUrl);
  result.replace("%CURRENT_SSID%",  config.ssid);
  result.replace("%AP_PASS_STATUS%", config.apPassword.length() > 0 ? "🔒 Встановлено" : "⚠️ Відкрито");
  result.replace("%UPS_CHECKED%",   config.upsEnabled ? "checked" : "");
  result.replace("%UPS_IP%",        config.upsProbeIp);
  return result;
}

void setupWebServer() {
  server.on("/", []() {
    String html = F(
      "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Svitlobot v3.2.9</title>%CSS%</head><body>"
      "<div class='container'><h1>⚡ SVITLOBOT</h1><div class='stats'>"
      "<div class='stat'><span class='stat-label'>📡 WiFi</span><span class='stat-value'>%SSID%</span></div>"
      "<div class='stat'><span class='stat-label'>📟 IP</span><span class='stat-value'>%IP%</span></div>"
      "<div class='stat'><span class='stat-label'>🔵 Svitlobot</span><span class='stat-value pulse'>%SVITLOBOT%</span></div>"
      "<div class='stat'><span class='stat-label'>🟢 Health</span><span class='stat-value pulse'>%URL%</span></div>"
      "<div class='stat'><span class='stat-label'>🌍 Інтернет</span><span class='stat-value'>%INTERNET%</span></div>"
      "<div class='stat'><span class='stat-label'>📶 Сигнал</span><span class='stat-value'>%RSSI%</span></div>"
      "<div class='stat'><span class='stat-label'>⏱️ Uptime</span><span class='stat-value' id='up'>%UPTIME%</span></div>"
      "<div class='stat'><span class='stat-label'>💾 RAM</span><span class='stat-value'>%HEAP% b</span></div>"
      "</div>"
      "<a href='/config' class='btn btn-cyan'>⚙️ Налаштування</a>"
      "<button onclick='location.reload()' class='btn btn-primary'>🔃 Refresh</button>"
      "</div>"
      "<script>setInterval(()=>fetch('/uptime').then(r=>r.text()).then(d=>document.getElementById('up').innerText=d),5000)</script>"
      "</body></html>"
    );
    server.send(200, "text/html; charset=utf-8", processTemplate(html));
  });

  server.on("/config", []() {
    if (!checkAuth()) return;
    String html = F(
      "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Config</title>%CSS%</head><body>"
      "<div class='container'><h1>⚙️ CONFIG</h1>"
      "<form action='/save' method='POST'>"
      "<div class='section'><div class='section-title'>WiFi</div>"
      "<select name='ssid' id='ss' required><option value=''>⏳ Сканування...</option></select>"
      "<input type='password' name='pass' id='wifipass' placeholder='Password (min 8)'>"
      "<label style='color:#ffc107'><input type='checkbox' name='clearwifipass' onchange='toggleWiFi(this)'> ❌ Видалити пароль WiFi</label>"
      "</div>"
      "<div class='section'><div class='section-title'>Security</div>"
      "<input type='password' name='admin' id='adminpass' placeholder='Admin password (login: admin)'>"
      "<label style='color:#ffc107'><input type='checkbox' name='clearadminpass' onchange='toggleAdmin(this)'> ❌ Вимкнути адмін пароль</label>"
      "<input type='password' name='appass' id='appass' placeholder='AP password (min 8)'>"
      "<label style='color:#ffc107'><input type='checkbox' name='clearappass' onchange='toggleAP(this)'> ❌ Видалити пароль AP</label>"
      "<small>Поточний AP: %AP_PASS_STATUS%</small>"
      "</div>"
      "<div class='section'><div class='section-title'>API</div>"
      "<input type='text' name='key' value='%KEY%' placeholder='Svitlobot key'>"
      "<input type='text' name='url' value='%HEALTHURL%' placeholder='Health URL'></div>"
      "<div class='section'><div class='section-title'>🔌 UPS Режим</div>"
      "<label><input type='checkbox' name='ups' %UPS_CHECKED%> Роутер/ESP на UPS</label>"
      "<input type='text' name='upsip' id='upsip' value='%UPS_IP%' placeholder='IP контрольного пристрою'>"
      "<button type='button' class='btn btn-warning' onclick='testUps()'>🔍 Тест пінгу</button>"
      "<p id='ups-test-result'></p>"
      "<small>Якщо UPS увімкнено, пінги будуть слатися лише коли цей IP відповідає</small>"
      "</div>"
      "<button type='submit' class='btn btn-primary'>💾 Save</button>"
      "</form>"
      "<button onclick='if(confirm(\"Reboot?\"))fetch(\"/reboot\")' class='btn btn-info'>🔄 Reboot</button>"
      "<button onclick='if(confirm(\"RESET ALL?\"))location.href=\"/factory-reset\"' class='btn btn-danger'>⚠️ Reset</button>"
      "<a href='/' class='btn btn-info'>⬅️ Back</a>"
      "</div>"
      "<script>"
      "let currentSsid='%CURRENT_SSID%';"
      "function load(){"
      "console.log('[JS] Запит /scan...');"
      "fetch('/scan').then(r=>{console.log('[JS] Статус:',r.status);return r.json();}).then(d=>{"
      "console.log('[JS] Отримано:',d);"
      "if(!Array.isArray(d)){console.error('[JS] Не масив!');return;}"
      "let s=document.getElementById('ss');s.innerHTML='';"
      "if(currentSsid.length>0){let o=new Option(currentSsid+' (поточна)',currentSsid);o.selected=true;s.add(o);}"
      "d.forEach(i=>{if(i.s!==currentSsid){let o=new Option(i.s+' ('+i.q+')',i.s);s.add(o);}});"
      "console.log('[JS] Список оновлено, всього:',s.options.length);"
      "}).catch(e=>console.error('[JS] Помилка:',e));"
      "}"
      "setTimeout(load,500);"
      "function testUps(){"
      "const r=document.getElementById('ups-test-result'),ip=document.getElementById('upsip').value;"
      "if(!ip){r.innerText='❌ Введіть IP';return;}"
      "r.innerText='⏳ Перевірка...';"
      "fetch('/test-ups-ping?ip='+ip).then(x=>x.text()).then(t=>r.innerText=t);}"
      "function toggleWiFi(cb){const i=document.getElementById('wifipass');if(cb.checked){i.disabled=true;i.value='';i.placeholder='Буде видалено';}else{i.disabled=false;i.placeholder='Password (min 8)';}}"
      "function toggleAdmin(cb){const i=document.getElementById('adminpass');if(cb.checked){i.disabled=true;i.value='';i.placeholder='Буде вимкнено';}else{i.disabled=false;i.placeholder='Admin password';}}"
      "function toggleAP(cb){const i=document.getElementById('appass');if(cb.checked){i.disabled=true;i.value='';i.placeholder='Буде видалено (AP відкрито!)';}else{i.disabled=false;i.placeholder='AP password (min 8)';}}"
      "</script></body></html>"
    );
    server.send(200, "text/html; charset=utf-8", processTemplate(html));
  });

  server.on("/test-ups-ping", []() {
    if (!checkAuth()) return;
    
    String testIp = server.hasArg("ip") ? server.arg("ip") : config.upsProbeIp;
    
    if (testIp.length() < 7) {
      server.send(200, "text/plain; charset=utf-8", "❌ Введіть коректний IP");
      return;
    }
    
    IPAddress ip;
    if (!ip.fromString(testIp)) {
      server.send(200, "text/plain; charset=utf-8", "❌ Невірний формат IP");
      return;
    }
    
    bool ok = Ping.ping(ip, 4); // 4 ПІНГИ
    String msg = ok ? 
      "✅ Пристрій " + testIp + " відповідає (є світло) [4 пінги OK]" : 
      "❌ Пристрій " + testIp + " НЕ відповідає (немає світла) [4 пінги FAIL]";
    
    server.send(200, "text/plain; charset=utf-8", msg);
  });

  server.on("/scan", []() {
    Serial.println("\n========== /SCAN ЗАПИТ ==========");
    Serial.printf("WiFi Mode: %d\n", WiFi.getMode());
    Serial.printf("AP Active: %s\n", state.apActive ? "YES" : "NO");
    
    Serial.println("Запуск WiFi.scanNetworks()...");
    int n = WiFi.scanNetworks(false, false); // Синхронне
    Serial.printf("Результат: %d мереж\n", n);
    
    if (n <= 0) {
      Serial.println("❌ Мереж не знайдено!");
      server.send(200, "application/json", "[]");
      Serial.println("=================================\n");
      return;
    }
    
    Serial.println("Знайдені мережі:");
    String json = "[";
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      Serial.printf("  %d. %s (%d dBm)\n", i+1, ssid.c_str(), rssi);
      
      if (i > 0) json += ",";
      ssid.replace("\"", "\\\"");
      String quality = (rssi > -50) ? "📶 Відмінно" : (rssi > -70) ? "📶 Добре" : "📶 Слабко";
      json += "{\"s\":\"" + ssid + "\",\"r\":" + String(rssi) + ",\"q\":\"" + quality + "\"}";
    }
    json += "]";
    
    Serial.println("JSON:");
    Serial.println(json);
    server.send(200, "application/json", json);
    Serial.println("=================================\n");
  });

  server.on("/save", HTTP_POST, []() {
    if (!checkAuth()) return;
    bool restartWiFi = false;
    bool restartAP = false;
    
    if (server.hasArg("ssid")) {
      String s = server.arg("ssid");
      if (s != config.ssid && s.length() > 0) { config.ssid = s; restartWiFi = true; }
    }
    
    if (server.hasArg("clearwifipass")) {
      config.password = "";
      restartWiFi = true;
    } else if (server.hasArg("pass")) {
      String p = server.arg("pass");
      if (p.length() >= 8) { config.password = p; restartWiFi = true; }
    }
    
    if (server.hasArg("clearappass")) {
      config.apPassword = "";
      restartAP = true;
    } else if (server.hasArg("appass")) {
      String ap = server.arg("appass");
      if (ap.length() >= 8) { config.apPassword = ap; restartAP = true; }
    }
    
    if (server.hasArg("clearadminpass")) {
      config.adminPassword = "";
    } else if (server.hasArg("admin")) {
      String a = server.arg("admin");
      if (a.length() > 0) config.adminPassword = a;
    }
    
    if (server.hasArg("key")) {
      config.channelKey = server.arg("key");
      config.channelKey.trim();
    }
    
    if (server.hasArg("url")) {
      config.healthUrl = server.arg("url");
      config.healthUrl.trim(); // ⚡ ОЧИСТКА!
      
      // Видаляємо зайві слеші
      while (config.healthUrl.endsWith("/")) {
        config.healthUrl.remove(config.healthUrl.length() - 1);
      }
      
      Serial.printf("[SAVE] Health URL: '%s'\n", config.healthUrl.c_str());
    }
    
    // UPS
    config.upsEnabled = server.hasArg("ups");
    if (server.hasArg("upsip")) {
      config.upsProbeIp = server.arg("upsip");
      config.upsProbeIp.trim();
    }
    
    saveConfig();
    
    if (restartAP && state.apActive) {
      WiFi.softAPdisconnect(true);
      delay(500);
      const char* apPass = (config.apPassword.length() >= 8) ? config.apPassword.c_str() : NULL;
      WiFi.softAP(AP_SSID, apPass);
      Serial.printf("[AP] ✅ Перезапущено (%s)\n", apPass ? "PASSWORD" : "OPEN");
      state.apStartTime = millis();
    }
    
    String resp = String("<html><head>") + String(FPSTR(HTML_CSS)) + "</head><body><div class='container'><h1>✅ ЗБЕРЕЖЕНО</h1>";
    
    if (restartWiFi) {
      resp += "<p style='text-align:center;color:#00fafa'>WiFi перепідключення через <span id='c'>3</span>с...</p>"
              "<script>let t=3;setInterval(()=>{t--;document.getElementById('c').innerText=t;if(t<=0)location.href='/'},1000)</script>";
    } else if (restartAP) {
      resp += "<p style='text-align:center;color:#ffc107'>⚠️ AP пароль оновлено!</p><a href='/config' class='btn btn-info'>⬅️ Назад</a>";
    } else {
      resp += "<a href='/config' class='btn btn-info'>⬅️ Назад</a>";
    }
    resp += "</div></body></html>";
    
    server.send(200, "text/html; charset=utf-8", resp);
    
    if (restartWiFi) { delay(3000); ESP.restart(); }
  });

  server.on("/uptime", []() {
    server.send(200, "text/plain", formatUptime(millis()));
  });

  server.on("/reboot", []() {
    if (!checkAuth()) return;
    server.send(200, "text/plain", "OK");
    delay(500);
    ESP.restart();
  });

  server.on("/factory-reset", []() {
    if (!checkAuth()) return;
    server.send(200, "text/plain", "RESETTING...");
    delay(1000);
    factoryReset();
  });

  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("[HTTP] ✓ Started");
}

// ========================================
// RESCUE MODE
// ========================================

void checkRescueMode() {
  state.bootCount = EEPROM.read(BOOT_COUNT_ADDR);
  if (state.bootCount >= 10) {
    Serial.println("[RESCUE] 10 завантажень - Reset...");
    factoryReset();
  }
  state.bootCount++;
  EEPROM.write(BOOT_COUNT_ADDR, state.bootCount);
  EEPROM.commit();
  Serial.printf("[RESCUE] Boot count: %d/10\n", state.bootCount);
}

void resetBootCounter() {
  static bool reset = false;
  if (!reset && millis() - state.bootTime > 20000) {
    EEPROM.write(BOOT_COUNT_ADDR, 0);
    EEPROM.commit();
    reset = true;
    Serial.println("[RESCUE] Counter reset");
  }
}

// ========================================
// SETUP & LOOP
// ========================================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║ SVITLOBOT v3.2.9 ESP8266+UPS  ║");
  Serial.println("║ github.com/dneese/svitlobot2  ║");
  Serial.println("╚════════════════════════════════╝\n");

  EEPROM.begin(EEPROM_SIZE);
  state.bootTime = millis();
  checkRescueMode();
  loadConfig();

  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  WiFi.hostname(HOSTNAME);

  // AP ЗАПУСКАЄТЬСЯ ЗАВЖДИ ПЕРШИМ
  const char* apPass = (config.apPassword.length() >= 8) ? config.apPassword.c_str() : NULL;
  bool apOk = WiFi.softAP(AP_SSID, apPass);
  Serial.printf("[AP] %s | SSID: %s | Pass: %s | IP: %s\n",
                apOk ? "✅ OK" : "❌ FAILED", AP_SSID, apPass ? apPass : "[OPEN]", WiFi.softAPIP().toString().c_str());
  state.apActive = apOk;
  state.apStartTime = millis();

  // ПІДКЛЮЧЕННЯ ДО WiFi
  if (config.ssid.length() > 0) {
    Serial.printf("[WiFi] Connecting: %s\n", config.ssid.c_str());
    WiFi.begin(config.ssid.c_str(), config.password.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WiFi] ✓ Connected | IP: %s | RSSI: %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      Serial.printf("[WiFi] ✗ Failed (status=%d)\n", WiFi.status());
    }
  }

  // ТЕСТОВЕ СКАНУВАННЯ
  Serial.println("\n[DEBUG] Тестове сканування:");
  int testScan = WiFi.scanNetworks();
  Serial.printf("  Знайдено: %d мереж\n", testScan);
  if (testScan > 0) {
    for (int i = 0; i < testScan && i < 5; i++) {
      Serial.printf("    %d. %s (%d dBm)\n", i+1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
    WiFi.scanDelete();
  }
  Serial.println();

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  setupWebServer();
  ESP.wdtEnable(30000);
  watchdog.attach(1, []() { ESP.wdtFeed(); });
  state.lastPingTime = millis() - PING_INTERVAL;

  Serial.println("[SYS] ✓ Ready");
  Serial.printf("[RAM] %d bytes\n\n", ESP.getFreeHeap());
}

void loop() {
  yield();
  server.handleClient();
  dnsServer.processNextRequest();
  checkAndReconnectWiFi();
  checkAPTimeout();
  checkInternetAccess();
  if (millis() - state.lastPingTime >= PING_INTERVAL) {
    performPings();
  }
  resetBootCounter();
}
