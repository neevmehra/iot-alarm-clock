// -------------------------------------------------------------------
// File name:     Lab4E_Main.c
// Description:   Lab 4E MQTT Clock Control with Lab 3 alarm clock integrated.
//                Switches on Port F: PF0=mode, PF1=index, PF2=light/dark, PF3=up, PF4=down.
//
// Authors:       Mark McDermott, Lab 3 integration
// Date:          June 6, 2023
// -------------------------------------------------------------------

#include <stdio.h>
#include <stdint.h>

#include "inc/tm4c123gh6pm.h"
#include "inc/ST7735.h"
#include "inc/PLL.h"
#include "inc/Timer0A.h"
#include "inc/Timer2A.h"
#include "inc/Timer5A.h"
#include "inc/UART.h"
#include "inc/UART5.h"
#include "inc/esp8266_base.h"
#include "inc/MQTT.h"
#include "inc/Unified_Port_Init.h"
#include "inc/Lab3Clock.h"

#define PF0  (*((volatile uint32_t *)0x40025004))
#define PF4  (*((volatile uint32_t *)0x40025040))

void DisableInterrupts(void);
void EnableInterrupts(void);
void WaitForInterrupt(void);

// -----------------------------------------------------------------
// -------------------- MAIN ---------------------------------------
// -----------------------------------------------------------------

int main(void) {
  uint32_t lastInput = 0x1F;
  uint32_t currentInput;

  DisableInterrupts();
  PLL_Init(Bus80MHz);
  UART_Init();
  UART5_Init();
  ST7735_InitR(INITR_REDTAB);
  Unified_Port_Init();

  ST7735_OutString("Reseting ESP\n");
  Reset_8266();
  SetupWiFi();

  Lab3Clock_Init();   // Port F switches + Sound (PD0 speaker)
  ST7735_FillScreen(DarkMode ? ST7735_WHITE : ST7735_BLACK);

  Timer0A_Init(&Clock_Task, 80000000, 4);   // 1 Hz clock tick (Lab 3)
  Timer2A_Init(&MQTT_to_TM4C, 400000, 7);  // Poll ESP for MQTT commands
  Timer5A_Init(&TM4C_to_MQTT, 80000000, 7); // Send clock state to MQTT every second

  EnableInterrupts();

  DrawClock();

  while (1) {
    currentInput = Switch_Input();

    // PF2: Light/dark mode
    if ((lastInput & 0x04) && !(currentInput & 0x04)) {
      DarkMode ^= 1;
      ST7735_FillScreen(DarkMode ? ST7735_WHITE : ST7735_BLACK);
      DrawClock();
      UpdateDisplay = 1;
      for (volatile int i = 0; i < 80000; i++) {}
    }

    if ((currentInput & 0x1F) != 0x1F) {
      Sound_Stop();
      Alarm_Sounding = 0;
      Alarm_TotalMinutes = 0;
      Alarm_RemainingMinutes = 0;
      Alarm_ProgressInit = 0;
    }

    // PF1: Index (alarm tone)
    if ((lastInput & 0x02) && !(currentInput & 0x02)) {
      DisableInterrupts();
      index = (index + 1) % 4;
      if (Alarm_Sounding)
        Sound_Start(soundarr[index]);
      EnableInterrupts();
      for (volatile int i = 0; i < 80000; i++) {}
    }

    // PF0: Mode
    if ((lastInput & 0x01) && !(currentInput & 0x01)) {
      Mode++;
      if (Mode > 4) Mode = 0;
      ST7735_FillScreen(DarkMode ? ST7735_WHITE : ST7735_BLACK);
      DrawClock();
      UpdateDisplay = 1;
      for (volatile int i = 0; i < 80000; i++) {}
    }

    // PF3: Up — increment
    if ((lastInput & 0x08) && !(currentInput & 0x08)) {
      if (Mode == 1) {
        DisableInterrupts();
        Time_Hours++;   if (Time_Hours > 12) Time_Hours = 1;
        EnableInterrupts();
      } else if (Mode == 2) {
        DisableInterrupts();
        Time_Minutes++; if (Time_Minutes >= 60) Time_Minutes = 0;
        EnableInterrupts();
      } else if (Mode == 3) {
        Alarm_Hours++; if (Alarm_Hours > 12) Alarm_Hours = 1;
        Alarm_TotalMinutes = 0;
      } else if (Mode == 4) {
        Alarm_Minutes++; if (Alarm_Minutes >= 60) Alarm_Minutes = 0;
      } else if (Mode == 0) {
        Alarm_Enabled ^= 1;
      }
      UpdateDisplay = 1;
      Alarm_TotalMinutes = 0;
      for (volatile int i = 0; i < 80000; i++) {}
    }

    // PF4: Down — decrement
    if ((lastInput & 0x10) && !(currentInput & 0x10)) {
      if (Mode == 1) {
        DisableInterrupts();
        Time_Hours--;   if (Time_Hours < 1) Time_Hours = 12;
        EnableInterrupts();
      } else if (Mode == 2) {
        DisableInterrupts();
        if (Time_Minutes == 0) Time_Minutes = 59; else Time_Minutes--;
        EnableInterrupts();
      } else if (Mode == 3) {
        Alarm_Hours--; if (Alarm_Hours < 1) Alarm_Hours = 12;
        Alarm_TotalMinutes = 0;
      } else if (Mode == 4) {
        if (Alarm_Minutes == 0) Alarm_Minutes = 59; else Alarm_Minutes--;
      } else if (Mode == 0) {
        Alarm_Enabled ^= 1;
      }
      UpdateDisplay = 1;
      for (volatile int i = 0; i < 80000; i++) {}
    }

    lastInput = currentInput;

    if (UpdateDisplay) {
      UpdateDisplay = 0;
      ST7735_SetTextColor(DarkMode ? ST7735_BLACK : ST7735_WHITE);
      ST7735_SetCursor(0, 3);
      if (Mode == 0)      ST7735_OutString("Mode: RUNNING   ");
      else if (Mode == 1) ST7735_OutString("Mode: SET HOUR  ");
      else if (Mode == 2) ST7735_OutString("Mode: SET MIN   ");
      else if (Mode == 3) ST7735_OutString("Mode: SET ALRM H");
      else                ST7735_OutString("Mode: SET ALRM M");
      DrawClock();
      /* Draw W2B after clock so it isn't covered; use bottom row so it stays visible */
      ST7735_SetCursor(0, 18);
      { char w = MQTT_LastW2BCmd(); ST7735_OutString("W2B:"); ST7735_OutChar(w ? w : '-'); ST7735_OutString("   "); }
    }
  }
}
