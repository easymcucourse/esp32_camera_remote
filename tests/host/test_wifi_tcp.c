#include "wifi_tcp.h"
#include "lwip/sockets.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

static int64_t now, cancel_at, changed_at;
static uint32_t generation;
static bool online, readable, fail_fcntl, fail_nodelay, fail_socket, eof, retry_io;
static unsigned sockets, closes, bytes;
static unsigned poll_interrupts;
static bool poll_error;
static int socket_error, diagnostic;
static void network(void *context, uint32_t *gen, bool *up)
{ assert(context == &generation); if (changed_at >= 0 && now >= changed_at) { ++generation; changed_at = -1; } *gen = generation; *up = online; }
static void record(void *context, int error) { assert(context == &generation); diagnostic = error; }
static bool cancelled(void *context) { (void)context; return cancel_at >= 0 && now >= cancel_at; }
int64_t esp_timer_get_time(void) { return now; }
int socket(int family, int type, int protocol)
{ assert(family == AF_INET && type == SOCK_STREAM && protocol == IPPROTO_TCP); ++sockets; if (fail_socket) { errno = ENOMEM; return -1; } return 3; }
int net_fcntl(int fd, int command, ...) { (void)command; assert(fd == 3); if (fail_fcntl) { errno = EIO; return -1; } return 0; }
int connect(int fd, const struct sockaddr *address, socklen_t size)
{ assert(fd == 3 && address && size == sizeof(struct sockaddr_in)); errno = EINPROGRESS; return -1; }
int select(int nfds, fd_set *read, fd_set *write, fd_set *error, struct timeval *wait)
{
    assert(nfds == 4 && ((read != NULL) != (write != NULL)) && !error && wait->tv_sec == 0 && wait->tv_usec <= 100000);
    if (!wait->tv_usec && poll_interrupts) { --poll_interrupts; errno = EINTR; return -1; }
    if (!wait->tv_usec && poll_error) { errno = EIO; return -1; }
    now += readable ? 1000 : wait->tv_usec; return readable ? 1 : 0;
}
int setsockopt(int fd, int level, int option, const void *value, socklen_t size)
{ assert(fd == 3 && level == IPPROTO_TCP && option == TCP_NODELAY && size == sizeof(int) && *(const int *)value == 1); if (fail_nodelay) { errno = EIO; return -1; } return 0; }
int getsockopt(int fd, int level, int option, void *value, socklen_t *size)
{ assert(fd == 3 && level == SOL_SOCKET && option == SO_ERROR && *size == sizeof(int)); *(int *)value = socket_error; return 0; }
int close(int fd) { assert(fd == 3); ++closes; return 0; }
uint16_t htons(uint16_t value) { return (uint16_t)(value << 8 | value >> 8); }
int inet_pton(int family, const char *text, void *address)
{ (void)address; assert(family == AF_INET); return !strcmp(text, "192.168.4.9"); }
int recv(int fd, void *data, size_t size, int flags)
{
    assert(fd == 3 && !flags); if (eof) return 0;
    if (retry_io) { retry_io = false; errno = EAGAIN; return -1; }
    size_t count = size > 2 ? 2 : size; memset(data, 0x55, count); bytes += (unsigned)count; return (int)count;
}
int send(int fd, const void *data, size_t size, int flags)
{ assert(data); uint8_t scratch[2]; return recv(fd, scratch, size > 2 ? 2 : size, flags); }
static void reset(void)
{
    now = 0; cancel_at = changed_at = -1; generation = 1; online = true; readable = true;
    fail_fcntl = fail_nodelay = fail_socket = eof = retry_io = false;
    sockets = closes = bytes = 0; socket_error = diagnostic = 0;
    poll_interrupts = 0; poll_error = false;
}
static app_wifi_result_t open_channel(void **channel, const app_wifi_deadline_t *deadline)
{
    const app_wifi_endpoint_t endpoint = {.address = "192.168.4.9", .port = 15740};
    return wifi_tcp_open(&generation, network, record, &endpoint, deadline, channel);
}
int main(void)
{
    void *channel = NULL; uint8_t data[9]; size_t transferred;
    app_wifi_deadline_t deadline = {.at_us = 800000, .generation = 1, .cancelled = cancelled};
    reset(); assert(open_channel(&channel, &deadline) == APP_WIFI_OK);
    bool available = true; readable = false; int64_t before_poll = now;
    assert(wifi_tcp_poll(channel, &deadline, &available) == APP_WIFI_OK && !available && now == before_poll && !bytes);
    readable = true; assert(wifi_tcp_poll(channel, &deadline, &available) == APP_WIFI_OK && available && !bytes);
    poll_interrupts = 35;
    assert(wifi_tcp_poll(channel, &deadline, &available) == APP_WIFI_OK && !available && poll_interrupts == 3);
    assert(wifi_tcp_poll(channel, &deadline, &available) == APP_WIFI_OK && available && !poll_interrupts);
    poll_error = true; assert(wifi_tcp_poll(channel, &deadline, &available) == APP_WIFI_IO && !available && diagnostic == EIO); poll_error = false;
    app_wifi_deadline_t expired = deadline; expired.at_us = now;
    assert(wifi_tcp_poll(channel, &expired, &available) == APP_WIFI_TIMEOUT && !available);
    cancel_at = now; assert(wifi_tcp_poll(channel, &deadline, &available) == APP_WIFI_CANCELLED && !available); cancel_at = -1;
    ++generation; assert(wifi_tcp_poll(channel, &deadline, &available) == APP_WIFI_STALE && !available); --generation;
    assert(wifi_tcp_io(channel, data, sizeof(data), false, &deadline, &transferred) == APP_WIFI_OK && transferred == 9 && bytes == 9 && data[0] == 0x55);
    retry_io = true; assert(wifi_tcp_io(channel, data, sizeof(data), true, &deadline, &transferred) == APP_WIFI_OK && transferred == 9); wifi_tcp_close(channel); channel = NULL; assert(closes == 1);
    reset(); readable = false; assert(open_channel(&channel, &deadline) == APP_WIFI_TIMEOUT && !channel && now == 800000 && closes == 1);
    reset(); readable = false; cancel_at = 200000;
    assert(open_channel(&channel, &deadline) == APP_WIFI_CANCELLED && !channel && now == 200000 && closes == 1);
    reset(); fail_fcntl = true; assert(open_channel(&channel, &deadline) == APP_WIFI_IO && closes == 1 && diagnostic == EIO);
    reset(); fail_nodelay = true; assert(open_channel(&channel, &deadline) == APP_WIFI_IO && closes == 1);
    reset(); socket_error = ECONNREFUSED; assert(open_channel(&channel, &deadline) == APP_WIFI_IO && closes == 1 && diagnostic == ECONNREFUSED);
    reset(); fail_socket = true; assert(open_channel(&channel, &deadline) == APP_WIFI_IO && !closes);
    reset(); changed_at = 1000; assert(open_channel(&channel, &deadline) == APP_WIFI_STALE && closes == 1);
    reset(); assert(open_channel(&channel, &deadline) == APP_WIFI_OK); cancel_at = now + 2000;
    assert(wifi_tcp_io(channel, data, sizeof(data), false, &deadline, &transferred) == APP_WIFI_CANCELLED && transferred == 2); wifi_tcp_close(channel); channel = NULL;
    reset(); assert(open_channel(&channel, &deadline) == APP_WIFI_OK); changed_at = now + 2000;
    assert(wifi_tcp_io(channel, data, sizeof(data), false, &deadline, &transferred) == APP_WIFI_STALE && transferred == 2); wifi_tcp_close(channel); channel = NULL;
    reset(); assert(open_channel(&channel, &deadline) == APP_WIFI_OK); readable = false;
    assert(wifi_tcp_io(channel, data, 1, false, &deadline, &transferred) == APP_WIFI_TIMEOUT && !transferred && now == 800000); wifi_tcp_close(channel); channel = NULL;
    reset(); assert(open_channel(&channel, &deadline) == APP_WIFI_OK); eof = true;
    assert(wifi_tcp_io(channel, data, 1, false, &deadline, &transferred) == APP_WIFI_CLOSED && !transferred);
    online = false; assert(wifi_tcp_io(channel, data, 1, false, &deadline, &transferred) == APP_WIFI_STATE); wifi_tcp_close(channel);
    return 0;
}
