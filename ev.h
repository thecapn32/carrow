#ifndef CARROW_EV_H
#define CARROW_EV_H


#include "core.h"

#include <sys/epoll.h>


#define EVMUSTWAIT() ((errno == EAGAIN) || (errno == EWOULDBLOCK))
#define EVIN    EPOLLIN
#define EVOUT   EPOLLOUT
#define EVET    EPOLLET


struct evglobalstate;


struct evpriv {
    int epollflags;
};


struct evstate {
    int fd;
    int events;
};


struct elementA *
evinitA(struct circuitA *c, struct evstate *s, struct evpriv *priv);


void 
evdeinitA();


void 
evcloseA(struct circuitA *c, struct evstate *s, struct evpriv *priv);


struct elementA * 
evwaitA(struct circuitA *c, struct evstate *s, int fd, int op);


int
evloop(volatile int *status);


#endif
