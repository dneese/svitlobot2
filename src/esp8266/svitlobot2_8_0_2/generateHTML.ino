// Обробка кореневого шляху
void handleRoot() {
  String html = generateHTML();
  server.send(200, "text/html", html);
}


void handleGame() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='en'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Игра с конфетти</title>";
  html += "<style>";
  html += "body { margin: 0; overflow: hidden; background-color: #000; font-family: Arial, sans-serif; }";
  html += ".confetti-container { position: absolute; width: 100%; height: 100vh; perspective: 1000px; overflow: hidden; }";
  html += ".confetti { position: absolute; background-color: #f00; animation: fall 10s linear; transform-origin: center center; background-size: cover; box-shadow: 0 0 5px rgba(0, 0, 0, 0.5); }";
  html += "@keyframes fall { 0% { transform: translateY(-100%) rotateX(0) rotateY(0) rotateZ(0); } 100% { transform: translateY(100vh) rotateX(720deg) rotateY(720deg) rotateZ(720deg); } }";
  html += ".score { position: absolute; top: 10px; right: 10px; color: white; font-size: 24px; }";
  html += ".firework { position: absolute; width: 5px; height: 5px; background-color: #f00; border-radius: 50%; animation: explode 1s linear infinite; }";
  html += "@keyframes explode { 0% { transform: scale(1); opacity: 1; } 100% { transform: scale(10); opacity: 0; } }";
  html += ".victory-message { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); color: white; font-size: 48px; display: none; text-shadow: 0 0 10px #fff, 0 0 20px #fff, 0 0 30px #ff00de, 0 0 40px #ff00de, 0 0 50px #ff00de, 0 0 60px #ff00de, 0 0 70px #ff00de; animation: glow 1s ease-in-out infinite alternate; }";
  html += "@keyframes glow { from { text-shadow: 0 0 10px #fff, 0 0 20px #fff, 0 0 30px #ff00de, 0 0 40px #ff00de, 0 0 50px #ff00de, 0 0 60px #ff00de, 0 0 70px #ff00de; } to { text-shadow: 0 0 20px #fff, 0 0 30px #ff4da6, 0 0 40px #ff4da6, 0 0 50px #ff4da6, 0 0 60px #ff4da6, 0 0 70px #ff4da6, 0 0 80px #ff4da6; } }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='confetti-container' id='confetti-container'></div>";
  html += "<div class='score' id='score'>Score: 0</div>";
  html += "<div class='victory-message' id='victory-message'>Світлобот</div>";
  html += "<script>";
  html += "const container = document.getElementById('confetti-container');";
  html += "const scoreElement = document.getElementById('score');";
  html += "const victoryMessage = document.getElementById('victory-message');";
  html += "let score = 0;";
  html += "const maxConfetti = 20;";
  html += "let stopConfetti = false;";
  html += "function createConfetti() {";
  html += "if (container.childElementCount < maxConfetti && !stopConfetti) {";
  html += "const confetti = document.createElement('div');";
  html += "confetti.classList.add('confetti');";
  html += "confetti.style.left = Math.random() * 100 + 'vw';";
  html += "confetti.style.backgroundColor = `hsl(${Math.random() * 360}, 100%, 50%)`;";
  html += "confetti.style.animationDuration = Math.random() * 10 + 5 + 's';";
  html += "confetti.style.animationDelay = Math.random() * 2 + 's';";
  html += "confetti.style.animationTimingFunction = `cubic-bezier(${Math.random()}, ${Math.random()}, ${Math.random()}, ${Math.random()})`;";
  html += "confetti.style.transform = `rotateX(${Math.random() * 360}deg) rotateY(${Math.random() * 360}deg) rotateZ(${Math.random() * 360}deg)`;";
  html += "confetti.style.zIndex = Math.random() * 100;";
  html += "const size = Math.random() * 40 + 20;";
  html += "confetti.style.width = size + 'px';";
  html += "confetti.style.height = size + 'px';";
  html += "const shape = Math.random() > 0.75 ? 'circle' : (Math.random() > 0.5 ? 'square' : (Math.random() > 0.25 ? 'star' : 'triangle'));";
  html += "confetti.style.borderRadius = shape === 'circle' ? '50%' : (shape === 'square' ? '0' : (shape === 'star' ? '50%' : '0 0 50% 50%'));";
  html += "if (shape === 'star') {";
  html += "confetti.style.clipPath = 'polygon(50% 0%, 61% 35%, 98% 35%, 68% 57%, 79% 91%, 50% 70%, 21% 91%, 32% 57%, 2% 35%, 39% 35%)';";
  html += "} else if (shape === 'triangle') {";
  html += "confetti.style.clipPath = 'polygon(50% 0%, 0% 100%, 100% 100%)';";
  html += "}";
  html += "if (Math.random() > 0.5) {";
  html += "const gradient = `linear-gradient(${Math.random() * 360}deg, hsl(${Math.random() * 360}, 100%, 50%), hsl(${Math.random() * 360}, 100%, 50%))`;";
  html += "confetti.style.background = gradient;";
  html += "}";
  html += "if (Math.random() > 0.5) {";
  html += "confetti.style.animationName = 'fall, colorChange';";
  html += "}";
  html += "if (Math.random() > 0.5) {";
  html += "confetti.style.animationName += ', glow';";
  html += "}";
  html += "if (Math.random() > 0.5) {";
  html += "confetti.style.animationName += ', sparkle';";
  html += "}";
  html += "if (Math.random() > 0.5) {";
  html += "confetti.style.animationName += ', spin';";
  html += "}";
  html += "if (Math.random() > 0.5) {";
  html += "confetti.style.animationName += ', pulse';";
  html += "}";
  html += "confetti.style.boxShadow = `0 0 5px rgba(0, 0, 0, 0.5), 0 0 10px rgba(255, 255, 255, 0.5)`;";
  html += "container.appendChild(confetti);";
  html += "confetti.addEventListener('animationend', () => {";
  html += "confetti.remove();";
  html += "});";
  html += "confetti.addEventListener('click', () => {";
  html += "confetti.remove();";
  html += "score++;";
  html += "scoreElement.textContent = `Score: ${score}`;";
  html += "if (score >= 100) {";
  html += "stopConfetti = true;";
  html += "showFireworks();";
  html += "showVictoryMessage();";
  html += "}";
  html += "});";
  html += "}";
  html += "}";
  html += "function showFireworks() {";
  html += "for (let i = 0; i < 100; i++) {";
  html += "const firework = document.createElement('div');";
  html += "firework.classList.add('firework');";
  html += "firework.style.left = Math.random() * 100 + 'vw';";
  html += "firework.style.top = Math.random() * 100 + 'vh';";
  html += "firework.style.backgroundColor = `hsl(${Math.random() * 360}, 100%, 50%)`;";
  html += "container.appendChild(firework);";
  html += "firework.addEventListener('animationend', () => {";
  html += "firework.remove();";
  html += "});";
  html += "}";
  html += "setInterval(() => {";
  html += "for (let i = 0; i < 10; i++) {";
  html += "const firework = document.createElement('div');";
  html += "firework.classList.add('firework');";
  html += "firework.style.left = Math.random() * 100 + 'vw';";
  html += "firework.style.top = Math.random() * 100 + 'vh';";
  html += "firework.style.backgroundColor = `hsl(${Math.random() * 360}, 100%, 50%)`;";
  html += "container.appendChild(firework);";
  html += "firework.addEventListener('animationend', () => {";
  html += "firework.remove();";
  html += "});";
  html += "}";
  html += "}, 1000);";
  html += "}";
  html += "function showVictoryMessage() {";
  html += "victoryMessage.style.display = 'block';";
  html += "}";
  html += "setInterval(createConfetti, 200);";
  html += "</script>";
  html += "</body>";
  html += "</html>";
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
  if (days > 0) {
    uptimeString += String(days) + " дн. ";
  }
  if (hours > 0) {
    uptimeString += String(hours) + " год. ";
  }
  if (minutes > 0) {
    uptimeString += String(minutes) + " хв.";
  } else {
    uptimeString += "0 хв.";
  }

  return uptimeString;
}

String generateHTML() {
  String token = readStringFromEEPROM(TOKEN_ADDR);
  String html = "<!DOCTYPE html>";
  html += "<html lang='uk'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Світлобот</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; color: #fff; }";
  html += ".container { background-color: #333; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(255, 255, 255, 0.1); width: 90%; max-width: 400px; }";
  html += "h1 { text-align: center; color: #fff; margin-bottom: 20px; font-size: 1.5em; position: relative; cursor: pointer; }";
  html += "h1::before { content: ''; position: absolute; top: -10px; left: -10px; right: -10px; bottom: -10px; border: 2px solid #00fafa; border-radius: 8px; animation: glow 2s infinite; }";
  html += "@keyframes glow { 0% { box-shadow: 0 0 10px #00fafa; } 50% { box-shadow: 0 0 20px #00fafa; } 100% { box-shadow: 0 0 10px #00fafa; } }";
  html += ".button-container { display: flex; flex-direction: column; align-items: center; }";
  html += "button, .button { padding: 10px; background-color: #28a745; color: #fff; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 0.9em; text-align: center; text-decoration: none; margin-bottom: 10px; }";
  html += "button:hover, .button:hover { background-color: #218838; }";
  html += ".clear-button { background-color: #dc3545; margin-top: 10px; font-size: 0.9em; }";
  html += ".clear-button:hover { background-color: #c82333; }";
  html += ".diagnostic { margin-bottom: 20px; }";
  html += ".diagnostic p { margin: 5px 0; font-size: 0.8em; color: #ccc; }";
  html += ".loader { display: flex; justify-content: center; margin-top: 20px; }";
  html += ".dot { width: 10px; height: 10px; border-radius: 50%; margin: 0 5px; animation: bounce 1.5s infinite ease-in-out; }";
  html += ".dot:nth-child(1) { background-color: #ff6384; }";
  html += ".dot:nth-child(2) { background-color: #36a2eb; animation-delay: 0.2s; }";
  html += ".dot:nth-child(3) { background-color: #ffce56; animation-delay: 0.4s; }";
  html += "@keyframes bounce {";
  html += "0%, 80%, 100% { transform: scale(0); }";
  html += "40% { transform: scale(1); }";
  html += "}";
  html += ".green-text { color: green; }"; // Додаємо стиль для зеленого кольору
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1 onclick='window.location.href=\"/game\"'>Світлобот</h1>";
  html += "<div class='diagnostic'>";
  html += "<p>📡 Збережений SSID: " + readStringFromEEPROM(SSID_ADDR) + "</p>";
  html += "<p>🔑 Ключ svitlobot: " + readStringFromEEPROM(CHANNEL_KEY_ADDR) + "</p>";
  html += "<p>🌐 Токен hc-ping: " + readStringFromEEPROM(TOKEN_ADDR) + "</p>";
  html += "<p id='last-ping-time'>⏱️ Час з останнього пінгу: </p>"; // додаємо ID до абзацу
  html += "<p>📶 Статус WiFi: " + String(WiFi.status() == WL_CONNECTED ? "✅ Підключено" : "❌ Не підключено") + "</p>";
  html += "<p>🌍 Статус Інтернету: " + String(WiFi.status() == WL_CONNECTED && checkInternetConnection() ? "✅ Доступний" : "❌ Недоступний") + "</p>";
  html += "<p id='uptime'>⏱️ Uptime: " + formatUptime(millis()) + "</p>"; // Додаємо інформацію про uptime
  html += "<p>📟 IP Адреса: " + WiFi.localIP().toString() + "</p>"; // Додаємо інформацію про IP-адресу
  
  // Додаємо інформацію про рівень сигналу WiFi з піктограмою
  int rssi = WiFi.RSSI();
  String signalIcon = "📶";
  if (rssi > -50) {
    signalIcon = "📶📶📶📶📶"; // 5 смужок
  } else if (rssi > -60) {
    signalIcon = "📶📶📶📶"; // 4 смужки
  } else if (rssi > -70) {
    signalIcon = "📶📶📶"; // 3 смужки
  } else if (rssi > -80) {
    signalIcon = "📶📶"; // 2 смужки
  } else {
    signalIcon = "📶"; // 1 смужка
  }
  html += "<p>" + signalIcon + " Рівень сигналу WiFi: " + String(rssi) + " dBm</p>";

  //html += "<p><a href='https://hc-ping.com/" + token + "'>hc-ping.com</a></p>"; // Додаємо посилання з токеном
  html += "</div>";
  html += "<div class='button-container'>";
  html += "<a href='/scan' class='button'>⚙️ Налаштування</a>"; // Кнопка для налаштування WiFi з піктограмою
  html += "<a href='/help' class='green-text'>Довідка</a>"; // Кнопка для довідки та інструкції з класом green-text та піктограмою

  html += "</div>";
  html += "<div class='loader'>";
  html += "<div class='dot'></div>";
  html += "<div class='dot'></div>";
  html += "<div class='dot'></div>";
  html += "</div>";
  html += "</div>";
  html += "<script>";
  html += "setInterval(function() {";
  html += "fetch('/last-ping-time')";
  html += ".then(response => response.text())";
  html += ".then(time => document.getElementById('last-ping-time').innerHTML = '⏱️ Час з останнього пінгу: ' + time + ' сек');";
  html += "fetch('/uptime')";
  html += ".then(response => response.text())";
  html += ".then(uptime => document.getElementById('uptime').innerHTML = '⏱️ Uptime: ' + uptime);";
  html += "}, 5000);";// Оновлення кожні 5 секунд
  html += "</script>";
  html += "</body>";
  html += "</html>";
  return html;
}


// Сканування WiFi мереж
// Сканування WiFi мереж
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
  html += ".button-row button { width: 30%; }"; // Змінено ширину кнопок для кращого вигляду
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>Налаштування WiFi</h1>";
  html += "<form action='/save' method='POST'>";
  html += "<label for='ssid'>Оберіть SSID:</label>";
  html += "<select id='ssid' name='ssid' required>";
  html += "<option value=''>Оберіть мережу</option>";
  html += "</select>";
  html += "<button type='button' onclick='scanNetworks()'>Сканувати мережі</button>";
  html += "<label for='password'>Password:</label>";
  html += "<input type='password' id='password' name='password' placeholder='Пароль вашої мережі wifi' required>";
  html += "<label for='channel_key'>Channel Key:</label>";
  html += "<input type='text' id='channel_key' name='channel_key' placeholder='Ключ світлобота' required>";
  html += "<label for='token'>Token для hc-ping.com:</label>";
  html += "<input type='text' id='token' name='token' placeholder='Введіть токен' >";
  html += "<label for='ap_password'>Пароль цієї точки доступу:</label>";
  html += "<input type='password' id='ap_password' name='ap_password' placeholder='Необовязково'>";
  html += "<button type='submit'>Зберегти</button>";
  html += "</form>";
  html += "<div class='button-row'>";
  html += "<button onclick='window.location.href=\"/\"' class='green-button'>Головна сторінка</button>";
  html += "<button onclick='confirmClear()' class='red-button'>Скидання</button>";
  html += "<button onclick='rebootDevice()' class='red-button'>Reboot</button>";
  html += "</div>";
  html += "</div>";
  html += "<script>";
  html += "function confirmClear() {";
  html += "if (confirm('Ви впевнені, що хочете очистити EEPROM?')) {";
  html += "fetch('/clear', {method: 'POST'})";
  html += ".then(response => response.text())";
  html += ".then(data => alert(data));";
  html += "}";
  html += "}";
  html += "function rebootDevice() {";
  html += "if (confirm('Ви впевнені, що хочете перезавантажити пристрій?')) {";
  html += "fetch('/reboot', {method: 'POST'})";
  html += ".then(response => response.text())";
  html += ".then(data => alert(data));";
  html += "}";
  html += "}";
  html += "function scanNetworks() {";
  html += "fetch('/scan-networks')";
  html += ".then(response => {";
  html += "if (!response.ok) {";
  html += "throw new Error('Network response was not ok');";
  html += "}";
  html += "return response.json();";
  html += "})";
  html += ".then(data => {";
  html += "var select = document.getElementById('ssid');";
  html += "select.innerHTML = '';";
  html += "data.forEach(network => {";
  html += "var option = document.createElement('option');";
  html += "option.value = network.SSID;";
  html += "option.text = network.SSID + ' (' + network.SignalStrength + ' dBm)';";
  html += "select.appendChild(option);";
  html += "});";
  html += "})";
  html += ".catch(error => {";
  html += "console.error('Error scanning networks:', error);";
  html += "alert('Не вдалося сканувати мережі. Перезавантажте сторінку та спробуйте знову.');";
  html += "});";
  html += "}";
  html += "</script>";
  html += "</body>";
  html += "</html>";
  server.send(200, "text/html", html);
}





// Сканування WiFi мереж
void handleScanNetworks() {
   int n = WiFi.scanNetworks();
  if (n == 0) {
    server.send(200, "application/json", "[]");
    return;
  }

  // Создаем массив индексов для сортировки
  int indices[n];
  for (int i = 0; i < n; i++) {
    indices[i] = i;
  }

  // Сортируем массив индексов по уровню сигнала (RSSI)
  for (int i = 0; i < n - 1; i++) {
    for (int j = 0; j < n - i - 1; j++) {
      if (WiFi.RSSI(indices[j]) < WiFi.RSSI(indices[j + 1])) {
        int temp = indices[j];
        indices[j] = indices[j + 1];
        indices[j + 1] = temp;
      }
    }
  }

  // Формируем JSON с отсортированными сетями
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"SSID\": \"" + WiFi.SSID(indices[i]) + "\",";
    json += "\"SignalStrength\": " + String(WiFi.RSSI(indices[i]));
    json += "}";
  }
  json += "]";

  server.send(200, "application/json", json);
}



// Обробка сторінки довідки
void handleHelp() {
  String html = generateHelpHTML();
  server.send(200, "text/html", html);
}

// HTML-сторінка довідки
String generateHelpHTML() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='uk'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Довідка</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #000; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; color: #fff; }";
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
  html += "<p>Цей пристрій дозволяє пінгувати через ваше WiFi-підключення вашого світлобота.</p>";
  html += "<p>1. Введіть SSID вашої WiFi-мережі.</p>";
  html += "<p>2. Введіть пароль від вашої WiFi-мережі.</p>";
  html += "<p>3. Введіть ключ каналу для пінгу.</p>";
  html += "<p>4. Введіть токен для hc-ping.com.</p>";
  html += "<p>5. За бажанням, введіть пароль для точки доступу.</p>";
  html += "<p>6. Натисніть кнопку 'Зберегти' для збереження налаштувань.</p>";
  html += "<p>7. Ви можете скинути всі налаштування та перезавантажити пристрій, натиснувши кнопку 'Скидання налаштувань і перезавантаження'.</p>";
  html += "<p>8. При перезавантаженні 4 рази підряд, також відбувається очистка пам'яті!</p>"; // Додаємо інформацію про очистку пам'яті
  html += "<p> Версія прошивки 2.8.0.2 </p>"; //
  html += "<p><a href='/'>Повернутися до головної сторінки</a></p>";
  html += "</div>";
  html += "</body>";
  html += "</html>";
  return html;
}

// Обробка збереження налаштувань
void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String channelKey = server.arg("channel_key");
  String apPassword = server.arg("ap_password");
  String token = server.arg("token"); // Отримуємо токен з форми

  if (ssid.length() == 0 || password.length() == 0 || channelKey.length() == 0 ) {   // перевірка чи не пусто || token.length() == 0
    server.send(400, "text/plain", "SSID, Password, Channel Key, and Token are required.");
    return;
  }

  if (ssid.length() > 32 || password.length() > 32 || channelKey.length() > 32 || apPassword.length() > 32 || token.length() > 64) {
    server.send(400, "text/plain", "Input too long.");
    return;
  }

  writeStringToEEPROM(SSID_ADDR, ssid);
  writeStringToEEPROM(PASSWORD_ADDR, password);
  writeStringToEEPROM(CHANNEL_KEY_ADDR, channelKey);
  writeStringToEEPROM(AP_PASSWORD_ADDR, apPassword);
  writeStringToEEPROM(TOKEN_ADDR, token); // Зберігаємо токен

  EEPROM.commit();

  server.send(200, "text/plain", "Settings saved. Rebooting...");
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

