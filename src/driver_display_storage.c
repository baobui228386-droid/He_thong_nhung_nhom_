#include "driver_display_storage.h"
#include <stdio.h>
#include <string.h>

#define LCD_I2C_ADDR    (0x27 << 1) // Doi thanh (0x3F << 1) neu dung chip PCF8574A
#define DS3231_I2C_ADDR (0x68 << 1)
#define I2C_TIMEOUT     50000

/* Ham truyen I2C1 bang thanh ghi co chong treo (Timeout) */
static uint8_t I2C1_Master_Transmit(uint8_t dev_addr, uint8_t *data, uint16_t size) {
    uint32_t timeout = I2C_TIMEOUT;
    while ((I2C1->SR2 & I2C_SR2_BUSY) && --timeout);
    if (!timeout) return 0;

    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_SB) && --timeout);
    if (!timeout) return 0;

    I2C1->DR = dev_addr;
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && --timeout);
    if (!timeout) { I2C1->CR1 |= I2C_CR1_STOP; return 0; }
    (void)I2C1->SR1; (void)I2C1->SR2; // Xoa co ADDR

    for (uint16_t i = 0; i < size; i++) {
        timeout = I2C_TIMEOUT;
        while (!(I2C1->SR1 & I2C_SR1_TXE) && --timeout);
        if (!timeout) return 0;
        I2C1->DR = data[i];
    }

    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_BTF) && --timeout);
    I2C1->CR1 |= I2C_CR1_STOP;
    return 1;
}

static void LCD_SendInternal(uint8_t data, uint8_t flags) {
    uint8_t up = data & 0xF0;
    uint8_t lo = (data << 4) & 0xF0;
    uint8_t arr[4] = {
        up | flags | 0x08 | 0x04, // EN = 1, Backlight = 1
        up | flags | 0x08,        // EN = 0
        lo | flags | 0x08 | 0x04,
        lo | flags | 0x08
    };
    I2C1_Master_Transmit(LCD_I2C_ADDR, arr, 4);
}

static void LCD_SendCmd(uint8_t cmd)  { LCD_SendInternal(cmd, 0); }
static void LCD_SendData(uint8_t data){ LCD_SendInternal(data, 1); }
static void LCD_PrintStr(char *str)   { while (*str) LCD_SendData((uint8_t)(*str++)); }

/* Khoi tao phan cung I2C1, SPI1 va man hinh LCD */
void DisplayStorage_HardwareInit(void) {
    /* 1. Khoi tao I2C1 (Toc do 100kHz, APB1 = 36MHz) */
    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    I2C1->CR2 = 36;    // Tan so APB1 = 36MHz
    I2C1->CCR = 180;   // 36MHz / (2 * 100kHz) = 180
    I2C1->TRISE = 37;  // 36 + 1
    I2C1->CR1 |= I2C_CR1_PE;

    /* 2. Khoi tao SPI1 (Master, toc do vua phai cho the MicroSD) */
    GPIOA->BSRR = GPIO_BSRR_BS4; // Keo chan CS (PA4) len muc cao
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_1 | SPI_CR1_BR_0 | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE;

    /* 3. Khoi tao LCD 16x2 o che do 4-bit */
    vTaskDelay(pdMS_TO_TICKS(50));
    LCD_SendCmd(0x30); vTaskDelay(pdMS_TO_TICKS(5));
    LCD_SendCmd(0x30); vTaskDelay(pdMS_TO_TICKS(1));
    LCD_SendCmd(0x30); vTaskDelay(pdMS_TO_TICKS(10));
    LCD_SendCmd(0x20); vTaskDelay(pdMS_TO_TICKS(10));
    LCD_SendCmd(0x28); // Giao tiep 4-bit, hien thi 2 dong
    LCD_SendCmd(0x0C); // Bat man hinh, tat con tro
    LCD_SendCmd(0x06); // Tu dong dich con tro sang phai
    LCD_SendCmd(0x01); // Xoa man hinh
    vTaskDelay(pdMS_TO_TICKS(5));
}

void LCD_ShowParkingStatus(uint8_t available_slots, uint8_t is_full) {
    char line1[17], line2[17];
    LCD_SendCmd(0x80); // Dau dong 1
    if (is_full) {
        snprintf(line1, sizeof(line1), "PARKING FULL    ");
        snprintf(line2, sizeof(line2), "SPACES: 0       ");
    } else {
        snprintf(line1, sizeof(line1), "SMART PARKING   ");
        snprintf(line2, sizeof(line2), "SPACES: %02d/%02d   ", available_slots, MAX_PARKING_SLOTS);
    }
    LCD_PrintStr(line1);
    LCD_SendCmd(0xC0); // Dau dong 2
    LCD_PrintStr(line2);
}

void Alert_SetStatus(uint8_t is_full) {
    if (is_full) {
        GPIOB->BSRR = GPIO_BSRR_BR0; // Tat LED Xanh (PB0)
        GPIOB->BSRR = GPIO_BSRR_BS1; // Bat LED Do (PB1)
    } else {
        GPIOB->BSRR = GPIO_BSRR_BS0; // Bat LED Xanh (PB0)
        GPIOB->BSRR = GPIO_BSRR_BR1; // Tat LED Do (PB1)
    }
}

void Alert_BeepShort(void) {
    GPIOB->BSRR = GPIO_BSRR_BS10; // Bat coi (PB10)
    vTaskDelay(pdMS_TO_TICKS(400));
    GPIOB->BSRR = GPIO_BSRR_BR10; // Tat coi (PB10)
}

static uint8_t BCD2DEC(uint8_t val) { return ((val >> 4) * 10) + (val & 0x0F); }

static uint8_t I2C1_Read_DS3231(uint8_t *buf) {
    uint32_t timeout = I2C_TIMEOUT;
    while ((I2C1->SR2 & I2C_SR2_BUSY) && --timeout);
    if (!timeout) return 0;

    // 1. Tro vao thanh ghi 0x00 cua DS3231
    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_SB) && --timeout);
    if (!timeout) return 0;

    I2C1->DR = DS3231_I2C_ADDR;
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && --timeout);
    if (!timeout) { I2C1->CR1 |= I2C_CR1_STOP; return 0; }
    (void)I2C1->SR1; (void)I2C1->SR2;

    I2C1->DR = 0x00;
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_BTF) && --timeout);
    if (!timeout) return 0;

    // 2. Khoi tao lai START de doc 7 byte thoi gian
    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_SB) && --timeout);
    if (!timeout) return 0;

    I2C1->DR = DS3231_I2C_ADDR | 0x01;
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && --timeout);
    if (!timeout) { I2C1->CR1 |= I2C_CR1_STOP; return 0; }
    (void)I2C1->SR1; (void)I2C1->SR2;

    I2C1->CR1 |= I2C_CR1_ACK;
    for (int i = 0; i < 7; i++) {
        if (i == 6) {
            I2C1->CR1 &= ~I2C_CR1_ACK; // Byte cuoi khong gui ACK
            I2C1->CR1 |= I2C_CR1_STOP;
        }
        timeout = I2C_TIMEOUT;
        while (!(I2C1->SR1 & I2C_SR1_RXNE) && --timeout);
        if (!timeout) return 0;
        buf[i] = I2C1->DR;
    }
    return 1;
}

void Storage_LogEventCSV(const char* event_name, uint8_t current_cars) {
    uint8_t rtc_buf[7] = {0};
    char csv_line[64];

    if (I2C1_Read_DS3231(rtc_buf)) {
        uint8_t sec  = BCD2DEC(rtc_buf[0] & 0x7F);
        uint8_t min  = BCD2DEC(rtc_buf[1]);
        uint8_t hour = BCD2DEC(rtc_buf[2] & 0x3F);
        uint8_t day  = BCD2DEC(rtc_buf[4]);
        uint8_t mon  = BCD2DEC(rtc_buf[5]);
        uint8_t year = BCD2DEC(rtc_buf[6]);
        snprintf(csv_line, sizeof(csv_line), "20%02d-%02d-%02d %02d:%02d:%02d,%s,%d\r\n",
                 year, mon, day, hour, min, sec, event_name, current_cars);
    } else {
        snprintf(csv_line, sizeof(csv_line), "NO_RTC,%s,%d\r\n", event_name, current_cars);
    }
    /* Chuoi csv_line da san sang de day qua SPI1 / FatFs */
}
