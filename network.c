/*
 *  Copyright (C) 2000-2025, Thomas Maier-Komor
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

#include "mbconf.h"

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif


#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "dest.h"
#include "globals.h"
#include "mux_proto.h"
#include "network.h"
#include "settings.h"
#include "log.h"

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif


static void openNetworkInput(const char *host, const char *portstr);


int32_t TCPBufSize = 0;
double TCPTimeout = 100;
#if defined(AF_INET6) && defined(AF_UNSPEC)
int AddrFam = AF_UNSPEC;
#else
int AddrFam = AF_INET;
#endif


static void setTCPBufferSize(int sock, int buffer)
{
	int err;
	int32_t osize, size;
	socklen_t bsize = (socklen_t)sizeof(osize);

	assert(buffer == SO_RCVBUF || buffer == SO_SNDBUF);
	err = getsockopt(sock,SOL_SOCKET,buffer,&osize,&bsize);
	assert((err == 0) && (bsize == sizeof(osize)));
	if (osize < TCPBufSize) {
		size = TCPBufSize;
		assert(size > 0);
		do {
			err = setsockopt(sock,SOL_SOCKET,buffer,(void *)&size,sizeof(size));
			size >>= 1;
		} while ((-1 == err) && (errno == ENOMEM) && (size > osize));
		if (err == -1) {
			warningmsg("unable to set socket buffer size: %s\n",strerror(errno));
			return;
		}
	}
	bsize = sizeof(size);
	err = getsockopt(sock,SOL_SOCKET,buffer,&size,&bsize);
	assert(err != -1);
	if (buffer == SO_RCVBUF) 
		infomsg("set TCP receive buffer size to %d\n",size);
	else
		infomsg("set TCP send buffer size to %d\n",size);
}


static int16_t getServicePort(const char *servstr)
{
	char *e;
	long l = strtol(servstr,&e,0);
	uint16_t port = 0;
	if (0 != *e) {
		struct servent *s = getservbyname(servstr,"tcp");
		if (0 == s)
			fatal("Cannot resolve service '%s'.\n",servstr);
		port = s->s_port;
	} else if ((l < 0) || (l > UINT16_MAX)) {
		fatal("Invalid port number %ld.\n",l);
	} else {
		port = l;
	}
	return port;
}


void initNetworkInput(const char *addr)
{
	char *host, *portstr;
	int l;

	debugmsg("initNetworkInput(\"%s\")\n",addr);
	if (Infile != 0)
		fatal("cannot initialize input from network - input from file already set\n");
	if (In != -1)
		fatal("cannot initialize input from network - input already set\n");
	l = strlen(addr) + 1;
	host = alloca(l);
	memcpy(host,addr,l);
	portstr = strrchr(host,':');
	if (portstr == 0) {
		portstr = host;
		host = 0;
	} else if (portstr == host) {
		portstr = host + 1;
		host = 0;
	} else {
		if ((host[0] == '[') && (portstr[-1] == ']')) {
			++host;
			portstr[-1] = 0;
		}
		*portstr = 0;
		++portstr;
	}
	openNetworkInput(host,portstr);
}

static const char *addrinfo2str(const struct addrinfo *ai, char *buf, size_t s)
{
	char *at = buf;
	struct protoent *pent = getprotobynumber(ai->ai_protocol);
	if (ai->ai_family == AF_INET) {
		memcpy(at,"IPv4/",5);
		at += 5;
	} else if (ai->ai_family == AF_INET6) {
		memcpy(at,"IPv6/",5);
		at += 5;
	}
	if (pent && pent->p_name) {
		size_t l = strlen(pent->p_name);
		memcpy(at,pent->p_name,l);
		at[l] = '/';
		at += l + 1;
	}
	if (getnameinfo(ai->ai_addr,ai->ai_addrlen,at,s-(at-buf),0,0,NI_NOFQDN))
		strcpy(at,"<error>");
	return buf;
}


static void openNetworkInput(const char *host, const char *portstr)
{
	struct hostent *h = 0, *r = 0;
	const int reuse_addr = 1;
	int sock;

	uint16_t port = getServicePort(portstr);
	debugmsg("openNetworkInput(\"%s\",%hu)\n",host?host:"<null>",port);
	int family = AddrFam == AF_UNSPEC ? AF_INET6 : AddrFam;
	sock = socket(family, SOCK_STREAM, IPPROTO_TCP);
	if (0 > sock)
		fatal("could not create socket for network input: %s\n",strerror(errno));
	if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)))
		warningmsg("cannot set socket to reuse address: %s\n",strerror(errno));
#ifdef IPV6_V6ONLY
	if (AF_INET6 == AddrFam) {
		const int ipv6_only = 1;
		if (-1 == setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof (ipv6_only)))
			warningmsg("cannot set socket to IPv6 only mode: %s\n",strerror(errno));
		else
			infomsg("input set to IPv6 only\n");
	} else if (AF_UNSPEC == AddrFam) {
		const int ipv6_only = 0;
		if (-1 == setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof (ipv6_only)))
			warningmsg("cannot set socket to combined IPv4/IPv6 mode: %s\n",strerror(errno));
		else
			infomsg("input set to IPv4/IPv6 mode\n");
	}
#else
	if (AF_INET6 == AddrFam) {
		warningmsg("socket option for setting IPv6 only mode is not available\n");
	} else if (AF_UNSPEC == AddrFam) {
		warningmsg("socket option for setting shared IPv4/IPv6 mode is not available\n");
	}
#endif // IPV6_V6ONLY
	setTCPBufferSize(sock,SO_RCVBUF);
	if (host) {
		debugmsg("resolving hostname '%s' of input...\n",host);
		h = gethostbyname(host);
		if (0 == h)
#ifdef HAVE_HSTRERROR
			fatal("could not resolve server hostname: %s\n",hstrerror(h_errno));
#else
			fatal("could not resolve server hostname: error code %d\n",h_errno);
#endif

	}
	if (AF_INET6 == family) {
		struct sockaddr_in6 saddr;
		bzero((void *) &saddr, sizeof(saddr));
		saddr.sin6_family = AF_INET6;
		saddr.sin6_port = htons(port);
		debugmsg("binding socket to port %d...\n",port);
		if (0 > bind(sock, (struct sockaddr *) &saddr, sizeof(saddr)))
			fatal("could not bind to ipv6 socket for network input: %s\n",strerror(errno));
	} else {
		struct sockaddr_in saddr;
		bzero((void *) &saddr, sizeof(saddr));
		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(port);
		debugmsg("binding socket to port %d...\n",port);
		if (0 > bind(sock, (struct sockaddr *) &saddr, sizeof(saddr)))
			fatal("could not bind to ipv4 socket for network input: %s\n",strerror(errno));
	}
	debugmsg("listening on socket...\n");
	if (0 > listen(sock,1))		/* accept only 1 incoming connection */
		fatal("could not listen on socket for network input: %s\n",strerror(errno));
	char peer[128];
	for (;;) {
		char addrbuf[sizeof(struct sockaddr_in6)];
		socklen_t clen = sizeof(addrbuf);
		int expected = 0;
		debugmsg("waiting to accept connection...\n");
		In = accept(sock, (struct sockaddr *)addrbuf, &clen);
		if (0 > In)
			fatal("could not accept connection for network input: %s\n",strerror(errno));
		int af,inlen;
		struct in_addr *caddr;
		if (sizeof(struct sockaddr_in6) == clen) {
			af = AF_INET6;
			struct sockaddr_in6 *caddr6 = (struct sockaddr_in6 *) addrbuf;
			caddr = (struct in_addr *)&caddr6->sin6_addr;
			inlen = sizeof(struct in6_addr);
		} else if (sizeof(struct sockaddr_in) == clen) {
			af = AF_INET;
			struct sockaddr_in *caddr4 = (struct sockaddr_in *) addrbuf;
			caddr = &caddr4->sin_addr;
			inlen = sizeof(struct in_addr);
		} else {
			abort();
		}
		inet_ntop(af,caddr,peer,sizeof(peer));
		if (h == 0) {
			expected = 1;	// any host is ok
		} else {
			debugmsg("checking connection from %s\n",peer);
			char **p;
			for (p = h->h_addr_list; *p; ++p) {
				char addrstr[64];
				inet_ntop(h->h_addrtype,*p,addrstr,sizeof(addrstr));
				debugmsg("checking against %s\n",addrstr);
				if (0 == memcmp(caddr,*p,h->h_length)) {
					infomsg("accepted connection from %s (%s)\n",h->h_name,addrstr);
					expected = 1;
					break;
				}
			}
		}
		if (expected) {
			break;
		} else {
			r = gethostbyaddr(caddr,inlen,af);
			if (r)
				warningmsg("rejected connection from %s (%s)\n",r->h_name,peer);
			else
				warningmsg("rejected connection from %s\n",peer);
			if (-1 == close(In))
				warningmsg("error closing rejected input: %s\n",strerror(errno));
		}
	}
	(void) close(sock);
	if (TCPTimeout) {
		struct timeval timeo;
		timeo.tv_sec = floor(TCPTimeout);
		timeo.tv_usec = (TCPTimeout-timeo.tv_sec)*1000000;
		if (-1 == setsockopt(In, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo)))
			warningmsg("cannot set socket send timeout: %s\n",strerror(errno));
		else
			infomsg("set TCP receive timeout to %usec, %uusec\n",timeo.tv_sec,timeo.tv_usec);
	} else {
		debugmsg("disabled TCP receive timeout\n");
	}
}


#ifdef HAVE_GETADDRINFO
dest_t *createNetworkOutput(const char *addr)
{
	char *host, *portstr;
	struct addrinfo hint, *ret = 0, *x;
	int err, fd = -1;
	dest_t *d;

	assert(addr);
	host = strdup(addr);
	assert(host);
	portstr = strrchr(host,':');
	if (0 == portstr)
		fatal("syntax error - target must be given in the form <host>:<port>\n");
	if ((host[0] == '[') && (portstr > host) && (portstr[-1] == ']')) {
		++host;
		portstr[-1] = 0;
	}
	*portstr++ = 0;
	bzero(&hint,sizeof(hint));
	hint.ai_family = AddrFam;
	hint.ai_protocol = IPPROTO_TCP;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_flags = AI_ADDRCONFIG;
	debugmsg("getting address info for host %s, port %s\n",host,portstr);
	err = getaddrinfo(host,portstr,&hint,&ret);
	if ((err != 0) || (ret == 0))
		errormsg("unable to resolve address information for '%s': %s\n",addr,gai_strerror(err));
	for (x = ret; x; x = x->ai_next) {
		fd = socket(x->ai_family, SOCK_STREAM, 0);
		if (fd == -1) {
			errormsg("unable to create output socket: %s\n",strerror(errno));
			continue;
		}
		char addrstr[64+IF_NAMESIZE];
		void *src = AF_INET == x->ai_family ? (void*)(&((struct sockaddr_in *)x->ai_addr)->sin_addr) : (void*)(&((struct sockaddr_in6 *)x->ai_addr)->sin6_addr);
		inet_ntop(x->ai_family,src,addrstr,sizeof(addrstr));
		debugmsg("connecting to %s\n",addrstr);
		if (0 == connect(fd, x->ai_addr, x->ai_addrlen)) {
			struct sockaddr_in6 laddr;
			socklen_t as = sizeof(laddr);
			getsockname(fd, (struct sockaddr *) &laddr, &as);
			uint16_t rport = 0;
			if (sizeof(struct sockaddr_in6) == as) {
				rport = laddr.sin6_port;
			} else if (sizeof(struct sockaddr_in) == as) {
				rport = ((struct sockaddr_in *)&laddr)->sin_port;
			} else {
				abort();
			}
			infomsg("successfully connected to %s from :%d\n",addr,ntohs(rport));
			break;
		}
		addrinfo2str(x,addrstr,sizeof(addrstr));
		warningmsg("error connecting to %s: %s\n",addrstr,strerror(errno));
		(void) close(fd);
		fd = -1;
	}
	if (fd == -1) {
		errormsg("unable to connect to %s\n",addr);
		host = 0;	// tag as start failed
	} else {
		if (TCPBufSize)
			setTCPBufferSize(fd,SO_SNDBUF);
		if (TCPTimeout) {
			struct timeval timeo;
			timeo.tv_sec = floor(TCPTimeout);
			timeo.tv_usec = (TCPTimeout-timeo.tv_sec)*1E6;
			if (-1 == setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo)))
				warningmsg("cannot set socket send timeout: %s\n",strerror(errno));
		} else {
			debugmsg("disabled TCP send timeout\n");
		}
	}
	d = (dest_t *) malloc(sizeof(dest_t));
	d->arg = addr;
	d->name = host;
	d->port = portstr;
	d->fd = fd;
	bzero(&d->thread,sizeof(d->thread));
	d->result = 0;
	d->next = 0;
	return d;
}


#else	/* HAVE_GETADDRINFO */
// deprecated!

static void openNetworkOutput(dest_t *dest)
{
	debugmsg("creating socket for output to %s:%s...\n",dest->name,dest->port);
	uint16_t pnr = getServicePort(dest->port);
	const int family = AddrFam != AF_UNSPEC ? AddrFam : (strchr(dest->name,':') ? AF_INET6 : AF_INET);
	int out = socket(family, SOCK_STREAM, 0);
	if (0 > out) {
		errormsg("could not create socket for network output: %s\n",strerror(errno));
		return;
	}
	setTCPBufferSize(out,SO_SNDBUF);
	struct sockaddr_in6 saddr;
	bzero((void *) &saddr, sizeof(saddr));
	saddr.sin6_port = htons(pnr);
	if (((dest->name[0] >= '0') && (dest->name[0] <= '9')) || (0 != strchr(dest->name,':')) || ('[' == dest->name[0])) {
		infomsg("translate address %s...\n",dest->name);
		saddr.sin6_family = family;
		int a = inet_pton(saddr.sin6_family,dest->name,&saddr.sin6_addr);
		if (a != 1) {
			dest->result = "unable to translate address";
			errormsg("unable to translate address %s\n",dest->name);
			dest->fd = -1;
			dest->name = 0;		// tag as start failed
			(void) close(out);
			return;
		}
	} else {
		infomsg("resolving host %s...\n",dest->name);
		struct hostent *h = gethostbyname(dest->name);
		if (0 == h) {
#ifdef HAVE_HSTRERROR
			dest->result = hstrerror(h_errno);
			errormsg("could not resolve hostname %s: %s\n",dest->name,dest->result);
#else
			dest->result = "unable to resolve hostname";
			errormsg("could not resolve hostname %s: error code %d\n",dest->name,h_errno);
#endif
			dest->fd = -1;
			dest->name = 0;		// tag as start failed
			(void) close(out);
			return;
		}
		saddr.sin6_family = h->h_addrtype;
		assert(h->h_length <= sizeof(saddr.sin6_addr));
		(void) memcpy(&saddr.sin6_addr,h->h_addr_list[0],h->h_length);
	}
	char addr[INET6_ADDRSTRLEN];
	inet_ntop(saddr.sin6_family,&saddr.sin6_addr,addr,sizeof(addr));
	infomsg("connecting to server at %s...\n",addr);
	if (0 > connect(out, (struct sockaddr *) &saddr, sizeof(saddr))) {
		dest->result = strerror(errno);
		errormsg("could not connect to %s:%s: %s\n",dest->name,dest->port,dest->result);
		(void) close(out);
		out = -1;
	} else if (TCPTimeout) {
		struct timeval timeo;
		timeo.tv_sec = floor(TCPTimeout);
		timeo.tv_usec = (TCPTimeout-timeo.tv_sec)*1000000;
		if (-1 == setsockopt(out, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo)))
			warningmsg("cannot set socket send timeout: %s\n",strerror(errno));
		else
			infomsg("set TCP transmit timeout to %usec, %uusec\n",timeo.tv_sec,timeo.tv_usec);
	} else {
		debugmsg("disabled TCP transmint timeout\n");

	}
	dest->fd = out;
}


dest_t *createNetworkOutput(const char *addr)
{
	char *host, *portstr;
	dest_t *d = (dest_t *) malloc(sizeof(dest_t));

	debugmsg("createNetworkOutput(\"%s\")\n",addr);
	host = strdup(addr);
	portstr = strrchr(host,':');
	if ((portstr == 0) || (portstr == host))
		fatal("argument '%s' doesn't match <host>:<port> format\n",addr);
	*portstr = 0;
	if (('[' == host[0]) && (']' == portstr[-1])) {
		portstr[-1] = 0;
		++host;
	}
	++portstr;
	bzero(d, sizeof(dest_t));
	d->fd = -1;
	d->arg = addr;
	d->name = host;
	d->port = portstr;
	d->result = 0;
	openNetworkOutput(d);
	return d;
}


#endif /* HAVE_GETADDRINFO */



int parseHostPort(const char *str, char **host, int *port)
{
	const char *colon = strrchr(str, ':');
	if (colon) {
		*port = atoi(colon + 1);
		size_t hlen = (size_t)(colon - str);
		if (hlen > 0) {
			*host = strndup(str, hlen);
		} else {
			*host = NULL;
		}
	} else {
		*host = strdup(str);
		*port = 0;
	}
	return 0;
}

static int create_tcp_socket(void)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		errormsg("socket: %s\n", strerror(errno));
		return -1;
	}
	int flag = 1;
	(void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
	return fd;
}

int setSocketBuffer(int fd, int bufsize)
{
	(void)setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
	(void)setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
	return 0;
}

int createControlConnection(const char *host, int port)
{
	int fd = create_tcp_socket();
	struct hostent *he;
	struct sockaddr_in addr;

	if (fd < 0)
		return -1;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	he = gethostbyname(host);
	if (0 == he) {
#ifdef HAVE_HSTRERROR
		errormsg("gethostbyname(%s): %s\n", host, hstrerror(h_errno));
#else
		errormsg("gethostbyname(%s): error %d\n", host, h_errno);
#endif
		close(fd);
		return -1;
	}
	(void)memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
	if (0 > connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
		errormsg("connect control %s:%d: %s\n", host, port, strerror(errno));
		close(fd);
		return -1;
	}
	setSocketBuffer(fd, 1048576);
	return fd;
}

int bindControlListen(int port)
{
	int fd = create_tcp_socket();
	const int reuse_addr = 1;
	struct sockaddr_in addr;

	if (fd < 0)
		return -1;
	(void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (0 > bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
		errormsg("bind control %d: %s\n", port, strerror(errno));
		close(fd);
		return -1;
	}
	if (0 > listen(fd, 5)) {
		errormsg("listen control %d: %s\n", port, strerror(errno));
		close(fd);
		return -1;
	}
	return fd;
}

int createDataConnection(const char *host, int port)
{
	int fd = create_tcp_socket();
	struct hostent *he;
	struct sockaddr_in addr;

	if (fd < 0)
		return -1;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	he = gethostbyname(host);
	if (0 == he) {
		close(fd);
		return -1;
	}
	(void)memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
	if (0 > connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
		close(fd);
		return -1;
	}
	setSocketBuffer(fd, 1048576);
	return fd;
}

int bindDataListen(int port)
{
	int fd = create_tcp_socket();
	const int reuse_addr = 1;
	struct sockaddr_in addr;

	if (fd < 0)
		return -1;
	(void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (0 > bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
		close(fd);
		return -1;
	}
	if (0 > listen(fd, 1)) {
		close(fd);
		return -1;
	}
	return fd;
}

int reconnectStream(stream_t *s, const char *host, int port)
{
	int fd;

	if (s->fd >= 0)
		close(s->fd);
	s->state = STREAM_CONNECTING;
	fd = createDataConnection(host, port);
	if (fd < 0) {
		s->state = STREAM_DEAD;
		return -1;
	}
	s->fd = fd;
	s->state = STREAM_ACTIVE;
	return 0;
}

/* vim:tw=0
 */
