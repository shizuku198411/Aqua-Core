#pragma once

#include "stdtypes.h"

int net_arp_resolve(uint32_t src_ip, uint32_t target_ip, uint8_t out_mac[6]);
