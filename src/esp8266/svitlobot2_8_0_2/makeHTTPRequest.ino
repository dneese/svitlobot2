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

// Обробка запиту на час останнього пінгу
void handleLastPingTime() {
  unsigned long currentTime = millis();
  unsigned long difference = (currentTime - lastPingTime) / 1000; // переводимо в секунди
  server.send(200, "text/plain", String(difference));
}