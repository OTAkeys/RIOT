#include <stdint.h>

#include "ztimer.h"

typedef struct {
    ztimer_dev_t super;
    ztimer_dev_t *parent;
    ztimer_t parent_entry;
    ztimer_t parent_overflow_entry;
    uint32_t overflows;
    unsigned shift;
} ztimer_extend_t;

void ztimer_extend_init(ztimer_extend_t *ztimer_extend, ztimer_dev_t *parent, unsigned shift);
