// Налаштування веб-сервера
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/clear", handleClearEEPROM);
  server.on("/last-ping-time", handleLastPingTime);
  server.on("/uptime", handleUptime);
  server.on("/game", handleGame);
  server.on("/help", handleHelp);
  server.on("/scan", handleWiFiScan);
  server.on("/scan-networks", handleScanNetworks);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.onNotFound(handleNotFound); // Обробка невідомих запитів
  server.begin();
}

void handleNotFound() {
  server.sendHeader("Location", "http://192.168.4.1", true);
  server.send(302, "text/plain", "");
}

// Налаштування DNS-сервера
void setupDNS() {
 const byte DNS_PORT = 53;
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
}