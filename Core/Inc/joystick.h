/*
 * joystick.h - Analog-Joystick und Taster-Erfassung
 */

#ifndef JOYSTICK_H_
#define JOYSTICK_H_

#include "main.h"
#include <stdbool.h>

// Totzone um die Ruhelage (12-Bit ADC: 0..4095)
#define JOY_MITTE_MIN 1700
#define JOY_MITTE_MAX 2400

void Joystick_Init(ADC_HandleTypeDef *hadc, uint32_t channel_x, uint32_t channel_y, GPIO_TypeDef *btn_port, uint16_t btn_pin);
void Joystick_Update(void);
void Joystick_Reset(void);

int32_t Joystick_GetX(void);
int32_t Joystick_GetY(void);
bool Joystick_GetButton(void);
bool Joystick_GetButtonFlanke(void);
bool Joystick_GetButtonClicked(void);
bool Joystick_GetButtonHeld(void);
bool Joystick_CheckSecretPin(void);

#endif /* JOYSTICK_H_ */
