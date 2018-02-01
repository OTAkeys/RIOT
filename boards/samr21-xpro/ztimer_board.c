#include "ztimer.h"
#include "ztimer/convert.h"
#include "ztimer/extend.h"
#include "ztimer/periph.h"
#include "ztimer/rtt.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

#ifdef MODULE_ZTIMER_USEC
static ztimer_periph_t _ztimer_periph;
ztimer_dev_t *const ZTIMER_USEC = (ztimer_dev_t*) &_ztimer_periph;
#endif

#ifdef MODULE_ZTIMER_MSEC
static ztimer_rtt_t _ztimer_rtt;
static ztimer_convert_t _ztimer_rtt_convert;
static ztimer_extend_t _ztimer_rtt_extend;

ztimer_dev_t *const ZTIMER_MSEC = (ztimer_dev_t*) &_ztimer_rtt_extend;
#endif

void ztimer_board_init(void)
{
#ifdef MODULE_ZTIMER_USEC
    ztimer_periph_init(&_ztimer_periph, 1, 1000000LU);
    _ztimer_periph.adjust = 22;
    DEBUG("ztimer_board_init(): ZTIMER_US diff=%"PRIu32"\n", _ztimer_periph.adjust);
#endif

#ifdef MODULE_ZTIMER_MSEC
    ztimer_rtt_init(&_ztimer_rtt);
    ztimer_convert_init(&_ztimer_rtt_convert, (ztimer_dev_t*)&_ztimer_rtt, 125, 128);
    ztimer_extend_init(&_ztimer_rtt_extend, (ztimer_dev_t*)&_ztimer_rtt_convert, 22);
#endif
}
