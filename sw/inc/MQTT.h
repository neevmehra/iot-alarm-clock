// Protocol: see sw/mqtt_protocol.json. W2B = "cmd,value"; B2W = "mode,hour,min,sec,mil".

// -------------------------   MQTT_to_TM4C  -----------------------------------
// Receives W2B command data from the Web via ESP8266 and parses it.
void MQTT_to_TM4C(void);

// --------------------------   Parser  ---------------------------------------
// Decodes "cmd,value" from w2b_buf and updates clock/edit state.
void Parser(void);

// -------------------------   TM4C_to_MQTT  -----------------------------------
// Sends clock state to Web via ESP (mode,hour,min,sec,mil).
void TM4C_to_MQTT(void);

// Clock state is in Lab3Clock (Time_*, Alarm_*, Mode, DarkMode). Parser updates those; TM4C_to_MQTT reads them.
extern uint32_t mqtt_mil;     // 0=12hr, 1=24hr (for web; Lab 3 is 12hr)

// Diagnostic: last w2b command received ('1'-'6') or 0. Show on LCD to verify web->board path.
char MQTT_LastW2BCmd(void);

