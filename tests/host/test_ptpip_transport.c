#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "ptpip_transport.h"
#include "lwip/sockets.h"
static int64_t now, cancel_at;
static bool ready;
static unsigned bytes, closed;
static int socket_error;
static struct timeval configured={.tv_sec=5};
int64_t esp_timer_get_time(void) { return now; }
static bool cancel(void *context) { (void)context; return cancel_at>=0 && now>=cancel_at; }
int select(int nfds,fd_set *read,fd_set *write,fd_set *error,struct timeval *wait) {
    (void)nfds;(void)read;(void)write;(void)error;
    assert(wait->tv_sec==0 && wait->tv_usec<=100000);
    if (ready) { now+=1000; return 1; }
    now+=wait->tv_usec; return 0;
}
int socket(int f,int t,int p) { (void)f;(void)t;(void)p; return 3; }
int connect(int fd,const struct sockaddr *a,socklen_t n) { (void)fd;(void)a;(void)n; errno=EINPROGRESS;return -1; }
int net_fcntl(int fd,int cmd,...) { (void)fd;(void)cmd;return 0; }
int close(int fd) { assert(fd==3); ++closed;return 0; }
uint16_t htons(uint16_t n) { return (uint16_t)(n<<8|n>>8); }
int inet_pton(int f,const char *text,void *a) { (void)f;(void)a;return !strcmp(text,"192.168.4.9"); }
int setsockopt(int fd,int level,int option,const void *data,socklen_t n) {
    (void)fd;(void)level;(void)option;assert(n==sizeof(configured));memcpy(&configured,data,n);return 0;
}
int getsockopt(int fd,int level,int option,void *data,socklen_t *n) {
    (void)fd;(void)level;(void)n;
    if (option==SO_ERROR) memcpy(data,&socket_error,sizeof(socket_error));
    else memcpy(data,&configured,sizeof(configured));
    return 0;
}
int recv(int fd,void *data,size_t n,int flags) {
    (void)fd;(void)flags;size_t amount=n>2?2:n;memset(data,0x55,amount);bytes+=amount;return (int)amount;
}
int send(int fd,const void *data,size_t n,int flags) { (void)data;uint8_t scratch[2];return recv(fd,scratch,n>2?2:n,flags); }
static void reset(void) { now=0;cancel_at=-1;ready=false;bytes=closed=0;socket_error=0;configured.tv_sec=5;configured.tv_usec=0;ptpip_set_cancel(cancel,NULL); }
int main(void) {
    uint8_t data[9];
    reset();ready=true;assert(ptpip_transfer(3,data,sizeof(data),false));assert(bytes==9);
    reset();ready=true;assert(ptpip_transfer(3,data,sizeof(data),true));assert(bytes==9);
    reset();cancel_at=200000;assert(!ptpip_transfer(3,data,1,false));
    assert(ptpip_last_status()==PTPIP_IO_CANCELLED && now==200000 && bytes==0);
    reset();ready=true;cancel_at=2000;assert(!ptpip_transfer(3,data,sizeof(data),false));
    assert(ptpip_last_status()==PTPIP_IO_CANCELLED && bytes==2);
    reset();configured.tv_sec=1;assert(!ptpip_transfer(3,data,1,false));
    assert(ptpip_last_status()==PTPIP_IO_TIMEOUT && now==1000000);
    reset();cancel_at=100000;assert(ptpip_connect_timeout("192.168.4.9",15740,800)==-1);
    assert(ptpip_last_status()==PTPIP_IO_CANCELLED && closed==1);
    reset();assert(ptpip_connect_timeout("192.168.4.9",15740,800)==-1);
    assert(ptpip_last_status()==PTPIP_IO_TIMEOUT && now==800000 && closed==1);
    reset();ready=true;assert(ptpip_connect("192.168.4.9",15740)==3);assert(closed==0);
    reset();ready=true;socket_error=1;assert(ptpip_connect("192.168.4.9",15740)==-1);
    assert(ptpip_last_status()==PTPIP_IO_NETWORK && closed==1);
    reset();assert(ptpip_connect("invalid",15740)==-1 && !closed);
    reset();ready=true;configured.tv_sec=1;
    assert(ptpip_transaction_begin(3));
    assert(ptpip_transfer(3,data,2,false)); now=900000;
    assert(ptpip_transaction_begin(3)); /* nested calls cannot extend the deadline */
    ptpip_transaction_end(); now=1000000;
    assert(!ptpip_transfer(3,data,2,false) && ptpip_last_status()==PTPIP_IO_TIMEOUT);
    ptpip_transaction_end();
    assert(ptpip_transfer(3,data,2,false)); /* deadline clears for the next transaction */
    puts("transport cancellation, deadline, partial transfer and connect tests passed");return 0;
}
