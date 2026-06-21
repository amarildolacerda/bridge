#pragma once

#define ESP_LOGI(tag, fmt, ...) fprintf(stdout, "I (%llu) %s: " fmt "\n", (unsigned long long)mock_esp_timer_get_time(), tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stdout, "W (%llu) %s: " fmt "\n", (unsigned long long)mock_esp_timer_get_time(), tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "E (%llu) %s: " fmt "\n", (unsigned long long)mock_esp_timer_get_time(), tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) fprintf(stdout, "D (%llu) %s: " fmt "\n", (unsigned long long)mock_esp_timer_get_time(), tag, ##__VA_ARGS__)

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned long long mock_esp_timer_get_time(void);

#ifdef __cplusplus
}
#endif
