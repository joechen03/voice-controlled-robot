#include <Arduino.h>
#include "CommandProcessor.h"

const char *words[] = {
    "forward",
    "backward",
    "left",
    "right",
    "_nonsense",
};

void commandQueueProcessorTask(void *param)
{
    CommandProcessor *commandProcessor = (CommandProcessor *)param;
    while (true)
    {
        uint16_t commandIndex = 0;
        if (xQueueReceive(commandProcessor->m_command_queue_handle, &commandIndex, portMAX_DELAY) == pdTRUE)
        {
            commandProcessor->processCommand(commandIndex);
        }
    }
}

int calcDuty(int ms)
{
    // 50Hz = 20ms period
    return (65536 * ms) / 20000;
}

const int leftForward = 1600;
const int leftBackward = 1400;
const int leftStop = 1500;
const int rightBackward = 1600;
const int rightForward = 1445;
const int rightStop = 1500;

void CommandProcessor::processCommand(uint16_t commandIndex)
{
    digitalWrite(GPIO_NUM_2, HIGH); // Debug LED
    switch (commandIndex)
    {
    case 0: // forward
        digitalWrite(GPIO_NUM_16, LOW);  // 左方向正
        digitalWrite(GPIO_NUM_18, LOW);  // 右方向正
        ledcWrite(0, calcDuty(leftForward));
        ledcWrite(1, calcDuty(rightForward));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        break;
    case 1: // backward
        digitalWrite(GPIO_NUM_16, HIGH); // 左反向
        digitalWrite(GPIO_NUM_18, HIGH); // 右反向
        ledcWrite(0, calcDuty(leftBackward));
        ledcWrite(1, calcDuty(rightBackward));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        break;
    case 2: // left
        digitalWrite(GPIO_NUM_16, HIGH); // 左馬達反轉
        digitalWrite(GPIO_NUM_18, LOW);  // 右馬達正轉
        ledcWrite(0, calcDuty(leftBackward));
        ledcWrite(1, calcDuty(rightForward));
        vTaskDelay(500 / portTICK_PERIOD_MS);
        break;
    case 3: // right
        digitalWrite(GPIO_NUM_16, LOW);  // 左馬達正轉
        digitalWrite(GPIO_NUM_18, HIGH); // 右馬達反轉
        ledcWrite(0, calcDuty(leftForward));
        ledcWrite(1, calcDuty(rightBackward));
        vTaskDelay(500 / portTICK_PERIOD_MS);
        break;
    }
    digitalWrite(GPIO_NUM_2, LOW); // Debug LED off
    ledcWrite(0, calcDuty(leftStop));
    ledcWrite(1, calcDuty(rightStop));
}

CommandProcessor::CommandProcessor()
{
    pinMode(GPIO_NUM_2, OUTPUT);    // Debug LED
    pinMode(GPIO_NUM_16, OUTPUT);   // 左方向控制
    pinMode(GPIO_NUM_18, OUTPUT);   // 右方向控制

    // setup PWM for motors
    ledcSetup(0, 50, 16);
    ledcAttachPin(GPIO_NUM_15, 0); // 左馬達 PWM
    ledcSetup(1, 50, 16);
    ledcAttachPin(GPIO_NUM_17, 1); // 右馬達 PWM

    ledcWrite(0, calcDuty(leftStop));
    ledcWrite(1, calcDuty(rightStop));

    // 建立 queue
    m_command_queue_handle = xQueueCreate(5, sizeof(uint16_t));
    if (!m_command_queue_handle)
    {
        Serial.println("Failed to create command queue");
    }

    // 建立任務
    TaskHandle_t command_queue_task_handle;
    xTaskCreate(commandQueueProcessorTask, "Command Queue Processor", 1024, this, 1, &command_queue_task_handle);
}

void CommandProcessor::queueCommand(uint16_t commandIndex, float best_score)
{
    if (commandIndex != 5 && commandIndex != -1)
    {
        Serial.printf("***** %ld Detected command %s(%f)\n", millis(), words[commandIndex], best_score);
        if (xQueueSendToBack(m_command_queue_handle, &commandIndex, 0) != pdTRUE)
        {
            Serial.println("No more space for command");
        }
    }
}
