#include "ptpip_transport.h"
#include <errno.h>
#include <fcntl.h>
#include "lwip/sockets.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "ptpip_transport";
/* Only the camera owner calls transport functions. The predicate reads atomics. */
static ptpip_cancel_fn cancel_fn;
static void *cancel_context;
static ptpip_io_status_t last_status;
static unsigned transaction_depth;
static int64_t transaction_deadline;

void ptpip_set_cancel(ptpip_cancel_fn fn, void *context)
{
    cancel_fn = fn; cancel_context = context;
}
ptpip_io_status_t ptpip_last_status(void) { return last_status; }
static bool cancelled(void) { return cancel_fn && cancel_fn(cancel_context); }

static bool timeout_budget(int fd, int option, int64_t *budget)
{
    struct timeval timeout = {0};
    socklen_t size = sizeof(timeout);
    if (getsockopt(fd, SOL_SOCKET, option, &timeout, &size) < 0) {
        last_status = PTPIP_IO_NETWORK; return false;
    }
    *budget = (int64_t)timeout.tv_sec * 1000000 + timeout.tv_usec;
    if (*budget <= 0) *budget = 5000000;
    return true;
}
bool ptpip_transaction_begin(int fd)
{
    if (cancelled()) { last_status = PTPIP_IO_CANCELLED; return false; }
    int64_t budget;
    if (!timeout_budget(fd, SO_RCVTIMEO, &budget)) return false;
    int64_t deadline = esp_timer_get_time() + budget;
    if (!transaction_depth || deadline < transaction_deadline) transaction_deadline = deadline;
    ++transaction_depth;
    return true;
}
void ptpip_transaction_end(void)
{
    if (transaction_depth && --transaction_depth == 0) transaction_deadline = 0;
}

static bool wait_ready(int fd, bool transmit, int64_t deadline)
{
    for (;;) {
        if (cancelled()) { last_status = PTPIP_IO_CANCELLED; return false; }
        int64_t remaining = deadline - esp_timer_get_time();
        if (remaining <= 0) { last_status = PTPIP_IO_TIMEOUT; return false; }
        if (remaining > 100000) remaining = 100000;
        struct timeval wait = {.tv_sec = 0, .tv_usec = (long)remaining};
        fd_set ready;
        FD_ZERO(&ready); FD_SET(fd, &ready);
        int rc = select(fd + 1, transmit ? NULL : &ready, transmit ? &ready : NULL, NULL, &wait);
        if (rc > 0) return true;
        if (rc < 0 && errno != EINTR) { last_status = PTPIP_IO_NETWORK; return false; }
    }
}

bool ptpip_transfer(int fd, void *buffer, size_t length, bool transmit)
{
    last_status = PTPIP_IO_OK;
    int64_t budget;
    if (!timeout_budget(fd, transmit ? SO_SNDTIMEO : SO_RCVTIMEO, &budget)) return false;
    int64_t deadline = esp_timer_get_time() + budget;
    if (transaction_depth && transaction_deadline < deadline) deadline = transaction_deadline;
    uint8_t *p = buffer;
    while (length) {
        if (!wait_ready(fd, transmit, deadline)) return false;
        if (cancelled()) { last_status = PTPIP_IO_CANCELLED; return false; }
        int n = transmit ? send(fd, p, length, 0) : recv(fd, p, length, 0);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (n <= 0) {
            last_status = PTPIP_IO_NETWORK;
            ESP_LOGW(TAG, "%s stopped: result=%d errno=%d", transmit ? "send" : "recv", n, errno);
            return false;
        }
        p += n; length -= n;
    }
    return true;
}

bool ptpip_timeout_set(int fd, int seconds)
{
    if (seconds <= 0) return false;
    struct timeval timeout = {.tv_sec = seconds};
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
           setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
}

int ptpip_connect_timeout(const char *address_text, uint16_t port, unsigned timeout_ms)
{
    last_status = PTPIP_IO_OK;
    if (cancelled()) { last_status = PTPIP_IO_CANCELLED; return -1; }
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(port)};
    if (!timeout_ms || inet_pton(AF_INET, address_text, &address.sin_addr) != 1) {
        last_status = PTPIP_IO_NETWORK; return -1;
    }
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) { last_status = PTPIP_IO_NETWORK; return -1; }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) goto fail;
    int rc = connect(fd, (struct sockaddr *)&address, sizeof(address));
    if (rc < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK) goto fail;
    if (rc < 0 && !wait_ready(fd, true, esp_timer_get_time() + (int64_t)timeout_ms * 1000)) goto fail;
    int error = 0;
    socklen_t size = sizeof(error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) < 0 || error) goto fail;
    if (cancelled()) { last_status = PTPIP_IO_CANCELLED; goto fail; }
    /* Command/event traffic uses short PTP/IP packets. Do not hold a command
     * waiting to coalesce it with a later packet or a delayed acknowledgement. */
    int no_delay = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay)) < 0) goto fail;
    /* Remain nonblocking: select slices bound stop latency during all transfers. */
    if (!ptpip_timeout_set(fd, 5)) goto fail;
    return fd;
fail:
    if (last_status == PTPIP_IO_OK) last_status = PTPIP_IO_NETWORK;
    close(fd);
    return -1;
}
int ptpip_connect(const char *address_text, uint16_t port)
{
    return ptpip_connect_timeout(address_text, port, 5000);
}
