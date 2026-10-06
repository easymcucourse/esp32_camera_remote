#pragma once
#include "lwip/sockets.h"
/* SDK HTTP transport hook only, selected by the LCD composition CMake. No fd
 * is exposed through app_wifi or to an application. TCP binds the live AP
 * interface and its concrete IPv4 address, including on a dual-stack socket.
 * HTTP's control UDP accepts only loopback. Failure never falls back to ANY. */
int wifi_esp32_http_bind(int socket, const struct sockaddr *address, socklen_t length);
