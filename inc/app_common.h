#ifndef APP_COMMON_H
#define APP_COMMON_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Thong so cau hinh he thong */
#define MAX_PARKING_SLOTS    10      // Tong so cho do xe toi da
#define DETECT_DISTANCE_CM   15.0f   // Khoang cach nhan dien xe (cm)
#define GATE_OPEN_TIME_MS    4000    // Thoi gian mo thanh chan (4 giay)

/* Su kien tu Cam bien -> Xu ly bai do */
typedef enum {
    EVENT_CAR_ENTER = 1,
    EVENT_CAR_EXIT  = 2
} ParkingEvent_t;

/* Lenh tu Xu ly bai do -> Dieu khien cong Servo */
typedef enum {
    CMD_OPEN_ENTRY_GATE = 1,
    CMD_OPEN_EXIT_GATE  = 2
} GateCommand_t;

/* Doi tuong FreeRTOS toan cuc */
extern QueueHandle_t xEventQueue;
extern QueueHandle_t xGateQueue;
extern SemaphoreHandle_t xCarCountMutex;
extern SemaphoreHandle_t xI2CMutex;

extern volatile uint8_t g_CarCount;

#endif /* APP_COMMON_H */