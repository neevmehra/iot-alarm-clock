/**
 * @file Dump.c
 * @author your name (you@domain.com), Jonathan Valvano, Matthew Yu
 *    <TA NAME and LAB SECTION # HERE>
 * @brief 
 *    A set of debugging functions that capture data for later inspection.
 *    There are two primary methods:
 *       - DebugCapture will record data and time.
 *       - JitterMeasure will measure real time jitter.
 * @version 0.2
 * @date 2022-09-01 <REPLACE WITH DATE OF LAST REVISION>
 *
 * @copyright Copyright (c) 2022
 */

/** File includes. */
#include <stdint.h>
#include "../inc/Dump.h"
#include "../inc/tm4c123gh6pm.h"

// Global variables
uint32_t DumpTimeBuffer[DUMPBUFSIZE];
uint32_t DumpDataBuf[DUMPBUFSIZE];
uint32_t DumpNum;

void Timer1_Init(void) {
    volatile uint32_t delay;
    SYSCTL_RCGCTIMER_R    |= 0x02;                // 0) activate TIMER1
    delay                  = SYSCTL_RCGCTIMER_R;  // allow time to finish activating
    TIMER1_CTL_R           = 0x00000000;          // 1) disable TIMER1A during setup
    TIMER1_CFG_R           = 0x00000000;          // 2) configure for 32-bit mode
    TIMER1_TAMR_R          = 0x00000002;          // 3) configure for periodic mode, default down-count settings
    TIMER1_TAILR_R         = 0xFFFFFFFF;          // 4) reload value
    TIMER1_TAPR_R          = 0;                   // 5) bus clock resolution
    TIMER1_CTL_R           = 0x00000001;          // 10) enable TIMER1A
}

void DumpInit(void){
    Timer1_Init();   /* so DumpCapture timestamps (TIMER1_TAR_R) are valid */
    DumpNum = 0;
}

void DumpCapture(uint32_t data){
    if (DumpNum < DUMPBUFSIZE) {
        DumpDataBuf[DumpNum] = data;
        DumpTimeBuffer[DumpNum] = TIMER1_TAR_R;
        DumpNum++;
    }
}

uint32_t DumpCount(void){ 
    return DumpNum;
}

uint32_t* DumpData(void){ 
    return DumpDataBuf;
}

uint32_t* DumpTime(void){ 
    return DumpTimeBuffer;
}

static uint32_t JitterLastTime;
static uint8_t  JitterFirstCall;
static uint32_t JitterMaxElapsed;
static uint32_t JitterMinElapsed;

void JitterInit(void){
    JitterFirstCall = 1;
    JitterMaxElapsed = 0;
    JitterMinElapsed = 0xFFFFFFFF;
}

void JitterMeasure(void){
    uint32_t currentTime = TIMER1_TAR_R;
    if (JitterFirstCall) {
        JitterFirstCall = 0;
        JitterLastTime = currentTime;
        return;
    }
    // Timer counts down, elapsed = lastTime - currentTime 
    uint32_t elapsed = JitterLastTime - currentTime;
    if (elapsed > JitterMaxElapsed) JitterMaxElapsed = elapsed;
    if (elapsed < JitterMinElapsed) JitterMinElapsed = elapsed;
    JitterLastTime = currentTime;
}

uint32_t JitterGet(void){
    return JitterMaxElapsed - JitterMinElapsed;
}