#include "ztimer.h"
#include "ztimer/convert.h"
#include "ztimer/extend.h"
#include "ztimer/periph.h"
#include "ztimer/rtt.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static ztimer_periph_t _ztimer_periph;
static ztimer_rtt_t _ztimer_rtt;
static ztimer_convert_t _ztimer_rtt_convert;
static ztimer_extend_t _ztimer_rtt_extend;

ztimer_dev_t *const ZTIMER_USEC = (ztimer_dev_t*) &_ztimer_periph;
ztimer_dev_t *const ZTIMER_MSEC = (ztimer_dev_t*) &_ztimer_rtt_extend;

void ztimer_board_init(void)
{
    ztimer_periph_init(&_ztimer_periph, 0, 1000000LU);
    _ztimer_periph.adjust = ztimer_diff(ZTIMER_USEC, 100);;
    DEBUG("ztimer_board_init(): ZTIMER_US diff=%"PRIu32"\n", _ztimer_periph.adjust);

    ztimer_rtt_init(&_ztimer_rtt);
    ztimer_convert_init(&_ztimer_rtt_convert, (ztimer_dev_t*)&_ztimer_rtt, &ztimer_convert_ops_1000to1024);
    ztimer_extend_init(&_ztimer_rtt_extend, (ztimer_dev_t*)&_ztimer_rtt_convert, 22);
}
