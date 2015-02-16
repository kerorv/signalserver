#pragma once

typedef struct lsession lsession_t;

lsession_t* lsession_new(tcpserver_t* server);
void lsession_init(lsession_t* ls, size_t id);
void lsession_onpacket(lsession_t* ls, size_t id, const char* packet, size_t len);
void lsession_close(lsession_t* ls);
void lsession_onclosing(lsession_t* ls, size_t id);
