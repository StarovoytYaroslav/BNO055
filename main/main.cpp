#include <stdio.h>
#include <inttypes.h>
#include "main.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "Arduino.h"
#include "SparkFun_BNO080_Arduino_Library.h"
#include "Adafruit_BNO055.h"
#include "gpios.h"
#include "accelerometers.h"

QueueHandle_t sensor_queue;
QueueHandle_t command_queue;
QueueHandle_t log_queue;

typedef struct
{
    int sensor_id;
    float ax, ay, az, gx, gy, gz;
    uint32_t timestamp_ms;
} sensor_data_t;

typedef struct
{
    int target_sensor; // 0 = both, 1 = BNO055, 2 = BNO080
    char cmd[32];      // e.g., "calibrate", "tare"
} sensor_command_t;

typedef struct {
    char msg[128];  // or 128, depending on max length you expect
    uint32_t timestamp_ms;
} log_message_t;

BNO055Wrapper *bno055;
BNO080Wrapper *bno080;
ADXL345Wrapper *adxl345;
MPU6050Wrapper *mpu6050;

void bno055_task(void *pvParameters)
{
    float* accelData;
    while (1)
    {
        accelData = bno055->read();
        sensor_data_t data = {
            .sensor_id = 1,
            .ax = accelData[0],
            .ay = accelData[1],
            .az = accelData[2],
            .gx = accelData[3],
            .gy = accelData[4],
            .gz = accelData[5],
            .timestamp_ms = esp_log_timestamp()};
        xQueueSend(sensor_queue, &data, 0);

        sensor_command_t cmd;
        if (xQueueReceive(command_queue, &cmd, 0))
        {
            if (cmd.target_sensor == 1 || cmd.target_sensor == 0)
            {
                if (strcmp(cmd.cmd, "calibrate") == 0)
                {
                    // Serial.println("calibration command");
                    // bno055_calibrate();  // your implementation
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // adjust per your sensor rate
    }
}

void bno080_task(void *pvParameters)
{
    float* accelData;
    while (1)
    {
        accelData = bno080->read();
        if(accelData == nullptr)
            continue;
        sensor_data_t data = {
            .sensor_id = 2,
            .ax = accelData[0],
            .ay = accelData[1],
            .az = accelData[2],
            .gx = accelData[3],
            .gy = accelData[4],
            .gz = accelData[5],
            .timestamp_ms = esp_log_timestamp()};
        xQueueSend(sensor_queue, &data, 0);

        sensor_command_t cmd;
        if (xQueueReceive(command_queue, &cmd, 0))
        {
            if (cmd.target_sensor == 2 || cmd.target_sensor == 0)
            {
                if (strcmp(cmd.cmd, "calibrate") == 0)
                {
                    log_message_t log;
                    snprintf(log.msg, sizeof(log.msg), "BNO080 calibration command");
                    log.timestamp_ms = esp_log_timestamp();

                    xQueueSend(log_queue, &log, 0);
                    // Serial.println("calibration command");
                    // bno055_calibrate();  // your implementation
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1)); // adjust per your sensor rate
    }
}

void adxl345_task(void *pvParameters)
{
    float* accelData;
    while (1)
    {
        accelData = adxl345->read();
        sensor_data_t data = {
            .sensor_id = 3,
            .ax = accelData[0],
            .ay = accelData[1],
            .az = accelData[2],
            .gx = accelData[3],
            .gy = accelData[4],
            .gz = accelData[5],
            .timestamp_ms = esp_log_timestamp()};
        xQueueSend(sensor_queue, &data, 0);

        sensor_command_t cmd;
        if (xQueueReceive(command_queue, &cmd, 0))
        {
            if (cmd.target_sensor == 3 || cmd.target_sensor == 0)
            {
                if (strcmp(cmd.cmd, "calibrate") == 0)
                {
                    log_message_t log;
                    snprintf(log.msg, sizeof(log.msg), "ADXL345 calibration command");
                    log.timestamp_ms = esp_log_timestamp();

                    xQueueSend(log_queue, &log, 0);
                    // Serial.println("calibration command");
                    // bno055_calibrate();  // your implementation
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // adjust per your sensor rate
    }
}

void mpu6050_task(void *pvParameters)
{
    float* accelData;
    while (1)
    {
        accelData = mpu6050->read();
        sensor_data_t data = {
            .sensor_id = 4,
            .ax = accelData[0],
            .ay = accelData[1],
            .az = accelData[2],
            .gx = accelData[3],
            .gy = accelData[4],
            .gz = accelData[5],
            .timestamp_ms = esp_log_timestamp()};
        xQueueSend(sensor_queue, &data, 0);

        sensor_command_t cmd;
        if (xQueueReceive(command_queue, &cmd, 0))
        {
            if (cmd.target_sensor == 4 || cmd.target_sensor == 0)
            {
                if (strcmp(cmd.cmd, "calibrate") == 0)
                {
                    log_message_t log;
                    snprintf(log.msg, sizeof(log.msg), "ADXL345 calibration command");
                    log.timestamp_ms = esp_log_timestamp();

                    xQueueSend(log_queue, &log, 0);
                    // Serial.println("calibration command");
                    // bno055_calibrate();  // your implementation
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // adjust per your sensor rate
    }
}

bool get_line_from_uart(char *line, size_t max_len, uint32_t timeout_ms = 100)
{
    size_t len = 0;
    uint32_t start = millis();

    while ((millis() - start) < timeout_ms && len < (max_len - 1))
    {
        while (Serial.available())
        {
            char c = Serial.read();

            if (c == '\n')
            {
                line[len] = '\0';
                return true;
            }
            else if (c == '\r')
            {
                continue; // skip carriage return
            }

            line[len++] = c;
            if (len >= max_len - 1)
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(5)); // allow other tasks to run & prevent WDT
    }

    // If timeout but something was read
    if (len > 0)
    {
        line[len] = '\0';
        return true;
    }

    return false; // nothing received in timeout window
}

void uart_rx_task(void *pvParams)
{
    char line[64];

    while (1)
    {
        if (get_line_from_uart(line, sizeof(line)))
        {
            log_message_t log;

            sensor_command_t cmd = {0};

            if (strstr(line, "bno055"))
                cmd.target_sensor = 1;
            else if (strstr(line, "bno080"))
                cmd.target_sensor = 2;
            else
                cmd.target_sensor = 0;

            if (strstr(line, "calibrate"))
                strcpy(cmd.cmd, "calibrate");
            else if (strstr(line, "tare"))
                strcpy(cmd.cmd, "tare");
            else
                continue;
            snprintf(log.msg, sizeof(log.msg),"%s and processed to: %d: %s", line, cmd.target_sensor, cmd.cmd);
            log.timestamp_ms = esp_log_timestamp();
            xQueueSend(log_queue, &log, 0);
            xQueueSend(command_queue, &cmd, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // small delay to prevent CPU hogging
    }
}

void uart_tx_task(void *pvParameters)
{
    sensor_data_t data;
    log_message_t log;
    Serial.println("tx_task");

    while (1)
    {

        if (xQueueReceive(sensor_queue, &data, 0))
        {
            Serial.printf("ID:%d A:%.2f,%.2f,%.2f G:%.2f,%.2f,%.2f T:%lu\n",
                          data.sensor_id, data.ax, data.ay, data.az, data.gx, data.gy, data.gz, data.timestamp_ms);
        }
        // if(xQueueReceive(log_queue, &log, 0))
        // {
        //         Serial.printf("[%lu ms] %s\n", log.timestamp_ms, log.msg);
        // }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void log_output_task(void *pvParams) {
    log_message_t log;
    while (1) {
        if (xQueueReceive(log_queue, &log, portMAX_DELAY)) {
            Serial.printf("[%lu ms] %s\n", log.timestamp_ms, log.msg);
        }
    }
}

extern "C" void app_main(void)
{
    configure_gpio();

    initArduino();

    Serial.begin(115200);

    Serial.println("Here we start");

    // bno055 = new BNO055Wrapper();
    // bno080 = new BNO080Wrapper();
    // adxl345 = new ADXL345Wrapper();
    mpu6050 = new MPU6050Wrapper();

    // Init sensor queue and command queue
    sensor_queue = xQueueCreate(10, sizeof(sensor_data_t));
    command_queue = xQueueCreate(10, sizeof(sensor_command_t));
    log_queue = xQueueCreate(10, sizeof(log_message_t));

    // Create sensor tasks
    // xTaskCreate(bno055_task, "bno055_task", 4096, NULL, 5, NULL);
    // xTaskCreate(bno080_task, "bno080_task", 4096, NULL, 5, NULL);
    // xTaskCreate(adxl345_task, "adxl345_task", 4096, NULL, 5, NULL);
    xTaskCreate(mpu6050_task, "mpu6050_task", 4096, NULL, 5, NULL);

    // UART task to read input commands (USB CDC or hardware UART)
    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 4, NULL);

    // Task to print output
    xTaskCreate(uart_tx_task, "uart_tx_task", 4096, NULL, 3, NULL);

    // xTaskCreate(log_output_task, "log_output_task", 4096, NULL, 3, NULL);
}
