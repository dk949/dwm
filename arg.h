#ifndef ARG_H
#define ARG_H

/// Arg functions for key and mouse bindings

/// Union to pass generic data to key/mouse binding functions
typedef union Arg {
    int i;
    unsigned int ui;
    float f;
    void const *v;
} Arg;

void bright_dec(Arg const *arg);
void bright_inc(Arg const *arg);
void bright_set(Arg const *arg) __attribute__((unused));
void focusmon(Arg const *arg);
void focusstack(Arg const *arg);
void incnmaster(Arg const *arg);
void killclient(Arg const *arg);
void movemouse(Arg const *arg);
void quit(Arg const *arg);
void resizemouse(Arg const *arg);
void restart(Arg const *arg);
void rotatestack(Arg const *arg);
void setcfact(Arg const *arg);
void setlayout(Arg const *arg);
void setmfact(Arg const *arg);
void spawn(Arg const *arg);
void tag(Arg const *arg);
void tagmon(Arg const *arg);
void togglebar(Arg const *arg);
void togglefloating(Arg const *arg);
void toggletag(Arg const *arg);
void toggleview(Arg const *arg);
void view(Arg const *arg);
void zoom(Arg const *arg);

#ifdef ASOUND
void volumechange(Arg const *arg);
#endif  // ASOUND

#endif  // ARG_H
