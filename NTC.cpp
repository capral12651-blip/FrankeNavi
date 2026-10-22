#include "NTC.h"
#include "Config.h"
#include <GyverNTC.h>

// ============================================================
//  NTC (температура салона)
// ============================================================
GyverNTC ntc(NTC_PIN, 10000, 3435, 25, 10000);

float filteredTemp = 0;
unsigned long lastTempPrint = 0;

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ NTC
// ============================================================
void initNTC() {
  pinMode(NTC_PIN, INPUT);
  filteredTemp = ntc.getTempAverage();
}

// ============================================================
//  ПОЛУЧЕНИЕ ТЕМПЕРАТУРЫ
// ============================================================
float getCabinTemp() {
  return filteredTemp;
}

// ============================================================
//  ПРОЦЕСС NTC
// ============================================================
void processNTC() {
  filteredTemp = 0.9 * filteredTemp + 0.1 * ntc.getTempAverage();
  int therm = filteredTemp;
  filteredTemp = therm;


  // Вывод раз в секунду (закомментирован, можно раскомментировать при отладке)
  
  /*if (millis() - lastTempPrint > 10000) {
    Serial.print("[NTC] Cabin temp: ");
    Serial.print(filteredTemp, 1);
    Serial.println(" °C");
    lastTempPrint = millis();
  }*/
  
}