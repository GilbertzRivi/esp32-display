#include "screen_mgr.h"
#include <string.h>
#include <esp_log.h>
#define TAG "screen_mgr"

#define MAX_SCREENS 16

typedef struct {
    char id[32];
    lv_obj_t *screen;
} screen_entry_t;

static screen_entry_t screens[MAX_SCREENS];
static int count = 0;

void screen_mgr_init(void)
{
    count = 0;
}

void screen_mgr_add(const char *id, lv_obj_t *screen)
{
    if (count >= MAX_SCREENS) { ESP_LOGW(TAG, "MAX_SCREENS reached"); return; }
    strncpy(screens[count].id, id, sizeof(screens[count].id) - 1);
    screens[count].screen = screen;
    ESP_LOGI(TAG, "registered [%d] '%s' obj=%p", count, id, screen);
    count++;
}

void screen_mgr_show(const char *id)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(screens[i].id, id) == 0) {
            ESP_LOGI(TAG, "show '%s'", id);
            lv_scr_load(screens[i].screen);
            return;
        }
    }
    ESP_LOGE(TAG, "screen '%s' not found (registered: %d)", id, count);
}
