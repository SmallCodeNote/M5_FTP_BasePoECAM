#ifndef MAIN_LOOP_FUNCTION_H
#define MAIN_LOOP_FUNCTION_H

void TimeUpdateLoop(void *arg);
void TimeServerAccessLoop(void *arg);

void ButtonKeepCountLoop(void *arg);

void SensorShotLoop(void *arg);
void SensorShotTask(void *param);
bool SensorShotTaskRunTrigger(unsigned long currentEpoch);
unsigned long SensorShotStartOffset();

typedef struct
{
  unsigned long currentEpoch;
} SensorShotTaskParams;

void unit_flash_set_brightness(uint8_t brightness);

#endif