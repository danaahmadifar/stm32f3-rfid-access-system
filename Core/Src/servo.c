/*
 * servo.c - PWM-basierte Servosteuerung (TIM2 CH1)
 */
#include "servo.h"
#include "main.h"
#include <stdbool.h>

static TIM_HandleTypeDef *servo_timer;
static uint32_t servo_kanal;
static int16_t aktueller_winkel = 90;
static int8_t  richtung = 1;       // 1 = Vorwaerts (+), -1 = Rueckwaerts (-)
static uint32_t letzte_zeit = 0;

void Servo_Init(TIM_HandleTypeDef *htim, uint32_t channel)
{
    servo_timer = htim;
    servo_kanal = channel;

    HAL_TIM_PWM_Start(servo_timer, servo_kanal);
    Servo_SetAngle(90); // Grundstellung
}

void Servo_SetAngle(uint8_t winkel)
{
    // Begrenzung auf 0..180 Grad
    if (winkel > 180) {
        winkel = 180;
    }

    aktueller_winkel = winkel;

    // Timer-Konfiguration: Periode 1000 bei 50 Hz
    // Pulsweiten-Mapping: 0 Grad = 25 (0.5 ms), 180 Grad = 125 (2.5 ms)
    uint32_t puls = 25 + ((uint32_t)winkel * 100) / 180;
    __HAL_TIM_SET_COMPARE(servo_timer, servo_kanal, puls);
}

uint8_t Servo_GetAngle(void)
{
    if (aktueller_winkel < 0) return 0;
    if (aktueller_winkel > 180) return 180;
    return (uint8_t)aktueller_winkel;
}

void Servo_Sweep(int8_t speed, bool aktiv)
{
    if (!aktiv || speed <= 0) {
        letzte_zeit = 0;
        return;
    }

    uint32_t jetzt = HAL_GetTick();

    // Nach Aktivierung oder laengerer Pause Zeitbasis synchronisieren
    if (letzte_zeit == 0 || (jetzt - letzte_zeit) > 200) {
        letzte_zeit = jetzt;
        return;
    }

    // Schrittweite und Aktualisierungsintervall nach Speed-Stufe (1..4)
    uint32_t intervall = 10;
    int16_t schritt = 1;

    if (speed == 1) {
        intervall = 15;
        schritt = 1;
    } else if (speed == 2) {
        intervall = 10;
        schritt = 1;
    } else if (speed == 3) {
        intervall = 10;
        schritt = 2;
    } else { // speed >= 4
        intervall = 10;
        schritt = 4;
    }

    if ((jetzt - letzte_zeit) < intervall) {
        return;
    }
    letzte_zeit = jetzt;

    // Einzelschritt ausfuehren
    aktueller_winkel += (richtung * schritt);

    // Endanschlaege pruefen und Richtung umkehren
    if (aktueller_winkel >= 180) {
        aktueller_winkel = 180;
        richtung = -1;
    } else if (aktueller_winkel <= 0) {
        aktueller_winkel = 0;
        richtung = 1;
    }

    Servo_SetAngle((uint8_t)aktueller_winkel);
}

