#include "ztimer.h"
#include "periph/timer.h"

typedef struct {
    ztimer_dev_t super;
    tim_t dev;
    uint32_t adjust;
} ztimer_periph_t;

void ztimer_periph_init(ztimer_periph_t *ztimer, tim_t dev, unsigned long freq);
