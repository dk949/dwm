#ifndef LAYOUT_H
#define LAYOUT_H


struct Monitor;

typedef struct Layout {
    char const *symbol;
    void (*arrange)(struct Monitor *);
} Layout;

void tile(struct Monitor *);
void monocle(struct Monitor *);
void centeredmaster(struct Monitor *);
void centeredfloatingmaster(struct Monitor *);

#endif  // LAYOUT_H
