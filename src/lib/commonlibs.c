#include "stdtypes.h"
#include "commonlibs.h"

void printf(const char *fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case '\0':
                    putchar('%');
                    goto end;
                case '%':
                    putchar('%');
                    break;
                // print a string
                case 's': {
                    const char *s = va_arg(vargs, const char *);
                    while (*s) {
                        putchar(*s);
                        s++;
                    }
                    break;
                }
                // print an integer in decimal
                case 'd': {
                    int value = va_arg(vargs, int);
                    unsigned magnitude = value;
                    if (value < 0) {
                        putchar('-');
                        magnitude = -magnitude;
                    }
                    unsigned divisor = 1;
                    while (magnitude / divisor > 9) {
                        divisor *= 10;
                    }
                    while (divisor > 0) {
                        putchar('0' + magnitude / divisor);
                        magnitude %= divisor;
                        divisor /= 10;
                    }
                    break;
                }
                // print an integer in hexadecimal
                case 'x': {
                    unsigned value = va_arg(vargs, unsigned);
                    for (int i = 7; i >= 0; i--) {
                        unsigned nibble = (value >> (i * 4)) & 0xf;
                        putchar("0123456789abcdef"[nibble]);
                    }
                }
            }
        } else {
            putchar(*fmt);
        }

        fmt++;
    }

end:
    va_end(vargs);
}


void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *) dst;
    const uint8_t *s = (const uint8_t *) src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}


void *memset(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    while (n--) {
        *p++ = c;
    }
    return buf;
}


char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while (*src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dst;
}


char *strcpy_s(char *dst, size_t n, const char *src) {
    char *d = dst;
    while (n--) {
        *d++ = *src++;
    }
    *d = '\0';
    return dst;
}


int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2)
            break;
        s1++;
        s2++;
    }

    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static int is_leap_year(int y) {
    if ((y % 400) == 0) return 1;
    if ((y % 100) == 0) return 0;
    return (y % 4) == 0;
}

static int append_char(char *out, size_t out_size, size_t *pos, char c) {
    if (*pos + 1 >= out_size) return -1;
    out[*pos] = c;
    (*pos)++;
    out[*pos] = '\0';
    return 0;
}

static int append_str(char *out, size_t out_size, size_t *pos, const char *s) {
    while (*s) {
        if (append_char(out, out_size, pos, *s++) < 0) return -1;
    }
    return 0;
}

static int append_u32(char *out, size_t out_size, size_t *pos, uint32_t v) {
    char tmp[10];
    int n = 0;
    if (v == 0) return append_char(out, out_size, pos, '0');
    while (v > 0 && n < 10) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int i = n - 1; i >= 0; i--) {
        if (append_char(out, out_size, pos, tmp[i]) < 0) return -1;
    }
    return 0;
}

static int append_2d(char *out, size_t out_size, size_t *pos, uint32_t v) {
    if (append_char(out, out_size, pos, (char)('0' + ((v / 10) % 10))) < 0) return -1;
    if (append_char(out, out_size, pos, (char)('0' + (v % 10))) < 0) return -1;
    return 0;
}

static uint64_t udiv64_32_full(uint64_t n, uint32_t d, uint32_t *rem_out) {
    uint64_t q = 0;
    uint64_t rem = 0;

    for (int i = 63; i >= 0; i--) {
        rem = (rem << 1) | ((n >> i) & 1ull);
        if (rem >= d) {
            rem -= d;
            q |= (1ull << i);
        }
    }

    if (rem_out) {
        *rem_out = (uint32_t) rem;
    }
    return q;
}

int unix_time_to_utc_str(uint64_t unix_sec, char *out, size_t out_size) {
    static const char *wday_name[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char *mon_name[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    static const uint32_t mon_days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

    if (!out || out_size == 0) return -1;
    out[0] = '\0';

    uint32_t sod = 0;
    uint64_t days = udiv64_32_full(unix_sec, 86400u, &sod);
    uint32_t hour = sod / 3600u;
    uint32_t min  = (sod % 3600u) / 60u;
    uint32_t sec  = sod % 60u;

    uint32_t wday = 0;
    (void) udiv64_32_full(days + 4ull, 7u, &wday);

    int year = 1970;
    uint64_t d = days;
    while (1) {
        uint64_t diy = is_leap_year(year) ? 366ull : 365ull;
        if (d < diy) break;
        d -= diy;
        year++;
    }

    int month = 0;
    while (month < 12) {
        uint32_t dim = mon_days[month];
        if (month == 1 && is_leap_year(year)) dim = 29;
        if (d < dim) break;
        d -= dim;
        month++;
    }
    uint32_t mday = (uint32_t) (d + 1u);

    size_t p = 0;
    if (append_str(out, out_size, &p, wday_name[wday]) < 0) return -1;
    if (append_char(out, out_size, &p, ' ') < 0) return -1;
    if (append_str(out, out_size, &p, mon_name[month]) < 0) return -1;
    if (append_char(out, out_size, &p, ' ') < 0) return -1;
    if (append_u32(out, out_size, &p, mday) < 0) return -1;
    if (append_char(out, out_size, &p, ' ') < 0) return -1;
    if (append_2d(out, out_size, &p, hour) < 0) return -1;
    if (append_char(out, out_size, &p, ':') < 0) return -1;
    if (append_2d(out, out_size, &p, min) < 0) return -1;
    if (append_char(out, out_size, &p, ':') < 0) return -1;
    if (append_2d(out, out_size, &p, sec) < 0) return -1;
    if (append_str(out, out_size, &p, " UTC ") < 0) return -1;
    if (append_u32(out, out_size, &p, (uint32_t)year) < 0) return -1;

    return 0;
}
