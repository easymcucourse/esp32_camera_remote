#include "wifi_tcp.h"
#include "lwip/sockets.h"
#include "esp_timer.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int fd;
    uint32_t generation;
    void *context;
    wifi_tcp_network_fn network;
    wifi_tcp_diagnostic_fn diagnostic;
} tcp_channel_t;
static app_wifi_result_t check(tcp_channel_t *channel, const app_wifi_deadline_t *deadline)
{
    uint32_t generation; bool online;
    channel->network(channel->context, &generation, &online);
    if (generation != channel->generation || deadline->generation != channel->generation) return APP_WIFI_STALE;
    if (!online) return APP_WIFI_STATE;
    if (deadline->cancelled && deadline->cancelled(deadline->context)) return APP_WIFI_CANCELLED;
    return esp_timer_get_time() >= deadline->at_us ? APP_WIFI_TIMEOUT : APP_WIFI_OK;
}
static app_wifi_result_t failed(tcp_channel_t *channel, int error)
{
    if (channel->diagnostic) channel->diagnostic(channel->context, error);
    return APP_WIFI_IO;
}
static app_wifi_result_t ready(tcp_channel_t *channel, bool transmit, const app_wifi_deadline_t *deadline)
{
    for (;;) {
        app_wifi_result_t error = check(channel, deadline);
        if (error != APP_WIFI_OK) return error;
        int64_t remaining = deadline->at_us - esp_timer_get_time();
        if (remaining <= 0) return APP_WIFI_TIMEOUT;
        if (remaining > 100000) remaining = 100000;
        struct timeval timeout = {.tv_sec = 0, .tv_usec = (long)remaining};
        fd_set set; FD_ZERO(&set); FD_SET(channel->fd, &set);
        int count = select(channel->fd + 1, transmit ? NULL : &set, transmit ? &set : NULL, NULL, &timeout);
        error = check(channel, deadline);
        if (error != APP_WIFI_OK) return error;
        if (count > 0) return APP_WIFI_OK;
        if (count < 0 && errno != EINTR) return failed(channel, errno);
    }
}
void wifi_tcp_close(void *context)
{
    tcp_channel_t *channel = context;
    if (!channel) return;
    if (channel->fd >= 0 && close(channel->fd) < 0 && channel->diagnostic)
        channel->diagnostic(channel->context, errno);
    free(channel);
}
app_wifi_result_t wifi_tcp_poll(void *context, const app_wifi_deadline_t *deadline, bool *readable)
{
    if (readable) *readable = false;
    if (!context || !deadline || !readable || deadline->at_us <= 0) return APP_WIFI_INVALID;
    tcp_channel_t *channel = context;
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        app_wifi_result_t error = check(channel, deadline);
        if (error != APP_WIFI_OK) return error;
        fd_set set; FD_ZERO(&set); FD_SET(channel->fd, &set);
        struct timeval zero = {0};
        int count = select(channel->fd + 1, &set, NULL, NULL, &zero);
        error = check(channel, deadline);
        if (error != APP_WIFI_OK) return error;
        if (count >= 0) { *readable = count > 0; return APP_WIFI_OK; }
        if (errno != EINTR) return failed(channel, errno);
    }
    return APP_WIFI_OK; /* bounded interruptions; next frame can retry */
}
app_wifi_result_t wifi_tcp_open(void *context, wifi_tcp_network_fn network,
    wifi_tcp_diagnostic_fn diagnostic, const app_wifi_endpoint_t *endpoint,
    const app_wifi_deadline_t *deadline, void **out)
{
    if (!context || !network || !endpoint || !deadline || !out || *out || !deadline->generation ||
        !endpoint->port || !memchr(endpoint->address, 0, sizeof(endpoint->address)) ||
        deadline->at_us <= 0) return APP_WIFI_INVALID;
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(endpoint->port)};
    if (inet_pton(AF_INET, endpoint->address, &address.sin_addr) != 1) return APP_WIFI_INVALID;
    tcp_channel_t *channel = calloc(1, sizeof(*channel));
    if (!channel) return APP_WIFI_NO_MEMORY;
    channel->fd = -1; channel->context = context; channel->network = network;
    channel->diagnostic = diagnostic; channel->generation = deadline->generation;
    app_wifi_result_t error = check(channel, deadline);
    if (error != APP_WIFI_OK) goto fail;
    channel->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (channel->fd < 0) { error = failed(channel, errno); goto fail; }
    if (fcntl(channel->fd, F_SETFL, O_NONBLOCK) < 0) { error = failed(channel, errno); goto fail; }
    int connected = connect(channel->fd, (struct sockaddr *)&address, sizeof(address));
    if (connected < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK) { error = failed(channel, errno); goto fail; }
    if (connected < 0) {
        error = ready(channel, true, deadline); if (error != APP_WIFI_OK) goto fail;
    }
    int socket_error = 0; socklen_t size = sizeof(socket_error);
    if (getsockopt(channel->fd, SOL_SOCKET, SO_ERROR, &socket_error, &size) < 0 || socket_error) {
        error = failed(channel, socket_error ? socket_error : errno); goto fail;
    }
    int no_delay = 1;
    if (setsockopt(channel->fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay)) < 0) {
        error = failed(channel, errno); goto fail;
    }
    error = check(channel, deadline); if (error != APP_WIFI_OK) goto fail;
    *out = channel; return APP_WIFI_OK;
fail:
    wifi_tcp_close(channel); return error;
}
app_wifi_result_t wifi_tcp_io(void *context, void *data, size_t size, bool transmit,
    const app_wifi_deadline_t *deadline, size_t *bytes)
{
    if (bytes) *bytes = 0;
    if (!context || (!data && size) || !bytes || !deadline || deadline->at_us <= 0) return APP_WIFI_INVALID;
    tcp_channel_t *channel = context;
    app_wifi_result_t error = check(channel, deadline);
    if (error != APP_WIFI_OK) return error;
    uint8_t *cursor = data;
    while (*bytes < size) {
        error = ready(channel, transmit, deadline); if (error != APP_WIFI_OK) return error;
        size_t remaining = size - *bytes;
        if (remaining > INT_MAX) remaining = INT_MAX;
        int count = transmit ? send(channel->fd, cursor + *bytes, remaining, 0) :
            recv(channel->fd, cursor + *bytes, remaining, 0);
        if (count < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (count < 0) return failed(channel, errno);
        if (!count) return APP_WIFI_CLOSED;
        if ((size_t)count > remaining) return failed(channel, EIO);
        *bytes += (size_t)count;
        error = check(channel, deadline); if (error != APP_WIFI_OK) return error;
    }
    return APP_WIFI_OK;
}
