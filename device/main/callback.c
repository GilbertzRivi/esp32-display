#include "callback.h"
#include "ui/screen_mgr.h"
#include <string.h>
#include <stdio.h>

#define MAX_HANDLERS 16

typedef struct {
    char ns[32];
    callback_handler_t handler;
} handler_entry_t;

static handler_entry_t handlers[MAX_HANDLERS];
static int handler_count = 0;

static void screen_handler(const char *args)
{
    printf("[screen] -> %s\n", args);
    screen_mgr_show(args);
}

void callback_init(void)
{
    handler_count = 0;
    callback_register("screen", screen_handler);
    /* "reply" registered by ws.c in M3 */
}

void callback_register(const char *ns, callback_handler_t handler)
{
    if (handler_count >= MAX_HANDLERS) return;
    strncpy(handlers[handler_count].ns, ns, sizeof(handlers[0].ns) - 1);
    handlers[handler_count].handler = handler;
    handler_count++;
}

void callback_dispatch(const char *action)
{
    if (!action) return;

    char buf[128];
    strncpy(buf, action, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *dot = strchr(buf, '.');
    if (!dot) return;
    *dot = '\0';

    const char *ns   = buf;
    const char *args = dot + 1;

    for (int i = 0; i < handler_count; i++) {
        if (strcmp(handlers[i].ns, ns) == 0) {
            handlers[i].handler(args);
            return;
        }
    }

    printf("[callback] unknown namespace: %s\n", ns);
}
