#ifndef _ZL_PD_H_
#define _ZL_PD_H_

#include "m_pd.h"
#include "zl/zl.h"

/*

To bridge 2 programming systems, the real hurdle is in
impedance-matching the data types and memory model.

In ZL this is done by keeping a very simple basic data type: C
variable argument lists where tags are reprented using

typedef const char *zl_tag;


A. List representation
----------------------

There are essentially 2 kinds of list in C that are easy to use inside
the language:

- Static data initializers.  This requires a union (optionally tagged)
  so has some syntactic overhead.  Works for global and local
  variables.

- Variable argument lists.  This is a feature supported by the C
  standard and is very convenient to use as there is no notational
  overhead.

Often converted data is short-lived, allowing data conversion to be
implemented with nested C calls.  Both are able to side-step malloc()
when they are combined with some kind of callback mechanism.


B. List tagging
---------------

Using const char * as a tag type allows 2 things to be married in a
very elegant way:

- Pointer-based comparison in C without memory allocation overhead
  (all string memory allocation is static).

- Easy conversion to run-time interned symbols (hashed strings) for
  dynamic languages.

*/


/* To further limit complexity, for interfaacing with Pd, ZL uses
   tagged double lists, easy to convert to pd format. */
static inline void outlet_zl_float_list(t_outlet *o, int nb_double_args, zl_tag tag, va_list args) {
    t_atom pd_list[nb_double_args];
    int i;
    for (i = 0; i< nb_double_args; i++) {
        SETFLOAT(&pd_list[i], va_arg(args, double));
    }
    outlet_anything(o, gensym(tag), nb_double_args, pd_list);
}


#endif
