#pragma once
#include "../../network_stubs/lwip/sockets.h"
#define sa_family family
#define AF_INET6 10
#define SOCK_DGRAM 2
#define SO_TYPE 20
#define SO_BINDTODEVICE 21
#define INADDR_LOOPBACK 0x7f000001u
#define IFNAMSIZ 6
struct ifreq { char ifr_name[IFNAMSIZ]; };
struct in6_addr { uint8_t s6_addr[16]; };
struct sockaddr_in6 { uint16_t sin6_family,sin6_port;struct in6_addr sin6_addr;uint32_t sin6_scope_id; };
uint32_t htonl(uint32_t value);
int lwip_bind(int fd,const struct sockaddr *address,socklen_t length);
