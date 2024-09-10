// Перевірка лічильника завантажень
void checkBootCount() {
  int bootCount = EEPROM.read(BOOT_COUNT_ADDR);
  bootCount++;
  EEPROM.write(BOOT_COUNT_ADDR, bootCount);
  EEPROM.commit();

  Serial.print("Кількість завантажень: ");
  Serial.println(bootCount);

  if (bootCount >= 4) {
    Serial.println("Досягнуто 4 завантажень. Очищення EEPROM та перезавантаження...");
    handleClearEEPROM();
  }
}

// Обнулення лічильника завантажень після 20 секунд
void resetBootCountAfterTimeout() {
  if (millis() - bootTime > 20000) { // 20000 мс = 20 секунд
    EEPROM.write(BOOT_COUNT_ADDR, 0);
    EEPROM.commit();
  }
}