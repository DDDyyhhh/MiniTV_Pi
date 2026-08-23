#pragma once

#include "esp_err.h"

#include "config_store.h"

esp_err_t network_manager_init(void);
esp_err_t network_manager_connect(const app_config_t *config);
void network_manager_start_provisioning(void);
