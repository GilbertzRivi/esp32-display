#include "placeholder.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <esp_log.h>
#include <lvgl.h>

#define TAG "placeholder"

#define MAX_BINDINGS 128
#define NAME_LEN     32

typedef struct {
    char                name[NAME_LEN];
    lv_obj_t           *obj;
    int                 field_id;
    widget_set_field_fn fn;
    bool                active;
    uint32_t            generation;
} binding_t;

static binding_t s_bindings[MAX_BINDINGS];
static int       s_count;

static void binding_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    if (!obj) return;

    for (int i = 0; i < s_count; i++) {
        binding_t *b = &s_bindings[i];
        if (b->active && b->obj == obj) {
            b->active = false;
            b->obj = NULL;
            b->fn = NULL;
            b->field_id = -1;
            b->name[0] = '\0';
            b->generation++;
        }
    }
}

void placeholder_init(void)
{
    memset(s_bindings, 0, sizeof(s_bindings));
    s_count = 0;
}

void placeholder_bind(const char *name, lv_obj_t *obj, int field_id, widget_set_field_fn fn)
{
    if (!name || !name[0] || !obj || !fn) {
        ESP_LOGW(TAG, "invalid bind args");
        return;
    }

    binding_t *b = NULL;

    for (int i = 0; i < s_count; i++) {
        if (!s_bindings[i].active) {
            b = &s_bindings[i];
            break;
        }
    }

    if (!b) {
        if (s_count >= MAX_BINDINGS) {
            ESP_LOGW(TAG, "binding limit reached");
            return;
        }
        b = &s_bindings[s_count++];
    }

    memset(b->name, 0, sizeof(b->name));
    snprintf(b->name, sizeof(b->name), "%s", name);
    b->obj        = obj;
    b->field_id   = field_id;
    b->fn         = fn;
    b->active     = true;
    b->generation++;

    lv_obj_add_event_cb(obj, binding_delete_cb, LV_EVENT_DELETE, NULL);
}

typedef struct {
    binding_t *binding;
    uint32_t   generation;
    char       val[32];
} async_update_t;

static void async_set_field(void *param)
{
    async_update_t *u = param;
    if (!u) return;

    binding_t *b = u->binding;
    if (!b) {
        free(u);
        return;
    }

    if (!b->active || b->generation != u->generation) {
        free(u);
        return;
    }

    if (!b->obj || !b->fn) {
        free(u);
        return;
    }

    if (!lv_obj_is_valid(b->obj)) {
        b->active = false;
        b->obj = NULL;
        b->fn = NULL;
        b->field_id = -1;
        b->name[0] = '\0';
        b->generation++;
        free(u);
        return;
    }

    b->fn(b->obj, b->field_id, u->val);
    free(u);
}

void placeholder_update(const char *name, const char *val)
{
    if (!name || !name[0]) return;
    if (!val) val = "";

    for (int i = 0; i < s_count; i++) {
        binding_t *b = &s_bindings[i];

        if (!b->active) continue;
        if (!b->fn) continue;
        if (strcmp(b->name, name) != 0) continue;

        async_update_t *u = malloc(sizeof(async_update_t));
        if (!u) {
            ESP_LOGW(TAG, "malloc failed for async update");
            continue;
        }

        u->binding    = b;
        u->generation = b->generation;
        snprintf(u->val, sizeof(u->val), "%s", val);

        lv_async_call(async_set_field, u);
    }
}