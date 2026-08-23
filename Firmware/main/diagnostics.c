#include "diagnostics.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"

#include "ui_runtime.h"

void diagnostics_log_baseline(const char *label)
{
    const size_t free_heap = esp_get_free_heap_size();
    const size_t minimum_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    const size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    ESP_LOGI("baseline", "[%s] free_heap=%u minimum_free_heap=%u largest_free_block=%u fps=%u buffer_lines=%u buffer_bytes=%u",
             label, (unsigned)free_heap, (unsigned)minimum_free_heap, (unsigned)largest_free_block,
             (unsigned)ui_runtime_fps(), (unsigned)ui_runtime_buffer_lines(), (unsigned)ui_runtime_buffer_bytes());
}
