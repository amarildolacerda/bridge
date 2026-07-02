#ifndef BRIDGE_CLIENT_H
#define BRIDGE_CLIENT_H

#include <Arduino.h>
#include "config.h"
#include "sensor_registry.h"

bool bridge_client_load_config();
void bridge_client_generate_device_ids();
bool bridge_client_save_config(const char *host, uint16_t port);
const char* bridge_client_get_host();
uint16_t bridge_client_get_port();
bool bridge_client_discover();
void bridge_client_maintain_discovery();
bool bridge_client_register_sensor(virtual_sensor_t *sensor);
bool bridge_client_register_all();
bool bridge_client_send_state(virtual_sensor_t *sensor);
bool bridge_client_send_heartbeat(virtual_sensor_t *sensor);
bool bridge_client_is_discovered();
const char* get_gateway_device_id();
const char* sensor_type_to_string(uint8_t type);
uint8_t* espnow_get_gateway_mac();
unsigned long espnow_get_rx_count();
unsigned long espnow_get_ack_count();
unsigned long espnow_get_crc_errors();
bool espnow_is_pairing();
bool espnow_start_pairing();
void espnow_stop_pairing();
void espnow_handler_loop();

#endif