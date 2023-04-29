#include "tcp.h"
#include "tty.h"

#include <mrb.h>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>


struct state {
    const char *hostname;
    const char *port;

    struct tcpconn conn;
    mrb_t inbuff;
    mrb_t outbuff;
};


#undef CSTATE
#undef CCORO
#undef CNAME
#undef CARROW_H

#define CSTATE   struct state
#define CCORO    tcpc
#define CNAME(n) tcpc_ ## n
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


struct tcpc 
errorA(struct tcpc *self, struct state *state, int no) {
    struct tcpconn *conn = &(state->conn);
    
    tcpc_nowait(STDIN_FILENO);
    tcpc_nowait(STDOUT_FILENO);
    if (conn->fd != -1) {
        tcpc_nowait(conn->fd);
    }
    
    if (conn->fd > 2) {
        close(conn->fd);
    }

    errno = 0;
    return tcpc_stop();
}


ssize_t
writeA(struct mrb *b, int fd, size_t count) {
    int res = 0;
    int bytes;

    if (ev.fd != fd || !(ev.op & EVOUT)) {
        return 0;
    }

    while (count) {
        bytes = mrb_writeout(b, fd, count);
        if (bytes <= 0) {
            break;
        }
        res += bytes;
        count -= bytes;
    }
    
    if ((bytes == -1) && (!EVMUSTWAIT())) {
        return -1;
    }

    return res;
}


ssize_t
readA(struct mrb *b, int fd, size_t count) {
    int res = 0;
    int bytes;

    if (ev.fd != fd || !(ev.op & EVIN)) {
        return 0;
    }

    while (count) {
        bytes = mrb_readin(b, fd, count);
        if (bytes <= 0) {
            break;
        }
        res += bytes;
        count -= bytes;
    }
    
    if ((bytes == -1) && (!EVMUSTWAIT())) {
        return -1;
    }

    return res;
}


struct tcpc 
ioA(struct tcpc *self, struct state *state) {
    ssize_t bytes;
    struct mrb *in = state->inbuff;
    struct mrb *out = state->outbuff;
    struct tcpconn *conn = &(state->conn);
    int outavail = mrb_available(out);
    int outused = mrb_used(out);
    int inavail = mrb_available(out);
    int inused = mrb_used(in);
 
    /* stdin read */
    bytes = readA(out, STDIN_FILENO, outavail);
    if (bytes == -1) {
        return tcpc_reject(self, state, DBG, "read(%d)", ev.fd);
    }
    outavail -= bytes;
    outused += bytes;

    /* stdout write */
    bytes = writeA(in, STDOUT_FILENO, inused);
    if (bytes == -1) {
        return tcpc_reject(self, state, DBG, "write(%d)", ev.fd);
    }
    inused -= bytes;
    inavail += bytes;

    /* tcp write */
    bytes = writeA(out, conn->fd, outused);
    if (bytes == -1) {
        return tcpc_reject(self, state, DBG, "writeead(%d)", ev.fd);
    }
    outused -= bytes;
    outavail += bytes;

    /* tcp read */
    bytes = readA(in, conn->fd, inavail);
    if (bytes == -1) {
        return tcpc_reject(self, state, DBG, "read(%d)", conn->fd);
    }
    inavail -= bytes;
    inused += bytes;

    /* reset errno and rearm events*/
    errno = 0;
    int op;

    /* stdin */
    if (outavail && tcpc_wait(self, state, &ev, STDIN_FILENO, 
                EVIN | EVONESHOT | EVET)) {
        return tcpc_reject(self, state, DBG, "wait(%d)", ev.fd);
    }

    /* stdout */
    if (inused && tcpc_wait(self, state, &ev, STDOUT_FILENO, 
                EVOUT | EVONESHOT | EVET)) {
        return tcpc_reject(self, state, DBG, "wait(%d)", ev.fd);
    }

    /* tcp socket */
    if (outused || inavail) {
        op = EVONESHOT | EVET;
        if (inavail) {
            op |= EVIN;
        }

        if (outused) {
            op |= EVOUT;
        }

        if (tcpc_wait(self, state, &ev, conn->fd, op)) {
            return tcpc_reject(self, state, DBG, "wait(%d)", ev.fd);
        }
    }
    
    return tcpc_stop();
}


struct tcpc 
connectA(struct tcpc *self, struct state *state) {
    struct tcpconn *conn = &(state->conn);
    // static struct event ev = {
    //     .fd = -1,
    //     .op = EVIN,
    // };

    if (tcp_connect(conn, state->hostname, state->port)) {
        goto failed;
    }

    if (errno == EINPROGRESS) {
        errno = 0;
        if (tcpc_wait(self, state, &ev, conn->fd, EVOUT | EVONESHOT)) {
            goto failed;
        }

        return tcpc_stop();
    }

    return tcpc_from(self, ioA);

failed:
    return tcpc_reject(self, state, DBG, "tcp_connect(%s:%s)", 
            state->hostname, state->port);
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
        .hostname = "localhost",
        .port = "3030",
    };
    
    state.inbuff = mrb_create(BUFFSIZE);
    state.outbuff = mrb_create(BUFFSIZE);
    if (state.inbuff == NULL || state.outbuff == NULL) {
        ERROR("Cannot initialized buffers.");
        return EXIT_FAILURE;
    }

    carrow_evloop_init();
    
    if (tcpc_runloop(connectA, errorA, &state, &status)) {
        ret = EXIT_FAILURE;
    }

    carrow_evloop_deinit();
    if (mrb_destroy(state.inbuff) || mrb_destroy(state.outbuff)) {
        ERROR("Cannot dispose buffers.");
        ret = EXIT_FAILURE;
    }
    return ret;
}
