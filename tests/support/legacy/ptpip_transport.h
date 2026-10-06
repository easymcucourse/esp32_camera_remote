#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// Configure once before starting the single socket owner. Predicate must not block.
typedef bool (*ptpip_cancel_fn)(void *context);
typedef enum { PTPIP_IO_OK, PTPIP_IO_CANCELLED, PTPIP_IO_TIMEOUT, PTPIP_IO_NETWORK } ptpip_io_status_t;
void ptpip_set_cancel(ptpip_cancel_fn fn, void *context);
ptpip_io_status_t ptpip_last_status(void);
// Socket lifetime stays with the camera task; cancellation is checked every <=100 ms.
int ptpip_connect_timeout(const char *address_text, uint16_t port, unsigned timeout_ms);
bool ptpip_transfer(int fd, void *buffer, size_t length, bool transmit);
bool ptpip_timeout_set(int fd, int seconds);
int ptpip_connect(const char *address_text, uint16_t port);

// Nested operations share the earliest whole-transaction deadline.
// Every successful begin must be paired with end on all exit paths.
bool ptpip_transaction_begin(int fd);
void ptpip_transaction_end(void);
