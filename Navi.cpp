#include "Navi.h"
#include "Config.h"
#include "Tables.h"
#include "Android.h"
#include "Button.h"

// ============================================================
//  СОСТОЯНИЕ NAVI
// ============================================================

volatile bool frameIsFree = false;

byte climatState[CLIMAT_MSG_LEN];
byte climatLastState[CLIMAT_MSG_LEN];
byte climatPreviousState[CLIMAT_MSG_LEN];

bool startReadState = false;

// ============================================================
//  СОСТОЯНИЯ КЛИМАТА
// ============================================================

extern int currentFanSpeed;
extern int currentModeIndex;
extern bool stateAC;

// Фактические температуры, подтверждённые callback
extern int callbackPasTemp;
extern int callbackDrvTemp;

// ============================================================
//  КОМАНДА 0x66
// ============================================================

byte climatCmd[CLIMAT_CMD_LEN] = {
  0x66, 0x0A, 0x98, 0xFF, 0x40, 0x41, 0x59,
  0x26, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ============================================================
//  ВНУТРЕННИЕ ФУНКЦИИ
// ============================================================

static byte calcCRC(byte* packet);
static int getTemperatureFromHVAC(byte highByte, byte lowByte);
static bool isPacketChanged(byte* current, byte* previous, int len);
static void updateStatesFromCallback(byte* state);
static void updateTemperatureFromCallback(byte* state);
static void updateACFromCallback(byte* state);
static void updateFanFromCallback(byte* state);
static void updateDirectionFromCallback(byte* state);

// ============================================================
//  CRC ДЛЯ КОМАНДЫ 0x66
// ============================================================

static byte calcCRC(byte* packet)
{
  byte crc = 0x00;

  for (byte i = 1; i < CLIMAT_CMD_LEN - 1; i++)
    crc ^= packet[i];

  return crc;
}

// ============================================================
//  CALLBACK → ЦЕЛОЧИСЛЕННАЯ ТЕМПЕРАТУРА
//
//  Tables.cpp:
//
//  [0] HVAC byte 8
//  [1] HVAC byte 9
//  [2] HiWorld byte
//  [3] Целочисленная температура
//
//  Например:
//  0x5E 0x5E → 22°C
// ============================================================

static int getTemperatureFromHVAC(byte highByte, byte lowByte)
{
  for (byte i = 0; i < 15; i++)
  {
    if (tempHVACtoAndroid[i][0] == highByte &&
        tempHVACtoAndroid[i][1] == lowByte)
    {
      return tempHVACtoAndroid[i][3];
    }
  }

  return -1;
}

// ============================================================
//  ПРОВЕРКА ИЗМЕНЕНИЯ ПАКЕТА
// ============================================================

static bool isPacketChanged(
  byte* current,
  byte* previous,
  int len)
{
  for (int i = 0; i < len; i++)
  {
    if (current[i] != previous[i])
      return true;
  }

  return false;
}

// ============================================================
//  INTERRUPT FRAME
// ============================================================

static void frameChange()
{
  frameIsFree = digitalRead(FRAME_PIN);
}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ NAVI
// ============================================================

void initNavi()
{
  pinMode(FRAME_PIN, INPUT_PULLUP);
  pinMode(CONTROL_A, OUTPUT);

  digitalWrite(CONTROL_A, LOW);

  Serial2.begin(19200, SERIAL_8E1);

  attachInterrupt(
    digitalPinToInterrupt(FRAME_PIN),
    frameChange,
    CHANGE
  );

  memset(climatState, 0, CLIMAT_MSG_LEN);
  memset(climatLastState, 0, CLIMAT_MSG_LEN);
  memset(climatPreviousState, 0, CLIMAT_MSG_LEN);

  startReadState = false;
}

// ============================================================
//  ОТПРАВКА КОМАНДЫ 0x66
// ============================================================

void sendCommand(uint16_t command, const char* name)
{
  byte group  = command >> 8;
  byte action = command & 0xFF;

  climatCmd[10] = group;
  climatCmd[11] = action;
  climatCmd[12] = calcCRC(climatCmd);

  // Ждём свободного состояния линии
  while (!frameIsFree)
    delay(1);

  // Arduino забирает управление линией
  pinMode(FRAME_PIN, OUTPUT);

  digitalWrite(CONTROL_A, HIGH);
  digitalWrite(FRAME_PIN, LOW);

  delay(300);

  // Ждём перехода линии в состояние передачи
  while (frameIsFree)
    delay(1);

  Serial2.write(climatCmd, CLIMAT_CMD_LEN);
  Serial2.flush();

  // Возвращаем линию автомобилю
  digitalWrite(CONTROL_A, LOW);
  digitalWrite(FRAME_PIN, HIGH);

  pinMode(FRAME_PIN, INPUT_PULLUP);

  attachInterrupt(
    digitalPinToInterrupt(FRAME_PIN),
    frameChange,
    CHANGE
  );

  // name оставлен в интерфейсе для совместимости
  // с текущими вызовами Button.cpp.
  (void)name;
}

// ============================================================
//  ЧТЕНИЕ CALLBACK HVAC
//
//  Формат:
//  0x98 ... ... 0x1E ... [16 байт]
// ============================================================

bool readHVAC()
{
  static byte index = 0;

  while (Serial2.available())
  {
    byte inByte = Serial2.read();

    // --------------------------------------------------------
    // Уже читаем callback
    // --------------------------------------------------------

    if (startReadState)
    {
      climatState[index++] = inByte;

      // Проверка третьего байта callback
      if (index == 3 && inByte != 0x1E)
      {
        startReadState = false;
        index = 0;
        continue;
      }

      // Полный callback получен
      if (index == CLIMAT_MSG_LEN)
      {
        index = 0;
        startReadState = false;

        memcpy(
          climatLastState,
          climatState,
          CLIMAT_MSG_LEN
        );

        return true;
      }

      continue;
    }

    // --------------------------------------------------------
    // Поиск начала callback
    // --------------------------------------------------------

    if (inByte == 0x98)
    {
      climatState[0] = inByte;
      index = 1;
      startReadState = true;
    }
  }

  return false;
}

// ============================================================
//  ТЕМПЕРАТУРЫ CALLBACK
// ============================================================

static void updateTemperatureFromCallback(byte* state)
{
  int passengerTemp = getTemperatureFromHVAC(
    state[8],
    state[9]
  );

  int driverTemp = getTemperatureFromHVAC(
    state[10],
    state[11]
  );

  // Неизвестный код не меняет последнее корректное значение
  if (passengerTemp >= 0) {
    callbackPasTemp = passengerTemp;
    waitingForCallback = false;
  }

  if (driverTemp >= 0){
    callbackDrvTemp = driverTemp;
    waitingForCallback = false;
  }
}

// ============================================================
//  AC CALLBACK
// ============================================================

static void updateACFromCallback(byte* state)
{
  byte mode = state[13];

  stateAC =
    mode == 0x24 ||
    mode == 0x25 ||
    mode == 0xC0 ||
    mode == 0x21 ||
    mode == 0x34 ||
    mode == 0x31 ||
    mode == 0x35 ||
    mode == 0xD0 ||
    mode == 0x30;
}

// ============================================================
//  СКОРОСТЬ CALLBACK
// ============================================================

static void updateFanFromCallback(byte* state)
{
  byte speedDir = state[14];

  currentFanSpeed = speedDir & 0x07;

  if (currentFanSpeed > 5)
    currentFanSpeed = 5;
}

// ============================================================
//  НАПРАВЛЕНИЕ CALLBACK
//
//  Старшие биты byte 14:
//
//  0x20 → Center
//  0x40 → Center + Foot
//  0x60 → Foot
//  0x80 → Foot + Def
// ============================================================

static void updateDirectionFromCallback(byte* state)
{
  byte direction = state[14] & 0xF8;

  switch (direction)
  {
    case 0x20:
      currentModeIndex = 0;
      break;

    case 0x40:
      currentModeIndex = 1;
      break;

    case 0x60:
      currentModeIndex = 2;
      break;

    case 0x80:
      currentModeIndex = 3;
      break;

    case 0x00:
      currentModeIndex = 4;
      break;

    default:
      currentModeIndex = 0;
      break;
  }
}

// ============================================================
//  ОБНОВЛЕНИЕ ВСЕХ СОСТОЯНИЙ ИЗ CALLBACK
// ============================================================

static void updateStatesFromCallback(byte* state)
{
  updateTemperatureFromCallback(state);
  updateACFromCallback(state);
  updateFanFromCallback(state);
  updateDirectionFromCallback(state);
}

// ============================================================
//  ОСНОВНОЙ ПРОЦЕСС NAVI
// ============================================================

void processNavi()
{
  // Нового callback нет
  if (!readHVAC())
    return;

  // Callback пришёл, но состояние фактически не изменилось
  if (!isPacketChanged(
        climatLastState,
        climatPreviousState,
        CLIMAT_MSG_LEN))
  {
    return;
  }

  // Разбираем новый callback
  updateStatesFromCallback(climatLastState);

  // Запоминаем его как обработанный
  memcpy(
    climatPreviousState,
    climatLastState,
    CLIMAT_MSG_LEN
  );

  // Сообщаем Android о новом состоянии.
  // Сам sendHiWorld() дополнительно отфильтрует
  // пакет, если его передаваемые данные не изменились.
  sendHiWorld();
}