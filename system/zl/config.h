#ifndef _ZL_CONFIG_H_
#define _ZL_CONFIG_H_

#ifdef PD

/* Tie ZL into mothership. */
void post(const char *msg, ...);
void startpost(const char *fmt, ...);

#if 0
#define ZL_LOG(...)    startpost("pdp: " __VA_ARGS__)
#define ZL_PERROR(...) perror("pdp: " __VA_ARGS__)
#else
#include <stdio.h>
#define ZL_LOG(...) fprintf(stderr, __VA_ARGS__)
#define ZL_PERROR ZL_LOG
#endif

#else

// #warning ZL running on plain libc

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
static inline void ZL_LOG(const char *msg, ...) {
    va_list vl;
    va_start(vl,msg);
    fprintf(stderr, "zl: ");
    vfprintf(stderr, msg, vl);
    fprintf(stderr, "\n");
    va_end(vl);
}
#define ZL_PERROR perror

#endif


#endif
