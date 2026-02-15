#include "commonlibs.h"
#include "user_syscall.h"
#include "net.h"
#include "rtc.h"

#define PING_ID        0x1234u
#define PING_SEQ_START 1u
#define PING_DATA_LEN  13u  // strlen("aquacore-ping")
#define PING_DEFAULT_COUNT 5u
#define PING_INTERVAL_MS   1000u

static int parse_octet(const char *s, int *consumed, uint32_t *out) {
    if (!s || !consumed || !out) {
        return -1;
    }
    if (s[0] < '0' || s[0] > '9') {
        return -1;
    }

    uint32_t v = 0;
    int i = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        v = v * 10u + (uint32_t) (s[i] - '0');
        if (v > 255u) {
            return -1;
        }
        i++;
    }
    *consumed = i;
    *out = v;
    return 0;
}

static int parse_ipv4(const char *s, uint32_t *out_ip) {
    if (!s || !out_ip) {
        return -1;
    }

    uint32_t b[4] = {0, 0, 0, 0};
    int pos = 0;
    for (int i = 0; i < 4; i++) {
        int n = 0;
        if (parse_octet(&s[pos], &n, &b[i]) < 0) {
            return -1;
        }
        pos += n;
        if (i < 3) {
            if (s[pos] != '.') {
                return -1;
            }
            pos++;
        }
    }
    if (s[pos] != '\0') {
        return -1;
    }

    *out_ip = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
    return 0;
}

static uint32_t elapsed_ms(const struct time_spec *t0, const struct time_spec *t1) {
    uint32_t sec0 = t0->sec_lo;
    uint32_t sec1 = t1->sec_lo;
    uint32_t nsec0 = t0->nsec;
    uint32_t nsec1 = t1->nsec;
    uint32_t dsec = 0;
    uint32_t dnsec = 0;

    if (sec1 >= sec0) {
        dsec = sec1 - sec0;
    }
    if (nsec1 >= nsec0) {
        dnsec = nsec1 - nsec0;
    } else if (dsec > 0) {
        dsec--;
        dnsec = (1000000000u - nsec0) + nsec1;
    }
    return dsec * 1000u + (dnsec / 1000000u);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: ping <ipv4>\n");
        return -1;
    }

    uint32_t dst_ip = 0;
    if (parse_ipv4(argv[1], &dst_ip) < 0) {
        printf("invalid ipv4: %s\n", argv[1]);
        return -1;
    }

    printf("PING %s: %d data bytes\n", argv[1], (int) PING_DATA_LEN);

    int transmitted = 0;
    int received = 0;
    uint32_t rtt_min = 0;
    uint32_t rtt_max = 0;
    uint32_t rtt_sum = 0;

    for (uint32_t i = 0; i < PING_DEFAULT_COUNT; i++) {
        uint16_t seq = (uint16_t) (PING_SEQ_START + i);
        struct time_spec t0, t1;

        if (gettime(&t0) < 0) {
            printf("gettime failed\n");
            return -1;
        }

        transmitted++;
        int ret = ping_tx(dst_ip, PING_ID, seq);

        if (gettime(&t1) < 0) {
            printf("gettime failed\n");
            return -1;
        }

        uint32_t rtt_ms = elapsed_ms(&t0, &t1);

        if (ret == 0) {
            received++;
            if (received == 1) {
                rtt_min = rtt_ms;
                rtt_max = rtt_ms;
            } else {
                if (rtt_ms < rtt_min) {
                    rtt_min = rtt_ms;
                }
                if (rtt_ms > rtt_max) {
                    rtt_max = rtt_ms;
                }
            }
            rtt_sum += rtt_ms;
            printf("%d bytes from %s: icmp_seq=%d time=%d ms\n",
                   (int) PING_DATA_LEN, argv[1], (int) seq, (int) rtt_ms);
        } else if (ret == NET_ERR_PING_TIMEOUT) {
            printf("Request timeout for icmp_seq %d\n", (int) seq);
        } else {
            printf("ping failed (%d)\n", ret);
        }

        if (i + 1u < PING_DEFAULT_COUNT) {
            sleep(PING_INTERVAL_MS);
        }
    }

    int loss = 100 - (received * 100 / transmitted);
    printf("\n--- %s ping statistics ---\n", argv[1]);
    printf("%d packets transmitted, %d packets received, %d%% packet loss\n",
           transmitted, received, loss);
    if (received > 0) {
        uint32_t rtt_avg = rtt_sum / (uint32_t) received;
        printf("round-trip min/avg/max = %d/%d/%d ms\n",
               (int) rtt_min, (int) rtt_avg, (int) rtt_max);
    }

    return (received > 0) ? 0 : -1;
}
