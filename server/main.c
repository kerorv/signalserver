#include <stdio.h>
#include "server.h"

#define LISTEN_PORT				9900

int main()
{
	tcpserver_t* server = server_new();
	server_listen(server, LISTEN_PORT);
	server_run();
	return 0;
}
