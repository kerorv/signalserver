#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "uv/uv.h"

typedef struct client
{
	uv_tcp_t handle;
	uv_connect_t creq;
	uv_write_t wreq;
	int write_pending;
	uv_timer_t timer;
	char rbuf[1024];
	char wbuf[1024];
} client_t;

typedef struct packet
{
	uint16_t msglen;
	char msg[0];
} packet_t;

static client_t* client_new()
{
	client_t* client = (client_t*)malloc(sizeof(client_t));
	assert(client);
	
	client->write_pending = 0;
	assert(uv_tcp_init(uv_default_loop(), &client->handle) == 0);
	assert(uv_timer_init(uv_default_loop(), &client->timer) == 0);
	return client;
}

void afterwrite(uv_write_t* req, int status)
{
	client_t* client = req->data;
	client->write_pending--;

	if (status == UV_ECANCELED) {
		return;  /* Handle has been closed. */
	}

	printf("write ok.\n");
}

void ontimer(uv_timer_t* handle)
{
	client_t* client = handle->data;
	if (client->write_pending > 0)
	{
		printf("write pending...\n");
		return;
	}

	packet_t* packet = (packet_t*)client->wbuf;
	int len = sprintf_s(packet->msg, sizeof(client->wbuf) - sizeof(packet_t), "{\"type\": 1, \"now\": %u}", time(NULL));
	packet->msglen = len;

	uv_buf_t buf;
	buf.base = (char*)packet;
	buf.len = packet->msglen + sizeof(packet_t);
	
	client->wreq.data = client;
	int ret = uv_write(&client->wreq, (uv_stream_t*)&client->handle, &buf, 1, afterwrite);
	if (ret != 0)
	{
		printf("uv_write fail: %d, %s\n", ret, uv_strerror(ret));
	}
	else
	{
		client->write_pending++;
	}
}

void onallocbuffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
	client_t* client = handle->data;
	buf->base = client->rbuf;
	buf->len = sizeof(client->rbuf);
}

void onread(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
	if (nread <= 0)
	{
		if (nread < 0)
		{
			printf("onread error:%d %s\n", nread, uv_strerror(nread));
		}
		return;
	}

	packet_t* packet = (packet_t*)buf->base;
	assert(packet->msglen + sizeof(packet_t) == nread);
	packet->msg[packet->msglen] = '\0';
	printf("%s\n", packet->msg);
}

void onconnected(uv_connect_t* req, int status)
{
	if (status != 0)
	{
		printf("connect fail: %s\n", uv_strerror(status));
		return;
	}

	client_t* client = req->data;
	client->timer.data = client;
	assert(uv_timer_start(&client->timer, ontimer, 1000, 1000) == 0);
	client->handle.data = client;
	assert(uv_read_start((uv_stream_t*)&client->handle, onallocbuffer, onread) == 0);
}

static int client_connect(client_t* client, const char* ip, int port)
{
	struct sockaddr_in remote;
	assert(uv_ip4_addr(ip, port, &remote) == 0);
	client->creq.data = client;
	assert(uv_tcp_connect(&client->creq, &client->handle, (const struct sockaddr*)&remote, onconnected) == 0);
	return 0;
}

static void onclosed(uv_handle_t* handle)
{
	client_t* client = handle->data;
	free(client);
}

static void client_close(client_t* client)
{
	uv_read_stop((uv_stream_t*)&client->handle);
	uv_timer_stop(&client->timer);
	uv_close((uv_handle_t*)&client->timer, NULL);
	uv_close((uv_handle_t*)&client->handle, onclosed);
}

int main()
{
	client_t* client = client_new();
	client_connect(client, "127.0.0.1", 9900);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);

	return 0;
}
