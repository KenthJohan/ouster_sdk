#include "ouster_clib/sock.h"
#include "ouster_clib/ouster_assert.h"
#include "ouster_clib/ouster_log.h"
#include "ouster_clib/ouster_net.h"
#include <stddef.h>

int ouster_sock_create_udp_lidar(int port)
{
	net_sock_desc_t desc = {0};
	desc.flags = NET_FLAGS_UDP | NET_FLAGS_NONBLOCK | NET_FLAGS_REUSE | NET_FLAGS_BIND;
	desc.hint_name = NULL;
	desc.rcvbuf_size = 1024 * 1024;
	desc.hint_service = NULL;
	desc.port = port;
	// desc.group = "239.201.201.201";
	// desc.group = "239.255.255.250";
	return net_create(&desc);
}

int ouster_sock_create_udp_imu(int port)
{
	net_sock_desc_t desc = {0};
	desc.flags = NET_FLAGS_UDP | NET_FLAGS_NONBLOCK | NET_FLAGS_REUSE | NET_FLAGS_BIND;
	desc.hint_name = NULL;
	desc.rcvbuf_size = 1024 * 1024;
	desc.hint_service = NULL;
	desc.port = port;
	return net_create(&desc);
}

int ouster_sock_create_tcp(char const *hint_name)
{
	ouster_assert_notnull(hint_name);

	net_sock_desc_t desc = {0};
	desc.flags = NET_FLAGS_TCP | NET_FLAGS_CONNECT;
	desc.hint_name = hint_name;
	desc.hint_service = "7501";
	desc.rcvtimeout_sec = 10;
	return net_create(&desc);
}
