/*
 *  Copyright (C) 2000-2017, Thomas Maier-Komor
 *
 *  This is the source code of mbuffer.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <pthread.h>
#include <semaphore.h>
#include "reorder.h"
#include "ready_pool.h"
#include "mux_proto.h"

typedef struct {
	uint64_t   seqnum;
	int16_t    stream_id;
	uint16_t   crc;
	uint32_t   payload_len;
	int        state;
} slot_meta_t;

typedef struct {
	int        fd;
	int        state;
	int       *pending;
	int        pending_count;
	int        pending_cap;
	pthread_t  thread;
	pthread_mutex_t pending_lock;
	int        retry_count;
} stream_t;

typedef struct {
	int        ctrl_fd;
	pthread_t  thread;
	int        heartbeat_ms;
	int        timeout_ms;
	int        is_server;
} control_t;

struct destination;
extern struct destination *Dest;
extern slot_meta_t    *SlotMeta;
extern stream_t        Streams[MAX_STREAMS];
extern int             NumStreams;
extern int             CtrlPort;
extern int             HeartbeatMs;
extern int             NoCrc;
extern int             NoMux;
extern control_t       Ctrl;
extern ready_pool_t    ReadyPool;
extern reorder_queue_t ReorderQ;
extern volatile int    PauseData;
extern pthread_mutex_t PauseMtx;
extern pthread_cond_t  PauseCv;
extern volatile uint64_t NextSeqnum;


#define OPTION_B 1
#define OPTION_M 2
#define OPTION_S 4

extern int
	Hashers,
	Terminal, 
	TermQ[2],
	Tmp,
	In,
	OptMode;

extern volatile int
	ActSenders,
	InputDone,	/* input has finished (mux mode) */
	NumSenders,	/* number of sender threads */
	SendSize,
	Terminate,	/* abort execution, because of error or signal */
	Watchdog;	/* 0: off, 1: started, 2: raised */

extern volatile unsigned
	Done,
	EmptyCount,	/* counter incremented when buffer runs empty */
	FullCount,	/* counter incremented when buffer gets full */
	MainOutOK;	/* is the main outputThread still writing or just coordinating senders */

extern unsigned
	Vol;		/* volume counter */

extern volatile unsigned long long
	Rest,
	Numin,
	Numout,
	InSize;

extern char *volatile
	SendAt;
extern char *		InputAddr;
extern char *		DestAddr;

extern size_t
	IDevBSize,
	PrefixLen;

extern long
	PgSz,
	Finish,		/* this is for graceful termination */
	TickTime;

extern char
	*Prefix,
	**Buffer;

extern pthread_mutex_t
	TermMut,	/* prevents statusThread from interfering with request*Volume */
	LowMut,
	HighMut,
	SendMut;

extern sem_t
	Dev2Buf,
	Buf2Dev;

extern pthread_cond_t
	PercLow,	/* low watermark */
	PercHigh,	/* high watermark */
	SendCond;

extern pthread_t
	ReaderThr,
	WatchdogThr;

extern struct timespec
	Starttime;

#endif
