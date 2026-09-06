# local_credentials.h is intentionally ignored by Git. Track its presence so
# Ninja reconfigures when a developer creates or removes the local override.
set(local_credentials "${CMAKE_CURRENT_SOURCE_DIR}/local_credentials.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${local_credentials}")
if(EXISTS "${local_credentials}")
    set_property(SOURCE "main.c" APPEND PROPERTY OBJECT_DEPENDS "${local_credentials}")
endif()

idf_component_register(
    SRCS
        "main.c"
        "app_model.c"
        "board_backlight.c"
        "board_button.c"
        "board_display.c"
        "board_touch.c"
        "config_store.c"
        "diagnostics.c"
        "ui_runtime.c"
        "ui_shell.c"
        "ui_control_center.c"
        "network_manager.c"
        "provisioning_portal.c"
        "time_service.c"
        "backend_probe.c"
        "card1.c"
        "pc_monitor.c"
        "fonts/font_cn_16.c"
    INCLUDE_DIRS "."
    REQUIRES
        app_update
        driver
        esp_event
        esp_http_client
        esp_http_server
        esp_lcd
        esp_driver_ledc
        esp_netif
        esp_wifi
        lwip
        nvs_flash
        spi_flash
        lvgl
)
