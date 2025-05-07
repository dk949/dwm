#ifndef DWM_LAYOUT_HPP
#define DWM_LAYOUT_HPP


struct Monitor;

struct Layout {
    char const *symbol;
    void (*arrange)(struct Monitor *);
};

void tile(struct Monitor *);
void monocle(struct Monitor *);
void centeredmaster(struct Monitor *);
void centeredfloatingmaster(struct Monitor *);

#endif  // DWM_LAYOUT_HPP
