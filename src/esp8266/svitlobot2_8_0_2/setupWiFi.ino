// Налаштування WiFi
void setupWiFi() {
  WiFi.begin(readStringFromEEPROM(SSID_ADDR).c_str(), readStringFromEEPROM(PASSWORD_ADDR).c_str());
  if (WiFi.status() != WL_CONNECTED) {
    createAccessPoint();
    apStartTime = millis();
    server.on("/scan", handleWiFiScan); // Перенаправляємо 
  } else {
    Serial.print("Веб-сервер доступний за адресою: http://");
    Serial.println(WiFi.localIP());
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


// Вимкнення точки доступу після таймауту
void disableAPAfterTimeout() {
  if (apStartTime > 0 && (millis() - apStartTime) > 900000) { // 600000 мс = 10 хвилин
   // server.stop();
    WiFi.softAPdisconnect(true);
    apStartTime = 0;
  }
}