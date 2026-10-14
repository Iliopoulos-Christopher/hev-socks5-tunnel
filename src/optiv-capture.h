#ifndef OPTIV_CAPTURE_H
#define OPTIV_CAPTURE_H

#include <stddef.h>
#include <stdint.h>

struct pbuf;

int optiv_capture_start (const char *path, size_t max_bytes);
void optiv_capture_stop (void);
void optiv_capture_packet (unsigned int interface_id, const struct pbuf *packet);
size_t optiv_capture_rotations (void);
size_t optiv_capture_failures (void);

#endif
