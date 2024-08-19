# Wemos D1 Mini ESP8266 HTTP Request для Світлоботу та healthchecks

скриншот
<p align="center">
<img src="https://github.com/dneese/Svitlobot2/blob/main/IMG_20240806_075333_580.jpg" alt="скиншот 1" width="300"/>
<img src="https://github.com/dneese/svitlobot2/blob/main/IMG_20240806_075335_721.jpg" alt="скиншот 2" width="300"/>
</p>

# Вебустановщик прямо з браузера
[dneese.github.io/svitlobot2](https://dneese.github.io/svitlobot2/) або [web.esphome.io](https://web.esphome.io)

# Інструкція з прошивання за допомогою ESPHome Flasher

Цей документ надає покрокову інструкцію щодо прошивання `bin` файлу на вашому ESP пристрої за допомогою ESPHome Flasher.

## Вимоги

- Комп'ютер з операційною системою Windows, macOS або Linux.
- USB кабель для підключення вашого ESP пристрою до комп'ютера.
- Завантажений `bin` файл, який ви хочете прошити, наприклад [svitlobot2_7_9_esp8266.generic.bin](https://github.com/dneese/svitlobot2/raw/main/svitlobot2_7_9_esp8266.generic.bin)

- [ESPHome Flasher](https://github.com/esphome/esphome-flasher/releases) — програма для прошивання ESP пристроїв.

## Покрокова інструкція

1. **Завантажте та встановіть ESPHome Flasher:**
   - Перейдіть на [сторінку релізів ESPHome Flasher](https://github.com/esphome/esphome-flasher/releases) і завантажте відповідну версію для вашої операційної системи.
   - Встановіть програму, дотримуючись стандартних інструкцій для вашої операційної системи.

2. **Підключіть ваш ESP пристрій до комп'ютера:**
   - Використовуйте USB кабель для підключення ESP пристрою до вашого комп'ютера.

3. **Запустіть ESPHome Flasher:**
   - Відкрийте ESPHome Flasher на вашому комп'ютері.

4. **Виберіть порт:**
   - У ESPHome Flasher виберіть порт, до якого підключений ваш ESP пристрій. Зазвичай це буде щось на кшталт `COM3` (для Windows) або `/dev/ttyUSB0` (для Linux/macOS).

5. **Виберіть `bin` файл:**
   - Натисніть кнопку "Browse" (Огляд) і виберіть `bin` файл, який ви хочете прошити.

6. **Почніть процес прошивання:**
   - Натисніть кнопку "Flash" (Прошити), щоб почати процес прошивання.

7. **Почекайте завершення процесу:**
   - Процес прошивання може зайняти кілька хвилин. Після завершення ви побачите повідомлення про успішне завершення.

## Підсумок

Тепер ваш ESP пристрій успішно прошитий з використанням ESPHome Flasher. Ви можете перевірити функціональність пристрою та переконатися, що все працює належним чином.


Цей проект використовує Wemos D1 Mini для періодичного виконання HTTP-запитів кожні 1хвилину 30 секунд

## прошивка через Arduino IDE 

- Arduino IDE
- Wemos D1 Mini
- WiFi мережа
- Канальний ключ для сервісу [SvitloBot](https://svitlobot.in.ua)
-  ключ для сервісу [healthchecks](https://healthchecks.io).

## Налаштування

1. Завантажте та встановіть [Arduino IDE](https://www.arduino.cc/en/software).
2. Додайте менеджер плат ESP8266:
   - Перейдіть у **Файл > Параметри** (File > Preferences) і додайте наступну URL-адресу в поле "Додаткові URL-адреси менеджера плат" (Additional Board Manager URLs): 
     ```
     http://arduino.esp8266.com/stable/package_esp8266com_index.json
     ```
   - Встановіть пакет плат ESP8266, перейшовши в **Інструменти > Плата > Менеджер плат** (Tools > Board > Boards Manager), знайдіть "ESP8266" і встановіть його.
3. Встановіть необхідні бібліотеки:
   - Перейдіть у **Sketch > Include Library > Manage Libraries** і встановіть наступні бібліотеки:
## Необхідні бібліотеки

- ESP8266WiFi.h
- ESP8266HTTPClient.h
- ESP8266WebServer.h
- EEPROM.h
- NTPClient.h
- WiFiUdp.h


 
 
# Проект: Wi-Fi Налаштування для ESP8266

Цей проект створений для налаштування Wi-Fi та ключа каналу на мікроконтролері ESP8266 через веб-інтерфейс.
Він зберігає налаштування у пам'яті EEPROM та автоматично підключається до Wi-Fi при наступному завантаженні.
Також є можливість очистити пам'ять EEPROM для скидання налаштувань.

## Функціонал

1. **Підключення до Wi-Fi**:
   - Якщо налаштування Wi-Fi та ключа каналу збережені в EEPROM, ESP8266 намагається підключитися до Wi-Fi.
   - Якщо підключення не вдається, створюється точка доступу для налаштування параметрів.

2. **Веб-інтерфейс для налаштування**:
   - При першому запуску або при невдалому підключенні до Wi-Fi, ESP8266 створює точку доступу і веб-сервер для введення параметрів.
   - Користувач може ввести SSID, пароль Wi-Fi та ключ каналу через веб-форму.
   - Збережені параметри зберігаються в EEPROM.

3. **Очистка пам'яті EEPROM**:
   - Через веб-інтерфейс користувач може очистити пам'ять EEPROM для скидання налаштувань.

4. **Пінгування серверу**:
   - Кожну хвилину пристрій відправляє HTTP GET запит на сервер з вказаним ключем каналу.



## Інструкція з використання

### Підключення

1. Підключіть ESP8266 до комп'ютера та завантажте код на пристрій.
2. При першому запуску ESP8266 створить точку доступу з ім'ям "svitlobot" та паролем "12345677".
3. Підключіться до цієї точки доступу зі свого пристрою.
4. Відкрийте веб-браузер та перейдіть за адресою: `http://192.168.4.1`.
5. Заповніть форму налаштування, ввівши SSID вашої Wi-Fi мережі, пароль та ключ каналу.
6. Натисніть "Save". Пристрій перезавантажиться та спробує підключитися до вказаної Wi-Fi мережі.

### Очистка пам'яті EEPROM

1. Підключіться до точки доступу "svitlobot".
2. Відкрийте веб-браузер та перейдіть за адресою: `http://192.168.4.1`.
3. Натисніть кнопку "Clear EEPROM" для очищення пам'яті.
4. Пристрій перезавантажиться та скине всі налаштування.
5. Починаючи з версії 2.6 очистку пам'яті EEPROM (скидання налаштувань) можливо зробити чотирима швидкими перезавантаженнями плати. Проміжок між вмиканням-вимиканням має бути не меншим за 3 секунди і не більше 20 секунд.
