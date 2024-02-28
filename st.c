#include "dwm.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <X11/Xutil.h>

static void st_kill(Display *dpy, char const *termclass, Monitor *m, int sig) {
    {
        XClassHint ch = {0};
        int i;
        Client *c;
        for (i = 0, c = m->clients; c; c = c->next, i++)
            if (ISVISIBLE(c)) {
                if (!XGetClassHint(dpy, c->win, &ch)) {
                    WARN("Could not get class hint for the window %lu of client %s", c->win, c->name);
                    return;
                }
                if (!strcmp(termclass, ch.res_class))
                    kill(c->pid, sig);
                else
                    XFree(ch.res_class);
                XFree(ch.res_name);
            }
    }
}

void st_make_opaque_(Display *dpy, char const *termclass, Monitor *m) {
    st_kill(dpy, termclass, m, SIGUSR1);
}

void st_make_transparent_(Display *dpy, char const *termclass, Monitor *m) {
    st_kill(dpy, termclass, m, SIGUSR2);
}
