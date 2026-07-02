#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include "espnow_protocol.h"
#include "sensor_registry.h"
#include <stdint.h>

bool espnow_handler_init();
void espnow_handler_loop();
bool espnow_start_pairing();
void espnow_stop_pairing();
bool espnow_is_pairing();
unsigned long espnow_get_rx_count();
unsigned long espnow_get_ack_count();
unsigned long espnow_get_crc_errors();
uint8_t* espnow_get_gateway_mac();

#endif