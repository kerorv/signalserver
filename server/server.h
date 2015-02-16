#pragma once

typedef struct tcpserver tcpserver_t;

tcpserver_t* server_new();
void server_listen(tcpserver_t* server, int port);
void server_run();
void server_sendmsg(tcpserver_t* server, size_t idx, const char* msg, size_t len);
void server_closeconn(tcpserver_t* server, size_t idx);
void server_onconninit(tcpserver_t* server, size_t idx);
void server_onpacket(tcpserver_t* server, size_t idx, const char* msg, size_t len);
void server_onconnclosing(tcpserver_t* server, size_t idx);
void server_onconnclosed(tcpserver_t* server, size_t idx);
