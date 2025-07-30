#ifndef DWM_LAYOUT_HPP
#define DWM_LAYOUT_HPP


#include <memory>
struct Monitor;

struct Layout {
    char const *symbol;
    void (*arrange)(struct std::shared_ptr<Monitor> const &);
};

void tile(struct std::shared_ptr<Monitor> const &);
void monocle(struct std::shared_ptr<Monitor> const &);
void centeredmaster(struct std::shared_ptr<Monitor> const &);
void centeredfloatingmaster(struct std::shared_ptr<Monitor> const &);

#endif  // DWM_LAYOUT_HPP
