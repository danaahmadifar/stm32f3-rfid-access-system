# STM32 RFID-Zugangskontrolle mit Servomotor & OLED

Ein Projekt für das STM32 Nucleo-F303RE Board. Das System liest RFID-Karten über ein RC522-Modul ein, zeigt den Status auf einem 0,96" OLED-Display an und schaltet bei Berechtigung einen Servomotor frei. Dieser kann anschließend manuell per Joystick gesteuert oder in einen automatischen Schwenkmodus (Sweep) versetzt werden.

<p align="center">
  <a href="https://www.youtube.com/watch?v=aio0vg1aWxY">
    <img src="https://img.youtube.com/vi/aio0vg1aWxY/hqdefault.jpg" alt="Video ansehen" width="600">
  </a>
  <br>
  <em>▶ Video-Demo des Systems auf YouTube ansehen</em>
</p>

---

## Verwendete Hardware

* **Mikrocontroller**: STM32 Nucleo-F303RE (ARM Cortex-M4)
* **RFID-Reader**: MFRC522 (13.56 MHz, Anbindung per SPI)
* **Display**: 0,96" OLED-Display mit SSD1306-Controller (128x64 Pixel, I2C)
* **Servomotor**: SG90 9g Micro-Servo (Ansteuerung per PWM)
* **Analog-Joystick**: 2-Achsen-Joystick mit Taster (z. B. KY-023)
* **Status-LEDs**: 3 LEDs (Rot, Grün, Blau) mit Vorwiderständen (oder eine RGB-LED mit gemeinsamer Kathode)
* **Taster**: Blauer Onboard-Button (B1 / PC13) auf dem Nucleo-Board

---

## Pinbelegung / Verkabelung

<img width="827" height="683" alt="image" src="https://github.com/user-attachments/assets/7ae6e894-4127-4bb8-8544-820e4ae8d878" />

<img width="1217" height="996" alt="image" src="https://github.com/user-attachments/assets/9fbcb6a2-513a-42b2-b368-90ae789497e2" />


### 1. RFID-Modul (MFRC522 an SPI2)
| RC522 Pin | STM32 Pin | Beschreibung |
| :--- | :--- | :--- |
| **VCC** | 3.3V | Stromversorgung (Achtung: nicht an 5V anschließen) |
| **RST** | PB11 | Reset-Leitung |
| **GND** | GND | Masse |
| **MISO** | PB14 | SPI2 MISO |
| **MOSI** | PB15 | SPI2 MOSI |
| **SCK** | PB13 | SPI2 Takt |
| **SDA (CS)** | PB12 | SPI2 Chip Select |

### 2. OLED-Display (SSD1306 an I2C1)
| OLED Pin | STM32 Pin | Beschreibung |
| :--- | :--- | :--- |
| **VCC** | 3.3V | Stromversorgung |
| **GND** | GND | Masse |
| **SCL** | PA15 | I2C1 Clock |
| **SDA** | PB7 | I2C1 Daten |

### 3. Servomotor (SG90 an TIM2)
| Servo Kabel | STM32 Pin | Beschreibung |
| :--- | :--- | :--- |
| **Signal (Orange/Gelb)** | PA0 | PWM-Signal (TIM2 Kanal 1, 50 Hz) |
| **VCC (Rot)** | 5V | Stromversorgung |
| **GND (Braun/Schwarz)**| GND | Masse |

### 4. Joystick (an ADC1 & GPIO)
| Joystick Pin | STM32 Pin | Beschreibung |
| :--- | :--- | :--- |
| **VCC** | 3.3V | Stromversorgung |
| **GND** | GND | Masse |
| **VRx (X-Achse)** | PA1 | ADC1 Kanal 2 (Links / Rechts) |
| **VRy (Y-Achse)** | PA2 | ADC1 Kanal 3 (Oben / Unten) |
| **SW (Taster)** | PC0 | Taster-Eingang (mit internem Pull-Up) |

### 5. Status-LEDs
| LED-Farbe | STM32 Pin | Zustand |
| :--- | :--- | :--- |
| **Rot** | PA4 | Alarm bei falscher Karte / Dauer-Rot bei Sicherheits-Lockout / Warnblinken |
| **Blau** | PA5 | Gesperrt / Bereit (1 s Blinkanimation nach Sperren, danach Dauer-Blau) |
| **Grün** | PA6 | Entsperrt / Freigabe (1 s Blinkanimation nach Freischalten, danach Dauer-Grün) |

---

## Funktionen und Bedienung

### 1. Freischalten und Sperren
* Im Grundzustand ist das System verriegelt. Die blaue LED leuchtet und das OLED zeigt `GESPERRT`. Der Servo steht fest auf 90°.
* **Freischalten**: Wird eine berechtigte Karte vorgehalten (oder der Geheimcode eingegeben), blinkt die grüne LED für 1 Sekunde zur optischen Bestätigung und geht danach auf Dauer-Grün. Das Display begrüßt den Nutzer (z. B. `Hallo Dana!`) und die Servosteuerung wird aktiv.
* **Sperren**: Wird das System gesperrt (per Karte oder Auto-Lock), blinkt die blaue LED für 1 Sekunde und leuchtet anschließend dauerhaft blau.
* **Kartenbindung beim Sperren**: Nur die Karte, die das System freigeschaltet hat, kann es auch wieder sperren. Hält ein anderer Nutzer seine Karte vor, bleibt das System offen: Das Display warnt (`Nur Freischalter kann sperren!`) und die rote LED blinkt kurz auf.

### 2. Automatischer Lock, Alarm & Sicherheits-Lockout
* **Auto-Lock**: Bleibt das System freigeschaltet und der Joystick wird 15 Sekunden lang nicht berührt, verriegelt sich das System automatisch. Der Servo fährt dabei auf die 90°-Mittelposition zurück.
* **Alarm**: Hält jemand eine unbekannte Karte vor, wird der Alarm ausgelöst (rote LED an, Display zeigt `ALARM! Fehlversuch X/5`). Nach 5 Sekunden schaltet sich der Alarm selbst ab. Über den blauen Nucleo-Button (B1) kann der Alarm vorzeitig quittiert werden.
* **Sicherheits-Sperre (Lockout nach 5 Fehlversuchen)**: Nach 5 aufeinanderfolgenden Fehlversuchen im verriegelten Zustand schaltet das System in den Sicherheits-Lockout (`GESPERRT! 5 Fehlversuche / Admin-Karte noetig`). In diesem Zustand leuchtet die rote LED dauerhaft. PIN-Eingabe, Anlernmodus und normale Nutzerkarten sind blockiert. Das System kann ausschließlich durch die hinterlegte Admin-Karte wieder freigeschaltet werden.

### 3. Zweistufiger Anlernmodus für neue Karten
1. Den blauen Nucleo-Button (**B1**) drücken.
2. **Schritt 1**: Neue Karte vorhalten (LED blinkt abwechselnd blau/grün, 10 Sekunden Zeit).
3. **Schritt 2**: Mit einer bereits freigeschalteten Admin-Karte bestätigen (LED blinkt rot/blau).
4. Nach erfolgreicher Bestätigung wird die neue Karte im RAM gespeichert (bis zu 10 Karten). Ein erneuter Druck auf B1 bricht den Modus jederzeit ab.

### 4. Servo-Bedienung & Sweep
* Sobald entsperrt ist, steuert die X-Achse des Joysticks den Winkel (0° bis 180°). Ein Balken auf dem OLED zeigt die Position an.
* **Geschwindigkeit ändern**: Ein kurzer Klick auf den Joystick-Knopf schaltet durch 4 Geschwindigkeitsstufen (`Spd: 1` bis `Spd: 4`).
* **Sweep-Modus**: Wird der Joystick-Knopf länger als 800 ms gehalten, startet der automatische Schwenkmodus (der Servo fährt gleichmäßig von 0° nach 180° und zurück). Bewegt man den Joystick manuell, wird der Sweep sofort beendet.

### 5. Notfall-PIN (Entsperren ohne Karte)
Falls gerade keine Karte zur Hand ist, kann das System per Joystick-Geste freigeschaltet werden:
* **Kombination**: Joystick nach **Links -> Rechts -> Links -> Klick** auf den Taster.
* Das System entsperrt sich mit der Anzeige `Hallo PIN-Code!`.

---

## Projektstruktur

* `Core/Src/main.c`: Initialisierung der Peripherie (Clocks, SPI, I2C, Timer, ADC), Hauptschleife und Timer-Interrupts.
* `Core/Src/access_control.c` & `access_control.h`: Zustandsautomat für die Zutrittskontrolle, Karten-Whitelist, Anlernmodus und LED-Steuerung.
* `Core/Src/joystick.c` & `joystick.h`: Auslesen der ADC-Werte, Totband-Filterung, Klick- und Halte-Erkennung sowie PIN-Gesten-Erkennung.
* `Core/Src/servo.c` & `servo.h`: PWM-Berechnung für den Servowinkel und Sweep-Logik.
* `Core/Src/MFRC522_STM32.c` & `MFRC522_STM32.h`: Treiber für das RFID-Modul über SPI.
* `Core/Src/ssd1306.c` & `ssd1306_fonts.c`: I2C-Grafiktreiber für das OLED-Display.

---

## Kompilieren und Flashen

### Mit STM32CubeIDE
1. Das Projekt in der STM32CubeIDE über `File -> Open Projects from File System...` importieren.
2. Das Board per USB-Kabel verbinden.
3. Auf **Run** oder **Debug** klicken – die IDE kompiliert das Projekt und überträgt das Programm automatisch auf den Mikrocontroller.

## BEKANNTE BUGS

1. Momentan bleibt die LED teilweise hängen bei einer Farbe. Ursache noch unklar.
2. Die LED flackert bei hohen Geschwindigkeitseinstellungen des Servos. Ursache ist wahrscheinlich hoher Stromverbrauch des Servos.
3. Notfall-PIN wird teilweise nicht erkannt. Ursache ist wahrscheinlich die Hardware selbst. (Alter Joystick)
