#include "Tables.h"

// ============================================================
//  ТАБЛИЦА КОМАНД РЕЖИМОВ ДЛЯ 0X66
// ============================================================
const uint16_t modeCommands[] = {
  0x0301,  // Center
  0x0302,  // Center+Foot
  0x0303,  // Foot
  0x0304   // Foot+Def
};

const char* modeNames[] = {
  "Center", "Center+Foot", "Foot", "Foot+Def"
};

// ============================================================
//  ТАБЛИЦА КОМАНД СКОРОСТЕЙ ДЛЯ 0X66
// ============================================================
const uint16_t fanCommands[] = {
  0x0501,  // Speed 1
  0x0502,  // Speed 2
  0x0503,  // Speed 3
  0x0504,  // Speed 4
  0x0505   // Speed 5
};

// === ТАБЛИЦЫ СООТВЕТСТВИЙ ===

// Температура: NAVI (2 байта 0x98: 8 и 9 - слева, 10 и 11 - справа) → Android (0x2E: 3 - слева, 4 - справа)
const byte tempHVACtoAndroid[][4] = {
  {0x49, 0x78, 0x10, 18}, // Low
  {0x24, 0x77, 0x26, 19}, // 19°C
  {0x5E, 0x6F, 0x28, 20}, // 20°C
  {0x5E, 0x24, 0x2A, 21}, // 21°C
  {0x5E, 0x5E, 0x2C, 22}, // 22°C
  {0x5E, 0x76, 0x2E, 23}, // 23°C
  {0x5E, 0x35, 0x30, 24}, // 24°C
  {0x5E, 0x73, 0x32, 25}, // 25°C
  {0x5E, 0x7B, 0x34, 26}, // 26°C
  {0x5E, 0x26, 0x36, 27}, // 27°C
  {0x5E, 0x7F, 0x38, 28}, // 28°C
  {0x5E, 0x77, 0x3A, 29}, // 29°C
  {0x76, 0x6F, 0x3C, 30}, // 30°C
  {0x76, 0x24, 0x3E, 31}, // 31°C
  {0x3D, 0x08, 0x50, 32}  // High
};

//Коды кнопок HiWorld
//2E 20 02 [key code]
const byte keyCode[][3] = {
  {0x01, 0x01, 0xDB},  //Power
  {0x02, 0x01, 0xDA},  //Menu
  {0x03, 0x01, 0xD9},  //Back
  {0x04, 0x01, 0xD8},  //Voice
  {0x05, 0x01, 0xD7},  //Bluetooth
  {0x06, 0x01, 0xD6},  //FM
  {0x07, 0x01, 0xD5},  //Mode
  {0x08, 0x01, 0xD4},  //Navigation
  {0x09, 0x01, 0xD3},  //Display
  {0x0A, 0x01, 0xD2},  //Volume +
  {0x0B, 0x01, 0xD1},  //Volume -
  
  {0x00, 0x00, 0xDD}   //Release - обязательная команда отпускания кнопки после каждой команды нажатия
};

byte packet[6] = {
  0x2E,       // Start
  0x20,       // DataType — кнопки
  0x02,       // Data length
  0x00,       // Key
  0x00,       // State
  0x00        // CRC
};