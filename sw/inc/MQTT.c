// ----------------------------------------------------------------------------
//
// File name: MQTT.c
//
// Description: This code is used to bridge the TM4C123 board and the MQTT Web Application
//              via the ESP8266 WiFi board

// Authors:       Mark McDermott
// Orig gen date: June 3, 2023
// Last update:   July 21, 2023
//
// ----------------------------------------------------------------------------


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "inc/tm4c123gh6pm.h"
#include "ST7735.h"
#include "PLL.h"

#include "UART.h"
#include "UART5.h"
#include "esp8266.h"
#include "Lab3Clock.h"

#define DEBUG1                // First level of Debug
//#undef  DEBUG1                // Comment out to enable Debug1

#define UART5_FR_TXFF            0x00000020  // UART Transmit FIFO Full
#define UART5_FR_RXFE            0x00000010  // UART Receive FIFO Empty
#define UART5_LCRH_WLEN_8        0x00000060  // 8 bit word length
#define UART5_LCRH_FEN           0x00000010  // UART Enable FIFOs
#define UART5_CTL_UARTEN         0x00000001  // UART Enable

// ----   Function Prototypes not defined in header files  ------
//
void DisableInterrupts(void);
void EnableInterrupts(void);
void MQTT_to_TM4C(void);   /* forward; defined below, called from TM4C_to_MQTT */

// MQTT protocol mode 0-3: alarm_hr, alarm_min, clock_hr, clock_min
// Lab3 Mode 0-4: RUNNING, SET HOUR, SET MIN, SET ALRM H, SET ALRM M
// Map MQTT mode -> Lab3 Mode: 0->3, 1->4, 2->1, 3->2.  Lab3 Mode -> MQTT: 1->2, 2->3, 3->0, 4->1, 0->0
static uint8_t mqtt_to_lab3_mode(uint32_t m) {
  if (m == 0) return 3; if (m == 1) return 4; if (m == 2) return 1; return 2;
}
static uint32_t lab3_to_mqtt_mode(uint8_t m) {
  if (m == 0) return 0; if (m == 1) return 2; if (m == 2) return 3; if (m == 3) return 0; return 1;
}

// ----------   VARIABLES  ----------------------------
uint32_t mqtt_mil = 0;   // 0=12hr, 1=24hr (Lab 3 is 12hr; keep for web)

//Buffers for send / recv
char                    input_char;
char                    b2w_buf[64];
char                    w2b_buf[128];
static uint32_t         bufpos          = 0;
static volatile char    last_w2b_cmd    = 0;   /* diagnostic: last cmd received, show on LCD */

// Helper: clamp value to [lo, hi]
static uint32_t clamp_u32(uint32_t v, uint32_t lo, uint32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// --------------------------     W2B Parser      ------------------------------
// Single topic w2b: payload "1"-"6" → 1=12/24, 2=inc, 3=dec, 4=mode cycle, 5=theme, 6=freq
// ----------------------------------------------------------------------------
static void do_cmd(char c) {
  last_w2b_cmd = c;   /* so main loop can show on LCD that we received a command */
  DisableInterrupts();
  switch (c) {
    case '1':  /* 12/24 toggle */
      mqtt_mil = mqtt_mil ? 0 : 1;
      UpdateDisplay = 1;
      break;
    case '2':  /* increment */
      switch (Mode) {
        case 1: Time_Hours++;   if (Time_Hours > 12) Time_Hours = 1;   break;
        case 2: Time_Minutes++; if (Time_Minutes >= 60) Time_Minutes = 0; break;
        case 3: Alarm_Hours++;  if (Alarm_Hours > 12) Alarm_Hours = 1; Alarm_TotalMinutes = 0; break;
        case 4: Alarm_Minutes++; if (Alarm_Minutes >= 60) Alarm_Minutes = 0; break;
        case 0: Alarm_Enabled ^= 1; break;
        default: break;
      }
      UpdateDisplay = 1;
      break;
    case '3':  /* decrement */
      switch (Mode) {
        case 1: Time_Hours--;   if (Time_Hours < 1) Time_Hours = 12;   break;
        case 2: if (Time_Minutes == 0) Time_Minutes = 59; else Time_Minutes--; break;
        case 3: Alarm_Hours--;  if (Alarm_Hours < 1) Alarm_Hours = 12; Alarm_TotalMinutes = 0; break;
        case 4: if (Alarm_Minutes == 0) Alarm_Minutes = 59; else Alarm_Minutes--; break;
        case 0: Alarm_Enabled ^= 1; break;
        default: break;
      }
      UpdateDisplay = 1;
      break;
    case '4':  /* mode cycle */
      Mode++;
      if (Mode > 4) Mode = 0;
      UpdateDisplay = 1;
      break;
    case '5':  /* theme toggle */
      DarkMode = DarkMode ? 0 : 1;
      UpdateDisplay = 1;
      break;
    case '6':  /* speaker freq (no state on TM4C; could trigger tone) */
      break;
    default:
      break;
  }
  EnableInterrupts();
}

char MQTT_LastW2BCmd(void) {
  return last_w2b_cmd;
}

void Parser(void) {
  /* Trim to first token if comma present (e.g. "1" or "1,") */
  char *rest = w2b_buf;
  while (*rest == ' ' || *rest == '\t') rest++;
  if (!*rest) return;
  char *comma = strchr(rest, ',');
  if (comma) *comma = '\0';

  /* Single-digit payload from w2b (1-6); allow trailing \r or space */
  if (rest[0] >= '1' && rest[0] <= '6' && (rest[1] == '\0' || rest[1] == '\r' || rest[1] == ' ')) {
    #ifdef DEBUG1
    UART_OutString("W2B cmd=");
    UART_OutChar(rest[0]);
    UART_OutString("\r\n");
    #endif
    do_cmd(rest[0]);
    return;
  }

  /* Legacy "cmd,value" format (optional) */
  char cmd[16];
  char val[24];
  size_t rlen = strlen(rest);
  strncpy(cmd, rest, sizeof(cmd) - 1);
  cmd[sizeof(cmd) - 1] = '\0';
  char *valp = rest + rlen + 1;  /* value after comma we nulled */
  if (*valp)
    strncpy(val, valp, sizeof(val) - 1);
  else
    val[0] = '\0';
  val[sizeof(val) - 1] = '\0';

  #ifdef DEBUG1
  UART_OutString("W2B cmd=");
  UART_OutString(cmd);
  UART_OutString(" val=");
  UART_OutString(val);
  UART_OutString("\r\n");
  #endif

  if (strcmp(cmd, "mil") == 0) {
    uint32_t v = (uint32_t)atoi(val);
    mqtt_mil = (v == 1) ? 1 : 0;
    UpdateDisplay = 1;
  } else if (strcmp(cmd, "inc") == 0) {
    do_cmd('2');
  } else if (strcmp(cmd, "dec") == 0) {
    do_cmd('3');
  } else if (strcmp(cmd, "mode") == 0) {
    uint32_t v = clamp_u32((uint32_t)atoi(val), 0, 3);
    Mode = mqtt_to_lab3_mode(v);
    UpdateDisplay = 1;
  } else if (strcmp(cmd, "theme") == 0) {
    if (strcmp(val, "dark") == 0) DarkMode = 1; else DarkMode = 0;
    UpdateDisplay = 1;
  }
}
  
// -----------------------  TM4C_to_MQTT  --------------------------------------
// Sends clock data to MQTT Web App via ESP. Format: mode,hour,min,sec,mil\n
// Uses UART5 output only (no sprintf) for speed.
// Process any pending w2b commands first so we send state after applying them.
// ----------------------------------------------------------------------------
void TM4C_to_MQTT(void){
  MQTT_to_TM4C();   /* drain w2b and apply commands before sending; declared below */
  uint32_t smode = lab3_to_mqtt_mode(Mode);
  UART5_OutUDec(smode);
  UART5_OutChar(',');
  UART5_OutUDec((uint32_t)Time_Hours);
  UART5_OutChar(',');
  UART5_OutUDec((uint32_t)Time_Minutes);
  UART5_OutChar(',');
  UART5_OutUDec((uint32_t)Time_Seconds);
  UART5_OutChar(',');
  UART5_OutUDec(mqtt_mil);
  UART5_OutChar('\n');

  #ifdef DEBUG1
  UART_OutString("B2W: ");
  UART_OutUDec(smode); UART_OutChar(',');
  UART_OutUDec((uint32_t)Time_Hours); UART_OutChar(',');
  UART_OutUDec((uint32_t)Time_Minutes); UART_OutChar(',');
  UART_OutUDec((uint32_t)Time_Seconds); UART_OutChar(',');
  UART_OutUDec(mqtt_mil); UART_OutString("\r\n");
  #endif
}
 
// -------------------------   MQTT_to_TM4C  -----------------------------------
// This routine receives the command data from the MQTT Web App and parses the
// data and feeds the commands to the TM4C.
// -----------------------------------------------------------------------------
//
//    Convert this routine to use a FIFO
//
// 
void MQTT_to_TM4C(void) {
  /* Drain UART5 until we have a complete line or buffer full (don't rely on one char per call) */
  while ((UART5_FR_R & UART5_FR_RXFE) == 0) {
    input_char = (char)(UART5_DR_R & 0xFF);

    if (input_char != '\n' && input_char != '\r') {
      if (bufpos < sizeof(w2b_buf) - 2) {
        w2b_buf[bufpos++] = input_char;
      } else {
        bufpos = 0;  /* overflow: discard and wait for next line */
      }
      #ifdef DEBUG1
      UART_OutChar(input_char);
      #endif
    } else {
      if (bufpos > 0) {
        w2b_buf[bufpos] = '\0';
        Parser();
      }
      bufpos = 0;
    }
  }
}

