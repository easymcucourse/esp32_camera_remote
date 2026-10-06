#include "wifi_http_scope.h"
#include "esp_netif.h"
#include <errno.h>
#include <string.h>
_Static_assert(IFNAMSIZ>=6,"esp_netif implementation name needs six bytes");

static int denied(int error) { errno=error;return -1; }
int wifi_esp32_http_bind(int socket, const struct sockaddr *address, socklen_t length)
{
    if (!address || length<sizeof(*address)) return denied(EINVAL);
    int type=0; socklen_t size=sizeof(type);
    if (getsockopt(socket,SOL_SOCKET,SO_TYPE,&type,&size)<0) return -1;
    if (type==SOCK_DGRAM) {
        /* ESP-IDF HTTP control is local to the process. Do not turn an
         * unexpected wildcard control bind into a reachable UDP endpoint. */
        if (address->sa_family!=AF_INET || length!=sizeof(struct sockaddr_in)) return denied(EAFNOSUPPORT);
        const struct sockaddr_in *v4=(const struct sockaddr_in *)address;
        if (v4->sin_addr.s_addr!=htonl(INADDR_LOOPBACK)) return denied(EACCES);
        return lwip_bind(socket,address,length);
    }
    if (type!=SOCK_STREAM) return denied(EPROTONOSUPPORT);
    if (address->sa_family==AF_INET) {
        if (length!=sizeof(struct sockaddr_in)) return denied(EINVAL);
    }
#if LWIP_IPV6
    else if (address->sa_family==AF_INET6) {
        if (length!=sizeof(struct sockaddr_in6)) return denied(EINVAL);
    }
#endif
    else return denied(EAFNOSUPPORT);
    esp_netif_t *ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t ip={0};
    if (!ap || !esp_netif_is_netif_up(ap) || esp_netif_get_ip_info(ap,&ip)!=ESP_OK || !ip.ip.addr)
        return denied(EADDRNOTAVAIL);
    struct ifreq interface={0};
    if (esp_netif_get_netif_impl_name(ap,interface.ifr_name)!=ESP_OK || !interface.ifr_name[0])
        return denied(ENODEV);
    if (setsockopt(socket,SOL_SOCKET,SO_BINDTODEVICE,&interface,sizeof(interface))<0) return -1;
    if (address->sa_family==AF_INET) {
        struct sockaddr_in bounded=*(const struct sockaddr_in *)address;
        bounded.sin_addr.s_addr=ip.ip.addr;
        return lwip_bind(socket,(const struct sockaddr *)&bounded,sizeof(bounded));
    }
#if LWIP_IPV6
    struct sockaddr_in6 bounded=*(const struct sockaddr_in6 *)address;
    memset(&bounded.sin6_addr,0,sizeof(bounded.sin6_addr));
    bounded.sin6_addr.s6_addr[10]=bounded.sin6_addr.s6_addr[11]=0xff;
    memcpy(&bounded.sin6_addr.s6_addr[12],&ip.ip.addr,4);
    bounded.sin6_scope_id=0;
    /* lwIP explicitly unmaps IPv4-mapped IPv6 addresses in lwip_bind. Retain
     * the SDK's dual-stack socket without exposing its IPv6 ANY listener. */
    return lwip_bind(socket,(const struct sockaddr *)&bounded,sizeof(bounded));
#else
    return denied(EAFNOSUPPORT);
#endif
}
