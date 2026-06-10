#ifndef CONTROL_H
#define CONTROL_H

#include "mux_proto.h"
#include <stdint.h>

extern int   control_start(int ctrl_fd, int is_server, int *effective_streams,
                           int *effective_bufsize, int *effective_blocks);
extern int   control_send_ack(int ctrl_fd, uint64_t seqnum);
extern int   control_send_nak(int ctrl_fd, uint64_t start, uint64_t end);
extern int   control_send_heartbeat(int ctrl_fd);
extern int   control_send_bye(int ctrl_fd, const char *reason);
extern void *control_listener(void *arg);
extern void  control_stop(int ctrl_fd);

#endif
