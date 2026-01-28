#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <time.h>
#include <DNSServer.h>

// ========================================
// SVITLOBOT v3.1.0 FINAL - Simple & Stable
// ========================================

const char* AP_SSID = "Svitlobot-Setup";
const char* HOSTNAME = "svitlobot";
const int DNS_PORT = 53;

const unsigned long PING_INTERVAL = 90000;          // 90 секунд
const unsigned long WIFI_CHECK_INTERVAL = 30000;    // 30 секунд
const unsigned long AP_TIMEOUT = 900000;            // 15 хвилин

AsyncWebServer server(80);
DNSServer dnsServer;
Preferences prefs;

struct SystemState {
  String svitlobotStatus = "⏳ Очікування";
  String urlStatus       = "⏳ Очікування";
  bool internetAccess    = false;

  unsigned long lastPingTime  = 0;
  unsigned long lastWiFiCheck = 0;
  unsigned long apStartTime   = 0;

  int wifiFailCount = 0;
  int bootCount     = 0;

  bool scanInProgress = false;
  int16_t scanResult  = -2;

  bool apActive = false;
} state;

struct Config {
  String ssid;
  String password;
  String channelKey;
  String healthUrl;
  String adminPassword;
  String apPassword;
} config;

// ========================================
// УТИЛІТИ
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

float getCpuTempC() {
  return temperatureRead();
}

void loadConfig() {
  prefs.begin("svitlobot", false);
  config.ssid          = prefs.getString("ssid", "");
  config.password      = prefs.getString("password", "");
  config.channelKey    = prefs.getString("channelKey", "");
  config.healthUrl     = prefs.getString("healthUrl", "");
  config.adminPassword = prefs.getString("adminPass", "admin");
  config.apPassword    = prefs.getString("apPass", "");
  state.bootCount      = prefs.getInt("bootCount", 0);
  prefs.end();

  Serial.printf("[CONFIG] SSID: %s | Key: %s\n",
                config.ssid.c_str(),
                config.channelKey.length() > 0 ? "✓" : "✗");
}

void saveConfig() {
  prefs.begin("svitlobot", false);
  prefs.putString("ssid",       config.ssid);
  prefs.putString("password",   config.password);
  prefs.putString("channelKey", config.channelKey);
  prefs.putString("healthUrl",  config.healthUrl);
  prefs.putString("adminPass",  config.adminPassword);
  prefs.putString("apPass",     config.apPassword);
  prefs.end();
  Serial.println("[CONFIG] ✓ Збережено");
}

void factoryReset() {
  Serial.println("[!!!] FACTORY RESET");
  prefs.begin("svitlobot", false);
  prefs.clear();
  prefs.end();
  delay(500);
  ESP.restart();
}

// ========================================
// БЕЗПЕКА
// ========================================

bool checkAuth(AsyncWebServerRequest *request) {
  if (config.adminPassword.length() == 0 || config.adminPassword == "admin") {
    return true;
  }
  if (request->hasHeader("Cookie")) {
    String cookie = request->header("Cookie");
    if (cookie.indexOf("auth=" + config.adminPassword) >= 0) {
      return true;
    }
  }
  return false;
}

String getLoginPage(String error = "") {
  String html = F(
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Вхід - Svitlobot</title>"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{background:linear-gradient(135deg,#0a0a1a 0%,#1a1a2e 100%);color:#e8e8e8;"
    "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
    "min-height:100vh;display:flex;justify-content:center;align-items:center;padding:15px}"
    ".container{background:rgba(25,25,40,0.95);backdrop-filter:blur(15px);padding:30px;"
    "border-radius:24px;width:100%;max-width:400px;box-shadow:0 10px 40px rgba(0,250,250,0.2);"
    "border:1px solid rgba(0,250,250,0.15)}"
    "h1{text-align:center;background:linear-gradient(135deg,#00fafa 0%,#00d4ff 100%);"
    "-webkit-background-clip:text;-webkit-text-fill-color:transparent;font-size:32px;"
    "font-weight:800;margin-bottom:30px}"
    "input{width:100%;padding:15px;margin:12px 0;background:rgba(35,35,55,0.7);color:#fff;"
    "border:2px solid rgba(0,250,250,0.25);border-radius:12px;font-size:14px;outline:none}"
    "input:focus{border-color:#00fafa;box-shadow:0 0 0 4px rgba(0,250,250,0.12)}"
    ".btn{width:100%;padding:16px;margin:15px 0;border:none;border-radius:12px;cursor:pointer;"
    "font-weight:700;background:linear-gradient(135deg,#00fafa 0%,#00c3ff 100%);color:#000;"
    "font-size:15px;text-transform:uppercase}"
    ".error{background:#dc3545;color:#fff;padding:12px;border-radius:8px;margin-bottom:15px;"
    "text-align:center}"
    "</style></head><body><div class='container'><h1>🔐 ВХІД</h1>"
  );
  if (error.length() > 0) {
    html += "<div class='error'>" + error + "</div>";
  }
  html += F(
    "<form action='/login' method='POST'>"
    "<input type='password' name='password' placeholder='Пароль адміністратора' required autofocus>"
    "<button type='submit' class='btn'>Увійти</button>"
    "</form></div></body></html>"
  );
  return html;
}

// ========================================
// МЕРЕЖА: ПРОСТА ЛОГІКА ЯК У 2.9.0
// ========================================

void checkAPTimeout() {
  if (state.apActive && WiFi.isConnected() &&
      millis() - state.apStartTime > AP_TIMEOUT) {
    Serial.println("[AP] 15 хв - вимикаємо AP");
    WiFi.softAPdisconnect(true);
    state.apActive = false;
  }
}

String httpErrText(int code) {
  if (code == 200) return "✅ OK";
  if (code == -1)  return "❌ Таймаут";
  if (code == -2)  return "❌ NET";
  if (code == -3)  return "❌ DNS";
  if (code == -11) return "❌ SSL Timeout";
  if (code >= 500) return "❌ Сервер " + String(code);
  if (code >= 400) return "❌ Помилка " + String(code);
  if (code > 0)    return "❌ Код " + String(code);
  return "❌ Помилка " + String(code);
}

// СПРОЩЕНА ФУНКЦІЯ ПІНГІВ - ЯК У 2.9.0, АЛЕ З ПРАВИЛЬНИМ HTTPS
// ВИПРАВЛЕНА ФУНКЦІЯ БЕЗ setBufferSizes - СУМІСНА ЗІ СТАРИМИ ЯДРАМИ
void performPings() {
  if (WiFi.status() != WL_CONNECTED) {
    state.svitlobotStatus = "❌ WiFi";
    state.urlStatus       = "❌ WiFi";
    state.internetAccess  = false;
    return;
  }

  Serial.println("\n[PING] Старт...");

  // ПІНГ 1: Svitlobot API (HTTP - швидкий і простий)
  if (config.channelKey.length() > 5) {
    HTTPClient http;
    http.begin("http://api.svitlobot.in.ua/channelPing?channel_key=" + config.channelKey);
    http.setTimeout(5000);
    http.addHeader("User-Agent", "ESP32-Svitlobot/3.1");
    
    int code = http.GET();
    state.svitlobotStatus = httpErrText(code);
    http.end();
    
    Serial.printf("[PING] Svitlobot: %s (%d)\n", state.svitlobotStatus.c_str(), code);
  } else {
    state.svitlobotStatus = "⚪ Не налаштовано";
  }

  // Невелика пауза між запитами
  delay(100);

  // ПІНГ 2: Health URL
  if (config.healthUrl.length() > 5 && config.healthUrl.startsWith("http")) {
    
    if (config.healthUrl.startsWith("https")) {
      // HTTPS - без setBufferSizes для сумісності
      WiFiClientSecure sClient;
      sClient.setInsecure(); // Вимикаємо перевірку сертифікатів
      
      HTTPClient https;
      if (https.begin(sClient, config.healthUrl)) {
        https.setTimeout(10000); // 10 секунд для SSL
        https.addHeader("User-Agent", "Svitlobot/3.1");
        
        int code = https.GET();
        state.urlStatus = httpErrText(code);
        https.end();
        
        Serial.printf("[PING] Health HTTPS: %s (%d)\n", state.urlStatus.c_str(), code);
      } else {
        state.urlStatus = "❌ SSL Init";
        Serial.println("[PING] Health: SSL init failed");
      }
      
      // Примусово закриваємо з'єднання
      sClient.stop();
      
    } else {
      // HTTP - простий випадок
      HTTPClient http;
      if (http.begin(config.healthUrl)) {
        http.setTimeout(5000);
        http.addHeader("User-Agent", "Svitlobot/3.1");
        
        int code = http.GET();
        state.urlStatus = httpErrText(code);
        http.end();
        
        Serial.printf("[PING] Health HTTP: %s (%d)\n", state.urlStatus.c_str(), code);
      }
    }
    
  } else {
    state.urlStatus = "⚪ Не налаштовано";
  }

  // Оновлюємо статус інтернету
  state.internetAccess = (state.svitlobotStatus.indexOf("OK") >= 0 || 
                          state.urlStatus.indexOf("OK") >= 0);

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

void startAsyncWiFiScan() {
  if (state.scanInProgress) return;
  WiFi.scanDelete();
  state.scanResult = WiFi.scanNetworks(true, false, false, 300);
  state.scanInProgress = true;
}

String getWiFiScanResults() {
  if (state.scanInProgress) {
    state.scanResult = WiFi.scanComplete();

    if (state.scanResult == WIFI_SCAN_RUNNING) {
      return "{\"status\":\"scanning\"}";
    } else if (state.scanResult >= 0) {
      state.scanInProgress = false;

      String json = "[";
      for (int i = 0; i < state.scanResult; i++) {
        if (i > 0) json += ",";
        String ssid = WiFi.SSID(i);
        ssid.replace("\"", "\\\"");
        int rssi = WiFi.RSSI(i);

        String quality;
        if (rssi > -50)      quality = "📶 Відмінно";
        else if (rssi > -70) quality = "📶 Добре";
        else                 quality = "📶 Слабко";

        json += "{\"s\":\"" + ssid + "\",\"r\":" + String(rssi) + ",\"q\":\"" + quality + "\"}";
      }
      json += "]";
      WiFi.scanDelete();
      return json;
    } else {
      state.scanInProgress = false;
      return "[]";
    }
  }

  startAsyncWiFiScan();
  return "{\"status\":\"scanning\"}";
}

// ========================================
// WEB UI
// ========================================

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
h1::after{content:'';position:absolute;bottom:0;left:50%;transform:translateX(-50%);
width:80px;height:4px;background:linear-gradient(90deg,transparent,#00fafa,transparent);
border-radius:3px;box-shadow:0 0 15px #00fafa}
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
input::placeholder{color:#7a7a8a}
label{display:block;margin:18px 0 8px;color:#b5b5c0;font-size:13px;font-weight:600;
text-transform:uppercase;letter-spacing:0.5px}
.btn{display:block;width:100%;padding:16px;margin:10px 0;border:none;border-radius:12px;cursor:pointer;
font-weight:700;text-align:center;text-decoration:none;color:#fff;font-size:15px;transition:all 0.3s ease;
position:relative;overflow:hidden;text-transform:uppercase;letter-spacing:0.5px}
.btn::before{content:'';position:absolute;top:50%;left:50%;width:0;height:0;border-radius:50%;
background:rgba(255,255,255,0.15);transition:width 0.5s,height 0.5s,top 0.5s,left 0.5s}
.btn:hover::before{width:320px;height:320px;top:calc(50% - 160px);left:calc(50% - 160px)}
.btn:active{transform:scale(0.97)}
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
@media(max-width:480px){.container{padding:22px;border-radius:18px}h1{font-size:26px}
.stat{font-size:13px;padding:10px 12px}}
.pulse{animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.6}}
.section{margin:25px 0;padding:20px;background:rgba(15,15,30,0.6);border-radius:12px;
border:1px solid rgba(0,250,250,0.1)}
.section-title{color:#00fafa;font-size:16px;font-weight:700;margin-bottom:15px;
text-transform:uppercase;letter-spacing:1px}
</style>
)rawliteral";

String processTemplate(const String& html) {
  String result = html;
  result.replace("%CSS%",           FPSTR(HTML_CSS));
  result.replace("%SSID%",          config.ssid.length() > 0 ? config.ssid : "Не підключено");
  result.replace("%IP%",            WiFi.isConnected() ? WiFi.localIP().toString() : "—");
  result.replace("%SVITLOBOT%",     state.svitlobotStatus);
  result.replace("%URL%",           state.urlStatus);
  result.replace("%INTERNET%",      state.internetAccess ? "✅ Активний" : "❌ Недоступний");
  result.replace("%RSSI%",          WiFi.isConnected() ? String(WiFi.RSSI()) + " dBm" : "—");
  result.replace("%UPTIME%",        formatUptime(millis()));
  result.replace("%TEMP%",          String(getCpuTempC(), 1));
  result.replace("%HEAP%",          String(ESP.getFreeHeap()));
  result.replace("%KEY%",           config.channelKey);
  result.replace("%HEALTHURL%",     config.healthUrl);
  result.replace("%CURRENT_SSID%",  config.ssid);
  result.replace("%AP_STATUS%",     state.apActive ? "✅" : "❌");
  return result;
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = F(
      "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Svitlobot v3.1</title>%CSS%</head><body>"
      "<div class='container'><h1>⚡ SVITLOBOT</h1><div class='stats'>"
      "<div class='stat'><span class='stat-label'>📡 WiFi</span><span class='stat-value'>%SSID%</span></div>"
      "<div class='stat'><span class='stat-label'>📟 IP</span><span class='stat-value'>%IP%</span></div>"
      "<div class='stat'><span class='stat-label'>🔵 Svitlobot</span><span class='stat-value pulse'>%SVITLOBOT%</span></div>"
      "<div class='stat'><span class='stat-label'>🟢 Health</span><span class='stat-value pulse'>%URL%</span></div>"
      "<div class='stat'><span class='stat-label'>🌍 Інтернет</span><span class='stat-value'>%INTERNET%</span></div>"
      "<div class='stat'><span class='stat-label'>📶 Сигнал</span><span class='stat-value'>%RSSI%</span></div>"
      "<div class='stat'><span class='stat-label'>⏱️ Uptime</span><span class='stat-value' id='up'>%UPTIME%</span></div>"
      "<div class='stat'><span class='stat-label'>🌡️ Темп.</span><span class='stat-value'>%TEMP% °C</span></div>"
      "<div class='stat'><span class='stat-label'>💾 RAM</span><span class='stat-value'>%HEAP% b</span></div>"
      "</div>"
      "<a href='/config' class='btn btn-cyan'>⚙️ Налаштування</a>"
      "<button onclick='if(confirm(\"Reboot?\"))fetch(\"/reboot\").then(()=>alert(\"OK\"))' class='btn btn-info'>🔄 Reboot</button>"
      "<button onclick='location.reload()' class='btn btn-primary'>🔃 Refresh</button>"
      "</div>"
      "<script>setInterval(()=>fetch('/uptime').then(r=>r.text()).then(d=>document.getElementById('up').innerText=d),5000)</script>"
      "</body></html>"
    );
    request->send(200, "text/html; charset=utf-8", processTemplate(html));
  });

  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html; charset=utf-8", getLoginPage());
  });

  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
    String password = "";
    if (request->hasParam("password", true)) {
      password = request->getParam("password", true)->value();
    }
    if (password == config.adminPassword || (config.adminPassword == "admin" && password == "admin")) {
      AsyncWebServerResponse *response = request->beginResponse(302);
      response->addHeader("Location", "/config");
      response->addHeader("Set-Cookie", "auth=" + config.adminPassword + "; Path=/; Max-Age=3600");
      request->send(response);
    } else {
      request->send(200, "text/html; charset=utf-8", getLoginPage("❌ Невірний пароль!"));
    }
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) { request->redirect("/login"); return; }
    String html = F(
      "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Config</title>%CSS%</head><body>"
      "<div class='container'><h1>⚙️ CONFIG</h1>"
      "<form action='/save' method='POST'>"
      "<div class='section'><div class='section-title'>WiFi</div>"
      "<select name='ssid' id='ss' required><option>Loading...</option></select>"
      "<input type='password' name='pass' placeholder='Password (min 8)'></div>"
      "<div class='section'><div class='section-title'>Security</div>"
      "<input type='password' name='admin' placeholder='Admin password'>"
      "<input type='password' name='appass' placeholder='AP password (min 8)'></div>"
      "<div class='section'><div class='section-title'>API</div>"
      "<input type='text' name='key' value='%KEY%' placeholder='Svitlobot key'>"
      "<input type='text' name='url' value='%HEALTHURL%' placeholder='Health URL'></div>"
      "<button type='submit' class='btn btn-primary'>💾 Save</button>"
      "</form>"
      "<button onclick='fetch(\"/toggle-ap\").then(r=>r.text()).then(alert)' class='btn btn-warning'>📡 Toggle AP</button>"
      "<button onclick='if(confirm(\"RESET ALL?\"))location.href=\"/factory-reset\"' class='btn btn-danger'>⚠️ Reset</button>"
      "<a href='/' class='btn btn-info'>⬅️ Back</a>"
      "</div>"
      "<script>"
      "window.onload=()=>{load();setInterval(load,3000)};"
      "function load(){fetch('/scan').then(r=>r.json()).then(d=>{"
      "if(d.status==='scanning')return;"
      "let s=document.getElementById('ss');s.innerHTML='';"
      "if(!Array.isArray(d))return;"
      "d.forEach(i=>{let o=new Option(i.s+' ('+i.q+')',i.s);if(i.s==='%CURRENT_SSID%')o.selected=true;s.add(o)})})}"
      "</script></body></html>"
    );
    request->send(200, "text/html; charset=utf-8", processTemplate(html));
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) { request->redirect("/login"); return; }
    
    bool restart = false;
    
    if (request->hasParam("ssid", true)) {
      String s = request->getParam("ssid", true)->value();
      if (s != config.ssid) { config.ssid = s; restart = true; }
    }
    if (request->hasParam("pass", true)) {
      String p = request->getParam("pass", true)->value();
      if (p.length() >= 8) { config.password = p; restart = true; }
    }
    if (request->hasParam("key", true)) config.channelKey = request->getParam("key", true)->value();
    if (request->hasParam("url", true)) config.healthUrl = request->getParam("url", true)->value();
    if (request->hasParam("admin", true)) {
      String a = request->getParam("admin", true)->value();
      if (a.length() > 0) config.adminPassword = a;
    }
    if (request->hasParam("appass", true)) {
      String ap = request->getParam("appass", true)->value();
      if (ap.length() >= 8 || ap.length() == 0) config.apPassword = ap;
    }
    
    saveConfig();
    
    String resp = String("<html><head>") + FPSTR(HTML_CSS) + "</head><body><div class='container'><h1>✅ OK</h1>";
    if (restart) {
      resp += "<p style='text-align:center'>Reboot in <span id='c'>3</span>s</p>"
              "<script>let t=3;setInterval(()=>{t--;document.getElementById('c').innerText=t;if(t<=0)location.href='/'},1000)</script>";
    } else {
      resp += "<a href='/config' class='btn btn-info'>Back</a>";
    }
    resp += "</div></body></html>";
    
    request->send(200, "text/html; charset=utf-8", resp);
    
    if (restart) { delay(3000); ESP.restart(); }
  });

  server.on("/toggle-ap", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) { request->send(403); return; }
    if (state.apActive) {
      WiFi.softAPdisconnect(true);
      state.apActive = false;
      request->send(200, "text/plain", "AP OFF");
    } else {
      WiFi.softAP(AP_SSID, config.apPassword.length() >= 8 ? config.apPassword.c_str() : NULL);
      state.apActive = true;
      state.apStartTime = millis();
      request->send(200, "text/plain", "AP ON");
    }
  });

  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", getWiFiScanResults());
  });

  server.on("/uptime", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", formatUptime(millis()));
  });

  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "OK");
    delay(500);
    ESP.restart();
  });

  server.on("/factory-reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) { request->redirect("/login"); return; }
    request->send(200, "text/plain", "RESETTING...");
    delay(1000);
    factoryReset();
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("http://" + WiFi.softAPIP().toString() + "/");
  });

  server.begin();
  Serial.println("[HTTP] ✓ Started");
}

// ========================================
// RESCUE MODE
// ========================================

void checkRescueMode() {
  prefs.begin("svitlobot", false);
  state.bootCount = prefs.getInt("bootCount", 0);

  if (state.bootCount >= 4) {
    Serial.println("[RESCUE] Reset...");
    prefs.clear();
    prefs.end();
    delay(2000);
    ESP.restart();
  }

  state.bootCount++;
  prefs.putInt("bootCount", state.bootCount);
  prefs.end();
}

void resetBootCounter() {
  prefs.begin("svitlobot", false);
  if (prefs.getInt("bootCount", 0) != 0) {
    prefs.putInt("bootCount", 0);
  }
  prefs.end();
}

// ========================================
// SETUP & LOOP
// ========================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║ SVITLOBOT v3.1.0 FINAL        ║");
  Serial.println("║ Simple & Stable (no OTA)      ║");
  Serial.println("╚════════════════════════════════╝\n");

  checkRescueMode();
  loadConfig();

  // КРИТИЧНО: правильний порядок ініціалізації WiFi
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);

  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false);  // ЦЕ КЛЮЧ ДО СТАБІЛЬНОСТІ!

  if (config.ssid.length() > 0) {
    Serial.printf("[WiFi] Connecting: %s\n", config.ssid.c_str());
    WiFi.begin(config.ssid.c_str(), config.password.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.isConnected()) {
      Serial.printf("[WiFi] ✓ Connected\n       IP: %s\n       RSSI: %d\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
  }

  if (!WiFi.isConnected()) {
    WiFi.softAP(AP_SSID, config.apPassword.length() >= 8 ? config.apPassword.c_str() : NULL);
    state.apActive = true;
    state.apStartTime = millis();
    Serial.printf("[AP] ✓ %s | %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  }

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[mDNS] ✓ http://%s.local\n", HOSTNAME);
  }

  setupWebServer();
  
  configTime(2 * 3600, 3600, "pool.ntp.org");

  // НЕ викликаємо пінг одразу - даємо 5 секунд на стабілізацію
  state.lastPingTime = millis() - (PING_INTERVAL - 5000);

  Serial.println("\n[SYS] ✓ Ready\n");
  Serial.printf("[RAM] Free: %d bytes\n\n", ESP.getFreeHeap());
}

void loop() {
  dnsServer.processNextRequest();
  
  checkAndReconnectWiFi();
  checkAPTimeout();

  if (millis() - state.lastPingTime >= PING_INTERVAL) {
    performPings();
  }

  static bool bootReset = false;
  if (!bootReset && millis() > 30000) {
    resetBootCounter();
    bootReset = true;
  }

  delay(10);
}
