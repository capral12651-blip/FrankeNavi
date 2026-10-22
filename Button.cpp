#include "Button.h"
#include "Config.h"
#include "Tables.h"
#include "Navi.h"
#include "Android.h"

// ============================================================
//  КНОПКИ
// ============================================================

const int buttonPins[] = {
  BTN_SPEED_MINUS,
  BTN_AUTO,
  BTN_SPEED_PLUS,
  BTN_OFF,
  BTN_DUAL,
  BTN_POWER,
  BTN_REC,
  BTN_DEFROST,
  BTN_AC,
  BTN_RR_DEF,
  BTN_MODE
};

const char* buttonNames[] = {
  "Speed-",
  "Auto",
  "Speed+",
  "Off",
  "Dual",
  "Power",
  "Rec",
  "Defrost",
  "AC",
  "RrDef",
  "Mode"
};

const int BUTTON_COUNT = 11;

int lastState[BUTTON_COUNT];
int stateDebounce[BUTTON_COUNT];
unsigned long lastDebounce[BUTTON_COUNT];

// ============================================================
//  ВНЕШНИЕ СОСТОЯНИЯ
// ============================================================

extern int currentFanSpeed;
extern int currentModeIndex;
extern bool stateAC;

// Целевые температуры
extern int encPassTemp;
extern int encDrvTemp;

// Фактические температуры из callback Navi
extern int callbackPasTemp;
extern int callbackDrvTemp;

// ============================================================
//  ЭНКОДЕРЫ
// ============================================================

volatile int encPassCounter = 0;
volatile int encDrvCounter  = 0;
volatile int encVolCounter  = 0;

int lastPassCounter = 0;
int lastDrvCounter  = 0;
int lastVolCounter  = 0;

volatile int lastEncPassA = HIGH;
volatile int lastEncDrvA  = HIGH;
volatile int lastEncVolA  = HIGH;

// ============================================================
//  ВРЕМЯ ПОСЛЕДНЕЙ АКТИВНОСТИ ТЕМПЕРАТУРНЫХ ЭНКОДЕРОВ
// ============================================================

unsigned long lastPassEncoderActivity = 0;
unsigned long lastDrvEncoderActivity  = 0;
volatile bool waitingForCallback = true;
unsigned long lastCallback = 0;
bool encoderActivity = false;
// ============================================================
//  СОСТОЯНИЯ ИНДИКАТОРОВ
// ============================================================

bool lastIndDual    = false;
bool lastIndAuto    = false;
bool lastIndRrDef   = false;
bool lastIndDefrost = false;
bool lastIndRec     = false;

bool indicatorsInitialized = false;

// ============================================================
//  ЧТЕНИЕ ИНДИКАТОРА
// ============================================================

bool readIndicator(int pin)
{
  return digitalRead(pin) == HIGH;
}

// ============================================================
//  ОПРОС СОСТОЯНИЙ ИНДИКАТОРОВ
//
//  Индикаторы являются физическим источником состояния.
//  Никаких отдельных dualState / recState / etc.
// ============================================================

void ind_State()
{
  bool dual     = readIndicator(IND_DUAL);
  bool autoMode = readIndicator(IND_AUTO);
  bool rrDef    = readIndicator(IND_RR_DEF);
  bool defrost  = readIndicator(IND_DEFROST);
  bool rec      = readIndicator(IND_REC);
  // --- Первичное чтение ---
  if (!indicatorsInitialized)
  {
    lastIndDual    = dual;
    lastIndAuto    = autoMode;
    lastIndRrDef   = rrDef;
    lastIndDefrost = defrost;
    lastIndRec     = rec;

    indicatorsInitialized = true;

    // При старте, если Dual выключен,
    // обе цели должны быть одинаковыми.
    /*if (!dual)
      encPassTemp = encDrvTemp;
      encoderActivity = false;*/
    return;
  }

  // --- Проверяем изменение любого индикатора ---
  bool changed =
    (dual     != lastIndDual)    ||
    (autoMode != lastIndAuto)    ||
    (rrDef    != lastIndRrDef)   ||
    (defrost  != lastIndDefrost) ||
    (rec      != lastIndRec);

  if (!changed)
    return;

  // --- Отладка ---
  if (dual != lastIndDual)
  {
    Serial.print("[IND] Dual -> ");
    Serial.println(dual ? "ON" : "OFF");

    // При отключении Dual пассажирская температура
    // должна стать равной водительской.
    if (!dual){
      encPassTemp = encDrvTemp;
      encoderActivity = false;}
  }

  if (autoMode != lastIndAuto)
  {
    Serial.print("[IND] Auto -> ");
    Serial.println(autoMode ? "ON" : "OFF");
  }

  if (rrDef != lastIndRrDef)
  {
    Serial.print("[IND] RrDef -> ");
    Serial.println(rrDef ? "ON" : "OFF");
  }

  if (defrost != lastIndDefrost)
  {
    Serial.print("[IND] Defrost -> ");
    Serial.println(defrost ? "ON" : "OFF");
  }

  if (rec != lastIndRec)
  {
    Serial.print("[IND] Rec -> ");
    Serial.println(rec ? "ON" : "OFF");
  }

  // Любое изменение физического состояния HVAC
  // отражаем в Android одним пакетом.
  sendHiWorld();

  // --- Запоминаем новые состояния ---
  lastIndDual    = dual;
  lastIndAuto    = autoMode;
  lastIndRrDef   = rrDef;
  lastIndDefrost = defrost;
  lastIndRec     = rec;
  
}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ КНОПОК
// ============================================================

void initButtons()
{
  for (int i = 0; i < BUTTON_COUNT; i++)
  {
    pinMode(buttonPins[i], INPUT_PULLUP);

    lastState[i] = HIGH;
    stateDebounce[i] = LOW;
    lastDebounce[i] = 0;
  }
}

// ============================================================
//  ISR ЭНКОДЕРА ПАССАЖИРА
//
//  ENC_PASS_A = A9 = PCINT17
// ============================================================

ISR(PCINT2_vect)
{
  int a = digitalRead(ENC_PASS_A);
  int b = digitalRead(ENC_PASS_B);

  if (a != lastEncPassA)
  {
    if (a == HIGH)
      encPassCounter += (b == LOW) ? 1 : -1;
    else
      encPassCounter += (b == HIGH) ? 1 : -1;

    lastEncPassA = a;
  }
}

// ============================================================
//  ISR ЭНКОДЕРА ВОДИТЕЛЯ
// ============================================================

void encoderDriverISR()
{
  int a = digitalRead(ENC_DRV_A);
  int b = digitalRead(ENC_DRV_B);

  if (a != lastEncDrvA)
  {
    if (a == HIGH)
      encDrvCounter += (b == LOW) ? 1 : -1;
    else
      encDrvCounter += (b == HIGH) ? 1 : -1;

    lastEncDrvA = a;
  }
}

// ============================================================
//  ISR ЭНКОДЕРА ГРОМКОСТИ
// ============================================================

void encoderVolumeISR()
{
  int a = digitalRead(ENC_VOL_A);
  int b = digitalRead(ENC_VOL_B);

  if (a != lastEncVolA)
  {
    if (a == HIGH)
      encVolCounter += (b == LOW) ? 1 : -1;
    else
      encVolCounter += (b == HIGH) ? 1 : -1;

    lastEncVolA = a;
  }
}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ ЭНКОДЕРОВ
// ============================================================

void initEncoders()
{
  pinMode(ENC_PASS_A, INPUT_PULLUP);
  pinMode(ENC_PASS_B, INPUT_PULLUP);

  pinMode(ENC_VOL_A, INPUT_PULLUP);
  pinMode(ENC_VOL_B, INPUT_PULLUP);

  pinMode(ENC_DRV_A, INPUT_PULLUP);
  pinMode(ENC_DRV_B, INPUT_PULLUP);

  lastEncPassA = digitalRead(ENC_PASS_A);
  lastEncVolA  = digitalRead(ENC_VOL_A);
  lastEncDrvA  = digitalRead(ENC_DRV_A);

  attachInterrupt(
    digitalPinToInterrupt(ENC_VOL_A),
    encoderVolumeISR,
    CHANGE
  );

  attachInterrupt(
    digitalPinToInterrupt(ENC_DRV_A),
    encoderDriverISR,
    CHANGE
  );

  // A9 = PCINT17
 PCICR |= (1 << PCIE2);          // Включаем PCINT1 (пины 10-15, 50-53)
  PCMSK2 |= (1 << PCINT16);        // D52
  PCMSK2 |= (1 << PCINT17);        // D50

  lastPassCounter = encPassCounter;
  lastDrvCounter  = encDrvCounter;
  lastVolCounter  = encVolCounter;
}

// ============================================================
//  ОБРАБОТКА НАЖАТИЯ КНОПКИ
// ============================================================

void processButtonPress(int index)
{
  const char* name = buttonNames[index];

  uint16_t cmd = 0x0000;
  String action = "";

  switch (index)
  {
    // --------------------------------------------------------
    // Speed-
    // --------------------------------------------------------
    case 0:
      if (currentFanSpeed > 1)
      {
        cmd = fanCommands[currentFanSpeed - 2];
        action = String("Speed ") + String(currentFanSpeed - 1);
      }
      else
      {
        Serial.println("[BTN] Speed- -> Already at minimum");
        return;
      }
      break;

    // --------------------------------------------------------
    // Auto
    // --------------------------------------------------------
    case 1:
      if (readIndicator(IND_AUTO))
      {
        cmd = 0x0100;
        action = "Auto OFF";
      }
      else
      {
        cmd = 0x0101;
        action = "Auto ON";
      }
      break;

    // --------------------------------------------------------
    // Speed+
    // --------------------------------------------------------
    case 2:
      if (currentFanSpeed < 5)
      {
        cmd = fanCommands[currentFanSpeed];
        action = String("Speed ") + String(currentFanSpeed + 1);
      }
      else
      {
        Serial.println("[BTN] Speed+ -> Already at maximum");
        return;
      }
      break;

    // --------------------------------------------------------
    // Off
    // --------------------------------------------------------
    case 3:
      cmd = 0x0100;
      action = "Full Off";
      break;

    // --------------------------------------------------------
    // Dual
    // --------------------------------------------------------
    case 4:
    {
      bool dual = readIndicator(IND_DUAL);

      cmd = dual ? 0x0A00 : 0x0A01;
      action = dual ? "Dual OFF" : "Dual ON";
      encoderActivity = dual ? false : true;

      break;
    }

    // --------------------------------------------------------
    // Power
    // --------------------------------------------------------
    case 5:
      sendKeyToAndroid(0);
      action = "Power";
      break;

    // --------------------------------------------------------
    // Rec
    // --------------------------------------------------------
    case 6:
    {
      bool rec = readIndicator(IND_REC);

      cmd = rec ? 0x0B01 : 0x0B00;
      action = rec ? "Rec OFF" : "Rec ON";

      break;
    }

    // --------------------------------------------------------
    // Defrost
    // --------------------------------------------------------
    case 7:
    {
      bool defrost = readIndicator(IND_DEFROST);

      cmd = defrost ? 0x0800 : 0x0801;
      action = defrost ? "Defrost OFF" : "Defrost ON";

      break;
    }

    // --------------------------------------------------------
    // AC
    //
    // Состояние AC берём только из callback.
    // Никаких изменений stateAC здесь.
    // --------------------------------------------------------
    case 8:
      cmd = stateAC ? 0x0200 : 0x0201;
      action = stateAC ? "AC OFF" : "AC ON";
      break;

    // --------------------------------------------------------
    // Rear Defrost
    // --------------------------------------------------------
    case 9:
    {
      bool rrDef = readIndicator(IND_RR_DEF);

      cmd = rrDef ? 0x0900 : 0x0901;
      action = rrDef ? "RrDef OFF" : "RrDef ON";

      break;
    }

    // --------------------------------------------------------
    // Mode
    // --------------------------------------------------------
    case 10:
    {
      int nextMode = (currentModeIndex + 1) % 4;

      cmd = modeCommands[nextMode];
      action = String("Mode: ") + String(modeNames[nextMode]);

      break;
    }
  }

  Serial.print("[BTN] ");
  Serial.print(name);
  Serial.print(" -> ");
  Serial.println(action);

  if (cmd != 0x0000)
    sendCommand(cmd, action.c_str());
}

// ============================================================
//  ОБРАБОТКА КНОПОК С АНТИДРЕБЕЗГОМ
// ============================================================

void processButtons()
{
  for (int i = 0; i < BUTTON_COUNT; i++)
  {
    int reading = digitalRead(buttonPins[i]);

    if (reading != stateDebounce[i]){
      lastDebounce[i] = millis();
    }

    if (millis() - lastDebounce[i] > DEBOUNCE_DELAY)
    {
      if (reading == LOW && lastState[i] == HIGH)
        processButtonPress(i);

      stateDebounce[i] = lastState[i];
      lastState[i] = reading;
    }
  }
}

// ============================================================
//  ВОДИТЕЛЬСКИЙ ЭНКОДЕР
// ============================================================

void processDriverEncoder()
{
  
  if (encDrvCounter == lastDrvCounter)
    return;

  int diff = encDrvCounter - lastDrvCounter;

  encDrvTemp += diff;

  if (encDrvTemp < 18)
    encDrvTemp = 18;

  if (encDrvTemp > 32)
    encDrvTemp = 32;

  lastDrvEncoderActivity = millis();
  
  // При выключенном Dual обе зоны должны иметь
  // одну целевую температуру.
  if (!readIndicator(IND_DUAL))
      encPassTemp = encDrvTemp;
    
  sendHiWorld();

  lastDrvCounter = encDrvCounter;
}

// ============================================================
//  ПАССАЖИРСКИЙ ЭНКОДЕР
//
//  Меняем только целевую температуру.
//  Фактический HVAC догоняется отдельно.
// ============================================================

void processPassengerEncoder()
{
  if (encPassCounter == lastPassCounter)
    return;

  int diff = encPassCounter - lastPassCounter;

  encPassTemp += diff;

  if (encPassTemp < 18)
    encPassTemp = 18;

  if (encPassTemp > 32)
    encPassTemp = 32;

  lastPassEncoderActivity = millis();
  encoderActivity = true;
  // Android должен видеть именно цель.
  sendHiWorld();
  lastPassCounter = encPassCounter;
  
}


// ============================================================
//  ДОГОН ПАССАЖИРСКОЙ ТЕМПЕРАТУРЫ
// ============================================================

void processPassengerTemperature()
{
  if (waitingForCallback){
    if (millis() - lastCallback > 2000){
      waitingForCallback = false;
    }
    return;
  }
  if (millis() - lastPassEncoderActivity < 100)
    return;
    
if (encoderActivity) {
  
  int diff = encPassTemp - callbackPasTemp;

  if (diff == 0)
    return;

  String dir = (diff > 0) ? "+1°C" : "-1°C";
  uint16_t cmd = (diff > 0) ? 0x8601 : 0x86FF;

  sendCommand(cmd, (String("Passenger ") + dir).c_str());
  waitingForCallback = true;
  lastCallback = millis();

  Serial.print("[ENC] Passenger: ");
  Serial.print (callbackPasTemp);
  Serial.print(" / ");
  Serial.println(dir);}
}

// ============================================================
//  ДОГОН ВОДИТЕЛЬСКОЙ ТЕМПЕРАТУРЫ
// ============================================================

void processDriverTemperature()
{
  if (waitingForCallback){
    if (millis() - lastCallback > 2000){
      waitingForCallback = false;
    }
    return;
  }
  if (millis() - lastDrvEncoderActivity < 100)
    return;

  bool dual = readIndicator(IND_DUAL);
  
  int diff = encDrvTemp - callbackDrvTemp;

  if (diff == 0)
    return;

  String dir = (diff > 0) ? "+1°C" : "-1°C";

  uint16_t cmd;
  String label;

  if (dual)
  {
    cmd = (diff > 0) ? 0x4601 : 0x46FF;
    label = String("Driver ") + dir;
  }
  else
  {
    cmd = (diff > 0) ? 0x1601 : 0x16FF;
    label = String("Sync ") + dir;
  }

  sendCommand(cmd, label.c_str());
  waitingForCallback = true;
  lastCallback = millis(); 
  Serial.print("[ENC] Driver: ");
  Serial.print (callbackDrvTemp);
  Serial.print(" / ");
  Serial.println (label);
  Serial.println(dir);
}

// ============================================================
//  ЭНКОДЕР ГРОМКОСТИ
// ============================================================

void processVolumeEncoder()
{
  if (encVolCounter == lastVolCounter)
    return;

  int diff = encVolCounter - lastVolCounter;

  // keyCode[9]  = Volume+
  // keyCode[10] = Volume-
  byte index = (diff > 0) ? 9 : 10;

  sendKeyToAndroid(index);

  Serial.print("[ENC] Volume: ");
  Serial.println(diff > 0 ? "RIGHT (+)" : "LEFT (-)");

  lastVolCounter = encVolCounter;
}

// ============================================================
//  ОТПРАВКА КНОПКИ В ANDROID
//
//  keyCode[index]:
//      [0] = key
//      [1] = state
//      [2] = CRC
//
//  keyCode[11] = полный RELEASE:
//      00 00 DD
// ============================================================

void sendKeyToAndroid(byte index)
{
  // --- PRESSED ---
  packet[3] = keyCode[index][0];
  packet[4] = keyCode[index][1];
  packet[5] = keyCode[index][2];

  Serial3.write(packet, 6);

  delay(50);

  // --- RELEASE ---
  packet[3] = keyCode[11][0];
  packet[4] = keyCode[11][1];
  packet[5] = keyCode[11][2];

  Serial3.write(packet, 6);
  Serial3.flush();
}

// ============================================================
//  ОБЩАЯ ОБРАБОТКА ЭНКОДЕРОВ
// ============================================================

void processEncoders()
{
  processPassengerEncoder();
  processDriverEncoder();
  processVolumeEncoder();

  processPassengerTemperature();
  processDriverTemperature();
}