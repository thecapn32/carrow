#include "tcp.h"
#include "tty.h"

#include <mrb.h>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>


/* TCP server carrow types and function */
struct state {
    int listenfd;

    const char *bindaddr;
    unsigned short bindport;
};


#undef CSTATE
#undef CCORO
#undef CNAME
#undef CARROW_H

#define CSTATE   state
#define CCORO    tcpsc
#define CNAME(n) tcps_ ## n
#include "carrow.h"
#include "carrow.c"


/* TCP connection carrow types and function */
struct connstate {
    struct tcpconn conn;
    mrb_t inbuff;
    mrb_t outbuff;
    struct event ev;
};


#undef CSTATE
#undef CCORO
#undef CNAME
#undef CARROW_H

#define CSTATE   connstate
#define CCORO    tcpcc
#define CNAME(n) tcpcc_ ## n
#include "carrow.h"
#include "carrow.c"


#define PAGESIZE 4096
#define BUFFSIZE (PAGESIZE * 32768)


#define WORKING 99999999
volatile int status = WORKING;
static struct sigaction old_action;
static struct event ev = {
    .fd = STDIN_FILENO,
    .op = EVIN,
};


struct tcpcc 
connerrorA(struct tcpcc *self, struct connstate *state, int no) {
    return tcpcc_stop();
}


struct tcpcc 
echoA(struct tcpcc *self, struct connstate *state) {
    // if (fd == -1) {
    //     if (EVMUSTWAIT()) {
    //         errno = 0;
    //         if (tcpcc_arm(&echo, c, &(c->ev), state->listenfd, EVIN)) {
    //             return tcps_reject(self, state, DBG, "tcpcc_arm");
    //         }
    //     }
    //     return tcps_reject(self, state, DBG, "accept4");
    // }

    return tcpcc_stop();
}


struct tcpsc 
errorA(struct tcpsc *self, struct state *state, int no) {
    return tcps_stop();
}


struct tcpsc
acceptA(struct tcpsc *self, struct state *state) {
    DEBUG("New conn");
    int fd;
    socklen_t addrlen = sizeof(struct sockaddr);
    struct connstate *c = malloc(sizeof(struct connstate));
    if (c == NULL) {
        return tcps_reject(self, state, DBG, "Out of memory");
    }

    fd = accept4(state->listenfd, &(c->conn.remoteaddr), &addrlen, 
            SOCK_NONBLOCK);
    if (fd == -1) {
        if (EVMUSTWAIT()) {
            errno = 0;
            if (tcps_arm(self, state, &ev, state->listenfd, EVIN | EVET)) {
                return tcps_reject(self, state, DBG, "tcpcc_arm");
            }
            return tcps_stop();
        }
        return tcps_reject(self, state, DBG, "accept4");
    }

    c->conn.fd = fd;
    static struct tcpcc echo = {echoA, connerrorA};
    if (tcpcc_arm(&echo, c, &(c->ev), c->conn.fd, 
                EVIN | EVOUT | EVONESHOT)) {
        return tcps_reject(self, state, DBG, "tcpcc_arm");
    }
}


struct tcpsc 
listenA(struct tcpsc *self, struct state *state) {
    int fd;

    fd = tcp_listen(state->bindaddr, state->bindport);
    if (fd < 0) {
        goto failed;
    }
    
    state->listenfd = fd;
    return tcps_from(self, acceptA);

failed:
    return tcps_reject(self, state, DBG, "tcp_listen(%s:%u)", 
            state->bindaddr, state->bindport);
}


void sighandler(int s) {
    PRINTE(CR);
    status = EXIT_SUCCESS;
}


void catch_signal() {
    struct sigaction new_action = {sighandler, 0, 0, 0, 0};
    if (sigaction(SIGINT, &new_action, &old_action) != 0) {
        FATAL("sigaction");
    }
}


int
main() {
    int ret = EXIT_SUCCESS;
    clog_verbosity = CLOG_DEBUG;

    /* Signal */
    catch_signal();

    /* Non blocking starndard input/output */
    if (stdin_nonblock() || stdout_nonblock()) {
        return EXIT_FAILURE;
    }

    struct state state = {
        .bindaddr = "0.0.0.0",
        .bindport = 3030,
        .listenfd = -1,
    };
    
    carrow_evloop_init();
    if (tcps_runloop(listenA, errorA, &state, &status)) {
        ret = EXIT_FAILURE;
    }

    carrow_evloop_deinit();
    return ret;
}
