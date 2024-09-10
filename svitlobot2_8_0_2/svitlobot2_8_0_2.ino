#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>

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
const char* DEFAULT_AP_PASSWORD = "";

WiFiClient wifiClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
ESP8266WebServer server(80);
DNSServer dnsServer;

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
  setupDNS();
  setupLED();
}

void loop() {
  server.handleClient();
  dnsServer.processNextRequest();
  handleWiFiConnection();
  updateLEDStatus();
  disableAPAfterTimeout();
  resetBootCountAfterTimeout();
  checkWiFiConnection();
}

// Налаштування клієнта часу
void setupTimeClient() {
  timeClient.begin();
}