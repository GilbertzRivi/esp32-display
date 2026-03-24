#pragma once

#include <stdint.h>
#include <stddef.h>

/* Heap-allocated null-terminated buffer. Caller calls free(). NULL on error. */
char *http_fetch(const char *url);

/* Binary fetch.
   Zwraca bufor zaalokowany przez malloc/realloc.
   Caller zwalnia przez free(buf). NULL on error.
   Max 512 KB. */
uint8_t *http_fetch_binary(const char *url, size_t *out_len);