#include "ztimer.h"

typedef struct {
    ztimer_dev_t super;
    ztimer_dev_t *parent;
    ztimer_t parent_entry;
    uint16_t mul;
    uint16_t div;
} ztimer_convert_t;

void ztimer_convert_init(ztimer_convert_t *ztimer_convert, ztimer_dev_t *parent, unsigned mul, unsigned div);
