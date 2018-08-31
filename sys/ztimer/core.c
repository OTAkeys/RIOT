/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include <stdint.h>

#include "irq.h"
#include "ztimer.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static void _add_entry_to_list(ztimer_dev_t *ztimer, ztimer_base_t *entry);
static void _del_entry_from_list(ztimer_dev_t *ztimer, ztimer_base_t *entry);
static void _update_head_offset(ztimer_dev_t *ztimer);
static void _ztimer_update(ztimer_dev_t *ztimer);
static void _ztimer_print(ztimer_dev_t *ztimer);

void ztimer_remove(ztimer_dev_t *ztimer, ztimer_t *entry)
{
    unsigned state = irq_disable();

    _update_head_offset(ztimer);
    _del_entry_from_list(ztimer, &entry->base);

    _ztimer_update(ztimer);

    irq_restore(state);
}

void ztimer_set(ztimer_dev_t *ztimer, ztimer_t *entry, uint32_t val)
{
    DEBUG("ztimer_set(): %p: set %p at %"PRIu32" offset %"PRIu32"\n",
            (void *)ztimer, (void *)entry, ztimer->ops->now(ztimer), val);

    unsigned state = irq_disable();

    _update_head_offset(ztimer);
    _del_entry_from_list(ztimer, &entry->base);

    entry->base.offset = val;
    _add_entry_to_list(ztimer, &entry->base);
    if (ztimer->list.next == &entry->base) {
        /* The added entry became the new list head */
        ztimer->ops->set(ztimer, val);
    }

    irq_restore(state);
}

static void _add_entry_to_list(ztimer_dev_t *ztimer, ztimer_base_t *entry)
{
    uint32_t delta_sum = 0;

    ztimer_base_t *list = &ztimer->list;

    /* Jump past all entries which are set to an earlier target than the new entry */
    while (list->next) {
        ztimer_base_t *list_entry = list->next;
        if ((list_entry->offset + delta_sum) > entry->offset) {
            break;
        }
        delta_sum += list_entry->offset;
        list = list->next;
    }

    /* Insert into list */
    entry->next = list->next;
    entry->offset -= delta_sum;
    if (entry->next) {
        entry->next->offset -= entry->offset;
    }
    list->next = entry;
    DEBUG("_add_entry_to_list() %p offset %"PRIu32"\n", (void *)entry, entry->offset);

}

static void _update_head_offset(ztimer_dev_t *ztimer)
{
    uint32_t old_base = ztimer->list.offset;
    uint32_t now = ztimer->ops->now(ztimer);
    uint32_t diff = now - old_base;

    if (ztimer->list.next) {
        ztimer_base_t *entry = ztimer->list.next;

        do {
            if (diff <= entry->offset) {
                entry->offset -= diff;
                break;
            }
            else {
                diff -= entry->offset;
                entry->offset = 0;
                if (diff) {
                    /* skip timers with offset==0 */
                    do {
                        entry = entry->next;
                    } while (entry && (entry->offset == 0));
                }
            }
        } while (diff && entry);
        DEBUG("ztimer %p: _update_head_offset(): now=%" PRIu32 " new head %p offset %" PRIu32 "\n",
            (void *)ztimer, now, (void *)entry, entry->offset);
    }

    ztimer->list.offset = now;
}

static void _del_entry_from_list(ztimer_dev_t *ztimer, ztimer_base_t *entry)
{
    DEBUG("_del_entry_from_list()\n");
    ztimer_base_t *list = &ztimer->list;

    while (list->next) {
        ztimer_base_t *list_entry = list->next;
        if (list_entry == entry) {
            list->next = entry->next;
            if (list->next) {
                list_entry = list->next;
                list_entry->offset += entry->offset;
            }
            break;
        }
        list = list->next;
    }
}

static ztimer_t *_now_next(ztimer_dev_t *ztimer)
{
    ztimer_base_t *entry = ztimer->list.next;

    if (entry && (entry->offset == 0)) {
        ztimer->list.next = entry->next;
        return (ztimer_t*)entry;
    }
    else {
        return NULL;
    }
}

static void _ztimer_update(ztimer_dev_t *ztimer)
{
    if (ztimer->list.next) {
        ztimer->ops->set(ztimer, ztimer->list.next->offset);
    }
    else {
        ztimer->ops->cancel(ztimer);
    }
}

void ztimer_handler(ztimer_dev_t *ztimer)
{
    DEBUG("ztimer_handler(): %p now=%"PRIu32"\n", (void *)ztimer, ztimer->ops->now(ztimer));
    if (ENABLE_DEBUG) {
        _ztimer_print(ztimer);
    }
    ztimer->list.offset += ztimer->list.next->offset;
    ztimer->list.next->offset = 0;

    ztimer_t *entry = _now_next(ztimer);
    while (entry) {
        DEBUG("ztimer_handler(): trigger %p->%p at %"PRIu32"\n",
                (void *)entry, (void *)entry->base.next, ztimer->ops->now(ztimer));
        entry->callback(entry->arg);
        entry = _now_next(ztimer);
        if (!entry) {
            /* See if any more alarms expired during callback processing */
            /* This reduces the number of implicit calls to ztimer->ops->now() */
            _update_head_offset(ztimer);
            entry = _now_next(ztimer);
        }
    }

    _ztimer_update(ztimer);

    if (ENABLE_DEBUG) {
        _ztimer_print(ztimer);
    }
    DEBUG("ztimer_handler(): %p done.\n", (void *)ztimer);
    if (!irq_is_in()) {
        thread_yield_higher();
    }
}

static void _ztimer_print(ztimer_dev_t *ztimer)
{
    ztimer_base_t *entry = &ztimer->list;
    do {
        printf("0x%08x:%"PRIu32"->", (unsigned)entry, entry->offset);
    } while ((entry = entry->next));
    puts("");
}
