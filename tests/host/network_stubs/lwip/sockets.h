#pragma once
#include <stddef.h>
#include <stdint.h>
typedef unsigned socklen_t;
struct timeval { long tv_sec, tv_usec; };
typedef struct { unsigned bits; } fd_set;
#define FD_ZERO(p) ((p)->bits=0)
#define FD_SET(fd,p) ((p)->bits |= 1u << (fd))
#define AF_INET 2
#define SOCK_STREAM 1
#define IPPROTO_TCP 6
#define TCP_NODELAY 1
#define SOL_SOCKET 1
#define SO_RCVTIMEO 2
#define SO_SNDTIMEO 3
#define SO_ERROR 4
struct in_addr { uint32_t s_addr; };
struct sockaddr { uint16_t family; };
struct sockaddr_in { uint16_t sin_family, sin_port; struct in_addr sin_addr; };
int socket(int family,int type,int protocol);
int connect(int fd,const struct sockaddr *addr,socklen_t size);
int send(int fd,const void *data,size_t n,int flags);
int recv(int fd,void *data,size_t n,int flags);
int select(int nfds,fd_set *read,fd_set *write,fd_set *error,struct timeval *wait);
int setsockopt(int fd,int level,int option,const void *data,socklen_t n);
int getsockopt(int fd,int level,int option,void *data,socklen_t *n);
int close(int fd);
int inet_pton(int family,const char *text,void *address);
uint16_t htons(uint16_t n);
