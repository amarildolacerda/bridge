#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t esp_timer_get_time(void);

void mock_esp_timer_set_time(int64_t t);
void mock_esp_timer_advance(int64_t us);

#ifdef __cplusplus
}
#endif
