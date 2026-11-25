#pragma once
#include "transport.h"
uint8_t ts_zifi_api(void);           // 1 if the core has the ZiFi API
extern const transport_t tr_zifi;    // ESP8266 UART
extern const transport_t tr_rs232;   // RS232 -> MCU -> USB serial (CDC if02 / USB-UART adapter)
