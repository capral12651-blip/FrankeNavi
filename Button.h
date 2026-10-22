#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ
// ============================================================

void initButtons();
void initEncoders();

// ============================================================
//  КНОПКИ
// ============================================================

void processButtons();
void processButtonPress(int index);

// ============================================================
//  ЭНКОДЕРЫ
// ============================================================

void processEncoders();
void processPassengerEncoder();
void processDriverEncoder();
void processVolumeEncoder();

// ============================================================
//  ТЕМПЕРАТУРЫ
// ============================================================

void processPassengerTemperature();
void processDriverTemperature();
extern volatile bool waitingForCallback;

// ============================================================
//  ИНДИКАТОРЫ
// ============================================================

void ind_State();
bool readIndicator(int pin);

// ============================================================
//  ANDROID
// ============================================================

void sendKeyToAndroid(byte index);

// ============================================================
//  INTERRUPTS
// ============================================================

void encoderDriverISR();
void encoderVolumeISR();

#endif