// Lab3Clock.h - Alarm clock from Lab 3, integrated for Lab 4 (switches on Port F).
// Clock state is shared with MQTT.c for web sync.

#ifndef LAB3_CLOCK_H
#define LAB3_CLOCK_H

#include <stdint.h>

// Clock time (12-hour display: 1-12)
extern volatile uint32_t Time_Seconds;
extern volatile uint32_t Time_Minutes;
extern volatile uint32_t Time_Hours;

// Alarm
extern volatile uint32_t Alarm_Minutes;
extern volatile uint32_t Alarm_Hours;
extern volatile uint32_t Alarm_Enabled;
extern volatile uint32_t Alarm_TotalMinutes;
extern volatile uint32_t Alarm_RemainingMinutes;
extern volatile uint8_t  Alarm_ProgressInit;
extern volatile int     Alarm_Sounding;

// Display
extern volatile uint32_t UpdateDisplay;
extern volatile uint8_t  DarkMode;
extern volatile uint8_t  Mode;  // 0=RUNNING, 1=SET HOUR, 2=SET MIN, 3=SET ALRM H, 4=SET ALRM M

// Sound frequency index (0-3)
extern volatile int index;
extern uint32_t soundarr[4];

void Lab3Clock_Init(void);   // Switch_Init (Port F) + Sound_Init
void Switch_Init(void);      // Port F: PF0=mode, PF1=index, PF2=light/dark, PF3=up, PF4=down
uint32_t Switch_Input(void); // returns ~(PF0-PF4), 0 = pressed
void Sound_Init(void);
void Sound_Start(uint32_t period);
void Sound_Stop(void);
void Clock_Task(void);       // 1 Hz tick (use with Timer0A)
void DrawClock(void);       // Redraw LCD

#endif
