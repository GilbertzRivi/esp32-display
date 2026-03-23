#pragma once

/* Heap-allocated null-terminated buffer. Caller calls free(). NULL on error. */
char *http_fetch(const char *url);
