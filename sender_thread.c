#include "mbconf.h"
#include "sender_thread.h"
#include "mux_proto.h"
typedef struct dest dest_t;
#include "globals.h"
#include "ready_pool.h"
#include "settings.h"
#include "log.h"

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static void sender_drain_pending(int stream_id)
{
	int i;
	int w = 0;

	pthread_mutex_lock(&Streams[stream_id].pending_lock);
	for (i = 0; i < Streams[stream_id].pending_count; ++i) {
		int sid = Streams[stream_id].pending[i];
		SlotMeta[sid].state = SLOT_PENDING_RETRANSMIT;
		ready_pool_push(&ReadyPool, sid);
	}
	Streams[stream_id].pending_count = w;
	pthread_mutex_unlock(&Streams[stream_id].pending_lock);
}

static void sender_wait_if_paused(void)
{
	pthread_mutex_lock(&PauseMtx);
	while (PauseData && !Terminate)
		pthread_cond_wait(&PauseCv, &PauseMtx);
	pthread_mutex_unlock(&PauseMtx);
}

void *sender_thread_main(void *arg)
{
	int stream_id = (int)(intptr_t)arg;

	while (!Terminate) {
		int slot_id;
		int ret;

		if (PauseData) {
			sender_wait_if_paused();
			continue;
		}

		ret = ready_pool_try_pop(&ReadyPool, &slot_id, 100);
		if (ret != 0) {
			if (ret < 0 && errno == EINTR)
				continue;
			if (InputDone && ready_pool_count(&ReadyPool) == 0) {
				/* Input finished and no more data to send - push EOF */
				frame_hdr_t hdr;
				encode_frame(&hdr, MUX_DATA, MUX_FLAG_EOF, NextSeqnum++, NULL, 0);
				send_frame(Streams[stream_id].fd, &hdr, NULL);
				break;
			}
			if (PauseData)
				sender_wait_if_paused();
			continue;
		}

		if (PauseData) {
			ready_pool_push(&ReadyPool, slot_id);
			sender_wait_if_paused();
			continue;
		}

		if (Terminate)
			break;

		if (Streams[stream_id].state == STREAM_DEAD) {
			ready_pool_push(&ReadyPool, slot_id);
			sender_drain_pending(stream_id);
			continue;
		}

		SlotMeta[slot_id].stream_id = (int16_t)stream_id;
		SlotMeta[slot_id].state = SLOT_IN_FLIGHT;

		pthread_mutex_lock(&Streams[stream_id].pending_lock);
		if (Streams[stream_id].pending_count < Streams[stream_id].pending_cap)
			Streams[stream_id].pending[Streams[stream_id].pending_count++] = slot_id;
		pthread_mutex_unlock(&Streams[stream_id].pending_lock);

		{
			frame_hdr_t hdr;
			uint8_t flags = 0;
			const uint8_t *payload = (const uint8_t *)Buffer[slot_id];
			uint32_t len = SlotMeta[slot_id].payload_len;

			if (SlotMeta[slot_id].seqnum == UINT64_MAX) {
				flags = MUX_FLAG_EOF;
				len = 0;
				payload = NULL;
			}

			encode_frame(&hdr, MUX_DATA, flags, SlotMeta[slot_id].seqnum, payload, len);
			if (send_frame(Streams[stream_id].fd, &hdr, payload) < 0) {
				debugmsg("sender[%d]: frame send failed\n", stream_id);
				Streams[stream_id].state = STREAM_DEAD;
				sender_drain_pending(stream_id);
			}
			if (flags & MUX_FLAG_EOF)
				break;
		}
	}

	return NULL;
}
