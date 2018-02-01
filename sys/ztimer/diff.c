#include "ztimer.h"

typedef struct {
    ztimer_dev_t *ztimer;
    volatile uint32_t *val;
} callback_arg_t;

static void _callback(void* arg)
{
    callback_arg_t *callback_arg = (callback_arg_t*) arg;
    *callback_arg->val = ztimer_now(callback_arg->ztimer);
}

uint32_t ztimer_diff(ztimer_dev_t *ztimer, uint32_t base)
{
    volatile uint32_t after = 0;
    uint32_t pre;
    callback_arg_t arg = { .ztimer=ztimer, .val=&after };
    ztimer_t t = {.callback = _callback, .arg=&arg };
    pre = ztimer_now(ztimer);
    ztimer_set(ztimer, &t, base);
    while(!after) {}
    return after-pre-base;
}
