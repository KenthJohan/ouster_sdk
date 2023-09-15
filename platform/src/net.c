#define _POSIX_C_SOURCE 200112L

#include "platform/net.h"
#include "platform/log.h"
#include "platform/assert.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

int32_t net_get_port(int sock)
{
	platform_assert(sock >= 0, "Socket not in range");

	struct sockaddr_storage ss;
	socklen_t addrlen = sizeof(ss);
	getsockname(sock, (struct sockaddr *)&ss, &addrlen);
	in_port_t port;
	switch (ss.ss_family)
	{
	case AF_INET:
		port = ((struct sockaddr_in *)&ss)->sin_port;
		break;
	case AF_INET6:
		port = ((struct sockaddr_in6 *)&ss)->sin6_port;
		break;
	}
	return ntohs(port);
}

void inet_ntop_addrinfo(struct addrinfo *ai, char *buf, socklen_t len)
{
	platform_assert_notnull(ai);

	void *addr = NULL;
	switch (ai->ai_family)
	{
	case AF_INET:
		addr = &(((struct sockaddr_in *)ai->ai_addr)->sin_addr);
		break;
	case AF_INET6:
		addr = &(((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr);
		break;
	}
	inet_ntop(ai->ai_family, addr, buf, len);
}

int try_create_socket(net_sock_desc_t *desc, struct addrinfo *ai)
{
	platform_assert_notnull(desc);
	platform_assert_notnull(ai);

	int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (s < 0)
	{
		platform_log("socket(): error\n");
		goto error;
	}

	if (desc->flags & NET_FLAGS_CONNECT)
	{
		int rc = connect(s, ai->ai_addr, (socklen_t)ai->ai_addrlen);
		if (rc)
		{
			platform_log("connect(): error: %s\n", strerror(errno));
			goto error;
		}
	}

	if (desc->rcvtimeout_sec)
	{
		struct timeval tv;
		tv.tv_sec = desc->rcvtimeout_sec;
		tv.tv_usec = 0;
		int rc = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
		if (rc == -1)
		{
			platform_log("fcntl(): error\n");
			goto error;
		}
	}

	if (desc->flags & NET_FLAGS_IPV6ONLY)
	{
		int off = 0;
		int rc = setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&off, sizeof(off));
		if (rc)
		{
			platform_log("setsockopt(): error\n");
			goto error;
		}
	}

	if (desc->flags & NET_FLAGS_REUSE)
	{
		int option = 1;
		int rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option));
		if (rc)
		{
			platform_log("setsockopt(): error\n");
			goto error;
		}
	}

	if (desc->flags & NET_FLAGS_BIND)
	{

		int rc = bind(s, ai->ai_addr, (socklen_t)ai->ai_addrlen);
		if (rc)
		{
			platform_log("bind(): error: %s\n", strerror(errno));
			goto error;
		}
	}

	// https://gist.github.com/hostilefork/f7cae3dc33e7416f2dd25a402857b6c6
	if (desc->group)
	{
		int rc;
		struct ip_mreq mreq; // IPv4
		rc = inet_pton(AF_INET, desc->group, &(mreq.imr_multiaddr.s_addr));
		if (rc != 1)
		{
			platform_log("inet_pton(): error\n");
			goto error;
		}
		if (IN_MULTICAST(ntohl(mreq.imr_multiaddr.s_addr)) == 0)
		{
			platform_log("Not multicast\n");
			goto error;
		}
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		rc = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
		if (rc < 0)
		{
			platform_log("setsockopt(): error\n");
			goto error;
		}
		platform_log("IP_ADD_MEMBERSHIP(): %s\n", desc->group);
	}

	if (desc->flags & NET_FLAGS_NONBLOCK)
	{
		int flags = fcntl(s, F_GETFL, 0);
		if (flags == -1)
		{
			platform_log("fcntl(): error\n");
			goto error;
		}
		int rc = fcntl(s, F_SETFL, flags | O_NONBLOCK);
		if (rc == -1)
		{
			platform_log("fcntl(): error\n");
			goto error;
		}
	}
	if (desc->rcvbuf_size)
	{
		int rc = setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&desc->rcvbuf_size, sizeof(desc->rcvbuf_size));
		if (rc)
		{
			platform_log("setsockopt(): error\n");
			goto error;
		}
	}
	return s;
error:
	if (s >= 0)
	{
		close(s);
	}
	return -1;
}

struct addrinfo *get_addrinfo(net_sock_desc_t *desc)
{
	platform_assert_notnull(desc);

	struct addrinfo *info = NULL;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	if (desc->flags & NET_FLAGS_IPV4)
	{
		hints.ai_family = AF_INET;
	}
	if (desc->flags & NET_FLAGS_IPV6)
	{
		hints.ai_family = AF_INET6;
	}
	if (desc->flags & NET_FLAGS_UDP)
	{
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_PASSIVE;
	}
	if (desc->flags & NET_FLAGS_TCP)
	{
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_NUMERICHOST;
	}
	int ret = getaddrinfo(desc->hint_name, desc->hint_service, &hints, &info);
	if (ret != 0)
	{
		hints.ai_flags = 0;
		ret = getaddrinfo(desc->hint_name, desc->hint_service, &hints, &info);
		if (ret != 0)
		{
			platform_log("getaddrinfo(): %s\n", gai_strerror(ret));
			goto error;
		}
	}
	if (info == NULL)
	{
		platform_log("getaddrinfo(): empty result\n");
		goto error;
	}
	return info;

error:
	if (info)
	{
		platform_log("freeaddrinfo()\n");
		freeaddrinfo(info);
	}
	return NULL;
}

int net_create(net_sock_desc_t *desc)
{
	platform_assert_notnull(desc);

	struct addrinfo *info = get_addrinfo(desc);
	if (info == NULL)
	{
		platform_log("get_addrinfo(): error\n");
		goto error;
	}
	struct addrinfo *ai;
	int s = -1;
	for (ai = info; ai != NULL; ai = ai->ai_next)
	{
		char buf[INET6_ADDRSTRLEN];
		inet_ntop_addrinfo(ai, buf, INET6_ADDRSTRLEN);
		platform_log("get_addrinfo: %s\n", buf);
	}
	for (ai = info; ai != NULL; ai = ai->ai_next)
	{
		char buf[INET6_ADDRSTRLEN];
		inet_ntop_addrinfo(ai, buf, INET6_ADDRSTRLEN);
		s = try_create_socket(desc, ai);
		platform_log("try_create_socket: %s:%i %s%s, socket=%i\n", buf, net_get_port(s),
					 (desc->flags & NET_FLAGS_TCP) ? "TCP" : "",
					 (desc->flags & NET_FLAGS_UDP) ? "UDP" : "",
					 s);
		if (s >= 0)
		{
			break;
		}
	}

	if (s < 0)
	{
		platform_log("try_create_socket(): error\n");
		goto error;
	}

	freeaddrinfo(info);
	return s;

error:
	if (info)
	{
		platform_log("freeaddrinfo()\n");
		freeaddrinfo(info);
	}
	if (s >= 0)
	{
		platform_log("close() socket\n");
		close(s);
	}
	return -1;
}

int64_t net_read(int sock, char *buf, int len)
{
	platform_assert(sock >= 0, "");
	platform_assert_notnull(buf);

	int64_t bytes_read = recv(sock, (char *)buf, len, 0);
	return bytes_read;
}

/*
https://stackoverflow.com/questions/5647503/with-a-single-file-descriptor-is-there-any-performance-difference-between-selec
*/
uint64_t net_select(int socks[], int n, const int timeout_sec, const int timeout_usec)
{
	platform_assert_notnull(socks);

	fd_set rfds;
	FD_ZERO(&rfds);
	for (int i = 0; i < n; ++i)
	{
		platform_assert(socks[i] >= 0, "Socket not in range");
		FD_SET(socks[i], &rfds);
	}

	int max = 0;
	for (int i = 0; i < n; ++i)
	{
		if (socks[i] > max)
		{
			max = socks[i];
		}
	}

	uint64_t result = 0;
	struct timeval tv;
	tv.tv_sec = timeout_sec;
	tv.tv_usec = timeout_usec;

	int rc = select((int)max + 1, &rfds, NULL, NULL, &tv);
	if (rc == -1)
	{
		platform_log("select(): error\n");
		return 0;
	}
	for (int i = 0; i < n; ++i)
	{
		if (FD_ISSET(socks[i], &rfds))
		{
			result |= (1 << i);
		}
	}

	return result;
}
