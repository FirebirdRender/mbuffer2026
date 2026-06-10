#include "mbconf.h"
#include "control.h"
#include "mux_proto.h"
#include "settings.h"
#include <stdint.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct dest;
typedef struct dest dest_t;

#include "globals.h"
#include "log.h"
#include "ready_pool.h"


static const char hello_key_streams[] = "streams";
static const char hello_key_bufsize[] = "bufsize";
static const char hello_key_blocks[] = "bufblocks";
static const char hello_key_crc[] = "crc";
static const char hello_val_crc_on[] = "crc16";
static const char hello_val_crc_off[] = "none";
#define HELLO_VAL_CRC_ON hello_val_crc_on
#define HELLO_VAL_CRC_OFF hello_val_crc_off

static int parse_hello(const char *msg, int *streams, int *bufsize, int *blocks, int *crc_enabled)
{
	const char *p;

	*streams = 0;
	*bufsize = 0;
	*blocks = 0;
	*crc_enabled = 0;

	p = msg;
	while (*p != '\0') {
		char key[64];
		char val[64];
		int n;

		n = sscanf(p, "%63[^=]=%63[^ \t\r\n]%*1[ \t\r\n]", key, val);
		if (n >= 2) {
			if (strcmp(key, hello_key_streams) == 0) {
				*streams = atoi(val);
			} else if (strcmp(key, hello_key_bufsize) == 0) {
				*bufsize = atoi(val);
			} else if (strcmp(key, hello_key_blocks) == 0) {
				*blocks = atoi(val);
			} else if (strcmp(key, hello_key_crc) == 0) {
				*crc_enabled = (strcmp(val, hello_val_crc_on) == 0);
			}
		}
		while ((*p != '\0') && (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n'))
			++p;
		while ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n'))
			++p;
	}

	return 0;
}

static int min_int(int a, int b)
{
	return (a < b) ? a : b;
}

static int parse_hello_frame(int fd, int *streams, int *bufsize, int *blocks, int *crc_enabled)
{
	frame_hdr_t hdr;
	uint8_t *payload = 0;
	int ret;

	ret = recv_frame(fd, &hdr, &payload);
	if (ret < 0) {
		free(payload);
		return -1;
	}
	if (hdr.msgtype != MUX_HELLO) {
		free(payload);
		return -1;
	}
	if (payload == 0) {
		return -1;
	}
	parse_hello((const char *)payload, streams, bufsize, blocks, crc_enabled);
	free(payload);
	return 0;
}

int control_start(int ctrl_fd, int is_server, int *effective_streams,
                  int *effective_bufsize, int *effective_blocks)
{
	char hello[256];
	frame_hdr_t hdr;
	int my_streams;
	int my_bufsize;
	int my_blocks;
	int my_crc;
	int peer_streams;
	int peer_bufsize;
	int peer_blocks;
	int peer_crc;
	int ret;
	int hello_len;

	my_streams = OptMux;
	my_bufsize = (int)Blocksize;
	my_blocks = (int)Numblocks;
	my_crc = OptNoCrc ? 0 : 1;

	hello_len = snprintf(hello, sizeof(hello), "mbuf-mux/1.0 streams=%d bufsize=%d bufblocks=%d crc=%s",
	                     my_streams, my_bufsize, my_blocks, my_crc ? HELLO_VAL_CRC_ON : HELLO_VAL_CRC_OFF);
	if ((hello_len < 0) || ((size_t)hello_len >= sizeof(hello))) {
		errormsg("mux: HELLO message too long\n");
		return -1;
	}

	if (is_server) {
		ret = parse_hello_frame(ctrl_fd, &peer_streams, &peer_bufsize, &peer_blocks, &peer_crc);
		if (ret < 0) {
			errormsg("mux: invalid HELLO from client\n");
			return -1;
		}

		encode_frame(&hdr, MUX_HELLO, 0, 0, (const uint8_t *)hello, (uint32_t)strlen(hello));
		if (send_frame(ctrl_fd, &hdr, (const uint8_t *)hello) < 0) {
			return -1;
		}
	} else {
		encode_frame(&hdr, MUX_HELLO, 0, 0, (const uint8_t *)hello, (uint32_t)strlen(hello));
		if (send_frame(ctrl_fd, &hdr, (const uint8_t *)hello) < 0) {
			return -1;
		}

		ret = parse_hello_frame(ctrl_fd, &peer_streams, &peer_bufsize, &peer_blocks, &peer_crc);
		if (ret < 0) {
			errormsg("mux: invalid HELLO from server\n");
			return -1;
		}
	}

	if (my_crc != peer_crc) {
		errormsg("mux: CRC capability mismatch (local=%s, peer=%s)\n",
		         my_crc ? HELLO_VAL_CRC_ON : HELLO_VAL_CRC_OFF,
		         peer_crc ? HELLO_VAL_CRC_ON : HELLO_VAL_CRC_OFF);
		return -1;
	}

	*effective_streams = min_int(my_streams, peer_streams);
	*effective_bufsize = min_int(my_bufsize, peer_bufsize);
	*effective_blocks = min_int(my_blocks, peer_blocks);

	Ctrl.heartbeat_ms = OptHeartbeat;
	Ctrl.is_server = is_server;

	debugmsg("mux: negotiated streams=%d bufsize=%d bufblocks=%d crc=%s\n",
	         *effective_streams, *effective_bufsize, *effective_blocks,
	         my_crc ? HELLO_VAL_CRC_ON : HELLO_VAL_CRC_OFF);

	return 0;
}

int control_send_ack(int ctrl_fd, uint64_t seqnum)
{
	frame_hdr_t hdr;
	uint8_t payload[sizeof(uint64_t)];
	uint64_t nseq;

	nseq = htobe64(seqnum);
	memcpy(payload, &nseq, sizeof(nseq));
	encode_frame(&hdr, MUX_ACK, 0, seqnum, payload, (uint32_t)sizeof(payload));
	return send_frame(ctrl_fd, &hdr, payload);
}

int control_send_nak(int ctrl_fd, uint64_t start, uint64_t end)
{
	frame_hdr_t hdr;
	uint8_t payload[sizeof(uint64_t) * 2];
	uint64_t nstart;
	uint64_t nend;

	nstart = htobe64(start);
	nend = htobe64(end);
	memcpy(payload, &nstart, sizeof(nstart));
	memcpy(payload + sizeof(nstart), &nend, sizeof(nend));
	encode_frame(&hdr, MUX_NAK, 0, start, payload, (uint32_t)sizeof(payload));
	return send_frame(ctrl_fd, &hdr, payload);
}

int control_send_heartbeat(int ctrl_fd)
{
	frame_hdr_t hdr;

	encode_frame(&hdr, MUX_HEARTBEAT, 0, 0, 0, 0);
	return send_frame(ctrl_fd, &hdr, 0);
}

int control_send_bye(int ctrl_fd, const char *reason)
{
	frame_hdr_t hdr;
	uint32_t len;
	const uint8_t *payload;

	if (reason != 0) {
		payload = (const uint8_t *)reason;
		len = (uint32_t)strlen(reason);
	} else {
		payload = 0;
		len = 0;
	}
	encode_frame(&hdr, MUX_BYE, 0, 0, payload, len);
	return send_frame(ctrl_fd, &hdr, payload);
}

static void control_handle_ack(uint8_t *payload)
{
	uint64_t ack_seq;
	int s;

	memcpy(&ack_seq, payload, sizeof(ack_seq));
	ack_seq = be64toh(ack_seq);

	for (s = 0; s < NumStreams; ++s) {
		int i;
		int w = 0;

		pthread_mutex_lock(&Streams[s].pending_lock);
		for (i = 0; i < Streams[s].pending_count; ++i) {
			int sid = Streams[s].pending[i];
			if (SlotMeta[sid].seqnum <= ack_seq) {
				SlotMeta[sid].state = SLOT_FREE;
				SlotMeta[sid].stream_id = -1;
				sem_post(&Dev2Buf);
			} else {
				Streams[s].pending[w++] = sid;
			}
		}
		Streams[s].pending_count = w;
		pthread_mutex_unlock(&Streams[s].pending_lock);
	}
}

static void control_handle_nak(uint8_t *payload)
{
	uint64_t nak_start;
	uint64_t nak_end;
	int s;

	memcpy(&nak_start, payload, sizeof(nak_start));
	memcpy(&nak_end, payload + sizeof(nak_start), sizeof(nak_end));
	nak_start = be64toh(nak_start);
	nak_end = be64toh(nak_end);

	for (s = 0; s < NumStreams; ++s) {
		int i;
		int w = 0;

		pthread_mutex_lock(&Streams[s].pending_lock);
		for (i = 0; i < Streams[s].pending_count; ++i) {
			int sid = Streams[s].pending[i];
			if ((SlotMeta[sid].seqnum >= nak_start) && (SlotMeta[sid].seqnum <= nak_end)) {
				SlotMeta[sid].state = SLOT_PENDING_RETRANSMIT;
				ready_pool_push(&ReadyPool, sid);
			} else {
				Streams[s].pending[w++] = sid;
			}
		}
		Streams[s].pending_count = w;
		pthread_mutex_unlock(&Streams[s].pending_lock);
	}
}

void *control_listener(void *arg)
{
	(void)arg;

	while (!Terminate) {
		fd_set rfds;
		struct timeval tv;
		int ret;
		frame_hdr_t hdr;
		uint8_t *payload = 0;

		FD_ZERO(&rfds);
		FD_SET(Ctrl.ctrl_fd, &rfds);
		tv.tv_sec = Ctrl.heartbeat_ms / 1000;
		tv.tv_usec = (Ctrl.heartbeat_ms % 1000) * 1000;
		ret = select(Ctrl.ctrl_fd + 1, &rfds, 0, 0, &tv);
		if (ret < 0) {
			if (errno == EINTR) {
				continue;
			}
			errormsg("mux: control connection select error: %s\n", strerror(errno));
			break;
		}
		if (ret == 0) {
			(void)control_send_heartbeat(Ctrl.ctrl_fd);
			continue;
		}
		if (recv_frame(Ctrl.ctrl_fd, &hdr, &payload) < 0) {
			errormsg("mux: control connection read error\n");
			Terminate = 1;
			free(payload);
			break;
		}
		switch (hdr.msgtype) {
		case MUX_HEARTBEAT:
			break;
		case MUX_BYE:
			debugmsg("mux: peer sent BYE\n");
			Terminate = 1;
			break;
		case MUX_ACK:
			if (payload != 0) {
				control_handle_ack(payload);
			}
			break;
		case MUX_NAK:
			if (payload != 0) {
				control_handle_nak(payload);
			}
			break;
		default:
			debugmsg("mux: unknown control msgtype 0x%02x\n", hdr.msgtype);
			break;
		}
		free(payload);
	}

	return 0;
}

void control_stop(int ctrl_fd)
{
	(void)control_send_bye(ctrl_fd, "normal");
	close(ctrl_fd);
	Ctrl.ctrl_fd = -1;
}
