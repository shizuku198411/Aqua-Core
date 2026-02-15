#pragma once

#include "stdtypes.h"

int net_init(void);
int net_tx_frame(const void *buf, size_t len);
