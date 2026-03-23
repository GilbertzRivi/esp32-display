#pragma once

typedef void (*callback_handler_t)(const char *args);

void callback_init(void);
void callback_register(const char *ns, callback_handler_t handler);
void callback_dispatch(const char *action);
