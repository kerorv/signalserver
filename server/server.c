#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "uv/uv.h"
#include "connection.h"
#include "lsession.h"
#include "server.h"

#define MAX_TCPCONNECTION_NUM	1024

struct tcpserver
{
	uv_tcp_t handle;
	lsession_t* ls;
	size_t last_slot;
	tcpconnection_t* conns[MAX_TCPCONNECTION_NUM];
};

void onconnection(uv_stream_t* server, int status);
void ondummyclientclosed(uv_handle_t* handle);

tcpserver_t* server_new()
{
	tcpserver_t* server = (tcpserver_t*)malloc(sizeof(tcpserver_t));
	assert(server);

	assert(uv_tcp_init(uv_default_loop(), &server->handle) == 0);
	memset(server->conns, 0, sizeof(tcpconnection_t*) * MAX_TCPCONNECTION_NUM);

	server->ls = lsession_new(server);
	assert(server->ls);

	server->last_slot = 0;

	return server;
}

static size_t server_getemptyslot(tcpserver_t* server)
{
	for (size_t i = server->last_slot; i < MAX_TCPCONNECTION_NUM; ++i)
	{
		if (server->conns[i])
			continue;

		server->last_slot = i;
		return i;
	}

	for (size_t i = 0; i < server->last_slot; ++i)
	{
		if (server->conns[i])
			continue;

		server->last_slot = i;
		return i;
	}

	return (size_t)-1;
}

static void server_setconnslot(tcpserver_t* server, size_t idx, tcpconnection_t* conn)
{
	assert(idx < MAX_TCPCONNECTION_NUM);
	server->conns[idx] = conn;
}

void server_listen(tcpserver_t* server, int port)
{
	struct sockaddr_in bind_addr;
	assert(uv_tcp_init(uv_default_loop(), &server->handle) == 0);
	assert(uv_ip4_addr("0.0.0.0", port, &bind_addr) == 0);
	assert(uv_tcp_bind(&server->handle, (const struct sockaddr*)&bind_addr, 0) == 0);

	server->handle.data = server;
	assert(uv_listen((uv_stream_t*)&server->handle, 10, onconnection) == 0);
}

void server_run()
{
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

static void onconnection(uv_stream_t* stream, int status)
{
	if (status != 0)
	{
		printf("onconnection error:%d %s\n", status, uv_strerror(status));
		return;
	}

	tcpserver_t* server = stream->data;
	size_t idx = server_getemptyslot(server);
	if (idx == (size_t)-1)
	{
		// refuse
		uv_tcp_t* dummy_client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
		uv_tcp_init(uv_default_loop(), dummy_client);
		uv_accept((uv_stream_t*)&server->handle, (uv_stream_t*)dummy_client);
		uv_close((uv_handle_t*)dummy_client, ondummyclientclosed);
	}
	else
	{
		// accept
		tcpconnection_t* conn = conn_new();
		assert(conn);

		int ret = uv_accept((uv_stream_t*)&server->handle, (uv_stream_t*)conn);
		if (ret == 0)
		{
			server_setconnslot(server, idx, conn);
			conn_init(conn, idx, server);
		}
		else
		{
			printf("accept fail:%s\n", uv_strerror(ret));
			conn_close(conn);
		}
	}
}

static void ondummyclientclosed(uv_handle_t* handle)
{
	free(handle);
}

void server_sendmsg(tcpserver_t* server, size_t idx, const char* msg, size_t len)
{
	if (idx >= MAX_TCPCONNECTION_NUM)
		return;

	tcpconnection_t* conn = server->conns[idx];
	if (conn == NULL)
		return;

	conn_sendmsg(conn, msg, len);
}

void server_closeconn(tcpserver_t* server, size_t idx)
{
	if (idx >= MAX_TCPCONNECTION_NUM)
		return;

	tcpconnection_t* conn = server->conns[idx];
	if (conn == NULL)
		return;

	conn_close(conn);
}

void server_onconninit(tcpserver_t* server, size_t idx)
{
	lsession_init(server->ls, idx);
}

void server_onmessage(tcpserver_t* server, size_t idx, const char* msg, size_t len)
{
	lsession_onmessage(server->ls, idx, msg, len);
}

void server_onconnclosing(tcpserver_t* server, size_t idx)
{
	lsession_onclosing(server->ls, idx);
}

void server_onconnclosed(tcpserver_t* server, size_t idx)
{
	server->conns[idx] = NULL;
}
