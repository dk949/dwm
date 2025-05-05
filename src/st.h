#ifndef ST_H
#define ST_H

#include <X11/Xlib.h>
struct Monitor;

#ifdef ST_INTEGRATION
void st_make_opaque_(Display *dpy, char const *termclass, struct Monitor *m);
void st_make_transparent_(Display *dpy, char const *termclass, struct Monitor *m);
#    define st_make_opaque(dpy, termclass, m)      st_make_opaque_(dpy, termclass, m)
#    define st_make_transparent(dpy, termclass, m) st_make_transparent_(dpy, termclass, m)
#else
// NOTE: casting termclass to void here (instead of (void)0) because it is otherwise unused
#    define st_make_opaque(dpy, termclass, m)      (void)termclass
#    define st_make_transparent(dpy, termclass, m) (void)termclass
#endif

#endif  // ST_H
