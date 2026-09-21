#include "access_control.h"
#include "main.h"
#include "MFRC522_STM32.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

// Timing-Parameter und Puffergroessen
#define KARTEN_POLL_INTERVALL  75    // Abtastintervall in ms
#define MAX_KARTEN             10    // Maximale Anzahl gespeicherter UIDs
#define ALARM_TIMEOUT_MS       5000  // Alarmdauer in ms
#define LEARN_TIMEOUT_MS       10000 // Schritt-Timeout fuer Anlernvorgang in ms

typedef struct {
    uint8_t uid[4];
    char name[16];
} GueltigeKarte_t;

// Whitelist autorisierter RFID-Karten (RAM-Speicher)
static GueltigeKarte_t gueltige_karten[MAX_KARTEN];
static uint8_t anzahl_karten = 1;

// Zustandsvariablen und Peripherie-Handle
static AccessState_t aktueller_status = STATUS_BEREIT;
static MFRC522_t *rfID;
static uint8_t gelesene_uid[4];
static uint8_t atqa_buf[2];

static uint32_t letzterPoll = 0;
static bool kartenWarten = false;

static bool alarm_an = false;
static uint32_t alarm_start_zeit = 0;
static bool zugriff_erlaubt = false;
static char aktiver_karten_name[16] = "Dana";

// UID-Bindung fuer Verriegelung (Sperren nur durch autorisierende UID zulaessig)
static uint8_t entsperrende_uid[4];
static bool hat_sperrkarte = false;
static uint32_t falsche_sperrkarte_timer = 0;

// Statistik-Zaehler
static uint32_t erfolgreiche_scans = 0;
static uint32_t fehlversuche = 0;

// Statusvariablen Anlernmodus
static LearnState_t learn_state = LEARN_INAKTIV;
static uint32_t learn_timer = 0;
static uint32_t learn_feedback_timer = 0;
static uint8_t temp_new_uid[4];

// Ansteuerung der diskreten RGB-LED-Kanaele
static void leds_setzen(bool rot, bool gruen, bool blau)
{
    HAL_GPIO_WritePin(LDROT_GPIO_Port, LDROT_Pin, rot   ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LDGR_GPIO_Port,  LDGR_Pin,  gruen ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LDBL_GPIO_Port,  LDBL_Pin,  blau  ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

// Lineare Suche nach einer UID im Speicher. Rueckgabe: Index oder -1 falls nicht vorhanden.
static int find_card_index(const uint8_t *uid)
{
    for (uint8_t i = 0; i < anzahl_karten; i++) {
        if (memcmp(uid, gueltige_karten[i].uid, 4) == 0) {
            return (int)i;
        }
    }
    return -1;
}

// Aktualisiert die LED-Signalisierung abhaengig vom aktuellen Systemzustand
static void leds_aktualisieren(void)
{
    if (learn_state != LEARN_INAKTIV) {
        // Blinkmuster waehrend des Anlernvorgangs
        uint32_t blink = (HAL_GetTick() / 250) % 2;
        if (learn_state == LEARN_WARTE_NEUE_KARTE) {
            leds_setzen(false, blink == 0, blink == 1); // Blau/Gruen alternierend
        } else if (learn_state == LEARN_WARTE_ADMIN_KARTE) {
            leds_setzen(blink == 0, false, blink == 1); // Rot/Blau alternierend
        } else if (learn_state == LEARN_ERFOLGREICH) {
            leds_setzen(false, true, false);           // Gruen statisch
        } else {
            leds_setzen(true, false, false);           // Rot statisch (Fehler/Abbruch)
        }
        return;
    }

    if (falsche_sperrkarte_timer > 0) {
        if (HAL_GetTick() - falsche_sperrkarte_timer < 2000) {
            uint32_t blink = (HAL_GetTick() / 150) % 2;
            leds_setzen(blink == 0, false, false); // Rotes Warnblinken bei unbefugtem Verriegelungsversuch
            return;
        } else {
            falsche_sperrkarte_timer = 0; // Warnzeit abgelaufen
        }
    }

    if (alarm_an) {
        leds_setzen(true, false, false);   // Rot = Alarmzustand
    } else if (zugriff_erlaubt) {
        leds_setzen(false, true, false);   // Gruen = Freigegeben
    } else {
        leds_setzen(false, false, true);   // Blau = Verriegelt / Bereit
    }
}

void AccessControl_Init(MFRC522_t *rfidHandle)
{
    rfID = rfidHandle;
    aktueller_status = STATUS_BEREIT;
    letzterPoll = HAL_GetTick();

    alarm_an = false;
    alarm_start_zeit = 0;
    zugriff_erlaubt = false;
    hat_sperrkarte = false;
    memset(entsperrende_uid, 0, sizeof(entsperrende_uid));
    falsche_sperrkarte_timer = 0;
    kartenWarten = false;

    learn_state = LEARN_INAKTIV;
    anzahl_karten = 1;

    // Standard-Admin-Karte registrieren
    uint8_t default_uid[4] = {0xE3, 0x7C, 0x7F, 0x0D};
    memcpy(gueltige_karten[0].uid, default_uid, 4);
    strncpy(gueltige_karten[0].name, "Dana", sizeof(gueltige_karten[0].name));

    strncpy(aktiver_karten_name, "Dana", sizeof(aktiver_karten_name));
    erfolgreiche_scans = 0;
    fehlversuche = 0;

    memset(gelesene_uid, 0, sizeof(gelesene_uid));
    leds_aktualisieren();
}

void AccessControl_StartLearn(void)
{
    if (learn_state != LEARN_INAKTIV) {
        return;
    }
    learn_state = LEARN_WARTE_NEUE_KARTE;
    learn_timer = HAL_GetTick();
    aktueller_status = STATUS_ANLERNEN;
    zugriff_erlaubt = false;
    hat_sperrkarte = false;
    kartenWarten = false;
    leds_aktualisieren();
}

void AccessControl_CancelLearn(void)
{
    learn_state = LEARN_INAKTIV;
    aktueller_status = STATUS_BEREIT;
    kartenWarten = false;
    leds_aktualisieren();
}

LearnState_t AccessControl_GetLearnState(void)
{
    return learn_state;
}

uint32_t AccessControl_GetLearnTimeoutRemaining(void)
{
    if (learn_state != LEARN_WARTE_NEUE_KARTE && learn_state != LEARN_WARTE_ADMIN_KARTE) {
        return 0;
    }
    uint32_t vergangen = HAL_GetTick() - learn_timer;
    if (vergangen >= LEARN_TIMEOUT_MS) return 0;
    return (LEARN_TIMEOUT_MS - vergangen) / 1000 + 1;
}

void AccessControl_ResetAlarm(void)
{
    alarm_an = false;
    aktueller_status = STATUS_BEREIT;
    leds_aktualisieren();
}

void AccessControl_Lock(void)
{
    zugriff_erlaubt = false;
    hat_sperrkarte = false;
    alarm_an = false;
    falsche_sperrkarte_timer = 0;
    aktueller_status = STATUS_BEREIT;
    leds_aktualisieren();
}

void AccessControl_UnlockViaPin(void)
{
    alarm_an = false;
    zugriff_erlaubt = true;
    hat_sperrkarte = false; // Entsperrung via PIN: universelle Sperrberechtigung
    falsche_sperrkarte_timer = 0;
    strncpy(aktiver_karten_name, "PIN-Code", sizeof(aktiver_karten_name));
    erfolgreiche_scans++;
    aktueller_status = STATUS_ERLAUBT;
    leds_aktualisieren();
}

const char* AccessControl_GetActiveCardName(void)
{
    return aktiver_karten_name;
}

uint32_t AccessControl_GetSuccessfulScans(void)
{
    return erfolgreiche_scans;
}

uint32_t AccessControl_GetFailedScans(void)
{
    return fehlversuche;
}

void AccessControl_Task(void)
{
    uint32_t jetzt = HAL_GetTick();

    leds_aktualisieren();

    // 1. Alarm-Timeout pruefen (automatische Deaktivierung nach 5 s)
    if (alarm_an && (jetzt - alarm_start_zeit >= ALARM_TIMEOUT_MS)) {
        alarm_an = false;
        aktueller_status = STATUS_BEREIT;
        leds_aktualisieren();
    }

    // 2. Anlernmodus: Anzeige des Abschluss-Status fuer 2.5 s halten
    if (learn_state == LEARN_ERFOLGREICH || learn_state == LEARN_ABGEBROCHEN || learn_state == LEARN_BEREITS_BEKANNT) {
        leds_aktualisieren();
        if (jetzt - learn_feedback_timer >= 2500) {
            learn_state = LEARN_INAKTIV;
            aktueller_status = STATUS_BEREIT;
            kartenWarten = true; // Flankensperre bis zum physischen Entfernen des Transponders
            leds_aktualisieren();
        }
        return;
    }

    // 3. Timeout-Ueberwachung der Anlernschritte
    if (learn_state == LEARN_WARTE_NEUE_KARTE || learn_state == LEARN_WARTE_ADMIN_KARTE) {
        leds_aktualisieren();
        if (jetzt - learn_timer >= LEARN_TIMEOUT_MS) {
            learn_state = LEARN_ABGEBROCHEN;
            learn_feedback_timer = jetzt;
            return;
        }
    }

    // 4. Polling-Intervall des RFID-Readers (75 ms) einhalten
    if (jetzt - letzterPoll < KARTEN_POLL_INTERVALL) {
        return;
    }
    letzterPoll = jetzt;

    // 5. Entprellung / Warten auf Verlassen des RFID-Feldes
    if (kartenWarten) {
        if (MFRC522_RequestA(rfID, atqa_buf) != STATUS_OK) {
            kartenWarten = false; // Transponder aus dem Erfassungsbereich entfernt
            if (learn_state == LEARN_INAKTIV && aktueller_status != STATUS_VERWEIGERT) {
                aktueller_status = STATUS_BEREIT;
            }
        }
        return;
    }

    // 6. Transponder-Erkennung (ISO 14443 Type A REQA)
    if (MFRC522_RequestA(rfID, atqa_buf) != STATUS_OK) {
        return;
    }

    // 7. Antikollisionssequenz und UID-Auslesen
    if (MFRC522_ReadUid(rfID, gelesene_uid) != STATUS_OK) {
        return;
    }

    // Flankenschutz aktivieren, um Mehrfachtrigger bei aufliegender Karte zu verhindern
    kartenWarten = true;

    // --- ZWEISTUFIGER ANLERNMODUS ---
    if (learn_state == LEARN_WARTE_NEUE_KARTE) {
        if (find_card_index(gelesene_uid) >= 0) {
            learn_state = LEARN_BEREITS_BEKANNT;
            learn_feedback_timer = jetzt;
        } else {
            // Neue UID temporaer speichern und Bestaetigung durch Master-Karte anfordern
            memcpy(temp_new_uid, gelesene_uid, 4);
            learn_state = LEARN_WARTE_ADMIN_KARTE;
            learn_timer = jetzt;
        }
        leds_aktualisieren();
        return;
    }

    if (learn_state == LEARN_WARTE_ADMIN_KARTE) {
        // Pruefen, ob die vorgehaltene Karte administrativ berechtigt ist
        if (find_card_index(gelesene_uid) >= 0) {
            if (anzahl_karten < MAX_KARTEN) {
                memcpy(gueltige_karten[anzahl_karten].uid, temp_new_uid, 4);
                snprintf(gueltige_karten[anzahl_karten].name, sizeof(gueltige_karten[anzahl_karten].name), "Karte %d", anzahl_karten + 1);
                anzahl_karten++;
                learn_state = LEARN_ERFOLGREICH;
            } else {
                learn_state = LEARN_ABGEBROCHEN; // Speicherkapazitaet erreicht
            }
        } else {
            learn_state = LEARN_ABGEBROCHEN; // Fehlende Autorisierung
        }
        learn_feedback_timer = jetzt;
        leds_aktualisieren();
        return;
    }

    // --- NORMALER BETRIEBSMODUS ---
    int card_idx = find_card_index(gelesene_uid);
    if (card_idx >= 0) {
        alarm_an = false;
        if (!zugriff_erlaubt) {
            // Freischaltung: UID erfassen und Eigentuemer-Bindung setzen
            zugriff_erlaubt = true;
            hat_sperrkarte = true;
            memcpy(entsperrende_uid, gelesene_uid, 4);
            strncpy(aktiver_karten_name, gueltige_karten[card_idx].name, sizeof(aktiver_karten_name));
            erfolgreiche_scans++;
            aktueller_status = STATUS_ERLAUBT;
        } else {
            // Verriegelung: Zugriff nur gestatten, wenn dieselbe Karte verwendet wird
            if (hat_sperrkarte && memcmp(gelesene_uid, entsperrende_uid, 4) != 0) {
                falsche_sperrkarte_timer = jetzt; // Unbefugter Sperrversuch
            } else {
                zugriff_erlaubt = false;
                hat_sperrkarte = false;
                aktueller_status = STATUS_BEREIT;
            }
        }
    } else {
        // Unbekannte UID: Alarm ausloesen und System sperren
        alarm_an = true;
        alarm_start_zeit = jetzt;
        zugriff_erlaubt = false;
        hat_sperrkarte = false;
        fehlversuche++;
        aktueller_status = STATUS_VERWEIGERT;
    }

    leds_aktualisieren();
}

bool AccessControl_IsUnlocked(void)
{
    return zugriff_erlaubt;
}

bool AccessControl_AlarmAktiv(void)
{
    return alarm_an;
}

AccessState_t AccessControl_GetState(void)
{
    return aktueller_status;
}

bool AccessControl_IsWrongCardBlocked(void)
{
    if (falsche_sperrkarte_timer == 0) return false;
    return (HAL_GetTick() - falsche_sperrkarte_timer < 2000);
}
