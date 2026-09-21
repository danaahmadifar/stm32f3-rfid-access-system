/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "MFRC522_STM32.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "ssd1306_conf.h"
#include "access_control.h"
#include <stdbool.h>
#include "servo.h"
#include "joystick.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim6;

/* USER CODE BEGIN PV */
volatile float aktuelle_position = 90.0f;
volatile int8_t speed_faktor = 1;
volatile bool sweep_modus = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM6_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */
static void Display_Aktualisieren(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
MFRC522_t rfID = {&hspi2, CS_GPIO_Port, CS_Pin, RESET_GPIO_Port, RESET_Pin};

// Aktualisierung der OLED-Anzeige (SSD1306, 128x64) ueber I2C
static void Display_Aktualisieren(void)
{
    static uint32_t letztes_update = 0;
    static int8_t vorheriges_entsperrt = -1;
    static int8_t vorheriger_alarm = -1;
    static bool vorheriger_admin_lockout = false;
    static uint8_t vorherige_fehlversuche_in_folge = 0;
    static int8_t vorheriger_speed = -1;
    static int vorheriger_winkel = -1;
    static LearnState_t vorheriger_learn_state = LEARN_INAKTIV;
    static uint32_t vorherige_restzeit = 0;
    static bool vorheriger_sweep = false;
    static bool vorheriger_falsch_gesperrt = false;

    // Rate-Limiting fuer I2C-Uebertragungen zur Vermeidung von Bus-Blockaden
    uint32_t min_intervall = sweep_modus ? 150 : 50;
    if (HAL_GetTick() - letztes_update < min_intervall) {
        return;
    }

    bool entsperrt = AccessControl_IsUnlocked();
    bool alarm = AccessControl_AlarmAktiv();
    bool admin_lockout = AccessControl_IsAdminLockout();
    uint8_t fehlversuche_in_folge = AccessControl_GetConsecutiveFailures();
    int winkel_int = (int)aktuelle_position;
    LearnState_t learn_state = AccessControl_GetLearnState();
    uint32_t restzeit = AccessControl_GetLearnTimeoutRemaining();
    bool falsch_gesperrt = AccessControl_IsWrongCardBlocked();

    // Redraw nur bei Zustandsaenderungen durchfuehren
    if (entsperrt == vorheriges_entsperrt &&
        alarm == vorheriger_alarm &&
        admin_lockout == vorheriger_admin_lockout &&
        fehlversuche_in_folge == vorherige_fehlversuche_in_folge &&
        speed_faktor == vorheriger_speed &&
        winkel_int == vorheriger_winkel &&
        learn_state == vorheriger_learn_state &&
        restzeit == vorherige_restzeit &&
        sweep_modus == vorheriger_sweep &&
        falsch_gesperrt == vorheriger_falsch_gesperrt) {
        return;
    }

    letztes_update = HAL_GetTick();
    vorheriges_entsperrt = entsperrt;
    vorheriger_alarm = alarm;
    vorheriger_admin_lockout = admin_lockout;
    vorherige_fehlversuche_in_folge = fehlversuche_in_folge;
    vorheriger_speed = speed_faktor;
    vorheriger_winkel = winkel_int;
    vorheriger_learn_state = learn_state;
    vorherige_restzeit = restzeit;
    vorheriger_sweep = sweep_modus;
    vorheriger_falsch_gesperrt = falsch_gesperrt;

    ssd1306_Fill(Black);

    char buffer[32];

    if (admin_lockout) {
        // Anzeige: Sicherheits-Lockout nach 5 Fehlversuchen
        ssd1306_SetCursor(15, 6);
        ssd1306_WriteString("GESPERRT!", Font_11x18, White);
        ssd1306_SetCursor(15, 26);
        ssd1306_WriteString("5 Fehlversuche", Font_7x10, White);
        ssd1306_SetCursor(1, 39);
        ssd1306_WriteString("Admin-Karte noetig", Font_7x10, White);
        ssd1306_SetCursor(15, 51);
        ssd1306_WriteString("zum Entsperren", Font_7x10, White);
    } else if (learn_state != LEARN_INAKTIV) {
        // Anzeige: Zweistufiger Anlernmodus
        if (learn_state == LEARN_WARTE_NEUE_KARTE) {
            ssd1306_SetCursor(5, 4);
            ssd1306_WriteString("1. NEUE KARTE", Font_7x10, White);
            ssd1306_SetCursor(5, 20);
            ssd1306_WriteString("Karte anhalten", Font_7x10, White);
            sprintf(buffer, "Timeout: %lu s", restzeit);
            ssd1306_SetCursor(5, 36);
            ssd1306_WriteString(buffer, Font_7x10, White);
            ssd1306_SetCursor(5, 52);
            ssd1306_WriteString("B1: Abbruch", Font_7x10, White);
        } else if (learn_state == LEARN_WARTE_ADMIN_KARTE) {
            ssd1306_SetCursor(5, 4);
            ssd1306_WriteString("2. BESTAETIGEN", Font_7x10, White);
            ssd1306_SetCursor(5, 20);
            ssd1306_WriteString("Admin vorhalten", Font_7x10, White);
            sprintf(buffer, "Timeout: %lu s", restzeit);
            ssd1306_SetCursor(5, 36);
            ssd1306_WriteString(buffer, Font_7x10, White);
            ssd1306_SetCursor(5, 52);
            ssd1306_WriteString("B1: Abbruch", Font_7x10, White);
        } else if (learn_state == LEARN_ERFOLGREICH) {
            ssd1306_SetCursor(25, 12);
            ssd1306_WriteString("ERFOLG!", Font_11x18, White);
            ssd1306_SetCursor(5, 36);
            ssd1306_WriteString("Karte gespeichert", Font_7x10, White);
        } else if (learn_state == LEARN_BEREITS_BEKANNT) {
            ssd1306_SetCursor(25, 12);
            ssd1306_WriteString("HINWEIS", Font_11x18, White);
            ssd1306_SetCursor(8, 36);
            ssd1306_WriteString("Bereits bekannt!", Font_7x10, White);
        } else {
            ssd1306_SetCursor(25, 12);
            ssd1306_WriteString("ABBRUCH", Font_11x18, White);
            ssd1306_SetCursor(8, 36);
            ssd1306_WriteString("Nicht bestaetigt", Font_7x10, White);
        }
    } else if (alarm) {
        // Anzeige: Alarmzustand mit Fehlversuchs-Zaehler
        ssd1306_SetCursor(30, 6);
        ssd1306_WriteString("ALARM!", Font_11x18, White);
        if (fehlversuche_in_folge > 0) {
            snprintf(buffer, sizeof(buffer), "Fehlversuch %u/5", fehlversuche_in_folge);
            ssd1306_SetCursor(11, 27);
            ssd1306_WriteString(buffer, Font_7x10, White);
        } else {
            ssd1306_SetCursor(18, 27);
            ssd1306_WriteString("Kein Zutritt!", Font_7x10, White);
        }
        ssd1306_SetCursor(18, 40);
        ssd1306_WriteString("Kein Zutritt!", Font_7x10, White);
        ssd1306_SetCursor(15, 52);
        ssd1306_WriteString("B1: Quittieren", Font_7x10, White);
    } else if (entsperrt) {
        if (falsch_gesperrt) {
            // Warnanzeige: Verriegelung durch abweichende UID abgewiesen
            ssd1306_SetCursor(20, 4);
            ssd1306_WriteString("ACHTUNG!", Font_11x18, White);
            ssd1306_SetCursor(15, 25);
            ssd1306_WriteString("Falsche Karte!", Font_7x10, White);
            ssd1306_SetCursor(8, 38);
            ssd1306_WriteString("Nur Freischalter", Font_7x10, White);
            ssd1306_SetCursor(18, 50);
            ssd1306_WriteString("kann sperren!", Font_7x10, White);
        } else {
            // Anzeige: System freigegeben
            snprintf(buffer, sizeof(buffer), "Hallo %s!", AccessControl_GetActiveCardName());
            ssd1306_SetCursor(5, 4);
            ssd1306_WriteString(buffer, Font_7x10, White);

            if (sweep_modus) {
                sprintf(buffer, "SWEEP >>> Spd:%d", speed_faktor);
            } else {
                sprintf(buffer, "W:%3d*   Spd:%d", winkel_int, speed_faktor);
            }
            ssd1306_SetCursor(5, 18);
            ssd1306_WriteString(buffer, Font_7x10, White);

            // Grafische Balkenanzeige fuer den Servowinkel
            ssd1306_DrawRectangle(5, 34, 122, 46, White);
            uint8_t bar_width = (uint8_t)(((uint32_t)winkel_int * 114) / 180);
            if (bar_width > 0) {
                if (bar_width > 114) bar_width = 114;
                ssd1306_FillRectangle(7, 36, 7 + bar_width, 44, White);
            }

            ssd1306_SetCursor(5, 50);
            ssd1306_WriteString("0*           180*", Font_7x10, White);
        }
    } else {
        // Anzeige: System verriegelt
        ssd1306_SetCursor(20, 6);
        ssd1306_WriteString("GESPERRT", Font_11x18, White);
        ssd1306_SetCursor(15, 28);
        ssd1306_WriteString("Karte oder PIN", Font_7x10, White);
        sprintf(buffer, "OK:%lu  Fehl:%lu", AccessControl_GetSuccessfulScans(), AccessControl_GetFailedScans());
        ssd1306_SetCursor(5, 40);
        ssd1306_WriteString(buffer, Font_7x10, White);
        ssd1306_SetCursor(18, 52);
        ssd1306_WriteString("B1: Anlernen", Font_7x10, White);
    }

    ssd1306_UpdateScreen();
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */


  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI2_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM6_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  ssd1306_Init();
  ssd1306_Fill(Black);
  ssd1306_SetCursor(10, 20);
  ssd1306_WriteString("Starte...", Font_11x18, White);
  ssd1306_UpdateScreen();

  // Peripheriemodule und Treiber initialisieren
  MFRC522_Init(&rfID);
  AccessControl_Init(&rfID);
  Servo_Init(&htim2, TIM_CHANNEL_1);
  Servo_SetAngle(90);
  Joystick_Init(&hadc1, ADC_CHANNEL_2, ADC_CHANNEL_3, SW_GPIO_Port, SW_Pin);

  // Basis-Timer TIM6 fuer zyklische Regler- und Steuerungs-Tasks starten
  HAL_TIM_Base_Start_IT(&htim6);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t letzte_aktivitaet = HAL_GetTick();
  bool b1_war_gedrueckt = false;
  bool system_war_entsperrt = false;

  while (1)
  {
      // 1. RFID-Task abarbeiten (Polling, Transponder-Erkennung, Timeout-Management)
      AccessControl_Task();

      // Flankenerkennung der Freigabe: Aktivitaetszeitstempel synchronisieren
      bool system_ist_entsperrt = AccessControl_IsUnlocked();
      if (system_ist_entsperrt && !system_war_entsperrt) {
          letzte_aktivitaet = HAL_GetTick();
      }
      system_war_entsperrt = system_ist_entsperrt;

      // 2. Taster B1 (PC13) abfragen (Entprellung per Flankenerkennung)
      bool b1_ist_gedrueckt = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET);
      if (b1_ist_gedrueckt && !b1_war_gedrueckt) {
          if (AccessControl_IsAdminLockout()) {
              // Im Sicherheits-Lockout sind Tasteraktionen gesperrt
          } else if (AccessControl_AlarmAktiv()) {
              AccessControl_ResetAlarm(); // Alarm quittieren
          } else if (AccessControl_GetState() == STATUS_ANLERNEN) {
              AccessControl_CancelLearn(); // Anlernmodus manuell abbrechen
          } else {
              AccessControl_StartLearn();  // Zweistufigen Anlernprozess initialisieren
          }
      }
      b1_war_gedrueckt = b1_ist_gedrueckt;

      // 3. Joystick-Abtastung im 10-ms-Raster
      static uint32_t lastJoyCheck = 0;
      if (HAL_GetTick() - lastJoyCheck >= 10) {
          lastJoyCheck = HAL_GetTick();
          Joystick_Update();

          // Im verriegelten Zustand: Auswertung der PIN-Gestenabfolge (gesperrt bei Admin-Lockout)
          if (!AccessControl_IsUnlocked() && !AccessControl_IsAdminLockout()) {
              if (Joystick_CheckSecretPin()) {
                  AccessControl_UnlockViaPin();
                  letzte_aktivitaet = HAL_GetTick();
              }
          }
      }

      // 4. Betriebsmodi bei aktivem Systemzugriff
      if (AccessControl_IsUnlocked()) {
          // Kurzer Tasterklick: Geschwindigkeitsstufe zyklisch inkrementieren (1..4)
          if (Joystick_GetButtonClicked()) {
              if (++speed_faktor > 4) {
                  speed_faktor = 1;
              }
              letzte_aktivitaet = HAL_GetTick();
          }

          // Langer Tastendruck (> 800 ms): Automatischen Sweep-Modus umschalten
          if (Joystick_GetButtonHeld()) {
              sweep_modus = !sweep_modus;
              letzte_aktivitaet = HAL_GetTick();
          }

          // Manuelle Achsenabweichung: Aktivitaets-Timer zuruecksetzen, bei starker Auslenkung Sweep beenden
          int32_t x_abw = Joystick_GetX();
          if (x_abw < -100 || x_abw > 100) {
              letzte_aktivitaet = HAL_GetTick();
              if (sweep_modus && (x_abw < -400 || x_abw > 400)) {
                  sweep_modus = false;
              }
          }

          // Bei aktivem Sweep: Auto-Lock durch fortlaufende Bewegung zurueckstellen
          if (sweep_modus) {
              letzte_aktivitaet = HAL_GetTick();
          }

          // Auto-Lock: Nach 15 Sekunden ohne Interaktion automatisch verriegeln
          if (!sweep_modus && (HAL_GetTick() - letzte_aktivitaet >= 15000)) {
              AccessControl_Lock();
              sweep_modus = false;
              Servo_SetAngle(90);
              aktuelle_position = 90.0f;
              letzte_aktivitaet = HAL_GetTick();
          }
      } else {
          // Im verriegelten Zustand Sweep deaktivieren
          sweep_modus = false;
      }

      // 5. Displayausgabe aktualisieren
      Display_Aktualisieren();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_ADC12
                              |RCC_PERIPHCLK_TIM2;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.Tim2ClockSelection = RCC_TIM2CLK_HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_61CYCLES_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x2000090E;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 719;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 999;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LDROT_GPIO_Port, LDROT_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LDGR_Pin|LDBL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, CS_Pin|RESET_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LDROT_Pin */
  GPIO_InitStruct.Pin = LDROT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LDROT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LDGR_Pin LDBL_Pin */
  GPIO_InitStruct.Pin = LDGR_Pin|LDBL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : CS_Pin RESET_Pin */
  GPIO_InitStruct.Pin = CS_Pin|RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SW_Pin */
  GPIO_InitStruct.Pin = SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SW_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// Zyklische Servosteuerung im 10-ms-Takt ueber TIM6-Interrupt (100 Hz)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        if (!AccessControl_IsUnlocked()) {
            return;
        }

        if (sweep_modus) {
            // Kontinuierliche Oszillation zwischen 0 und 180 Grad
            static int8_t sweep_richtung = 1;
            float sweep_schrittweite = 0.5f * (float)speed_faktor;

            aktuelle_position += (float)sweep_richtung * sweep_schrittweite;

            if (aktuelle_position >= 180.0f) {
                aktuelle_position = 180.0f;
                sweep_richtung = -1;
            } else if (aktuelle_position <= 0.0f) {
                aktuelle_position = 0.0f;
                sweep_richtung = 1;
            }
        } else {
            // Schrittweise Winkelverstellung abhaengig von Joystick-Auslenkung und Speed-Stufe
            int32_t x_abw = Joystick_GetX();
            float schrittweite = 0.5f * (float)speed_faktor;

            if (x_abw > 100) {
                aktuelle_position += schrittweite;
                if (aktuelle_position > 180.0f) aktuelle_position = 180.0f;
            } else if (x_abw < -100) {
                aktuelle_position -= schrittweite;
                if (aktuelle_position < 0.0f) aktuelle_position = 0.0f;
            }
        }

        Servo_SetAngle((uint8_t)aktuelle_position);
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
