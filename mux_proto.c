#include "mbconf.h"
#include "mux_proto.h"
#include "log.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>

// CRC16-CCITT lookup table (polynomial 0x1021)
static uint16_t crc16_table[256];
static int      crc16_initialized = 0;

void crc16_init(void)
{
    if (crc16_initialized)
        return;
    for (int i = 0; i < 256; i++) {
        uint16_t crc = (uint16_t)(i << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            else
                crc = (uint16_t)(crc << 1);
        }
        crc16_table[i] = crc;
    }
    crc16_initialized = 1;
}

uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    if (!crc16_initialized)
        crc16_init();
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
        crc = (uint16_t)((crc << 8) ^ crc16_table[((crc >> 8) ^ data[i]) & 0xFF]);
    return crc;
}

void encode_frame(frame_hdr_t *hdr, uint8_t msgtype, uint8_t flags,
                  uint64_t seqnum, const uint8_t *payload, uint32_t len)
{
    hdr->magic       = htons(MUX_MAGIC);
    hdr->version     = MUX_VERSION;
    hdr->msgtype     = msgtype;
    hdr->flags       = flags;
    hdr->seqnum      = htobe64(seqnum);
    hdr->payload_len = htonl(len);
    if (payload && len > 0)
        hdr->crc = htons(crc16_ccitt(payload, len));
    else
        hdr->crc = 0;
}

int send_frame(int fd, const frame_hdr_t *hdr, const uint8_t *payload)
{
    if (write_exactly(fd, hdr, MUX_HEADER_SIZE) < 0)
        return -1;
    uint32_t len = ntohl(hdr->payload_len);
    if (len > 0 && payload) {
        if (write_exactly(fd, payload, len) < 0)
            return -1;
    }
    return 0;
}

int recv_frame(int fd, frame_hdr_t *hdr, uint8_t **payload)
{
    if (read_exactly(fd, hdr, MUX_HEADER_SIZE) < 0)
        return -1;
    uint32_t len = ntohl(hdr->payload_len);
    if (len > 0) {
        *payload = malloc(len);
        if (!*payload)
            return -1;
        if (read_exactly(fd, *payload, len) < 0) {
            free(*payload);
            *payload = NULL;
            return -1;
        }
    } else {
        *payload = NULL;
    }
    return 0;
}

int read_exactly(int fd, void *buf, size_t n)
{
    size_t done = 0;
    while (done < n) {
        ssize_t r = read(fd, (uint8_t*)buf + done, n - done);
        if (r <= 0) {
            if (r == 0) {
                debugmsg("mux_proto: EOF reading %zu bytes (got %zu)\n", n, done);
                return -1;
            }
            if (errno == EINTR)
                continue;
            return -1;
        }
        done += (size_t)r;
    }
    return 0;
}

int write_exactly(int fd, const void *buf, size_t n)
{
    size_t done = 0;
    while (done < n) {
        ssize_t w = write(fd, (const uint8_t*)buf + done, n - done);
        if (w <= 0) {
            if (w == 0)
                return -1;
            if (errno == EINTR)
                continue;
            return -1;
        }
        done += (size_t)w;
    }
    return 0;
}
