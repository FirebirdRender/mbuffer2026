#ifndef MUX_PROTO_H
#define MUX_PROTO_H

#include <stdint.h>
#include <sys/types.h>

#define MUX_MAGIC       0x4D42   // 'MB'
#define MUX_VERSION     0x01
#define MUX_HEADER_SIZE 19

// MsgType values
#define MUX_DATA        0x01
#define MUX_ACK         0x02
#define MUX_NAK         0x03
#define MUX_WRITE_ACK   0x04
#define MUX_HELLO       0x05
#define MUX_HEARTBEAT   0x06
#define MUX_BYE         0x07

// Flags bits
#define MUX_FLAG_EOF    0x01

// Buffer slot states
#define SLOT_FREE               0
#define SLOT_FILLED             1
#define SLOT_IN_FLIGHT          2
#define SLOT_PENDING_RETRANSMIT 3

// Stream states
#define STREAM_IDLE       0
#define STREAM_CONNECTING 1
#define STREAM_ACTIVE     2
#define STREAM_DEAD       3

#define MAX_STREAMS 64

typedef struct __attribute__((packed)) {
    uint16_t magic;        // MUX_MAGIC
    uint8_t  version;      // MUX_VERSION
    uint8_t  msgtype;      // MUX_DATA .. MUX_BYE
    uint8_t  flags;        // MUX_FLAG_EOF
    uint64_t seqnum;       // Big-endian
    uint32_t payload_len;  // Big-endian
    uint16_t crc;          // CRC16-CCITT of payload (0x0000 = not computed)
} frame_hdr_t;

// CRC16-CCITT (polynomial 0x1021)
extern uint16_t crc16_ccitt(const uint8_t *data, size_t len);
extern void     crc16_init(void);  // precompute lookup table

// Frame encode/decode helpers
extern void   encode_frame(frame_hdr_t *hdr, uint8_t msgtype, uint8_t flags,
                           uint64_t seqnum, const uint8_t *payload, uint32_t len);
extern int    send_frame(int fd, const frame_hdr_t *hdr, const uint8_t *payload);
extern int    recv_frame(int fd, frame_hdr_t *hdr, uint8_t **payload);

// Read exactly n bytes (returns 0 on success, -1 on error/EOF)
extern int    read_exactly(int fd, void *buf, size_t n);
extern int    write_exactly(int fd, const void *buf, size_t n);

#endif
