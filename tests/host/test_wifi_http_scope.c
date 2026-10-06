#include "wifi_http_scope.h"
#include "esp_netif.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
static esp_netif_t ap={.identity=3};
static bool present=true,up=true;
static esp_err_t ip_error,name_error;
static uint32_t ip;
static int type=SOCK_STREAM,get_error,set_error,bind_error;
static unsigned bound,device_sets;
static struct sockaddr_in last4;
static struct sockaddr_in6 last6;
static socklen_t last_length;
uint32_t htonl(uint32_t n)
{ return (n>>24)|((n>>8)&0xff00)|((n<<8)&0xff0000)|(n<<24); }
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key)
{ assert(!strcmp(key,"WIFI_AP_DEF"));return present ? &ap : NULL; }
bool esp_netif_is_netif_up(esp_netif_t *netif) { assert(netif==&ap);return up; }
esp_err_t esp_netif_get_ip_info(esp_netif_t *netif,esp_netif_ip_info_t *out)
{ assert(netif==&ap);out->ip.addr=ip;return ip_error; }
esp_err_t esp_netif_get_netif_impl_name(esp_netif_t *netif,char *name)
{ assert(netif==&ap);strcpy(name,"ap1");return name_error; }
int getsockopt(int fd,int level,int option,void *out,socklen_t *length)
{ assert(fd==7 && level==SOL_SOCKET && option==SO_TYPE && *length==sizeof(int));*(int *)out=type;return get_error; }
int setsockopt(int fd,int level,int option,const void *data,socklen_t size)
{
    assert(fd==7 && level==SOL_SOCKET && option==SO_BINDTODEVICE && size==sizeof(struct ifreq));
    assert(!strcmp(((const struct ifreq *)data)->ifr_name,"ap1"));++device_sets;return set_error;
}
int lwip_bind(int fd,const struct sockaddr *address,socklen_t length)
{
    assert(fd==7);++bound;last_length=length;
    if(address->sa_family==AF_INET) { assert(length==sizeof last4);memcpy(&last4,address,length); }
    else { assert(address->sa_family==AF_INET6 && length==sizeof last6);memcpy(&last6,address,length); }
    return bind_error;
}
int main(void)
{
    ip=htonl(0xc0a80401u);
    struct sockaddr_in v4={.sin_family=AF_INET,.sin_port=0x5000};
    assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==0);
    assert(bound==1 && device_sets==1 && last4.sin_addr.s_addr==ip && last4.sin_port==v4.sin_port);
    assert(!v4.sin_addr.s_addr); /* Original SDK ANY address is not mutated. */
    struct sockaddr_in6 v6={.sin6_family=AF_INET6,.sin6_port=0x5000,.sin6_scope_id=9};
#if LWIP_IPV6
    assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v6,sizeof v6)==0 && bound==2);
    uint8_t expected[16]={0};expected[10]=expected[11]=0xff;memcpy(expected+12,&ip,4);
    assert(last_length==sizeof last6 && !memcmp(last6.sin6_addr.s6_addr,expected,16) && !last6.sin6_scope_id);
    assert(last6.sin6_port==v6.sin6_port && v6.sin6_scope_id==9);
#else
    assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v6,sizeof v6)==-1 && errno==EAFNOSUPPORT);
#endif
    unsigned before=bound;
    present=false;assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==-1 && errno==EADDRNOTAVAIL);present=true;
    up=false;assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==-1);up=true;
    uint32_t saved=ip;ip=0;assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==-1);ip=saved;
    ip_error=ESP_FAIL;assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==-1);ip_error=ESP_OK;
    name_error=ESP_FAIL;assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==-1 && errno==ENODEV);name_error=ESP_OK;
    set_error=-1;assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==-1);set_error=0;
    get_error=-1;assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==-1);get_error=0;
    assert(bound==before); /* None of those failures falls back to wildcard bind. */
    assert(wifi_esp32_http_bind(7,NULL,0)==-1 && errno==EINVAL);
    assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4-1)==-1 && errno==EINVAL);
    v4.sin_family=99;assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==-1 && errno==EAFNOSUPPORT);v4.sin_family=AF_INET;
    type=99;assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==-1 && errno==EPROTONOSUPPORT);
    type=SOCK_DGRAM;
    assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==-1 && errno==EACCES && bound==before);
    v4.sin_addr.s_addr=htonl(INADDR_LOOPBACK);unsigned sets_before=device_sets;
    assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==0);
    assert(bound==before+1 && last4.sin_addr.s_addr==htonl(INADDR_LOOPBACK) && device_sets==sets_before);
    bind_error=-1;assert(wifi_esp32_http_bind(7,(struct sockaddr *)&v4,sizeof v4)==-1);
    return 0;
}
