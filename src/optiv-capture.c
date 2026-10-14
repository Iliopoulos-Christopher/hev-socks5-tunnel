#include "optiv-capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#include <lwip/pbuf.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static FILE *stream;
static char *capture_path;
static size_t byte_limit;
static size_t bytes_written;
static size_t rotations;
static size_t failures;

static int write_u16 (uint16_t value) { return fwrite (&value, sizeof value, 1, stream) == 1; }
static int write_u32 (uint32_t value) { return fwrite (&value, sizeof value, 1, stream) == 1; }

static int write_headers (void)
{
    /* Section Header Block, little endian. */
    if (!write_u32 (0x0A0D0D0A) || !write_u32 (28) ||
        !write_u32 (0x1A2B3C4D) || !write_u16 (1) || !write_u16 (0) ||
        !write_u32 (0xFFFFFFFF) || !write_u32 (0xFFFFFFFF) || !write_u32 (28))
        return 0;

    /* Two DLT_RAW interfaces: 0 is app-to-network, 1 is network-to-app. */
    for (unsigned int i = 0; i < 2; i++) {
        if (!write_u32 (1) || !write_u32 (20) || !write_u16 (101) ||
            !write_u16 (0) || !write_u32 (65535) || !write_u32 (20))
            return 0;
    }
    bytes_written = 68;
    return 1;
}

static int reopen_capture (void)
{
    if (stream)
        fclose (stream);
    stream = fopen (capture_path, "wb");
    if (!stream || !write_headers ()) {
        failures++;
        if (stream) fclose (stream);
        stream = NULL;
        return 0;
    }
    return 1;
}

int optiv_capture_start (const char *path, size_t max_bytes)
{
    int result;
    pthread_mutex_lock (&lock);
    if (stream) fclose (stream);
    stream = NULL;
    free (capture_path);
    capture_path = strdup (path);
    byte_limit = max_bytes < 1048576 ? 1048576 : max_bytes;
    bytes_written = rotations = failures = 0;
    result = capture_path && reopen_capture ();
    pthread_mutex_unlock (&lock);
    return result;
}

void optiv_capture_stop (void)
{
    pthread_mutex_lock (&lock);
    if (stream) {
        fflush (stream);
        fclose (stream);
    }
    stream = NULL;
    free (capture_path);
    capture_path = NULL;
    pthread_mutex_unlock (&lock);
}

void optiv_capture_packet (unsigned int interface_id, const struct pbuf *packet)
{
    struct timeval now;
    uint64_t micros;
    uint32_t length, padded, block_length;
    unsigned char *data;

    if (!packet || !packet->tot_len) return;
    length = packet->tot_len;
    padded = (length + 3U) & ~3U;
    block_length = 32U + padded;
    data = malloc (padded);
    if (!data) { failures++; return; }
    memset (data, 0, padded);
    pbuf_copy_partial (packet, data, length, 0);
    gettimeofday (&now, NULL);
    micros = (uint64_t)now.tv_sec * 1000000ULL + now.tv_usec;

    pthread_mutex_lock (&lock);
    if (stream && bytes_written + block_length > byte_limit) {
        rotations++;
        reopen_capture ();
    }
    if (stream) {
        int ok = write_u32 (6) && write_u32 (block_length) &&
                 write_u32 (interface_id > 1 ? 1 : interface_id) &&
                 write_u32 ((uint32_t)(micros >> 32)) &&
                 write_u32 ((uint32_t)micros) && write_u32 (length) &&
                 write_u32 (length) && fwrite (data, padded, 1, stream) == 1 &&
                 write_u32 (block_length);
        if (ok) {
            bytes_written += block_length;
            /* A capture must remain inspectable after process interruption. */
            if (fflush (stream) != 0) failures++;
        }
        else failures++;
    }
    pthread_mutex_unlock (&lock);
    free (data);
}

size_t optiv_capture_rotations (void) { return rotations; }
size_t optiv_capture_failures (void) { return failures; }
