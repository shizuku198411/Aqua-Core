#pragma once

#define TIMER_INTERVAL 200000  // 20ms


void enable_timer_interrupt(void);
static inline uint64_t rdtime(void);
static inline void wrtimecmp(uint64_t val);
void timer_set_next();
