#ifndef CARROW_CORE_H
#define CARROW_CORE_H


#define OK  0
#define ERR 1
#define CARROW_ERRORMSG_BUFFSIZE    1024


struct elementA;
struct circuitA;


typedef void (*taskA) (struct circuitA *a, void *state, void *priv);
typedef void (*okA) (struct circuitA *a, void* state);
typedef void (*failA) (struct circuitA *a, void* state, const char *msg);


struct circuitA *
newA(failA errcb);


void
freeA(struct circuitA *a);


void
bindA(struct elementA *e1, struct elementA *e2);


struct elementA *
appendA(struct circuitA *c, taskA f, void *priv);


int
loopA(struct elementA *e);


void
returnA(struct circuitA *c, void *state);


void
errorA(struct circuitA *c, void *state, const char *format, ...);


void
runA(struct circuitA *c, void *state);


void *
privA(struct circuitA *c);


#endif
