// Перевірка наявності інтернету
bool checkInternetConnection() {
  WiFiClient client;
  if (client.connect("www.google.com", 80)) {
    client.stop();
    return true;
  } else {
    return false;
  }
}