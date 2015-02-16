#pragma once

typedef struct tcpconnection tcpconnection_t;
struct tcpserver;
typedef struct tcpserver tcpserver_t;

tcpconnection_t* conn_new();
void conn_init(tcpconnection_t* conn, size_t idx, tcpserver_t* server);
void conn_close(tcpconnection_t* conn);
void conn_sendmsg(tcpconnection_t* conn, const char* msg, size_t len);
