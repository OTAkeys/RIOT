/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include "ztimer/convert.h"

static uint32_t _convert_set(ztimer_convert_t *ztimer_convert, uint32_t val)
{
    if (ztimer_convert->mul > 1) {
        val *= ztimer_convert->mul;
    }
    if (ztimer_convert->div > 1) {
        val /= ztimer_convert->div;
    }
    return val;
}

static uint32_t _convert_now(ztimer_convert_t *ztimer_convert, uint32_t val)
{
    if (ztimer_convert->div > 1) {
        val *= ztimer_convert->div;
    }
    if (ztimer_convert->mul > 1) {
        val /= ztimer_convert->mul;
    }
    return val;
}

static void _ztimer_convert_cancel(ztimer_dev_t *ztimer)
{
    ztimer_convert_t *ztimer_convert = (ztimer_convert_t*) ztimer;
    ztimer_remove(ztimer_convert->parent, &ztimer_convert->parent_entry);
}

static void _ztimer_convert_set(ztimer_dev_t *ztimer, uint32_t val)
{
    ztimer_convert_t *ztimer_convert = (ztimer_convert_t*) ztimer;
    ztimer_set(ztimer_convert->parent, &ztimer_convert->parent_entry, _convert_set(ztimer_convert, val));
}

static uint32_t _ztimer_convert_now(ztimer_dev_t *ztimer)
{
    ztimer_convert_t *ztimer_convert = (ztimer_convert_t*) ztimer;
    return _convert_now(ztimer_convert, ztimer_now(ztimer_convert->parent));
}

static const ztimer_ops_t _ztimer_convert_ops = {
    .set=_ztimer_convert_set,
    .now=_ztimer_convert_now,
    .cancel=_ztimer_convert_cancel,
};

void ztimer_convert_init(ztimer_convert_t *ztimer_convert, ztimer_dev_t *parent, unsigned mul, unsigned div)
{
    ztimer_convert_t tmp = {
        .super.ops=&_ztimer_convert_ops,
        .parent=parent,
        .parent_entry.callback=(void (*)(void *))ztimer_handler,
        .parent_entry.arg=ztimer_convert,
        .mul = mul,
        .div = div
    };

    *ztimer_convert = tmp;
}
