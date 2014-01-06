#ifndef _ZL_H_
#define _ZL_H_

#include <stdarg.h>

/* ZL uses the "tagged va_list" approach:

   To facilitate data transfer to a different programming system, it
   supports a generic data structure modeled after Lisp tagged lists.

   * Lists are encoded using a standard datastructure: va_lists.  This
     prevents re-invention and makes it very easy to use on both the
     sending and receiving end.

   * Tags are encoded as const char *.  This allows fast C pointer
     comparisons and makes bridging to interned symbols (hashed
     strings) common in dynamically typed languages.  */

typedef const char * zl_tag;
typedef void (*zl_receiver)(void *ctx, int nb_args, zl_tag tag, va_list args);

static inline void zl_send(zl_receiver fn, void *ctx, int nb_args, zl_tag tag, ...) {
    va_list args;
    va_start(args, tag);
    fn(ctx, nb_args, tag, args);
    va_end(args);
}

typedef unsigned int uint;

/* The <name>_p postfix is for pointers to struct <name>.  The main
   reason to use pointer typedefs is to allow for lexical tricks in
   .api files. */
typedef void* void_p;
typedef char* char_p;

#endif
