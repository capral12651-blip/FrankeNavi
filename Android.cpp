#include "Android.h"
#include "Config.h"
#include "Tables.h"
#include "NTC.h"
#include "Button.h"

// ============================================================
//  ВНЕШНИЕ СОСТОЯНИЯ
// ============================================================

extern int currentFanSpeed;
extern int currentModeIndex;
extern bool stateAC;

// Целевые температуры
extern int encPassTemp;
extern int encDrvTemp;

// ============================================================
//  ПОСЛЕДНИЙ ОТПРАВЛЕННЫЙ ПАКЕТ
// ============================================================

byte lastPacket[13] = {0};
bool firstSend = true;

// ============================================================
//  CRC HiWorld
// ============================================================

uint8_t calcHiWorldCRC(byte* packet, int len)
{
  int sum = 0;

  for (int i = 1; i < len - 1; i++)
    sum += packet[i];

  return (255 - (sum & 0xFF)) & 0xFF;
}

// ============================================================
//  ПРЕОБРАЗОВАНИЕ ЦЕЛЕВОЙ ТЕМПЕРАТУРЫ
//
//  Таблица:
//  [0] = HVAC byte 8
//  [1] = HVAC byte 9
//  [2] = HiWorld
//  [3] = целое значение температуры
//
//  Android получает только значение из столбца [2].
// ============================================================

uint8_t getHiWorldTemp(int temperature)
{
  for (int i = 0; i < 15; i++)
  {
    if (tempHVACtoAndroid[i][3] == temperature)
      return tempHVACtoAndroid[i][2];
  }

  // Защита от некорректного значения
  return 0x10;  // 22°C
}

// ============================================================
//  ПРЕОБРАЗОВАНИЕ СКОРОСТИ + НАПРАВЛЕНИЯ
// ============================================================

uint8_t getHiWorldSpeedDir()
{
  uint8_t speed = currentFanSpeed;

  if (speed > 5)
    speed = 5;
    if (speed == 0)
    speed = 9;

  uint8_t dirBase;

  switch (currentModeIndex)
  {
    case 0:
      dirBase = 0x10;  // Center
      break;

    case 1:
      dirBase = 0x20;  // Center + Foot
      break;

    case 2:
      dirBase = 0x30;  // Foot
      break;

    case 3:
      dirBase = 0x40;  // Foot + Windshield
      break;

    case 4:
      dirBase = 0x00;  // Auto
      break;

    default:
      dirBase = 0x00;
      break;
  }

  return dirBase | speed;
}

// ============================================================
//  ФОРМИРОВАНИЕ БАЙТА 7
//
//  Источники:
//  Defrost -> физический индикатор
//  RrDef   -> физический индикатор
//  Dual    -> физический индикатор
//  Auto    -> физический индикатор
//  AC      -> callback stateAC
// ============================================================

uint8_t getHiWorldMode()
{
  uint8_t highNibble = 0x00;
  uint8_t lowNibble  = 0x00;

  // --- Defrost + RrDef ---
  if (readIndicator(IND_DEFROST))
    highNibble |= 0x20;

  if (readIndicator(IND_RR_DEF))
    highNibble |= 0x40;

  // --- Dual + Auto + AC ---
  if (!readIndicator(IND_DUAL))
    lowNibble |= 0x04;

  if (readIndicator(IND_AUTO))
    lowNibble |= 0x01;

  if (stateAC)
    lowNibble |= 0x02;

  return highNibble | lowNibble;
}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ
// ============================================================

void initAndroid()
{
  Serial3.begin(38400);

  Serial.println("[ANDROID] Initialized on UART3 (D15)");
}

// ============================================================
//  ОТПРАВКА ПАКЕТА HiWorld 0x55
//
//  Температуры:
//      encPassTemp -> packet[3]
//      encDrvTemp  -> packet[4]
//
//  Callback-температуры здесь НЕ используются.
// ============================================================

void sendHiWorld()
{
  // --- Целевые температуры ---
  uint8_t hiworldTempL = getHiWorldTemp(encPassTemp);
  uint8_t hiworldTempR = getHiWorldTemp(encDrvTemp);

  // --- Уличная температура (NTC) ---
  int cabinTemp = getCabinTemp();
  uint8_t outsideTempByte;

  if (cabinTemp >= -40 && cabinTemp <= 87)
  {
    outsideTempByte = cabinTemp + 40;
  }
  else
  {
    outsideTempByte = 0x4F;  // 15°C
  }

  // --- Сборка пакета ---
  byte packet[13];

  packet[0] = 0x2E;
  packet[1] = 0x55;
  packet[2] = 0x09;

  packet[3] = hiworldTempL;              // Цель пассажира
  packet[4] = hiworldTempR;              // Цель водителя
  packet[5] = getHiWorldSpeedDir();      // Скорость + направление
  packet[6] = 0x00;                      // Резерв
  packet[7] = getHiWorldMode();          // Режим
  packet[8] = readIndicator(IND_REC) ? 0x01 : 0x02;
  packet[9] = 0x00;                      // Резерв
  packet[10] = outsideTempByte;          // Температура салона
  packet[11] = 0xAA;                     // Постоянно
  packet[12] = calcHiWorldCRC(packet, 13);

  // --- Фильтр изменений ---
  if (!firstSend)
  {
    bool changed = false;

    for (int i = 0; i < 13; i++)
    {
      if (packet[i] != lastPacket[i])
      {
        changed = true;
        break;
      }
    }

    if (!changed)
      return;
  }

  // --- Отправка ---
  Serial3.write(packet, 13);
  Serial3.flush();

  memcpy(lastPacket, packet, 13);
  firstSend = false;

  // --- Отладка ---
  Serial.print("[ANDROID] HiWorld: ");

  for (int i = 0; i < 13; i++)
  {
    if (packet[i] < 0x10)
      Serial.print("0");

    Serial.print(packet[i], HEX);
    Serial.print(" ");
  }

  Serial.print(" | Temp: ");
  Serial.print(encPassTemp);
  Serial.print("/");
  Serial.print(encDrvTemp);

  Serial.print(" | Mode: 0x");
  Serial.print(packet[7], HEX);

  Serial.print(" SpeedDir: 0x");
  Serial.print(packet[5], HEX);

  Serial.print(" Recirc: ");
  Serial.print(packet[8] == 0x01 ? "ON" : "OFF");

  Serial.print(" Outside: ");
  Serial.print(cabinTemp);
  Serial.println("°C");
}