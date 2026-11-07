#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#define SCH_MAX_TASKS       (16U)
#define SCH_INVALID_TASK_ID (0U)

typedef struct {
    void (*pTask)(void);
    uint32_t Delay, Period;
    uint8_t RunMe;
    uint32_t TaskID;
    uint32_t DueTick;
} sTask;

void SCH_Init(void);
uint32_t SCH_Add_Task(void (*pFunc)(void), uint32_t delay_ticks,
                      uint32_t period_ticks);
uint8_t SCH_Delete_Task(uint32_t taskID);
void SCH_Update(void);
void SCH_Dispatch_Tasks(void);
uint32_t SCH_GetTick(void);

#endif
