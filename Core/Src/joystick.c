/*
 * joystick.c - Erfassung und Signalverarbeitung der Joystick- und Taster-Eingaben
 */

#include "joystick.h"
#include "main.h"
#include <stdbool.h>

// Peripherie-Konfiguration und Pinbelegung
static ADC_HandleTypeDef *joystick_adc;
static uint32_t joystick_kanal_x;
static uint32_t joystick_kanal_y;
static GPIO_TypeDef *knopf_port;
static uint16_t knopf_pin;

// Gefilterte Achsenwerte und Tasterzustaende
static volatile int32_t ausschlag_x = 0;
static volatile int32_t ausschlag_y = 0;
static volatile bool knopf_wird_gehalten = false;
static volatile bool knopf_wurde_neu_gedrueckt = false;
static bool knopf_war_vorher_gedrueckt = false;

static uint32_t knopf_drueck_start = 0;
static bool knopf_held_event_sent = false;
static volatile bool knopf_geklickt = false;
static volatile bool knopf_gehalten = false;

// Zustandsautomat fuer PIN-Gestenabfolge (Links -> Rechts -> Links -> Klick)
static uint8_t pin_stufe = 0;
static uint32_t pin_letzte_eingabe = 0;

// Liest einen einzelnen ADC-Kanal per Polling aus (12-Bit Single Conversion)
static uint32_t Analogen_Wert_Lesen(uint32_t kanal)
{
    ADC_ChannelConfTypeDef adc_einstellung = {0};

    adc_einstellung.Channel = kanal;
    adc_einstellung.Rank = ADC_REGULAR_RANK_1;
    adc_einstellung.SingleDiff = ADC_SINGLE_ENDED;
    adc_einstellung.SamplingTime = ADC_SAMPLETIME_61CYCLES_5;

    HAL_ADC_ConfigChannel(joystick_adc, &adc_einstellung);
    HAL_ADC_Start(joystick_adc);
    HAL_ADC_PollForConversion(joystick_adc, 10);
    uint32_t messwert = HAL_ADC_GetValue(joystick_adc);
    HAL_ADC_Stop(joystick_adc);

    return messwert;
}

// Initialisiert die Modulvariablen mit den konfigurierten Hardware-Parametern
void Joystick_Init(ADC_HandleTypeDef *hadc, uint32_t channel_x, uint32_t channel_y, GPIO_TypeDef *btn_port, uint16_t btn_pin)
{
    joystick_adc = hadc;
    joystick_kanal_x = channel_x;
    joystick_kanal_y = channel_y;
    knopf_port = btn_port;
    knopf_pin = btn_pin;
}

// Abtastung der analogen Achsen und Auswertung der Tasterzustaende
void Joystick_Update(void)
{
    // 1. X-Achse abtasten und Totband anwenden
    uint32_t messwert_x = Analogen_Wert_Lesen(joystick_kanal_x);

    if (messwert_x > JOY_MITTE_MAX) {
        ausschlag_x = messwert_x - JOY_MITTE_MAX;
    } else if (messwert_x < JOY_MITTE_MIN) {
        ausschlag_x = messwert_x - JOY_MITTE_MIN;
    } else {
        ausschlag_x = 0;
    }

    // 2. Y-Achse abtasten und Totband anwenden
    uint32_t messwert_y = Analogen_Wert_Lesen(joystick_kanal_y);

    if (messwert_y > JOY_MITTE_MAX) {
        ausschlag_y = messwert_y - JOY_MITTE_MAX;
    } else if (messwert_y < JOY_MITTE_MIN) {
        ausschlag_y = messwert_y - JOY_MITTE_MIN;
    } else {
        ausschlag_y = 0;
    }

    // 3. Taster einlesen (Active-Low durch internen Pull-Up)
    bool knopf_ist_jetzt_gedrueckt = (HAL_GPIO_ReadPin(knopf_port, knopf_pin) == GPIO_PIN_RESET);
    knopf_wird_gehalten = knopf_ist_jetzt_gedrueckt;

    // 4. Flankenerkennung und Unterscheidung zwischen Klick und Halten (>= 800 ms)
    if (knopf_ist_jetzt_gedrueckt && !knopf_war_vorher_gedrueckt) {
        knopf_wurde_neu_gedrueckt = true;
        knopf_drueck_start = HAL_GetTick();
        knopf_held_event_sent = false;
    } else if (knopf_ist_jetzt_gedrueckt) {
        if (!knopf_held_event_sent && (HAL_GetTick() - knopf_drueck_start >= 800)) {
            knopf_gehalten = true;
            knopf_held_event_sent = true;
        }
    } else if (!knopf_ist_jetzt_gedrueckt && knopf_war_vorher_gedrueckt) {
        if (!knopf_held_event_sent) {
            knopf_geklickt = true;
        }
    }

    knopf_war_vorher_gedrueckt = knopf_ist_jetzt_gedrueckt;
}

int32_t Joystick_GetX(void)
{
    return ausschlag_x;
}

int32_t Joystick_GetY(void)
{
    return ausschlag_y;
}

bool Joystick_GetButton(void)
{
    return knopf_wird_gehalten;
}

bool Joystick_GetButtonFlanke(void)
{
    bool neu_gedrueckt = knopf_wurde_neu_gedrueckt;
    knopf_wurde_neu_gedrueckt = false;
    return neu_gedrueckt;
}

bool Joystick_GetButtonClicked(void)
{
    bool geklickt = knopf_geklickt;
    knopf_geklickt = false;
    return geklickt;
}

bool Joystick_GetButtonHeld(void)
{
    bool gehalten = knopf_gehalten;
    knopf_gehalten = false;
    return gehalten;
}

// Prueft auf Ausfuehrung der Freischalt-Geste (Sequenz: Links -> Mitte -> Rechts -> Mitte -> Links -> Mitte -> Klick)
bool Joystick_CheckSecretPin(void)
{
    uint32_t jetzt = HAL_GetTick();
    // Timeout bei Inaktivitaet nach 2.5 Sekunden
    if (pin_stufe > 0 && (jetzt - pin_letzte_eingabe > 2500)) {
        pin_stufe = 0;
    }

    if (pin_stufe == 0 && ausschlag_x < -1200) {
        pin_stufe = 1;
        pin_letzte_eingabe = jetzt;
    } else if (pin_stufe == 1 && ausschlag_x > -400 && ausschlag_x < 400) {
        pin_stufe = 2;
        pin_letzte_eingabe = jetzt;
    } else if (pin_stufe == 2 && ausschlag_x > 1200) {
        pin_stufe = 3;
        pin_letzte_eingabe = jetzt;
    } else if (pin_stufe == 3 && ausschlag_x > -400 && ausschlag_x < 400) {
        pin_stufe = 4;
        pin_letzte_eingabe = jetzt;
    } else if (pin_stufe == 4 && ausschlag_x < -1200) {
        pin_stufe = 5;
        pin_letzte_eingabe = jetzt;
    } else if (pin_stufe == 5 && ausschlag_x > -400 && ausschlag_x < 400) {
        pin_stufe = 6;
        pin_letzte_eingabe = jetzt;
    } else if (pin_stufe == 6 && Joystick_GetButtonClicked()) {
        pin_stufe = 0;
        return true; // Sequenz erfolgreich erkannt
    }

    return false;
}

void Joystick_Reset(void)
{
    ausschlag_x = 0;
    ausschlag_y = 0;
    knopf_wurde_neu_gedrueckt = false;
    knopf_wird_gehalten = false;
    knopf_geklickt = false;
    knopf_gehalten = false;
    pin_stufe = 0;
}

