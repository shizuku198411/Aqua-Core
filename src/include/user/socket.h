#pragma once

#include "core/stdtypes.h"

#define AF_INET         2
#define SOCK_DGRAM      2
#define IPPROTO_UDP     17

struct socket_addr_in {
    uint16_t sin_family;  // AF_INET
    uint16_t sin_port;    // network byte order
    uint32_t sin_addr;    // network byte order (IPv4)
};
