#include "ztimer.h"
#include "ztimer/convert.h"
#include "ztimer/extend.h"
#include "ztimer/periph.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

#if defined(MODULE_ZTIMER_USEC) || defined(MODULE_ZTIMER_MSEC)
static ztimer_periph_t _ztimer_periph;
#endif

#if defined(MODULE_ZTIMER_USEC)
ztimer_dev_t *const ZTIMER_USEC = (ztimer_dev_t*) &_ztimer_periph;
#endif

#if defined(MODULE_ZTIMER_MSEC)
static ztimer_convert_t _ztimer_periph_convert;
static ztimer_extend_t _ztimer_periph_extend;

ztimer_dev_t *const ZTIMER_MSEC = (ztimer_dev_t*) &_ztimer_periph_extend;
#endif

void ztimer_board_init(void)
{
#if defined(MODULE_ZTIMER_USEC) || defined(MODULE_ZTIMER_MSEC)
    ztimer_periph_init(&_ztimer_periph, 0, 1000000LU);
    _ztimer_periph.adjust = 22;
    DEBUG("ztimer_board_init(): ZTIMER_US diff=%"PRIu32"\n", _ztimer_periph.adjust);
#endif

#if defined(MODULE_ZTIMER_MSEC)
    ztimer_convert_init(&_ztimer_periph_convert, (ztimer_dev_t*)&_ztimer_periph, 1000, 1);
    ztimer_extend_init(&_ztimer_periph_extend, (ztimer_dev_t*)&_ztimer_periph_convert, 22);
#endif
}
