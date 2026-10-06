#pragma once
#include <stdint.h>
/* One sequence per firmware for UI, Wi-Fi and UART asynchronous producers. */
uint32_t async_token_next(void);
