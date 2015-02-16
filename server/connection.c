#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "uv/uv.h"
#include "server.h"
#include "connection.h"

#define INIT_READBUF_SIZE				512
#define MAX_READBUF_SIZE				((size_t)UINT16_MAX)
#define MAX_MESSAGE_SIZE				(MAX_READBUF_SIZE-sizeof(uint16_t))
#define MAX_WRITE_PENDING_COUNT			64

typedef struct write_req
{
	uv_write_t req;
	uv_buf_t buf;
} write_req_t;

struct tcpconnection
{
	uv_tcp_t handle;
	size_t idx;
	tcpserver_t* server;
	char* readbuf;
	size_t rbufsize;
	size_t readpos;
	int write_pending_count;
};

void onallocbuffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
void onread(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
void afterwrite(uv_write_t* req, int status);
void onclosed(uv_handle_t* handle);
void onpacket(tcpconnection_t* conn, const char* packet, size_t len);
int parsestream(tcpconnection_t* conn, const char* ptr, size_t len);

tcpconnection_t* conn_new()
{
	tcpconnection_t* conn = (tcpconnection_t*)malloc(sizeof(tcpconnection_t));
	assert(conn);

	conn->idx = (size_t)(-1);
	conn->server = NULL;
	assert(uv_tcp_init(uv_default_loop(), &conn->handle) == 0);
	conn->handle.data = conn;
	conn->readbuf = (char*)malloc(INIT_READBUF_SIZE);
	assert(conn->readbuf);
	conn->rbufsize = INIT_READBUF_SIZE;
	conn->readpos = 0;
	conn->write_pending_count = 0;

	return conn;
}

void conn_init(tcpconnection_t* conn, size_t idx, tcpserver_t* server)
{
	conn->idx = idx;
	conn->server = server;

	uv_read_start((uv_stream_t*)&conn->handle, onallocbuffer, onread);

	server_onconninit(server, conn->idx);
}

void conn_close(tcpconnection_t* conn)
{
	if (uv_is_active((uv_handle_t*)&conn->handle))
	{
		uv_read_stop((uv_stream_t*)&conn->handle);
	}
	
	uv_close((uv_handle_t*)&conn->handle, onclosed);

	// has init?
	if (conn->idx != (size_t)-1)
	{
		// notify server
		server_onconnclosing(conn->server, conn->idx);
	}
}

void conn_sendmsg(tcpconnection_t* conn, const char* msg, size_t len)
{
	if (len > MAX_MESSAGE_SIZE)
	{
		return;
	}

	if (conn->write_pending_count > MAX_WRITE_PENDING_COUNT)
	{
		// max pending write overflow
		// close connection
		conn_close(conn);
		return;
	}

	write_req_t* wreq = (write_req_t*)malloc(sizeof(write_req_t));
	wreq->req.data = conn;

	size_t packet_len = len + sizeof(uint16_t);
	wreq->buf.base = (char*)malloc(packet_len);
	wreq->buf.len = packet_len;
	uint16_t* header = (uint16_t*)wreq->buf.base;
	*header = (uint16_t)packet_len;
	memcpy(wreq->buf.base + sizeof(uint16_t), msg, len);

	int ret = uv_write(&wreq->req, (uv_stream_t*)&conn->handle, &wreq->buf, 1, afterwrite);
	if (ret == 0)
	{
		conn->write_pending_count++;
	}
	else
	{
		printf("uv_write fail:%d %s\n", ret, uv_strerror(ret));
		// close connection
		conn_close(conn);
	}
}

void onallocbuffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
	tcpconnection_t* conn = handle->data;

	if (conn->readpos == conn->rbufsize)
	{
		if (conn->rbufsize == MAX_READBUF_SIZE)
		{
			printf("rbufsize is MAX_READBUF_SIZE!");
			buf->base = 0;
			buf->len = 0;
			return;
		}
		
		size_t new_capacity = conn->rbufsize * 2;
		if (new_capacity > MAX_READBUF_SIZE)
			new_capacity = MAX_READBUF_SIZE;

		char* new_buf = (char*)malloc(new_capacity);
		assert(new_buf);
		memcpy(new_buf, conn->readbuf, conn->readpos);
		free(conn->readbuf);
		conn->readbuf = new_buf;
		conn->rbufsize = new_capacity;
	}

	buf->base = conn->readbuf + conn->readpos;
	buf->len = conn->rbufsize - conn->readpos;
}

static void onread(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
	tcpconnection_t* conn = stream->data;

	if (nread <= 0)
	{
		if (nread < 0)
		{
			printf("onread error:%s\n", uv_strerror(nread));
			// close connection
			conn_close(conn);
		}

		return;
	}

	conn->readpos += nread;
	int offset = parsestream(conn, conn->readbuf, conn->readpos);
	if (offset > 0)
	{
		memmove(conn->readbuf, conn->readbuf + offset, conn->readpos);
		conn->readpos -= offset;
	}
}

static void afterwrite(uv_write_t* req, int status)
{
	write_req_t* wreq = (write_req_t*)req;
	tcpconnection_t* conn = wreq->req.data;
	assert(conn);
	conn->write_pending_count--;
	if (wreq->buf.base)
	{
		free(wreq->buf.base);
	}
	free(wreq);
}

static void onclosed(uv_handle_t* handle)
{
	tcpconnection_t* conn = handle->data;
	server_onconnclosed(conn->server, conn->idx);

	free(conn->readbuf);
	free(conn);
}

static void onpacket(tcpconnection_t* conn, const char* packet, size_t len)
{
	server_onpacket(conn->server, conn->idx, packet, len);
}

static int parsestream(tcpconnection_t* conn, const char* ptr, size_t len)
{
	if (len < sizeof(uint16_t))
		return 0;

	uint16_t msglen = *(uint16_t*)ptr;
	if (len < msglen + sizeof(uint16_t))
		return 0;

	onpacket(conn, ptr + sizeof(uint16_t), msglen);

	size_t offset = sizeof(uint16_t) + msglen;
	int suboff = parsestream(conn, ptr + offset, len - offset);
	if (suboff > 0)
		offset += suboff;
	return offset;
}
