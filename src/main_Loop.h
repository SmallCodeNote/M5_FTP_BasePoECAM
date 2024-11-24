#ifndef MAIN_LOOP_FUNCTION_H
#define MAIN_LOOP_FUNCTION_H

void TimeUpdateLoop(void *arg);
void TimeServerAccessLoop(void *arg);
void ButtonKeepCountLoop(void *arg);

void ShotLoop(void *arg);
void ShotTask(void *param);
bool ShotTaskRunTrigger(unsigned long currentEpoch);
unsigned long CheckStartOffset();


#endif