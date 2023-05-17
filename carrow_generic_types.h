#ifndef CARROW_GENERIC_TYPES_H
#define CARROW_GENERIC_TYPES_H


#include <stdbool.h>


struct CCORO;


typedef struct CCORO 
    (*CNAME(resolver)) (struct CCORO *self, CSTATE *state);


typedef struct CCORO 
    (*CNAME(rejector)) (struct CCORO *self, CSTATE *state, int no);


void
CNAME(resolve) (struct CCORO *self, CSTATE *s);


#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)


#define DBG errno, __FILENAME__, __LINE__, __FUNCTION__


#endif
