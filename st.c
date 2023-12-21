#include "dwm.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <X11/Xutil.h>

void st_kill(Display *dpy, char const *termclass, Monitor *m, int sig) {
    {
        XClassHint ch = {0};
        int i;
        Client *c;
        for (i = 0, c = m->clients; c; c = c->next, i++)
            if (ISVISIBLE(c)) {
                DEBUG_PRINTF("[%d] client name = %s", i, c->name);
                if (!XGetClassHint(dpy, c->win, &ch)) {
                    WARN("Could not get class hint for a window");
                    return;
                }
                if (!strcmp(termclass, ch.res_class)) {
                    DEBUG_PRINTF("Sending signal %d to pid %d, class = %s, instance = %s",
                        sig,
                        c->pid,
                        ch.res_class,
                        ch.res_name);
                    kill(c->pid, sig);
                } else
                    DEBUG_PRINTF("Not sending signal %d to pid %d, class = %s, instance = %s",
                        sig,
                        c->pid,
                        ch.res_class,
                        ch.res_name);
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
