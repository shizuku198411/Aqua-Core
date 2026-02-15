#pragma once

#include "stdtypes.h"

#define NET_ERR_INVAL          (-1)
#define NET_ERR_NOT_READY      (-2)
#define NET_ERR_TOO_LARGE      (-3)
#define NET_ERR_NO_DEVICE      (-4)
#define NET_ERR_TIMEOUT        (-5)
#define NET_ERR_BUILD_FAILED   (-6)
#define NET_ERR_PING_TIMEOUT   (-7)
#define NET_ERR_ARP_TIMEOUT    (-8)

int net_init(void);
int net_tx_frame(const void *buf, size_t len);
int net_get_mac(uint8_t out_mac[6]);
int net_ping_send_once(uint32_t dst_ip, uint16_t id, uint16_t seq);
int net_rx_try_dequeue(const uint8_t **frame_out, size_t *len_out);
