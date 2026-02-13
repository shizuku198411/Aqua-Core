#pragma once

#include "stdtypes.h"

// functions
void putchar(char ch);
long getchar(void);
void exit(void);

void printf(const char *fmt, ...);
void *memset(void *buf, char c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
char *strcpy(char *dst, const char *src);
char *strcpy_s(char *dst, size_t n, const char *src);
int strcmp(const char *s1, const char *s2);
int unix_time_to_utc_str(uint64_t unix_sec, char *out, size_t out_size);
