#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <esp_task_wdt.h>
#include "I2SMicSampler.h"
#include "config.h"
#include "CommandDetector.h"
#include "CommandProcessor.h"

// I2S config for INMP441 (I2S mic)
i2s_config_t i2sMemsConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
};

// i2s microphone pins
i2s_pin_config_t i2s_mic_pins = {
    .bck_io_num = I2S_MIC_SERIAL_CLOCK,
    .ws_io_num = I2S_MIC_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SERIAL_DATA};

// This task does all the heavy lifting for our application
void applicationTask(void *param)
{
  esp_task_wdt_add(NULL);  // register current task to watchdog

  CommandDetector *commandDetector = static_cast<CommandDetector *>(param);

  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
  while (true)
  {
    // wait for some audio samples to arrive
    uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    if (ulNotificationValue > 0)
    {
      esp_task_wdt_reset();
      commandDetector->run();
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting up");

  // Initialize the WDT for tasks
  esp_task_wdt_init(10, false);
  esp_task_wdt_delete(NULL);  // Remove loopTask from watchdog

  // Create I2S mic sampler
  I2SSampler *i2s_sampler = new I2SMicSampler(i2s_mic_pins, false);

  // the command processor
  CommandProcessor *command_processor = new CommandProcessor();

  // create our application
  CommandDetector *commandDetector = new CommandDetector(i2s_sampler, command_processor);

  // set up the i2s sample writer task
  TaskHandle_t applicationTaskHandle;
  xTaskCreatePinnedToCore(applicationTask, "Command Detect", 8192, commandDetector, 1, &applicationTaskHandle, 1);

  // Start I2S mic
  i2s_sampler->start(I2S_NUM_0, i2sMemsConfig, applicationTaskHandle);
}

void loop()
{
  //vTaskDelay(pdMS_TO_TICKS(1000));
}

//--------------------------------------------------------------------------
// Test for left wheel
// #define IA_L GPIO_NUM_17   // B-IA: Left wheel forward
// #define IB_L GPIO_NUM_18   // B-IB: Left wheel backward

// void setup() {
//   Serial.begin(115200);

//   pinMode(IA_L, OUTPUT);
//   pinMode(IB_L, OUTPUT);

//   // Ensure the motor is stopped initially
//   digitalWrite(IA_L, LOW);
//   digitalWrite(IB_L, LOW);

//   Serial.println("Left wheel test starting");
// }

// void loop() {
//   // Forward
//   Serial.println("LEFT wheel FORWARD (IA=HIGH, IB=LOW)");
//   digitalWrite(IA_L, HIGH);
//   digitalWrite(IB_L, LOW);
//   delay(2000);

//   // Stop
//   Serial.println("LEFT wheel STOP");
//   digitalWrite(IA_L, LOW);
//   digitalWrite(IB_L, LOW);
//   delay(1000);

//   // Backward
//   Serial.println("LEFT wheel BACKWARD (IA=LOW, IB=HIGH)");
//   digitalWrite(IA_L, LOW);
//   digitalWrite(IB_L, HIGH);
//   delay(2000);

//   // Stop
//   Serial.println("LEFT wheel STOP");
//   digitalWrite(IA_L, LOW);
//   digitalWrite(IB_L, LOW);
//   delay(2000);
// }

//--------------------------------------------------------------------------
// Test for right wheel
// #define IA_R GPIO_NUM_15  // A-IA: Right wheel forward
// #define IB_R GPIO_NUM_7   // A-IB: Right wheel backward

// void setup() {
//   Serial.begin(115200);

//   pinMode(IA_R, OUTPUT);
//   pinMode(IB_R, OUTPUT);

//   // Ensure the motor is stopped initially
//   digitalWrite(IA_R, LOW);
//   digitalWrite(IB_R, LOW);

//   Serial.println("Right wheel test starting");
// }

// void loop() {
//   // Forward
//   Serial.println("RIGHT wheel FORWARD (IA=HIGH, IB=LOW)");
//   digitalWrite(IA_R, HIGH);
//   digitalWrite(IB_R, LOW);
//   delay(2000);

//   // Stop
//   Serial.println("RIGHT wheel STOP");
//   digitalWrite(IA_R, LOW);
//   digitalWrite(IB_R, LOW);
//   delay(1000);

//   // Backward
//   Serial.println("RIGHT wheel BACKWARD (IA=LOW, IB=HIGH)");
//   digitalWrite(IA_R, LOW);
//   digitalWrite(IB_R, HIGH);
//   delay(2000);

//   // Stop
//   Serial.println("RIGHT wheel STOP");
//   digitalWrite(IA_R, LOW);
//   digitalWrite(IB_R, LOW);
//   delay(2000);
// }

//--------------------------------------------------------------------------