#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#include "MFRC522_STM32.h"
#include <stdbool.h>
#include <stdint.h>

// Zustaende der Zugriffskontrolle
typedef enum {
    STATUS_BEREIT = 0,
    STATUS_ERLAUBT,
    STATUS_VERWEIGERT,
    STATUS_ANLERNEN
} AccessState_t;

// Zustaende des zweistufigen Anlernprozesses
typedef enum {
    LEARN_INAKTIV = 0,
    LEARN_WARTE_NEUE_KARTE,
    LEARN_WARTE_ADMIN_KARTE,
    LEARN_ERFOLGREICH,
    LEARN_ABGEBROCHEN,
    LEARN_BEREITS_BEKANNT
} LearnState_t;

// Initialisierung und Haupt-Task
void AccessControl_Init(MFRC522_t *rfidHandle);
void AccessControl_Task(void);
bool AccessControl_AlarmAktiv(void);
bool AccessControl_IsUnlocked(void);
AccessState_t AccessControl_GetState(void);

// Steuerungsfunktionen und Diagnose
void AccessControl_ResetAlarm(void);
void AccessControl_Lock(void);
void AccessControl_UnlockViaPin(void);
const char* AccessControl_GetActiveCardName(void);
bool AccessControl_IsWrongCardBlocked(void);

// Zweistufiger Anlernmodus
void AccessControl_StartLearn(void);
void AccessControl_CancelLearn(void);
LearnState_t AccessControl_GetLearnState(void);
uint32_t AccessControl_GetLearnTimeoutRemaining(void);

// Statistik-Zaehler
uint32_t AccessControl_GetSuccessfulScans(void);
uint32_t AccessControl_GetFailedScans(void);

#endif /* ACCESS_CONTROL_H */
