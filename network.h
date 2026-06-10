/*
 *  Copyright (C) 2000-2009, Thomas Maier-Komor
 *
 *  This file is part of mbuffer's source code.
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

#ifndef NETWORK_H
#define NETWORK_H

#include "dest.h"
#include "globals.h"

void initNetworkInput(const char *addr);
struct destination *createNetworkOutput(const char *addr);

extern int  createControlConnection(const char *host, int port);
extern int  bindControlListen(int port);
extern int  createDataConnection(const char *host, int port);
extern int  bindDataListen(int port);
extern int  reconnectStream(stream_t *s, const char *host, int port);
extern int  parseHostPort(const char *str, char **host, int *port);
extern int  setSocketBuffer(int fd, int bufsize);

#endif
