#pragma once
#include "stdtypes.h"

#define GOLDFISH_RTC_BASE 0x00101000u
#define RTC_MMIO_BASE 0x00101000u
#define RTC_MMIO_END  0x00102000u

#define RTC_TIME_LOW  0x00
#define RTC_TIME_HIGH 0x04

int rtc_init(void);
uint64_t rtc_now_ns(void);
uint32_t rtc_now_sec(void);

struct time_spec {
    uint32_t sec_lo;
    uint32_t sec_hi;
    uint32_t nsec;
};
