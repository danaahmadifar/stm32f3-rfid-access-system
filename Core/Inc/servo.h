/*
 * servo.h - Schnittstelle Servosteuerung
 */

#ifndef SERVO_H
#define SERVO_H

#include "stm32f3xx_hal.h"
#include <stdbool.h>

void Servo_Init(TIM_HandleTypeDef *htim, uint32_t channel);
void Servo_SetAngle(uint8_t winkel);
uint8_t Servo_GetAngle(void);
void Servo_Sweep(int8_t speed, bool aktiv);

#endif /* SERVO_H */
