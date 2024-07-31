#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <TimerOne.h>

// WiFi налаштування
const char* ssid = "ВАШ_ССІД";
const char* password = "ВАШ_ПАРОЛЬ";

// Інтервал між запитами в мілісекундах (4 хвилини)
const unsigned long interval = 240000;
const String channelKey = "ВАШКЛЮЧ";
const String serverURL = "http://api.svitlobot.in.ua/channelPing?channel_key=" + channelKey;

void setup() {
  Serial.begin(9600);                  // Ініціалізація серійного зв'язку
  WiFi.begin(ssid, password);          // Підключення до WiFi

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Підключення до WiFi...");
  }

  Serial.println("Підключено до WiFi");
  Serial.println("IP адреса: ");
  Serial.println(WiFi.localIP());

  Timer1.initialize(interval);         // Ініціалізація таймера
  Timer1.attachInterrupt(makeHTTPRequest); // Прив'язування переривання
}

void loop() {
  // Порожня функція loop
}

void makeHTTPRequest() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    Serial.print("Відправка HTTP-запиту на: ");
    Serial.println(serverURL);

    http.begin(serverURL);           // Встановлення URL для запиту
    int httpCode = http.GET();       // Виконання GET-запиту

    if (httpCode > 0) { // Перевірка статусу відповіді
      String payload = http.getString();
      Serial.println(httpCode);      // Виведення статусного коду
      Serial.println(payload);       // Виведення відповіді
    } else {
      Serial.println("Помилка HTTP-запиту");
    }

    http.end(); // Завершення запиту
  } else {
    Serial.println("Не підключено до WiFi");
  }
}
