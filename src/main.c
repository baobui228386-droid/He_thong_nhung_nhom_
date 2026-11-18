#include "app_common.h"
#include "task_gate_sensor.h"
#include "task_parking_display.h"

/* Khai bao cac doi tuong giao tiep dung chung toan he thong */
QueueHandle_t xEventQueue;
QueueHandle_t xGateQueue;
SemaphoreHandle_t xCarCountMutex;
SemaphoreHandle_t xI2CMutex;
volatile uint8_t g_CarCount = 0; // So xe ban dau

/* Ham khoi tao Xung nhip va Chan GPIO thuan CMSIS (Thay the HAL) */
static void Hardware_GPIO_RCC_Init(void) {
    /* 1. Bat Clock cho GPIOA, GPIOB, AFIO, TIM1, SPI1 (APB2) va TIM2, I2C1 (APB1) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN |
                    RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SPI1EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_I2C1EN;

    /* 2. Cau hinh GPIOA (PA0 -> PA7 tren thanh ghi CRL)
     * PA0 (TRIG1): Output Push-Pull 2MHz (0x2)
     * PA1 (ECHO1): Input Pull-down/up    (0x8, ODR=0)
     * PA2 (TRIG2): Output Push-Pull 2MHz (0x2)
     * PA3 (ECHO2): Input Pull-down/up    (0x8, ODR=0)
     * PA4 (SD_CS): Output Push-Pull 50MHz(0x3)
     * PA5 (SCK)  : AF Push-Pull 50MHz    (0xB)
     * PA6 (MISO) : Input Pull-up         (0x8, ODR=1)
     * PA7 (MOSI) : AF Push-Pull 50MHz    (0xB)
     */
    GPIOA->CRL = 0xB8B38282;
    GPIOA->ODR &= ~((1 << 1) | (1 << 3)); // Keo tro noi PA1, PA3 xuong GND
    GPIOA->ODR |= (1 << 6);               // Keo tro noi PA6 len VCC

    /* Cau hinh PA8 (TIM1_CH1), PA9 (TIM1_CH2): AF Push-Pull 50MHz (0xB) */
    GPIOA->CRH &= ~(0xFF << 0);
    GPIOA->CRH |=  (0xBB << 0);

    /* 3. Cau hinh GPIOB
     * PB0 (LED Xanh), PB1 (LED Do): Output Push-Pull 2MHz (0x2)
     * PB6 (I2C1_SCL), PB7 (I2C1_SDA): AF Open-Drain 50MHz (0xF)
     */
    GPIOB->CRL &= ~((0xFF << 0) | (0xFF << 24));
    GPIOB->CRL |=  ((0x22 << 0) | (0xFF << 24));

    /* PB10 (Coi Buzzer): Output Push-Pull 2MHz (0x2) */
    GPIOB->CRH &= ~(0x0F << 8);
    GPIOB->CRH |=  (0x02 << 8);
}

int main(void) {
    /* Khoi tao xung nhip he thong 72MHz tu CMSIS */
    SystemInit();
    
    /* Thiet lap bang uu tien ngat cho FreeRTOS tren loi ARM Cortex-M3 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* Khoi tao toan bo phan cung (Clock & GPIO) */
    Hardware_GPIO_RCC_Init();

    /* Tao cac doi tuong giao tiep giua cac Task */
    xEventQueue    = xQueueCreate(5, sizeof(ParkingEvent_t));
    xGateQueue     = xQueueCreate(5, sizeof(GateCommand_t));
    xCarCountMutex = xSemaphoreCreateMutex();
    xI2CMutex      = xSemaphoreCreateMutex();

    /* Khoi tao cac Task cua Thanh vien A va Thanh vien B */
    MemberA_InitTasks();
    MemberB_InitTasks();

    /* Bat dau chay bo lap lich FreeRTOS */
    vTaskStartScheduler();

    /* Chuong trinh se khong bao gio chay den day tru khi khong du RAM de khoi dong */
    while (1) {}
}

/* =================================================================================== */
/* CAC HAM HOOK CUA FREERTOS DE XU LY LOI TRAN BO NHO (KHAC PHUC LOI LINKING L6218E) */
/* =================================================================================== */

/* Ham xu ly khi mot Task bi tran bo nho Stack */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    /* Treo he thong bang vong lap vo han de an toan va cho debug */
    while (1) {
    }
}

/* Ham xu ly khi FreeRTOS khong du khong gian RAM (Heap) de tao doi tuong (Task, Queue) */
void vApplicationMallocFailedHook(void) {
    /* Treo he thong */
    while (1) {
    }
}
