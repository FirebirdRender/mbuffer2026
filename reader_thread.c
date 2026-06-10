#include "mbconf.h"
#include "reader_thread.h"
#include "dest.h"
#include "settings.h"
#include "mux_proto.h"
#include "globals.h"
#include "reorder.h"
#include "control.h"
#include "log.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>

void *reader_thread_main(void *arg)
{
	int stream_id = (int)(intptr_t)arg;

	while (!Terminate) {
		frame_hdr_t hdr;
		uint8_t *payload = 0;
		int slot_id = -1;
		uint32_t payload_len;
		uint64_t seqnum;
		reorder_entry_t entry;

		if (sem_wait(&Dev2Buf) < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (Terminate)
			break;

		for (unsigned i = 0; i < Numblocks; ++i) {
			if (SlotMeta[i].state == SLOT_FREE) {
				slot_id = (int)i;
				SlotMeta[i].state = SLOT_FILLED;
				break;
			}
		}
		if (slot_id < 0)
			continue;

		if (recv_frame(Streams[stream_id].fd, &hdr, &payload) < 0) {
			SlotMeta[slot_id].state = SLOT_FREE;
			sem_post(&Dev2Buf);
			free(payload);
			break;
		}

		if (hdr.msgtype != MUX_DATA) {
			SlotMeta[slot_id].state = SLOT_FREE;
			sem_post(&Dev2Buf);
			free(payload);
			continue;
		}

		payload_len = ntohl(hdr.payload_len);
		seqnum = be64toh(hdr.seqnum);
		if (!NoCrc) {
			uint16_t expected = ntohs(hdr.crc);
			uint16_t actual = crc16_ccitt(payload, payload_len);
			if (actual != expected) {
				control_send_nak(Ctrl.ctrl_fd, seqnum, seqnum);
				SlotMeta[slot_id].state = SLOT_FREE;
				sem_post(&Dev2Buf);
				free(payload);
				continue;
			}
		}

		if (payload_len > 0 && payload != 0)
			memcpy(Buffer[slot_id], payload, payload_len);

		SlotMeta[slot_id].seqnum = seqnum;
		SlotMeta[slot_id].crc = ntohs(hdr.crc);
		control_send_ack(Ctrl.ctrl_fd, seqnum);

		if (hdr.flags & MUX_FLAG_EOF) {
			/* EOF frame has no payload - free the unused buffer slot */
			SlotMeta[slot_id].state = SLOT_FREE;
			sem_post(&Dev2Buf);
			entry.slot_id = -1;  /* marker: no data to output */
			entry.seqnum = seqnum;
			entry.stream_id = stream_id;
			entry.payload_len = 0;
			reorder_push(&ReorderQ, &entry);
			free(payload);
			break;
		}

		entry.slot_id = slot_id;
		entry.seqnum = seqnum;
		entry.stream_id = stream_id;
		entry.payload_len = payload_len;
		reorder_push(&ReorderQ, &entry);

		free(payload);
	}

	return 0;
}
