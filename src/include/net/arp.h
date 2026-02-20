#pragma once

#include "core/stdtypes.h"

int net_arp_resolve(uint32_t src_ip, uint32_t target_ip, uint8_t out_mac[6]);
int net_arp_try_reply(const void *frame, size_t len);
