#ifndef DRIVER_DISPLAY_STORAGE_H
#define DRIVER_DISPLAY_STORAGE_H

#include "app_common.h"

void DisplayStorage_HardwareInit(void);
void LCD_ShowParkingStatus(uint8_t available_slots, uint8_t is_full);
void Alert_SetStatus(uint8_t is_full);
void Alert_BeepShort(void);
void Storage_LogEventCSV(const char* event_name, uint8_t current_cars);

#endif /* DRIVER_DISPLAY_STORAGE_H */