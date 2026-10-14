#ifndef OPTIV_FLOW_H
#define OPTIV_FLOW_H

#include <stddef.h>
#include <lwip/ip_addr.h>

void optiv_flow_observe_tcp (const ip_addr_t *app_addr, unsigned short app_port,
                             const ip_addr_t *remote_addr, unsigned short remote_port);
void optiv_flow_observe_udp (const ip_addr_t *app_addr, unsigned short app_port,
                             const ip_addr_t *remote_addr, unsigned short remote_port);
size_t optiv_flow_drain (char output[][192], size_t capacity);
size_t optiv_flow_drops (void);

#endif
