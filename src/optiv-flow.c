#include "optiv-flow.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <lwip/ip_addr.h>

#define FLOW_RING_CAPACITY 1024

static pthread_mutex_t flow_lock = PTHREAD_MUTEX_INITIALIZER;
static char flow_ring[FLOW_RING_CAPACITY][192];
static size_t flow_head;
static size_t flow_count;
static size_t flow_dropped;
static unsigned long long flow_sequence;

static void
observe (const char *protocol, const ip_addr_t *app_addr, unsigned short app_port,
         const ip_addr_t *remote_addr, unsigned short remote_port)
{
    struct timeval now;
    char app[IPADDR_STRLEN_MAX];
    char remote[IPADDR_STRLEN_MAX];
    char event[192];
    unsigned long long sequence;

    if (!ipaddr_ntoa_r (app_addr, app, sizeof app) ||
        !ipaddr_ntoa_r (remote_addr, remote, sizeof remote))
        return;
    gettimeofday (&now, NULL);

    pthread_mutex_lock (&flow_lock);
    sequence = ++flow_sequence;
    snprintf (event, sizeof event,
              "{\"sequence\":%llu,\"protocol\":\"%s\",\"localAddress\":\"%s\",\"localPort\":%u,\"remoteAddress\":\"%s\",\"remotePort\":%u,\"observedMillis\":%lld}",
              sequence, protocol, app, app_port, remote, remote_port,
              (long long)now.tv_sec * 1000LL + now.tv_usec / 1000);
    if (flow_count == FLOW_RING_CAPACITY) {
        flow_head = (flow_head + 1) % FLOW_RING_CAPACITY;
        flow_count--;
        flow_dropped++;
    }
    strncpy (flow_ring[(flow_head + flow_count) % FLOW_RING_CAPACITY], event, 191);
    flow_ring[(flow_head + flow_count) % FLOW_RING_CAPACITY][191] = '\0';
    flow_count++;
    pthread_mutex_unlock (&flow_lock);
}

void
optiv_flow_observe_tcp (const ip_addr_t *app_addr, unsigned short app_port,
                        const ip_addr_t *remote_addr, unsigned short remote_port)
{
    observe ("tcp", app_addr, app_port, remote_addr, remote_port);
}

void
optiv_flow_observe_udp (const ip_addr_t *app_addr, unsigned short app_port,
                        const ip_addr_t *remote_addr, unsigned short remote_port)
{
    observe ("udp", app_addr, app_port, remote_addr, remote_port);
}

size_t
optiv_flow_drain (char output[][192], size_t capacity)
{
    size_t count = 0;
    pthread_mutex_lock (&flow_lock);
    while (count < capacity && flow_count) {
        memcpy (output[count], flow_ring[flow_head], 192);
        flow_head = (flow_head + 1) % FLOW_RING_CAPACITY;
        flow_count--;
        count++;
    }
    pthread_mutex_unlock (&flow_lock);
    return count;
}

size_t
optiv_flow_drops (void)
{
    size_t value;
    pthread_mutex_lock (&flow_lock);
    value = flow_dropped;
    pthread_mutex_unlock (&flow_lock);
    return value;
}
