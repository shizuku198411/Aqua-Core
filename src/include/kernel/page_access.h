#pragma once

uint32_t sum_enter(void);
void sum_leave(uint32_t saved);
int copyinstr(char *kdst, const char *usrc, size_t max);
int copyin(void *kdst, const void *usrc, size_t n);
int copyout(void *udst, const void *ksrc, size_t n);