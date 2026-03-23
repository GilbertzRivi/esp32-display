#include "placeholder.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <esp_log.h>

#define TAG "placeholder"

#define MAX_BINDINGS 128
#define NAME_LEN     32

typedef struct {
    char               name[NAME_LEN];
    lv_obj_t          *obj;
    int                field_id;
    widget_set_field_fn fn;
} binding_t;

static binding_t s_bindings[MAX_BINDINGS];
static int       s_count;

void placeholder_init(void)
{
    s_count = 0;
}

void placeholder_bind(const char *name, lv_obj_t *obj, int field_id, widget_set_field_fn fn)
{
    if (s_count >= MAX_BINDINGS) {
        ESP_LOGW(TAG, "binding limit reached");
        return;
    }
    binding_t *b = &s_bindings[s_count++];
    snprintf(b->name, NAME_LEN, "%s", name);
    b->obj      = obj;
    b->field_id = field_id;
    b->fn       = fn;
}

/* Called from WS task — use lv_async_call for thread safety */
typedef struct {
    lv_obj_t          *obj;
    int                field_id;
    widget_set_field_fn fn;
    char               val[32];
} async_update_t;

static void async_set_field(void *param)
{
    async_update_t *u = param;
    u->fn(u->obj, u->field_id, u->val);
    free(u);
}

void placeholder_update(const char *name, const char *val)
{
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_bindings[i].name, name) == 0) {
            async_update_t *u = malloc(sizeof(async_update_t));
            if (!u) continue;
            u->obj      = s_bindings[i].obj;
            u->field_id = s_bindings[i].field_id;
            u->fn       = s_bindings[i].fn;
            snprintf(u->val, sizeof(u->val), "%s", val);
            lv_async_call(async_set_field, u);
        }
    }
}
