#include "task_parking_display.h"
#include "driver_display_storage.h"

void MemberB_InitTasks(void) {
    /* Chi tao Task tai day; khoi tao LCD se goi ben trong Task khi Scheduler da chay */
    xTaskCreate(TaskParking, "TaskParking", 256, NULL, 3, NULL);
    xTaskCreate(TaskDisplay, "TaskDisplay", 256, NULL, 2, NULL);
}

void TaskParking(void *argument) {
    ParkingEvent_t evt;
    GateCommand_t gate_cmd;
    uint8_t cars_snapshot = 0;

    for (;;) {
        if (xQueueReceive(xEventQueue, &evt, portMAX_DELAY) == pdPASS) {
            if (evt == EVENT_CAR_ENTER) {
                xSemaphoreTake(xCarCountMutex, portMAX_DELAY);
                if (g_CarCount < MAX_PARKING_SLOTS) {
                    g_CarCount++;
                    cars_snapshot = g_CarCount;
                    xSemaphoreGive(xCarCountMutex);

                    gate_cmd = CMD_OPEN_ENTRY_GATE;
                    xQueueSend(xGateQueue, &gate_cmd, 0);

                    xSemaphoreTake(xI2CMutex, portMAX_DELAY);
                    Storage_LogEventCSV("XE_VAO", cars_snapshot);
                    xSemaphoreGive(xI2CMutex);
                } else {
                    xSemaphoreGive(xCarCountMutex);
                    Alert_BeepShort(); // Bai day: Keu coi canh bao, giu dong cong
                }
            } else if (evt == EVENT_CAR_EXIT) {
                xSemaphoreTake(xCarCountMutex, portMAX_DELAY);
                if (g_CarCount > 0) {
                    g_CarCount--;
                    cars_snapshot = g_CarCount;
                    xSemaphoreGive(xCarCountMutex);

                    gate_cmd = CMD_OPEN_EXIT_GATE;
                    xQueueSend(xGateQueue, &gate_cmd, 0);

                    xSemaphoreTake(xI2CMutex, portMAX_DELAY);
                    Storage_LogEventCSV("XE_RA", cars_snapshot);
                    xSemaphoreGive(xI2CMutex);
                } else {
                    xSemaphoreGive(xCarCountMutex);
                }
            }
        }
    }
}

void TaskDisplay(void *argument) {
    uint8_t cars = 0, available = 0, is_full = 0;

    xSemaphoreTake(xI2CMutex, portMAX_DELAY);
    DisplayStorage_HardwareInit(); // Khoi tao I2C & LCD an toan bang vTaskDelay
    xSemaphoreGive(xI2CMutex);

    for (;;) {
        xSemaphoreTake(xCarCountMutex, portMAX_DELAY);
        cars = g_CarCount;
        xSemaphoreGive(xCarCountMutex);

        available = MAX_PARKING_SLOTS - cars;
        is_full = (available == 0) ? 1 : 0;

        Alert_SetStatus(is_full);

        xSemaphoreTake(xI2CMutex, portMAX_DELAY);
        LCD_ShowParkingStatus(available, is_full);
        xSemaphoreGive(xI2CMutex);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
