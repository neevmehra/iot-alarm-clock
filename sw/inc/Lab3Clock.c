// Lab3Clock.c - Lab 3 alarm clock integrated for Lab 4.
// Switches on Port F (PF0=mode, PF1=index, PF2=light/dark, PF3=up, PF4=down).
// PD0 = speaker (may conflict with Lab 4 SSI1 on PD0; move speaker if needed).

#include <stdint.h>
#include "inc/tm4c123gh6pm.h"
#include "inc/ST7735.h"
#include "inc/Lab3Clock.h"
#include "inc/Dump.h"

// ---------- Clock state (shared with MQTT for web sync) ----------
volatile uint32_t Time_Seconds = 0;
volatile uint32_t Time_Minutes = 0;
volatile uint32_t Time_Hours = 12;

volatile uint32_t Alarm_Minutes = 0;
volatile uint32_t Alarm_Hours = 6;
volatile uint32_t Alarm_Enabled = 1;
volatile uint32_t Alarm_TotalMinutes = 0;
volatile uint32_t Alarm_RemainingMinutes = 0;
volatile uint8_t  Alarm_ProgressInit = 0;
volatile int      Alarm_Sounding = 0;

volatile uint32_t UpdateDisplay = 0;
volatile uint8_t  DarkMode = 0;
volatile uint8_t  Mode = 0;  // 0=RUNNING, 1=SET HOUR, 2=SET MIN, 3=SET ALRM H, 4=SET ALRM M

volatile int index = 0;
uint32_t soundarr[4] = { 25000, 110000, 210000, 310000 };

// ---------- Sound (PD0 = speaker) ----------
void Sound_Init(void) {
    SYSCTL_RCGCGPIO_R |= 0x08;
    while ((SYSCTL_PRGPIO_R & 0x08) == 0) {}
    GPIO_PORTD_DIR_R |= 0x01;
    GPIO_PORTD_AFSEL_R &= ~0x01;
    GPIO_PORTD_DEN_R |= 0x01;
    GPIO_PORTD_DATA_R &= ~0x01;

    NVIC_ST_CTRL_R = 0;
    NVIC_ST_RELOAD_R = 0;
    NVIC_ST_CURRENT_R = 0;
    NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R & 0x00FFFFFF) | 0x40000000;
    NVIC_ST_CTRL_R = 0x07;
}

void Sound_Start(uint32_t period) {
    NVIC_ST_RELOAD_R = period - 1;
    NVIC_ST_CURRENT_R = 0;
}

void Sound_Stop(void) {
    NVIC_ST_RELOAD_R = 0;
    GPIO_PORTD_DATA_R &= ~0x01;
}

void SysTick_Handler(void) {
    if (NVIC_ST_RELOAD_R > 0)
        GPIO_PORTD_DATA_R ^= 0x01;
}

// ---------- Switches on Port F (PF0-PF4), pull-up, 0 = pressed ----------
void Switch_Init(void) {
    volatile unsigned long delay;
    SYSCTL_RCGCGPIO_R |= 0x20;
    while ((SYSCTL_PRGPIO_R & 0x20) == 0) {}
    delay = SYSCTL_RCGCGPIO_R;
    GPIO_PORTF_LOCK_R = 0x4C4F434B;
    GPIO_PORTF_CR_R = 0x1F;
    GPIO_PORTF_AMSEL_R &= ~0x1F;
    GPIO_PORTF_PCTL_R &= ~0x000FFFFF;
    GPIO_PORTF_DIR_R &= ~0x1F;   // all inputs
    GPIO_PORTF_AFSEL_R &= ~0x1F;
    GPIO_PORTF_PUR_R |= 0x1F;    // pull-up on all five
    GPIO_PORTF_DEN_R |= 0x1F;
}

uint32_t Switch_Input(void) {
    return (GPIO_PORTF_DATA_R & 0x1F);  // 0x1F = all released, 0 = pressed
}

void Lab3Clock_Init(void) {
    Switch_Init();
    Sound_Init();
}

// ---------- Clock tick ----------
static uint32_t MinutesUntilAlarm(void) {
    uint32_t th = Time_Hours % 12;
    uint32_t ah = Alarm_Hours % 12;
    uint32_t nowS = ((th * 60) + Time_Minutes) * 60 + Time_Seconds;
    uint32_t alarmS = ((ah * 60) + Alarm_Minutes) * 60;
    int32_t diff = (int32_t)alarmS - (int32_t)nowS;
    if (diff <= 0) return 0;
    return (uint32_t)diff;
}

void Clock_Task(void) {
    Time_Seconds++;
    if (Time_Seconds >= 60) {
        Time_Seconds = 0;
        Time_Minutes++;
        if (Time_Minutes >= 60) {
            Time_Minutes = 0;
            Time_Hours++;
            if (Time_Hours > 12) Time_Hours = 1;
        }
    }

    if (Alarm_Enabled && (Time_Hours == Alarm_Hours) && (Time_Minutes == Alarm_Minutes) && (Time_Seconds == 0)) {
        Alarm_Sounding = 1;
        Sound_Start(soundarr[index]);
    }

    if (Mode == 0 && Alarm_Enabled) {
        uint32_t rem = MinutesUntilAlarm();
        Alarm_RemainingMinutes = rem;
        if (Alarm_TotalMinutes == 0 && rem > 0)
            Alarm_TotalMinutes = rem;
    } else {
        Alarm_TotalMinutes = 0;
        Alarm_RemainingMinutes = 0;
    }
    UpdateDisplay = 1;

    DumpCapture(Time_Seconds);   /* lab: Time_Seconds in data buffer for watch */
}

// ---------- Display helpers ----------
static void u8_to_str(uint32_t val, char *buf) {
    buf[0] = (char)('0' + (val / 10));
    buf[1] = (char)('0' + (val % 10));
    buf[2] = '\0';
}

#define CLOCK_CX    64
#define CLOCK_CY    110
#define CLOCK_R     48
#define CLOCK_RECT_LEFT   (CLOCK_CX - CLOCK_R)
#define CLOCK_RECT_TOP    (CLOCK_CY - CLOCK_R)
#define CLOCK_RECT_WH     (2 * CLOCK_R + 1)
#define PROG_X      14
#define PROG_Y      40
#define PROG_W      100
#define PROG_H      6

static const int16_t hand_dx[60] = {
    0, 27, 53, 80, 106, 132, 158, 183, 208, 232, 255, 256, 256, 255, 252, 256, 238, 228, 216, 201, 185, 167, 147, 125, 102, 77, 51, 25, 0, -25, -51, -77, -102, -125, -147, -167, -185, -201, -216, -228, -238, -246, -252, -255, -256, -256, -255, -232, -208, -183, -158, -132, -106, -80, -53, -27
};
static const int16_t hand_dy[60] = {
    -256, -255, -255, -252, -246, -238, -228, -216, -201, -185, -167, -147, -125, -102, -77, 0, 25, 51, 77, 102, 125, 147, 167, 185, 201, 216, 228, 238, 246, 252, 255, 255, 256, 256, 255, 252, 246, 238, 228, 216, 201, 185, 167, 147, 125, 102, 77, 51, 25, 0, -25, -51, -77, -102, -125, -147, -167, -185, -201, -255
};
static const int8_t hour_dx[12] = { 17, 29, 34, 29, 17, 0, -17, -29, -34, -29, -17, 0 };
static const int8_t hour_dy[12] = { -29, -17, 0, 17, 29, 34, 29, 17, 0, -17, -29, -34 };

static void DrawCircleOutline(int cx, int cy, int r, uint16_t color) {
    int x = 0, y = r;
    int d = 1 - r;
    while (y >= x) {
        ST7735_DrawPixel(cx + x, cy + y, color);
        ST7735_DrawPixel(cx - x, cy + y, color);
        ST7735_DrawPixel(cx + x, cy - y, color);
        ST7735_DrawPixel(cx - x, cy - y, color);
        ST7735_DrawPixel(cx + y, cy + x, color);
        ST7735_DrawPixel(cx - y, cy + x, color);
        ST7735_DrawPixel(cx + y, cy - x, color);
        ST7735_DrawPixel(cx - y, cy - x, color);
        x++;
        if (d < 0) d += 2 * x + 1;
        else { y--; d += 2 * (x - y) + 1; }
    }
}

static void DrawProgressBar(uint32_t total, uint32_t remaining, uint16_t fg, uint16_t bg) {
    ST7735_FillRect(PROG_X, PROG_Y, PROG_W, PROG_H, bg);
    ST7735_DrawFastHLine(PROG_X, PROG_Y, PROG_W, fg);
    ST7735_DrawFastHLine(PROG_X, PROG_Y + PROG_H - 1, PROG_W, fg);
    ST7735_DrawFastVLine(PROG_X, PROG_Y, PROG_H, fg);
    ST7735_DrawFastVLine(PROG_X + PROG_W - 1, PROG_Y, PROG_H, fg);
    if (total == 0 || remaining > total) return;
    uint32_t fillWidth = ((total - remaining) * (PROG_W - 2)) / total;
    if (fillWidth > 0)
        ST7735_FillRect(PROG_X + 1, PROG_Y + 1, fillWidth, PROG_H - 2, fg);
}

void DrawClock(void) {
    char str[12];
    int cx = CLOCK_CX, cy = CLOCK_CY, r = CLOCK_R;
    int hx, hy, mx, my, sx, sy;
    uint32_t h = Time_Hours, m = Time_Minutes, s = Time_Seconds;
    uint32_t ah = Alarm_Hours, am = Alarm_Minutes;
    uint32_t hi;
    uint32_t prog_total = Alarm_TotalMinutes, prog_rem = Alarm_RemainingMinutes;
    uint8_t alarm_enabled = (uint8_t)Alarm_Enabled;
    uint16_t bg = DarkMode ? ST7735_WHITE : ST7735_BLACK;
    uint16_t fg = DarkMode ? ST7735_BLACK : ST7735_WHITE;

    ST7735_SetTextColor(fg);
    ST7735_SetCursor(0, 0);
    u8_to_str(h, str); ST7735_OutString(str); ST7735_OutString(":");
    u8_to_str(m, str); ST7735_OutString(str); ST7735_OutString(":");
    u8_to_str(s, str); ST7735_OutString(str);

    ST7735_SetCursor(0, 1);
    ST7735_OutString("Alarm ");
    u8_to_str(ah, str); ST7735_OutString(str); ST7735_OutString(":");
    u8_to_str(am, str); ST7735_OutString(str);
    if (Alarm_Enabled) ST7735_OutString(" ON "); else ST7735_OutString(" OFF");

    if (alarm_enabled && (prog_total > 0) && (prog_rem <= prog_total))
        DrawProgressBar(prog_total, prog_rem, fg, bg);
    else
        DrawProgressBar(0, 0, fg, bg);

    ST7735_FillRect(CLOCK_RECT_LEFT, CLOCK_RECT_TOP, CLOCK_RECT_WH, CLOCK_RECT_WH, bg);
    DrawCircleOutline(cx, cy, r, fg);

    for (uint32_t hr = 1; hr <= 12; hr++) {
        int nx = cx + (int)hour_dx[hr - 1], ny = cy + (int)hour_dy[hr - 1];
        if (hr == 12) {
            ST7735_DrawChar(nx - 6, ny - 4, '1', fg, bg, 1);
            ST7735_DrawChar(nx, ny - 4, '2', fg, bg, 1);
        } else if (hr >= 10) {
            ST7735_DrawChar(nx - 6, ny - 4, '1', fg, bg, 1);
            ST7735_DrawChar(nx, ny - 4, (char)('0' + (hr - 10)), fg, bg, 1);
        } else {
            ST7735_DrawChar(nx - 3, ny - 4, (char)('0' + hr), fg, bg, 1);
        }
    }

    hi = ((h % 12) * 60 + m) / 12; hi %= 60;
    hx = cx + (22 * (int)hand_dx[hi]) / 256; hy = cy + (22 * (int)hand_dy[hi]) / 256;
    ST7735_DrawLine((int32_t)cx, (int32_t)cy, (int32_t)hx, (int32_t)hy, fg);
    hi = m % 60;
    mx = cx + (36 * (int)hand_dx[hi]) / 256; my = cy + (36 * (int)hand_dy[hi]) / 256;
    ST7735_DrawLine((int32_t)cx, (int32_t)cy, (int32_t)mx, (int32_t)my, fg);
    hi = s % 60;
    sx = cx + (42 * (int)hand_dx[hi]) / 256; sy = cy + (42 * (int)hand_dy[hi]) / 256;
    ST7735_DrawLine((int32_t)cx, (int32_t)cy, (int32_t)sx, (int32_t)sy, fg);
}
