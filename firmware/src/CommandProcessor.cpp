#include <Arduino.h>
#include "CommandProcessor.h"

/* ---------- Keyword List ---------- */
static const char *words[] = {
    "forward",
    "backward",
    "left",
    "right",
    "_nonsense",
};

/* ---------- L9110 Pin & LEDC Channel Mapping ---------- */
constexpr uint8_t IA_L_PIN = GPIO_NUM_15;  // Left motor IA
constexpr uint8_t IB_L_PIN = GPIO_NUM_7;   // Left motor IB
constexpr uint8_t IA_R_PIN = GPIO_NUM_17;  // Right motor IA
constexpr uint8_t IB_R_PIN = GPIO_NUM_18;  // Right motor IB

constexpr uint8_t CH_IA_L = 0;
constexpr uint8_t CH_IB_L = 1;
constexpr uint8_t CH_IA_R = 2;
constexpr uint8_t CH_IB_R = 3;

/* ---------- Helper: Drive a Motor via IA/IB ---------- */
inline void driveMotor(int16_t speed,
                       uint8_t chIA, uint8_t chIB,
                       uint8_t pinIA, uint8_t pinIB)
{
    speed = constrain(speed, -255, 255);

    if (speed > 0) {
        Serial.printf("[DEBUG] Motor forward | Channel %d = %d, Channel %d = 0\n",
                      chIA, speed, chIB);
        ledcAttachPin(pinIA, chIA);
        ledcAttachPin(pinIB, chIB);
        ledcWrite(chIA, speed);
        ledcWrite(chIB, 0);
    } else if (speed < 0) {
        Serial.printf("[DEBUG] Motor backward | Channel %d = 0, Channel %d = %d\n",
                      chIA, chIB, -speed);
        ledcAttachPin(pinIA, chIA);
        ledcAttachPin(pinIB, chIB);
        ledcWrite(chIA, 0);
        ledcWrite(chIB, -speed);
    } else {
        Serial.printf("[DEBUG] Motor stop | %d and %d set to LOW via digitalWrite\n", pinIA, pinIB);
        ledcDetachPin(pinIA);
        ledcDetachPin(pinIB);
        pinMode(pinIA, OUTPUT);
        pinMode(pinIB, OUTPUT);
        digitalWrite(pinIA, LOW);
        digitalWrite(pinIB, LOW);
        Serial.printf("[VERIFY] digitalWrite(%d, LOW), digitalWrite(%d, LOW)\n", pinIA, pinIB);
    }
}

/* ---------- Task: Consume Commands from Queue ---------- */
void commandQueueProcessorTask(void *param)
{
    auto *commandProcessor = static_cast<CommandProcessor *>(param);

    while (true) {
        uint16_t cmdIndex = 0;
        if (xQueueReceive(commandProcessor->m_command_queue_handle,
                          &cmdIndex, portMAX_DELAY) == pdTRUE)
        {
            commandProcessor->processCommand(cmdIndex);
        }
    }
}

/* ---------- Speed Profiles ---------- */
constexpr int16_t SPEED_FWD   = 200;
constexpr int16_t SPEED_BACK  = -200;
constexpr int16_t SPEED_TURN  = 200;
constexpr TickType_t FWD_BACK_TIME = 1000 / portTICK_PERIOD_MS;
constexpr TickType_t TURN_TIME     =  500 / portTICK_PERIOD_MS;

/* ---------- Command Handler ---------- */
void CommandProcessor::processCommand(uint16_t commandIndex)
{
    Serial.printf("[INFO][%lu ms] Handling command index: %d\n", millis(), commandIndex);

    switch (commandIndex) {
    case 0:  // forward
        Serial.println("[DEBUG] Command: FORWARD");
        driveMotor(SPEED_FWD,  CH_IA_L, CH_IB_L, IA_L_PIN, IB_L_PIN);
        driveMotor(SPEED_FWD,  CH_IA_R, CH_IB_R, IA_R_PIN, IB_R_PIN);
        Serial.printf("[DEBUG] Left Motor Speed: %d, Right Motor Speed: %d\n", SPEED_FWD, SPEED_FWD);
        vTaskDelay(FWD_BACK_TIME);
        break;

    case 1:  // backward
        Serial.println("[DEBUG] Command: BACKWARD");
        driveMotor(SPEED_BACK, CH_IA_L, CH_IB_L, IA_L_PIN, IB_L_PIN);
        driveMotor(SPEED_BACK, CH_IA_R, CH_IB_R, IA_R_PIN, IB_R_PIN);
        Serial.printf("[DEBUG] Left Motor Speed: %d, Right Motor Speed: %d\n", SPEED_BACK, SPEED_BACK);
        vTaskDelay(FWD_BACK_TIME);
        break;

    case 2:  // left
        Serial.println("[DEBUG] Command: LEFT");
        driveMotor(SPEED_TURN,  CH_IA_L, CH_IB_L, IA_L_PIN, IB_L_PIN);
        driveMotor(SPEED_BACK, CH_IA_R, CH_IB_R, IA_R_PIN, IB_R_PIN);
        Serial.printf("[DEBUG] Left Motor Speed: %d, Right Motor Speed: %d\n", SPEED_TURN, SPEED_BACK);
        vTaskDelay(TURN_TIME);
        break;

    case 3:  // right
        Serial.println("[DEBUG] Command: RIGHT");
        driveMotor(SPEED_BACK, CH_IA_L, CH_IB_L, IA_L_PIN, IB_L_PIN);
        driveMotor(SPEED_TURN, CH_IA_R, CH_IB_R, IA_R_PIN, IB_R_PIN);
        Serial.printf("[DEBUG] Left Motor Speed: %d, Right Motor Speed: %d\n", SPEED_BACK, SPEED_TURN);
        vTaskDelay(TURN_TIME);
        break;

    default:
        return;
    }

    // Stop motors after movement
    driveMotor(0, CH_IA_L, CH_IB_L, IA_L_PIN, IB_L_PIN);
    driveMotor(0, CH_IA_R, CH_IB_R, IA_R_PIN, IB_R_PIN);
    Serial.println("[DEBUG] Motors stopped\n");
}

/* ---------- Constructor ---------- */
CommandProcessor::CommandProcessor()
{
    constexpr uint32_t PWM_FREQ = 20000;
    constexpr uint8_t  PWM_RES  = 8;

    ledcSetup(CH_IA_L, PWM_FREQ, PWM_RES);
    ledcSetup(CH_IB_L, PWM_FREQ, PWM_RES);
    ledcSetup(CH_IA_R, PWM_FREQ, PWM_RES);
    ledcSetup(CH_IB_R, PWM_FREQ, PWM_RES);

    // Initially stop motors
    driveMotor(0, CH_IA_L, CH_IB_L, IA_L_PIN, IB_L_PIN);
    driveMotor(0, CH_IA_R, CH_IB_R, IA_R_PIN, IB_R_PIN);

    m_command_queue_handle = xQueueCreate(5, sizeof(uint16_t));
    if (!m_command_queue_handle) {
        Serial.println("Failed to create command queue");
    }

    TaskHandle_t taskHandle;
    xTaskCreate(commandQueueProcessorTask, "CmdQueueProc",
                2048, this, 1, &taskHandle);
}

/* ---------- Enqueue Command ---------- */
void CommandProcessor::queueCommand(uint16_t commandIndex, float best_score)
{
    if (commandIndex < 4) {
        Serial.printf("***** %ld ms: Detected command %s (%.2f)\n",
                      millis(), words[commandIndex], best_score);

        if (xQueueSendToBack(m_command_queue_handle, &commandIndex, 0) != pdTRUE) {
            Serial.println("Command queue full");
        }
    }
}
