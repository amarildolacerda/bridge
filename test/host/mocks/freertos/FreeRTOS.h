#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t TickType_t;

#define portMAX_DELAY ((TickType_t)0xFFFFFFFFUL)
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define pdPASS 1
#define pdFAIL 0

typedef void *TaskHandle_t;
typedef void *SemaphoreHandle_t;

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
void vSemaphoreDelete(SemaphoreHandle_t xSemaphore);

#ifdef __cplusplus
}
#endif
